#include "../../include/input/file_manager_.hpp"
#include "../../include/lex/lexer.hpp"
#include "../../include/lex/token.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <vector>


using mylang::input::FileManager;

namespace {
const std::filesystem::path test_cases_path =
  std::filesystem::path(__FILE__).parent_path() / "test_cases" / "test_tokens";

std::u16string load_source(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::string utf8_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return utf8::utf8to16(utf8_contents);
}
}


inline void PrintTo(const mylang::lex::tok::Token& tok, std::ostream* os)
{
    *os << "mylang::lex::tok::Token(\"" << utf8::utf16to8(tok.lexeme()) << "\", type=" << static_cast<int>(tok.type())
        << ", line=" << tok.line() << ", col=" << tok.column() << ")";
}

TEST(LexerTest, RecognizesPlus)
{
    FileManager file_manager(test_cases_path / "recognizes_plus.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    auto expected = lexer.make_token(mylang::lex::tok::TokenType::OP_PLUS, u"+", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::BEGINMARKER);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesInteger)
{
    FileManager file_manager(test_cases_path / "recognizes_integer.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    auto expected = lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"123", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::BEGINMARKER);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesIdentifier)
{
    FileManager file_manager(test_cases_path / "recognizes_identifier.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    auto expected = lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"مرحبا", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::BEGINMARKER);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesKeyword)
{
    FileManager file_manager(test_cases_path / "recognizes_keyword.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::BEGINMARKER);
    EXPECT_EQ(tokens[1].type(), mylang::lex::tok::TokenType::KW_IF);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::ENDMARKER);
}

TEST(LexerTest, RecognizesNoneKeyword)
{
    FileManager file_manager(test_cases_path / "recognizes_none_keyword.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    auto none_it = std::find_if(tokens.begin(), tokens.end(), [](const auto& tok) {
        return tok.type() == mylang::lex::tok::TokenType::KW_NONE;
    });
    ASSERT_NE(none_it, tokens.end());
    EXPECT_EQ(none_it->lexeme(), u"عدم");
}

TEST(LexerTest, RecognizesBooleanKeywords)
{
    FileManager file_manager(test_cases_path / "recognizes_boolean_keywords.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    auto has_true = std::any_of(tokens.begin(), tokens.end(), [](const auto& tok) {
        return tok.type() == mylang::lex::tok::TokenType::KW_TRUE && tok.lexeme() == u"صحيح";
    });
    auto has_false = std::any_of(tokens.begin(), tokens.end(), [](const auto& tok) {
        return tok.type() == mylang::lex::tok::TokenType::KW_FALSE && tok.lexeme() == u"خطا";
    });
    EXPECT_TRUE(has_true);
    EXPECT_TRUE(has_false);
}

TEST(LexerTest, RecognizesStringLiteral)
{
    FileManager file_manager(test_cases_path / "recognizes_string_literal.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    auto expected = lexer.make_token(mylang::lex::tok::TokenType::STRING, u"العالم", 1, 1);
    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::BEGINMARKER);
    EXPECT_EQ(tokens[1], expected);
    EXPECT_EQ(tokens[2].type(), mylang::lex::tok::TokenType::ENDMARKER);
}

/*
TEST(LexerTest, HandlesUnexpectedCharacter) {
  LexTokenizer lexer("@");
  EXPECT_THROW(lexer.tokenize(), LexerError);
}
*/

TEST(LexerTest, RecognizesExpression00)
{
    mylang::input::FileManager file_manager(test_cases_path / "recognizes_expression.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::BEGINMARKER, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::OP_EQ, u"=", 1, 3),
      lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"42", 1, 5),
      lexer.make_token(mylang::lex::tok::TokenType::OP_PLUS, u"+", 1, 8),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ي", 1, 10),
      lexer.make_token(mylang::lex::tok::TokenType::ENDMARKER, u"", 1, 10)};
    EXPECT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[0].type(), mylang::lex::tok::TokenType::BEGINMARKER);
    EXPECT_EQ(tokens, expected);
}

TEST(LexerTest, RecognizesStmt00)
{
    mylang::input::FileManager file_manager(test_cases_path / "recognizes_stmt_00.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::BEGINMARKER, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_IF, u"اذا", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 1, 5),
      lexer.make_token(mylang::lex::tok::TokenType::OP_EQ, u"=", 1, 7),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"د", 1, 9),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 1, 10),
      lexer.make_token(mylang::lex::tok::TokenType::ENDMARKER, u"", 1, 10)};
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}

TEST(LexerTest, RecognizesStmt01)
{
    mylang::input::FileManager file_manager(test_cases_path / "recognizes_stmt_01.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::BEGINMARKER, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_WHILE, u"طالما", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 1, 7),
      lexer.make_token(mylang::lex::tok::TokenType::OP_NEQ, u"!=", 1, 9),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"د", 1, 12),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 1, 13),
      lexer.make_token(mylang::lex::tok::TokenType::ENDMARKER, u"", 1, 13)};
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}

TEST(LexerTest, RecognizesStmt02)
{
    mylang::input::FileManager file_manager(test_cases_path / "recognizes_stmt_02.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::BEGINMARKER, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_FOR, u"بكل", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ل", 1, 5),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"في", 1, 7),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ك", 1, 10),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 1, 11),
      lexer.make_token(mylang::lex::tok::TokenType::ENDMARKER, u"", 1, 11)};
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}

TEST(LexerTest, RecognizesStmt03)
{
    mylang::input::FileManager file_manager(test_cases_path / "recognizes_stmt_03.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::BEGINMARKER, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ا", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::OP_ASSIGN, u":=", 1, 3),
      lexer.make_token(mylang::lex::tok::TokenType::KW_FALSE, u"خطا", 1, 6),
      lexer.make_token(mylang::lex::tok::TokenType::ENDMARKER, u"", 1, 8),
    };
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}

TEST(LexerTest, RecognizesStmt04)
{
    mylang::input::FileManager file_manager(test_cases_path / "recognizes_stmt_04.txt");
    mylang::lex::Lexer lexer(&file_manager);
    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::BEGINMARKER, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_IF, u"اذا", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"ا", 1, 5),
      lexer.make_token(mylang::lex::tok::TokenType::OP_EQ, u"=", 1, 7),
      lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"3", 1, 9),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 1, 10),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 1, 11),
      lexer.make_token(mylang::lex::tok::TokenType::INDENT, u"    ", 2, 5),
      lexer.make_token(mylang::lex::tok::TokenType::KW_RETURN, u"اخرج", 2, 5),
      lexer.make_token(mylang::lex::tok::TokenType::DEDENT, u"", 2, 9),
      lexer.make_token(mylang::lex::tok::TokenType::ENDMARKER, u"", 2, 8)};
    EXPECT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens, expected);
}
