//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeMathDispatch, SupportsDocumentedUnaryOperations)
{
    VM vm;
    Value sine_args[] = { str("sin"), Value::from_real(0.0) };
    Value exp_args[] = { str("exp"), Value::from_real(0.0) };
    EXPECT_DOUBLE_EQ(run_success(vm.math_unary(2, sine_args)).as_double(), 0.0);
    EXPECT_DOUBLE_EQ(run_success(vm.math_unary(2, exp_args)).as_double(), 1.0);
}

TEST(NativeMathDispatch, SupportsDocumentedBinaryOperations)
{
    VM vm;
    Value hypot_args[] = { str("hypot"), Value::from_int(3), Value::from_int(4) };
    Value atan_args[] = { str("atan2"), Value::from_real(0.0), Value::from_real(1.0) };
    EXPECT_DOUBLE_EQ(run_success(vm.math_binary(3, hypot_args)).as_double(), 5.0);
    EXPECT_DOUBLE_EQ(run_success(vm.math_binary(3, atan_args)).as_double(), 0.0);
}

TEST(NativeMathDispatch, RejectsBadTypesAndUnknownOperations)
{
    VM vm;
    Value bad_type[] = { str("sin"), str("not-ا-number") };
    Value unknown[] = { str("unknown"), Value::from_int(1) };
    EXPECT_EQ(run_failed(vm.math_unary(2, bad_type)).errc, ErrorCode::NATIVE_TYPE_ERROR);
    EXPECT_EQ(run_failed(vm.math_unary(2, unknown)).errc, ErrorCode::RUNTIME_ERROR);
    EXPECT_EQ(run_failed(vm.math_binary(0, nullptr)).errc, ErrorCode::NATIVE_ARG_COUNT);
}
