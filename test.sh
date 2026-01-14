#!/usr/bin/env bash

# Parse script-specific flags
CLEAN_BUILD=false
TEST_ARGS=()

for arg in "$@"; do
    if [[ "$arg" == "-d" ]]; then
        CLEAN_BUILD=true
    else
        # Collect all other arguments to forward to test binary
        TEST_ARGS+=("$arg")
    fi
done

# Clean build if requested
if [[ "$CLEAN_BUILD" == true ]]; then
    rm -rf build
    mkdir build
fi

mkdir -p build
cd build || exit 1

w

cmake -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path) ..

make

# Forward all collected arguments to the test binary
./mylang_tests "${TEST_ARGS[@]}"