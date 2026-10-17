#include "test_runtime.hpp"
#include "test_source_program.hpp"
#include <gtest/gtest-spi.h>

using namespace fairuz;
using namespace fairuz::runtime;

TEST(RuntimeResultHelpers, SuccessReturnsValue)
{
    auto value = run_success(VM::RuntimeResult(Value::from_int(42)));
    EXPECT_EQ(value.as_int(), 42);
}

TEST(RuntimeResultHelpers, FailureRetainsItsSourceAndLocation)
{
    test_source::Program program("تاكد(خطا)\n");
    RuntimeError error(ErrorCode::ASSERT_FAILED, program.source, { 1, 1, 0, 0 });
    auto returned = run_failed(VM::RuntimeResult(error));
    EXPECT_EQ(returned.errc, error.errc);
    EXPECT_EQ(returned.source, program.source);
    EXPECT_EQ(returned.loc.line, 1u);
}

TEST(RuntimeResultHelpers, UnexpectedErrorReportsFatalFailureWithoutUnwrapping)
{
    test_source::Program program("تاكد(خطا)\n");
    RuntimeError error(ErrorCode::ASSERT_FAILED, program.source, { 1, 1, 0, 0 });
    testing::TestPartResultArray failures;
    unsigned caller_line = 0;
    {
        testing::ScopedFakeTestPartResultReporter reporter(
            testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &failures);
        caller_line = __LINE__ + 1;
        EXPECT_THROW(run_success(VM::RuntimeResult(error)), testing::AssertionException);
    }
    ASSERT_EQ(failures.size(), 1);
    EXPECT_TRUE(failures.GetTestPartResult(0).fatally_failed());
    EXPECT_STREQ(failures.GetTestPartResult(0).file_name(), __FILE__);
    EXPECT_EQ(failures.GetTestPartResult(0).line_number(), caller_line);
}

TEST(RuntimeResultHelpers, UnexpectedSuccessReportsFatalFailureWithoutFabricatingError)
{
    testing::TestPartResultArray failures;
    unsigned caller_line = 0;
    {
        testing::ScopedFakeTestPartResultReporter reporter(
            testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &failures);
        caller_line = __LINE__ + 1;
        EXPECT_THROW(run_failed(VM::RuntimeResult(Value::nil())), testing::AssertionException);
    }
    ASSERT_EQ(failures.size(), 1);
    EXPECT_TRUE(failures.GetTestPartResult(0).fatally_failed());
    EXPECT_STREQ(failures.GetTestPartResult(0).file_name(), __FILE__);
    EXPECT_EQ(failures.GetTestPartResult(0).line_number(), caller_line);
}

TEST(RuntimeRecovery, NativeHostFailureHasNoInventedSourceAndDoesNotReport)
{
    VM vm;
    auto error = run_failed(vm.len(0, nullptr));
    EXPECT_EQ(error.errc, ErrorCode::LEN_ARG_COUNT);
    EXPECT_EQ(error.source, nullptr);
    EXPECT_TRUE(error.traceback.empty());
    EXPECT_FALSE(diagnostic::has_errors());
    EXPECT_FALSE(error.str().empty());
    Value argument = Value::from_int(42);
    EXPECT_TRUE(run_success(vm.Int(1, &argument)).is_int());
}

TEST(RuntimeRecovery, CallbackFailureStopsSideEffectsAndVmCanRunAgain)
{
    test_source::Program failing(
        "قيم := [1]\n"
        "دالة افشل():\n"
        "    اضف(قيم، 2)\n"
        "    عطل(\"callback failed\")\n"
        "    اضف(قيم، 3)\n"
        "__استدعاء__(افشل، [])\n"
        "اضف(قيم، 4)\n");
    auto* failed_chunk = run_success(failing.compile());
    test_source::Program succeeding("طول(قيم)\n");
    auto* success_chunk = run_success(succeeding.compile());
    VM vm;
    auto error = run_failed(vm.run(failed_chunk));
    EXPECT_EQ(error.errc, ErrorCode::RUNTIME_ERROR);
    EXPECT_EQ(error.detail, "callback failed");
    EXPECT_EQ(error.loc.line, 4u);
    ASSERT_EQ(error.traceback.size(), 2u);
    EXPECT_EQ(error.traceback.top().loc.line, 6u);
    EXPECT_FALSE(diagnostic::has_errors());
    EXPECT_EQ(run_success(vm.run(success_chunk)).as_int(), 2);
    // The returned error remains usable after another execution resets diagnostics.
    EXPECT_NE(error.str().find("callback failed"), std::string::npos);
    EXPECT_EQ(error.source->line(4).find("عطل"), 4u);
}

TEST(RuntimeRecovery, NativeCallbackReturnsBeforeCallerContinues)
{
    test_source::Program program(
        "قيم := [1، 2]\n"
        "طول_القيم := __استدعاء__(طول، [قيم])\n"
        "تاكد(طول_القيم = 2)\n"
        "طبيعي(طول_القيم + 40)\n");
    auto* chunk = run_success(program.compile());
    VM vm;
    EXPECT_EQ(run_success(vm.run(chunk)).as_int(), 42);
}
