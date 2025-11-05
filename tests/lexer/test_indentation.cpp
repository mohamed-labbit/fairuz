#include "lex/lexer.h"
#include <gtest/gtest.h>
#include <iostream>


const std::string test_cases_path = "/Users/mohamedrabbit/mylang/tests/lexer/test_cases/test_indentation.txt";


inline void PrintTo(const mylang::lex::tok::Token& tok, std::ostream* os)
{
    *os << "Token(\"" << utf8::utf16to8(tok.lexeme()) << "\", type=" << static_cast<int>(tok.type()) << ", line=" << tok.line()
        << ", col=" << tok.column() << ")" << '\n';
}