/// token.cc

#include "../include/token.hpp"

namespace mylang::tok {

static std::unordered_map<std::string_view, TokenType> const &getKeywords() {
  static std::unordered_map<std::string_view, TokenType> const map = {
      {"خطا", TokenType::KW_FALSE},   {"عدم", TokenType::KW_NONE},      {"صحيح", TokenType::KW_TRUE}, {"و", TokenType::OP_AND},
      {"اخرج", TokenType::KW_BREAK},  {"اكمل", TokenType::KW_CONTINUE}, {"عرف", TokenType::KW_FN},    {"او", TokenType::OP_OR},
      {"بكل", TokenType::KW_FOR},     {"اذا", TokenType::KW_IF},        {"ليس", TokenType::OP_NOT},   {"ارجع", TokenType::KW_RETURN},
      {"طالما", TokenType::KW_WHILE},
  };
  return map;
}

static std::unordered_map<std::string_view, TokenType> const &getOperators() {
  static std::unordered_map<std::string_view, TokenType> const map = {
      {"=", TokenType::OP_EQ},      {":=", TokenType::OP_ASSIGN}, {"+", TokenType::OP_PLUS}, {"-", TokenType::OP_MINUS}, {"*", TokenType::OP_STAR},
      {"/", TokenType::OP_SLASH},   {"<", TokenType::OP_LT},      {">", TokenType::OP_GT},   {"<=", TokenType::OP_LTE},  {">=", TokenType::OP_GTE},
      {"٪", TokenType::OP_PERCENT}, {"%", TokenType::OP_PERCENT}, {"!=", TokenType::OP_NEQ},
  };
  return map;
}

std::optional<TokenType> lookupKeyword(StringRef const &s) {
  auto it = getKeywords().find(std::string_view(s.data(), s.len()));
  if (it == getKeywords().end())
    return std::nullopt;

  return it->second;
}

std::optional<TokenType> lookupOperator(StringRef const &s) {
  auto it = getOperators().find(std::string_view(s.data(), s.len()));
  if (it == getOperators().end())
    return std::nullopt;

  return it->second;
}

bool Token::operator==(Token const &other) const {
  if (Type_ == TokenType::INDENT || Type_ == TokenType::DEDENT || Type_ == TokenType::BEGINMARKER || Type_ == TokenType::ENDMARKER)
    return Type_ == other.Type_;

  return Value_ == other.Value_ && Type_ == other.Type_ && Location_.line == other.Location_.line && Location_.column == other.Location_.column;
}

bool Token::operator!=(Token const &other) const { return !(*this == other); }

StringRef const &Token::lexeme() const { return Value_; }

TokenType const &Token::type() const { return Type_; }

uint32_t const &Token::line() const { return Location_.line; }

uint16_t const &Token::column() const { return Location_.column; }

Location const &Token::location() const { return Location_; }

std::string const &Token::filepath() const { return Location_.filepath; }

bool Token::is(TokenType const tt) const { return tt == Type_; }

bool Token::atbol() const { return Atbol_; }

bool Token::isOperator() const {
  return (Type_ >= TokenType::OP_PLUS && Type_ <= TokenType::OP_RSHIFTEQ) || Type_ == TokenType::OP_AND || Type_ == TokenType::OP_OR;
}

bool Token::isUnaryOp() const {
  return Type_ == TokenType::OP_PLUS || Type_ == TokenType::OP_MINUS || Type_ == TokenType::OP_BITNOT || Type_ == TokenType::OP_NOT;
}

bool Token::isBinaryOp() const {
  return Type_ == TokenType::OP_PLUS || Type_ == TokenType::OP_MINUS || Type_ == TokenType::OP_STAR || Type_ == TokenType::OP_SLASH ||
         Type_ == TokenType::OP_PERCENT || Type_ == TokenType::OP_POWER || Type_ == TokenType::OP_EQ || Type_ == TokenType::OP_NEQ ||
         Type_ == TokenType::OP_LT || Type_ == TokenType::OP_GT || Type_ == TokenType::OP_LTE || Type_ == TokenType::OP_GTE ||
         Type_ == TokenType::OP_BITAND || Type_ == TokenType::OP_BITOR || Type_ == TokenType::OP_BITXOR || Type_ == TokenType::OP_BITNOT ||
         Type_ == TokenType::OP_LSHIFT || Type_ == TokenType::OP_RSHIFT || Type_ == TokenType::OP_AND || Type_ == TokenType::OP_OR;
}

bool Token::isComparisonOp() const {
  return Type_ == TokenType::OP_EQ || Type_ == TokenType::OP_NEQ || Type_ == TokenType::OP_LT || Type_ == TokenType::OP_GT ||
         Type_ == TokenType::OP_LTE || Type_ == TokenType::OP_GTE;
}

bool Token::isWhitespace() const { return Type_ == TokenType::INDENT || Type_ == TokenType::DEDENT || Type_ == TokenType::NEWLINE; }

bool Token::isNumeric() const {
  return Type_ == TokenType::INTEGER || Type_ == TokenType::HEX || Type_ == TokenType::OCTAL || Type_ == TokenType::BINARY ||
         Type_ == TokenType::DECIMAL;
}

double Token::toDouble() const { return lexeme().toDouble(); }

int Token::toInt() const { return static_cast<int>(lexeme().toDouble()); }

int Token::getPrecedence(bool is_unary) const {
  switch (Type_) {
  case TokenType::DOT: // .
    return PREC_POSTFIX;
  case TokenType::OP_BITNOT: // ~
  case TokenType::OP_MINUS:  // -
  case TokenType::OP_PLUS:   // +
    return is_unary ? PREC_UNARY : PREC_BINARY;
  case TokenType::OP_STAR:    // *
  case TokenType::OP_SLASH:   // division /
  case TokenType::OP_PERCENT: // %
    return PREC_FACTOR;
  case TokenType::OP_LSHIFT: // <<
  case TokenType::OP_RSHIFT: // >>
    return PREC_SHIFT;
  case TokenType::OP_GT:  // <
  case TokenType::OP_GTE: // <=
  case TokenType::OP_LT:  // >
  case TokenType::OP_LTE: // >=
    return PREC_CMP;
  case TokenType::OP_EQ:  // ==
  case TokenType::OP_NEQ: // !=
    return PREC_EQ;
  case TokenType::OP_BITAND: // &
    return PREC_BITAND;
  case TokenType::OP_BITXOR: // ^
    return PREC_BITXOR;
  case TokenType::OP_BITOR: // |
    return PREC_BITOR;
  case TokenType::OP_AND: // and
    return PREC_AND;
  case TokenType::OP_OR: // or
    return PREC_OR;
  default:
    return PREC_NONE;
  }
}

StringRef const Token::toString(TokenType const tt) {
  switch (tt) {
  case TokenType::OP_EQ:
    return "=";
  case TokenType::OP_ASSIGN:
    return ":=";
  case TokenType::OP_PLUS:
    return "+";
  case TokenType::OP_MINUS:
    return "-";
  case TokenType::OP_STAR:
    return "*";
  case TokenType::OP_SLASH:
    return "/";
  case TokenType::OP_PERCENT:
    return "%";
  case TokenType::OP_POWER:
    return "**";
  case TokenType::OP_LT:
    return "<";
  case TokenType::OP_GT:
    return ">";
  case TokenType::OP_LTE:
    return "<=";
  case TokenType::OP_GTE:
    return ">=";
  case TokenType::OP_NEQ:
    return "!=";
  case TokenType::OP_BITAND:
    return "&";
  case TokenType::OP_BITOR:
    return "|";
  case TokenType::OP_BITXOR:
    return "^";
  case TokenType::OP_BITNOT:
    return "~";
  case TokenType::OP_LSHIFT:
    return "<<";
  case TokenType::OP_RSHIFT:
    return ">>";
  case TokenType::OP_PLUSEQ:
    return "+=";
  case TokenType::OP_MINUSEQ:
    return "-=";
  case TokenType::OP_STAREQ:
    return "*=";
  case TokenType::OP_SLASHEQ:
    return "/=";
  case TokenType::OP_PERCENTEQ:
    return "%=";
  case TokenType::OP_ANDEQ:
    return "&=";
  case TokenType::OP_OREQ:
    return "|=";
  case TokenType::OP_XOREQ:
    return "^=";
  case TokenType::OP_LSHIFTEQ:
    return "<<=";
  case TokenType::OP_RSHIFTEQ:
    return ">>=";
  default:
    return "";
  }
}

} // namespace mylang::tok
