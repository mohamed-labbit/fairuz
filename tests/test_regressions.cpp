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
    auto const base = std::filesystem::temp_directory_path() / std::filesystem::path("mylang_regression_XXXXXX");
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

std::filesystem::path binary_path()
{
    return std::filesystem::current_path() / "mylang";
}

std::filesystem::path repo_root()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

} // namespace

TEST(RegressionCli, DemoElseAcceptsTrailingTimeFlag)
{
    RunResult r = run_installed(binary_path(), repo_root() / "demo_else.txt", "--time");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("صحيح"), std::string::npos);
    EXPECT_NE(r.err.find("time:"), std::string::npos);
}

TEST(RegressionNatives, StringListDemoOutput)
{
    RunResult r = run_installed(binary_path(), repo_root() / "demo_strings_lists.txt");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("alpha|beta|gamma|delta"), std::string::npos);
    EXPECT_NE(r.out.find("true"), std::string::npos);
}

TEST(RegressionNatives, NumericDemoOutput)
{
    RunResult r = run_installed(binary_path(), repo_root() / "demo_numeric_natives.txt");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("1024"), std::string::npos);
    EXPECT_NE(r.out.find("190"), std::string::npos);
}
