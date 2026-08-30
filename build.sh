#!/usr/bin/env bash

PROJECT_ROOT="$(pwd)"

DEBUG=0
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

    if [[ -n "$requested" ]]; then
        c_bin="$(command -v "${c_prefix}-${requested}" 2>/dev/null)"
        cxx_bin="$(command -v "${cxx_prefix}-${requested}" 2>/dev/null)"
        if [[ -n "$c_bin" && -n "$cxx_bin" ]]; then
            echo "$c_bin|$cxx_bin"
            return 0
        fi
        return 1
    fi

    # No version requested: take the highest versioned binary on the system.
    for v in $(seq 25 -1 9); do
        c_bin="$(command -v "${c_prefix}-${v}" 2>/dev/null)"
        cxx_bin="$(command -v "${cxx_prefix}-${v}" 2>/dev/null)"
        if [[ -n "$c_bin" && -n "$cxx_bin" ]]; then
            echo "$c_bin|$cxx_bin"
            return 0
        fi
    done

    # Fall back to the unversioned binary.
    c_bin="$(command -v "$c_prefix" 2>/dev/null)"
    cxx_bin="$(command -v "$cxx_prefix" 2>/dev/null)"
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

if [[ "$RUN_INCLUDES" == true ]]; then
    echo "🔍 Running clang include-cleaner..."

    find src -name "*.cpp" \
    | xargs -P 8 -I {} clangd --check="{}" \
        --compile-commands-dir=build \
        --enable-config
fi

if [[ "$RUN_TESTS" == true ]]; then
    ASAN_OPTIONS=detect_leaks="$DEBUG" \
        "$PROJECT_ROOT/build/fairuz_tests" "${TEST_ARGS[@]}"
fi

if [[ "$RUN_MAIN" == true ]]; then
    if [[ ${#MAIN_ARGS[@]} -eq 0 ]]; then
        echo "usage: ./build.sh run <file.fa>"
        exit 1
    fi

    ASAN_OPTIONS=detect_leaks="$DEBUG" \
        "$PROJECT_ROOT/build/fairuz" "${MAIN_ARGS[@]}"
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

    ASAN_OPTIONS=detect_leaks="$DEBUG" \
        "$PROJECT_ROOT/build/fairuz" format "${MAIN_ARGS[@]}"
fi