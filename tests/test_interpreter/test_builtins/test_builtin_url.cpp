//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeUrl, EncodesAndDecodesUtf8Bytes)
{
    VM vm;
    Value input = str("لغة فيروز/1");
    Value encoded = run_success(vm.url_encode(1, &input));
    ASSERT_TRUE(encoded.is_string());
    Value decoded = run_success(vm.url_decode(1, &encoded));
    ASSERT_TRUE(decoded.is_string());
    EXPECT_EQ(decoded.as_string()->str, input.as_string()->str);
    Value malformed = str("%GG");
    EXPECT_EQ(run_failed(vm.url_decode(1, &malformed)).errc, ErrorCode::RUNTIME_ERROR);
}

TEST(NativeUrl, ParsesAndRebuildsStructuredUrl)
{
    VM vm;
    Value input = str("https://example.test:443/api?q=1#part");
    Value parsed = run_success(vm.url_parse(1, &input));
    ASSERT_TRUE(parsed.is_dict());
    Value port = vm.dict_get(&parsed, str("port"));
    ASSERT_TRUE(port.is_int());
    EXPECT_EQ(port.as_int(), 443);
    Value rebuilt = run_success(vm.url_build(1, &parsed));
    ASSERT_TRUE(rebuilt.is_string());
    EXPECT_EQ(rebuilt.as_string()->str, input.as_string()->str);
}

TEST(NativeUrl, RejectsInvalidBoundaryArguments)
{
    VM vm;
    Value number = Value::from_int(42);
    EXPECT_EQ(run_failed(vm.url_encode(1, &number)).errc, ErrorCode::NATIVE_TYPE_ERROR);
    EXPECT_EQ(run_failed(vm.url_parse(1, &number)).errc, ErrorCode::NATIVE_TYPE_ERROR);
    EXPECT_EQ(run_failed(vm.url_build(1, &number)).errc, ErrorCode::NATIVE_TYPE_ERROR);
}
