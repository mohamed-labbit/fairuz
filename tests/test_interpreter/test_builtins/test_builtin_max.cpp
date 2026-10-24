//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinMax, OneArg_ReturnsArg)
{
    VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = Value::from_int(static_cast<i64>(rand()));
    auto result = run_success(vm.max(1, &n));
    Value v = result;
    EXPECT_EQ(v.as_int(), n.as_int());
}

TEST(BuiltinMax, TwoPositiveIntegers)
{
    VM vm;
    Value args[] = { Value::from_int(3), Value::from_int(7) };
    auto result = run_success(vm.max(2, args));
    Value v = result;
    EXPECT_EQ(v.as_int(), 7);
}

TEST(BuiltinMax, NegativeValues)
{
    VM vm;
    Value args[] = { Value::from_int(-3), Value::from_int(-1) };
    auto result = run_success(vm.max(2, args));
    Value v = result;
    EXPECT_EQ(v.as_int(), -1);
}

TEST(BuiltinMax, AllIntegersReturnsInteger)
{
    VM vm;
    Value args[] = { Value::from_int(1), Value::from_int(9), Value::from_int(4) };
    auto result = run_success(vm.max(3, args));
    Value v = result;
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 9);
}

TEST(BuiltinMax, MixedFloatAndInteger)
{
    VM vm;
    Value args[] = { Value::from_int(3), Value::from_real(3.5) };
    auto result = run_success(vm.max(2, args));
    Value v = result;
    EXPECT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.5);
}

TEST(BuiltinMax, StringFirstArg_ReturnsArg)
{
    VM vm;
    Value args[] = { str("a"), str("b") };
    auto result = run_success(vm.max(2, args));
    Value v = result;
    EXPECT_EQ(v.as_string()->str, "b");
}
