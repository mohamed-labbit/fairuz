//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_PARSER_PARSER_MACROS_HPP
#define FAIRUZ_PARSER_PARSER_MACROS_HPP

#include "Parser/ast.hpp"
#include "Parser/token.hpp"
#include "Support/string.hpp"

using TT = fairuz::tok::TokenType;
using EK = fairuz::AST::ExprKind;

// Each expression is evaluated once. These macros propagate existing errors;
// they never emit a second diagnostic for the same failure. FA_PARSE_TRY is a
// declaration and must be used as a standalone statement in a braced scope.
#define FA_PARSE_JOIN_I(a, b) a##b
#define FA_PARSE_JOIN(a, b) FA_PARSE_JOIN_I(a, b)
#define FA_PARSE_TRY(name, expression)                                                                             \
    auto FA_PARSE_JOIN(parse_result_, __LINE__) = (expression);                                                    \
    if (FA_PARSE_JOIN(parse_result_, __LINE__).has_error()) return FA_PARSE_JOIN(parse_result_, __LINE__).error(); \
    auto name = std::move(FA_PARSE_JOIN(parse_result_, __LINE__)).value()
#define FA_PARSE_REQUIRE(expression)                                                 \
    do {                                                                             \
        auto parse_required_result = (expression);                                   \
        if (parse_required_result.has_error()) return parse_required_result.error(); \
    } while (false)

// This spelling is part of the existing parser/compiler interface.
constexpr char class_instance_name[] = "__class$instance";

struct Infix {
    EK kind;
    u32 precedence;
};

// Only left-associative operators belong here. Assignment and exponentiation
// have separate productions. Precedence constants match token.hpp.
static std::optional<Infix> infix_operator(TT type)
{
    switch (type) {
    case TT::OP_OR: return Infix { EK::OP_OR, fairuz::tok::PREC_OR };
    case TT::OP_AND: return Infix { EK::OP_AND, fairuz::tok::PREC_AND };
    case TT::OP_BITOR: return Infix { EK::OP_BITOR, fairuz::tok::PREC_BITOR };
    case TT::OP_BITXOR: return Infix { EK::OP_BITXOR, fairuz::tok::PREC_BITXOR };
    case TT::OP_BITAND: return Infix { EK::OP_BITAND, fairuz::tok::PREC_BITAND };
    case TT::OP_EQ: return Infix { EK::OP_EQ, fairuz::tok::PREC_EQ };
    case TT::OP_NEQ: return Infix { EK::OP_NEQ, fairuz::tok::PREC_EQ };
    case TT::OP_LT: return Infix { EK::OP_LT, fairuz::tok::PREC_CMP };
    case TT::OP_GT: return Infix { EK::OP_GT, fairuz::tok::PREC_CMP };
    case TT::OP_LTE: return Infix { EK::OP_LTE, fairuz::tok::PREC_CMP };
    case TT::OP_GTE: return Infix { EK::OP_GTE, fairuz::tok::PREC_CMP };
    case TT::OP_LSHIFT: return Infix { EK::OP_LSHIFT, fairuz::tok::PREC_SHIFT };
    case TT::OP_RSHIFT: return Infix { EK::OP_RSHIFT, fairuz::tok::PREC_SHIFT };
    case TT::OP_PLUS: return Infix { EK::OP_ADD, fairuz::tok::PREC_BINARY };
    case TT::OP_MINUS: return Infix { EK::OP_SUB, fairuz::tok::PREC_BINARY };
    case TT::OP_STAR: return Infix { EK::OP_MUL, fairuz::tok::PREC_FACTOR };
    case TT::OP_SLASH: return Infix { EK::OP_DIV, fairuz::tok::PREC_FACTOR };
    case TT::OP_PERCENT: return Infix { EK::OP_MOD, fairuz::tok::PREC_FACTOR };
    default: return std::nullopt;
    }
}
inline std::optional<EK> augmented_operator(TT type)
{
    switch (type) {
    case TT::OP_PLUSEQ: return EK::OP_ADD;
    case TT::OP_MINUSEQ: return EK::OP_SUB;
    case TT::OP_STAREQ: return EK::OP_MUL;
    case TT::OP_SLASHEQ: return EK::OP_DIV;
    case TT::OP_PERCENTEQ: return EK::OP_MOD;
    case TT::OP_ANDEQ: return EK::OP_BITAND;
    case TT::OP_OREQ: return EK::OP_BITOR;
    case TT::OP_XOREQ: return EK::OP_BITXOR;
    case TT::OP_LSHIFTEQ: return EK::OP_LSHIFT;
    case TT::OP_RSHIFTEQ: return EK::OP_RSHIFT;
    default: return std::nullopt;
    }
}

inline std::optional<EK> unary_operator(TT type)
{
    switch (type) {
    case TT::OP_PLUS: return EK::OP_PLUS;
    case TT::OP_MINUS: return EK::OP_NEG;
    case TT::OP_BITNOT: return EK::OP_BITNOT;
    case TT::OP_NOT: return EK::OP_NOT;
    default: return std::nullopt;
    }
}

inline bool is_assignment(fairuz::tok::TokenType type)
{
    return type == fairuz::tok::TokenType::OP_ASSIGN || augmented_operator(type).has_value();
}

inline bool is_assignable(fairuz::AST::ExprPtr expr)
{
    assert(expr != nullptr);
    return fairuz::AST::is_identifier(expr) || fairuz::AST::is_index(expr) || fairuz::AST::is_get(expr);
}

// Distinguish overflow from invalid spelling, including invalid bytes AFTER an
// overflow. The utility's bool result cannot make that distinction. This also
// keeps malformed replay tokens on the parser's recoverable error path.
enum class IntegerStatus {
    INVALID,
    SMALL,
    LARGE,
};

inline bool starts_compound(TT type)
{
    return type == TT::KW_IF || type == TT::KW_WHILE || type == TT::KW_FOR || type == TT::KW_FN || type == TT::KW_CLASS;
}

inline bool same_name(fairuz::AST::ExprPtr expr, fairuz::StringRef const& name)
{
    return expr != nullptr && fairuz::AST::is_identifier(expr) && fairuz::AST::as_identifier(expr)->spelling == name;
}

#endif // FAIRUZ_PARSER_PARSER_MACROS_HPP
