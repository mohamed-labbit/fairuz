#include "../../include/lex/lexer.h"
#include "../../include/lex/token.h"
#include <gtest/gtest.h>
#include <vector>


const std::string test_cases_path = "/Users/mohamedrabbit/mylang/tests/lexer/test_cases/test_tokens/";


inline void PrintTo(const Token& tok, std::ostream* os) {
    *os << "Token(\"" << to_utf8(tok.str()) << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line()
        << ", col=" << tok.column() << ")";
}

TEST(LexerTest, RecognizesPlus) {
    Lexer              lexer(test_cases_path + "recognizes_plus.txt");
    std::vector<Token> tokens = lexer.tokenize();
    Token              expected(L"+", TokenType::PLUS, {1, 1});

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesInteger) {
    Lexer              lexer(test_cases_path + "recognizes_integer.txt");
    std::vector<Token> tokens = lexer.tokenize();
    Token              expected(L"123", TokenType::NUMBER, {1, 1});

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesIdentifier) {
    Lexer              lexer(test_cases_path + "recognizes_identifier.txt");
    std::vector<Token> tokens = lexer.tokenize();
    Token              expected(L"مرحبا", TokenType::IDENTIFIER, {1, 1});

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesKeyword) {
    Lexer              lexer(test_cases_path + "recognizes_keyword.txt");
    std::vector<Token> tokens = lexer.tokenize();

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1].type(), TokenType::KW_IF);
    EXPECT_EQ(tokens[2].type(), TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesStringLiteral) {
    Lexer              lexer(test_cases_path + "recognizes_string_literal.txt");
    std::vector<Token> tokens = lexer.tokenize();
    Token              expected(L"العالم", TokenType::STRING, {1, 1});

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), TokenType::END_OF_FILE);
}

/*
TEST(LexerTest, HandlesUnexpectedCharacter) {
  LexTokenizer lexer("@");
  EXPECT_THROW(lexer.tokenize(), LexerError);
}
*/

TEST(LexerTest, TokenizesExpression) {
    Lexer              lexer(test_cases_path + "recognizes_expression.txt");
    std::vector<Token> tokens = lexer.tokenize();

    std::vector<Token> expected = {{L"", TokenType::START_OF_FILE, {1, 1}}, {L"س", TokenType::IDENTIFIER, {1, 1}},
                                   {L"=", TokenType::EQ, {1, 3}},           {L"42", TokenType::NUMBER, {1, 5}},
                                   {L"+", TokenType::PLUS, {1, 8}},         {L"ي", TokenType::IDENTIFIER, {1, 10}},
                                   {L"", TokenType::END_OF_FILE, {1, 10}}};

    EXPECT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[0].type(), TokenType::START_OF_FILE);
    EXPECT_EQ(tokens, expected);
}