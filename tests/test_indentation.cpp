#include "../include/lexer.hpp"

#include <gtest/gtest.h>

using namespace fairuz;

std::filesystem::path lexer_test_cases_dir()
{
    static auto const dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
    return dir;
}

inline void PrintTo(tok::Fa_Token const& tok, std::ostream* os)
{
    *os << "tok::Fa_Token(\"" << tok.lexeme() << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line() << ", col=" << tok.column() << ")";
}

TEST(LexerTest, TestIndentationLevel0)
{
    lex::Fa_FileManager m_file_manager(lexer_test_cases_dir() / "recognizes_indentation_level0.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    tok::Fa_Token const* expected = MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ا", 1, 1);
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::Fa_TokenType::ENDMARKER);
}

TEST(LexerTest, TestIndentationLevel1)
{
    lex::Fa_FileManager m_file_manager(lexer_test_cases_dir() / "recognizes_indentation_level1.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<tok::Fa_Token const*> expected = { MAKE_TOKEN(tok::Fa_TokenType::BEGINMARKER, "", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ا", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::NEWLINE, "\n", 1, 2),
        MAKE_TOKEN(tok::Fa_TokenType::INDENT, "", 0, 0),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ا", 2, 5),
        MAKE_TOKEN(tok::Fa_TokenType::NEWLINE, "\n", 2, 6),
        MAKE_TOKEN(tok::Fa_TokenType::DEDENT, "", 0, 0),
        MAKE_TOKEN(tok::Fa_TokenType::DEDENT, "", 0, 0),
        MAKE_TOKEN(tok::Fa_TokenType::ENDMARKER, "", 3, 1) };
    // ASSERT_EQ(tokens.size(), 9);
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    for (int i = 1; i < tokens.size() - 1; ++i)
        EXPECT_EQ(*expected[i], *tokens[i]);
    EXPECT_EQ(tokens.back()->type(), tok::Fa_TokenType::ENDMARKER);
}

TEST(LexerTest, TestIndentationLevel2)
{
    lex::Fa_FileManager m_file_manager(lexer_test_cases_dir() / "recognizes_indentation_level2.txt");
    lex::Fa_Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<tok::Fa_Token const*> expected = { MAKE_TOKEN(tok::Fa_TokenType::BEGINMARKER, "", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ا", 1, 1),
        MAKE_TOKEN(tok::Fa_TokenType::NEWLINE, "\n", 1, 2),
        MAKE_TOKEN(tok::Fa_TokenType::INDENT, "", 0, 0),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ا", 2, 5),
        MAKE_TOKEN(tok::Fa_TokenType::NEWLINE, "\n", 2, 6),
        MAKE_TOKEN(tok::Fa_TokenType::INDENT, "", 0, 0),
        MAKE_TOKEN(tok::Fa_TokenType::IDENTIFIER, "ا", 3, 9),
        MAKE_TOKEN(tok::Fa_TokenType::NEWLINE, "\n", 3, 10),
        MAKE_TOKEN(tok::Fa_TokenType::DEDENT, "", 0, 0),
        MAKE_TOKEN(tok::Fa_TokenType::DEDENT, "", 0, 0),
        MAKE_TOKEN(tok::Fa_TokenType::ENDMARKER, "", 4, 1) };

    ASSERT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens[0]->type(), tok::Fa_TokenType::BEGINMARKER);
    for (int i = 1; i < tokens.size() - 1; ++i)
        EXPECT_EQ(*expected[i], *tokens[i]);
    EXPECT_EQ(tokens.back()->type(), tok::Fa_TokenType::ENDMARKER);
}
