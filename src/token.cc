/// token.cc

#include "../include/token.hpp"

namespace fairuz::tok {

static std::unordered_map<std::string_view, Fa_TokenType> const& get_keywords()
{
    static std::unordered_map<std::string_view, Fa_TokenType> const map = {
        { "خطا", Fa_TokenType::KW_FALSE },
        { "عدم", Fa_TokenType::KW_NIL },
        { "صحيح", Fa_TokenType::KW_TRUE },
        { "و", Fa_TokenType::OP_AND },
        { "اخرج", Fa_TokenType::KW_BREAK },
        { "اكمل", Fa_TokenType::KW_CONTINUE },
        { "دالة", Fa_TokenType::KW_FN },
        { "او", Fa_TokenType::OP_OR },
        { "بكل", Fa_TokenType::KW_FOR },
        { "في", Fa_TokenType::KW_IN },
        { "اذا", Fa_TokenType::KW_IF },
        { "غيره", Fa_TokenType::KW_ELSE },
        { "ليس", Fa_TokenType::OP_NOT },
        { "ارجع", Fa_TokenType::KW_RETURN },
        { "طالما", Fa_TokenType::KW_WHILE },
    };
    return map;
}

static std::unordered_map<std::string_view, Fa_TokenType> const& get_operators()
{
    static std::unordered_map<std::string_view, Fa_TokenType> const map = {
        { "=", Fa_TokenType::OP_EQ },
        { ":=", Fa_TokenType::OP_ASSIGN },
        { "+", Fa_TokenType::OP_PLUS },
        { "-", Fa_TokenType::OP_MINUS },
        { "*", Fa_TokenType::OP_STAR },
        { "/", Fa_TokenType::OP_SLASH },
        { "**", Fa_TokenType::OP_POWER },
        { "<", Fa_TokenType::OP_LT },
        { ">", Fa_TokenType::OP_GT },
        { "<=", Fa_TokenType::OP_LTE },
        { ">=", Fa_TokenType::OP_GTE },
        { "٪", Fa_TokenType::OP_PERCENT },
        { "%", Fa_TokenType::OP_PERCENT },
        { "!=", Fa_TokenType::OP_NEQ },
        { ">>", Fa_TokenType::OP_RSHIFT },
        { "<<", Fa_TokenType::OP_LSHIFT },
        { "&", Fa_TokenType::OP_BITAND },
        { "|", Fa_TokenType::OP_BITOR },
        { "~", Fa_TokenType::OP_BITNOT },
        { "^", Fa_TokenType::OP_BITXOR },
        { "+=", Fa_TokenType::OP_PLUSEQ },
        { "-=", Fa_TokenType::OP_MINUSEQ },
        { "*=", Fa_TokenType::OP_STAREQ },
        { "/=", Fa_TokenType::OP_SLASHEQ }
    };
    return map;
}

std::optional<Fa_TokenType> lookup_keyword(Fa_StringRef const& s)
{
    auto it = get_keywords().find(std::string_view(s.data(), s.len()));
    if (it == get_keywords().end())
        return std::nullopt;

    return it->second;
}

std::optional<Fa_TokenType> lookup_operator(Fa_StringRef const& s)
{
    auto it = get_operators().find(std::string_view(s.data(), s.len()));
    if (it == get_operators().end())
        return std::nullopt;

    return it->second;
}

bool Fa_Token::operator==(Fa_Token const& other) const
{
    if (m_type == Fa_TokenType::INDENT || m_type == Fa_TokenType::DEDENT || m_type == Fa_TokenType::BEGINMARKER || m_type == Fa_TokenType::ENDMARKER)
        return m_type == other.m_type;

    return m_value == other.m_value && m_type == other.m_type && m_location.line == other.m_location.line && m_location.column == other.m_location.column;
}

bool Fa_Token::operator!=(Fa_Token const& other) const { return !(*this == other); }

Fa_StringRef const& Fa_Token::lexeme() const { return m_value; }

Fa_TokenType const& Fa_Token::type() const { return m_type; }

u32 const& Fa_Token::line() const { return m_location.line; }

u16 const& Fa_Token::column() const { return m_location.column; }

Location const& Fa_Token::location() const { return m_location; }

std::string const& Fa_Token::filepath() const { return m_location.filepath; }

bool Fa_Token::is(Fa_TokenType const tt) const { return tt == m_type; }

bool Fa_Token::atbol() const { return m_atbol; }

bool Fa_Token::is_operator() const
{
    return (m_type >= Fa_TokenType::OP_PLUS && m_type <= Fa_TokenType::OP_RSHIFTEQ) || m_type == Fa_TokenType::OP_AND || m_type == Fa_TokenType::OP_OR;
}

bool Fa_Token::is_unary_op() const
{
    return m_type == Fa_TokenType::OP_PLUS || m_type == Fa_TokenType::OP_MINUS || m_type == Fa_TokenType::OP_BITNOT || m_type == Fa_TokenType::OP_NOT;
}

bool Fa_Token::is_binary_op() const
{
    return m_type == Fa_TokenType::OP_PLUS
        || m_type == Fa_TokenType::OP_MINUS
        || m_type == Fa_TokenType::OP_STAR
        || m_type == Fa_TokenType::OP_SLASH
        || m_type == Fa_TokenType::OP_PERCENT
        || m_type == Fa_TokenType::OP_POWER
        || m_type == Fa_TokenType::OP_EQ
        || m_type == Fa_TokenType::OP_NEQ
        || m_type == Fa_TokenType::OP_LT
        || m_type == Fa_TokenType::OP_GT
        || m_type == Fa_TokenType::OP_LTE
        || m_type == Fa_TokenType::OP_GTE
        || m_type == Fa_TokenType::OP_BITAND
        || m_type == Fa_TokenType::OP_BITOR
        || m_type == Fa_TokenType::OP_BITXOR
        || m_type == Fa_TokenType::OP_LSHIFT
        || m_type == Fa_TokenType::OP_RSHIFT
        || m_type == Fa_TokenType::OP_AND
        || m_type == Fa_TokenType::OP_OR;
}

bool Fa_Token::is_comparison_op() const
{
    return m_type == Fa_TokenType::OP_EQ
        || m_type == Fa_TokenType::OP_NEQ
        || m_type == Fa_TokenType::OP_LT
        || m_type == Fa_TokenType::OP_GT
        || m_type == Fa_TokenType::OP_LTE
        || m_type == Fa_TokenType::OP_GTE;
}

bool Fa_Token::is_whitespace() const
{
    return m_type == Fa_TokenType::INDENT || m_type == Fa_TokenType::DEDENT || m_type == Fa_TokenType::NEWLINE;
}

bool Fa_Token::is_numeric() const
{
    return m_type == Fa_TokenType::INTEGER || m_type == Fa_TokenType::HEX || m_type == Fa_TokenType::OCTAL || m_type == Fa_TokenType::BINARY || m_type == Fa_TokenType::DECIMAL;
}

f64 Fa_Token::to_double() const { return lexeme().to_double(); }

int Fa_Token::to_int() const { return static_cast<int>(lexeme().to_double()); }

int Fa_Token::get_precedence(bool is_unary) const
{
    switch (m_type) {
    case Fa_TokenType::DOT: // .
        return PREC_POSTFIX;
    case Fa_TokenType::OP_BITNOT: // ~
    case Fa_TokenType::OP_MINUS:  // -
    case Fa_TokenType::OP_PLUS:   // +
        return is_unary ? PREC_UNARY : PREC_BINARY;
    case Fa_TokenType::OP_STAR:    // *
    case Fa_TokenType::OP_SLASH:   // division /
    case Fa_TokenType::OP_PERCENT: // %
        return PREC_FACTOR;
    case Fa_TokenType::OP_LSHIFT: // <<
    case Fa_TokenType::OP_RSHIFT: // >>
        return PREC_SHIFT;
    case Fa_TokenType::OP_GT:  // <
    case Fa_TokenType::OP_GTE: // <=
    case Fa_TokenType::OP_LT:  // >
    case Fa_TokenType::OP_LTE: // >=
        return PREC_CMP;
    case Fa_TokenType::OP_EQ:  // ==
    case Fa_TokenType::OP_NEQ: // !=
        return PREC_EQ;
    case Fa_TokenType::OP_BITAND: // &
        return PREC_BITAND;
    case Fa_TokenType::OP_BITXOR: // ^
        return PREC_BITXOR;
    case Fa_TokenType::OP_BITOR: // |
        return PREC_BITOR;
    case Fa_TokenType::OP_AND: // and
        return PREC_AND;
    case Fa_TokenType::OP_OR: // or
        return PREC_OR;
    default:
        return PREC_NONE;
    }
}

Fa_StringRef const Fa_Token::to_string(Fa_TokenType const tt)
{
    switch (tt) {
    case Fa_TokenType::OP_EQ:
        return "=";
    case Fa_TokenType::OP_ASSIGN:
        return ":=";
    case Fa_TokenType::OP_PLUS:
        return "+";
    case Fa_TokenType::OP_MINUS:
        return "-";
    case Fa_TokenType::OP_STAR:
        return "*";
    case Fa_TokenType::OP_SLASH:
        return "/";
    case Fa_TokenType::OP_PERCENT:
        return "%";
    case Fa_TokenType::OP_POWER:
        return "**";
    case Fa_TokenType::OP_LT:
        return "<";
    case Fa_TokenType::OP_GT:
        return ">";
    case Fa_TokenType::OP_LTE:
        return "<=";
    case Fa_TokenType::OP_GTE:
        return ">=";
    case Fa_TokenType::OP_NEQ:
        return "!=";
    case Fa_TokenType::OP_BITAND:
        return "&";
    case Fa_TokenType::OP_BITOR:
        return "|";
    case Fa_TokenType::OP_BITXOR:
        return "^";
    case Fa_TokenType::OP_BITNOT:
        return "~";
    case Fa_TokenType::OP_LSHIFT:
        return "<<";
    case Fa_TokenType::OP_RSHIFT:
        return ">>";
    case Fa_TokenType::OP_PLUSEQ:
        return "+=";
    case Fa_TokenType::OP_MINUSEQ:
        return "-=";
    case Fa_TokenType::OP_STAREQ:
        return "*=";
    case Fa_TokenType::OP_SLASHEQ:
        return "/=";
    case Fa_TokenType::OP_PERCENTEQ:
        return "%=";
    case Fa_TokenType::OP_ANDEQ:
        return "&=";
    case Fa_TokenType::OP_OREQ:
        return "|=";
    case Fa_TokenType::OP_XOREQ:
        return "^=";
    case Fa_TokenType::OP_LSHIFTEQ:
        return "<<=";
    case Fa_TokenType::OP_RSHIFTEQ:
        return ">>=";
    default:
        return "";
    }
}

} // namespace fairuz::tok
