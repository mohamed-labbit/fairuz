//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeMin, OneArg_ReturnsArg)
{
    VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = Value::from_int(static_cast<i64>(rand()));
    auto result = run_success(vm.min(1, &n));
    Value v = result;
    EXPECT_EQ(v.as_int(), n.as_int());
}

TEST(NativeMin, TwoPositiveIntegers)
{
    VM vm;
    Value args[] = { Value::from_int(3), Value::from_int(7) };
    auto result = run_success(vm.min(2, args));
    Value v = result;
    EXPECT_EQ(v.as_int(), 3);
}

TEST(NativeMin, AllIntegersReturnsInteger)
{
    VM vm;
    Value args[] = { Value::from_int(5), Value::from_int(2), Value::from_int(8) };
    auto result = run_success(vm.min(3, args));
    Value v = result;
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 2);
}

TEST(NativeMin, MixedFloatAndInteger_ReturnsFloat)
{
    VM vm;
    Value args[] = { Value::from_int(3), Value::from_real(1.5) };
    auto result = run_success(vm.min(2, args));
    Value v = result;
    EXPECT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 1.5);
}

TEST(NativeMin, NegativeValues)
{
    VM vm;
    Value args[] = { Value::from_int(-1), Value::from_int(-5), Value::from_int(-2) };
    auto result = run_success(vm.min(3, args));
    Value v = result;
    EXPECT_EQ(v.as_int(), -5);
}
