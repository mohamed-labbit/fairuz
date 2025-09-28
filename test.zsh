rm -rf build
mkdir build
cd build
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path) ..
make
./mylang_tests -v