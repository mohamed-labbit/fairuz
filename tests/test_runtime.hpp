#ifndef FAIRUZ_TEST_RUNTIME_HPP
#define FAIRUZ_TEST_RUNTIME_HPP

#include "Diag/errc.hpp"
#include "Interpreter/vm.hpp"
#include <gtest/gtest.h>
#include <source_location>

namespace test_runtime_detail {

[[noreturn]] inline void fail(char const* message, char const* file, unsigned line)
{
    ::testing::TestPartResult failure(::testing::TestPartResult::kFatalFailure, file, static_cast<int>(line), message);
    GTEST_MESSAGE_AT_(file, static_cast<int>(line), message, ::testing::TestPartResult::kFatalFailure);
    throw ::testing::AssertionException(failure);
}

} // namespace test_runtime_detail

// GCC on Darwin can emit overlapping ASan globals for source_location's
// synthesized call-site objects. Builtin default arguments retain the caller
// location without creating those global objects.
#if defined(__APPLE__) && defined(__GNUC__) && !defined(__clang__)
#    define FAIRUZ_TEST_CALL_SITE char const *file = __builtin_FILE(), unsigned line = __builtin_LINE()
#    define FAIRUZ_TEST_CALL_SITE_ARGS file, line
#else
#    define FAIRUZ_TEST_CALL_SITE std::source_location where = std::source_location::current()
#    define FAIRUZ_TEST_CALL_SITE_ARGS where.file_name(), where.line()
#endif

template<typename T, typename E> inline T run_success(fairuz::ErrorOr<T, E> result, FAIRUZ_TEST_CALL_SITE)
{
    if (result.has_error())
        test_runtime_detail::fail(
            (std::string("Expected success, but received an error : ") + result.error().str()).c_str(), FAIRUZ_TEST_CALL_SITE_ARGS);
    return std::move(result).value();
}

template<typename T, typename E> inline E run_failed(fairuz::ErrorOr<T, E> result, FAIRUZ_TEST_CALL_SITE)
{
    if (result.has_value()) test_runtime_detail::fail("Expected an error, but execution succeeded", FAIRUZ_TEST_CALL_SITE_ARGS);
    return std::move(result).error();
}

#undef FAIRUZ_TEST_CALL_SITE
#undef FAIRUZ_TEST_CALL_SITE_ARGS

#endif
