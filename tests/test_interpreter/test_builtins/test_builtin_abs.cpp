//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinAbs, PositiveInteger)
{
    VM vm;
    Value arg = Value::from_int(5);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 5);
}

TEST(BuiltinAbs, NegativeInteger)
{
    VM vm;
    Value arg = Value::from_int(-5);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 5);
}

TEST(BuiltinAbs, ZeroInteger)
{
    VM vm;
    Value arg = Value::from_int(0);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 0);
}

TEST(BuiltinAbs, PositiveFloat)
{
    VM vm;
    Value arg = Value::from_real(3.5);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.5);
}

TEST(BuiltinAbs, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-3.5);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.5);
}
