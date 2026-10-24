//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeSqrt, PerfectSquare)
{
    VM vm;
    Value arg = Value::from_real(9.0);
    auto result = run_success(vm.sqrt(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.0);
}

TEST(NativeSqrt, Zero)
{
    VM vm;
    Value arg = Value::from_real(0.0);
    auto result = run_success(vm.sqrt(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 0.0);
}

TEST(NativeSqrt, NegativeInput_SpecBehavior)
{
    VM vm;
    Value arg = Value::from_real(-1.0);
    EXPECT_EQ(run_failed(vm.sqrt(1, &arg)).errc, ErrorCode::RUNTIME_ERROR);
}
