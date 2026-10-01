#include "test_process.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

namespace {

using RunResult = test_process::Result;

std::string read_file(std::filesystem::path const& path)
{
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

RunResult run_cli(std::vector<std::string> const& args)
{
    auto result = test_process::run(test_process::executable(), args);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.crashed) << result.err;
    return result;
}

std::filesystem::path write_program(std::string const& source)
{
    auto path = std::filesystem::temp_directory_path() / fairuz::platform::path("fairuz_cli_program_" + std::to_string(test_process::process_id()) + ".ف");
    std::ofstream out(path);
    out << source;
    return path;
}

} // namespace

TEST(CliE2E, Help)
{
    RunResult r = run_cli({ "--help" });
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("Usage:"), std::string::npos);
    EXPECT_NE(r.out.find("--dump-bytecode"), std::string::npos);
}

TEST(CliE2E, Version)
{
    RunResult r = run_cli({ "--version" });
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("fairuz 0.1.0"), std::string::npos);
}

TEST(CliE2E, CheckOnly)
{
    auto program = write_program("ا := 42\n");
    RunResult r = run_cli({ "--check", fairuz::platform::utf8(program) });
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_TRUE(r.out.empty());
}

TEST(CliE2E, FileThenTrailingOption)
{
    auto program = write_program("اذا 20 + 5 < 400:\n    اكتب(\"صحيح\")\nغيره:\n    اكتب(\"خطأ\")\n");
    RunResult r = run_cli({ fairuz::platform::utf8(program), "--time" });
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("صحيح"), std::string::npos);
    EXPECT_NE(r.err.find("time:"), std::string::npos);
}

TEST(CliE2E, ValidProgramDoesNotWriteDiagnostics)
{
    auto program = write_program("اذا 1 <= 2:\n    اكتب(1)\n");
    RunResult r = run_cli({ fairuz::platform::utf8(program) });
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "1\n");
    EXPECT_TRUE(r.err.empty());
    std::filesystem::remove(program);
}

TEST(CliE2E, MissingFile)
{
    RunResult r = run_cli({ fairuz::platform::utf8(std::filesystem::temp_directory_path() / "definitely_missing_fairuz_input.fa") });
    EXPECT_EQ(r.exit_code, 66);
    EXPECT_NE(r.err.find("Input file not found"), std::string::npos);
}

TEST(CliE2E, FormatDoesNotOverwriteInvalidSource)
{
    std::string const source = "اذا صحيح\n    اكتب(1)\n";
    auto program = write_program(source);

    RunResult r = run_cli({ "format", fairuz::platform::utf8(program) });

    EXPECT_EQ(r.exit_code, 65);
    EXPECT_EQ(read_file(program), source);
    std::filesystem::remove(program);
}

TEST(CliE2E, FormatCheckReportsChangesWithoutWriting)
{
    std::string const source = "ن:=1\n";
    auto program = write_program(source);
    auto path = fairuz::platform::utf8(program);

    RunResult needed = run_cli({ "format", "--check", path });
    EXPECT_EQ(needed.exit_code, 1);
    EXPECT_NE(needed.out.find(path), std::string::npos);
    EXPECT_EQ(read_file(program), source);

    EXPECT_EQ(run_cli({ "format", path }).exit_code, 0);
    RunResult clean = run_cli({ "format", "--check", path });
    EXPECT_EQ(clean.exit_code, 0);
    EXPECT_TRUE(clean.out.empty());
    std::filesystem::remove(program);
}

TEST(CliE2E, FormatDirectoryReportsInvalidSource)
{
    auto directory = std::filesystem::temp_directory_path()
        / ("fairuz_format_dir_" + std::to_string(test_process::process_id()));
    std::filesystem::create_directory(directory);
    auto invalid = directory / fairuz::platform::path("invalid.ف");
    std::string const source = "اذا صحيح\n    اكتب(1)\n";
    {
        std::ofstream out(invalid);
        out << source;
    }
    RunResult result = run_cli({ "format", fairuz::platform::utf8(directory) });
    EXPECT_EQ(result.exit_code, 65);
    EXPECT_EQ(read_file(invalid), source);
    std::filesystem::remove_all(directory);
}

TEST(CliE2E, FormatCheckDirectoryReportsFilesInStableOrder)
{
    auto directory = std::filesystem::temp_directory_path()
        / ("fairuz_format_check_dir_" + std::to_string(test_process::process_id()));
    std::filesystem::create_directory(directory);
    auto first = directory / fairuz::platform::path("a.ف");
    auto second = directory / fairuz::platform::path("b.ف");
    {
        std::ofstream out(second);
        out << "ب:=2\n";
    }
    {
        std::ofstream out(first);
        out << "ا:=1\n";
    }
    RunResult result = run_cli({ "format", "--check", fairuz::platform::utf8(directory) });
    EXPECT_EQ(result.exit_code, 1);
    EXPECT_EQ(result.out, fairuz::platform::utf8(first) + "\n" + fairuz::platform::utf8(second) + "\n");
    EXPECT_EQ(read_file(first), "ا:=1\n");
    EXPECT_EQ(read_file(second), "ب:=2\n");
    std::filesystem::remove_all(directory);
}

TEST(CliE2E, DiagnosticsEscapeTerminalControlBytes)
{
    std::string source = "ا := 1";
    source.push_back('\x1b');
    source += "[2J\n";
    auto program = write_program(source);

    RunResult r = run_cli({ "--check", fairuz::platform::utf8(program) });

    EXPECT_EQ(r.exit_code, 65);
    EXPECT_EQ(r.err.find('\x1b'), std::string::npos);
    EXPECT_NE(r.err.find("\\x1B"), std::string::npos);
    std::filesystem::remove(program);
}

/*
TEST(CliE2E, SyntaxRecoveryReportsOnceAndNeverExecutesPartialProgram)
{
    auto program = write_program("س :=\nص :=\nاكتب(\"must not run\")\n");
    RunResult r = run_cli({fairuz::platform::utf8(program)});
    EXPECT_EQ(r.exit_code, 65);
    EXPECT_TRUE(r.out.empty());
    auto first = r.err.find("error: SyntaxError:");
    ASSERT_NE(first, std::string::npos);
    auto second = r.err.find("error: SyntaxError:", first + 1);
    ASSERT_NE(second, std::string::npos);
    EXPECT_EQ(r.err.find("error: SyntaxError:", second + 1), std::string::npos);
    std::filesystem::remove(program);
}

TEST(CliE2E, JsonDiagnosticsPreserveProgramOutputAndEscapeDetails)
{
    auto program = write_program("اكتب(42)\nتاكد(خطا، \"quote\\\"\\nline\")\n");
    RunResult r = run_cli({"--diagnostics=json", fairuz::platform::utf8(program)});
    EXPECT_EQ(r.exit_code, 65);
    EXPECT_EQ(r.out, "42\n");
    ASSERT_FALSE(r.err.empty());
    EXPECT_EQ(r.err.front(), '[');
    EXPECT_NE(r.err.find("\"type\":\"AssertionError\""), std::string::npos);
    EXPECT_NE(r.err.find("quote\\\"\\u000aline"), std::string::npos);
    EXPECT_NE(r.err.find("\"traceback\":[{"), std::string::npos);
    EXPECT_EQ(r.err.find("Traceback ("), std::string::npos);
    std::filesystem::remove(program);
}

TEST(CliE2E, NameErrorsSuggestSimilarArabicNames)
{
    auto program = write_program("المجموع := 42\nاكتب(المجمو)\n");
    RunResult r = run_cli({fairuz::platform::utf8(program)});
    EXPECT_EQ(r.exit_code, 65);
    EXPECT_NE(r.err.find("NameError:"), std::string::npos);
    EXPECT_NE(r.err.find("Did you mean 'المجموع'?"), std::string::npos);
    std::filesystem::remove(program);
}

TEST(CliE2E, ArithmeticErrorsIdentifyOperatorAndOperandTypes)
{
    auto program = write_program("س := \"text\"\nص := 2\nاكتب(س + ص)\n");
    RunResult r = run_cli({fairuz::platform::utf8(program)});
    EXPECT_EQ(r.exit_code, 65);
    EXPECT_NE(r.err.find("TypeError:"), std::string::npos);
    EXPECT_NE(r.err.find("operator '+' received سلسلة and طبيعي"), std::string::npos);
    std::filesystem::remove(program);
}
*/

TEST(CliE2E, FormatPreservesFilePermissions)
{
    auto program = write_program("ا := [1,2,3]\n");
    std::filesystem::permissions(program, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    auto permissions = std::filesystem::status(program).permissions();

    RunResult r = run_cli({ "format", fairuz::platform::utf8(program) });

    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(std::filesystem::status(program).permissions(), permissions);
    std::filesystem::remove(program);
}

TEST(CliE2E, FormatRejectsSymbolicLinks)
{
    auto target = write_program("ا := 1\n");
    auto link = target;
    link += ".link";
    std::filesystem::remove(link);
    std::error_code error;
    std::filesystem::create_symlink(target, link, error);
    ASSERT_FALSE(error) << error.message();

    RunResult r = run_cli({ "format", fairuz::platform::utf8(link) });

    EXPECT_NE(r.exit_code, 0);
    EXPECT_EQ(read_file(target), "ا := 1\n");
    std::filesystem::remove(link);
    std::filesystem::remove(target);
}

TEST(CliE2E, FormatPreservesProgramBehaviorAndIsIdempotent)
{
    auto program = write_program(
        "# formatting must not erase this\n"
        "ن:=0\n"
        "لكل عنصر في [1,2,3]:\n"
        "  ن+=عنصر\n"
        "تاكد ن=6, 'sum'\n"
        "اكتب(ن, 0.0000001, (1+2)*3)\n");
    auto before = run_cli({ fairuz::platform::utf8(program) });
    ASSERT_EQ(before.exit_code, 0) << before.err;
    auto result = run_cli({ "format", fairuz::platform::utf8(program) });
    ASSERT_EQ(result.exit_code, 0) << result.err;
    auto once = read_file(program);
    EXPECT_NE(once.find("# formatting must not erase this"), std::string::npos);
    EXPECT_NE(once.find("ن += عنصر"), std::string::npos);
    auto after = run_cli({ fairuz::platform::utf8(program) });
    EXPECT_EQ(after.exit_code, before.exit_code) << after.err;
    EXPECT_EQ(after.out, before.out);
    EXPECT_EQ(run_cli({ "format", fairuz::platform::utf8(program) }).exit_code, 0);
    EXPECT_EQ(read_file(program), once);
    std::filesystem::remove(program);
}
