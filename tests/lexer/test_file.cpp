#include "../../include/lex/lexer.h"
#include "../../include/lex/token.h"
#include <gtest/gtest.h>
#include <iostream>


const std::string test_cases_path =
  "/Users/mohamedrabbit/mylang/tests/lexer/test_cases/test_file.txt";


inline void PrintTo(const mylang::lex::tok::Token& tok, std::ostream* os)
{
    *os << "mylang::lex::Token(\"" << utf8::utf16to8(tok.str())
        << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line()
        << ", col=" << tok.column() << ")" << '\n';
}

TEST(TestFile, TestFileTokenization)
{
    mylang::lex::Lexer lexer(test_cases_path);

    auto tokens = lexer.tokenize();
    std::vector<mylang::lex::tok::Token> expected = {
      {u"", mylang::lex::tok::TokenType::START_OF_FILE, {1, 1}},
      {u"عرف", mylang::lex::tok::TokenType::KW_FN, {1, 1}},
      {u"فاكتريال", mylang::lex::tok::TokenType::IDENTIFIER, {1, 5}},
      {u"(", mylang::lex::tok::TokenType::LPAREN, {1, 13}},
      {u"س", mylang::lex::tok::TokenType::IDENTIFIER, {1, 14}},
      {u")", mylang::lex::tok::TokenType::RPAREN, {1, 15}},
      {u":", mylang::lex::tok::TokenType::COLON, {1, 16}},
      {u"\n", mylang::lex::tok::TokenType::NEWLINE, {1, 17}},
      {u"", mylang::lex::tok::TokenType::INDENT, {2, 1}},
      {u"اذا", mylang::lex::tok::TokenType::KW_IF, {2, 5}},
      {u"س", mylang::lex::tok::TokenType::IDENTIFIER, {2, 9}},
      {u"=", mylang::lex::tok::TokenType::EQ, {2, 11}},
      {u"0", mylang::lex::tok::TokenType::NUMBER, {2, 13}},
      {u":", mylang::lex::tok::TokenType::COLON, {2, 14}},
      {u"\n", mylang::lex::tok::TokenType::NEWLINE, {2, 15}},
      {u"", mylang::lex::tok::TokenType::INDENT, {3, 5}},
      {u"النتيجة", mylang::lex::tok::TokenType::KW_RETURN, {3, 9}},
      {u"1", mylang::lex::tok::TokenType::NUMBER, {3, 17}},
      {u"\n", mylang::lex::tok::TokenType::NEWLINE, {3, 18}},
      {u"", mylang::lex::tok::TokenType::DEDENT, {4, 5}},
      {u"", mylang::lex::tok::TokenType::DEDENT, {4, 1}},
      {u"\n", mylang::lex::tok::TokenType::NEWLINE, {4, 1}},  // empty line
      {u"", mylang::lex::tok::TokenType::INDENT, {5, 1}},
      {u"م", mylang::lex::tok::TokenType::IDENTIFIER, {5, 5}},
      {u":=", mylang::lex::tok::TokenType::ASSIGN, {5, 7}},
      {u"1", mylang::lex::tok::TokenType::NUMBER, {5, 10}},
      {u"\n", mylang::lex::tok::TokenType::NEWLINE, {5, 11}},
      {u"طالما", mylang::lex::tok::TokenType::KW_WHILE, {6, 5}},
      {u"س", mylang::lex::tok::TokenType::IDENTIFIER, {6, 11}},
      {u">", mylang::lex::tok::TokenType::GT, {6, 13}},
      {u"1", mylang::lex::tok::TokenType::NUMBER, {6, 15}},
      {u":", mylang::lex::tok::TokenType::COLON, {6, 16}},
      {u"\n", mylang::lex::tok::TokenType::NEWLINE, {6, 17}},
      {u"", mylang::lex::tok::TokenType::INDENT, {7, 5}},
      {u"م", mylang::lex::tok::TokenType::IDENTIFIER, {7, 9}},
      {u":=", mylang::lex::tok::TokenType::ASSIGN, {7, 11}},
      {u"م", mylang::lex::tok::TokenType::IDENTIFIER, {7, 14}},
      {u"*", mylang::lex::tok::TokenType::STAR, {7, 16}},
      {u"س", mylang::lex::tok::TokenType::IDENTIFIER, {7, 18}},
      {u"\n", mylang::lex::tok::TokenType::NEWLINE, {7, 19}},
      {u"س", mylang::lex::tok::TokenType::IDENTIFIER, {8, 9}},
      {u":=", mylang::lex::tok::TokenType::ASSIGN, {8, 11}},
      {u"س", mylang::lex::tok::TokenType::IDENTIFIER, {8, 14}},
      {u"-", mylang::lex::tok::TokenType::MINUS, {8, 16}},
      {u"1", mylang::lex::tok::TokenType::NUMBER, {8, 18}},
      {u"\n", mylang::lex::tok::TokenType::NEWLINE, {8, 19}},
      {u"", mylang::lex::tok::TokenType::DEDENT, {9, 5}},
      {u"", mylang::lex::tok::TokenType::DEDENT, {9, 1}},
      {u"\n", mylang::lex::tok::TokenType::NEWLINE, {9, 1}},  // empty line
      {u"", mylang::lex::tok::TokenType::INDENT, {10, 1}},
      {u"النتيجة", mylang::lex::tok::TokenType::KW_RETURN, {10, 5}},
      {u"م", mylang::lex::tok::TokenType::IDENTIFIER, {10, 13}},
      {u"", mylang::lex::tok::TokenType::END_OF_FILE, {10, 13}}};

    EXPECT_EQ(tokens.size(), expected.size());

    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(tokens[i], expected[i]);

    std::cout << "DEBUG -> indent_size:" << '\n';
    std::cout << lexer.indent_size() << std::endl;
}
