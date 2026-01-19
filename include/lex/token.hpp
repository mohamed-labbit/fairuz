#pragma once

#include "../../utfcpp/source/utf8.h"
#include "../macros.hpp"
#include "util.hpp"

#include <cstdint>
#include <iostream>
#include <locale>
#include <string>
#include <unordered_map>
#include <vector>


namespace mylang {
namespace tok {

struct Location
{
  std::string filepath{""};
  std::size_t line{0};
  std::size_t column{0};
  std::size_t FilePos{0};

  Location() = default;

  Location(std::string fpath, std::size_t line, std::size_t col, std::size_t fpos) :
      filepath(fpath),
      line(line),
      column(col),
      FilePos(fpos)
  {
  }
};

enum class TokenType : int {
  // Keywords (Arabic)
  KW_IF,        // اذا
  KW_WHILE,     // طالما
  KW_FOR,       // لكل
  KW_IN,        // في
  KW_FN,        // عرف
  KW_RETURN,    // رجوع
  KW_CONTINUE,  // اكمل
  KW_AND,       // و
  KW_OR,        // او
  KW_NOT,       // ليس
  KW_TRUE,      // صحيح
  KW_FALSE,     // خطا
  KW_NONE,      // عدم

  /// TODO: reorder into binary and unary
  // Operators
  OP_PLUS,     // +
  OP_MINUS,    // -
  OP_STAR,     // *
  OP_SLASH,    // /
  OP_PERCENT,  // %
  OP_POWER,    // **
  OP_EQ,       // =
  OP_NEQ,      // !=
  OP_LT,       // <
  OP_GT,       // >
  OP_LTE,      // <=
  OP_GTE,      // >=
  OP_ASSIGN,   // :=
  OP_BITAND,   // &
  OP_BITOR,    // |
  OP_BITXOR,   // ^
  OP_BITNOT,   // ~
  OP_LSHIFT,   // <<
  OP_RSHIFT,   // >>

  // Augmented assignment
  OP_PLUSEQ,     // +=
  OP_MINUSEQ,    // -=
  OP_STAREQ,     // *=
  OP_SLASHEQ,    // /=
  OP_PERCENTEQ,  // %=
  OP_ANDEQ,      // &=
  OP_OREQ,       // |=
  OP_XOREQ,      // ^=
  OP_LSHIFTEQ,   // <<=
  OP_RSHIFTEQ,   // >>=

  // Delimiters
  LPAREN,     // (
  RPAREN,     // )
  LBRACKET,   // [
  RBRACKET,   // ]
  LBRACE,     // {
  RBRACE,     // }
  COMMA,      // ,
  COLON,      // :
  SEMICOLON,  // ;
  ARROW,      // ->
  DOT,        // .

  // Literals
  NUMBER,
  FLOAT,
  STRING,
  NAME,

  // Special
  NEWLINE,
  INDENT,
  DEDENT,
  ENDMARKER,
  BEGINMARKER,
  TYPE_COMMENT,

  // identifier
  IDENTIFIER,

  // Error
  INVALID
};

static const std::unordered_map<StringType, TokenType, util::U16StringHash, util::U16StringEqual> operators = {
  {u"=", TokenType::OP_EQ},   {u":=", TokenType::OP_ASSIGN}, {u"+", TokenType::OP_PLUS}, {u"-", TokenType::OP_MINUS},
  {u"*", TokenType::OP_STAR}, {u"/", TokenType::OP_SLASH},   {u"<", TokenType::OP_LT},   {u">", TokenType::OP_GT},
  {u"<=", TokenType::OP_LTE}, {u">=", TokenType::OP_GTE},    {u"!=", TokenType::OP_NEQ}};

static const std::unordered_map<StringType, TokenType, util::U16StringHash, util::U16StringEqual> keywords = {
  {u"خطا", TokenType::KW_FALSE},   {u"عدم", TokenType::KW_NONE},      {u"صحيح", TokenType::KW_TRUE}, {u"و", TokenType::KW_AND},
  {u"اخرج", TokenType::KW_RETURN}, {u"اكمل", TokenType::KW_CONTINUE}, {u"عرف", TokenType::KW_FN},    {u"او", TokenType::KW_OR},
  {u"بكل", TokenType::KW_FOR},     {u"اذا", TokenType::KW_IF},        {u"ليس", TokenType::KW_NOT},   {u"ارجع", TokenType::KW_RETURN},
  {u"طالما", TokenType::KW_WHILE}};

static const StringType toString(TokenType tt)
{
  switch (tt)
  {
  case TokenType::OP_EQ :
    return u"=";
  case TokenType::OP_ASSIGN :
    return u":=";
  case TokenType::OP_PLUS :
    return u"+";
  case TokenType::OP_MINUS :
    return u"-";
  case TokenType::OP_STAR :
    return u"*";
  case TokenType::OP_SLASH :
    return u"/";
  case TokenType::OP_LT :
    return u"<";
  case TokenType::OP_GT :
    return u">";
  case TokenType::OP_LTE :
    return u"<=";
  case TokenType::OP_GTE :
    return u">=";
  case TokenType::OP_NEQ :
    return u"!=";
  default :
    return u"";
  }
}

class Token
{
 public:
  Token(StringType val, TokenType tt, std::size_t line, std::size_t col, std::size_t fpos, std::string fpath) :
      Value_(std::move(val)),
      Type_(tt),
      Location_(fpath, line, col, fpos)
  {
  }

  Token()                        = default;
  Token(const Token&)            = default;
  Token(Token&&) MYLANG_NOEXCEPT = default;

  bool operator==(const Token& other) const
  {
    if (Type_ == TokenType::INDENT || Type_ == TokenType::DEDENT)
      return Type_ == other.Type_;
    return Value_ == other.Value_ && Type_ == other.Type_ && Location_.line == other.Location_.line && Location_.column == other.Location_.column;
  }

  bool operator!=(const Token& other) const { return !(*this == other); }

  Token& operator=(const Token&)            = default;
  Token& operator=(Token&&) MYLANG_NOEXCEPT = default;

  // Return const references to avoid copies
  const StringType& lexeme() const { return Value_; }

  std::string utf8Lexeme() const { return utf8::utf16to8(Value_); }

  const TokenType& type() const { return Type_; }

  std::size_t size() const { return Value_.length(); }

  const std::size_t& line() const { return Location_.line; }

  const std::size_t& column() const { return Location_.column; }

  const Location& location() const { return Location_; }

  const std::string& filepath() const { return Location_.filepath; }

  bool is(const TokenType tt) const { return tt == Type_; }

  // is at beginning of a new line
  bool atbol() const { return Atbol_; }

  bool isOperator() const { return Type_ > TokenType::OP_PLUS && Type_ < TokenType::OP_RSHIFTEQ; }

  bool isUnaryOp() const
  {
    return Type_ == TokenType::OP_PLUS || Type_ == TokenType::OP_MINUS || Type_ == TokenType::OP_BITNOT || Type_ == TokenType::KW_NOT;
  }

  bool isBinaryOp() const
  {
    // well?
    return !isUnaryOp();
  }

  bool isComparisonOp() const
  {
    return Type_ == TokenType::OP_EQ || Type_ == TokenType::OP_NEQ || Type_ == TokenType::OP_LT || Type_ == TokenType::OP_GT
           || Type_ == TokenType::OP_LTE || Type_ == TokenType::OP_GTE;
  }

  int getArithmeticOpPrecedence() const
  {
    switch (Type_)
    {
    case TokenType::OP_STAR :   // *
    case TokenType::OP_SLASH :  // /
      return 3;
    case TokenType::OP_PLUS :   // +
    case TokenType::OP_MINUS :  // -
      return 2;
    default :
      return -1;  // Not an arithmetic operator
    }
  }

  int getLogicalOpPrecedence()
  {
    switch (Type_)
    {
    case TokenType::OP_BITOR :  // '|'
    case TokenType::KW_OR :     // 'or'
      return 1;
    case TokenType::OP_BITXOR :  // '^'
      return 2;
    case TokenType::OP_BITAND :  // '&'
    case TokenType::KW_AND :     // 'and'
      return 3;
    default :
      return -1;  // Not a logical operator
    }
  }

  // friend ostream operator for pretty-printing in tests/logs
  friend std::ostream& operator<<(std::ostream& os, const Token& tok)
  {
    os << "Token(\"" << utf8::utf16to8(tok.Value_) << "\", type=" << static_cast<std::int32_t>(tok.Type_) << ", line=" << tok.Location_.line
       << ", col=" << tok.Location_.column << "\", file_pos=" << tok.Location_.FilePos << "\", file path=" << tok.Location_.filepath << ")";
    return os;
  }

 private:
  StringType Value_;
  TokenType  Type_;
  Location   Location_;

  bool Atbol_;
};

}  // tok
}  // mylang