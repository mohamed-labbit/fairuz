//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies numeric literals, optimization, and bitwise assignments through the complete CLI pipeline.
//===----------------------------------------------------------------------===//

#include "support/cli_program.hpp"

TEST(NumericSemanticsE2E, NumericLiteralsUseCorrectBaseAndUtf8Cursor)
{
    test_source::SourceFile program("اكتب(0xFF)\n"
                                    "اكتب(0b1010)\n"
                                    "اكتب(0o17)\n"
                                    "اكتب(١٢)\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out,
        "255\n"
        "10\n"
        "15\n"
        "12\n");
}

TEST(NumericSemanticsE2E, OptimizerPreservesRuntimeTypesAndErrors)
{
    test_source::SourceFile equality("اكتب(1 = 1.0)\n");
    test_process::Result eq = test_cli::run(equality.path);
    EXPECT_EQ(eq.exit_code, 0);
    EXPECT_EQ(eq.out, "صحيح\n");

    test_source::SourceFile invalid("اكتب(صحيح + 1)\n");
    test_process::Result bad = test_cli::run(invalid.path);
    EXPECT_EQ(bad.exit_code, 65);
}

TEST(NumericSemanticsE2E, StrengthReductionDoesNotChangeMeaning)
{
    test_source::SourceFile valid("س := 5\n"
                                  "اكتب(~~س)\n");
    test_process::Result ok = test_cli::run(valid.path);
    EXPECT_EQ(ok.exit_code, 0);
    EXPECT_EQ(ok.out, "5\n");
}

TEST(NumericSemanticsE2E, BitwiseCompoundAssignments)
{
    std::string src = "س := 1\n"
                      "ص := 2\n"
                      "س &= ص\n"
                      "اكتب(س)\n"
                      "س |= ص\n"
                      "اكتب(س)\n"
                      "س ^= ص\n"
                      "اكتب(س)\n"
                      "س <<= ص\n"
                      "اكتب(س)\n"
                      "س >>= ص\n"
                      "اكتب(س)\n";

    test_source::SourceFile program(src);
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out,
        "0\n"
        "2\n"
        "0\n"
        "0\n"
        "0\n");
}
