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

# Fairuz's own headers live flat inside fairuz/, next to their matching .cc
# files (e.g. fairuz/fvm.hpp), not under a separate include/ tree — so no
# extra include_directories() call is needed for them. Headers are found via
# each target's own target_include_directories(... ${CMAKE_SOURCE_DIR}/fairuz)
# below.

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

# Collect source files from fairuz/
file(GLOB_RECURSE fairuz_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/fairuz/*.cc)

# Build an object library from your language sources
add_library(fairuz_objects OBJECT ${fairuz_SOURCES})
target_include_directories(fairuz_objects PUBLIC
    ${CMAKE_SOURCE_DIR}/fairuz
)
target_link_libraries(fairuz_objects PUBLIC simdutf::simdutf)

# ─── Main executable ────────────────────────────────────────────────────────
add_executable(fairuz
    main.cpp
    $<TARGET_OBJECTS:fairuz_objects>
)

target_include_directories(fairuz PRIVATE ${CMAKE_SOURCE_DIR}/fairuz)
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

    # A prebuilt GTest (from Homebrew, apt, or a previous CMake run) is only
    # safe to link if it was built with the exact same compiler/standard
    # library this configure is using. This project supports switching
    # between GCC and Clang on the same machine (build.sh --gcc / --clang),
    # and a prebuilt libgtest.a is fixed to whichever one built it — mixing
    # a Clang-built libgtest.a into a GCC-built test binary (or vice versa)
    # produces a wall of "Undefined symbols" at link time, because libc++'s
    # std::__1::basic_string and libstdc++'s std::__cxx11::basic_string are
    # different, incompatible types under the hood despite both being
    # "std::string" in source. So: always build GoogleTest from source,
    # with the same compiler as everything else in this configure. This
    # costs a one-time source build per compiler instead of a prebuilt
    # library, but it's the only way to guarantee ABI compatibility across
    # a GCC/Clang-switching workflow.
    message(STATUS "Fetching GoogleTest from source (built with the active compiler to guarantee ABI compatibility)")

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Picks up every .cpp under tests/, including subdirectories.
    # CONFIGURE_DEPENDS asks CMake to re-glob on every build (not just every
    # manual re-configure), so a newly added test file is picked up on the
    # next build without anyone having to remember to rerun cmake by hand.
    # Not all CMake generators honor this equally well, so CI should still
    # do a fresh configure per run rather than reuse a stale build dir.
    file(GLOB_RECURSE fairuz_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*.cpp)

    add_executable(fairuz_tests
        ${fairuz_TEST_SOURCES}
        $<TARGET_OBJECTS:fairuz_objects>
    )

    # Test-build flags favor catching bugs over raw speed:
    #  - no -march=native: the test binary must run correctly on whatever
    #    CPU the CI runner (or a contributor's machine) happens to have.
    #    -march=native bakes in the *build* machine's instruction set and
    #    can SIGILL when run elsewhere.
    #  - no -ffast-math: it changes floating-point semantics (NaN handling,
    #    rounding, associativity) and can hide or introduce numeric bugs
    #    that the test suite exists to catch.
    #  - no -flto here: LTO is a release-build optimization; keeping it out
    #    of the test build keeps rebuild-after-single-file-change fast and
    #    avoids LTO/ASan interaction issues on some toolchains.
    target_compile_options(fairuz_tests PRIVATE
        -Wunused-variable
        -Wunused-parameter
        $<$<CXX_COMPILER_ID:GNU,Clang>:-O0>
        $<$<CXX_COMPILER_ID:MSVC>:/Od>
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

    target_include_directories(fairuz_tests PRIVATE ${CMAKE_SOURCE_DIR}/fairuz)

    enable_testing()
    add_test(NAME AllTestsInMain COMMAND fairuz_tests)
endif()