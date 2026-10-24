//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeSubstr, BasicSubstr)
{
    VM vm; // substr is exclusive
    Value args[] = { str("hello"), Value::from_int(1), Value::from_int(4) };
    auto result = run_success(vm.substr(3, args));
    Value v = result;
    if (!v.is_nil()) {
        ASSERT_TRUE(v.is_string());
        EXPECT_EQ(std::string(v.as_string()->str.data()), "ell");
    }
}

TEST(NativeSubstr, FromStart)
{
    VM vm; // substr is exclusive
    Value args[] = { str("hello"), Value::from_int(0), Value::from_int(3) };
    auto result = run_success(vm.substr(3, args));
    Value v = result;
    if (!v.is_nil()) {
        ASSERT_TRUE(v.is_string());
        EXPECT_EQ(std::string(v.as_string()->str.data()), "hel");
    }
}
