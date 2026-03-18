#ifndef OPTIM_HPP
#define OPTIM_HPP

#include "../ast.hpp"
#include "value_.hpp"
#include <cstdint>
#include <optional>

namespace mylang::runtime {

using namespace ast;

static std::optional<Value> constValue(Expr const* e)
{
    if (!e || e->getKind() != Expr::Kind::LITERAL)
        return std::nullopt;

    LiteralExpr const* lit = static_cast<LiteralExpr const*>(e);

    if (lit->isNil())
        return NIL_VAL;
    if (lit->isBoolean())
        return makeBool(lit->getBool());
    if (lit->isInteger())
        return makeInteger(lit->getInt());
    if (lit->isDecimal())
        return makeReal(lit->getFloat());

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
        if (isNumber(*cv))
            return IS_INTEGER(*cv) ? makeInteger(-asInteger(*cv)) : makeReal(-asDouble(*cv));

        return std::nullopt;
    case UnaryOp::OP_NOT:
        return makeBool(!isTruthy(*cv));
    case UnaryOp::OP_BITNOT:
        if (IS_INTEGER(*cv))
            return makeInteger(~asInteger(*cv));

        return std::nullopt;
    default:
        return std::nullopt;
    }
}

static std::optional<Value> _tryFoldBinary(BinaryExpr const* e)
{
    auto L = constValue(e->getLeft());
    auto R = constValue(e->getRight());

    if (!L || !R)
        return std::nullopt;

    BinaryOp op = e->getOperator();

    // Equality works on all types
    if (op == BinaryOp::OP_EQ)
        return makeBool(*L == *R);

    if (op == BinaryOp::OP_NEQ)
        return makeBool(*L != *R);

    // Arithmetic requires numeric operands
    if (!isNumber(*L) || !isNumber(*R))
        return std::nullopt;

    bool both_ints = IS_INTEGER(*L) && IS_INTEGER(*R);

    double ld = asDouble(*L);
    double rd = asDouble(*R);

    int64_t li = IS_INTEGER(*L) ? asInteger(*L) : static_cast<int64_t>(asDouble(*L));
    int64_t ri = IS_INTEGER(*R) ? asInteger(*R) : static_cast<int64_t>(asDouble(*R));

    switch (op) {
    case BinaryOp::OP_ADD:
        return both_ints ? makeInteger(li + ri) : makeReal(ld + rd);
    case BinaryOp::OP_SUB:
        return both_ints ? makeInteger(li - ri) : makeReal(ld - rd);
    case BinaryOp::OP_MUL:
        return both_ints ? makeInteger(li * ri) : makeReal(ld * rd);
    case BinaryOp::OP_DIV:
        if (rd == 0.0)
            return std::nullopt; // don't fold division by zero

        return makeReal(ld / rd);
    case BinaryOp::OP_MOD: {
        if (rd == 0.0)
            return std::nullopt;

        if (both_ints)
            return makeInteger(li % ri);

        return makeReal(std::fmod(ld, rd));
    }
    case BinaryOp::OP_POW:
        return makeReal(std::pow(ld, rd));
    case BinaryOp::OP_LT:
        return makeBool(ld < rd);
    case BinaryOp::OP_GT:
        return makeBool(ld > rd);
    case BinaryOp::OP_LTE:
        return makeBool(ld <= rd);
    case BinaryOp::OP_GTE:
        return makeBool(ld >= rd);
    case BinaryOp::OP_BITAND:
        return both_ints ? makeInteger(li & ri) : std::optional<Value> { };
    case BinaryOp::OP_BITOR:
        return both_ints ? makeInteger(li | ri) : std::optional<Value> { };
    case BinaryOp::OP_BITXOR:
        return both_ints ? makeInteger(li ^ ri) : std::optional<Value> { };
    case BinaryOp::OP_LSHIFT:
        if (!both_ints || ri < 0 || ri >= 64)
            return std::nullopt;

        return makeInteger(li << ri);
    case BinaryOp::OP_RSHIFT:
        if (!both_ints || ri < 0 || ri >= 64)
            return std::nullopt;

        return makeInteger(li >> ri);
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
        if (isDouble(v))
            return makeLiteralFloat(asDouble(v));
        if (IS_INTEGER(v))
            return makeLiteralInt(asInteger(v));
        if (IS_BOOL(v))
            return makeLiteralBool(asBool(v));

        return makeLiteralNil();
    };

    if (L)
        ce->setLeft(makeLiteralFromVal(*L));
    if (R)
        ce->setRight(makeLiteralFromVal(*R));

    return _tryFoldBinary(ce);
}

static std::optional<Value> tryFoldExpr(Expr const* e)
{
    if (!e)
        return std::nullopt;

    switch (e->getKind()) {
    case Expr::Kind::LITERAL:
        return constValue(e);
    case Expr::Kind::UNARY:
        return tryFoldUnary(static_cast<UnaryExpr const*>(e));
    case Expr::Kind::BINARY:
        return tryFoldBinary(static_cast<BinaryExpr const*>(e));
    default:
        return std::nullopt;
    }
}

} // namespace mylang::runtime

#endif // OPTIM_HPP
