//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies arbitrary-precision integer semantics and GC survival through the complete CLI pipeline.
//===----------------------------------------------------------------------===//

#include "support/cli_program.hpp"

#include <algorithm>
#include <iterator>
#include <sstream>

TEST(IntegerRuntimeContract, IntegerOverflowPromotes)
{
    test_source::SourceFile program("س := 9223372036854775807\n"
                                    "اكتب(س + 1)\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "9223372036854775808\n");
}

TEST(IntegerRuntimeContract, LiteralsBuiltinsAndCollections)
{
    test_source::SourceFile program("س := 340282366920938463463374607431768211455\n"
                                    "اكتب(س + 1)\n"
                                    "اكتب(0x100000000000000000000000000000000)\n"
                                    "اكتب(١٨٤٤٦٧٤٤٠٧٣٧٠٩٥٥١٦١٦)\n"
                                    "اكتب(سلسلة(س))\n"
                                    "اكتب(ادنى(18446744073709551616.0))\n"
                                    "اكتب(اعلى(-18446744073709551616.0))\n"
                                    "اكتب(قوة(2، 128))\n"
                                    "د := {9007199254740992: 10، 9007199254740993: 20}\n"
                                    "اكتب(طول(د))\n"
                                    "اكتب(د[9007199254740992.0])\n"
                                    "اكتب(د[9007199254740993])\n"
                                    "اكتب(9007199254740993 = 9007199254740992.0)\n"
                                    "اكتب(9007199254740993 > 9007199254740992.0)\n"
                                    "اكتب(-3 >> 1)\n"
                                    "ن := 128\n"
                                    "اكتب(1 << ن)\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0) << r.err;
    EXPECT_EQ(r.out,
        "340282366920938463463374607431768211456\n"
        "340282366920938463463374607431768211456\n"
        "18446744073709551616\n"
        "340282366920938463463374607431768211455\n"
        "18446744073709551616\n"
        "-18446744073709551616\n"
        "340282366920938463463374607431768211456\n"
        "2\n"
        "10\n"
        "20\n"
        "خطا\n"
        "صحيح\n"
        "-2\n"
        "340282366920938463463374607431768211456\n");
}

TEST(IntegerRuntimeContract, OperandsAndLiteralsSurviveCollections)
{
    test_source::SourceFile program("دالة ثابت():\n"
                                    "    ارجع 340282366920938463463374607431768211456\n"
                                    "اصل := ثابت()\n"
                                    "ق := [اصل]\n"
                                    "س := اصل\n"
                                    "ن := 0\n"
                                    "طالما ن < 4000:\n"
                                    "    س := س + 1\n"
                                    "    ن += 1\n"
                                    "اكتب(س - اصل)\n"
                                    "اكتب(ق[0] = ثابت())\n"
                                    "اكتب(اصل)\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0) << r.err;
    EXPECT_EQ(r.out,
        "4000\n"
        "صحيح\n"
        "340282366920938463463374607431768211456\n");
}

TEST(IntegerRuntimeContract, PythonOracle1400Comparisons)
{
    struct OracleCase {
        char const* expression;
        char const* expected;
    };
    static constexpr OracleCase cases[] = {
#include "test_parser/test_cases/integer_oracle.inc"
    };
    static_assert(std::size(cases) == 1400);
    // Small batches stay below the compiler's per-function inline-cache limit.
    // Every case exercises lexing, parsing, bytecode execution, GC, and printing.
    constexpr size_t batch_size = 40;
    for (size_t start = 0; start < std::size(cases); start += batch_size) {
        std::string source;
        size_t end = std::min(start + batch_size, std::size(cases));
        for (size_t i = start; i < end; ++i) source += std::string("اكتب(") + cases[i].expression + ")\n";
        test_source::SourceFile program(source);
        test_process::Result result = test_cli::run(program.path);
        ASSERT_EQ(result.exit_code, 0) << "batch starting at case " << start << "\n" << result.err;
        std::istringstream output(result.out);
        for (size_t i = start; i < end; ++i) {
            SCOPED_TRACE("oracle case " + std::to_string(i) + ": " + cases[i].expression);
            std::string actual;
            ASSERT_TRUE(static_cast<bool>(std::getline(output, actual)));
            EXPECT_EQ(actual, cases[i].expected);
        }
        std::string extra;
        EXPECT_FALSE(static_cast<bool>(std::getline(output, extra))) << extra;
    }
}
