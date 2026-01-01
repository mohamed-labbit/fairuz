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
namespace lex {
namespace tok {

struct Location
{
  std::string filepath{""};
  std::size_t line{0};
  std::size_t column{0};
  std::size_t filepos{0};

  Location() = default;

  Location(std::string fpath, std::size_t line, std::size_t col, std::size_t fpos) :
      filepath(fpath),
      line(line),
      column(col),
      filepos(fpos)
  {
  }
};

enum class TokenType : std::int32_t {
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

static const std::unordered_map<string_type, TokenType, util::U16StringHash, util::U16StringEqual> operators = {
  {u"=", TokenType::OP_EQ},   {u":=", TokenType::OP_ASSIGN}, {u"+", TokenType::OP_PLUS}, {u"-", TokenType::OP_MINUS},
  {u"*", TokenType::OP_STAR}, {u"/", TokenType::OP_SLASH},   {u"<", TokenType::OP_LT},   {u">", TokenType::OP_GT},
  {u"<=", TokenType::OP_LTE}, {u">=", TokenType::OP_GTE},    {u"!=", TokenType::OP_NEQ}};

static const std::unordered_map<string_type, TokenType, util::U16StringHash, util::U16StringEqual> keywords = {
  {u"خطا", TokenType::KW_FALSE},   {u"عدم", TokenType::KW_NONE},      {u"صحيح", TokenType::KW_TRUE}, {u"و", TokenType::KW_AND},
  {u"اخرج", TokenType::KW_RETURN}, {u"اكمل", TokenType::KW_CONTINUE}, {u"عرف", TokenType::KW_FN},    {u"او", TokenType::KW_OR},
  {u"بكل", TokenType::KW_FOR},     {u"اذا", TokenType::KW_IF},        {u"ليس", TokenType::KW_NOT},   {u"ارجع", TokenType::KW_RETURN},
  {u"طالما", TokenType::KW_WHILE}};

static const string_type to_string(TokenType tt)
{
  switch (tt)
  {
  case TokenType::OP_EQ : return u"=";
  case TokenType::OP_ASSIGN : return u":=";
  case TokenType::OP_PLUS : return u"+";
  case TokenType::OP_MINUS : return u"-";
  case TokenType::OP_STAR : return u"*";
  case TokenType::OP_SLASH : return u"/";
  case TokenType::OP_LT : return u"<";
  case TokenType::OP_GT : return u">";
  case TokenType::OP_LTE : return u"<=";
  case TokenType::OP_GTE : return u">=";
  case TokenType::OP_NEQ : return u"!=";
  default : return u"";
  }
}

class Token
{
 public:
  Token(string_type val, TokenType tt, std::size_t line, std::size_t col, std::size_t fpos, std::string fpath) :
      value_(std::move(val)),
      type_(tt),
      location_(fpath, line, col, fpos)
  {
  }

  Token() = default;
  Token(const Token&) = default;
  Token(Token&&) MYLANG_NOEXCEPT = default;

  bool operator==(const Token& other) const;
  bool operator!=(const Token& other) const;
  Token& operator=(const Token&) = default;
  Token& operator=(Token&&) MYLANG_NOEXCEPT = default;
  // Return const references to avoid copies
  const string_type& lexeme() const;
  std::string utf8_lexeme() const;
  const TokenType& type() const;
  std::size_t size() const;
  const std::size_t& line() const;
  const std::size_t& column() const;
  const Location& location() const;
  const std::string& filepath() const;
  bool is(const TokenType tt) const { return tt == type_; }

  // friend ostream operator for pretty-printing in tests/logs
  friend std::ostream& operator<<(std::ostream& os, const Token& tok)
  {
    os << "Token(\"" << utf8::utf16to8(tok.value_) << "\", type=" << static_cast<std::int32_t>(tok.type_) << ", line=" << tok.location_.line
       << ", col=" << tok.location_.column << "\", file_pos=" << tok.location_.filepos << "\", file path=" << tok.location_.filepath << ")";
    return os;
  }

 private:
  string_type value_;
  TokenType type_;
  Location location_;
};
}
}  // lex
}  // mylang