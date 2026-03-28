#ifndef OPTIM_HPP
#define OPTIM_HPP

#include "ast.hpp"
#include "value.hpp"

namespace fairuz::runtime {

static std::optional<Fa_Value> constValue(AST::Fa_Expr const* e)
{
    if (!e || e->getKind() != AST::Fa_Expr::Kind::LITERAL)
        return std::nullopt;

    auto lit = static_cast<AST::Fa_LiteralExpr const*>(e);

    if (lit->isNil())
        return NIL_VAL;
    if (lit->isBoolean())
        return Fa_MAKE_BOOL(lit->getBool());
    if (lit->isInteger())
        return Fa_MAKE_INTEGER(lit->getInt());
    if (lit->isDecimal())
        return Fa_MAKE_REAL(lit->getFloat());

    return std::nullopt;
}

static std::optional<Fa_Value> tryFoldUnary(AST::Fa_UnaryExpr const* e)
{
    std::optional<Fa_Value> cv = constValue(e->getOperand());
    if (!cv)
        return std::nullopt;

    switch (e->getOperator()) {
    case AST::Fa_UnaryOp::OP_NEG:
        if (Fa_IS_INTEGER(*cv))
            return Fa_IS_INTEGER(*cv) ? Fa_MAKE_INTEGER(-Fa_AS_INTEGER(*cv)) : Fa_MAKE_REAL(-Fa_AS_DOUBLE(*cv));

        return std::nullopt;
    case AST::Fa_UnaryOp::OP_NOT:
        return Fa_MAKE_BOOL(!Fa_IS_TRUTHY(*cv));
    case AST::Fa_UnaryOp::OP_BITNOT:
        if (Fa_IS_INTEGER(*cv))
            return Fa_MAKE_INTEGER(~Fa_AS_INTEGER(*cv));

        return std::nullopt;
    default:
        return std::nullopt;
    }
}

static std::optional<Fa_Value> _tryFoldBinary(AST::Fa_BinaryExpr const* e)
{
    auto L = constValue(e->getLeft());
    auto R = constValue(e->getRight());

    if (!L || !R)
        return std::nullopt;

    AST::Fa_BinaryOp op = e->getOperator();

    if (op == AST::Fa_BinaryOp::OP_EQ)
        return Fa_MAKE_BOOL(*L == *R);

    if (op == AST::Fa_BinaryOp::OP_NEQ)
        return Fa_MAKE_BOOL(*L != *R);

    bool both_ints = Fa_IS_INTEGER(*L) && Fa_IS_INTEGER(*R);

    f64 ld = Fa_AS_DOUBLE(*L);
    f64 rd = Fa_AS_DOUBLE(*R);

    auto li = Fa_IS_INTEGER(*L) ? Fa_AS_INTEGER(*L) : static_cast<i64>(Fa_AS_DOUBLE(*L));
    auto ri = Fa_IS_INTEGER(*R) ? Fa_AS_INTEGER(*R) : static_cast<i64>(Fa_AS_DOUBLE(*R));

    switch (op) {
    case AST::Fa_BinaryOp::OP_ADD:
        return both_ints ? Fa_MAKE_INTEGER(li + ri) : Fa_MAKE_REAL(ld + rd);
    case AST::Fa_BinaryOp::OP_SUB:
        return both_ints ? Fa_MAKE_INTEGER(li - ri) : Fa_MAKE_REAL(ld - rd);
    case AST::Fa_BinaryOp::OP_MUL:
        return both_ints ? Fa_MAKE_INTEGER(li * ri) : Fa_MAKE_REAL(ld * rd);
    case AST::Fa_BinaryOp::OP_DIV:
        if (rd == 0.0)
            return std::nullopt;

        return Fa_MAKE_REAL(ld / rd);
    case AST::Fa_BinaryOp::OP_MOD: {
        if (rd == 0.0)
            return std::nullopt;

        if (both_ints)
            return Fa_MAKE_INTEGER(li % ri);

        return Fa_MAKE_REAL(std::fmod(ld, rd));
    }
    case AST::Fa_BinaryOp::OP_POW:
        return Fa_MAKE_REAL(std::pow(ld, rd));
    case AST::Fa_BinaryOp::OP_LT:
        return Fa_MAKE_BOOL(ld < rd);
    case AST::Fa_BinaryOp::OP_GT:
        return Fa_MAKE_BOOL(ld > rd);
    case AST::Fa_BinaryOp::OP_LTE:
        return Fa_MAKE_BOOL(ld <= rd);
    case AST::Fa_BinaryOp::OP_GTE:
        return Fa_MAKE_BOOL(ld >= rd);
    case AST::Fa_BinaryOp::OP_BITAND:
        return both_ints ? Fa_MAKE_INTEGER(li & ri) : std::optional<Fa_Value> { };
    case AST::Fa_BinaryOp::OP_BITOR:
        return both_ints ? Fa_MAKE_INTEGER(li | ri) : std::optional<Fa_Value> { };
    case AST::Fa_BinaryOp::OP_BITXOR:
        return both_ints ? Fa_MAKE_INTEGER(li ^ ri) : std::optional<Fa_Value> { };
    case AST::Fa_BinaryOp::OP_LSHIFT:
        if (!both_ints || ri < 0 || ri >= 64)
            return std::nullopt;

        return Fa_MAKE_INTEGER(li << ri);
    case AST::Fa_BinaryOp::OP_RSHIFT:
        if (!both_ints || ri < 0 || ri >= 64)
            return std::nullopt;

        return Fa_MAKE_INTEGER(li >> ri);
    default:
        return std::nullopt;
    }
}

static std::optional<Fa_Value> tryFoldBinary(AST::Fa_BinaryExpr const* e)
{
    if (!e)
        return std::nullopt;

    AST::Fa_Expr* LE = e->getLeft();
    AST::Fa_Expr* RE = e->getRight();

    if (!LE || !RE)
        return std::nullopt;

    if (LE->getKind() == AST::Fa_Expr::Kind::LITERAL && RE->getKind() == AST::Fa_Expr::Kind::LITERAL)
        return _tryFoldBinary(e);

    std::optional<Fa_Value> L, R;

    if (LE->getKind() == AST::Fa_Expr::Kind::BINARY)
        L = tryFoldBinary(static_cast<AST::Fa_BinaryExpr const*>(LE));
    else if (LE->getKind() == AST::Fa_Expr::Kind::UNARY)
        L = tryFoldUnary(static_cast<AST::Fa_UnaryExpr const*>(LE));

    if (RE->getKind() == AST::Fa_Expr::Kind::BINARY)
        R = tryFoldBinary(static_cast<AST::Fa_BinaryExpr const*>(RE));
    else if (RE->getKind() == AST::Fa_Expr::Kind::UNARY)
        R = tryFoldUnary(static_cast<AST::Fa_UnaryExpr const*>(RE));

    if (!R && !L)
        return std::nullopt;

    AST::Fa_BinaryExpr* ce = e->clone();

    auto makeLiteralFromVal = [](Fa_Value const v) {
        if (Fa_IS_DOUBLE(v))
            return AST::Fa_makeLiteralFloat(Fa_AS_DOUBLE(v));
        if (Fa_IS_INTEGER(v))
            return AST::Fa_makeLiteralInt(Fa_AS_INTEGER(v));
        if (Fa_IS_BOOL(v))
            return AST::Fa_makeLiteralBool(Fa_AS_BOOL(v));

        return AST::Fa_makeLiteralNil();
    };

    if (L)
        ce->setLeft(makeLiteralFromVal(*L));
    if (R)
        ce->setRight(makeLiteralFromVal(*R));

    return _tryFoldBinary(ce);
}

static std::optional<Fa_Value> tryFoldExpr(AST::Fa_Expr const* e)
{
    if (!e)
        return std::nullopt;

    switch (e->getKind()) {
    case AST::Fa_Expr::Kind::LITERAL:
        return constValue(e);
    case AST::Fa_Expr::Kind::UNARY:
        return tryFoldUnary(static_cast<AST::Fa_UnaryExpr const*>(e));
    case AST::Fa_Expr::Kind::BINARY:
        return tryFoldBinary(static_cast<AST::Fa_BinaryExpr const*>(e));
    default:
        return std::nullopt;
    }
}

} // namespace fairuz::runtime

#endif // OPTIM_HPP
