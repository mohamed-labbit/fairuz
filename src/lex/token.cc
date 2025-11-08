#include "../../include/lex/token.h"


// getters
namespace mylang {
namespace lex {
namespace tok {


const std::u16string& Token::lexeme() const { return this->value_; }

const TokenType& Token::type() const { return this->type_; }

typename std::size_t Token::size() const { return this->value_.length(); }

const std::size_t& Token::line() const { return this->location_.line_; }

const std::size_t& Token::column() const { return this->location_.column_; }

const typename Token::Location& Token::location() const { return this->location_; }

const std::string& Token::filepath() const { return this->location_.filepath_; }

// operators

bool Token::operator==(const Token& other) const
{
    return value_ == other.value_ && type_ == other.type_
      && location_.line_ == other.location_.line_ && location_.column_ == other.location_.column_;
}

bool Token::operator!=(const Token& other) const { return !(*this == other); }


}
}
}