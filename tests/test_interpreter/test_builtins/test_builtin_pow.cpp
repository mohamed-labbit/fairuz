//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativePow, BasicSquare)
{
    VM vm;
    Value args[] = { Value::from_real(3.0), Value::from_real(2.0) };
    auto result = run_success(vm.pow(2, args));
    Value v = result;
    if (!v.is_nil()) EXPECT_DOUBLE_EQ(v.as_double(), 9.0);
}

TEST(NativePow, ZeroExponent)
{
    VM vm;
    Value args[] = { Value::from_real(5.0), Value::from_real(0.0) };
    auto result = run_success(vm.pow(2, args));
    Value v = result;
    if (!v.is_nil()) EXPECT_DOUBLE_EQ(v.as_double(), 1.0);
}

TEST(NativePow, NegativeExponent)
{
    VM vm;
    Value args[] = { Value::from_real(2.0), Value::from_real(-1.0) };
    auto result = run_success(vm.pow(2, args));
    Value v = result;
    if (!v.is_nil()) EXPECT_DOUBLE_EQ(v.as_double(), 0.5);
}
