#!/usr/bin/env bash
#
# -e: exit immediately if any command fails.
# -u: treat unset variables as an error instead of expanding to empty.
# -o pipefail: a pipeline's exit status is its first failing command, not
#   just its last one (relevant to the `find | xargs` pipeline below).
# Without these, a failing step earlier in the script (e.g. a bad cmake
# configure) can be silently swallowed and the script carries on with
# whatever partial state resulted — exactly what "production grade CI"
# must not do.
set -euo pipefail

PROJECT_ROOT="$(pwd)"

DEBUG=0
DETECT_LEAKS=""
CLEAN_BUILD=false
RUN_TESTS=false
RUN_MAIN=false
RUN_INCLUDES=false
RUN_INSTALL=false
FORMAT=false
USE_GCC=false
USE_CLANG=false
GCC_VERSION=""
CLANG_VERSION=""

TEST_ARGS=()
MAIN_ARGS=()
INSTALL_ARGS=()

for arg in "$@"; do
    case "$arg" in
        --clean)
            CLEAN_BUILD=true
            ;;
        --debug)
            DEBUG=1
            ;;
        --leak-check)
            DETECT_LEAKS=1
            ;;
        --no-leak-check)
            DETECT_LEAKS=0
            ;;
        --includes)
            RUN_INCLUDES=true
            ;;
        --gcc)
            USE_GCC=true
            ;;
        --gcc=*)
            USE_GCC=true
            GCC_VERSION="${arg#--gcc=}"
            ;;
        --clang)
            USE_CLANG=true
            ;;
        --clang=*)
            USE_CLANG=true
            CLANG_VERSION="${arg#--clang=}"
            ;;
        test)
            RUN_TESTS=true
            ;;
        run)
            RUN_MAIN=true
            ;;
        install)
            RUN_INSTALL=true
            ;;
        format)
            FORMAT=true
            ;;
        *)
            if [[ "$RUN_TESTS" == true ]]; then
                TEST_ARGS+=("$arg")
            elif [[ "$RUN_INSTALL" == true ]]; then
                INSTALL_ARGS+=("$arg")
            else
                MAIN_ARGS+=("$arg")
            fi
            ;;
    esac
done

# Resolve a compiler family ("gcc" or "clang") plus optional version to a
# concrete C/C++ binary pair. With no version, scans for the highest
# versioned binary present (e.g. gcc-14, gcc-13, ...) before falling back
# to the unversioned name. Prints "c_bin|cxx_bin" and returns 0 on success.
find_versioned_compiler() {
    local family="$1"
    local requested="$2"
    local c_prefix cxx_prefix c_bin cxx_bin v

    if [[ "$family" == "gcc" ]]; then
        c_prefix="gcc"
        cxx_prefix="g++"
    else
        c_prefix="clang"
        cxx_prefix="clang++"
    fi

    # NOTE: every `command -v` below is followed by `|| true`. Under
    # `set -e` (enabled for this whole script), a bare
    # `x="$(command -v not-found)"` aborts the script the instant the
    # substitution fails — before the surrounding `if [[ -n "$x" ]]` ever
    # gets a chance to handle the not-found case. `|| true` neutralizes
    # that so "not found" stays a normal, checked condition instead of a
    # fatal error.
    if [[ -n "$requested" ]]; then
        c_bin="$(command -v "${c_prefix}-${requested}" 2>/dev/null || true)"
        cxx_bin="$(command -v "${cxx_prefix}-${requested}" 2>/dev/null || true)"
        if [[ -n "$c_bin" && -n "$cxx_bin" ]]; then
            echo "$c_bin|$cxx_bin"
            return 0
        fi
        return 1
    fi

    # No version requested: take the highest versioned binary on the system.
    for v in $(seq 25 -1 9); do
        c_bin="$(command -v "${c_prefix}-${v}" 2>/dev/null || true)"
        cxx_bin="$(command -v "${cxx_prefix}-${v}" 2>/dev/null || true)"
        if [[ -n "$c_bin" && -n "$cxx_bin" ]]; then
            echo "$c_bin|$cxx_bin"
            return 0
        fi
    done

    # Fall back to the unversioned binary.
    c_bin="$(command -v "$c_prefix" 2>/dev/null || true)"
    cxx_bin="$(command -v "$cxx_prefix" 2>/dev/null || true)"
    if [[ -n "$c_bin" && -n "$cxx_bin" ]]; then
        echo "$c_bin|$cxx_bin"
        return 0
    fi

    return 1
}

if [[ "$CLEAN_BUILD" == true ]]; then
    rm -rf build
fi

mkdir -p build
cd build || exit 1

COMMON_FLAGS=(
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

if [[ "$USE_GCC" == true && "$USE_CLANG" == true ]]; then
    echo "error: --gcc and --clang are mutually exclusive" >&2
    exit 1
fi

if [[ "$USE_GCC" == true ]]; then
    COMPILER_PAIR="$(find_versioned_compiler gcc "$GCC_VERSION")" || {
        if [[ -n "$GCC_VERSION" ]]; then
            echo "error: gcc-${GCC_VERSION}/g++-${GCC_VERSION} not found" >&2
        else
            echo "error: no gcc/g++ installation found" >&2
        fi
        exit 1
    }
else
    # Clang is the default when --gcc isn't given, whether or not --clang
    # was explicit about it.
    COMPILER_PAIR="$(find_versioned_compiler clang "$CLANG_VERSION")" || {
        if [[ -n "$CLANG_VERSION" ]]; then
            echo "error: clang-${CLANG_VERSION}/clang++-${CLANG_VERSION} not found" >&2
        else
            echo "error: no clang/clang++ installation found" >&2
        fi
        exit 1
    }
fi

C_COMPILER="${COMPILER_PAIR%%|*}"
CXX_COMPILER="${COMPILER_PAIR##*|}"
echo "⚙️  Using compiler: $C_COMPILER / $CXX_COMPILER"

# CMakeCache.txt (and everything under build/_deps, including a from-source
# GoogleTest) is tied to whatever compiler configured it. Switching
# compilers (clang -> gcc or back) without wiping the build directory
# leaves stale, incompatible object files and static libraries around —
# this is what produces the wall of "Undefined symbols ... std::__1:: vs
# std::__cxx11::" linker errors when GTest was built by one compiler and
# the test .cpp files by another. Detect the switch here and force the
# same clean that --clean would do, so nobody has to remember it by hand.
PREVIOUS_COMPILER_MARKER="compiler.marker"
CURRENT_COMPILER_MARKER="$CXX_COMPILER"
if [[ -f "$PREVIOUS_COMPILER_MARKER" ]]; then
    if [[ "$(cat "$PREVIOUS_COMPILER_MARKER")" != "$CURRENT_COMPILER_MARKER" ]]; then
        echo "⚠️  Compiler changed since the last build in this build/ directory — cleaning to avoid a stale, ABI-incompatible mix (e.g. a GTest built by the old compiler linked against object files from the new one)."
        cd ..
        rm -rf build
        mkdir -p build
        cd build || exit 1
    fi
fi
echo "$CURRENT_COMPILER_MARKER" > "$PREVIOUS_COMPILER_MARKER"

COMMON_FLAGS+=(
    -DCMAKE_C_COMPILER="$C_COMPILER"
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER"
)

# -isysroot/SDK path only makes sense on macOS with Xcode tooling.
if [[ "$OSTYPE" == darwin* ]] && command -v xcrun >/dev/null 2>&1; then
    COMMON_FLAGS+=(
        -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"
    )
fi

if [[ "$DEBUG" == 1 || "$RUN_TESTS" == true ]]; then
    COMMON_FLAGS+=(
        -DCMAKE_CXX_FLAGS="-fsanitize=address -g -Wall -Wextra -Wpedantic"
    )
else
    COMMON_FLAGS+=(
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
    )
fi

if [[ "$RUN_TESTS" == true ]]; then
    cmake "${COMMON_FLAGS[@]}" -DBUILD_TESTS=ON .. || exit 1
else
    cmake "${COMMON_FLAGS[@]}" .. || exit 1
fi

make || exit 1

# Resolve the effective leak-detection setting. Precedence:
#   1. explicit --leak-check / --no-leak-check, if given
#   2. otherwise, on whenever ASan is in play (i.e. --debug or `test`),
#      since that's the whole point of running under ASan/LSan in CI
#   3. otherwise off (a plain optimized `run` has no sanitizer runtime at all)
if [[ -z "$DETECT_LEAKS" ]]; then
    if [[ "$DEBUG" == 1 || "$RUN_TESTS" == true ]]; then
        DETECT_LEAKS=1
    else
        DETECT_LEAKS=0
    fi
fi

if [[ "$RUN_INCLUDES" == true ]]; then
    echo "🔍 Running clang include-cleaner..."

    # Fairuz's sources are fairuz/*.cc (plus main.cpp), not src/*.cpp.
    find fairuz -name "*.cc" -o -name "*.hpp" -o -name "*.tpp" \
    | xargs -P 8 -I {} clangd --check="{}" \
        --compile-commands-dir=build \
        --enable-config
fi

if [[ "$RUN_TESTS" == true ]]; then
    # ${arr[@]:-} is NOT equivalent to ${arr[@]} when arr is empty: it still
    # expands to one empty-string argument ("") instead of zero arguments,
    # which fairuz_tests' own argv loop then rejects as an unrecognized
    # option. ${arr[@]+"${arr[@]}"} is the actual portable idiom: it only
    # expands the array at all if at least one element is set, so an empty
    # array correctly produces zero arguments on every bash version,
    # including macOS's stock bash 3.2.
    ASAN_OPTIONS="detect_leaks=$DETECT_LEAKS" \
        "$PROJECT_ROOT/build/fairuz_tests" ${TEST_ARGS[@]+"${TEST_ARGS[@]}"}
fi

if [[ "$RUN_MAIN" == true ]]; then
    if [[ ${#MAIN_ARGS[@]} -eq 0 ]]; then
        echo "usage: ./build.sh run <file.fa>"
        exit 1
    fi

    ASAN_OPTIONS="detect_leaks=$DETECT_LEAKS" \
        "$PROJECT_ROOT/build/fairuz" ${MAIN_ARGS[@]+"${MAIN_ARGS[@]}"}
fi

if [[ "$RUN_INSTALL" == true ]]; then
    INSTALL_PREFIX=""
    if [[ ${#INSTALL_ARGS[@]} -gt 0 ]]; then
        INSTALL_PREFIX="${INSTALL_ARGS[0]}"
        cmake --install . --prefix "$INSTALL_PREFIX" || exit 1
    else
        INSTALL_PREFIX="/usr/local"
        cmake --install . || exit 1
    fi

    echo
    echo "Installed fairuz to: $INSTALL_PREFIX"
    echo "Run it directly with:"
    echo "  $INSTALL_PREFIX/bin/fairuz <file.fa> [options]"
    echo
    echo "If you want 'fairuz' on your PATH for this shell:"
    echo "  export PATH=\"$INSTALL_PREFIX/bin:\$PATH\""
fi

if [[ "$FORMAT" == true ]]; then
    if [[ ${#MAIN_ARGS[@]} -eq 0 ]]; then
        echo "usage: ./build.sh format <file.fa>"
        exit 1
    fi

    ASAN_OPTIONS="detect_leaks=$DETECT_LEAKS" \
        "$PROJECT_ROOT/build/fairuz" format ${MAIN_ARGS[@]+"${MAIN_ARGS[@]}"}
fi