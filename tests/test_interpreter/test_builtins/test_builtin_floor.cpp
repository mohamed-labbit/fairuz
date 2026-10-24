//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinFloor, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(5);
    auto result = run_success(vm.floor(1, &arg));
    Value v = result;
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 5);
}

TEST(BuiltinFloor, PositiveFloat)
{
    VM vm;
    Value arg = Value::from_real(3.7);
    auto result = run_success(vm.floor(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 3);
}

TEST(BuiltinFloor, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-2.3);
    auto result = run_success(vm.floor(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -3);
}

TEST(BuiltinFloor, ExactFloat)
{
    VM vm;
    Value arg = Value::from_real(4.0);
    auto result = run_success(vm.floor(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 4);
}
