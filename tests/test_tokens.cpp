#include "../include/lexer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace fairuz;

namespace {

std::filesystem::path const test_cases_path = std::filesystem::path(__FILE__).parent_path() / "test_cases" / "test_tokens";

Fa_StringRef load_source(std::filesystem::path const& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()).data();
}

} // namespace

inline void PrintTo(tok::Fa_Token const& tok, std::ostream* os)
{
    *os << "tok::Fa_Token(\"" << tok.lexeme() << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line() << ", col=" << tok.column() << ")";
}

TEST(LexerTest, RecognizesPlus)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_plus.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    tok::Fa_Token const* expected = MAKE_TOKEN(tok::Fa_TokenType::OP_PLUS, "+", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::Fa_TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesInteger)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_integer.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    tok::Fa_Token const* expected = MAKE_TOKEN(tok::Fa_TokenType::INTEGER, "123", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::Fa_TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesFloat)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_float.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    tok::Fa_Token const* expected = MAKE_TOKEN(tok::Fa_TokenType::DECIMAL, "123.456", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::Fa_TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesIdentifier)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_identifier.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    tok::Fa_Token const* expected = MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "مرحبا", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::Fa_TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesKeyword)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_keyword.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    EXPECT_EQ(tokens[1]->type(), tok::Fa_TokenType::KW_IF);
    EXPECT_EQ(tokens[2]->type(), tok::Fa_TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesNoneKeyword)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_none_keyword.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    auto none_it = std::find_if(tokens.begin(), tokens.end(), [](auto const& tok) { return tok->type() == tok::Fa_TokenType::KW_NIL; });
    ASSERT_NE(none_it, tokens.end());
    EXPECT_EQ((*none_it)->lexeme(), "عدم");
}

TEST(LexerTest, RecognizesBooleanKeywords)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_boolean_keywords.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    auto has_true = std::any_of(tokens.begin(), tokens.end(), [](auto const& tok) { return tok->type() == tok::Fa_TokenType::KW_TRUE && tok->lexeme() == "صحيح"; });
    auto has_false = std::any_of(tokens.begin(), tokens.end(), [](auto const& tok) { return tok->type() == tok::Fa_TokenType::KW_FALSE && tok->lexeme() == "خطا"; });
    EXPECT_TRUE(has_true);
    EXPECT_TRUE(has_false);
}

TEST(LexerTest, RecognizesStringLiteral)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_string_literal.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    tok::Fa_Token const* expected = MAKE_TOKEN(tok::Fa_TokenType::STRING, "العالم", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::Fa_TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesFa_Expression00)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_expression.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<tok::Fa_Token const*> expected = {
        MAKE_TOKEN(tok::Fa_TokenType::BEGINMARKER, "", 1, 1), MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "س", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::OP_EQ, "=", 1, 3), MAKE_TOKEN(tok::Fa_TokenType::INTEGER, "42", 1, 5),
        MAKE_TOKEN(tok::Fa_TokenType::OP_PLUS, "+", 1, 8), MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ي", 1, 10),
        MAKE_TOKEN(tok::Fa_TokenType::ENDMARKER, "", 1, 10)
    };
    EXPECT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    for (size_t i = 0; i < tokens.size(); i += 1)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt00)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_stmt_00.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<tok::Fa_Token const*> expected = {
        MAKE_TOKEN(tok::Fa_TokenType::BEGINMARKER, "", 1, 1), MAKE_TOKEN(tok::Fa_TokenType::KW_IF, "اذا", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "س", 1, 5), MAKE_TOKEN(tok::Fa_TokenType::OP_EQ, "=", 1, 7),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "د", 1, 9), MAKE_TOKEN(tok::Fa_TokenType::COLON, ":", 1, 10),
        MAKE_TOKEN(tok::Fa_TokenType::ENDMARKER, "", 1, 10)
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i += 1)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt01)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_stmt_01.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<tok::Fa_Token const*> expected = {
        MAKE_TOKEN(tok::Fa_TokenType::BEGINMARKER, "", 1, 1), MAKE_TOKEN(tok::Fa_TokenType::KW_WHILE, "طالما", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "س", 1, 7), MAKE_TOKEN(tok::Fa_TokenType::OP_NEQ, "!=", 1, 9),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "د", 1, 12), MAKE_TOKEN(tok::Fa_TokenType::COLON, ":", 1, 13),
        MAKE_TOKEN(tok::Fa_TokenType::ENDMARKER, "", 1, 13)
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i += 1)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt02)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_stmt_02.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<tok::Fa_Token const*> expected = {
        MAKE_TOKEN(tok::Fa_TokenType::BEGINMARKER, "", 1, 1), MAKE_TOKEN(tok::Fa_TokenType::KW_FOR, "بكل", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ل", 1, 5), MAKE_TOKEN(tok::Fa_TokenType::KW_IN, "في", 1, 7),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ك", 1, 10), MAKE_TOKEN(tok::Fa_TokenType::COLON, ":", 1, 11),
        MAKE_TOKEN(tok::Fa_TokenType::ENDMARKER, "", 1, 11)
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i += 1)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt03)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_stmt_03.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<tok::Fa_Token const*> expected = {
        MAKE_TOKEN(tok::Fa_TokenType::BEGINMARKER, "", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ا", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::OP_ASSIGN, ":=", 1, 3),
        MAKE_TOKEN(tok::Fa_TokenType::KW_FALSE, "خطا", 1, 6),
        MAKE_TOKEN(tok::Fa_TokenType::ENDMARKER, "", 1, 8),
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i += 1)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt04)
{
    lex::Fa_FileManager m_file_manager(test_cases_path / "recognizes_stmt_04.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<tok::Fa_Token const*> expected = {
        MAKE_TOKEN(tok::Fa_TokenType::BEGINMARKER, "", 1, 1), MAKE_TOKEN(tok::Fa_TokenType::KW_IF, "اذا", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ا", 1, 5), MAKE_TOKEN(tok::Fa_TokenType::OP_EQ, "=", 1, 7),
        MAKE_TOKEN(tok::Fa_TokenType::INTEGER, "3", 1, 9), MAKE_TOKEN(tok::Fa_TokenType::COLON, ":", 1, 10),
        MAKE_TOKEN(tok::Fa_TokenType::NEWLINE, "\n", 1, 11), MAKE_TOKEN(tok::Fa_TokenType::INDENT, "", 0, 0),
        MAKE_TOKEN(tok::Fa_TokenType::KW_RETURN, "ارجع", 2, 5), MAKE_TOKEN(tok::Fa_TokenType::DEDENT, "", 0, 0),
        MAKE_TOKEN(tok::Fa_TokenType::ENDMARKER, "", 2, 8)
    };

    EXPECT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i += 1)
        EXPECT_EQ(*tokens[i], *expected[i]);
}
