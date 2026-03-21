#include "../include/lexer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace mylang;

namespace {

std::filesystem::path const test_cases_path = std::filesystem::path(__FILE__).parent_path() / "test_cases" / "test_tokens";

StringRef load_source(std::filesystem::path const &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()).data();
}

} // namespace

inline void PrintTo(tok::Token const &tok, std::ostream *os) {
  *os << "tok::Token(\"" << tok.lexeme() << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line() << ", col=" << tok.column() << ")";
}

TEST(LexerTest, RecognizesPlus) {
  lex::FileManager file_manager(test_cases_path / "recognizes_plus.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  tok::Token const *expected = lexer.makeToken(tok::TokenType::OP_PLUS, "+", 1, 1);
  EXPECT_EQ(tokens.size(), 3);
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  EXPECT_EQ(*tokens[1], *expected);
  EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesInteger) {
  lex::FileManager file_manager(test_cases_path / "recognizes_integer.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  tok::Token const *expected = lexer.makeToken(tok::TokenType::INTEGER, "123", 1, 1);
  EXPECT_EQ(tokens.size(), 3);
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  EXPECT_EQ(*tokens[1], *expected);
  EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesFloat) {
  lex::FileManager file_manager(test_cases_path / "recognizes_float.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  tok::Token const *expected = lexer.makeToken(tok::TokenType::DECIMAL, "123.456", 1, 1);
  EXPECT_EQ(tokens.size(), 3);
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  EXPECT_EQ(*tokens[1], *expected);
  EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesIdentifier) {
  lex::FileManager file_manager(test_cases_path / "recognizes_identifier.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  tok::Token const *expected = lexer.makeToken(tok::TokenType::IDENTIFIER, "مرحبا", 1, 1);
  EXPECT_EQ(tokens.size(), 3);
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  EXPECT_EQ(*tokens[1], *expected);
  EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesKeyword) {
  lex::FileManager file_manager(test_cases_path / "recognizes_keyword.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  EXPECT_EQ(tokens.size(), 3);
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  EXPECT_EQ(tokens[1]->type(), tok::TokenType::KW_IF);
  EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesNoneKeyword) {
  lex::FileManager file_manager(test_cases_path / "recognizes_none_keyword.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  auto none_it = std::find_if(tokens.begin(), tokens.end(), [](auto const &tok) { return tok->type() == tok::TokenType::KW_NONE; });
  ASSERT_NE(none_it, tokens.end());
  EXPECT_EQ((*none_it)->lexeme(), "عدم");
}

TEST(LexerTest, RecognizesBooleanKeywords) {
  lex::FileManager file_manager(test_cases_path / "recognizes_boolean_keywords.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  auto has_true =
      std::any_of(tokens.begin(), tokens.end(), [](auto const &tok) { return tok->type() == tok::TokenType::KW_TRUE && tok->lexeme() == "صحيح"; });
  auto has_false =
      std::any_of(tokens.begin(), tokens.end(), [](auto const &tok) { return tok->type() == tok::TokenType::KW_FALSE && tok->lexeme() == "خطا"; });
  EXPECT_TRUE(has_true);
  EXPECT_TRUE(has_false);
}

TEST(LexerTest, RecognizesStringLiteral) {
  lex::FileManager file_manager(test_cases_path / "recognizes_string_literal.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  tok::Token const *expected = lexer.makeToken(tok::TokenType::STRING, "العالم", 1, 1);
  EXPECT_EQ(tokens.size(), 3);
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  EXPECT_EQ(*tokens[1], *expected);
  EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesExpression00) {
  lex::FileManager file_manager(test_cases_path / "recognizes_expression.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  std::vector<tok::Token const *> expected = {
      lexer.makeToken(tok::TokenType::BEGINMARKER, "", 1, 1), lexer.makeToken(tok::TokenType::IDENTIFIER, "س", 1, 1),
      lexer.makeToken(tok::TokenType::OP_EQ, "=", 1, 3),      lexer.makeToken(tok::TokenType::INTEGER, "42", 1, 5),
      lexer.makeToken(tok::TokenType::OP_PLUS, "+", 1, 8),    lexer.makeToken(tok::TokenType::IDENTIFIER, "ي", 1, 10),
      lexer.makeToken(tok::TokenType::ENDMARKER, "", 1, 10)};
  EXPECT_EQ(tokens.size(), 7);
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  for (size_t i = 0; i < tokens.size(); ++i)
    EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt00) {
  lex::FileManager file_manager(test_cases_path / "recognizes_stmt_00.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  std::vector<tok::Token const *> expected = {
      lexer.makeToken(tok::TokenType::BEGINMARKER, "", 1, 1), lexer.makeToken(tok::TokenType::KW_IF, "اذا", 1, 1),
      lexer.makeToken(tok::TokenType::IDENTIFIER, "س", 1, 5), lexer.makeToken(tok::TokenType::OP_EQ, "=", 1, 7),
      lexer.makeToken(tok::TokenType::IDENTIFIER, "د", 1, 9), lexer.makeToken(tok::TokenType::COLON, ":", 1, 10),
      lexer.makeToken(tok::TokenType::ENDMARKER, "", 1, 10)};
  EXPECT_EQ(tokens.size(), expected.size());
  for (size_t i = 0; i < tokens.size(); ++i)
    EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt01) {
  lex::FileManager file_manager(test_cases_path / "recognizes_stmt_01.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  std::vector<tok::Token const *> expected = {
      lexer.makeToken(tok::TokenType::BEGINMARKER, "", 1, 1),  lexer.makeToken(tok::TokenType::KW_WHILE, "طالما", 1, 1),
      lexer.makeToken(tok::TokenType::IDENTIFIER, "س", 1, 7),  lexer.makeToken(tok::TokenType::OP_NEQ, "!=", 1, 9),
      lexer.makeToken(tok::TokenType::IDENTIFIER, "د", 1, 12), lexer.makeToken(tok::TokenType::COLON, ":", 1, 13),
      lexer.makeToken(tok::TokenType::ENDMARKER, "", 1, 13)};
  EXPECT_EQ(tokens.size(), expected.size());
  for (size_t i = 0; i < tokens.size(); ++i)
    EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt02) {
  lex::FileManager file_manager(test_cases_path / "recognizes_stmt_02.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  std::vector<tok::Token const *> expected = {
      lexer.makeToken(tok::TokenType::BEGINMARKER, "", 1, 1),  lexer.makeToken(tok::TokenType::KW_FOR, "بكل", 1, 1),
      lexer.makeToken(tok::TokenType::IDENTIFIER, "ل", 1, 5),  lexer.makeToken(tok::TokenType::IDENTIFIER, "في", 1, 7),
      lexer.makeToken(tok::TokenType::IDENTIFIER, "ك", 1, 10), lexer.makeToken(tok::TokenType::COLON, ":", 1, 11),
      lexer.makeToken(tok::TokenType::ENDMARKER, "", 1, 11)};
  EXPECT_EQ(tokens.size(), expected.size());
  for (size_t i = 0; i < tokens.size(); ++i)
    EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt03) {
  lex::FileManager file_manager(test_cases_path / "recognizes_stmt_03.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  std::vector<tok::Token const *> expected = {
      lexer.makeToken(tok::TokenType::BEGINMARKER, "", 1, 1), lexer.makeToken(tok::TokenType::IDENTIFIER, "ا", 1, 1),
      lexer.makeToken(tok::TokenType::OP_ASSIGN, ":=", 1, 3), lexer.makeToken(tok::TokenType::KW_FALSE, "خطا", 1, 6),
      lexer.makeToken(tok::TokenType::ENDMARKER, "", 1, 8),
  };
  EXPECT_EQ(tokens.size(), expected.size());
  for (size_t i = 0; i < tokens.size(); ++i)
    EXPECT_EQ(*tokens[i], *expected[i]);
}

TEST(LexerTest, RecognizesStmt04) {
  lex::FileManager file_manager(test_cases_path / "recognizes_stmt_04.txt");
  lex::Lexer lexer(&file_manager);
  auto tokens = lexer.tokenize();
  std::vector<tok::Token const *> expected = {
      lexer.makeToken(tok::TokenType::BEGINMARKER, "", 1, 1),   lexer.makeToken(tok::TokenType::KW_IF, "اذا", 1, 1),
      lexer.makeToken(tok::TokenType::IDENTIFIER, "ا", 1, 5),   lexer.makeToken(tok::TokenType::OP_EQ, "=", 1, 7),
      lexer.makeToken(tok::TokenType::INTEGER, "3", 1, 9),      lexer.makeToken(tok::TokenType::COLON, ":", 1, 10),
      lexer.makeToken(tok::TokenType::NEWLINE, "\n", 1, 11),    lexer.makeToken(tok::TokenType::INDENT, ""),
      lexer.makeToken(tok::TokenType::KW_RETURN, "ارجع", 2, 5), lexer.makeToken(tok::TokenType::DEDENT),
      lexer.makeToken(tok::TokenType::ENDMARKER, "", 2, 8)};

  EXPECT_EQ(tokens.size(), expected.size());
  for (size_t i = 0; i < tokens.size(); ++i)
    EXPECT_EQ(*tokens[i], *expected[i]);
}
