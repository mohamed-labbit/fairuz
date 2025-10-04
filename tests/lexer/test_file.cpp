#include "../../include/lex/lexer.h"
#include "../../include/lex/token.h"
#include <gtest/gtest.h>
#include <iostream>


const std::string test_cases_path = "/Users/mohamedrabbit/mylang/tests/lexer/test_cases/test_file.txt";


inline void PrintTo(const Token& tok, std::ostream* os) {
    *os << "Token(\"" << utf8::utf16to8(tok.str()) << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line()
        << ", col=" << tok.column() << ")" << '\n';
}

TEST(TestFile, TestFileTokenization) {
    Lexer lexer(test_cases_path);

    auto               tokens   = lexer.tokenize();
    std::vector<Token> expected = {{u"", TokenType::START_OF_FILE, {1, 1}},
                                   {u"عرف", TokenType::KW_FN, {1, 1}},
                                   {u"فاكتريال", TokenType::IDENTIFIER, {1, 5}},
                                   {u"(", TokenType::LPAREN, {1, 13}},
                                   {u"س", TokenType::IDENTIFIER, {1, 14}},
                                   {u")", TokenType::RPAREN, {1, 15}},
                                   {u":", TokenType::COLON, {1, 16}},
                                   {u"\n", TokenType::NEWLINE, {1, 17}},

                                   {u"", TokenType::INDENT, {2, 1}},
                                   {u"اذا", TokenType::KW_IF, {2, 5}},
                                   {u"س", TokenType::IDENTIFIER, {2, 9}},
                                   {u"=", TokenType::EQ, {2, 11}},
                                   {u"0", TokenType::NUMBER, {2, 13}},
                                   {u":", TokenType::COLON, {2, 14}},
                                   {u"\n", TokenType::NEWLINE, {2, 15}},

                                   {u"", TokenType::INDENT, {3, 5}},
                                   {u"النتيجة", TokenType::KW_RETURN, {3, 9}},
                                   {u"1", TokenType::NUMBER, {3, 17}},
                                   {u"\n", TokenType::NEWLINE, {3, 18}},

                                   {u"", TokenType::DEDENT, {4, 5}},
                                   {u"", TokenType::DEDENT, {4, 1}},
                                   {u"\n", TokenType::NEWLINE, {4, 1}},  // empty line

                                   {u"", TokenType::INDENT, {5, 1}},
                                   {u"م", TokenType::IDENTIFIER, {5, 5}},
                                   {u":=", TokenType::ASSIGN, {5, 7}},
                                   {u"1", TokenType::NUMBER, {5, 10}},
                                   {u"\n", TokenType::NEWLINE, {5, 11}},

                                   {u"طالما", TokenType::KW_WHILE, {6, 5}},
                                   {u"س", TokenType::IDENTIFIER, {6, 11}},
                                   {u">", TokenType::GT, {6, 13}},
                                   {u"1", TokenType::NUMBER, {6, 15}},
                                   {u":", TokenType::COLON, {6, 16}},
                                   {u"\n", TokenType::NEWLINE, {6, 17}},

                                   {u"", TokenType::INDENT, {7, 5}},
                                   {u"م", TokenType::IDENTIFIER, {7, 9}},
                                   {u":=", TokenType::ASSIGN, {7, 11}},
                                   {u"م", TokenType::IDENTIFIER, {7, 14}},
                                   {u"*", TokenType::STAR, {7, 16}},
                                   {u"س", TokenType::IDENTIFIER, {7, 18}},
                                   {u"\n", TokenType::NEWLINE, {7, 19}},

                                   {u"س", TokenType::IDENTIFIER, {8, 9}},
                                   {u":=", TokenType::ASSIGN, {8, 11}},
                                   {u"س", TokenType::IDENTIFIER, {8, 14}},
                                   {u"-", TokenType::MINUS, {8, 16}},
                                   {u"1", TokenType::NUMBER, {8, 18}},
                                   {u"\n", TokenType::NEWLINE, {8, 19}},

                                   {u"", TokenType::DEDENT, {9, 5}},
                                   {u"", TokenType::DEDENT, {9, 1}},
                                   {u"\n", TokenType::NEWLINE, {9, 1}},  // empty line

                                   {u"", TokenType::INDENT, {10, 1}},
                                   {u"النتيجة", TokenType::KW_RETURN, {10, 5}},
                                   {u"م", TokenType::IDENTIFIER, {10, 13}},
                                   {u"", TokenType::END_OF_FILE, {10, 13}}};

    EXPECT_EQ(tokens.size(), expected.size());

    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(tokens[i], expected[i]);

    std::cout << "DEBUG -> indent_size:" << '\n';
    std::cout << lexer.indent_size() << std::endl;
}
