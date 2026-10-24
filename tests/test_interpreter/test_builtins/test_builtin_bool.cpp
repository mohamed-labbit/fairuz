//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinBool, TrueBoolean)
{
    VM vm;
    Value arg = Value::from_bool(true);
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_TRUE(v.as_bool());
}

TEST(BuiltinBool, FalseBoolean)
{
    VM vm;
    Value arg = Value::from_bool(false);
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_FALSE(v.as_bool());
}

TEST(BuiltinBool, NonZeroInteger_IsTrue)
{
    VM vm;
    Value arg = Value::from_int(1);
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_TRUE(v.as_bool());
}

TEST(BuiltinBool, ZeroInteger_IsFalsy)
{
    VM vm;
    Value arg = Value::from_int(0);
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_FALSE(v.as_bool());
}

TEST(BuiltinBool, NilArg)
{
    VM vm;
    Value arg = Value::nil();
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_FALSE(v.as_bool());
}

TEST(BuiltinBool, NonEmptyString_IsTrue)
{
    VM vm;
    Value arg = str("hi");
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_TRUE(v.as_bool());
}
