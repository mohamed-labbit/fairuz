#include "fplatform.hpp"
#include "test_integer_oracle.hpp"
#include "test_process.hpp"
#include <fstream>
#include <gtest/gtest.h>

namespace {

class PlatformTest : public ::testing::Test {
protected:
    std::filesystem::path directory;
    void SetUp() override { directory = test_process::temporary_directory(); }
    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }
};

}

TEST_F(PlatformTest, UnicodePathsWorkForCliImportsFilesAndFormatting)
{
    using fairuz::platform::path;
    using fairuz::platform::utf8;
    auto cwd = directory / path("مشروع عربي 😀 with spaces");
    ASSERT_TRUE(std::filesystem::create_directory(cwd));
    auto main = cwd / path("برنامج.ف");
    {
        std::ofstream module(cwd / path("وحدة.ف"), std::ios::binary);
        module << "القيمة := 42\n";
        std::ofstream source(main, std::ios::binary);
        source << "من وحدة استورد القيمة\n"
                  "من ملفات استورد اكتب_نص_ملف، اقرا_نص_ملف\n"
                  "تاكد(اكتب_نص_ملف(\"بيانات 😀.txt\"، \"مرحبا\"))\n"
                  "تاكد(اقرا_نص_ملف(\"بيانات 😀.txt\") = \"مرحبا\")\n"
                  "اكتب(القيمة)\n";
    }
    EXPECT_EQ(utf8(path(utf8(main))), utf8(main));
    auto result = test_process::run(test_process::executable(), { utf8(main) }, cwd);
    ASSERT_EQ(result.exit_code, 0) << result.err;
    EXPECT_EQ(result.out, "42\n");
    result = test_process::run(test_process::executable(), { "format", utf8(main) }, cwd);
    ASSERT_EQ(result.exit_code, 0) << result.err;
    result = test_process::run(test_process::executable(), { utf8(main) }, cwd);
    EXPECT_EQ(result.exit_code, 0) << result.err;
    EXPECT_EQ(result.out, "42\n");
}

TEST(PlatformProcess, PreservesArgumentsOutputAndExitCode)
{
    std::vector<std::string> args { "", "with spaces", "a\"b", "trailing\\", "back\\\"quote", "مرحبا 😀", "& | %PATH% $HOME" };
    std::string expected;
    for (auto const& arg : args)
        expected += std::to_string(arg.size()) + ":" + arg + "\n";
    auto result = test_process::run(fairuz::platform::path(FAIRUZ_TEST_PROCESS_PROBE), args);
    EXPECT_FALSE(result.crashed);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.exit_code, 23);
    EXPECT_EQ(result.out, expected);
    EXPECT_EQ(result.err, "probe stderr\n");
}

TEST(PlatformProcess, TerminatesAndReapsTimedOutChild)
{
    auto result = test_process::run(fairuz::platform::path(FAIRUZ_TEST_PROCESS_PROBE),
        { "sleep" }, { }, std::chrono::seconds(1));
    EXPECT_TRUE(result.timed_out);
    EXPECT_NE(result.exit_code, 0);
}

TEST(PortableIntegerOracle, HandlesSignedBoundaries)
{
    EXPECT_EQ(integer_oracle::add(INT64_MAX, INT64_MAX), "18446744073709551614");
    EXPECT_EQ(integer_oracle::add(INT64_MIN, INT64_MIN), "-18446744073709551616");
    EXPECT_EQ(integer_oracle::add(INT64_MIN, INT64_MAX), "-1");
    EXPECT_EQ(integer_oracle::add(INT64_MAX, INT64_MIN, true), "18446744073709551615");
    EXPECT_EQ(integer_oracle::add(INT64_MIN, INT64_MIN, true), "0");
    EXPECT_EQ(integer_oracle::multiply(INT64_MIN, INT64_MIN), "85070591730234615865843651857942052864");
    EXPECT_EQ(integer_oracle::multiply(INT64_MIN, INT64_MAX), "-85070591730234615856620279821087277056");
    EXPECT_EQ(integer_oracle::multiply(INT64_MIN, 0), "0");
}
