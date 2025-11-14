#include "../../include/lex/token.hpp"


// getters
namespace mylang {
namespace lex {
namespace tok {


const std::u16string& Token::lexeme() const { return this->value_; }

std::string Token::utf8_lexeme() const { return utf8::utf16to8(this->value_); }

const TokenType& Token::type() const { return this->type_; }

typename std::size_t Token::size() const { return this->value_.length(); }

const std::size_t& Token::line() const { return this->location_.line; }

const std::size_t& Token::column() const { return this->location_.column; }

const typename Token::Location& Token::location() const { return this->location_; }

const std::string& Token::filepath() const { return this->location_.filepath; }

// operators

bool Token::operator==(const Token& other) const
{
    return value_ == other.value_ && type_ == other.type_ && location_.line == other.location_.line
      && location_.column == other.location_.column;
}

bool Token::operator!=(const Token& other) const { return !(*this == other); }


}
}
}