/// token.cc

#include "../include/token.hpp"

namespace fairuz::tok {

static std::unordered_map<std::string_view, Fa_TokenType> const& getKeywords()
{
    static std::unordered_map<std::string_view, Fa_TokenType> const map = {
        { "خطا", Fa_TokenType::KW_FALSE },
        { "عدم", Fa_TokenType::KW_NONE },
        { "صحيح", Fa_TokenType::KW_TRUE },
        { "و", Fa_TokenType::OP_AND },
        { "اخرج", Fa_TokenType::KW_BREAK },
        { "اكمل", Fa_TokenType::KW_CONTINUE },
        { "دالة", Fa_TokenType::KW_FN },
        { "او", Fa_TokenType::OP_OR },
        { "بكل", Fa_TokenType::KW_FOR },
        { "اذا", Fa_TokenType::KW_IF },
        { "غيره", Fa_TokenType::KW_ELSE },
        { "ليس", Fa_TokenType::OP_NOT },
        { "ارجع", Fa_TokenType::KW_RETURN },
        { "طالما", Fa_TokenType::KW_WHILE },
    };
    return map;
}

static std::unordered_map<std::string_view, Fa_TokenType> const& getOperators()
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
        { "^", Fa_TokenType::OP_BITXOR }
    };
    return map;
}

std::optional<Fa_TokenType> lookupKeyword(Fa_StringRef const& s)
{
    auto it = getKeywords().find(std::string_view(s.data(), s.len()));
    if (it == getKeywords().end())
        return std::nullopt;

    return it->second;
}

std::optional<Fa_TokenType> lookupOperator(Fa_StringRef const& s)
{
    auto it = getOperators().find(std::string_view(s.data(), s.len()));
    if (it == getOperators().end())
        return std::nullopt;

    return it->second;
}

bool Fa_Token::operator==(Fa_Token const& other) const
{
    if (Type_ == Fa_TokenType::INDENT || Type_ == Fa_TokenType::DEDENT || Type_ == Fa_TokenType::BEGINMARKER || Type_ == Fa_TokenType::ENDMARKER)
        return Type_ == other.Type_;

    return Value_ == other.Value_ && Type_ == other.Type_ && Location_.line == other.Location_.line && Location_.column == other.Location_.column;
}

bool Fa_Token::operator!=(Fa_Token const& other) const { return !(*this == other); }

Fa_StringRef const& Fa_Token::lexeme() const { return Value_; }

Fa_TokenType const& Fa_Token::type() const { return Type_; }

u32 const& Fa_Token::line() const { return Location_.line; }

u16 const& Fa_Token::column() const { return Location_.column; }

Location const& Fa_Token::location() const { return Location_; }

std::string const& Fa_Token::filepath() const { return Location_.filepath; }

bool Fa_Token::is(Fa_TokenType const tt) const { return tt == Type_; }

bool Fa_Token::atbol() const { return Atbol_; }

bool Fa_Token::isOperator() const
{
    return (Type_ >= Fa_TokenType::OP_PLUS && Type_ <= Fa_TokenType::OP_RSHIFTEQ)
        || Type_ == Fa_TokenType::OP_AND || Type_ == Fa_TokenType::OP_OR;
}

bool Fa_Token::isUnaryOp() const
{
    return Type_ == Fa_TokenType::OP_PLUS
        || Type_ == Fa_TokenType::OP_MINUS
        || Type_ == Fa_TokenType::OP_BITNOT
        || Type_ == Fa_TokenType::OP_NOT;
}

bool Fa_Token::isBinaryOp() const
{
    return Type_ == Fa_TokenType::OP_PLUS
        || Type_ == Fa_TokenType::OP_MINUS
        || Type_ == Fa_TokenType::OP_STAR
        || Type_ == Fa_TokenType::OP_SLASH
        || Type_ == Fa_TokenType::OP_PERCENT
        || Type_ == Fa_TokenType::OP_POWER
        || Type_ == Fa_TokenType::OP_EQ
        || Type_ == Fa_TokenType::OP_NEQ
        || Type_ == Fa_TokenType::OP_LT
        || Type_ == Fa_TokenType::OP_GT
        || Type_ == Fa_TokenType::OP_LTE
        || Type_ == Fa_TokenType::OP_GTE
        || Type_ == Fa_TokenType::OP_BITAND
        || Type_ == Fa_TokenType::OP_BITOR
        || Type_ == Fa_TokenType::OP_BITXOR
        || Type_ == Fa_TokenType::OP_LSHIFT
        || Type_ == Fa_TokenType::OP_RSHIFT
        || Type_ == Fa_TokenType::OP_AND
        || Type_ == Fa_TokenType::OP_OR;
}

bool Fa_Token::isComparisonOp() const
{
    return Type_ == Fa_TokenType::OP_EQ
        || Type_ == Fa_TokenType::OP_NEQ
        || Type_ == Fa_TokenType::OP_LT
        || Type_ == Fa_TokenType::OP_GT
        || Type_ == Fa_TokenType::OP_LTE
        || Type_ == Fa_TokenType::OP_GTE;
}

bool Fa_Token::isWhitespace() const
{
    return Type_ == Fa_TokenType::INDENT
        || Type_ == Fa_TokenType::DEDENT
        || Type_ == Fa_TokenType::NEWLINE;
}

bool Fa_Token::isNumeric() const
{
    return Type_ == Fa_TokenType::INTEGER
        || Type_ == Fa_TokenType::HEX
        || Type_ == Fa_TokenType::OCTAL
        || Type_ == Fa_TokenType::BINARY
        || Type_ == Fa_TokenType::DECIMAL;
}

f64 Fa_Token::toDouble() const { return lexeme().toDouble(); }

int Fa_Token::toInt() const { return static_cast<int>(lexeme().toDouble()); }

int Fa_Token::getPrecedence(bool is_unary) const
{
    switch (Type_) {
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

Fa_StringRef const Fa_Token::toString(Fa_TokenType const tt)
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
