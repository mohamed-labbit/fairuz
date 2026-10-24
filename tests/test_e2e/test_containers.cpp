//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies container rendering and slicing through the complete CLI pipeline.
//===----------------------------------------------------------------------===//

#include "support/cli_program.hpp"

TEST(ContainersE2E, CyclicContainersRenderWithMarker)
{
    test_source::SourceFile program("س := قائمة()\n"
                                    "اضف(س، س)\n"
                                    "اكتب(س)\n"
                                    "اكتب(سلسلة(س))\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out,
        "[<cycle>]\n"
        "[<cycle>]\n");
}

TEST(ContainersE2E, DeepContainersRenderWithoutOverflowingTheHostStack)
{
    test_source::SourceFile program("س := []\n"
                                    "ع := 0\n"
                                    "طالما ع < 200:\n"
                                    "    س := [س]\n"
                                    "    ع += 1\n"
                                    "اكتب(س)\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("<max-depth>"), std::string::npos);
}

TEST(ContainersE2E, SliceUsesInclusiveEndForStringsAndLists)
{
    test_source::SourceFile program("اكتب(مقطع(\"أبج\", 1, 1))\n"
                                    "اكتب(مقطع([1, 2, 3], 1, 1))\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out,
        "ب\n"
        "[2]\n");
}

TEST(ContainersE2E, SliceAllowsAnEmptyContainerTail)
{
    test_source::SourceFile program("اكتب(مقطع([], 0))\n"
                                    "اكتب(مقطع(\"\", 0))\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out,
        "[]\n"
        "\n");
}
