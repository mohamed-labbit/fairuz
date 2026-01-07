#include "../../include/lex/token.hpp"


// getters
namespace mylang {
namespace lex {
namespace tok {

const string_type& Token::lexeme() const { return Value_; }

std::string Token::utf8Lexeme() const { return utf8::utf16to8(Value_); }

const TokenType& Token::type() const { return Type_; }

typename std::size_t Token::size() const { return Value_.length(); }

const std::size_t& Token::line() const { return Location_.line; }

const std::size_t& Token::column() const { return Location_.column; }

const Location& Token::location() const { return Location_; }

const std::string& Token::filepath() const { return Location_.filepath; }

// operators

bool Token::operator==(const Token& other) const
{
  return Value_ == other.Value_ && Type_ == other.Type_ && Location_.line == other.Location_.line && Location_.column == other.Location_.column;
}

bool Token::operator!=(const Token& other) const { return !(*this == other); }

}
}
}