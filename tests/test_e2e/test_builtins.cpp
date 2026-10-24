//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies builtin output and invalid argument handling through the complete CLI pipeline.
//===----------------------------------------------------------------------===//

#include "support/cli_program.hpp"

TEST(BuiltinsE2E, StringListDemoOutput)
{
    test_source::SourceFile program("اكتب(اجمع([\"alpha\", \"beta\", \"gamma\", \"delta\"], \"|\"))\n"
                                    "اكتب(يحتوي(\"alphabet\", \"alpha\"))\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("alpha|beta|gamma|delta"), std::string::npos);
    EXPECT_NE(r.out.find("صحيح"), std::string::npos);
}

TEST(BuiltinsE2E, NumericDemoOutput)
{
    test_source::SourceFile program("اكتب(ادنى(1024.9))\n"
                                    "اكتب(اعلى(189.1))\n");
    test_process::Result r = test_cli::run(program.path);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("1024"), std::string::npos);
    EXPECT_NE(r.out.find("190"), std::string::npos);
}

TEST(BuiltinsE2E, BuiltinBoundaryRejectsBadArgumentsWithoutCrash)
{
    test_source::SourceFile open_bad("اكتب(افتح(\"missing\"))\n");
    test_process::Result open_result = test_cli::run(open_bad.path);
    EXPECT_EQ(open_result.exit_code, 65);

    test_source::SourceFile substr_bad("اكتب(جزء(1, 0, 1))\n");
    test_process::Result substr_result = test_cli::run(substr_bad.path);
    EXPECT_EQ(substr_result.exit_code, 65);

    test_source::SourceFile pop_bad("احذف([])\n");
    test_process::Result pop_result = test_cli::run(pop_bad.path);
    EXPECT_EQ(pop_result.exit_code, 65);

    test_source::SourceFile min_bad("اكتب(اصغر(1، \"x\"))\n");
    test_process::Result min_result = test_cli::run(min_bad.path);
    EXPECT_EQ(min_result.exit_code, 65);

    auto output = std::filesystem::temp_directory_path()
        / ("fairuz_closed_handle_" + std::to_string(test_process::process_id()) + ".txt");
    std::string file_source = "م := افتح(\"" + fairuz::platform::utf8(output)
        + "\", \"اكتب\")\n"
          "اغلق(م)\n"
          "اضف_ملف(م، \"x\")\n";
    test_source::SourceFile closed_file(file_source);
    test_process::Result closed_result = test_cli::run(closed_file.path);
    EXPECT_EQ(closed_result.exit_code, 65);
    std::filesystem::remove(output);
}
