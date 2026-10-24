//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMIntegration, FunctionLocalDeclarationShadowsGlobal)
{
    std::string make_local = "دالة انشئ_محليا():\n"
                             "    س := 2\n"
                             "    ارجع س\n";

    std::string read_global = "دالة اقرا_عام():\n"
                              "    ارجع س\n";

    std::string main_fn = "دالة رئيسي():\n"
                          "    ارجع [انشئ_محليا(), اقرا_عام()]\n";

    auto ast = Array<std::string> {
        "س := 1\n",
        make_local,
        read_global,
        main_fn,
        "نتيجة := رئيسي()\n"
        "تاكد(صنف(نتيجة) = \"قائمة\")\n"
        "تاكد(طول(نتيجة) = 2)\n"
        "تاكد(صنف(نتيجة[0]) = \"طبيعي\")\n"
        "تاكد(صنف(نتيجة[1]) = \"طبيعي\")\n"
        "تاكد(نتيجة[0] = 2)\n"
        "تاكد(نتيجة[1] = 1)\n",
    };
    EXPECT_EQ(test_source::run(test_source::join(ast)), ExitStatus::SUCCESS);
}

TEST(VMIntegration, TopLevelWhileAssignmentUpdatesGlobal)
{
    std::string read_global = "دالة اقرا_عام():\n"
                              "    ارجع س\n";

    auto ast = Array<std::string> {
        "س := 0\n",
        "حد := 3\n",
        "طالما (س < حد):\n"
        "    س := (س + 1)\n",
        read_global,
        "نتيجة := اقرا_عام()\n"
        "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
        "تاكد(نتيجة = 3)\n",
    };

    EXPECT_EQ(test_source::run(test_source::join(ast)), ExitStatus::SUCCESS);
}

TEST(VMIntegration, Fibonacci_fib10_equals_55)
{
    std::string fib = (std::string("دالة فيبوناتشي(ن):\n")
        + test_source::indent((std::string((std::string("اذا (ن <= 1):\n"
                                                        "    ارجع ن\n")
                                   + (std::string("غيره:\n") + test_source::indent({ }))))
            + "ارجع (فيبوناتشي((ن - 1)) + فيبوناتشي((ن - 2)))\n")));

    EXPECT_EQ(test_source::run(fib
                  + "نتيجة := فيبوناتشي(10)\n"
                    "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
                    "تاكد(نتيجة = 55)\n"),
        ExitStatus::SUCCESS);
}

TEST(VMIntegration, SumForLoopOverList)
{
    std::string sum = "دالة مجموع():\n"
                      "    عناصر := [1, 2, 3, 4, 5]\n"
                      "    اجمالي := 0\n"
                      "    لكل عنصر في عناصر:\n"
                      "        اجمالي := (اجمالي + عنصر)\n"
                      "    ارجع اجمالي\n";

    EXPECT_EQ(test_source::run(sum
                  + "نتيجة := مجموع()\n"
                    "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
                    "تاكد(نتيجة = 15)\n"),
        ExitStatus::SUCCESS);
}

TEST(VMIntegration, StringConcat_3Parts)
{
    std::string test = "دالة اختبار():\n"
                       "    ارجع ((\"hello\" + \", \") + \"world\")\n";

    EXPECT_EQ(test_source::run(test
                  + "نتيجة := اختبار()\n"
                    "تاكد(صنف(نتيجة) = \"سلسلة\")\n"
                    "تاكد(نتيجة = \"hello, world\")\n"),
        ExitStatus::SUCCESS);
}

TEST(VMIntegration, EmptyForLoopLeavesStateUnchanged)
{
    std::string first = "دالة اول():\n"
                        "    عناصر := []\n"
                        "    مرئي := 99\n"
                        "    لكل عنصر في عناصر:\n"
                        "        مرئي := عنصر\n"
                        "    ارجع مرئي\n";

    EXPECT_EQ(test_source::run(first
                  + "نتيجة := اول()\n"
                    "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
                    "تاكد(نتيجة = 99)\n"),
        ExitStatus::SUCCESS);
}

TEST(VMIntegration, BreakAndContinueWorkInLoops)
{
    std::string test = "دالة اختبار():\n"
                       "    عناصر := [1, 2, 3, 4, 5]\n"
                       "    اجمالي := 0\n"
                       "    لكل عنصر في عناصر:\n"
                       "        اذا (عنصر = 2):\n"
                       "            اكمل\n"
                       "        اذا (عنصر = 5):\n"
                       "            اخرج\n"
                       "        اجمالي := (اجمالي + عنصر)\n"
                       "    ارجع اجمالي\n";

    EXPECT_EQ(test_source::run(test
                  + "نتيجة := اختبار()\n"
                    "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
                    "تاكد(نتيجة = 8)\n"),
        ExitStatus::SUCCESS);
}
