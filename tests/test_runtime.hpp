#ifndef FAIRUZ_TEST_RUNTIME_HPP
#define FAIRUZ_TEST_RUNTIME_HPP

#include "Diag/errc.hpp"
#include "Interpreter/vm.hpp"
#include <gtest/gtest.h>
#include <source_location>

namespace test_runtime_detail {

[[noreturn]] inline void fail(char const* message, std::source_location where)
{
    ::testing::TestPartResult failure(::testing::TestPartResult::kFatalFailure,
        where.file_name(), static_cast<int>(where.line()), message);
    GTEST_MESSAGE_AT_(where.file_name(), static_cast<int>(where.line()), message,
        ::testing::TestPartResult::kFatalFailure);
    throw ::testing::AssertionException(failure);
}

} // namespace test_runtime_detail

template<typename T, typename E>
inline T run_success(fairuz::ErrorOr<T, E> result,
    std::source_location where = std::source_location::current())
{
    if (result.has_error())
        test_runtime_detail::fail(
            (std::string("Expected success, but received an error : ") + result.error().str()).c_str(), where);
    return std::move(result).value();
}

template<typename T, typename E>
inline E run_failed(fairuz::ErrorOr<T, E> result,
    std::source_location where = std::source_location::current())
{
    if (result.has_value())
        test_runtime_detail::fail("Expected an error, but execution succeeded", where);
    return std::move(result).error();
}

#endif
