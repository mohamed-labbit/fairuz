//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies inline blocks and loop execution through the complete CLI pipeline.
//===----------------------------------------------------------------------===//

#include "support/cli_program.hpp"

TEST(InlineBlocksE2E, ConditionalsMixInlineAndIndentedBranches)
{
    test_source::SourceFile program("دالة اختر(س):\n"
                                    "    اذا س = 1: ارجع 10 # inline return\n"
                                    "    غيره اذا س = 2:\n"
                                    "        ارجع 20\n"
                                    "    غيره اذا س = 3: ارجع 30\n"
                                    "    غيره: ارجع 40\n"
                                    "تاكد اختر(1) = 10\n"
                                    "تاكد اختر(2) = 20\n"
                                    "تاكد اختر(3) = 30\n"
                                    "تاكد اختر(4) = 40\n"
                                    "س := 0\n"
                                    "اذا صحيح:\n"
                                    "    اذا خطا: س := 1\n"
                                    "    غيره: س := 2\n"
                                    "غيره: س := 3\n"
                                    "تاكد س = 2\n"
                                    "اذا خطا: اذا صحيح: س := 4\n"
                                    "غيره: س := 5\n"
                                    "تاكد س = 5\n"
                                    "اكتب(س)\n");
    auto result = test_cli::run(program.path);
    EXPECT_EQ(result.exit_code, 0) << result.err;
    EXPECT_EQ(result.out, "5\n");
}

TEST(InlineBlocksE2E, LoopsSupportAssignmentsBreakContinueAndReturn)
{
    test_source::SourceFile program("مجموع := 0\n"
                                    "لكل س في [1، 2، 3]: مجموع += س\n"
                                    "تاكد مجموع = 6\n"
                                    "عدد := 0\n"
                                    "طالما عدد < 3: عدد += 1\n"
                                    "تاكد عدد = 3\n"
                                    "طالما صحيح: اخرج\n"
                                    "لكل س في [1، 2، 3]: اكمل\n"
                                    "لكل س في [1، 2، 3]:\n"
                                    "    اذا س = 2: اكمل\n"
                                    "    مجموع += س\n"
                                    "تاكد مجموع = 10\n"
                                    "طالما عدد < 9:\n"
                                    "    عدد += 1\n"
                                    "    اذا عدد = 5: اكمل\n"
                                    "    اذا عدد = 7: اخرج\n"
                                    "تاكد عدد = 7\n"
                                    "لكل س في [1، 2]: لكل ص في [3، 4]: مجموع += س * ص\n"
                                    "تاكد مجموع = 31\n"
                                    "دالة اول(قيم):\n"
                                    "    لكل س في قيم: ارجع س\n"
                                    "    ارجع 0\n"
                                    "تاكد اول([7، 8]) = 7\n"
                                    "تاكد اول([]) = 0\n"
                                    "اكتب(مجموع)\n");
    auto result = test_cli::run(program.path);
    EXPECT_EQ(result.exit_code, 0) << result.err;
    EXPECT_EQ(result.out, "31\n");
}

TEST(InlineBlocksE2E, InlineClassesAndMethodsKeepFieldAndScopeSemantics)
{
    test_source::SourceFile program("نوع علبة: دالة بداية(قيمة): .قيمة := قيمة\n"
                                    "اول := علبة(9)\n"
                                    "تاكد اول.قيمة = 9\n"
                                    "نوع عداد:\n"
                                    "    دالة بداية(): .قيمة := 0\n"
                                    "    دالة زد(): .قيمة += 1\n"
                                    "    دالة اضبط(قيمة): اذا قيمة > 0: هذا.قيمة := قيمة\n"
                                    "    دالة اقرأ(): ارجع هذا.قيمة\n"
                                    "ثاني := عداد()\n"
                                    "ثاني.زد()\n"
                                    "تاكد ثاني.اقرأ() = 1\n"
                                    "ثاني.اضبط(12)\n"
                                    "تاكد ثاني.اقرأ() = 12\n"
                                    "ثاني.اضبط(0)\n"
                                    "تاكد ثاني.اقرأ() = 12\n"
                                    "دالة مضاعف(قيمة): ارجع قيمة * 2\n"
                                    "تاكد مضاعف(3) = 6\n"
                                    "اكتب(ثاني.اقرأ())\n");
    auto result = test_cli::run(program.path);
    EXPECT_EQ(result.exit_code, 0) << result.err;
    EXPECT_EQ(result.out, "12\n");
}

TEST(LoopsE2E, IterateForLoop)
{
    test_source::SourceFile program("ارقام := [1 , 2 , 3]\n"
                                    "لكل رقم في ارقام:\n"
                                    "    اكتب(رقم)");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out,
        "1\n"
        "2\n"
        "3\n");
}
