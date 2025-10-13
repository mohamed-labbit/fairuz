#include "../../include/lex/lexer.h"
#include "../../include/lex/token.h"

#include <gtest/gtest.h>
#include <vector>


const std::string test_cases_path = "/Users/mohamedrabbit/mylang/tests/lexer/test_cases/test_tokens/";


inline void PrintTo(const mylang::lex::tok::Token& tok, std::ostream* os)
{
    *os << "mylang::lex::tok::Token(\"" << utf8::utf16to8(tok.str()) << "\", type=" << static_cast<int>(tok.type())
        << ", line=" << tok.line() << ", col=" << tok.column() << ")";
}

TEST(LexerTest, RecognizesPlus)
{
    mylang::lex::Lexer                   lexer(test_cases_path + "recognizes_plus.txt");
    std::vector<mylang::lex::tok::Token> tokens   = lexer.tokenize();
    mylang::lex::tok::Token              expected = lexer.make_token(mylang::lex::tok::TokenType::PLUS, u"+", 1, 1);

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesInteger)
{
    mylang::lex::Lexer                   lexer(test_cases_path + "recognizes_integer.txt");
    std::vector<mylang::lex::tok::Token> tokens   = lexer.tokenize();
    mylang::lex::tok::Token              expected = lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"123", 1, 1);

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesIdentifier)
{
    mylang::lex::Lexer                   lexer(test_cases_path + "recognizes_identifier.txt");
    std::vector<mylang::lex::tok::Token> tokens = lexer.tokenize();
    mylang::lex::tok::Token expected = lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"مرحبا", 1, 1);

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesKeyword)
{
    mylang::lex::Lexer                   lexer(test_cases_path + "recognizes_keyword.txt");
    std::vector<mylang::lex::tok::Token> tokens = lexer.tokenize();

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1].type(), mylang::lex::tok::TokenType::KW_IF);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesStringLiteral)
{
    mylang::lex::Lexer                   lexer(test_cases_path + "recognizes_string_literal.txt");
    std::vector<mylang::lex::tok::Token> tokens = lexer.tokenize();
    mylang::lex::tok::Token expected = lexer.make_token(mylang::lex::tok::TokenType::STRING, u"العالم", 1, 1);

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::END_OF_FILE);
}

/*
TEST(LexerTest, HandlesUnexpectedCharacter) {
  LexTokenizer lexer("@");
  EXPECT_THROW(lexer.tokenize(), LexerError);
}
*/

TEST(LexerTest, TokenizesExpression)
{
    mylang::lex::Lexer                   lexer(test_cases_path + "recognizes_expression.txt");
    std::vector<mylang::lex::tok::Token> tokens = lexer.tokenize();

    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::START_OF_FILE, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::EQ, u"=", 1, 3),
      lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"42", 1, 5),
      lexer.make_token(mylang::lex::tok::TokenType::PLUS, u"+", 1, 8),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ي", 1, 10),
      lexer.make_token(mylang::lex::tok::TokenType::END_OF_FILE, u"", 1, 10)};


    EXPECT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens, expected);
}