#include "lex/lexer.h"
#include "lex/loader/loader.h"
#include <gtest/gtest.h>
#include <iostream>


const std::string test_cases_path =
  "/Users/mohamedrabbit/mylang/tests/lexer/test_cases/test_indentation.txt";


inline void PrintTo(const Token& tok, std::ostream* os) {
  *os << "Token(\"" << to_utf8(tok.str()) << "\", type=" << static_cast<int>(tok.type())
      << ", line=" << tok.line() << ", col=" << tok.column() << ")" << '\n';
}