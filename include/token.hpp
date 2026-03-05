#ifndef _TOKEN_HPP
#define _TOKEN_HPP

#include "macros.hpp"
#include "string.hpp"

#include <cstdint>
#include <iostream>
#include <locale>
#include <string>
#include <unordered_map>
#include <vector>

namespace mylang {
namespace tok {

struct Location {
    std::string filepath { "" };
    uint32_t line { 0 };
    uint32_t column { 0 };
    uint64_t FilePos { 0 };

    Location() = default;

    Location(std::string fpath, size_t line, size_t col, size_t fpos)
        : filepath(fpath)
        , line(line)
        , column(col)
        , FilePos(fpos)
    {
    }
};

enum class TokenType : int {
    // keywords
    KW_IF,       // اذا
    KW_WHILE,    // طالما
    KW_FOR,      // لكل
    KW_IN,       // في
    KW_FN,       // عرف
    KW_RETURN,   // ارجع
    KW_CONTINUE, // اكمل
    KW_BREAK,    // اخرج
    KW_FALSE,    // خطا
    KW_NONE,     // عدم
    KW_TRUE,     // صحيح

    // these are ops with keywords instead of symbols
    OP_AND, // و
    OP_OR,  // او
    OP_NOT, // ليس

    // ops
    OP_PLUS,    // +
    OP_MINUS,   // -
    OP_STAR,    // *
    OP_SLASH,   // /
    OP_PERCENT, // %
    OP_POWER,   // **
    OP_EQ,      // =
    OP_NEQ,     // !=
    OP_LT,      // <
    OP_GT,      // >
    OP_LTE,     // <=
    OP_GTE,     // >=
    OP_ASSIGN,  // :=
    OP_BITAND,  // &
    OP_BITOR,   // |
    OP_BITXOR,  // ^
    OP_BITNOT,  // ~
    OP_LSHIFT,  // <<
    OP_RSHIFT,  // >>

    // augmented assignment
    OP_PLUSEQ,    // +=
    OP_MINUSEQ,   // -=
    OP_STAREQ,    // *=
    OP_SLASHEQ,   // /=
    OP_PERCENTEQ, // %=
    OP_ANDEQ,     // &=
    OP_OREQ,      // |=
    OP_XOREQ,     // ^=
    OP_LSHIFTEQ,  // <<=
    OP_RSHIFTEQ,  // >>=

    // delimiters
    LPAREN,    // (
    RPAREN,    // )
    LBRACKET,  // [
    RBRACKET,  // ]
    LBRACE,    // {
    RBRACE,    // }
    COMMA,     // ,
    COLON,     // :
    SEMICOLON, // ;
    ARROW,     // ->
    DOT,       // .

    // literals
    INTEGER,
    HEX,
    OCTAL,
    BINARY,
    DECIMAL,
    STRING,
    NAME,

    // special
    NEWLINE,
    INDENT,
    DEDENT,
    BEGINMARKER,
    ENDMARKER,

    // identifier
    IDENTIFIER,

    // error
    INVALID
};

static std::unordered_map<StringRef, TokenType, StringRefHash, StringRefEqual> const operators
    = {
          { "=", TokenType::OP_EQ },
          { ":=", TokenType::OP_ASSIGN },
          { "+", TokenType::OP_PLUS },
          { "-", TokenType::OP_MINUS },
          { "*", TokenType::OP_STAR },
          { "/", TokenType::OP_SLASH },
          { "<", TokenType::OP_LT },
          { ">", TokenType::OP_GT },
          { "<=", TokenType::OP_LTE },
          { ">=", TokenType::OP_GTE },
          { "٪", TokenType::OP_PERCENT },
          { "%", TokenType::OP_PERCENT },
          { "!=", TokenType::OP_NEQ }
      };

static std::unordered_map<StringRef, TokenType, StringRefHash, StringRefEqual> const keywords
    = {
          { "خطا", TokenType::KW_FALSE },
          { "عدم", TokenType::KW_NONE },
          { "صحيح", TokenType::KW_TRUE },
          { "و", TokenType::OP_AND },
          { "اخرج", TokenType::KW_BREAK },
          { "اكمل", TokenType::KW_CONTINUE },
          { "عرف", TokenType::KW_FN },
          { "او", TokenType::OP_OR },
          { "بكل", TokenType::KW_FOR },
          { "اذا", TokenType::KW_IF },
          { "ليس", TokenType::OP_NOT },
          { "ارجع", TokenType::KW_RETURN },
          { "طالما", TokenType::KW_WHILE }
      };

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
    Token(StringRef val, TokenType tt, uint32_t line, uint32_t col, uint64_t fpos = 0, std::string fpath = "", bool atbol = false)
        : Value_(val)
        , Type_(tt)
        , Location_(fpath, line, col, fpos)
        , Atbol_(atbol)
    {
    }

    Token()
        : Value_()
        , Type_(TokenType::INVALID)
        , Location_()
        , Atbol_(false)
    {
    }

    Token(Token const&) = default;
    Token(Token&&) noexcept = default;

    bool operator==(Token const& other) const;
    bool operator!=(Token const& other) const;

    Token& operator=(Token const&) = default;
    Token& operator=(Token&&) noexcept = default;

    // Return const references to avoid copies
    StringRef const& lexeme() const;
    TokenType const& type() const;
    uint32_t const& line() const;
    uint32_t const& column() const;
    Location const& location() const;
    std::string const& filepath() const;

    bool is(TokenType const tt) const;

    // is at beginning of a new line
    bool atbol() const;

    bool isOperator() const;
    bool isUnaryOp() const;
    bool isBinaryOp() const;
    bool isComparisonOp() const;
    bool isWhitespace() const;
    bool isNumeric() const;

    double toDouble() const;
    int toInt() const;

    int getPrecedence(bool is_unary = false) const;

    // friend ostream operator for pretty-printing in tests/logs
    friend std::ostream& operator<<(std::ostream& os, Token const& tok)
    {
        os << "Token(\""
           << tok.Value_
           << "\", type="
           << static_cast<int32_t>(tok.Type_)
           << ", line="
           << tok.Location_.line
           << ", col="
           << tok.Location_.column
           << ", file_pos="
           << tok.Location_.FilePos
           << ", file_path="
           << tok.Location_.filepath
           << ")";
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

#endif // _TOKEN_HPP
