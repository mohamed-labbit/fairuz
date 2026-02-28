#include <filesystem>
#include <gtest/gtest.h>

#include "../../include/lex/lexer.hpp"

using namespace mylang;

std::filesystem::path lexer_test_cases_dir()
{
    static auto const dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
    return dir;
}

inline void PrintTo(tok::Token const& tok, std::ostream* os)
{
    *os << "tok::Token(\"" << tok.lexeme() << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line() << ", col=" << tok.column() << ")";
}

TEST(LexerTest, TestIndentationLevel0)
{
    lex::FileManager file_manager(lexer_test_cases_dir() / "recognizes_indentation_level0.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    tok::Token const* expected = lexer.makeToken(tok::TokenType::IDENTIFIER, "ا", 1, 1);
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, TestIndentationLevel1)
{
    lex::FileManager file_manager(lexer_test_cases_dir() / "recognizes_indentation_level1.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    std::vector<tok::Token const*> expected = {
        lexer.makeToken(tok::TokenType::BEGINMARKER, "", 1, 1),
        lexer.makeToken(tok::TokenType::IDENTIFIER, "ا", 1, 1),
        lexer.makeToken(tok::TokenType::NEWLINE, "\n", 1, 2),
        lexer.makeToken(tok::TokenType::INDENT),
        lexer.makeToken(tok::TokenType::IDENTIFIER, "ا", 2, 5),
        lexer.makeToken(tok::TokenType::NEWLINE, "\n", 2, 6),
        lexer.makeToken(tok::TokenType::DEDENT),
        lexer.makeToken(tok::TokenType::DEDENT),
        lexer.makeToken(tok::TokenType::ENDMARKER, "", 3, 1)
    };
    // ASSERT_EQ(tokens.size(), 9);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    for (int i = 1; i < tokens.size() - 1; ++i)
        EXPECT_EQ(*expected[i], *tokens[i]);
    EXPECT_EQ(tokens.back()->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, TestIndentationLevel2)
{
    lex::FileManager file_manager(lexer_test_cases_dir() / "recognizes_indentation_level2.txt");
    lex::Lexer lexer(&file_manager);
    std::vector<tok::Token const*> tokens = lexer.tokenize();
    std::vector<tok::Token const*> expected = {
        lexer.makeToken(tok::TokenType::BEGINMARKER, "", 1, 1),
        lexer.makeToken(tok::TokenType::IDENTIFIER, "ا", 1, 1),
        lexer.makeToken(tok::TokenType::NEWLINE, "\n", 1, 2),
        lexer.makeToken(tok::TokenType::INDENT),
        lexer.makeToken(tok::TokenType::IDENTIFIER, "ا", 2, 5),
        lexer.makeToken(tok::TokenType::NEWLINE, "\n", 2, 6),
        lexer.makeToken(tok::TokenType::INDENT),
        lexer.makeToken(tok::TokenType::IDENTIFIER, "ا", 3, 9),
        lexer.makeToken(tok::TokenType::NEWLINE, "\n", 3, 10),
        lexer.makeToken(tok::TokenType::DEDENT),
        lexer.makeToken(tok::TokenType::DEDENT),
        lexer.makeToken(tok::TokenType::ENDMARKER, "", 4, 1)
    };

    ASSERT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    for (int i = 1; i < tokens.size() - 1; ++i)
        EXPECT_EQ(*expected[i], *tokens[i]);
    EXPECT_EQ(tokens.back()->type(), tok::TokenType::ENDMARKER);
}
