#include "../../include/lex/lexer.h"
#include "../../include/lex/token.h"

#include <gtest/gtest.h>
#include <vector>


const std::string test_cases_path =
  "/Users/mohamedrabbit/mylang/tests/lexer/test_cases/test_tokens/";


inline void PrintTo(const mylang::lex::Token& tok, std::ostream* os)
{
    *os << "mylang::lex::Token(\"" << utf8::utf16to8(tok.str())
        << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line()
        << ", col=" << tok.column() << ")";
}

TEST(LexerTest, RecognizesPlus)
{
    mylang::lex::Lexer              lexer(test_cases_path + "recognizes_plus.txt");
    std::vector<mylang::lex::Token> tokens = lexer.tokenize();
    mylang::lex::Token              expected(u"+", mylang::lex::TokenType::PLUS, {1, 1});

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesInteger)
{
    mylang::lex::Lexer              lexer(test_cases_path + "recognizes_integer.txt");
    std::vector<mylang::lex::Token> tokens = lexer.tokenize();
    mylang::lex::Token              expected(u"123", mylang::lex::TokenType::NUMBER, {1, 1});

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesIdentifier)
{
    mylang::lex::Lexer              lexer(test_cases_path + "recognizes_identifier.txt");
    std::vector<mylang::lex::Token> tokens = lexer.tokenize();
    mylang::lex::Token              expected(u"مرحبا", mylang::lex::TokenType::IDENTIFIER, {1, 1});

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesKeyword)
{
    mylang::lex::Lexer              lexer(test_cases_path + "recognizes_keyword.txt");
    std::vector<mylang::lex::Token> tokens = lexer.tokenize();

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1].type(), mylang::lex::TokenType::KW_IF);
    EXPECT_EQ(tokens[2].type(), mylang::lex::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesStringLiteral)
{
    mylang::lex::Lexer              lexer(test_cases_path + "recognizes_string_literal.txt");
    std::vector<mylang::lex::Token> tokens = lexer.tokenize();
    mylang::lex::Token              expected(u"العالم", mylang::lex::TokenType::STRING, {1, 1});

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::TokenType::END_OF_FILE);
}

/*
TEST(LexerTest, HandlesUnexpectedCharacter) {
  LexTokenizer lexer("@");
  EXPECT_THROW(lexer.tokenize(), LexerError);
}
*/

TEST(LexerTest, TokenizesExpression)
{
    mylang::lex::Lexer              lexer(test_cases_path + "recognizes_expression.txt");
    std::vector<mylang::lex::Token> tokens = lexer.tokenize();

    std::vector<mylang::lex::Token> expected = {
      {u"", mylang::lex::TokenType::START_OF_FILE, {1, 1}},
      {u"س", mylang::lex::TokenType::IDENTIFIER, {1, 1}},
      {u"=", mylang::lex::TokenType::EQ, {1, 3}},
      {u"42", mylang::lex::TokenType::NUMBER, {1, 5}},
      {u"+", mylang::lex::TokenType::PLUS, {1, 8}},
      {u"ي", mylang::lex::TokenType::IDENTIFIER, {1, 10}},
      {u"", mylang::lex::TokenType::END_OF_FILE, {1, 10}}};

    EXPECT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[0].type(), mylang::lex::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens, expected);
}