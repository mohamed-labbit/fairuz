//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinFloat, IntegerToFloat)
{
    VM vm;
    Value arg = Value::from_int(3);
    auto result = run_success(vm.Float(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.0);
}

TEST(BuiltinFloat, FloatPassthrough)
{
    VM vm;
    Value arg = Value::from_real(2.5);
    auto result = run_success(vm.Float(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 2.5);
}

TEST(BuiltinFloat, NegativeInteger)
{
    VM vm;
    Value arg = Value::from_int(-10);
    auto result = run_success(vm.Float(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), -10.0);
}
