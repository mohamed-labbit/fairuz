//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeInt, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(7);
    auto result = run_success(vm.Int(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 7);
}

TEST(NativeInt, FloatTruncates)
{
    VM vm;
    Value arg = Value::from_real(3.9);
    auto result = run_success(vm.Int(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 3);
}

TEST(NativeInt, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-2.7);
    auto result = run_success(vm.Int(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -2);
}
