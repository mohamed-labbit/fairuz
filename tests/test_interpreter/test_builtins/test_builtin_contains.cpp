//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinContains, StringContains_True)
{
    VM vm;
    Value args[] = { str("hello world"), str("world") };
    auto result = run_success(vm.contains(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_TRUE(v.as_bool());
}

TEST(BuiltinContains, StringContains_False)
{
    VM vm;
    Value args[] = { str("hello"), str("xyz") };
    auto result = run_success(vm.contains(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_FALSE(v.as_bool());
}
