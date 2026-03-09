#ifndef OPTIM_HPP
#define OPTIM_HPP

#include "../ast.hpp"
#include "value.hpp"
#include <optional>

namespace mylang::runtime {

using namespace ast;

static std::optional<Value> constValue(Expr const* e)
{
    if (!e)
        return std::nullopt;
    if (e->getKind() != Expr::Kind::LITERAL)
        return std::nullopt;

    LiteralExpr const* lit = static_cast<LiteralExpr const*>(e);
    if (lit->isBoolean())
        return Value::boolean(lit->getBool());
    /*
    if (lit->isNil())
        return Value::nil();
    */
    if (lit->isInteger())
        return Value::integer(lit->getInt());
    if (lit->isDecimal())
        return Value::real(lit->getFloat());

    // Strings are not folded here (they are heap objects)
    return std::nullopt;
}

static std::optional<Value> tryFoldUnary(UnaryExpr const* e)
{
    std::optional<Value> cv = constValue(e->getOperand());
    if (!cv)
        return std::nullopt;

    switch (e->getOperator()) {
    case UnaryOp::OP_NEG:
        if (cv->isInteger())
            return Value::integer(-cv->asInteger());
        if (cv->isDouble())
            return Value::real(-cv->asDouble());

        return std::nullopt;
    case UnaryOp::OP_NOT:
        return Value::boolean(!cv->isTruthy());
    case UnaryOp::OP_BITNOT:
        if (cv->isInteger())
            return Value::integer(~cv->asInteger());

        return std::nullopt;
    default:
        return std::nullopt;
    }
}

static std::optional<Value> _tryFoldBinary(BinaryExpr const* e)
{
    std::optional<Value> L = constValue(e->getLeft());
    std::optional<Value> R = constValue(e->getRight());

    if (!L || !R)
        return std::nullopt;

    BinaryOp op = e->getOperator();

    // Equality works on all types
    if (op == BinaryOp::OP_EQ)
        return Value::boolean(*L == *R);
    if (op == BinaryOp::OP_NEQ)
        return Value::boolean(*L != *R);
    // Arithmetic requires numeric operands
    if (!L->isNumber() || !R->isNumber())
        return std::nullopt;

    bool both_int = L->isInteger() && R->isInteger();

    int64_t li = L->isInteger() ? L->asInteger() : static_cast<int64_t>(L->asDouble());
    int64_t ri = R->isInteger() ? R->asInteger() : static_cast<int64_t>(R->asDouble());

    double ld = L->asDoubleAny();
    double rd = R->asDoubleAny();

    switch (op) {
    case BinaryOp::OP_ADD: return both_int ? Value::integer(li + ri) : Value::real(ld + rd);
    case BinaryOp::OP_SUB: return both_int ? Value::integer(li - ri) : Value::real(ld - rd);
    case BinaryOp::OP_MUL: return both_int ? Value::integer(li * ri) : Value::real(ld * rd);
    case BinaryOp::OP_DIV:
        if (rd == 0.0)
            return std::nullopt; // don't fold division by zero

        return Value::real(ld / rd);
        /*
        case BinaryOp::OP_IDIV:
        if (ri == 0)
        return std::nullopt;
        return Value::integer(std::floor(ld / rd) >= 0
        ? li / ri
        : (li - (ri - 1)) / ri); // floor division
        */
    case BinaryOp::OP_MOD:
        if (ri == 0)
            return std::nullopt;

        return both_int ? Value::integer(((li % ri) + ri) % ri) : Value::real(std::fmod(ld, rd));
    case BinaryOp::OP_POW: return Value::real(std::pow(ld, rd));
    case BinaryOp::OP_LT: return Value::boolean(ld < rd);
    case BinaryOp::OP_GT: return Value::boolean(ld > rd);
    case BinaryOp::OP_LTE: return Value::boolean(ld <= rd);
    case BinaryOp::OP_GTE: return Value::boolean(ld >= rd);
    case BinaryOp::OP_BITAND: return both_int ? Value::integer(li & ri) : std::optional<Value> { };
    case BinaryOp::OP_BITOR: return both_int ? Value::integer(li | ri) : std::optional<Value> { };
    case BinaryOp::OP_BITXOR: return both_int ? Value::integer(li ^ ri) : std::optional<Value> { };
    case BinaryOp::OP_LSHIFT:
        if (!both_int || ri < 0 || ri >= 64)
            return std::nullopt;

        return Value::integer(li << ri);
    case BinaryOp::OP_RSHIFT:
        if (!both_int || ri < 0 || ri >= 64)
            return std::nullopt;

        return Value::integer(li >> ri);
    default:
        return std::nullopt;
    }
}

static std::optional<Value> tryFoldBinary(BinaryExpr const* e)
{
    if (!e)
        return std::nullopt;

    Expr* LE = e->getLeft();
    Expr* RE = e->getRight();

    if (!LE || !RE)
        return std::nullopt;

    if (LE->getKind() == Expr::Kind::LITERAL && RE->getKind() == Expr::Kind::LITERAL)
        // simple non nested expression
        return _tryFoldBinary(e);

    std::optional<Value> L, R;

    // fold left
    if (LE->getKind() == Expr::Kind::BINARY)
        L = tryFoldBinary(static_cast<BinaryExpr const*>(LE));
    else if (LE->getKind() == Expr::Kind::UNARY)
        L = tryFoldUnary(static_cast<UnaryExpr const*>(LE));

    // fold right
    if (RE->getKind() == Expr::Kind::BINARY)
        R = tryFoldBinary(static_cast<BinaryExpr const*>(RE));
    else if (RE->getKind() == Expr::Kind::UNARY)
        R = tryFoldUnary(static_cast<UnaryExpr const*>(RE));

    if (!R && !L)
        return std::nullopt;

    BinaryExpr* ce = e->clone();

    auto makeLiteralFromVal = [](Value const v) {
        if (v.isInteger())
            return makeLiteralInt(v.asInteger());
        else if (v.isDouble())
            return makeLiteralFloat(v.asDouble());
        else if (v.isBoolean())
            return makeLiteralBool(v.asBoolean());

        return makeLiteralNil();
    };

    if (L)
        ce->setLeft(makeLiteralFromVal(*L));
    if (R)
        ce->setRight(makeLiteralFromVal(*R));

    return _tryFoldBinary(ce);
}

} // namespace mylang::runtime
#endif // OPTIM_HPP
