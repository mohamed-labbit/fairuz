//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinRound, HalfRoundsUp)
{
    VM vm;
    Value arg = Value::from_real(2.5);
    auto result = run_success(vm.round(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 3);
}

TEST(BuiltinRound, HalfNegativeRoundsDown)
{
    VM vm;
    Value arg = Value::from_real(-2.5);
    auto result = run_success(vm.round(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -3);
}

TEST(BuiltinRound, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(4);
    auto result = run_success(vm.round(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 4);
}
