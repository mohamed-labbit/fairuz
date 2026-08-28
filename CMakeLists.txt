cmake_minimum_required(VERSION 3.14)

# ─── Compiler selection ─────────────────────────────────────────────────────
# Default preference: clang/clang++. Fall back to gcc/g++ if clang isn't
# found, or force gcc/g++ regardless via -DUSE_GCC=ON.
# This must run before project() — that's what actually fixes the compiler,
# and it will not pick up a CMAKE_CXX_COMPILER set afterward.
option(USE_GCC "Force compiling with GCC instead of Clang" OFF)

if(NOT CMAKE_C_COMPILER AND NOT CMAKE_CXX_COMPILER)
    find_program(CLANG_C_COMPILER NAMES clang)
    find_program(CLANG_CXX_COMPILER NAMES clang++)
    find_program(GCC_C_COMPILER NAMES gcc)
    find_program(GCC_CXX_COMPILER NAMES g++)

    if(USE_GCC)
        if(GCC_C_COMPILER AND GCC_CXX_COMPILER)
            message(STATUS "USE_GCC is ON: using GCC (${GCC_C_COMPILER}, ${GCC_CXX_COMPILER})")
            set(CMAKE_C_COMPILER ${GCC_C_COMPILER})
            set(CMAKE_CXX_COMPILER ${GCC_CXX_COMPILER})
        else()
            message(FATAL_ERROR "USE_GCC is ON but gcc/g++ could not be found")
        endif()
    elseif(CLANG_C_COMPILER AND CLANG_CXX_COMPILER)
        message(STATUS "Using Clang (${CLANG_C_COMPILER}, ${CLANG_CXX_COMPILER})")
        set(CMAKE_C_COMPILER ${CLANG_C_COMPILER})
        set(CMAKE_CXX_COMPILER ${CLANG_CXX_COMPILER})
    elseif(GCC_C_COMPILER AND GCC_CXX_COMPILER)
        message(STATUS "Clang not found, falling back to GCC (${GCC_C_COMPILER}, ${GCC_CXX_COMPILER})")
        set(CMAKE_C_COMPILER ${GCC_C_COMPILER})
        set(CMAKE_CXX_COMPILER ${GCC_CXX_COMPILER})
    else()
        message(FATAL_ERROR "Neither clang/clang++ nor gcc/g++ could be found")
    endif()
endif()

project(Fairuz VERSION 0.1.0 LANGUAGES C CXX)

# Add right after project()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# ─── C++ standard selection ─────────────────────────────────────────────────
# Prefer C++26, but fall back to C++23 if (and only if) the selected
# compiler can't actually compile with -std=c++26 — e.g. GCC < 14, where
# C++26 support doesn't exist yet. This must run before CMAKE_CXX_STANDARD
# is set: once that's set with CMAKE_CXX_STANDARD_REQUIRED ON, every
# try_compile (including ones inside find_package calls below, like
# FindOpenMP) fails hard on an unsupported dialect instead of letting us
# probe for one.
include(CheckCXXCompilerFlag)

check_cxx_compiler_flag("-std=c++26" COMPILER_SUPPORTS_CXX26)

if(COMPILER_SUPPORTS_CXX26)
    set(CMAKE_CXX_STANDARD 26)
    message(STATUS "Using C++26 (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
else()
    set(CMAKE_CXX_STANDARD 23)
    message(STATUS "C++26 not supported by ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}, falling back to C++23")
endif()

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
file(GLOB_RECURSE fairuz_SOURCES ${CMAKE_SOURCE_DIR}/fairuz/*.cc)

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

install(DIRECTORY
    ${CMAKE_SOURCE_DIR}/examples/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/fairuz/examples
    FILES_MATCHING PATTERN "*.fa"
)

if(BUILD_TESTS)
    message(STATUS "Building tests")

    find_package(GTest CONFIG QUIET)

    if(NOT GTest_FOUND)
        add_library(minimal_gtest STATIC tests/support/minimal_gtest_main.cpp)
        target_include_directories(minimal_gtest PUBLIC ${CMAKE_SOURCE_DIR}/tests/support)
        add_library(GTest::gtest_main ALIAS minimal_gtest)
    endif()

    # Picks up every .cpp under tests/, including subdirectories, except
    # tests/support/, which holds the minimal_gtest helper built separately
    # above (compiling it again here would duplicate its main() at link
    # time). Note: this list is captured at configure time — adding a new
    # test file requires re-running cmake (or a --clean build) before it's
    # picked up.
    file(GLOB_RECURSE fairuz_TEST_SOURCES ${CMAKE_SOURCE_DIR}/tests/*.cpp)
    list(FILTER fairuz_TEST_SOURCES EXCLUDE REGEX "/tests/support/")

    add_executable(fairuz_tests
        ${fairuz_TEST_SOURCES}
        $<TARGET_OBJECTS:fairuz_objects>
    )

    target_compile_options(fairuz_tests PRIVATE
        -Wunused-variable 
        -Wunused-parameter
        $<$<CXX_COMPILER_ID:GNU,Clang>:-O0>
        $<$<CXX_COMPILER_ID:MSVC>:/Od>
        -march=native          # use all instructions available on this specific CPU
        -flto                  # link-time optimization — inlines across TUs
        -ffast-math            # relaxed float semantics, faster but breaks IEEE edge cases
        -fno-strict-aliasing
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