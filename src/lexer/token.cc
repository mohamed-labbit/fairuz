#include "lex/token.h"


// getters

const typename Token::string_type& Token::str() const { return this->value_; }
TokenType                          Token::type() const { return this->type_; }
typename Token::size_type          Token::size() const { return this->value_.length(); }
typename Token::size_type          Token::line() const { return this->location_.line_; }
typename Token::size_type          Token::column() const { return this->location_.column_; }
typename Token::Location           Token::location() const { return this->location_; }


// operators

bool Token::operator==(const Token& other) const {
  return value_ == other.value_ && type_ == other.type_ && location_.line_ == other.location_.line_
      && location_.column_ == other.location_.column_;
}

bool Token::operator!=(const Token& other) const { return !(*this == other); }