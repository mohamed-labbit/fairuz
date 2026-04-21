#ifndef TOKEN_HPP
#define TOKEN_HPP

#include "string.hpp"

#include <sstream>

namespace fairuz::tok {

enum class Fa_TokenType : int {
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
}; // enum Fa_TokenType

[[nodiscard]] std::optional<Fa_TokenType> lookup_keyword(Fa_StringRef const& s);
[[nodiscard]] std::optional<Fa_TokenType> lookup_operator(Fa_StringRef const& s);

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

class Fa_Token {
public:
    Fa_Token(Fa_StringRef val, Fa_TokenType tt, Fa_SourceLocation loc, bool atbol = false)
        : m_value(val)
        , m_type(tt)
        , m_location(loc)
        , m_atbol(atbol)
    {
    }

    Fa_Token()
        : m_value()
        , m_type(Fa_TokenType::INVALID)
        , m_location()
        , m_atbol(false)
    {
    }

    Fa_Token(Fa_Token const&) = default;
    Fa_Token(Fa_Token&&) noexcept = default;

    bool operator==(Fa_Token const& other) const;
    bool operator!=(Fa_Token const& other) const;

    Fa_Token& operator=(Fa_Token const&) = default;
    Fa_Token& operator=(Fa_Token&&) noexcept = default;

    // Return const references to avoid copies
    [[nodiscard]] Fa_StringRef const& lexeme() const;

    [[nodiscard]] Fa_TokenType const& type() const;

    [[nodiscard]] u32 const& line() const;

    [[nodiscard]] u16 const& column() const;

    [[nodiscard]] Fa_SourceLocation const& location() const;

    [[nodiscard]] std::string const& filepath() const;

    [[nodiscard]] bool is(Fa_TokenType const tt) const;

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
    friend std::ostream& operator<<(std::ostream& os, Fa_Token const& tok)
    {
        os << "Fa_Token(\"" << tok.m_value << "\", type=" << static_cast<i32>(tok.m_type) << ", line=" << tok.m_location.line
           << ", col=" << tok.m_location.column << ", file_pos=" << tok.m_location.offset << ")";
        return os;
    }

    static Fa_StringRef const to_string(Fa_TokenType const tt);

private:
    Fa_StringRef m_value;
    Fa_TokenType m_type;
    Fa_SourceLocation m_location;
    bool m_atbol;
}; // class Fa_Token

} // namespace fairuz::tok

#endif // TOKEN_HPP
