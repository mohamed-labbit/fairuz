//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinSplit, BasicSplit)
{
    VM vm;
    Value args[] = { str("a,b,c"), str(",") };
    auto result = run_success(vm.split(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_list());
    EXPECT_EQ(v.as_list()->elements.size(), 3u);

    ASSERT_TRUE(v.as_list()->elements[0].is_string());
    ASSERT_TRUE(v.as_list()->elements[1].is_string());
    ASSERT_TRUE(v.as_list()->elements[2].is_string());

    EXPECT_EQ(std::string(v.as_list()->elements[0].as_string()->str.data()), "a");
    EXPECT_EQ(std::string(v.as_list()->elements[1].as_string()->str.data()), "b");
    EXPECT_EQ(std::string(v.as_list()->elements[2].as_string()->str.data()), "c");
}

TEST(BuiltinSplit, NoDelimiterFound)
{
    VM vm;
    Value args[] = { str("hello"), str(",") };
    auto result = run_success(vm.split(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_list());
    EXPECT_EQ(v.as_list()->elements.size(), 1u);
    ASSERT_TRUE(v.as_list()->elements[0].as_string());
    EXPECT_EQ(std::string(v.as_list()->elements[0].as_string()->str.data()), "hello");
}
