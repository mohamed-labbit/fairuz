#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

struct RunResult {
    int exit_code;
    std::string out;
    std::string err;
};

std::string shell_quote(std::string const& s) { return "'" + s + "'"; }

std::string read_file(std::filesystem::path const& path)
{
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

RunResult run_installed(std::filesystem::path const& binary, std::filesystem::path const& input, std::string const& extra = "")
{
    auto const base = std::filesystem::temp_directory_path() / std::filesystem::path("fairuz_regression_XXXXXX");
    std::string tmpl = base.string();
    std::vector<char> writable(tmpl.begin(), tmpl.end());
    writable.push_back('\0');
    char* dir = mkdtemp(writable.data());
    EXPECT_NE(dir, nullptr);

    std::filesystem::path dir_path(dir);
    auto out_path = dir_path / "stdout.fa";
    auto err_path = dir_path / "stderr.fa";
    // LeakSanitizer remains enabled for the parent suite. Avoid nested leak
    // scans in repeatedly spawned interpreter processes; ASan itself remains
    // active and still reports memory-safety violations from these children.
    std::string cmd = "ASAN_OPTIONS=detect_leaks=0 " + shell_quote(binary.string())
        + " " + shell_quote(input.string());
    if (!extra.empty())
        cmd += " " + extra;
    cmd += " >" + shell_quote(out_path.string()) + " 2>" + shell_quote(err_path.string());

    int raw = std::system(cmd.c_str());
    int code = WIFEXITED(raw) ? WEXITSTATUS(raw) : raw;

    RunResult result { code, read_file(out_path), read_file(err_path) };
    std::filesystem::remove_all(dir_path);
    return result;
}

std::filesystem::path write_program(std::string const& source)
{
    auto path = std::filesystem::temp_directory_path() / ("fairuz_regression_program_" + std::to_string(::getpid()) + ".fa");
    std::ofstream out(path);
    out << source;
    return path;
}

std::filesystem::path binary_path()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "build" / "fairuz";
}

} // namespace

TEST(RegressionCli, DemoElseAcceptsTrailingTimeFlag)
{
    auto program = write_program("اذا 20 + 5 < 400:\n    اكتب(\"صحيح\")\nغيره:\n    اكتب(\"خطأ\")\n");
    RunResult r = run_installed(binary_path(), program, "--time");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("صحيح"), std::string::npos);
    EXPECT_NE(r.err.find("time:"), std::string::npos);
}

TEST(RegressionNatives, StringListDemoOutput)
{
    auto program = write_program("اكتب(اجمع([\"alpha\", \"beta\", \"gamma\", \"delta\"], \"|\"))\nاكتب(يحتوي(\"alphabet\", \"alpha\"))\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("alpha|beta|gamma|delta"), std::string::npos);
    EXPECT_NE(r.out.find("صحيح"), std::string::npos);
}

TEST(RegressionNatives, NumericDemoOutput)
{
    auto program = write_program("اكتب(ادنى(1024.9))\nاكتب(اعلى(189.1))\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("1024"), std::string::npos);
    EXPECT_NE(r.out.find("190"), std::string::npos);
}


TEST(HardeningRegression, NumericLiteralsUseCorrectBaseAndUtf8Cursor)
{
    auto program = write_program("اكتب(0xFF)\nاكتب(0b1010)\nاكتب(0o17)\nاكتب(١٢)\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "255\n10\n15\n12\n");
}

TEST(HardeningRegression, OptimizerPreservesRuntimeTypesAndErrors)
{
    auto equality = write_program("اكتب(1 = 1.0)\n");
    RunResult eq = run_installed(binary_path(), equality);
    EXPECT_EQ(eq.exit_code, 0);
    EXPECT_EQ(eq.out, "صحيح\n");

    auto invalid = write_program("اكتب(صحيح + 1)\n");
    RunResult bad = run_installed(binary_path(), invalid);
    EXPECT_EQ(bad.exit_code, 65);
}

TEST(HardeningRegression, StrengthReductionDoesNotChangeMeaning)
{
    auto valid = write_program("س := 5\nاكتب(~~س)\n");
    RunResult ok = run_installed(binary_path(), valid);
    EXPECT_EQ(ok.exit_code, 0);
    EXPECT_EQ(ok.out, "5\n");

    auto invalid = write_program("اكتب(مفقود * 0)\n");
    RunResult bad = run_installed(binary_path(), invalid);
    EXPECT_EQ(bad.exit_code, 65);
}

TEST(HardeningRegression, NativeBoundaryRejectsBadArgumentsWithoutCrash)
{
    auto open_bad = write_program("اكتب(افتح(\"missing\"))\n");
    RunResult open_result = run_installed(binary_path(), open_bad);
    EXPECT_EQ(open_result.exit_code, 65);

    auto substr_bad = write_program("اكتب(جزء(1, 0, 1))\n");
    RunResult substr_result = run_installed(binary_path(), substr_bad);
    EXPECT_EQ(substr_result.exit_code, 65);

    auto pop_bad = write_program("احذف([])\n");
    RunResult pop_result = run_installed(binary_path(), pop_bad);
    EXPECT_EQ(pop_result.exit_code, 65);

    auto min_bad = write_program("اكتب(اصغر(1، \"x\"))\n");
    RunResult min_result = run_installed(binary_path(), min_bad);
    EXPECT_EQ(min_result.exit_code, 65);

    auto output = std::filesystem::temp_directory_path()
        / ("fairuz_closed_handle_" + std::to_string(::getpid()) + ".txt");
    std::string file_source = "م := افتح(\"" + output.string()
        + "\", \"اكتب\")\nاغلق(م)\nاضف_ملف(م، \"x\")\n";
    auto closed_file = write_program(file_source);
    RunResult closed_result = run_installed(binary_path(), closed_file);
    EXPECT_EQ(closed_result.exit_code, 65);
    std::filesystem::remove(output);
}

TEST(HardeningRegression, CyclicContainersRenderWithMarker)
{
    auto program = write_program("س := قائمة()\nاضف(س، س)\nاكتب(س)\nاكتب(سلسلة(س))\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "[<cycle>]\n[<cycle>]\n");
}

TEST(HardeningRegression, DeepContainersRenderWithoutOverflowingTheHostStack)
{
    auto program = write_program(
        "س := []\n"
        "ع := 0\n"
        "طالما ع < 200:\n"
        "    س := [س]\n"
        "    ع += 1\n"
        "اكتب(س)\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("<max-depth>"), std::string::npos);
}

TEST(HardeningRegression, SliceUsesInclusiveEndForStringsAndLists)
{
    auto program = write_program("اكتب(مقطع(\"abc\", 1, 1))\nاكتب(مقطع([1, 2, 3], 1, 1))\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "b\n[2]\n");
}

TEST(HardeningRegression, SliceAllowsAnEmptyContainerTail)
{
    auto program = write_program("اكتب(مقطع([], 0))\nاكتب(مقطع(\"\", 0))\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "[]\n\n");
}

TEST(HardeningRegression, EmbeddedNulIsRejectedRatherThanTruncatingSource)
{
    std::string source = "اكتب(1)\n";
    source.push_back('\0');
    source += "اكتب(2)\n";
    auto program = write_program(source);
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 65);
    EXPECT_NE(r.err.find("Invalid character"), std::string::npos);
}

TEST(HardeningRegression, IntegerOverflowIsDiagnosed)
{
    auto program = write_program("س := 140737488355327\nاكتب(س + 1)\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 65);
    EXPECT_NE(r.err.find("signed 48-bit"), std::string::npos);
}

TEST(RegressionOperators, OpShift)
{
    std::string src = "x := 1\n"
                      "y := 2\n"
                      "اكتب(x &= y)\n"
                      "اكتب(x |= y)\n"
                      "اكتب(x ^= y)\n"
                      "اكتب(x <<= y)\n"
                      "اكتب(x >>= y)\n";

    auto program = write_program(src);
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "0\n2\n0\n0\n0\n");
}

TEST(RegressionConstruct, IterateForLoop)
{
    std::string src = "ارقام := [1 , 2 , 3]\n"
                      "لكل رقم في ارقام:\n"
                      "    اكتب(رقم)\n";
    auto program = write_program("ارقام := [1 , 2 , 3]\nلكل رقم في ارقام:\n    اكتب(رقم)");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "1\n2\n3\n");
}
