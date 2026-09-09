#include "../fairuz/../fairuz/farena.hpp"
#include "test_config.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#    include <io.h>
#    define ISATTY _isatty
#    define FILENO _fileno
#else
#    include <unistd.h>
#    define ISATTY isatty
#    define FILENO fileno
#endif

using namespace fairuz;

namespace test_config {

bool print_ast = false;
bool verbose = false;
bool dump_bytecode = false;

} // namespace test_config

namespace {

constexpr char const* kReset = "\033[0m";
constexpr char const* kGreen = "\033[32m";
constexpr char const* kRed = "\033[31m";

// Standard "[i/n] Suite.Test" line, rewritten in place while everything is
// passing (green), then red for the rest of the run after the first
// failure. A passing test's own stdout/stderr (error-path prints, AST
// dumps, disassembly, etc.) is captured and thrown away -- it only gets
// printed if that specific test actually fails.
class NinjaStyleListener : public ::testing::EmptyTestEventListener {
public:
    NinjaStyleListener()
        : is_tty_(ISATTY(FILENO(stdout)) != 0)
    {
    }

    void OnTestProgramStart(::testing::UnitTest const& unit_test) override
    {
        total_ = unit_test.total_test_count();
        current_ = 0;
        any_failed_ = false;
    }

    void OnTestStart(::testing::TestInfo const& test_info) override
    {
        ++current_;
        failures_.clear();

        // Print (and flush) the line FIRST, while nothing is captured, so
        // it actually reaches the terminal.
        print_progress_line(test_info);

        // Only now start swallowing whatever the test itself prints.
        ::testing::internal::CaptureStdout();
        ::testing::internal::CaptureStderr();
    }

    void OnTestPartResult(::testing::TestPartResult const& result) override
    {
        // stdout is captured right now -- stash this, print it for real
        // from OnTestEnd once capturing has stopped.
        if (!result.failed())
            return;

        std::ostringstream msg;
        msg << (result.file_name() ? result.file_name() : "unknown file")
            << ":" << result.line_number() << "\n"
            << result.summary();
        failures_.push_back(msg.str());
    }

    void OnTestEnd(::testing::TestInfo const& test_info) override
    {
        std::string out = ::testing::internal::GetCapturedStdout();
        std::string err = ::testing::internal::GetCapturedStderr();

        if (!test_info.result()->Failed())
            return; // passed -- discard captured output, line stays as-is

        any_failed_ = true;

        // Break out of single-line mode so this failure gets a permanent
        // block instead of being overwritten by the next progress line.
        std::cout << "\n";
        if (is_tty_)
            std::cout << kRed;
        std::cout << "[FAILED] " << test_info.test_suite_name() << "." << test_info.name();
        if (is_tty_)
            std::cout << kReset;
        std::cout << "\n";

        for (auto const& failure : failures_)
            std::cout << failure << "\n\n";
        if (!out.empty())
            std::cout << "----- stdout -----\n"
                      << out << "\n";
        if (!err.empty())
            std::cout << "----- stderr -----\n"
                      << err << "\n";

        std::cout << std::flush;
    }

    void OnTestProgramEnd(::testing::UnitTest const& unit_test) override
    {
        std::cout << "\n";
        if (is_tty_)
            std::cout << (unit_test.Passed() ? kGreen : kRed);
        std::string failed_string = unit_test.failed_test_case_count() != 0 ? " failed from "
                + std::to_string(unit_test.failed_test_suite_count()) + " test suite"
                                                                            : " failed";
        std::cout << "==== " << unit_test.successful_test_count() << "/" << unit_test.total_test_count()
                  << " tests passed from " << unit_test.successful_test_suite_count() << " test suite ("
                  << unit_test.failed_test_count() << failed_string << ", "
                  << unit_test.skipped_test_count() << " skipped)" << " ====";
        if (is_tty_)
            std::cout << kReset;
        std::cout << "\n"
                  << std::flush;
    }

private:
    void print_progress_line(::testing::TestInfo const& test_info)
    {
        std::ostringstream line;
        if (is_tty_)
            line << "\r\033[K" << (any_failed_ ? kRed : kGreen);
        line << "[" << current_ << "/" << total_ << "] "
             << test_info.test_suite_name() << "." << test_info.name();
        if (is_tty_)
            line << kReset;
        else
            line << "\n"; // no overwrite available -- one line per test
        std::cout << line.str() << std::flush;
    }

    bool is_tty_;
    int total_ = 0;
    int current_ = 0;
    bool any_failed_ = false;
    std::vector<std::string> failures_;
};

} // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    fairuz::Fa_AllocatorContext g_ctx;
    fairuz::set_context(&g_ctx);

    for (int i = 1; i < argc; i += 1) {
        std::string arg = argv[i];

        if (arg == "--print-ast")
            test_config::print_ast = true;
        else if (arg == "--dump-bytecode")
            test_config::dump_bytecode = true;
        else if (arg == "-v")
            test_config::verbose = true;
        else {
            std::cerr << "main: unknown option " << arg << std::endl;
            return 1;
        }
    }

    auto& listeners = ::testing::UnitTest::GetInstance()->listeners();

    if (test_config::verbose || test_config::dump_bytecode) {
        // Keep gtest's normal reporting -- you asked to see everything, so
        // don't fight it for control of stdout.
    } else {
        // Remove gtest's default reporter; otherwise its own
        // "[ RUN ]"/"[ OK ]" lines print alongside ours.
        delete listeners.Release(listeners.default_result_printer());
        listeners.Append(new NinjaStyleListener());
    }

    std::cout << "-- If you want to enable performance tests, set TEST_PERF=1 or TEST_PERF=true\n";

    int ret = RUN_ALL_TESTS();
    fairuz::g_context = nullptr;

    return ret;
}
