//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinPrint, NoArgs_PrintsNewline)
{
    VM vm;
    auto result = run_success(vm.print(0, nullptr));
    EXPECT_TRUE(result.is_truthy());
}

TEST(BuiltinPrint, StringArg)
{
    VM vm;
    auto s = str("hello world");
    auto result = run_success(vm.print(1, &s));
    EXPECT_TRUE(result.is_truthy());
}

TEST(BuiltinPrint, IntegerArg)
{
    VM vm;
    Value arg = Value::from_int(42);
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(BuiltinPrint, FloatArg)
{
    VM vm;
    Value arg = Value::from_real(3.14);
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(BuiltinPrint, BoolArg_True)
{
    VM vm;
    Value arg = Value::from_bool(true);
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(BuiltinPrint, BoolArg_False)
{
    VM vm;
    Value arg = Value::from_bool(false);
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(BuiltinPrint, NilArg)
{
    VM vm;
    Value arg = Value::nil();
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(BuiltinPrint, TwoArgs_DoesNotCrash)
{
    VM vm;
    auto s = str("a");
    Value args[] = { s, s };
    auto result = run_success(vm.print(2, args));
    EXPECT_TRUE(result.is_truthy());
}
