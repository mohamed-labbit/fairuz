#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <sys/wait.h>

namespace {

struct RunResult {
    int exit_code;
    std::string out;
    std::string err;
};

std::string shell_quote(std::string const& s)
{
    return "'" + s + "'";
}

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
    auto out_path = dir_path / "stdout.txt";
    auto err_path = dir_path / "stderr.txt";
    std::string cmd = shell_quote(binary.string()) + " " + shell_quote(input.string());
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

std::filesystem::path repo_root()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path();
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
    auto program = write_program("اكتب(join([\"alpha\", \"beta\", \"gamma\", \"delta\"], \"|\"))\nاكتب(contains(\"alphabet\", \"alpha\"))\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("alpha|beta|gamma|delta"), std::string::npos);
    EXPECT_NE(r.out.find("true"), std::string::npos);
}

TEST(RegressionNatives, NumericDemoOutput)
{
    auto program = write_program("اكتب(floor(1024.9))\nاكتب(ceil(189.1))\n");
    RunResult r = run_installed(binary_path(), program);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("1024"), std::string::npos);
    EXPECT_NE(r.out.find("190"), std::string::npos);
}
