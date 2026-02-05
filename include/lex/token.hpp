#pragma once

#include "../macros.hpp"
#include "../types.hpp"
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
  SizeType    line{0};
  SizeType    column{0};
  SizeType    FilePos{0};

  Location() = default;

  Location(std::string fpath, SizeType line, SizeType col, SizeType fpos) :
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
  BEGINMARKER,
  ENDMARKER,

  // identifier
  IDENTIFIER,

  // Error
  INVALID
};

static const std::unordered_map<StringRef, TokenType, StringRefHash, StringRefEqual> operators = {
  {u"=", TokenType::OP_EQ},   {u":=", TokenType::OP_ASSIGN}, {u"+", TokenType::OP_PLUS}, {u"-", TokenType::OP_MINUS},
  {u"*", TokenType::OP_STAR}, {u"/", TokenType::OP_SLASH},   {u"<", TokenType::OP_LT},   {u">", TokenType::OP_GT},
  {u"<=", TokenType::OP_LTE}, {u">=", TokenType::OP_GTE},    {u"!=", TokenType::OP_NEQ}};

static const std::unordered_map<StringRef, TokenType, StringRefHash, StringRefEqual> keywords = {
  {u"خطا", TokenType::KW_FALSE},   {u"عدم", TokenType::KW_NONE},      {u"صحيح", TokenType::KW_TRUE}, {u"و", TokenType::KW_AND},
  {u"اخرج", TokenType::KW_RETURN}, {u"اكمل", TokenType::KW_CONTINUE}, {u"عرف", TokenType::KW_FN},    {u"او", TokenType::KW_OR},
  {u"بكل", TokenType::KW_FOR},     {u"اذا", TokenType::KW_IF},        {u"ليس", TokenType::KW_NOT},   {u"ارجع", TokenType::KW_RETURN},
  {u"طالما", TokenType::KW_WHILE}};

static const StringRef toString(TokenType tt)
{
  switch (tt)
  {
  case TokenType::OP_EQ : return u"=";
  case TokenType::OP_ASSIGN : return u":=";
  case TokenType::OP_PLUS : return u"+";
  case TokenType::OP_MINUS : return u"-";
  case TokenType::OP_STAR : return u"*";
  case TokenType::OP_SLASH : return u"/";
  case TokenType::OP_PERCENT : return u"%";
  case TokenType::OP_POWER : return u"**";
  case TokenType::OP_LT : return u"<";
  case TokenType::OP_GT : return u">";
  case TokenType::OP_LTE : return u"<=";
  case TokenType::OP_GTE : return u">=";
  case TokenType::OP_NEQ : return u"!=";
  case TokenType::OP_BITAND : return u"&";
  case TokenType::OP_BITOR : return u"|";
  case TokenType::OP_BITXOR : return u"^";
  case TokenType::OP_BITNOT : return u"~";
  case TokenType::OP_LSHIFT : return u"<<";
  case TokenType::OP_RSHIFT : return u">>";
  case TokenType::OP_PLUSEQ : return u"+=";
  case TokenType::OP_MINUSEQ : return u"-=";
  case TokenType::OP_STAREQ : return u"*=";
  case TokenType::OP_SLASHEQ : return u"/=";
  case TokenType::OP_PERCENTEQ : return u"%=";
  case TokenType::OP_ANDEQ : return u"&=";
  case TokenType::OP_OREQ : return u"|=";
  case TokenType::OP_XOREQ : return u"^=";
  case TokenType::OP_LSHIFTEQ : return u"<<=";
  case TokenType::OP_RSHIFTEQ : return u">>=";
  default : return u"";
  }
}

class Token
{
 public:
  Token(StringRef val, TokenType tt, SizeType line, SizeType col, SizeType fpos, std::string fpath, bool atbol = false) :
      Value_(val),
      Type_(tt),
      Location_(fpath, line, col, fpos),
      Atbol_(atbol)
  {
  }

  Token() :
      Value_(),
      Type_(TokenType::INVALID),
      Location_(),
      Atbol_(false)
  {
  }

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
  const StringRef& lexeme() const { return Value_; }

  const TokenType& type() const { return Type_; }

  SizeType size() const { return Value_.len(); }

  const SizeType& line() const { return Location_.line; }

  const SizeType& column() const { return Location_.column; }

  const Location& location() const { return Location_; }

  const std::string& filepath() const { return Location_.filepath; }

  bool is(const TokenType tt) const { return tt == Type_; }

  // is at beginning of a new line
  bool atbol() const { return Atbol_; }

  // FIXED: Correct operator range check
  bool isOperator() const { return (Type_ >= TokenType::OP_PLUS && Type_ <= TokenType::OP_RSHIFTEQ); }

  bool isUnaryOp() const
  {
    return Type_ == TokenType::OP_PLUS || Type_ == TokenType::OP_MINUS || Type_ == TokenType::OP_BITNOT || Type_ == TokenType::KW_NOT;
  }

  // FIXED: Proper binary operator check
  bool isBinaryOp() const
  {
    // Binary operators are operators that are not unary
    // This excludes unary operators and non-operators
    return isOperator() && !isUnaryOp();
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
    case TokenType::OP_POWER :  // **
      return 4;
    case TokenType::OP_STAR :     // *
    case TokenType::OP_SLASH :    // /
    case TokenType::OP_PERCENT :  // %
      return 3;
    case TokenType::OP_PLUS :   // +
    case TokenType::OP_MINUS :  // -
      return 2;
    default : return -1;  // Not an arithmetic operator
    }
  }

  // FIXED: Added const qualifier
  int getLogicalOpPrecedence() const
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
    default : return -1;  // Not a logical operator
    }
  }

  // friend ostream operator for pretty-printing in tests/logs
  friend std::ostream& operator<<(std::ostream& os, const Token& tok)
  {
    os << "Token(\"" << tok.Value_ << "\", type=" << static_cast<std::int32_t>(tok.Type_) << ", line=" << tok.Location_.line
       << ", col=" << tok.Location_.column << ", file_pos=" << tok.Location_.FilePos << ", file_path=" << tok.Location_.filepath << ")";
    return os;
  }

 private:
  StringRef Value_;
  TokenType Type_;
  Location  Location_;
  bool      Atbol_;  // FIXED: Now properly initialized
};

}  // namespace tok
}  // namespace mylang