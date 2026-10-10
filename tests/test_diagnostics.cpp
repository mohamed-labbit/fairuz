#include "../fairuz/fdiagnostic.hpp"

#include <gtest/gtest.h>
#include <iostream>
#include <set>
#include <sstream>

using namespace fairuz;

namespace {

struct CaptureErrors {
    std::ostringstream text;
    std::streambuf* previous = std::cerr.rdbuf(text.rdbuf());
    ~CaptureErrors() { std::cerr.rdbuf(previous); }
};

}

TEST(DiagnosticsTest, EveryRegisteredCodeHasUniqueIdentityAndCompleteMetadata)
{
    std::set<u16> numbers;
    std::set<std::string> names;
    for (auto code : all_error_codes) {
        EXPECT_TRUE(numbers.insert(static_cast<u16>(code)).second);
        EXPECT_TRUE(names.insert(error_name_for(code)).second);
        EXPECT_STRNE(error_name_for(code), "");
        EXPECT_STRNE(error_message_for(code), "");
        EXPECT_STRNE(diagnostic::error_type_for(code), "");
        if (code != ErrorCode::UNKNOWN) {
            EXPECT_STRNE(error_name_for(code), "UNKNOWN");
            EXPECT_STRNE(error_message_for(code), "Unknown error");
            EXPECT_NE(error_category_for(code), ErrorCategory::UNKNOWN);
        }
    }
    EXPECT_EQ(static_cast<u16>(ErrorCode::FILE_NOT_FOUND), 0x0001);
    EXPECT_EQ(static_cast<u16>(ErrorCode::INVALID_OCTAL_DIGIT), 0x0101);
    EXPECT_EQ(static_cast<u16>(ErrorCode::EXPECTED_INDENT), 0x0201);
    EXPECT_EQ(static_cast<u16>(ErrorCode::DIVISION_BY_ZERO), 0x0503);
    EXPECT_EQ(static_cast<u16>(ErrorCode::ALLOC_FAILED), 0x0800);
    auto unknown = static_cast<ErrorCode>(0xFFFF);
    EXPECT_STREQ(error_name_for(unknown), "UNKNOWN");
    EXPECT_STREQ(diagnostic::error_type_for(unknown), "unknown");
}

TEST(DiagnosticsTest, FatalSeverityCanBeRecordedAnnotatedAndRenderedWithoutAborting)
{
    diagnostic::DiagnosticEngine engine;
    CaptureErrors captured;
    auto id = engine.report_deferred(diagnostic::Severity::FATAL, { }, ErrorCode::INTERNAL_ERROR, "detail");
    engine.add_note(id, 0, "annotation after report");
    EXPECT_TRUE(captured.text.str().empty());
    EXPECT_EQ(engine.error_count(), 1);
    EXPECT_NO_THROW(engine.pretty_print());
    EXPECT_NE(captured.text.str().find("annotation after report"), std::string::npos);
    auto first = captured.text.str();
    engine.pretty_print();
    EXPECT_EQ(captured.text.str(), first);
    EXPECT_NO_THROW(engine.emit("display only", diagnostic::Severity::FATAL));
}

TEST(DiagnosticsTest, SaturationDoesNotDiscardFatalDiagnostics)
{
    diagnostic::DiagnosticEngine engine;
    for (u32 i = 0; i < engine.LIMIT; ++i)
        engine.report_deferred(diagnostic::Severity::ERROR, { }, ErrorCode::UNEXPECTED_TOKEN);
    EXPECT_EQ(engine.report_deferred(diagnostic::Severity::ERROR, { }, ErrorCode::UNEXPECTED_TOKEN), engine.INVALID_ID);
    EXPECT_NE(engine.report_deferred(diagnostic::Severity::FATAL, { }, ErrorCode::INTERNAL_ERROR), engine.INVALID_ID);
    EXPECT_EQ(engine.error_count(), engine.LIMIT + 1);
    engine.reset();
    EXPECT_FALSE(engine.has_errors());
    engine.report_deferred(diagnostic::Severity::WARNING, { }, ErrorCode::LOOP_VAR_SHADOW);
    EXPECT_EQ(engine.get_warning_count(), 1);
    EXPECT_FALSE(engine.has_errors());
}

TEST(DiagnosticsTest, ExplicitFatalHelperStillAbortsAndReportsOnce)
{
    diagnostic::reset();
    diagnostic::engine.set_json_output(false);
    CaptureErrors captured;
    EXPECT_THROW(diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "explicit abort"), diagnostic::DiagnosticAbort);
    EXPECT_EQ(diagnostic::error_count(), 1);
    auto output = captured.text.str();
    auto first = output.find("explicit abort");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(output.find("explicit abort", first + 1), std::string::npos);
    diagnostic::reset();
}
