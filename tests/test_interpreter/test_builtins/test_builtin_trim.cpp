//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeTrim, LeadingAndTrailingSpaces)
{
    VM vm;
    Value arg = str("  hello  ");
    auto result = run_success(vm.trim(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "hello");
}

TEST(NativeTrim, NoSpaces)
{
    VM vm;
    Value arg = str("hello");
    auto result = run_success(vm.trim(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "hello");
}

TEST(NativeTrim, OnlySpaces)
{
    VM vm;
    Value arg = str("   ");
    auto result = run_success(vm.trim(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "");
}
