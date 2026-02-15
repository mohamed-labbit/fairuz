#include "../../include/lex/file_manager.hpp"
#include "../../include/lex/lexer.hpp"
#include "../../include/lex/token.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <vector>

using namespace mylang;
using StringRef = StringRef;

namespace {

std::filesystem::path const test_cases_path = std::filesystem::path(__FILE__).parent_path() / "test_cases" / "test_tokens";

StringRef load_source(std::filesystem::path const& path)
{
    std::ifstream file(path, std::ios::binary);
    std::string utf8_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return StringRef(utf8_contents.data());
}

}

inline void PrintTo(tok::Token const& tok, std::ostream* os)
{
    *os << "tok::Token(\"" << tok.lexeme() << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line() << ", col=" << tok.column() << ")";
}

TEST(LexerTest, RecognizesPlus)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_plus.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    tok::Token const* expected = lexer.make_token(tok::TokenType::OP_PLUS, u"+", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesInteger)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_integer.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    tok::Token const* expected = lexer.make_token(tok::TokenType::NUMBER, u"123", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesFloat)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_float.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    tok::Token const* expected = lexer.make_token(tok::TokenType::FLOAT, u"123.456", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesIdentifier)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_identifier.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    tok::Token const* expected = lexer.make_token(tok::TokenType::IDENTIFIER, u"مرحبا", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesKeyword)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_keyword.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(tokens[1]->type(), tok::TokenType::KW_IF);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesNoneKeyword)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_none_keyword.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    auto none_it = std::find_if(tokens.begin(), tokens.end(), [](auto const& tok) { return tok->type() == tok::TokenType::KW_NONE; });
    ASSERT_NE(none_it, tokens.end());
    EXPECT_EQ((*none_it)->lexeme(), u"عدم");
}

TEST(LexerTest, RecognizesBooleanKeywords)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_boolean_keywords.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    auto has_true = std::any_of(tokens.begin(), tokens.end(), [](auto const& tok) { return tok->type() == tok::TokenType::KW_TRUE && tok->lexeme() == u"صحيح"; });
    auto has_false = std::any_of(tokens.begin(), tokens.end(), [](auto const& tok) { return tok->type() == tok::TokenType::KW_FALSE && tok->lexeme() == u"خطا"; });
    EXPECT_TRUE(has_true);
    EXPECT_TRUE(has_false);
}

TEST(LexerTest, RecognizesStringLiteral)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_string_literal.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    tok::Token const* expected = lexer.make_token(tok::TokenType::STRING, u"العالم", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

#if 0
  TEST(LexerTest, HandlesUnexpectedCharacter) {
    LexTokenizer lexer("@");
    EXPECT_THROW(lexer.tokenize(), LexerError);
  }
#endif

TEST(LexerTest, RecognizesExpression00)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_expression.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    std::vector<tok::Token const*> expected = {
        lexer.make_token(tok::TokenType::BEGINMARKER, u"", 1, 1), lexer.make_token(tok::TokenType::IDENTIFIER, u"س", 1, 1),
        lexer.make_token(tok::TokenType::OP_EQ, u"=", 1, 3), lexer.make_token(tok::TokenType::NUMBER, u"42", 1, 5),
        lexer.make_token(tok::TokenType::OP_PLUS, u"+", 1, 8), lexer.make_token(tok::TokenType::IDENTIFIER, u"ي", 1, 10),
        lexer.make_token(tok::TokenType::ENDMARKER, u"", 1, 10)
    };
    EXPECT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    for (SizeType i = 0; i < tokens.size(); ++i)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt00)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_stmt_00.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    std::vector<tok::Token const*> expected = { lexer.make_token(tok::TokenType::BEGINMARKER, u"", 1, 1), lexer.make_token(tok::TokenType::KW_IF, u"اذا", 1, 1),
        lexer.make_token(tok::TokenType::IDENTIFIER, u"س", 1, 5), lexer.make_token(tok::TokenType::OP_EQ, u"=", 1, 7),
        lexer.make_token(tok::TokenType::IDENTIFIER, u"د", 1, 9), lexer.make_token(tok::TokenType::COLON, u":", 1, 10),
        lexer.make_token(tok::TokenType::ENDMARKER, u"", 1, 10) };
    EXPECT_EQ(tokens.size(), expected.size());
    for (SizeType i = 0; i < tokens.size(); ++i)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt01)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_stmt_01.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    std::vector<tok::Token const*> expected = {
        lexer.make_token(tok::TokenType::BEGINMARKER, u"", 1, 1), lexer.make_token(tok::TokenType::KW_WHILE, u"طالما", 1, 1),
        lexer.make_token(tok::TokenType::IDENTIFIER, u"س", 1, 7), lexer.make_token(tok::TokenType::OP_NEQ, u"!=", 1, 9),
        lexer.make_token(tok::TokenType::IDENTIFIER, u"د", 1, 12), lexer.make_token(tok::TokenType::COLON, u":", 1, 13),
        lexer.make_token(tok::TokenType::ENDMARKER, u"", 1, 13)
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (SizeType i = 0; i < tokens.size(); ++i)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt02)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_stmt_02.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    std::vector<tok::Token const*> expected = {
        lexer.make_token(tok::TokenType::BEGINMARKER, u"", 1, 1), lexer.make_token(tok::TokenType::KW_FOR, u"بكل", 1, 1),
        lexer.make_token(tok::TokenType::IDENTIFIER, u"ل", 1, 5), lexer.make_token(tok::TokenType::IDENTIFIER, u"في", 1, 7),
        lexer.make_token(tok::TokenType::IDENTIFIER, u"ك", 1, 10), lexer.make_token(tok::TokenType::COLON, u":", 1, 11),
        lexer.make_token(tok::TokenType::ENDMARKER, u"", 1, 11)
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (SizeType i = 0; i < tokens.size(); ++i)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt03)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_stmt_03.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    std::vector<tok::Token const*> expected = {
        lexer.make_token(tok::TokenType::BEGINMARKER, u"", 1, 1),
        lexer.make_token(tok::TokenType::IDENTIFIER, u"ا", 1, 1),
        lexer.make_token(tok::TokenType::OP_ASSIGN, u":=", 1, 3),
        lexer.make_token(tok::TokenType::KW_FALSE, u"خطا", 1, 6),
        lexer.make_token(tok::TokenType::ENDMARKER, u"", 1, 8),
    };
    EXPECT_EQ(tokens.size(), expected.size());
    for (SizeType i = 0; i < tokens.size(); ++i)
        EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt04)
{
    lex::FileManager file_manager(test_cases_path / "recognizes_stmt_04.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    std::vector<tok::Token const*> expected = {
        lexer.make_token(tok::TokenType::BEGINMARKER, u"", 1, 1), 
        lexer.make_token(tok::TokenType::KW_IF, u"اذا", 1, 1),
        lexer.make_token(tok::TokenType::IDENTIFIER, u"ا", 1, 5), 
        lexer.make_token(tok::TokenType::OP_EQ, u"=", 1, 7),
        lexer.make_token(tok::TokenType::NUMBER, u"3", 1, 9), 
        lexer.make_token(tok::TokenType::COLON, u":", 1, 10),
        lexer.make_token(tok::TokenType::NEWLINE, u"\n", 1, 11), 
        lexer.make_token(tok::TokenType::INDENT, u""),
        lexer.make_token(tok::TokenType::KW_RETURN, u"اخرج", 2, 5), 
        lexer.make_token(tok::TokenType::DEDENT),
        lexer.make_token(tok::TokenType::ENDMARKER, u"", 2, 8)
    };

    EXPECT_EQ(tokens.size(), expected.size());
    for (SizeType i = 0; i < tokens.size(); ++i)
        EXPECT_EQ(*tokens[i], *expected[i]);
}
