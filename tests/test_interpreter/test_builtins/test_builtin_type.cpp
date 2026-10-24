//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeType, ReturnsTypeNames)
{
    VM vm;
    Value i = Value::from_int(0);
    Value f = Value::from_real(0.0);
    Value b = Value::from_bool(false);
    Value n = Value::nil();
    Value s = str("x");

    EXPECT_EQ(run_success(vm.type(1, &i)).as_string()->str, "طبيعي");
    EXPECT_EQ(run_success(vm.type(1, &f)).as_string()->str, "حقيقي");
    EXPECT_EQ(run_success(vm.type(1, &b)).as_string()->str, "منطقي");
    EXPECT_EQ(run_success(vm.type(1, &n)).as_string()->str, "عدم");
    EXPECT_EQ(run_success(vm.type(1, &s)).as_string()->str, "سلسلة");
}
