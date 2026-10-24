//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinLen, MissingArgumentReturnsError)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("طول()\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(BuiltinLen, EmptyString)
{
    VM vm;
    auto s = str("");

    auto result = run_success(vm.len(1, &s));
    EXPECT_EQ(result.as_int(), 0);
}

TEST(BuiltinLen, NonEmptyString)
{
    VM vm;
    auto s = str("hello");
    auto result = run_success(vm.len(1, &s));
    EXPECT_EQ(result.as_int(), 5);
}
