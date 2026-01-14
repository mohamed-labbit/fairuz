cmake_minimum_required(VERSION 3.14)
project(MyProgrammingLanguage)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# For LLVM/Clang on macOS, ensure we use the right standard library
if(APPLE)
    # Use libc++ and ensure proper linkage
    add_compile_options(-stdlib=libc++ -g -O0)
    add_link_options(-stdlib=libc++)

    # Point to Homebrew LLVM's libc++
    link_directories(/opt/homebrew/opt/llvm/lib/c++)
    include_directories(/opt/homebrew/opt/llvm/include/c++/v1)
endif()

# Add include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find OpenMP if available
find_package(OpenMP)

if(OpenMP_FOUND)
    message(STATUS "Found OpenMP: ${OpenMP_CXX_FLAGS}")
endif()

# Collect source files from src/
file(GLOB_RECURSE MYLANG_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.cc)

# Build an object library from your language sources
add_library(mylang_objects OBJECT ${MYLANG_SOURCES})
target_include_directories(mylang_objects PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/utfcpp/source
)

if(OpenMP_FOUND)
    target_compile_options(mylang_objects PRIVATE ${OpenMP_CXX_FLAGS})
endif()

# Find GTest (use system package if available, otherwise fall back to a minimal
# bundled implementation so tests can still run in restricted environments)
find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
    add_library(minimal_gtest STATIC tests/support/minimal_gtest_main.cpp)
    target_include_directories(minimal_gtest PUBLIC ${CMAKE_SOURCE_DIR}/tests/support)
    add_library(GTest::gtest_main ALIAS minimal_gtest)
endif()

# Define the test executable
add_executable(mylang_tests
    tests/lexer/test_tokens.cpp
    tests/runtime/allocator/test_arena.cpp
    tests/parser/test_parser.cpp
    tests/test_include_all.cpp
    tests/test_main.cpp
    $<TARGET_OBJECTS:mylang_objects>
)

target_compile_options(mylang_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O0>
    $<$<CXX_COMPILER_ID:MSVC>:/Od>
)

target_link_libraries(mylang_tests
    PRIVATE
    GTest::gtest_main
)

if(OpenMP_FOUND)
    target_link_libraries(mylang_tests PRIVATE OpenMP::OpenMP_CXX)
endif()

# Also propagate include dirs
target_include_directories(mylang_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Enable testing and register tests
enable_testing()
add_test(NAME AllTestsInMain COMMAND mylang_tests)