#ifndef FA_TOKEN_HPP
#define FA_TOKEN_HPP

#include "fstring.hpp"

#include <sstream>

namespace fairuz::tok {

enum class TokenType : int {
    KW_IF,
    KW_ELSE,
    KW_WHILE,
    KW_FOR,
    KW_IN,
    KW_FN,
    KW_RETURN,
    KW_CONTINUE,
    KW_BREAK,
    KW_FALSE,
    KW_NIL,
    KW_TRUE,
    KW_CLASS,
    KW_THIS,
    KW_IMPORT,
    KW_FROM,
    KW_AS,
    KW_ASSERT,
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
    BINARY,
    OCTAL,
    INTEGER,
    HEX,
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
}; // enum TokenType

[[nodiscard]] std::optional<TokenType> lookup_keyword(StringRef const& s);
[[nodiscard]] std::optional<TokenType> lookup_operator(StringRef const& s);

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
}; // enum

class Token {
public:
    Token(StringRef val, TokenType tt, SourceLocation loc, bool atbol = false)
        : m_value(val)
        , m_type(tt)
        , m_location(loc)
        , m_atbol(atbol)
    {
    }

    Token()
        : m_value()
        , m_type(TokenType::INVALID)
        , m_location()
        , m_atbol(false)
    {
    }

    Token(Token const&) = default;
    Token(Token&&) noexcept = default;

    bool operator==(Token const& other) const;
    bool operator!=(Token const& other) const;

    Token& operator=(Token const&) = default;
    Token& operator=(Token&&) noexcept = default;

    // Return const references to avoid copies
    [[nodiscard]] StringRef const& lexeme() const;

    [[nodiscard]] TokenType const& type() const;

    [[nodiscard]] u32 const& line() const;

    [[nodiscard]] u16 const& column() const;

    [[nodiscard]] SourceLocation const& location() const;

    [[nodiscard]] std::string const& filepath() const;

    [[nodiscard]] bool is(TokenType const tt) const;

    // is at beginning of a newline
    [[nodiscard]] bool atbol() const;

    [[nodiscard]] bool is_operator() const;
    [[nodiscard]] bool is_unary_op() const;
    [[nodiscard]] bool is_binary_op() const;
    [[nodiscard]] bool is_comparison_op() const;
    [[nodiscard]] bool is_whitespace() const;
    [[nodiscard]] bool is_numeric() const;

    [[nodiscard]] f64 to_double() const;
    [[nodiscard]] int to_int() const;

    [[nodiscard]] int get_precedence(bool is_unary = false) const;

    // friend ostream operator for pretty-printing in tests/logs
    friend std::ostream& operator<<(std::ostream& os, Token const& tok)
    {
        os << "Token(\"" << tok.m_value << "\", type=" << static_cast<i32>(tok.m_type) << ", line=" << tok.m_location.line
           << ", col=" << tok.m_location.column << ", file_pos=" << tok.m_location.offset << ")";
        return os;
    }

    static StringRef const to_string(TokenType const tt);

private:
    StringRef m_value;
    TokenType m_type;
    SourceLocation m_location;
    bool m_atbol;
}; // class Token

} // namespace fairuz::tok

#endif // FA_TOKEN_HPP
