#!/usr/bin/env bash

if [[ "$1" == "-d" ]]; then
    rm -rf build
    mkdir build
fi

mkdir -p build
cd build || exit 1
w
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path) ..
make

./mylang_tests -v