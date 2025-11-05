cmake_minimum_required(VERSION 3.14)
project(MyProgrammingLanguage)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(-g -stdlib=libc++ -fopenmp)

# Add include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Collect source files from src/
file(GLOB_RECURSE MYLANG_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.cc)

# Build an object library from your language sources
add_library(mylang_objects OBJECT ${MYLANG_SOURCES})
target_include_directories(mylang_objects PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Find GTest
set(GTest_DIR /opt/homebrew/Cellar/googletest/1.16.0)
find_package(GTest REQUIRED CONFIG)

# Find OpenMP
find_package(OpenMP REQUIRED)

if(OpenMP_FOUND)
    message(STATUS "Found OpenMP: ${OpenMP_CXX_FLAGS}")
    target_link_libraries(mylang_objects PUBLIC OpenMP::OpenMP_CXX)
endif()

# Define the test executable
add_executable(mylang_tests
    tests/lexer/test_tokens.cpp
    $<TARGET_OBJECTS:mylang_objects>
)

# Link GTest
target_link_libraries(mylang_tests PRIVATE GTest::gtest_main)

# Also propagate include dirs
target_include_directories(mylang_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Enable testing and register tests
enable_testing()
add_test(NAME AllTestsInMain COMMAND mylang_tests)