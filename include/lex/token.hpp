#pragma once

#include "../macros.hpp"
#include "../types/string.hpp"

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
    std::size_t line { 0 };
    std::size_t column { 0 };
    std::size_t FilePos { 0 };

    Location() = default;

    Location(std::string fpath, std::size_t line, std::size_t col, std::size_t fpos)
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
    KW_RETURN,   // رجوع
    KW_CONTINUE, // اكمل
    KW_AND,      // و
    KW_OR,       // او
    KW_NOT,      // ليس
    KW_TRUE,     // صحيح
    KW_FALSE,    // خطا
    KW_NONE,     // عدم

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
    NUMBER,
    FLOAT,
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
    = { { "=", TokenType::OP_EQ }, { ":=", TokenType::OP_ASSIGN }, { "+", TokenType::OP_PLUS }, { "-", TokenType::OP_MINUS },
          { "*", TokenType::OP_STAR }, { "/", TokenType::OP_SLASH }, { "<", TokenType::OP_LT }, { ">", TokenType::OP_GT },
          { "<=", TokenType::OP_LTE }, { ">=", TokenType::OP_GTE },
          { "٪", TokenType::OP_PERCENT }, { "%", TokenType::OP_PERCENT },
          { "!=", TokenType::OP_NEQ } };

static std::unordered_map<StringRef, TokenType, StringRefHash, StringRefEqual> const keywords = { { "خطا", TokenType::KW_FALSE },
    { "عدم", TokenType::KW_NONE }, { "صحيح", TokenType::KW_TRUE }, { "و", TokenType::KW_AND }, { "اخرج", TokenType::KW_RETURN },
    { "اكمل", TokenType::KW_CONTINUE }, { "عرف", TokenType::KW_FN }, { "او", TokenType::KW_OR }, { "بكل", TokenType::KW_FOR },
    { "اذا", TokenType::KW_IF }, { "ليس", TokenType::KW_NOT }, { "ارجع", TokenType::KW_RETURN }, { "طالما", TokenType::KW_WHILE } };

static StringRef const toString(TokenType tt)
{
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

class Token {
public:
    Token(StringRef val, TokenType tt, std::size_t line, std::size_t col, std::size_t fpos, std::string fpath, bool atbol = false)
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

    bool operator==(Token const& other) const
    {
        if (Type_ == TokenType::INDENT || Type_ == TokenType::DEDENT
            || Type_ == TokenType::BEGINMARKER || Type_ == TokenType::ENDMARKER)
            return Type_ == other.Type_;

        return Value_ == other.Value_ && Type_ == other.Type_ && Location_.line == other.Location_.line && Location_.column == other.Location_.column;
    }

    bool operator!=(Token const& other) const
    {
        return !(*this == other);
    }

    Token& operator=(Token const&) = default;
    Token& operator=(Token&&) noexcept = default;

    // Return const references to avoid copies
    StringRef const& lexeme() const
    {
        return Value_;
    }

    TokenType const& type() const
    {
        return Type_;
    }

    std::size_t size() const
    {
        return Value_.len();
    }

    std::size_t const& line() const
    {
        return Location_.line;
    }

    std::size_t const& column() const
    {
        return Location_.column;
    }

    Location const& location() const
    {
        return Location_;
    }

    std::string const& filepath() const
    {
        return Location_.filepath;
    }

    bool is(TokenType const tt) const
    {
        return tt == Type_;
    }

    // is at beginning of a new line
    bool atbol() const
    {
        return Atbol_;
    }

    // FIXED: Correct operator range check
    bool isOperator() const
    {
        return (Type_ >= TokenType::OP_PLUS && Type_ <= TokenType::OP_RSHIFTEQ);
    }

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

    bool isWhitespace() const
    {
        return Type_ == TokenType::INDENT || Type_ == TokenType::DEDENT || Type_ == TokenType::NEWLINE;
    }

    int getArithmeticOpPrecedence() const
    {
        switch (Type_) {
        case TokenType::OP_POWER: // **
            return 4;
        case TokenType::OP_STAR:    // *
        case TokenType::OP_SLASH:   // /
        case TokenType::OP_PERCENT: // %
            return 3;
        case TokenType::OP_PLUS:  // +
        case TokenType::OP_MINUS: // -
            return 2;
        default:
            return -1; // Not an arithmetic operator
        }
    }

    // FIXED: Added const qualifier
    int getLogicalOpPrecedence() const
    {
        switch (Type_) {
        case TokenType::OP_BITOR: // '|'
        case TokenType::KW_OR:    // 'or'
            return 1;
        case TokenType::OP_BITXOR: // '^'
            return 2;
        case TokenType::OP_BITAND: // '&'
        case TokenType::KW_AND:    // 'and'
            return 3;
        default:
            return -1; // Not a logical operator
        }
    }

    // friend ostream operator for pretty-printing in tests/logs
    friend std::ostream& operator<<(std::ostream& os, Token const& tok)
    {
        os << "Token(\"" << tok.Value_ << "\", type=" << static_cast<std::int32_t>(tok.Type_) << ", line=" << tok.Location_.line
           << ", col=" << tok.Location_.column << ", file_pos=" << tok.Location_.FilePos << ", file_path=" << tok.Location_.filepath << ")";
        return os;
    }

private:
    StringRef Value_;
    TokenType Type_;
    Location Location_;
    bool Atbol_;
};

} // namespace tok
} // namespace mylang
