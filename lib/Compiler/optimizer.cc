//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Implements the optimizer for constant folding and simplification.
//===----------------------------------------------------------------------===//

#include "Compiler/optimizer.hpp"
#include "Interpreter/gc.hpp"
#include "Interpreter/integer.hpp"
#include "Parser/ast.hpp"
#include "Parser/ast_builder.hpp"

#include <bit>
#include <cmath>

namespace fairuz::runtime {

namespace {

// Compile-time work is deliberately much smaller than the runtime integer limit.
constexpr u32 max_fold_bits = 4096;
constexpr u32 max_literal_bytes = max_fold_bits / 4;

std::optional<Value> numeric_literal(AST::ExprPtr expression, GarbageCollector& gc)
{
    if (AST::is_literal_float(expression)) return Value::from_real(AST::as_literal_float(expression)->value);
    if (!AST::is_literal_int(expression)) return std::nullopt;

    auto* literal = AST::as_literal_int(expression);
    if (literal->large_literal.empty()) return Value::from_int(literal->value, gc);
    if (literal->large_literal.len() > max_literal_bytes) return std::nullopt;
    return integer::finish(integer::parse(literal->large_literal, literal->literal_base), gc);
}

u32 magnitude_bits(Value value)
{
    if (value.is_big_int()) {
        auto const& limbs = value.as_big_int()->limbs;
        return limbs.empty() ? 0 : static_cast<u32>((limbs.size() - 1) * 32 + std::bit_width(limbs.back()));
    }
    i64 n = value.as_int();
    u64 magnitude = n < 0 ? u64 { 0 } - static_cast<u64>(n) : static_cast<u64>(n);
    return static_cast<u32>(std::bit_width(magnitude));
}

AST::ExprPtr make_literal(Value value, SourceLocation loc)
{
    if (value.is_bool()) return AST::ASTBuilder::make_literal_bool(value.as_bool(), loc);
    if (value.is_double()) return AST::ASTBuilder::make_literal_float(value.as_double(), loc);

    i64 small;
    if (integer::to_i64(value, small)) return AST::ASTBuilder::make_literal_int(small, loc);

    auto* literal = AST::ASTBuilder::make_literal_int(i64 { 0 }, loc);
    // StringRef copies into the active arena; no pointer into the temporary GC
    // or the decimal std::string escapes this call.
    literal->large_literal = StringRef(integer::to_string(value).c_str());
    return literal;
}

} // namespace

std::optional<AST::ExprPtr> Optimizer::try_constant_folding(AST::BinaryExpr* e)
{
    if (e == nullptr || e->lhs == nullptr || e->rhs == nullptr) return std::nullopt;

    using Kind = AST::ExprKind;
    GarbageCollector gc;
    auto left = numeric_literal(e->lhs, gc);
    auto right = numeric_literal(e->rhs, gc);
    if (!left || !right) return std::nullopt;

    Value lhs = *left, rhs = *right;
    bool integers = lhs.is_int() && rhs.is_int();
    Value result = Value::nil();
    switch (e->get_kind()) {
    case Kind::OP_ADD: result = integers ? integer::add(lhs, rhs, gc) : Value::from_real(lhs.as_double_any() + rhs.as_double_any()); break;
    case Kind::OP_SUB: result = integers ? integer::sub(lhs, rhs, gc) : Value::from_real(lhs.as_double_any() - rhs.as_double_any()); break;
    case Kind::OP_MUL: result = integers ? integer::mul(lhs, rhs, gc) : Value::from_real(lhs.as_double_any() * rhs.as_double_any()); break;
    case Kind::OP_DIV:
    case Kind::OP_MOD: {
        // Preserve runtime errors and their execution order, including -0.0.
        if (!rhs.is_truthy()) return std::nullopt;
        bool remainder = e->get_kind() == Kind::OP_MOD;
        result = integers ? integer::div(lhs, rhs, gc, remainder)
            : remainder   ? Value::from_real(std::fmod(lhs.as_double_any(), rhs.as_double_any()))
                          : Value::from_real(lhs.as_double_any() / rhs.as_double_any());
        break;
    }
    case Kind::OP_POW:
        if (integers) {
            i64 exponent;
            if (!integer::to_i64(rhs, exponent)) return std::nullopt;
            u32 bits = magnitude_bits(lhs);
            if (exponent >= 0 && (exponent > max_fold_bits || (bits != 0 && exponent > max_fold_bits / bits)))
                return std::nullopt;
            result = integer::pow(lhs, rhs, gc);
        } else {
            result = Value::from_real(std::pow(lhs.as_double_any(), rhs.as_double_any()));
        }
        break;
    case Kind::OP_BITAND:
    case Kind::OP_BITOR:
    case Kind::OP_BITXOR:
        if (!integers) return std::nullopt;
        result = integer::bitwise(lhs, rhs,
            e->get_kind() == Kind::OP_BITAND      ? '&'
                : e->get_kind() == Kind::OP_BITOR ? '|'
                                                  : '^',
            gc);
        break;
    case Kind::OP_LSHIFT:
    case Kind::OP_RSHIFT: {
        if (!integers) return std::nullopt;
        i64 count;
        if (!integer::to_i64(rhs, count) || count < 0) return std::nullopt;
        bool left_shift = e->get_kind() == Kind::OP_LSHIFT;
        if (left_shift && count > max_fold_bits - magnitude_bits(lhs)) return std::nullopt;
        result = integer::shift(lhs, rhs, left_shift, gc);
        break;
    }
    case Kind::OP_EQ:
    case Kind::OP_NEQ:
    case Kind::OP_LT:
    case Kind::OP_LTE:
    case Kind::OP_GT:
    case Kind::OP_GTE: {
        // Do not round large integer operands to double for comparisons.
        // compare_numbers returns 2 for unordered (NaN).
        int compared = integer::compare_numbers(lhs, rhs);
        bool value = false;
        switch (e->get_kind()) {
        case Kind::OP_EQ: value = compared == 0; break;
        case Kind::OP_NEQ: value = compared != 0; break;
        case Kind::OP_LT: value = compared == -1; break;
        case Kind::OP_LTE: value = compared == -1 || compared == 0; break;
        case Kind::OP_GT: value = compared == 1; break;
        case Kind::OP_GTE: value = compared == 1 || compared == 0; break;
        default: break;
        }
        result = Value::from_bool(value);
        break;
    }
    default: return std::nullopt;
    }

    if (result.is_int() && magnitude_bits(result) > max_fold_bits) return std::nullopt;
    return make_literal(result, e->get_location());
}

} // namespace fairuz::runtime
