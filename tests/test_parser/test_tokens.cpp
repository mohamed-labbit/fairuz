#include "Parser/lexer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

using namespace fairuz;

namespace {

std::filesystem::path const test_cases_path = std::filesystem::path(__FILE__).parent_path() / "test_cases" / "test_tokens";

} // namespace

inline void PrintTo(tok::Token const& tok, std::ostream* os)
{
    *os << "tok::Token(\"" << tok.lexeme() << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line() << ", col=" << tok.column() << ")";
}

TEST(LexerTest, RecognizesPlus)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_plus.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    SourceLocation loc = { 1, 1, 0, 0 };
    TokenPtr expected = make_token(tok::TokenType::OP_PLUS, "+", loc);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, ShiftOperatorsPreserveFollowingToken)
{
    for (auto const& [spelling, type] : {
             std::pair { "<<", tok::TokenType::OP_LSHIFT },
             std::pair { ">>", tok::TokenType::OP_RSHIFT },
             std::pair { "<<=", tok::TokenType::OP_LSHIFTEQ },
             std::pair { ">>=", tok::TokenType::OP_RSHIFTEQ } }) {
        for (char const* suffix : { "1", " 1", "س", " س", "" }) {
            std::string source = std::string(spelling) + suffix;
            SCOPED_TRACE(source);
            lex::FileManager file;
            file.buffer() = source.c_str();
            lex::Lexer lexer(&file);
            auto tokens = lexer.tokenize();
            ASSERT_GE(tokens.size(), 3);
            EXPECT_EQ(tokens[1]->type(), type);
            EXPECT_EQ(tokens[1]->lexeme(), spelling);
            if (*suffix != '\0') {
                char const* operand = *suffix == ' ' ? suffix + 1 : suffix;
                EXPECT_EQ(tokens[2]->lexeme(), operand);
            } else {
                EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
            }
        }
    }
}

TEST(LexerTest, RecognizesInteger)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_integer.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    SourceLocation loc = { 1, 1, 0, 0 };
    TokenPtr expected = make_token(tok::TokenType::INTEGER, "123", loc);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesFloat)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_float.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    SourceLocation loc = { 1, 1, 0, 0 };
    TokenPtr expected = make_token(tok::TokenType::DECIMAL, "123.456", loc);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesIdentifier)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_identifier.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    SourceLocation loc = { 1, 1, 0, 0 };
    TokenPtr expected = make_token(tok::TokenType::IDENTIFIER, "مرحبا", loc);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesKeyword)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_keyword.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(tokens[1]->type(), tok::TokenType::KW_IF);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesNoneKeyword)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_none_keyword.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    auto none_it = std::find_if(tokens.begin(), tokens.end(), [](auto const& tok) { return tok->type() == tok::TokenType::KW_NIL; });
    ASSERT_NE(none_it, tokens.end());
    EXPECT_EQ((*none_it)->lexeme(), "عدم");
}

TEST(LexerTest, RecognizesBooleanKeywords)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_boolean_keywords.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    auto has_true = std::any_of(tokens.begin(), tokens.end(), [](auto const& tok) { return tok->type() == tok::TokenType::KW_TRUE && tok->lexeme() == "صحيح"; });
    auto has_false = std::any_of(tokens.begin(), tokens.end(), [](auto const& tok) { return tok->type() == tok::TokenType::KW_FALSE && tok->lexeme() == "خطا"; });
    EXPECT_TRUE(has_true);
    EXPECT_TRUE(has_false);
}

TEST(LexerTest, RecognizesStringLiteral)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_string_literal.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    SourceLocation loc = { 1, 1, 0, 0 };
    TokenPtr expected = make_token(tok::TokenType::STRING, "العالم", loc);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, DecodesStringEscapesAndKeepsEscapedQuoteInsideLiteral)
{
    lex::FileManager source;
    source.buffer() = R"FA("quote: \" slash: \\ newline:\n tab:\t unicode:\u0645")FA";
    lex::Lexer lexer(&source);
    auto tokens = lexer.tokenize();

    ASSERT_EQ(tokens.size(), 3u);
    ASSERT_EQ(tokens[1]->type(), tok::TokenType::STRING);
    EXPECT_EQ(tokens[1]->lexeme(), "quote: \" slash: \\ newline:\n tab:\t unicode:م");
}

TEST(LexerTest, RecognizesExpression00)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_expression.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<TokenPtr> expected = {
        make_token(tok::TokenType::BEGINMARKER, "", { 1, 1, 0, 0 }), make_token(tok::TokenType::IDENTIFIER, "س", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::OP_EQ, "=", { 1, 3, 0, 0 }), make_token(tok::TokenType::INTEGER, "42", { 1, 5, 0, 0 }),
        make_token(tok::TokenType::OP_PLUS, "+", { 1, 8, 0, 0 }), make_token(tok::TokenType::IDENTIFIER, "ي", { 1, 10, 0, 0 }),
        make_token(tok::TokenType::ENDMARKER, "", { 1, 10, 0, 0 })
    };
    EXPECT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt00)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_stmt_00.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<TokenPtr> expected = {
        make_token(tok::TokenType::BEGINMARKER, "", { 1, 1, 0, 0 }), make_token(tok::TokenType::KW_IF, "اذا", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "س", { 1, 5, 0, 0 }), make_token(tok::TokenType::OP_EQ, "=", { 1, 7, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "د", { 1, 9, 0, 0 }), make_token(tok::TokenType::COLON, ":", { 1, 10, 0, 0 }),
        make_token(tok::TokenType::ENDMARKER, "", { 1, 10, 0, 0 })
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt01)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_stmt_01.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<TokenPtr> expected = {
        make_token(tok::TokenType::BEGINMARKER, "", { 1, 1, 0, 0 }), make_token(tok::TokenType::KW_WHILE, "طالما", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "س", { 1, 7, 0, 0 }), make_token(tok::TokenType::OP_NEQ, "!=", { 1, 9, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "د", { 1, 12, 0, 0 }), make_token(tok::TokenType::COLON, ":", { 1, 13, 0, 0 }),
        make_token(tok::TokenType::ENDMARKER, "", { 1, 13, 0, 0 })
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt02)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_stmt_02.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<TokenPtr> expected = {
        make_token(tok::TokenType::BEGINMARKER, "", { 1, 1, 0, 0 }), make_token(tok::TokenType::KW_FOR, "لكل", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "ل", { 1, 5, 0, 0 }), make_token(tok::TokenType::KW_IN, "في", { 1, 7, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "ك", { 1, 10, 0, 0 }), make_token(tok::TokenType::COLON, ":", { 1, 11, 0, 0 }),
        make_token(tok::TokenType::ENDMARKER, "", { 1, 11, 0, 0 })
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt03)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_stmt_03.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<TokenPtr> expected = {
        make_token(tok::TokenType::BEGINMARKER, "", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "ا", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::OP_ASSIGN, ":=", { 1, 3, 0, 0 }),
        make_token(tok::TokenType::KW_FALSE, "خطا", { 1, 6, 0, 0 }),
        make_token(tok::TokenType::ENDMARKER, "", { 1, 8, 0, 0 }),
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt04)
{
    lex::FileManager m_file_manager(test_cases_path / "recognizes_stmt_04.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<TokenPtr> expected = {
        make_token(tok::TokenType::BEGINMARKER, "", { 1, 1, 0, 0 }), make_token(tok::TokenType::KW_IF, "اذا", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "ا", { 1, 5, 0, 0 }), make_token(tok::TokenType::OP_EQ, "=", { 1, 7, 0, 0 }),
        make_token(tok::TokenType::INTEGER, "3", { 1, 9, 0, 0 }), make_token(tok::TokenType::COLON, ":", { 1, 10, 0, 0 }),
        make_token(tok::TokenType::NEWLINE, "\n", { 1, 11, 0, 0 }), make_token(tok::TokenType::INDENT, "", { 0, 0, 0, 0 }),
        make_token(tok::TokenType::KW_RETURN, "ارجع", { 2, 5, 0, 0 }), make_token(tok::TokenType::DEDENT, "", { 0, 0, 0, 0 }),
        make_token(tok::TokenType::ENDMARKER, "", { 2, 8, 0, 0 })
    };

    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, UnicodeColumnsAndByteOffsetsAreIndependent)
{
    diagnostic::reset();
    lex::FileManager file;
    file.buffer() = "س + \"€😀\"\r\nص";
    lex::Lexer lexer(&file);
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[1]->lexeme(), "س");
    EXPECT_EQ(tokens[2]->location().column, 3);
    EXPECT_EQ(tokens[2]->location().offset, 3);
    EXPECT_EQ(tokens[3]->lexeme(), "€😀");
    EXPECT_EQ(tokens[3]->location().column, 5);
    EXPECT_EQ(tokens[3]->location().length, 4);
    EXPECT_EQ(tokens[3]->location().offset, 5);
    EXPECT_EQ(tokens[5]->lexeme(), "ص");
    EXPECT_EQ(tokens[5]->location().line, 2);
    EXPECT_EQ(tokens[5]->location().column, 1);
    EXPECT_EQ(tokens[5]->location().offset, 16);
    diagnostic::reset();
}

TEST(LexerTest, MalformedUtf8IsALocatedLexicalError)
{
    for (std::string const bad : {
             std::string("\x80"), std::string("\xc0\xaf"), std::string("\xc2"),
             std::string("\xe2\x82"), std::string("\xf0\x9f\x98"),
             std::string("\xc2x"), std::string("\xe2\x82x"), std::string("\xf0\x9f\x98x"),
             std::string("\xe0\x80\x80"), std::string("\xf0\x80\x80\x80"),
             std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xff") }) {
        for (std::string const prefix : { std::string(), std::string("س\nص ") }) {
            diagnostic::reset();
            lex::FileManager file;
            file.buffer() = prefix + bad;
            std::ostringstream captured;
            auto* previous = std::cerr.rdbuf(captured.rdbuf());
            EXPECT_THROW({
                lex::Lexer lexer(&file);
                lexer.tokenize(); }, diagnostic::DiagnosticAbort);
            std::cerr.rdbuf(previous);
            auto output = captured.str();
            EXPECT_EQ(diagnostic::error_count(), 1);
            EXPECT_NE(output.find("Invalid UTF-8"), std::string::npos);
            EXPECT_EQ(output.find("Internal compiler error"), std::string::npos);
            auto json = diagnostic::engine.to_json();
            EXPECT_NE(json.find(prefix.empty() ? "\"line\":1" : "\"line\":2"), std::string::npos);
            EXPECT_NE(json.find(prefix.empty() ? "\"column\":1" : "\"column\":3"), std::string::npos);
        }
    }
    diagnostic::reset();
}

TEST(LexerTest, TokensAndDiagnosticSourceOutliveFileManager)
{
    diagnostic::reset();
    Array<TokenPtr> tokens;
    diagnostic::SourcePtr snapshot;
    {
        lex::FileManager file;
        file.buffer() = "س + 1\n";
        lex::Lexer lexer(&file);
        snapshot = lexer.source();
        file.buffer() = "different";
        tokens = lexer.tokenize();
    }
    diagnostic::reset();
    EXPECT_EQ(snapshot->line(1), "س + 1");
    EXPECT_EQ(tokens[1]->lexeme(), "س");
    lex::Lexer replay(tokens, snapshot);
    EXPECT_EQ(replay.source_slice(0, 2), "س");
    EXPECT_EQ(replay.get_line_at(1), "س + 1");
}
