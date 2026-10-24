//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMDicts, IndexReturnsStoredValue)
{
    VMRunner r;
    auto ast = Array<std::string> {
        "دالة دالة_اختبار():\n"
        "    مفتاح := \"lang\"\n"
        "    قاموس := {}\n"
        "    قاموس[مفتاح] := 7\n"
        "    س := قاموس[مفتاح]\n"
        "    ارجع س\n",
        "دالة_اختبار()\n",
    };

    Value result = run_success(r.compile_and_run(ast));

    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 7);
}

TEST(VMDicts, SetUpdatesAndAppendsByKey)
{
    VMRunner r;
    auto ast = Array<std::string> {
        "دالة دالة_اختبار():\n"
        "    قاموس := {}\n"
        "    قاموس[\"fk\"] := 1\n"
        "    قاموس[\"sk\"] := 2\n"
        "    ارجع [قاموس[\"fk\"], قاموس[\"sk\"]]\n",
        "دالة_اختبار()\n",
    };

    Value result = run_success(r.compile_and_run(ast));
    ASSERT_TRUE(result.is_list());
    ObjList* ret_obj = result.as_list();
    ASSERT_EQ(ret_obj->size(), 2);
    EXPECT_EQ(ret_obj->elements[0].as_int(), 1);
    EXPECT_EQ(ret_obj->elements[1].as_int(), 2);
}

TEST(VMDicts, MissingKeyReturnsNil)
{
    VMRunner r;
    auto ast = Array<std::string> {
        "س := {}[\"missing\"]\n",
    };

    EXPECT_TRUE(run_success(r.compile_and_run(ast)).is_nil());
}
