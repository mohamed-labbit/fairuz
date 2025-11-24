cmake_minimum_required(VERSION 3.14)
project(MyProgrammingLanguage)

set(CMAKE_CXX_STANDARD 26)

# Force CMake to use Homebrew's LLVM
set(CMAKE_CXX_COMPILER "/opt/homebrew/opt/llvm/bin/clang++")
set(CMAKE_C_COMPILER "/opt/homebrew/opt/llvm/bin/clang")

# Use LLVM's libc++ explicitly with full path
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -g -I/opt/homebrew/opt/llvm/include/c++/v1")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")

# Add include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find OpenMP first
find_package(OpenMP REQUIRED)

if(OpenMP_FOUND)
    message(STATUS "Found OpenMP: ${OpenMP_CXX_FLAGS}")
endif()

# Collect source files from src/
file(GLOB_RECURSE MYLANG_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.cc)

# Build an object library from your language sources
add_library(mylang_objects OBJECT ${MYLANG_SOURCES})
target_include_directories(mylang_objects PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_options(mylang_objects PRIVATE ${OpenMP_CXX_FLAGS})

# Find GTest
set(GTest_DIR /opt/homebrew/Cellar/googletest/1.16.0)
find_package(GTest REQUIRED CONFIG)

# Define the test executable
add_executable(mylang_tests
    tests/lexer/test_tokens.cpp
    tests/runtime/allocator/test_arena.cpp
    tests/parser/test_parser.cpp
    tests/test_include_all.cpp
    $<TARGET_OBJECTS:mylang_objects>
)

# Link with full paths to ensure correct libraries
target_link_libraries(mylang_tests 
    PRIVATE 
    GTest::gtest_main
    OpenMP::OpenMP_CXX
    /opt/homebrew/opt/llvm/lib/c++/libc++.dylib
    /opt/homebrew/opt/llvm/lib/c++/libc++abi.dylib
)

# Also propagate include dirs
target_include_directories(mylang_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Enable testing and register tests
enable_testing()
add_test(NAME AllTestsInMain COMMAND mylang_tests)