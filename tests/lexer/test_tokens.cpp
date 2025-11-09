#include "../../include/lex/lexer.h"
#include "../../include/lex/token.h"

#include <gtest/gtest.h>
#include <vector>


const std::string test_cases_path =
  "/Users/mohamedrabbit/mylang/tests/lexer/test_cases/test_tokens/";


inline void PrintTo(const mylang::lex::tok::Token& tok, std::ostream* os)
{
    *os << "mylang::lex::tok::Token(\"" << utf8::utf16to8(tok.lexeme())
        << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line()
        << ", col=" << tok.column() << ")";
}

TEST(LexerTest, RecognizesPlus)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_plus.txt");
    auto tokens = lexer.tokenize();
    auto expected = lexer.make_token(mylang::lex::tok::TokenType::PLUS, u"+", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesInteger)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_integer.txt");
    auto tokens = lexer.tokenize();
    auto expected = lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"123", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesIdentifier)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_identifier.txt");
    auto tokens = lexer.tokenize();
    auto expected = lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"مرحبا", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesKeyword)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_keyword.txt");
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::START_OF_FILE);
    EXPECT_EQ(tokens[1].type(), mylang::lex::tok::TokenType::KW_IF);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::END_OF_FILE);
}

TEST(LexerTest, RecognizesStringLiteral)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_string_literal.txt");
    auto tokens = lexer.tokenize();
    auto expected = lexer.make_token(mylang::lex::tok::TokenType::STRING, u"العالم", 1, 1);
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

TEST(LexerTest, RecognizesExpression00)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_expression.txt");
    auto tokens = lexer.tokenize();
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

TEST(LexerTest, RecognizesStmt00)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_stmt_00.txt");
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::START_OF_FILE, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_IF, u"اذا", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 1, 5),
      lexer.make_token(mylang::lex::tok::TokenType::EQ, u"=", 1, 7),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"د", 1, 9),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 1, 10),
      lexer.make_token(mylang::lex::tok::TokenType::END_OF_FILE, u"", 1, 10)};
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}

TEST(LexerTest, RecognizesStmt01)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_stmt_01.txt");
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::START_OF_FILE, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_WHILE, u"طالما", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 1, 7),
      lexer.make_token(mylang::lex::tok::TokenType::NEQ, u"!=", 1, 9),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"د", 1, 12),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 1, 13),
      lexer.make_token(mylang::lex::tok::TokenType::END_OF_FILE, u"", 1, 13)};
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}

TEST(LexerTest, RecognizesStmt02)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_stmt_02.txt");
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::START_OF_FILE, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_FOR, u"بكل", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ل", 1, 5),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"في", 1, 7),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ك", 1, 10),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 1, 11),
      lexer.make_token(mylang::lex::tok::TokenType::END_OF_FILE, u"", 1, 11)};
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}

TEST(LexerTest, RecognizesStmt03)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_stmt_03.txt");
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::START_OF_FILE, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ا", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::ASSIGN, u":=", 1, 3),
      lexer.make_token(mylang::lex::tok::TokenType::KW_FALSE, u"خطا", 1, 6),
      lexer.make_token(mylang::lex::tok::TokenType::END_OF_FILE, u"", 1, 8),
    };
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}

TEST(LexerTest, RecognizesStmt04)
{
    mylang::lex::Lexer lexer(test_cases_path + "recognizes_stmt_04.txt");
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::START_OF_FILE, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_IF, u"اذا", 1, 5),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ا", 1, 3),
      lexer.make_token(mylang::lex::tok::TokenType::EQ, u"=", 1, 7),
      lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"3", 1, 9),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 1, 10),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 1, 10),
      lexer.make_token(mylang::lex::tok::TokenType::INDENT, u"    ", 2, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_RETURN, u"اخرج", 2, 5),
      lexer.make_token(mylang::lex::tok::TokenType::DEDENT, u"", 2, 9),
      lexer.make_token(mylang::lex::tok::TokenType::END_OF_FILE, u"", 2, 5)};
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}