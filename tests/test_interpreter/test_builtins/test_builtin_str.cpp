//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeStr, NoArgs_ReturnsEmpty)
{
    VM vm;
    auto result = run_success(vm.str(0, nullptr));
    EXPECT_TRUE(result.is_string());
}

TEST(NativeStr, Integer)
{
    VM vm;
    Value arg = Value::from_int(42);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "42");
}

TEST(NativeStr, NegativeInteger)
{
    VM vm;
    Value arg = Value::from_int(-7);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "-7");
}

TEST(NativeStr, Zero)
{
    VM vm;
    Value arg = Value::from_int(0);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "0");
}

TEST(NativeStr, BoolTrue)
{
    VM vm;
    Value arg = Value::from_bool(true);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "صحيح");
}

TEST(NativeStr, BoolFalse)
{
    VM vm;
    Value arg = Value::from_bool(false);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "خطا");
}

TEST(NativeStr, Float)
{
    VM vm;
    Value arg = Value::from_real(1.5);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    char const* text_ptr = v.as_string()->str.data();
    f64 parsed = 0.0;
    auto [end_ptr, ec] = std::from_chars(text_ptr, text_ptr + v.as_string()->str.len(), parsed);
    ASSERT_EQ(ec, std::errc());
    ASSERT_EQ(end_ptr, text_ptr + v.as_string()->str.len());
    EXPECT_DOUBLE_EQ(parsed, 1.5);
}

TEST(NativeStr, StringPassthrough)
{
    VM vm;
    Value s = str("hello");
    auto result = run_success(vm.str(1, &s));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "hello");
}
