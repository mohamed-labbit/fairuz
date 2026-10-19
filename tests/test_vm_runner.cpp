//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies the file-runner boundary, diagnostics, and host-state lifetime.
//===----------------------------------------------------------------------===//

#include "Diag/diagnostic.hpp"
#include "Platform.hpp"
#include "SourceManager.hpp"
#include "Support/arena.hpp"
#include "Support/string.hpp"
#include "VMRunner.hpp"
#include "VMRunner.hpp" // The public header must be safe to include repeatedly.
#include "test_process.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>

using namespace fairuz;
using namespace fairuz::runtime;

namespace {

// Capture iostream output without nesting GoogleTest's file-descriptor
// capture, which the repository's progress listener already uses.
class StreamCapture {
public:
    explicit StreamCapture(std::ostream& stream)
        : m_stream(stream)
        , m_previous(stream.rdbuf(m_output.rdbuf()))
    {
    }
    ~StreamCapture() { m_stream.rdbuf(m_previous); }

    std::string str() const { return m_output.str(); }

    StreamCapture(StreamCapture const&) = delete;
    StreamCapture& operator=(StreamCapture const&) = delete;

private:
    std::ostream& m_stream;
    std::ostringstream m_output;
    std::streambuf* m_previous;
};

size_t occurrences(std::string const& output, std::string const& text)
{
    size_t count = 0;
    for (size_t position = 0; (position = output.find(text, position)) != std::string::npos; position += text.size())
        ++count;
    return count;
}

class VMRunnerBoundaryTest : public ::testing::Test {
protected:
    std::filesystem::path directory;
    AllocatorContext* previous_allocator { nullptr };
    std::optional<diagnostic::DiagnosticEngine> previous_diagnostics;

    void SetUp() override
    {
        previous_allocator = g_context;
        previous_diagnostics.emplace(std::move(diagnostic::engine));
        diagnostic::reset();
        diagnostic::engine.set_json_output(false);
        directory = test_process::temporary_directory();
    }

    void TearDown() override
    {
        set_context(previous_allocator);
        diagnostic::engine = std::move(*previous_diagnostics);
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    std::string write(std::string const& name, std::string const& source)
    {
        auto path = directory / platform::path(name);
        std::ofstream file(path, std::ios::binary);
        file << source;
        file.close();
        if (!file)
            throw std::runtime_error("Could not write VMRunner test source");
        return platform::utf8(path);
    }
};

TEST_F(VMRunnerBoundaryTest, SuccessUsesConventionalStatusAndDoesNotPrintTheReturnedValue)
{
    static_assert(static_cast<int>(ExitStatus::SUCCESS) == 0);
    static_assert(static_cast<int>(ExitStatus::ERROR) != 0);

    auto path = write("program.fa", "تاكد 6 * 7 = 42\nاكتب(42)\n");
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(path.c_str()), ExitStatus::SUCCESS);
    EXPECT_EQ(out.str(), "42\n");
    EXPECT_TRUE(err.str().empty());
    EXPECT_EQ(g_context, previous_allocator);
    EXPECT_FALSE(diagnostic::has_errors());
}

TEST_F(VMRunnerBoundaryTest, EmptyNilAndFalseProgramsCompleteSuccessfully)
{
    for (char const* source : { "", "عدم\n", "خطا\n" }) {
        SCOPED_TRACE(source);
        auto path = write("empty_or_value.fa", source);
        EXPECT_EQ(run_protected(path.c_str()), ExitStatus::SUCCESS);
        EXPECT_EQ(g_context, previous_allocator);
    }
}

TEST_F(VMRunnerBoundaryTest, PrimaryEntryClassifiesFailuresWithoutChangingTheConvenienceStatus)
{
    auto valid = write("status_valid.fa", "تاكد صحيح\n");
    auto invalid = write("status_invalid.fa", "تاكد خطا\n");
    auto missing = platform::utf8(directory / "status_missing.fa");
    StreamCapture err(std::cerr);

    auto success = run_protected(valid.c_str(), RunOptions { });
    EXPECT_EQ(success.status, ExitStatus::SUCCESS);
    EXPECT_EQ(success.failure, RunFailure::NONE);
    auto program_error = run_protected(invalid.c_str(), RunOptions { });
    EXPECT_EQ(program_error.status, ExitStatus::ERROR);
    EXPECT_EQ(program_error.failure, RunFailure::PROGRAM);
    auto input_error = run_protected(missing.c_str(), RunOptions { });
    EXPECT_EQ(input_error.status, ExitStatus::ERROR);
    EXPECT_EQ(input_error.failure, RunFailure::INPUT_NOT_FOUND);
    EXPECT_EQ(run_protected(missing.c_str()), ExitStatus::ERROR);
    EXPECT_EQ(g_context, previous_allocator);
}

TEST_F(VMRunnerBoundaryTest, CheckOnlyValidatesUnusedBodiesAndNeverExecutes)
{
    auto valid = write("checked_valid.fa", "اكتب(\"must not execute\")\n");
    auto invalid = write("checked_unused.fa",
        "دالة غير_مستعملة():\n"
        "    اخرج\n"
        "اكتب(\"must not execute\")\n");
    RunOptions options { .check_only = true };
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(valid.c_str(), options).status, ExitStatus::SUCCESS);
    EXPECT_EQ(run_protected(invalid.c_str(), options).status, ExitStatus::ERROR);
    EXPECT_TRUE(out.str().empty());
    EXPECT_EQ(occurrences(err.str(), "'break' used outside of a loop"), 1u);
}

TEST_F(VMRunnerBoundaryTest, AstDumpEagerlyRejectsInvalidUnusedBodies)
{
    auto path = write("ast_unused.fa",
        "دالة غير_مستعملة():\n"
        "    ارجع +\n"
        "اكتب(\"must not execute\")\n");
    RunOptions options { .dump_ast = true };
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(path.c_str(), options).status, ExitStatus::ERROR);
    EXPECT_TRUE(out.str().empty());
    EXPECT_EQ(occurrences(err.str(), "Unexpected token"), 1u);
}

TEST_F(VMRunnerBoundaryTest, JsonOptionsProduceOneDocumentForCompileAndRuntimeErrors)
{
    for (auto const& test : {
             std::pair { "اخرج\n", "BREAK_OUTSIDE_LOOP" },
             std::pair { "تاكد خطا\n", "ASSERT_FAILED" } }) {
        SCOPED_TRACE(test.second);
        auto path = write("json_error.fa", test.first);
        StreamCapture err(std::cerr);
        RunOptions options { .json_diagnostics = true };
        EXPECT_EQ(run_protected(path.c_str(), options).status, ExitStatus::ERROR);
        auto output = err.str();
        ASSERT_GE(output.size(), 2u);
        EXPECT_EQ(output.front(), '[');
        EXPECT_EQ(output.substr(output.size() - 2), "]\n");
        EXPECT_EQ(occurrences(output, "\"errorCode\":"), 1u);
        EXPECT_NE(output.find(test.second), std::string::npos);
        EXPECT_EQ(output.find("Traceback ("), std::string::npos);
        EXPECT_EQ(g_context, previous_allocator);
    }
}

TEST_F(VMRunnerBoundaryTest, JsonSuccessRetainsProgramOutputAndRestoresCallerMode)
{
    auto path = write("json_success.fa", "اكتب(42)\n");
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);
    RunOptions options { .json_diagnostics = true };
    EXPECT_EQ(run_protected(path.c_str(), options).status, ExitStatus::SUCCESS);
    EXPECT_EQ(out.str(), "42\n");
    EXPECT_EQ(err.str(), "[]\n");

    diagnostic::report(diagnostic::Severity::ERROR, { }, ErrorCode::INTERNAL_ERROR, "caller text mode");
    diagnostic::dump();
    EXPECT_NE(err.str().find("caller text mode"), std::string::npos);
}

TEST_F(VMRunnerBoundaryTest, TimingOptionKeepsMeasurementsOutOfProgramOutput)
{
    auto path = write("timed.fa", "اكتب(42)\n");
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);
    RunOptions options { .print_time = true };
    EXPECT_EQ(run_protected(path.c_str(), options).status, ExitStatus::SUCCESS);
    EXPECT_EQ(out.str(), "42\n");
    EXPECT_EQ(occurrences(err.str(), "time: "), 1u);
}

TEST_F(VMRunnerBoundaryTest, RejectsNullAndEmptyPathsWithoutThrowing)
{
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(nullptr), ExitStatus::ERROR);
    EXPECT_EQ(run_protected(""), ExitStatus::ERROR);
    EXPECT_EQ(occurrences(err.str(), "input path is empty"), 2u);
    EXPECT_EQ(g_context, previous_allocator);
}

TEST_F(VMRunnerBoundaryTest, ReportsMissingFilesAndDirectoriesWithoutInventingSourceLocations)
{
    auto missing = platform::utf8(directory / "missing.fa");
    auto folder = platform::utf8(directory);
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(missing.c_str()), ExitStatus::ERROR);
    EXPECT_EQ(run_protected(folder.c_str()), ExitStatus::ERROR);
    EXPECT_NE(err.str().find("File not found"), std::string::npos);
    EXPECT_NE(err.str().find("File is not open"), std::string::npos);
    EXPECT_EQ(err.str().find("--> line"), std::string::npos);
}

TEST_F(VMRunnerBoundaryTest, ParseAndCompileFailuresPreventExecutionAndReportOnce)
{
    struct Case {
        char const* source;
        char const* diagnostic;
    };
    for (auto const& test : {
             Case { "قيمة :=\nاكتب(\"must not execute\")\n", "Unexpected token" },
             Case { "اخرج\nاكتب(\"must not execute\")\n", "'break' used outside of a loop" } }) {
        SCOPED_TRACE(test.source);
        auto path = write("invalid.fa", test.source);
        StreamCapture out(std::cout);
        StreamCapture err(std::cerr);
        EXPECT_EQ(run_protected(path.c_str()), ExitStatus::ERROR);
        EXPECT_TRUE(out.str().empty());
        EXPECT_EQ(occurrences(err.str(), test.diagnostic), 1u);
        EXPECT_EQ(g_context, previous_allocator);
    }
}

TEST_F(VMRunnerBoundaryTest, LexicalAbortIsContainedAndLaterRunsRemainUsable)
{
    auto invalid = write("bad_string.fa", "نص := \"unterminated\n");
    auto valid = write("valid.fa", "تاكد صحيح\n");
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(invalid.c_str()), ExitStatus::ERROR);
    EXPECT_EQ(occurrences(err.str(), "Unterminated string"), 1u);
    EXPECT_EQ(run_protected(valid.c_str()), ExitStatus::SUCCESS);
    EXPECT_EQ(g_context, previous_allocator);
}

TEST_F(VMRunnerBoundaryTest, RuntimeFailureStopsFollowingStatementsAndKeepsItsTraceback)
{
    auto path = write("runtime_error.fa",
        "دالة افشل():\n"
        "    تاكد خطا\n"
        "اكتب(\"before\")\n"
        "افشل()\n"
        "اكتب(\"after\")\n");
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(path.c_str()), ExitStatus::ERROR);
    EXPECT_EQ(out.str(), "before\n");
    EXPECT_EQ(occurrences(err.str(), "assertion failed"), 1u);
    EXPECT_NE(err.str().find("Traceback"), std::string::npos);
    EXPECT_NE(err.str().find("runtime_error.fa"), std::string::npos);
}

TEST_F(VMRunnerBoundaryTest, FunctionBodiesStayLazyAndInvokedBodyErrorsAreNotDuplicated)
{
    auto unused = write("unused.fa", "دالة مثال():\n    ارجع +\nتاكد صحيح\n");
    auto invoked = write("invoked.fa", "دالة مثال():\n    ارجع +\nمثال()\n");
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(unused.c_str()), ExitStatus::SUCCESS);
    EXPECT_TRUE(err.str().empty());
    EXPECT_EQ(run_protected(invoked.c_str()), ExitStatus::ERROR);
    EXPECT_EQ(occurrences(err.str(), "Unexpected token"), 1u);
}

TEST_F(VMRunnerBoundaryTest, ResolvesRelativeImportsFromTheInputFile)
{
    write("runner_dependency.ف", "دالة احسب():\n    ارجع 42\n");
    auto path = write("imports.fa",
        "من runner_dependency استورد احسب\n"
        "تاكد احسب() = 42\n");
    auto caller_directory = std::filesystem::current_path();
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(path.c_str()), ExitStatus::SUCCESS);
    EXPECT_EQ(std::filesystem::current_path(), caller_directory);
    EXPECT_TRUE(err.str().empty());
}

TEST_F(VMRunnerBoundaryTest, SeparateRunsDoNotShareGlobals)
{
    auto first = write("first.fa", "خاص_بالاول := 42\n");
    auto second = write("second.fa", "اكتب(خاص_بالاول)\n");
    auto third = write("third.fa", "تاكد صحيح\n");
    StreamCapture out(std::cout);
    StreamCapture err(std::cerr);
    EXPECT_EQ(run_protected(first.c_str()), ExitStatus::SUCCESS);
    EXPECT_EQ(run_protected(second.c_str()), ExitStatus::ERROR);
    EXPECT_EQ(run_protected(third.c_str()), ExitStatus::SUCCESS);
    EXPECT_TRUE(out.str().empty());
    EXPECT_EQ(g_context, previous_allocator);
}

TEST_F(VMRunnerBoundaryTest, RestoresCallerDiagnosticsSourceOutputModeAndAllocatorOnEveryPath)
{
    auto valid = write("valid.fa", "تاكد صحيح\n");
    auto invalid = write("invalid.fa", "تاكد خطا\n");
    auto source = std::make_shared<SourceManager>("caller.fa", "caller source\n");
    diagnostic::engine.set_source(source);
    auto id = diagnostic::report(diagnostic::Severity::ERROR, { 1, 1, 0, 0 },
        ErrorCode::INTERNAL_ERROR, "caller diagnostic");
    diagnostic::engine.add_suggestion(id, "preserve this suggestion");
    diagnostic::engine.set_json_output(true);
    auto caller_diagnostics = diagnostic::engine.to_json();
    StringRef caller_value("caller arena remains valid");
    StreamCapture err(std::cerr);

    for (auto const& path : { valid, invalid, platform::utf8(directory / "missing.fa") }) {
        SCOPED_TRACE(path);
        auto status = run_protected(path.c_str());
        EXPECT_EQ(status, path == valid ? ExitStatus::SUCCESS : ExitStatus::ERROR);
        EXPECT_EQ(g_context, previous_allocator);
        EXPECT_EQ(diagnostic::engine.source(), source);
        EXPECT_EQ(diagnostic::engine.to_json(), caller_diagnostics);
        EXPECT_EQ(caller_value, "caller arena remains valid");
    }

    // The runner must render its error even when the saved caller uses JSON,
    // but must not leak or render the caller's previously accumulated errors.
    EXPECT_NE(err.str().find("assertion failed"), std::string::npos);
    EXPECT_EQ(err.str().find("caller diagnostic"), std::string::npos);
    auto before_dump = err.str();
    diagnostic::dump();
    EXPECT_EQ(err.str(), before_dump); // Caller JSON mode was restored.
}

TEST_F(VMRunnerBoundaryTest, RestoresAnInitiallyNullAllocator)
{
    auto path = write("no_context.fa", "تاكد صحيح\n");
    set_context(nullptr);
    EXPECT_EQ(run_protected(path.c_str()), ExitStatus::SUCCESS);
    EXPECT_EQ(g_context, nullptr);
    EXPECT_EQ(run_protected(nullptr), ExitStatus::ERROR);
    EXPECT_EQ(g_context, nullptr);
}

TEST_F(VMRunnerBoundaryTest, PreservesTheCallersDiagnosticPrintingPosition)
{
    auto path = write("printing_position.fa", "تاكد صحيح\n");
    diagnostic::report(diagnostic::Severity::ERROR, { }, ErrorCode::INTERNAL_ERROR, "already printed");
    StreamCapture err(std::cerr);
    diagnostic::dump();
    auto printed = err.str();
    ASSERT_EQ(occurrences(printed, "already printed"), 1u);

    EXPECT_EQ(run_protected(path.c_str()), ExitStatus::SUCCESS);
    diagnostic::dump();
    EXPECT_EQ(err.str(), printed);
}

class ThrowingBuffer final : public std::streambuf {
    int_type overflow(int_type) override { throw std::runtime_error("diagnostic stream failed"); }
    std::streamsize xsputn(char const*, std::streamsize) override
    {
        throw std::runtime_error("diagnostic stream failed");
    }
};

class FailingDiagnostics {
public:
    FailingDiagnostics()
        : m_previous_buffer(std::cerr.rdbuf())
        , m_previous_state(std::cerr.rdstate())
        , m_previous_exceptions(std::cerr.exceptions())
    {
        std::cerr.exceptions(std::ios::goodbit);
        std::cerr.rdbuf(&m_buffer);
        std::cerr.clear();
        std::cerr.exceptions(std::ios::badbit | std::ios::failbit);
    }

    ~FailingDiagnostics()
    {
        std::cerr.exceptions(std::ios::goodbit);
        std::cerr.rdbuf(m_previous_buffer);
        std::cerr.clear(m_previous_state);
        std::cerr.exceptions(m_previous_exceptions);
    }

    FailingDiagnostics(FailingDiagnostics const&) = delete;
    FailingDiagnostics& operator=(FailingDiagnostics const&) = delete;

private:
    ThrowingBuffer m_buffer;
    std::streambuf* m_previous_buffer;
    std::ios::iostate m_previous_state;
    std::ios::iostate m_previous_exceptions;
};

TEST_F(VMRunnerBoundaryTest, ReportingExceptionsCannotEscapeOrLeaveDanglingHostState)
{
    auto invalid = write("unreportable.fa", "قيمة :=\n");
    {
        FailingDiagnostics failure;
        EXPECT_EQ(run_protected(invalid.c_str()), ExitStatus::ERROR);
    }
    EXPECT_EQ(g_context, previous_allocator);
    EXPECT_FALSE(diagnostic::has_errors());
    auto valid = write("after_reporting_failure.fa", "تاكد صحيح\n");
    EXPECT_EQ(run_protected(valid.c_str()), ExitStatus::SUCCESS);
}

} // namespace
