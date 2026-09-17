#include "../fairuz/flexer.hpp"

#include <gtest/gtest.h>

using namespace fairuz;

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
    lex::FileManager m_file_manager(lexer_test_cases_dir() / "recognizes_indentation_level0.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    TokenPtr expected = make_token(tok::TokenType::IDENTIFIER, "ا", { 1, 1, 0, 0 });
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    EXPECT_EQ(*tokens[1], *expected);
    EXPECT_EQ(tokens[2]->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, TestIndentationLevel1)
{
    lex::FileManager m_file_manager(lexer_test_cases_dir() / "recognizes_indentation_level1.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<TokenPtr> expected = { make_token(tok::TokenType::BEGINMARKER, "", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "ا", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::NEWLINE, "\n", { 1, 2, 0, 0 }),
        make_token(tok::TokenType::INDENT, "", { 0, 0, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "ا", { 2, 5, 0, 0 }),
        make_token(tok::TokenType::NEWLINE, "\n", { 2, 6, 0, 0 }),
        make_token(tok::TokenType::DEDENT, "", { 0, 0, 0, 0 }),
        make_token(tok::TokenType::DEDENT, "", { 0, 0, 0, 0 }),
        make_token(tok::TokenType::ENDMARKER, "", { 3, 1, 0, 0 }) };
    // ASSERT_EQ(tokens.size(), 9);
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    for (u32 i = 1; i < tokens.size() - 1; i++)
        EXPECT_EQ(*expected[i], *tokens[i]);
    EXPECT_EQ(tokens.back()->type(), tok::TokenType::ENDMARKER);
}

TEST(LexerTest, TestIndentationLevel2)
{
    lex::FileManager m_file_manager(lexer_test_cases_dir() / "recognizes_indentation_level2.fa");
    lex::Lexer m_lexer(&m_file_manager);
    auto tokens = m_lexer.tokenize();
    std::vector<TokenPtr> expected = { make_token(tok::TokenType::BEGINMARKER, "", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "ا", { 1, 1, 0, 0 }),
        make_token(tok::TokenType::NEWLINE, "\n", { 1, 2, 0, 0 }),
        make_token(tok::TokenType::INDENT, "", { 0, 0, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "ا", { 2, 5, 0, 0 }),
        make_token(tok::TokenType::NEWLINE, "\n", { 2, 6, 0, 0 }),
        make_token(tok::TokenType::INDENT, "", { 0, 0, 0, 0 }),
        make_token(tok::TokenType::IDENTIFIER, "ا", { 3, 9, 0, 0 }),
        make_token(tok::TokenType::NEWLINE, "\n", { 3, 10, 0, 0 }),
        make_token(tok::TokenType::DEDENT, "", { 0, 0, 0, 0 }),
        make_token(tok::TokenType::DEDENT, "", { 0, 0, 0, 0 }),
        make_token(tok::TokenType::ENDMARKER, "", { 4, 1, 0, 0 }) };

    ASSERT_EQ(tokens.size(), expected.size());
    EXPECT_EQ(tokens[0]->type(), tok::TokenType::BEGINMARKER);
    for (u32 i = 1; i < tokens.size() - 1; i++)
        EXPECT_EQ(*expected[i], *tokens[i]);
    EXPECT_EQ(tokens.back()->type(), tok::TokenType::ENDMARKER);
}
