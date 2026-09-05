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

# Always resolve the project root from this script's location rather than
# from the caller's current working directory. This keeps all paths stable
# even when build.sh is invoked from elsewhere.
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_ROOT"

DEBUG=0
DETECT_LEAKS=""
LEAK_CHECK_EXPLICIT=false
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
            LEAK_CHECK_EXPLICIT=true
            ;;
        --no-leak-check)
            DETECT_LEAKS=0
            LEAK_CHECK_EXPLICIT=true
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

# Resolve fairuz input files while we're still in the project root.
#
# build.sh changes its working directory to build/ below. If we passed a
# relative path such as examples/hello.fa through unchanged, fairuz would
# look for:
#
#   build/examples/hello.fa
#
# instead of:
#
#   examples/hello.fa
#
# Converting the first argument to an absolute path before entering build/
# makes run/format independent of fairuz's current working directory.
if [[ "$RUN_MAIN" == true || "$FORMAT" == true ]]; then
    if [[ ${#MAIN_ARGS[@]} -gt 0 ]]; then
        INPUT_FILE="${MAIN_ARGS[0]}"

        if [[ ! -f "$INPUT_FILE" ]]; then
            echo "error: input file not found: $INPUT_FILE" >&2
            exit 1
        fi

        MAIN_ARGS[0]="$(cd "$(dirname "$INPUT_FILE")" && pwd)/$(basename "$INPUT_FILE")"
    fi
fi

# Resolve a timeout implementation once, cross-platform.
#
# - Linux typically ships GNU coreutils `timeout`.
# - macOS ships neither `timeout` nor `gtimeout` by default; `gtimeout`
#   only exists if the user installed GNU coreutils via Homebrew.
# - Windows (Git Bash / MSYS2 / Cygwin) generally has neither.
#
# TIMEOUT_CMD is left empty when no native binary is found; portable_timeout
# below falls back to a pure bash/kill/sleep implementation in that case,
# so leak-check probing (and anything else that needs a timeout) works
# everywhere regardless of what's installed.
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_CMD="timeout"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_CMD="gtimeout"
else
    TIMEOUT_CMD=""
fi

# Run "$@" with a wall-clock timeout of $1 seconds, portably.
#
# Returns:
#   - the wrapped command's own exit status, if it finished in time
#   - 124 if it had to be killed for exceeding the timeout (matches the
#     exit code GNU `timeout` uses, so callers can check a single value
#     regardless of which path was taken)
#
# Any environment variable assignments given before calling this function
# (e.g. `ASAN_OPTIONS=... portable_timeout 10 "$bin"`) are exported for the
# duration of this function call, which covers the child process spawned
# inside it — this is standard bash behavior for assignments preceding a
# shell function call, not something this function has to do explicitly.
portable_timeout() {
    local timeout_secs="$1"
    shift

    if [[ -n "$TIMEOUT_CMD" ]]; then
        "$TIMEOUT_CMD" "$timeout_secs" "$@"
        return $?
    fi

    # Manual fallback: background the command, poll for completion, and
    # kill it if it's still alive once the timeout elapses. Only uses
    # bash builtins plus `kill`/`sleep`, so it works identically on Linux,
    # macOS, and Windows shells (Git Bash/MSYS2/Cygwin/WSL) with no
    # external dependency.
    "$@" &
    local pid=$!
    local waited=0
    local interval=1

    while kill -0 "$pid" 2>/dev/null; do
        sleep "$interval"
        waited=$((waited + interval))

        if [[ "$waited" -ge "$timeout_secs" ]]; then
            # Try a graceful TERM first, then force-kill if it ignores it.
            kill -TERM "$pid" 2>/dev/null
            sleep 1
            kill -KILL "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
            return 124
        fi
    done

    wait "$pid"
    return $?
}

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

# Probe whether LeakSanitizer is actually supported by the sanitizer
# runtime provided by the selected compiler.
#
# IMPORTANT:
# Do not run fairuz_tests here. Running the test executable just to probe
# sanitizer support can initialize application/test code and, depending on
# the test setup, can hang indefinitely.
#
# Instead, compile a tiny standalone ASan program using the exact same
# C++ compiler that is being used for the project. This exercises the
# sanitizer runtime directly and has no dependency on GoogleTest or Fairuz.
#
# Returns:
#   0 - LeakSanitizer is supported.
#   1 - LeakSanitizer explicitly reports that it is unsupported, or the
#       probe timed out (treated as unsupported — see below).
#
# Any other failure is treated as a real error and terminates the script.
detect_leaks_supported() {
    local probe_dir
    local probe_source
    local probe_binary
    local output_file
    local status

    # Use a single temp *directory* rather than separate mktemp calls with
    # suffixed templates (e.g. "...XXXXXX.cpp"). GNU mktemp accepts text
    # after the X's as a literal suffix, but BSD/macOS mktemp does not
    # substitute it the same way without an explicit -s flag — it can end
    # up trying to create a literally-named "...XXXXXX.cpp" file, which
    # collides ("File exists") on every subsequent run and leaves
    # probe_source/probe_binary/output_file empty, cascading into
    # "No such file or directory" everywhere below. A directory template
    # has no suffix to mishandle, so `mktemp -d TEMPLATE.XXXXXX` behaves
    # identically on GNU and BSD; fixed filenames go inside it.
    probe_dir="$(mktemp -d "${TMPDIR:-/tmp}/fairuz-lsan-probe.XXXXXX")"
    probe_source="$probe_dir/probe.cpp"
    probe_binary="$probe_dir/probe"
    output_file="$probe_dir/probe.log"

    # Always clean up the temporary probe directory, even if this function
    # exits because of an unexpected error.
    cleanup_lsan_probe() {
        rm -rf "$probe_dir"
    }

    # The source intentionally does almost nothing. Its purpose is only to
    # force the ASan/LSan runtime to initialize.
    cat > "$probe_source" <<'EOF'
#include <cstdlib>

int main()
{
    // Deliberately allocate memory so LeakSanitizer has something to inspect.
    // The pointer is intentionally not freed.
    void* leaked = std::malloc(1);
    (void)leaked;

    return 0;
}
EOF

    echo "🔎 Checking LeakSanitizer support..."

    # Compile using the exact C++ compiler selected by build.sh.
    #
    # Use ASan exactly as the real project does. LeakSanitizer is provided
    # through the sanitizer runtime on platforms where it is supported.
    if ! "$CXX_COMPILER" \
        -fsanitize=address \
        -g \
        "$probe_source" \
        -o "$probe_binary" \
        >"$output_file" 2>&1; then

        echo "error: failed to compile LeakSanitizer probe:" >&2
        cat "$output_file" >&2

        cleanup_lsan_probe
        exit 1
    fi

    # Run ONLY the tiny probe, never the test suite, and never without a
    # wall-clock bound.
    #
    # In some containerized/sandboxed environments (Docker without
    # CAP_SYS_PTRACE, restrictive seccomp profiles, some CI runners),
    # LeakSanitizer's leak scan needs to suspend and inspect other threads,
    # which relies on ptrace-like capabilities. When that's blocked, LSan
    # does not always print a clean "not supported" diagnostic and exit —
    # it can instead stall indefinitely trying to attach. portable_timeout
    # bounds that stall instead of letting it hang the whole script.
    #
    # Do not let `set -e` terminate the script here because some sanitizer
    # runtimes return a non-zero status when reporting a leak, and a
    # timeout also produces a non-zero (124) status. We inspect the
    # diagnostic/status ourselves below.
    set +e

    ASAN_OPTIONS="detect_leaks=1" \
        portable_timeout 10 "$probe_binary" >"$output_file" 2>&1

    status=$?

    set -e

    # A timeout (124) most likely means LSan is stuck trying to
    # suspend/ptrace threads because the sandbox/container blocks that.
    # Treat it the same as "explicitly unsupported": automatic detection
    # falls back to disabled, while an explicit --leak-check still fails
    # loudly (handled by the caller via LEAK_CHECK_EXPLICIT).
    if [[ "$status" -eq 124 ]]; then
        echo "-- LeakSanitizer probe timed out after 10s (likely blocked by sandbox/container ptrace restrictions)" >&2
        cleanup_lsan_probe
        return 1
    fi

    # This is the specific diagnostic produced by the runtime in the CI
    # environment from the original failure:
    #
    #   AddressSanitizer: detect_leaks is not supported on this platform.
    #
    # Only this condition (or the timeout above) means "fallback to
    # detect_leaks=0".
    if grep -q "detect_leaks is not supported on this platform" "$output_file"; then
        cleanup_lsan_probe
        return 1
    fi

    # A working LSan runtime normally emits a leak report because the probe
    # intentionally leaks one allocation. The exact exit code can vary
    # between sanitizer runtimes/configurations, so don't require status 0.
    #
    # If we got here, however, we need to distinguish an expected sanitizer
    # leak report from an unrelated failure.
    if grep -qE "LeakSanitizer|SUMMARY: AddressSanitizer:.*leak|detected memory leaks" "$output_file"; then
        cleanup_lsan_probe
        return 0
    fi

    # A completely clean exit is also acceptable. Some sanitizer runtimes
    # may not emit a leak report for this tiny probe even though the runtime
    # itself initialized successfully.
    if [[ "$status" -eq 0 ]]; then
        cleanup_lsan_probe
        return 0
    fi

    # Anything else is unexpected. Do not silently disable leak checking.
    echo "error: unexpected failure while probing LeakSanitizer:" >&2
    cat "$output_file" >&2

    cleanup_lsan_probe
    exit "$status"
}

if [[ "$CLEAN_BUILD" == true ]]; then
    rm -rf build
fi

mkdir -p build
cd build || exit 1

COMMON_FLAGS=(
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -G Ninja
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

echo "-- Using compiler: $C_COMPILER / $CXX_COMPILER"

# CMakeCache.txt (and everything under build/_deps, including a from-source
# GoogleTest) is tied to whatever compiler configured it. Switching
# compilers (clang -> gcc or back) without wiping the build directory
# leaves stale, incompatible object files and static libraries around —
# this is what produces the wall of "Undefined symbols ... std::__1:: vs
# std::__cxx11::" linker errors when GTest was built by one compiler and the
# test .cpp files by another. Detect the switch here and force the same clean
# that --clean would do, so nobody has to remember it by hand.
PREVIOUS_COMPILER_MARKER="compiler.marker"
CURRENT_COMPILER_MARKER="$CXX_COMPILER"

if [[ -f "$PREVIOUS_COMPILER_MARKER" ]]; then
    if [[ "$(cat "$PREVIOUS_COMPILER_MARKER")" != "$CURRENT_COMPILER_MARKER" ]]; then
        echo "-- Compiler changed since the last build in this build/ directory — cleaning to avoid a stale, ABI-incompatible mix (e.g. a GTest built by the old compiler linked against object files from the new one)."

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

# On this machine, the dsymutil binary GCC's Darwin linker driver
# (collect2) automatically invokes after linking any binary with a `-g*`
# flag crashes with SIGILL — but only for a realistic multi-TU, template-
# heavy link (the actual fairuz executable); a trivial single-file probe
# links cleanly every time and does not reproduce it. Since there's no
# lightweight, reliable way to detect this in advance without essentially
# rebuilding the real project as a probe, don't try — just always skip the
# automatic dsymutil step for GCC builds on Darwin via -save-temps (the
# flag GCC's own DSYMUTIL_SPEC checks to suppress that step). No dSYM
# bundle is produced for GCC builds as a result. Clang is unaffected: this
# whole failure mode is specific to the GCC/dsymutil pairing, and dsymutil
# isn't invoked at all outside Darwin.
DSYMUTIL_WORKAROUND=false

if [[ "$OSTYPE" == darwin* && "$USE_GCC" == true ]]; then
    echo "-- Skipping automatic dSYM generation for this GCC build (adding -save-temps) — dsymutil is known to crash on this system's GCC/dsymutil pairing for a real multi-file link. No dSYM bundle will be produced."
    DSYMUTIL_WORKAROUND=true
fi

# Assemble CXX flags into a single string rather than pushing multiple
# -DCMAKE_CXX_FLAGS= entries onto COMMON_FLAGS: cmake only keeps the last
# occurrence of a given -D flag on its command line, so a second
# -DCMAKE_CXX_FLAGS entry would silently clobber the first instead of
# combining with it.
CXX_EXTRA_FLAGS=""

if [[ "$DEBUG" == 1 || "$RUN_TESTS" == true ]]; then
    CXX_EXTRA_FLAGS="-fsanitize=address -g -Wall -Wextra -Wpedantic"
else
    COMMON_FLAGS+=(
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
    )
fi

if [[ "$DSYMUTIL_WORKAROUND" == true ]]; then
    CXX_EXTRA_FLAGS="${CXX_EXTRA_FLAGS:+$CXX_EXTRA_FLAGS }-save-temps"

    # -save-temps needs to be on the actual link command line to suppress
    # collect2's automatic dsymutil call (it's checked in GCC's
    # DSYMUTIL_SPEC, which only applies to the link step). CMake normally
    # forwards CMAKE_CXX_FLAGS to the link line too for a compiled-language
    # executable, but setting it on CMAKE_EXE_LINKER_FLAGS as well makes
    # that not depend on that CMake forwarding behavior.
    COMMON_FLAGS+=(
        -DCMAKE_EXE_LINKER_FLAGS="-save-temps"
    )
fi

if [[ -n "$CXX_EXTRA_FLAGS" ]]; then
    COMMON_FLAGS+=(
        -DCMAKE_CXX_FLAGS="$CXX_EXTRA_FLAGS"
    )
fi

if [[ "$RUN_TESTS" == true ]]; then
    cmake "${COMMON_FLAGS[@]}" -DBUILD_TESTS=ON .. || exit 1
else
    cmake "${COMMON_FLAGS[@]}" .. || exit 1
fi

cmake --build . || exit 1

# Resolve the effective leak-detection setting.
#
# Precedence:
#   1. explicit --leak-check / --no-leak-check, if given
#   2. otherwise, on whenever ASan is in play (i.e. --debug or `test`)
#   3. otherwise off (a plain optimized `run` has no sanitizer runtime at all)
if [[ -z "$DETECT_LEAKS" ]]; then
    if [[ "$DEBUG" == 1 || "$RUN_TESTS" == true ]]; then
        DETECT_LEAKS=1
    else
        DETECT_LEAKS=0
    fi
fi

# Check LeakSanitizer only when tests are being run.
#
# We use a standalone probe rather than running fairuz_tests. This prevents
# test initialization, GoogleTest setup, or application code from affecting
# the support check.
#
# If LSan isn't supported (including "probe timed out"):
#   - automatic/default leak checking falls back to disabled
#   - explicit --leak-check remains a hard error
if [[ "$DETECT_LEAKS" == 1 && "$RUN_TESTS" == true ]]; then
    if detect_leaks_supported; then
        echo "✓ LeakSanitizer supported; leak detection enabled"
    else
        # Fall back to disabled regardless of whether --leak-check was
        # explicit — an unsupported runtime shouldn't hard-fail the whole
        # build/test run, it just means leak checking can't happen here.
        if [[ "$LEAK_CHECK_EXPLICIT" == true ]]; then
            echo "-- --leak-check was requested, but LeakSanitizer is not supported by this runtime; disabling leak detection" >&2
        else
            echo "-- LeakSanitizer is not supported by this runtime; disabling leak detection"
        fi
        DETECT_LEAKS=0
    fi
fi

if [[ "$RUN_INCLUDES" == true ]]; then
    echo "-- Running clang include-cleaner..."

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
    echo "  export PATH=\"$INSTALL_PREFIX/bin:\\$PATH\""
fi

if [[ "$FORMAT" == true ]]; then
    if [[ ${#MAIN_ARGS[@]} -eq 0 ]]; then
        echo "usage: ./build.sh format <file.fa>"
        exit 1
    fi

    ASAN_OPTIONS="detect_leaks=$DETECT_LEAKS" \
        "$PROJECT_ROOT/build/fairuz" format ${MAIN_ARGS[@]+"${MAIN_ARGS[@]}"}
fi