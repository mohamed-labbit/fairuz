#include "../../include/lex/lexer.h"
#include "../../include/lex/token.h"
#include <gtest/gtest.h>
#include <iostream>


const std::string test_cases_path = "/Users/mohamedrabbit/mylang/tests/lexer/test_cases/test_file.txt";


inline void PrintTo(const Token& tok, std::ostream* os) {
    *os << "Token(\"" << to_utf8(tok.str()) << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line()
        << ", col=" << tok.column() << ")" << '\n';
}

TEST(TestFile, TestFileTokenization) {
    Lexer lexer(test_cases_path);

    auto               tokens   = lexer.tokenize();
    std::vector<Token> expected = {{L"", TokenType::START_OF_FILE, {1, 1}},
                                   {L"عرف", TokenType::KW_FN, {1, 1}},
                                   {L"فاكتريال", TokenType::IDENTIFIER, {1, 5}},
                                   {L"(", TokenType::LPAREN, {1, 13}},
                                   {L"س", TokenType::IDENTIFIER, {1, 14}},
                                   {L")", TokenType::RPAREN, {1, 15}},
                                   {L":", TokenType::COLON, {1, 16}},
                                   {L"\n", TokenType::NEWLINE, {1, 17}},

                                   {L"", TokenType::INDENT, {2, 1}},
                                   {L"اذا", TokenType::KW_IF, {2, 5}},
                                   {L"س", TokenType::IDENTIFIER, {2, 9}},
                                   {L"=", TokenType::EQ, {2, 11}},
                                   {L"0", TokenType::NUMBER, {2, 13}},
                                   {L":", TokenType::COLON, {2, 14}},
                                   {L"\n", TokenType::NEWLINE, {2, 15}},

                                   {L"", TokenType::INDENT, {3, 5}},
                                   {L"النتيجة", TokenType::KW_RETURN, {3, 9}},
                                   {L"1", TokenType::NUMBER, {3, 17}},
                                   {L"\n", TokenType::NEWLINE, {3, 18}},

                                   {L"", TokenType::DEDENT, {4, 5}},
                                   {L"", TokenType::DEDENT, {4, 1}},
                                   {L"\n", TokenType::NEWLINE, {4, 1}},  // empty line

                                   {L"", TokenType::INDENT, {5, 1}},
                                   {L"م", TokenType::IDENTIFIER, {5, 5}},
                                   {L":=", TokenType::ASSIGN, {5, 7}},
                                   {L"1", TokenType::NUMBER, {5, 10}},
                                   {L"\n", TokenType::NEWLINE, {5, 11}},

                                   {L"طالما", TokenType::KW_WHILE, {6, 5}},
                                   {L"س", TokenType::IDENTIFIER, {6, 11}},
                                   {L">", TokenType::GT, {6, 13}},
                                   {L"1", TokenType::NUMBER, {6, 15}},
                                   {L":", TokenType::COLON, {6, 16}},
                                   {L"\n", TokenType::NEWLINE, {6, 17}},

                                   {L"", TokenType::INDENT, {7, 5}},
                                   {L"م", TokenType::IDENTIFIER, {7, 9}},
                                   {L":=", TokenType::ASSIGN, {7, 11}},
                                   {L"م", TokenType::IDENTIFIER, {7, 14}},
                                   {L"*", TokenType::STAR, {7, 16}},
                                   {L"س", TokenType::IDENTIFIER, {7, 18}},
                                   {L"\n", TokenType::NEWLINE, {7, 19}},

                                   {L"س", TokenType::IDENTIFIER, {8, 9}},
                                   {L":=", TokenType::ASSIGN, {8, 11}},
                                   {L"س", TokenType::IDENTIFIER, {8, 14}},
                                   {L"-", TokenType::MINUS, {8, 16}},
                                   {L"1", TokenType::NUMBER, {8, 18}},
                                   {L"\n", TokenType::NEWLINE, {8, 19}},

                                   {L"", TokenType::DEDENT, {9, 5}},
                                   {L"", TokenType::DEDENT, {9, 1}},
                                   {L"\n", TokenType::NEWLINE, {9, 1}},  // empty line

                                   {L"", TokenType::INDENT, {10, 1}},
                                   {L"النتيجة", TokenType::KW_RETURN, {10, 5}},
                                   {L"م", TokenType::IDENTIFIER, {10, 13}},
                                   {L"", TokenType::END_OF_FILE, {10, 13}}};

    EXPECT_EQ(tokens.size(), expected.size());

    for (size_t i = 0; i < tokens.size(); i++)
        EXPECT_EQ(tokens[i], expected[i]);

    std::cout << "DEBUG -> indent_size:" << '\n';
    std::cout << lexer.indent_size() << std::endl;
}
