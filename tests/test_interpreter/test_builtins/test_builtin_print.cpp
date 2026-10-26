//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

#include <iostream>
#include <sstream>

class BuiltinPrint : public ::testing::Test {
protected:
    // Redirect the C++ stream independently of the runner's stdout capture.
    void SetUp() override { original_ = std::cout.rdbuf(output_.rdbuf()); }
    void TearDown() override { std::cout.rdbuf(original_); }

    void expect_print(VM& vm, int argc, Value* argv, char const* expected)
    {
        auto result = run_success(vm.print(argc, argv));
        ASSERT_TRUE(result.is_bool());
        EXPECT_TRUE(result.as_bool());
        EXPECT_EQ(output_.str(), expected);
    }

private:
    std::ostringstream output_;
    std::streambuf* original_ { nullptr };
};

TEST_F(BuiltinPrint, NoArgs_PrintsNewline)
{
    VM vm;
    expect_print(vm, 0, nullptr, "\n");
}

TEST_F(BuiltinPrint, StringArg)
{
    VM vm;
    auto s = str("hello world");
    expect_print(vm, 1, &s, "hello world\n");
}

TEST_F(BuiltinPrint, IntegerArg)
{
    VM vm;
    Value arg = Value::from_int(42);
    expect_print(vm, 1, &arg, "42\n");
}

TEST_F(BuiltinPrint, FloatArg)
{
    VM vm;
    Value arg = Value::from_real(3.14);
    expect_print(vm, 1, &arg, "3.14\n");
}

TEST_F(BuiltinPrint, BoolArg_True)
{
    VM vm;
    Value arg = Value::from_bool(true);
    expect_print(vm, 1, &arg, "صحيح\n");
}

TEST_F(BuiltinPrint, BoolArg_False)
{
    VM vm;
    Value arg = Value::from_bool(false);
    expect_print(vm, 1, &arg, "خطا\n");
}

TEST_F(BuiltinPrint, NilArg)
{
    VM vm;
    Value arg = Value::nil();
    expect_print(vm, 1, &arg, "عدم\n");
}

TEST_F(BuiltinPrint, TwoArgs_SeparatedByTab)
{
    VM vm;
    Value args[] = { str("a"), str("b") };
    expect_print(vm, 2, args, "a\tb\n");
}
