//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMClass, ConstructorRejectsWrongArgumentCount)
{
    std::string ast = "نوع يتطلب_وسيطة:\n"
                      "    دالة بداية(وسيطة): ارجع\n"
                      "دالة اختبار():\n"
                      "    كائن := يتطلب_وسيطة()\n"
                      "    ارجع كائن\n"
                      "اختبار()\n";

    VMRunner r;
    run_failed(r.compile_and_run(ast));
}

TEST(VMClass, ConstructorInitializesFieldsFromParameters)
{
    std::string ast = "نوع نقطة:\n"
                      "    دالة بداية(س, ص):\n"
                      "        .س := س\n"
                      "        .ص := ص\n"
                      "دالة اختبار():\n"
                      "    النقطة := نقطة(3, 4)\n"
                      "    ارجع النقطة\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_instance());

    ObjInstance* point = result.as_instance();
    ASSERT_EQ(point->fields.size(), 2);

    ASSERT_TRUE(point->fields[0].is_int());
    ASSERT_TRUE(point->fields[1].is_int());

    EXPECT_EQ(point->fields[0].as_int(), 3);
    EXPECT_EQ(point->fields[1].as_int(), 4);
}

TEST(VMClass, FieldGetExpressionReadsInstanceField)
{
    std::string ast = "نوع علبة:\n"
                      "    دالة بداية(قيمة): .قيمة := قيمة\n"
                      "دالة اختبار():\n"
                      "    العلبة := علبة(12)\n"
                      "    ارجع العلبة.قيمة\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 12);
}

TEST(VMClass, MethodReceivesExplicitArguments)
{
    std::string ast = "نوع جامع:\n"
                      "    دالة اجمع(ا, ب): ارجع ا + ب\n"
                      "دالة اختبار():\n"
                      "    الجامع := جامع()\n"
                      "    ارجع الجامع.اجمع(2, 5)\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 7);
}

TEST(VMClass, MethodReadsInstanceField)
{
    std::string ast = "نوع علبة:\n"
                      "    دالة بداية(قيمة): .قيمة := قيمة\n"
                      "    دالة اقرا_قيمة(): ارجع هذا.قيمة\n"
                      "دالة اختبار():\n"
                      "    العلبة := علبة(31)\n"
                      "    ارجع العلبة.اقرا_قيمة()\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 31);
}

TEST(VMClass, MethodMutatesInstanceFieldAndPersists)
{
    std::string ast = "نوع عداد:\n"
                      "    دالة بداية(): .عدد := 0\n"
                      "    دالة زد():\n"
                      "        .عدد := هذا.عدد + 1\n"
                      "        ارجع هذا.عدد\n"
                      "دالة اختبار():\n"
                      "    العداد := عداد()\n"
                      "    العداد.زد()\n"
                      "    ارجع العداد.زد()\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(VMClass, MultipleInstancesKeepIndependentFieldState)
{
    std::string ast = "نوع علبة:\n"
                      "    دالة بداية(قيمة): .قيمة := قيمة\n"
                      "    دالة اقرا_قيمة(): ارجع هذا.قيمة\n"
                      "دالة اختبار():\n"
                      "    يسار := علبة(10)\n"
                      "    يمين := علبة(20)\n"
                      "    ارجع يسار.اقرا_قيمة() + يمين.اقرا_قيمة()\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 30);
}

TEST(VMClass, MethodReturningNoValueReturnsSelf)
{
    std::string ast = "نوع متسلسل:\n"
                      "    دالة المس(): عدم\n"
                      "دالة اختبار():\n"
                      "    المتسلسل := متسلسل()\n"
                      "    ارجع المتسلسل.المس()\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_instance());
    EXPECT_EQ(result.as_instance()->klass->name, "متسلسل");
}

TEST(VMClass, MethodRejectsWrongArgumentCount)
{
    std::string ast = "نوع جامع:\n"
                      "    دالة اجمع(قيمة): ارجع قيمة\n"
                      "دالة اختبار():\n"
                      "    الجامع := جامع()\n"
                      "    ارجع الجامع.اجمع()\n"
                      "اختبار()\n";

    VMRunner r;
    run_failed(r.compile_and_run(ast));
}

TEST(VMClass, MultipleMethodsAreStoredInRuntimeClass)
{
    std::string ast = "نوع عمليات:\n"
                      "    دالة اول(): ارجع 1\n"
                      "    دالة ثان(): ارجع 2\n"
                      "دالة اختبار(): ارجع عمليات\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_class());

    ObjClass* klass_obj = result.as_class();
    EXPECT_GE(klass_obj->method_names.size(), static_cast<u32>(ObjClass::_COUNT + 2));
    EXPECT_GE(klass_obj->method_slot("اول"), 0);
    EXPECT_GE(klass_obj->method_slot("ثان"), 0);
}

TEST(VMClass, AddSpecialMethodHandlesBinaryPlus)
{
    std::string ast = "نوع شبيه_عدد:\n"
                      "    دالة عملية+(اخر): ارجع 99\n"
                      "دالة اختبار():\n"
                      "    يسار := شبيه_عدد()\n"
                      "    يمين := شبيه_عدد()\n"
                      "    ارجع يسار + يمين\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 99);
}
