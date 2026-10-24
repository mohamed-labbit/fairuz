//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeRegex, CompilesSearchesMatchesAndCaptures)
{
    VM vm;
    Value compile_args[] = { str("([a-z])([0-9]+)"), Value::from_int(0) };
    Value handle = run_success(vm.regex_compile(2, compile_args));
    ASSERT_TRUE(handle.is_string());
    Value search_args[] = { handle, str("قبل a12 بعد"), Value::from_int(0) };
    Value found = run_success(vm.regex_search(3, search_args));
    ASSERT_TRUE(found.is_dict());
    EXPECT_EQ(vm.dict_get(&found, str("start")).as_int(), 4);
    Value groups = vm.dict_get(&found, str("groups"));
    ASSERT_TRUE(groups.is_list());
    ASSERT_EQ(groups.as_list()->elements.size(), 3u);
    EXPECT_EQ(groups.as_list()->elements[1].as_string()->str, StringRef("a"));

    Value match_args[] = { handle, str("a12 tail"), Value::from_int(0) };
    EXPECT_TRUE(run_success(vm.regex_match(3, match_args)).is_dict());
    Value full_args[] = { handle, str("a12") };
    EXPECT_TRUE(run_success(vm.regex_fullmatch(2, full_args)).is_dict());
}

TEST(NativeRegex, FindsAllSplitsAndReplacesWithLimits)
{
    VM vm;
    Value handle = str("[0-9]+");
    Value all_args[] = { handle, str("a1 b22 c333") };
    Value all = run_success(vm.regex_findall(2, all_args));
    ASSERT_TRUE(all.is_list());
    EXPECT_EQ(all.as_list()->elements.size(), 3u);

    Value split_args[] = { handle, str("a1b22c"), Value::from_int(1) };
    Value split = run_success(vm.regex_split(3, split_args));
    ASSERT_TRUE(split.is_list());
    EXPECT_EQ(split.as_list()->elements.size(), 2u);

    Value replace_args[] = { handle, str("a1b22c"), str("#"), Value::from_int(1) };
    Value replaced = run_success(vm.regex_replace(4, replace_args));
    ASSERT_TRUE(replaced.is_string());
    EXPECT_EQ(replaced.as_string()->str, StringRef("a#b22c"));
}

TEST(NativeRegex, RejectsInvalidPatternsAndArguments)
{
    VM vm;
    Value invalid[] = { str("["), Value::from_int(0) };
    EXPECT_EQ(run_failed(vm.regex_compile(2, invalid)).errc, ErrorCode::RUNTIME_ERROR);
    EXPECT_EQ(run_failed(vm.regex_search(0, nullptr)).errc, ErrorCode::NATIVE_ARG_COUNT);
    EXPECT_EQ(run_failed(vm.regex_split(0, nullptr)).errc, ErrorCode::NATIVE_ARG_COUNT);
    EXPECT_EQ(run_failed(vm.regex_replace(0, nullptr)).errc, ErrorCode::NATIVE_ARG_COUNT);
}
