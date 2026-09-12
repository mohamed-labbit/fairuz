#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

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

std::filesystem::path test_binary()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "build" / "fairuz";
}

RunResult run_cli(std::string const& m_args)
{
    auto const base = std::filesystem::temp_directory_path() / std::filesystem::path("fairuz_cli_XXXXXX");
    std::string tmpl = base.string();
    std::vector<char> writable(tmpl.begin(), tmpl.end());
    writable.push_back('\0');
    char* dir = mkdtemp(writable.data());
    EXPECT_NE(dir, nullptr);

    std::filesystem::path dir_path(dir);
    auto out_path = dir_path / "stdout.fa";
    auto err_path = dir_path / "stderr.fa";

    // The parent fairuz_tests process performs the suite-level leak scan.
    // Running a second LeakSanitizer scan in every ASan-instrumented child is
    // unreliable on macOS (the nested fork/exit stop-the-world pass can
    // intermittently abort). Keep AddressSanitizer active in the interpreter
    // subprocess while disabling only its redundant leak-at-exit phase.
    std::string cmd = "ASAN_OPTIONS=detect_leaks=0 " + shell_quote(test_binary().string())
        + " " + m_args + " >" + shell_quote(out_path.string()) + " 2>" + shell_quote(err_path.string());
    int raw = std::system(cmd.c_str());
    int code = WIFEXITED(raw) ? WEXITSTATUS(raw) : raw;

    RunResult result { code, read_file(out_path), read_file(err_path) };
    std::filesystem::remove_all(dir_path);
    return result;
}

std::filesystem::path write_program(std::string const& source)
{
    auto path = std::filesystem::temp_directory_path() / ("fairuz_cli_program_" + std::to_string(::getpid()) + ".fa");
    std::ofstream out(path);
    out << source;
    return path;
}

} // namespace

TEST(CliE2E, Help)
{
    RunResult r = run_cli("--help");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("Usage:"), std::string::npos);
    EXPECT_NE(r.out.find("--dump-bytecode"), std::string::npos);
}

TEST(CliE2E, Version)
{
    RunResult r = run_cli("--version");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("fairuz 0.1.0"), std::string::npos);
}

TEST(CliE2E, CheckOnly)
{
    auto program = write_program("ا := 42\n");
    RunResult r = run_cli("--check " + shell_quote(program.string()));
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_TRUE(r.out.empty());
}

TEST(CliE2E, FileThenTrailingOption)
{
    auto program = write_program("اذا 20 + 5 < 400:\n    اكتب(\"صحيح\")\nغيره:\n    اكتب(\"خطأ\")\n");
    RunResult r = run_cli(shell_quote(program.string()) + " --time");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("صحيح"), std::string::npos);
    EXPECT_NE(r.err.find("time:"), std::string::npos);
}

TEST(CliE2E, ValidProgramDoesNotWriteDiagnostics)
{
    auto program = write_program("اذا 1 <= 2:\n    اكتب(1)\n");
    RunResult r = run_cli(shell_quote(program.string()));
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.out, "1\n");
    EXPECT_TRUE(r.err.empty());
    std::filesystem::remove(program);
}

TEST(CliE2E, MissingFile)
{
    RunResult r = run_cli(shell_quote("/tmp/definitely_missing_fairuz_input.fa"));
    EXPECT_EQ(r.exit_code, 66);
    EXPECT_NE(r.err.find("Input file not found"), std::string::npos);
}

TEST(CliE2E, FormatDoesNotOverwriteInvalidSource)
{
    std::string const source = "اذا صحيح\n    اكتب(1)\n";
    auto program = write_program(source);

    RunResult r = run_cli("format " + shell_quote(program.string()));

    EXPECT_EQ(r.exit_code, 65);
    EXPECT_EQ(read_file(program), source);
    std::filesystem::remove(program);
}

TEST(CliE2E, DiagnosticsEscapeTerminalControlBytes)
{
    std::string source = "ا := 1";
    source.push_back('\x1b');
    source += "[2J\n";
    auto program = write_program(source);

    RunResult r = run_cli("--check " + shell_quote(program.string()));

    EXPECT_EQ(r.exit_code, 65);
    EXPECT_EQ(r.err.find('\x1b'), std::string::npos);
    EXPECT_NE(r.err.find("\\x1B"), std::string::npos);
    std::filesystem::remove(program);
}

TEST(CliE2E, FormatPreservesFilePermissions)
{
    auto program = write_program("ا := [1,2,3]\n");
    ASSERT_EQ(::chmod(program.c_str(), 0600), 0);

    RunResult r = run_cli("format " + shell_quote(program.string()));

    struct stat info { };
    ASSERT_EQ(::stat(program.c_str(), &info), 0);
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(info.st_mode & 0777, 0600);
    std::filesystem::remove(program);
}

TEST(CliE2E, FormatRejectsSymbolicLinks)
{
    auto target = write_program("ا := 1\n");
    auto link = target;
    link += ".link";
    std::filesystem::remove(link);
    ASSERT_EQ(::symlink(target.c_str(), link.c_str()), 0);

    RunResult r = run_cli("format " + shell_quote(link.string()));

    EXPECT_EQ(r.exit_code, 70);
    EXPECT_NE(r.err.find("symbolic link"), std::string::npos);
    EXPECT_EQ(read_file(target), "ا := 1\n");
    std::filesystem::remove(link);
    std::filesystem::remove(target);
}
