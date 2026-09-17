//
// token.cc
//

#include "ftoken.hpp"

namespace fairuz::tok {

static std::unordered_map<std::string_view, TokenType> const& get_keywords()
{
    static std::unordered_map<std::string_view, TokenType> const map = {
        { "خطا", TokenType::KW_FALSE },
        { "عدم", TokenType::KW_NIL },
        { "صحيح", TokenType::KW_TRUE },
        { "و", TokenType::OP_AND },
        { "اخرج", TokenType::KW_BREAK },
        { "اكمل", TokenType::KW_CONTINUE },
        { "دالة", TokenType::KW_FN },
        { "او", TokenType::OP_OR },
        { "لكل", TokenType::KW_FOR },
        { "في", TokenType::KW_IN },
        { "اذا", TokenType::KW_IF },
        { "غيره", TokenType::KW_ELSE },
        { "ليس", TokenType::OP_NOT },
        { "ارجع", TokenType::KW_RETURN },
        { "طالما", TokenType::KW_WHILE },
        { "نوع", TokenType::KW_CLASS },
        { "هذا", TokenType::KW_THIS },
        { "استورد", TokenType::KW_IMPORT },
        { "من", TokenType::KW_FROM },
        { "باسم", TokenType::KW_AS },
        { "تاكد", TokenType::KW_ASSERT },
        { "assert", TokenType::KW_ASSERT },
    };
    return map;
}

static std::unordered_map<std::string_view, TokenType> const& get_operators()
{
    static std::unordered_map<std::string_view, TokenType> const map = {
        { "=", TokenType::OP_EQ },
        { ":=", TokenType::OP_ASSIGN },
        { "+", TokenType::OP_PLUS },
        { "-", TokenType::OP_MINUS },
        { "*", TokenType::OP_STAR },
        { "/", TokenType::OP_SLASH },
        { "**", TokenType::OP_POWER },
        { "<", TokenType::OP_LT },
        { ">", TokenType::OP_GT },
        { "<=", TokenType::OP_LTE },
        { ">=", TokenType::OP_GTE },
        { "٪", TokenType::OP_PERCENT },
        { "%", TokenType::OP_PERCENT },
        { "!=", TokenType::OP_NEQ },
        { ">>", TokenType::OP_RSHIFT },
        { "<<", TokenType::OP_LSHIFT },
        { "&", TokenType::OP_BITAND },
        { "|", TokenType::OP_BITOR },
        { "~", TokenType::OP_BITNOT },
        { "^", TokenType::OP_BITXOR },
        { "+=", TokenType::OP_PLUSEQ },
        { "-=", TokenType::OP_MINUSEQ },
        { "*=", TokenType::OP_STAREQ },
        { "/=", TokenType::OP_SLASHEQ },
        { "%=", TokenType::OP_PERCENTEQ },
        { "٪=", TokenType::OP_PERCENTEQ },
        { "&=", TokenType::OP_ANDEQ },
        { "|=", TokenType::OP_OREQ },
        { "^=", TokenType::OP_XOREQ },
        { "<<=", TokenType::OP_LSHIFTEQ },
        { ">>=", TokenType::OP_RSHIFTEQ },
    };
    return map;
}

std::optional<TokenType> lookup_keyword(StringRef const& s)
{
    auto it = get_keywords().find(std::string_view(s.data(), s.len()));
    if (it == get_keywords().end())
        return std::nullopt;

    return it->second;
}

std::optional<TokenType> lookup_operator(StringRef const& s)
{
    auto it = get_operators().find(std::string_view(s.data(), s.len()));
    if (it == get_operators().end())
        return std::nullopt;

    return it->second;
}

bool Token::operator==(Token const& other) const
{
    if (m_type == TokenType::INDENT || m_type == TokenType::DEDENT || m_type == TokenType::BEGINMARKER || m_type == TokenType::ENDMARKER)
        return m_type == other.m_type;

    return m_value == other.m_value && m_type == other.m_type && m_location.line == other.m_location.line && m_location.column == other.m_location.column;
}

bool Token::operator!=(Token const& other) const { return !(*this == other); }

StringRef const& Token::lexeme() const { return m_value; }

TokenType const& Token::type() const { return m_type; }

u32 const& Token::line() const { return m_location.line; }

u16 const& Token::column() const { return m_location.column; }

SourceLocation const& Token::location() const { return m_location; }

bool Token::is(TokenType const tt) const { return tt == m_type; }

bool Token::atbol() const { return m_atbol; }

bool Token::is_operator() const
{
    return (m_type >= TokenType::OP_PLUS && m_type <= TokenType::OP_RSHIFTEQ) || m_type == TokenType::OP_AND || m_type == TokenType::OP_OR;
}

bool Token::is_unary_op() const
{
    return m_type == TokenType::OP_PLUS || m_type == TokenType::OP_MINUS || m_type == TokenType::OP_BITNOT || m_type == TokenType::OP_NOT;
}

bool Token::is_binary_op() const
{
    return m_type == TokenType::OP_PLUS
        || m_type == TokenType::OP_MINUS
        || m_type == TokenType::OP_STAR
        || m_type == TokenType::OP_SLASH
        || m_type == TokenType::OP_PERCENT
        || m_type == TokenType::OP_POWER
        || m_type == TokenType::OP_EQ
        || m_type == TokenType::OP_NEQ
        || m_type == TokenType::OP_LT
        || m_type == TokenType::OP_GT
        || m_type == TokenType::OP_LTE
        || m_type == TokenType::OP_GTE
        || m_type == TokenType::OP_BITAND
        || m_type == TokenType::OP_BITOR
        || m_type == TokenType::OP_BITXOR
        || m_type == TokenType::OP_LSHIFT
        || m_type == TokenType::OP_RSHIFT
        || m_type == TokenType::OP_AND
        || m_type == TokenType::OP_OR;
}

bool Token::is_comparison_op() const
{
    return m_type == TokenType::OP_EQ
        || m_type == TokenType::OP_NEQ
        || m_type == TokenType::OP_LT
        || m_type == TokenType::OP_GT
        || m_type == TokenType::OP_LTE
        || m_type == TokenType::OP_GTE;
}

bool Token::is_whitespace() const
{
    return m_type == TokenType::INDENT || m_type == TokenType::DEDENT || m_type == TokenType::NEWLINE;
}

bool Token::is_numeric() const
{
    return m_type == TokenType::INTEGER || m_type == TokenType::HEX || m_type == TokenType::OCTAL
        || m_type == TokenType::BINARY || m_type == TokenType::DECIMAL;
}

f64 Token::to_double() const { return lexeme().to_double(); }

int Token::to_int() const { return static_cast<int>(lexeme().to_double()); }

int Token::get_precedence(bool is_unary) const
{
    switch (m_type) {
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
    case TokenType::OP_EQ:                         // ==
    case TokenType::OP_NEQ: return PREC_EQ;        // !=
    case TokenType::OP_BITAND: return PREC_BITAND; // &
    case TokenType::OP_BITXOR: return PREC_BITXOR; // ^
    case TokenType::OP_BITOR: return PREC_BITOR;   // |
    case TokenType::OP_AND: return PREC_AND;       // and
    case TokenType::OP_OR: return PREC_OR;         // or
    default: return PREC_NONE;
    }
}

StringRef const Token::to_string(TokenType const tt)
{
    switch (tt) {
    case TokenType::OP_EQ: return "=";
    case TokenType::OP_ASSIGN: return ":=";
    case TokenType::OP_PLUS: return "+";
    case TokenType::OP_MINUS: return "-";
    case TokenType::OP_STAR: return "*";
    case TokenType::OP_SLASH: return "/";
    case TokenType::OP_PERCENT: return "%";
    case TokenType::OP_POWER: return "**";
    case TokenType::OP_LT: return "<";
    case TokenType::OP_GT: return ">";
    case TokenType::OP_LTE: return "<=";
    case TokenType::OP_GTE: return ">=";
    case TokenType::OP_NEQ: return "!=";
    case TokenType::OP_BITAND: return "&";
    case TokenType::OP_BITOR: return "|";
    case TokenType::OP_BITXOR: return "^";
    case TokenType::OP_BITNOT: return "~";
    case TokenType::OP_LSHIFT: return "<<";
    case TokenType::OP_RSHIFT: return ">>";
    case TokenType::OP_PLUSEQ: return "+=";
    case TokenType::OP_MINUSEQ: return "-=";
    case TokenType::OP_STAREQ: return "*=";
    case TokenType::OP_SLASHEQ: return "/=";
    case TokenType::OP_PERCENTEQ: return "%=";
    case TokenType::OP_ANDEQ: return "&=";
    case TokenType::OP_OREQ: return "|=";
    case TokenType::OP_XOREQ: return "^=";
    case TokenType::OP_LSHIFTEQ: return "<<=";
    case TokenType::OP_RSHIFTEQ: return ">>=";
    default: return "";
    }
}

} // namespace fairuz::tok
