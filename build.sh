#!/usr/bin/env bash

PROJECT_ROOT="$(pwd)"

DEBUG=0
CLEAN_BUILD=false
RUN_TESTS=false
RUN_MAIN=false
RUN_INCLUDES=false
RUN_INSTALL=false

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
        test)
            RUN_TESTS=true
            ;;
        run)
            RUN_MAIN=true
            ;;
        install)
            RUN_INSTALL=true
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

if [[ "$CLEAN_BUILD" == true ]]; then
    rm -rf build
fi

mkdir -p build
cd build || exit 1

COMMON_FLAGS=(
    -DCMAKE_C_COMPILER=clang
    -DCMAKE_CXX_COMPILER=clang++
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"
)

if [[ "$DEBUG" == 1 || "$RUN_TESTS" == true ]]; then
    COMMON_FLAGS+=(
        -DCMAKE_BUILD_TYPE=Debug
        -DCMAKE_CXX_FLAGS="-fsanitize=address -g"
    )
else
    COMMON_FLAGS+=(
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_CXX_FLAGS="-Ofast -DNDEBUG -flto=thin"
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
