#include "../../include/lex/lexer.hpp"
#include "../../include/lex/token.hpp"
#include <gtest/gtest.h>
#include <iostream>


const std::string test_cases_path = "/Users/mohamedrabbit/code/mylang/tests/lexer/test_cases/test_file.txt";


MYLANG_INLINE void PrintTo(const mylang::lex::tok::Token& tok, std::ostream* os)
{
    *os << "mylang::lex::Token(\"" << utf8::utf16to8(tok.lexeme()) << "\", type=" << static_cast<int>(tok.type())
        << ", line=" << tok.line() << ", col=" << tok.column() << ")" << '\n';
}

#if 0
TEST(TestFile, TestFileTokenization)
{
    mylang::lex::Lexer lexer(test_cases_path);

    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      lexer.make_token(mylang::lex::tok::TokenType::BEGINMARKER, u"", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_FN, u"عرف", 1, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"فاكتريال", 1, 5),
      lexer.make_token(mylang::lex::tok::TokenType::LPAREN, u"(", 1, 13),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 1, 14),
      lexer.make_token(mylang::lex::tok::TokenType::RPAREN, u")", 1, 15),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 1, 16),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 1, 17),
      lexer.make_token(mylang::lex::tok::TokenType::INDENT, u"", 2, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_IF, u"اذا", 2, 5),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 2, 9),
      lexer.make_token(mylang::lex::tok::TokenType::OP_EQ, u"=", 2, 11),
      lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"0", 2, 13),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 2, 14),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 2, 15),
      lexer.make_token(mylang::lex::tok::TokenType::INDENT, u"", 3, 5),
      lexer.make_token(mylang::lex::tok::TokenType::KW_RETURN, u"النتيجة", 3, 9),
      lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"1", 3, 17),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 3, 18),
      lexer.make_token(mylang::lex::tok::TokenType::DEDENT, u"", 4, 5),
      lexer.make_token(mylang::lex::tok::TokenType::DEDENT, u"", 4, 1),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 4, 1),  // empty line
      lexer.make_token(mylang::lex::tok::TokenType::INDENT, u"", 5, 1),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"م", 5, 5),
      lexer.make_token(mylang::lex::tok::TokenType::OP_ASSIGN, u":=", 5, 7),
      lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"1", 5, 10),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 5, 11),
      lexer.make_token(mylang::lex::tok::TokenType::KW_WHILE, u"طالما", 6, 5),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 6, 11),
      lexer.make_token(mylang::lex::tok::TokenType::OP_GT, u">", 6, 13),
      lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"1", 6, 15),
      lexer.make_token(mylang::lex::tok::TokenType::COLON, u":", 6, 16),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 6, 17),
      lexer.make_token(mylang::lex::tok::TokenType::INDENT, u"", 7, 5),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"م", 7, 9),
      lexer.make_token(mylang::lex::tok::TokenType::OP_ASSIGN, u":=", 7, 11),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"م", 7, 14),
      lexer.make_token(mylang::lex::tok::TokenType::OP_STAR, u"*", 16),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 7, 18),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 7, 19),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 8, 9),
      lexer.make_token(mylang::lex::tok::TokenType::OP_ASSIGN, u":=", 8, 11),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"س", 8, 14),
      lexer.make_token(mylang::lex::tok::TokenType::OP_MINUS, u"-", 8, 16),
      lexer.make_token(mylang::lex::tok::TokenType::NUMBER, u"1", 8, 18),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 8, 19),
      lexer.make_token(mylang::lex::tok::TokenType::DEDENT, u"", 9, 5),
      lexer.make_token(mylang::lex::tok::TokenType::DEDENT, u"", 9, 1),
      lexer.make_token(mylang::lex::tok::TokenType::NEWLINE, u"\n", 9, 1),  // empty line
      lexer.make_token(mylang::lex::tok::TokenType::INDENT, u"", 10, 1),
      lexer.make_token(mylang::lex::tok::TokenType::KW_RETURN, u"النتيجة", 10, 5),
      lexer.make_token(mylang::lex::tok::TokenType::IDENTIFIER, u"م", 10, 13),

      lexer.make_token(mylang::lex::tok::TokenType::ENDMARKER, u"", 10, 13)};

    EXPECT_EQ(tokens.size(), expected.size());

    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(tokens[i], expected[i]);

    std::cout << "DEBUG -> indent_size:" << '\n';
    std::cout << lexer.indent_size() << std::endl;
};
#endif