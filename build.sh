#!/usr/bin/env bash

PROJECT_ROOT="$(pwd)"
DEBUG=0
CLEAN_BUILD=false
RUN_TESTS=false
RUN_MAIN=false
TEST_ARGS=()
MAIN_ARGS=()

for arg in "$@"; do
    if [[ "$arg" == "--clean" ]]; then
        CLEAN_BUILD=true
    elif [[ "$arg" == "test" ]]; then
        RUN_TESTS=true
    elif [[ "$arg" == "run" ]]; then
        RUN_MAIN=true
    elif [[ "$arg" == "--debug" ]]; then
        DEBUG=1
    else
        if [[ "$RUN_TESTS" == true ]]; then
            TEST_ARGS+=("$arg")
        else
            MAIN_ARGS+=("$arg")
        fi
    fi
done

if [[ "$CLEAN_BUILD" == true ]]; then
    rm -rf build
fi

mkdir -p build
cd build || exit 1

if [[ "$RUN_TESTS" == true ]]; then
    cmake -DCMAKE_C_COMPILER=clang \
          -DCMAKE_CXX_COMPILER=clang++ \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
          -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)" \
          -DBUILD_TESTS=ON .. || exit 1
else
    cmake -DCMAKE_C_COMPILER=clang \
          -DCMAKE_CXX_COMPILER=clang++ \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
          -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)" .. || exit 1
fi

make || exit 1

if [[ "$RUN_TESTS" == true ]]; then
    ASAN_OPTIONS=detect_leaks="$DEBUG" \
        "$PROJECT_ROOT/build/mylang_tests" "${TEST_ARGS[@]}"
fi

if [[ "$RUN_MAIN" == true ]]; then
    if [[ ${#MAIN_ARGS[@]} -eq 0 ]]; then
        echo "usage: ./build.sh run <file>"
        exit 1
    fi
    ASAN_OPTIONS=detect_leaks="$DEBUG" \
        "$PROJECT_ROOT/build/mylang" "${MAIN_ARGS[@]}"
fi