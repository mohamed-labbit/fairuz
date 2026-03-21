#ifndef TOKEN_HPP
#define TOKEN_HPP

#include "string.hpp"

#include <sstream>

namespace mylang {
namespace tok {

struct Location {
  std::string filepath{""};
  uint32_t line{0};
  uint16_t column{0};
  uint64_t FilePos{0};

  Location() = default;

  Location(std::string fpath, size_t line, size_t col, size_t fpos) : filepath(fpath), line(line), column(col), FilePos(fpos) {}
};

enum class TokenType : int {
  KW_IF,
  KW_WHILE,
  KW_FOR,
  KW_IN,
  KW_FN,
  KW_RETURN,
  KW_CONTINUE,
  KW_BREAK,
  KW_FALSE,
  KW_NONE,
  KW_TRUE,
  OP_AND,
  OP_OR,
  OP_NOT,
  OP_PLUS,
  OP_MINUS,
  OP_STAR,
  OP_SLASH,
  OP_PERCENT,
  OP_POWER,
  OP_EQ,
  OP_NEQ,
  OP_LT,
  OP_GT,
  OP_LTE,
  OP_GTE,
  OP_ASSIGN,
  OP_BITAND,
  OP_BITOR,
  OP_BITXOR,
  OP_BITNOT,
  OP_LSHIFT,
  OP_RSHIFT,
  OP_PLUSEQ,
  OP_MINUSEQ,
  OP_STAREQ,
  OP_SLASHEQ,
  OP_PERCENTEQ,
  OP_ANDEQ,
  OP_OREQ,
  OP_XOREQ,
  OP_LSHIFTEQ,
  OP_RSHIFTEQ,
  LPAREN,
  RPAREN,
  LBRACKET,
  RBRACKET,
  LBRACE,
  RBRACE,
  COMMA,
  COLON,
  DOT,
  INTEGER,
  HEX,
  OCTAL,
  BINARY,
  DECIMAL,
  STRING,
  NAME,
  NEWLINE,
  INDENT,
  DEDENT,
  BEGINMARKER,
  ENDMARKER,
  IDENTIFIER,
  INVALID
};

MY_NODISCARD std::optional<TokenType> lookupKeyword(StringRef const &s);
MY_NODISCARD std::optional<TokenType> lookupOperator(StringRef const &s);

enum {
  PREC_COMMA,
  PREC_ASSIGN,
  PREC_TERNARY,
  PREC_OR,
  PREC_AND,
  PREC_BITOR,
  PREC_BITXOR,
  PREC_BITAND,
  PREC_EQ,
  PREC_CMP,
  PREC_SHIFT,
  PREC_BINARY,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_POSTFIX,
  PREC_NONE
};

class Token {
public:
  Token(StringRef val, TokenType tt, uint32_t line, uint16_t col, uint64_t fpos = 0, std::string fpath = "", bool atbol = false)
      : Value_(val), Type_(tt), Location_(fpath, line, col, fpos), Atbol_(atbol) {}

  Token() : Value_(), Type_(TokenType::INVALID), Location_(), Atbol_(false) {}

  Token(Token const &) = default;
  Token(Token &&) noexcept = default;

  bool operator==(Token const &other) const;
  bool operator!=(Token const &other) const;

  Token &operator=(Token const &) = default;
  Token &operator=(Token &&) noexcept = default;

  // Return const references to avoid copies
  MY_NODISCARD StringRef const &lexeme() const;

  MY_NODISCARD TokenType const &type() const;

  MY_NODISCARD uint32_t const &line() const;

  MY_NODISCARD uint16_t const &column() const;

  MY_NODISCARD Location const &location() const;

  MY_NODISCARD std::string const &filepath() const;

  MY_NODISCARD bool is(TokenType const tt) const;

  // is at beginning of a newline
  MY_NODISCARD bool atbol() const;

  MY_NODISCARD bool isOperator() const;
  MY_NODISCARD bool isUnaryOp() const;
  MY_NODISCARD bool isBinaryOp() const;
  MY_NODISCARD bool isComparisonOp() const;
  MY_NODISCARD bool isWhitespace() const;
  MY_NODISCARD bool isNumeric() const;

  MY_NODISCARD double toDouble() const;
  MY_NODISCARD int toInt() const;

  MY_NODISCARD int getPrecedence(bool is_unary = false) const;

  // friend ostream operator for pretty-printing in tests/logs
  friend std::ostream &operator<<(std::ostream &os, Token const &tok) {
    os << "Token(\"" << tok.Value_ << "\", type=" << static_cast<int32_t>(tok.Type_) << ", line=" << tok.Location_.line
       << ", col=" << tok.Location_.column << ", file_pos=" << tok.Location_.FilePos << ", file_path=" << tok.Location_.filepath << ")";
    return os;
  }

  static StringRef const toString(TokenType const tt);

private:
  StringRef Value_;
  TokenType Type_;
  Location Location_;
  bool Atbol_;
};

} // namespace tok
} // namespace mylang

#endif // TOKEN_HPP
