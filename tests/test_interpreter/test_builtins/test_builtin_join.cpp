//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(NativeJoin, BasicJoin)
{
    VM vm;
    Value list = run_success(vm.list(0, nullptr));
    ObjList* l = list.as_list();
    l->elements.push(str("a"));
    l->elements.push(str("b"));
    l->elements.push(str("c"));
    Value args[] = { list, str("|") };
    auto result = run_success(vm.join(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "a|b|c");
}
