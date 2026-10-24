//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinClock, ReturnsFiniteMonotonicSeconds)
{
    VM vm;
    Value first = run_success(vm.clock(0, nullptr));
    Value second = run_success(vm.clock(0, nullptr));
    ASSERT_TRUE(first.is_double());
    ASSERT_TRUE(second.is_double());
    EXPECT_TRUE(std::isfinite(first.as_double()));
    EXPECT_GE(second.as_double(), first.as_double());
    EXPECT_TRUE(run_success(vm.clock(0, &first)).is_double());
    VMRunner runner;
    run_failed(runner.compile_and_run("ساعة(1)\n"));
}
