#include <filesystem>
#include <gtest/gtest.h>

#include "../../include/lex/lexer.hpp"

using namespace mylang;


std::filesystem::path lexer_test_cases_dir()
{
  static const auto dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
  return dir;
}

TEST(LexerTest, TestIndentationLevel0)
{
  input::FileManager file_manager(lexer_test_cases_dir() / "recognizes_indentation_level0.txt");
  lex::Lexer         lexer(&file_manager);
  std::vector<const tok::Token*> tokens   = lexer.tokenize();
  const tok::Token*              expected = lexer.make_token(tok::TokenType::IDENTIFIER, u"ا", 1, 1);
  EXPECT_EQ(tokens.size(), 3);
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  EXPECT_EQ(*tokens[1], *expected);
  EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, TestIndentationLevel1)
{
  input::FileManager             file_manager(lexer_test_cases_dir() / "recognizes_indentation_level1.txt");
  lex::Lexer                     lexer(&file_manager);
  std::vector<const tok::Token*> tokens   = lexer.tokenize();
  std::vector<const tok::Token*> expected = {lexer.make_token(tok::TokenType::IDENTIFIER, u"ا", 1, 1),
                                             lexer.make_token(tok::TokenType::NEWLINE, u"\n", 1, 2),
                                             lexer.make_token(tok::TokenType::INDENT),
                                             lexer.make_token(tok::TokenType::IDENTIFIER, u"ا", 2, 5),
                                             lexer.make_token(tok::TokenType::NEWLINE, u"\n", 2, 6),
                                             lexer.make_token(tok::TokenType::DEDENT),
                                             lexer.make_token(tok::TokenType::DEDENT),
                                             lexer.make_token(tok::TokenType::ENDMARKER, std::nullopt, 3, 1)};
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  for (int i = 1; i < tokens.size() - 1; ++i)
    EXPECT_EQ(*expected[i - 1], *tokens[i]);
  EXPECT_EQ(tokens.back()->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, TestIndentationLevel2)
{
  input::FileManager             file_manager(lexer_test_cases_dir() / "recognizes_indentation_level1.txt");
  lex::Lexer                     lexer(&file_manager);
  std::vector<const tok::Token*> tokens   = lexer.tokenize();
  std::vector<const tok::Token*> expected = {lexer.make_token(tok::TokenType::IDENTIFIER, u"ا", 1, 1),
                                             lexer.make_token(tok::TokenType::NEWLINE, u"\n", 1, 2),
                                             lexer.make_token(tok::TokenType::INDENT),
                                             lexer.make_token(tok::TokenType::IDENTIFIER, u"ا", 2, 5),
                                             lexer.make_token(tok::TokenType::NEWLINE, u"\n", 2, 6),
                                             lexer.make_token(tok::TokenType::INDENT),
                                             lexer.make_token(tok::TokenType::IDENTIFIER, u"ا", 3, 9),
                                             lexer.make_token(tok::TokenType::NEWLINE, u"\n", 3, 10),
                                             lexer.make_token(tok::TokenType::DEDENT),
                                             lexer.make_token(tok::TokenType::DEDENT),
                                             lexer.make_token(tok::TokenType::ENDMARKER, std::nullopt, 4, 1)};
  EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
  for (int i = 1; i < tokens.size() - 1; ++i)
    EXPECT_EQ(*expected[i - 1], *tokens[i]);
  EXPECT_EQ(tokens.back()->type(), tok::TokenType::ENDMARKER);
}