cmake_minimum_required(VERSION 3.14)
project(MyProgrammingLanguage VERSION 0.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Optional: build tests (pass -DBUILD_TESTS=ON or use the 'test' preset)
option(BUILD_TESTS "Build the test suite" OFF)

# Find OpenMP if available
find_package(OpenMP)

if(OpenMP_FOUND)
    message(STATUS "Found OpenMP: ${OpenMP_CXX_FLAGS}")
endif()

# Find or fetch simdutf
find_package(simdutf CONFIG QUIET)

if(NOT simdutf_FOUND)
    message(STATUS "simdutf not found via find_package, fetching from GitHub...")

    include(FetchContent)
    FetchContent_Declare(
        simdutf
        GIT_REPOSITORY https://github.com/simdutf/simdutf.git
        GIT_TAG v5.5.0
        GIT_SHALLOW TRUE
    )

    set(SIMDUTF_TESTS OFF CACHE BOOL "Disable simdutf tests" FORCE)
    set(SIMDUTF_TOOLS OFF CACHE BOOL "Disable simdutf tools" FORCE)
    set(SIMDUTF_BENCHMARKS OFF CACHE BOOL "Disable simdutf benchmarks" FORCE)

    FetchContent_MakeAvailable(simdutf)
    message(STATUS "simdutf fetched successfully")
else()
    message(STATUS "Found simdutf via package manager")
endif()

# Collect source files from src/
file(GLOB_RECURSE fairuz_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.cc)

# Build an object library from your language sources
add_library(fairuz_objects OBJECT ${fairuz_SOURCES})
target_include_directories(fairuz_objects PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(fairuz_objects PUBLIC simdutf::simdutf)

# ─── Main executable ────────────────────────────────────────────────────────
add_executable(fairuz
    main.cpp
    $<TARGET_OBJECTS:fairuz_objects>
)

target_include_directories(fairuz PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(fairuz PRIVATE simdutf::simdutf)
target_compile_definitions(fairuz PRIVATE fairuz_VERSION="${PROJECT_VERSION}")

if(OpenMP_FOUND)
    target_link_libraries(fairuz PRIVATE OpenMP::OpenMP_CXX)
endif()

install(TARGETS fairuz
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_SOURCE_DIR}/README.md
    ${CMAKE_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# ─── Test executable (only when BUILD_TESTS=ON) ──────────────────────────────
if(BUILD_TESTS)
    message(STATUS "Building tests")

    find_package(GTest CONFIG QUIET)

    if(NOT GTest_FOUND)
        add_library(minimal_gtest STATIC tests/support/minimal_gtest_main.cpp)
        target_include_directories(minimal_gtest PUBLIC ${CMAKE_SOURCE_DIR}/tests/support)
        add_library(GTest::gtest_main ALIAS minimal_gtest)
    endif()

    add_executable(fairuz_tests
        tests/test_main.cpp
        tests/test_arena.cpp
        tests/test_narrow_table.cpp
        tests/test_string.cpp
        tests/test_tokens.cpp
        tests/test_indentation.cpp
        tests/test_parser.cpp
        tests/test_parser_golden.cpp
        tests/test_analyzer.cpp
        tests/test_opcode.cpp
        tests/test_chunk.cpp
        tests/test_compiler.cpp
        tests/test_vm.cpp
        tests/test_stdlib.cpp
        tests/test_cli.cpp
        tests/test_regressions.cpp
        tests/test_property.cpp
        $<TARGET_OBJECTS:fairuz_objects>
    )

    target_compile_options(fairuz_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-O0>
        $<$<CXX_COMPILER_ID:MSVC>:/Od>
        -march=native          # use all instructions available on this specific CPU
        -flto                  # link-time optimization — inlines across TUs
        -ffast-math            # relaxed float semantics, faster but breaks IEEE edge cases
    )

    target_link_libraries(fairuz_tests
        PRIVATE
        GTest::gtest_main
        simdutf::simdutf
    )

    if(OpenMP_FOUND)
        target_link_libraries(fairuz_tests PRIVATE OpenMP::OpenMP_CXX)
    endif()

    target_include_directories(fairuz_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

    enable_testing()
    add_test(NAME AllTestsInMain COMMAND fairuz_tests)
endif()
