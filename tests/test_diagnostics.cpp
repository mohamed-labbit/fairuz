#include "../fairuz/fdiagnostic.hpp"
#include "../fairuz/flexer.hpp"
#include "../fairuz/fparser.hpp"

#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

using namespace fairuz;

namespace {

class CaptureDiagnostics {
public:
    std::ostringstream output;
    std::streambuf* previous = std::cerr.rdbuf(output.rdbuf());
    ~CaptureDiagnostics() { std::cerr.rdbuf(previous); }
};

}

TEST(Diagnostics, OwnsSourcesAndPrintsEachErrorOnce)
{
    diagnostic::Fa_DiagnosticEngine engine;
    {
        lex::Fa_FileManager file;
        file.buffer() = "original\n";
        engine.set_source(&file);
        engine.report_deferred(diagnostic::Severity::ERROR, { 1, 1, 3 }, 0x0212);
        file.buffer() = "changed\n";
    }
    engine.set_source(std::make_shared<diagnostic::Source>("second.ف", "second\n"));
    engine.report_deferred(diagnostic::Severity::ERROR, { 1, 1, 1 }, 0x0212);
    CaptureDiagnostics capture;
    engine.pretty_print();
    auto first = capture.output.str();
    engine.pretty_print();
    EXPECT_EQ(first, capture.output.str());
    EXPECT_NE(first.find("original"), std::string::npos);
    EXPECT_NE(first.find("second.ف"), std::string::npos);
    EXPECT_EQ(first.find("changed"), std::string::npos);
}

TEST(Diagnostics, UnderlinesUnicodeAndTabsInDisplayColumns)
{
    diagnostic::Fa_DiagnosticEngine engine;
    engine.set_source(std::make_shared<diagnostic::Source>("unicode.ف", "س😀ب +\n\tس + 1\r\n"));
    engine.report_deferred(diagnostic::Severity::ERROR, { 1, 5, 1 }, 0x0212);
    engine.report_deferred(diagnostic::Severity::ERROR, { 2, 4, 1 }, 0x0212);
    CaptureDiagnostics capture;
    engine.pretty_print();
    EXPECT_NE(capture.output.str().find("    |      ^\n"), std::string::npos);
    EXPECT_NE(capture.output.str().find("    |       ^\n"), std::string::npos);
    EXPECT_EQ(capture.output.str().find('\r'), std::string::npos);
}

TEST(Diagnostics, JsonEscapesDetailsAndIncludesSuggestionsAndTraceback)
{
    diagnostic::Fa_DiagnosticEngine engine;
    auto source = std::make_shared<diagnostic::Source>("quote\"\\.ف", "اكتب(\"نص\")\n");
    engine.set_source(source);
    auto id = engine.report_deferred(diagnostic::Severity::ERROR, { 1, 1, 4 }, 0x0618, "quote\"\\\n\t");
    engine.add_suggestion(id, "try \"this\"");
    engine.add_frame(id, source, { 1, 1, 4 }, "دالة");
    auto json = engine.to_json();
    EXPECT_NE(json.find("quote\\\"\\\\\\u000a\\u0009"), std::string::npos);
    EXPECT_NE(json.find("\"type\":\"AssertionError\""), std::string::npos);
    EXPECT_NE(json.find("\"traceback\":[{\"path\":"), std::string::npos);
    EXPECT_NE(json.find("try \\\"this\\\""), std::string::npos);
}

TEST(Diagnostics, ErrorLimitSuppressesWithoutThrowingAndFatalCountsAsError)
{
    diagnostic::Fa_DiagnosticEngine engine;
    for (unsigned i = 0; i < 100; ++i)
        EXPECT_NO_THROW(engine.report_deferred(diagnostic::Severity::ERROR, { i + 1, 1, 1 }, 0x0212));
    EXPECT_EQ(engine.error_count(), 20u);
    EXPECT_EQ(engine.report_deferred(diagnostic::Severity::WARNING, { }, 0x0308), diagnostic::Fa_DiagnosticEngine::INVALID_ID);
    engine.reset();
    EXPECT_THROW(engine.report_deferred(diagnostic::Severity::FATAL, { }, 0x0224), diagnostic::Fa_DiagnosticAbort);
    EXPECT_TRUE(engine.has_errors());
}

TEST(ParserRecovery, ContinuesAfterIndependentErrorsInsideAndOutsideBlocks)
{
    diagnostic::reset();
    lex::Fa_FileManager file;
    file.buffer() = "س :=\nدالة مثال():\n    ص :=\n    ع :=\n    ارجع 1\nاكتب(42)\n";
    parser::Fa_Parser parser(&file);
    Fa_Array<AST::Fa_Stmt*> statements;
    EXPECT_NO_THROW(statements = parser.parse_program());
    EXPECT_EQ(diagnostic::error_count(), 3u);
    ASSERT_EQ(statements.size(), 2u);
    EXPECT_TRUE(AST::is_func(statements[0]));
    EXPECT_EQ(statements[1]->get_kind(), AST::Fa_Stmt::Kind::EXPR);
    diagnostic::reset();
}

TEST(ParserRecovery, MissingColonSkipsOrphanedSuiteWithoutCascading)
{
    diagnostic::reset();
    lex::Fa_FileManager file;
    file.buffer() = "اذا صحيح\n    اكتب(1)\nاكتب(2)\n";
    parser::Fa_Parser parser(&file);
    auto statements = parser.parse_program();
    EXPECT_EQ(diagnostic::error_count(), 1u);
    EXPECT_EQ(statements.size(), 1u);
    EXPECT_NE(diagnostic::engine.to_json().find("Add ':'"), std::string::npos);
    diagnostic::reset();
}

TEST(ParserRecovery, StopsAtErrorLimitAndCanParseAgain)
{
    diagnostic::reset();
    lex::Fa_FileManager file;
    for (int i = 0; i < 100; ++i)
        file.buffer() += "س :=\n";
    parser::Fa_Parser parser(&file);
    EXPECT_NO_THROW(parser.parse_program());
    EXPECT_EQ(diagnostic::error_count(), 20u);
    diagnostic::reset();
    lex::Fa_FileManager valid;
    valid.buffer() = "اكتب(42)\n";
    parser::Fa_Parser next(&valid);
    EXPECT_EQ(next.parse_program().size(), 1u);
    EXPECT_FALSE(diagnostic::has_errors());
    diagnostic::reset();
}
