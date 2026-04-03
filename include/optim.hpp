#ifndef OPTIM_HPP
#define OPTIM_HPP

#include "ast.hpp"
#include "value.hpp"

namespace fairuz::runtime {

static std::optional<Fa_Value> const_value(AST::Fa_Expr const* e)
{
    if (e == nullptr || e->get_kind() != AST::Fa_Expr::Kind::LITERAL)
        return std::nullopt;

    auto lit = static_cast<AST::Fa_LiteralExpr const*>(e);

    if (lit->is_nil())
        return NIL_VAL;
    if (lit->is_bool())
        return Fa_MAKE_BOOL(lit->get_bool());
    if (lit->is_integer())
        return Fa_MAKE_INTEGER(lit->get_int());
    if (lit->is_float())
        return Fa_MAKE_REAL(lit->get_float());

    return std::nullopt;
}

static std::optional<Fa_Value> try_fold_unary(AST::Fa_UnaryExpr const* e)
{
    std::optional<Fa_Value> cv = const_value(e->get_operand());
    if (!cv)
        return std::nullopt;

    switch (e->get_operator()) {
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

static std::optional<Fa_Value> _try_fold_binary(AST::Fa_BinaryExpr const* e)
{
    auto L = const_value(e->get_left());
    auto R = const_value(e->get_right());

    if (!L || !R)
        return std::nullopt;

    AST::Fa_BinaryOp op = e->get_operator();

    if (op == AST::Fa_BinaryOp::OP_EQ)
        return Fa_MAKE_BOOL(*L == *R);

    if (op == AST::Fa_BinaryOp::OP_NEQ)
        return Fa_MAKE_BOOL(*L != *R);

    bool both_ints = Fa_IS_INTEGER(*L) && Fa_IS_INTEGER(*R);

    f64 ld = Fa_AS_DOUBLE_ANY(*L);
    f64 rd = Fa_AS_DOUBLE_ANY(*R);

    auto li = Fa_IS_INTEGER(*L) ? Fa_AS_INTEGER(*L) : static_cast<i64>(Fa_AS_DOUBLE_ANY(*L));
    auto ri = Fa_IS_INTEGER(*R) ? Fa_AS_INTEGER(*R) : static_cast<i64>(Fa_AS_DOUBLE_ANY(*R));

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

static std::optional<Fa_Value> try_fold_binary(AST::Fa_BinaryExpr const* e)
{
    if (e == nullptr)
        return std::nullopt;

    AST::Fa_Expr* LE = e->get_left();
    AST::Fa_Expr* RE = e->get_right();

    if (!LE || !RE)
        return std::nullopt;

    if (LE->get_kind() == AST::Fa_Expr::Kind::LITERAL && RE->get_kind() == AST::Fa_Expr::Kind::LITERAL)
        return _try_fold_binary(e);

    std::optional<Fa_Value> L, R;

    if (LE->get_kind() == AST::Fa_Expr::Kind::BINARY)
        L = try_fold_binary(static_cast<AST::Fa_BinaryExpr const*>(LE));
    else if (LE->get_kind() == AST::Fa_Expr::Kind::UNARY)
        L = try_fold_unary(static_cast<AST::Fa_UnaryExpr const*>(LE));

    if (RE->get_kind() == AST::Fa_Expr::Kind::BINARY)
        R = try_fold_binary(static_cast<AST::Fa_BinaryExpr const*>(RE));
    else if (RE->get_kind() == AST::Fa_Expr::Kind::UNARY)
        R = try_fold_unary(static_cast<AST::Fa_UnaryExpr const*>(RE));

    if (!R && !L)
        return std::nullopt;

    AST::Fa_BinaryExpr* ce = e->clone();

    auto make_literal_from_val = [](Fa_Value const v) {
        if (Fa_IS_DOUBLE(v))
            return AST::Fa_makeLiteralFloat(Fa_AS_DOUBLE(v));
        if (Fa_IS_INTEGER(v))
            return AST::Fa_makeLiteralInt(Fa_AS_INTEGER(v));
        if (Fa_IS_BOOL(v))
            return AST::Fa_makeLiteralBool(Fa_AS_BOOL(v));

        return AST::Fa_makeLiteralNil();
    };

    if (L)
        ce->set_left(make_literal_from_val(*L));
    if (R)
        ce->set_right(make_literal_from_val(*R));

    return _try_fold_binary(ce);
}

static std::optional<Fa_Value> try_fold_expr(AST::Fa_Expr const* e)
{
    if (e == nullptr)
        return std::nullopt;

    switch (e->get_kind()) {
    case AST::Fa_Expr::Kind::LITERAL:
        return const_value(e);
    case AST::Fa_Expr::Kind::UNARY:
        return try_fold_unary(static_cast<AST::Fa_UnaryExpr const*>(e));
    case AST::Fa_Expr::Kind::BINARY:
        return try_fold_binary(static_cast<AST::Fa_BinaryExpr const*>(e));
    default:
        return std::nullopt;
    }
}

} // namespace fairuz::runtime

#endif // OPTIM_HPP
