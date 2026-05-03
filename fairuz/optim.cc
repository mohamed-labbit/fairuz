#include "optim.hpp"

namespace fairuz::runtime {

std::optional<Fa_Value> const_value(AST::Fa_Expr const* e)
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

std::optional<Fa_Value> try_fold_unary(AST::Fa_UnaryExpr const* e)
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

std::optional<Fa_Value> _try_fold_binary(AST::Fa_BinaryExpr const* e)
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

std::optional<Fa_Value> try_fold_binary(AST::Fa_BinaryExpr const* e)
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

    auto make_literal_from_val = [](Fa_Value const v, Fa_SourceLocation loc) {
        if (Fa_IS_DOUBLE(v))
            return AST::Fa_make_literal_float(Fa_AS_DOUBLE(v), loc);
        if (Fa_IS_INTEGER(v))
            return AST::Fa_make_literal_int(Fa_AS_INTEGER(v), loc);
        if (Fa_IS_BOOL(v))
            return AST::Fa_make_literal_bool(Fa_AS_BOOL(v), loc);

        return AST::Fa_make_literal_nil(loc);
    };

    if (L)
        ce->set_left(make_literal_from_val(*L, ce->get_location()));
    if (R)
        ce->set_right(make_literal_from_val(*R, ce->get_location()));

    return _try_fold_binary(ce);
}

std::optional<Fa_Value> try_fold_expr(AST::Fa_Expr const* e)
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

std::optional<AST::Fa_Expr const*> try_strength_reduce_binary(AST::Fa_Expr const* e)
{
    if (e == nullptr || e->get_kind() != AST::Fa_Expr::Kind::BINARY)
        return std::nullopt;

    auto binary_expr = static_cast<AST::Fa_BinaryExpr const*>(e);
    AST::Fa_Expr const* lhs = binary_expr->get_left();
    AST::Fa_Expr const* rhs = binary_expr->get_right();
    AST::Fa_BinaryOp bin_op = binary_expr->get_operator();
    AST::Fa_Expr::Kind lkind = lhs->get_kind();
    AST::Fa_Expr::Kind rkind = rhs->get_kind();

    // x * c (where c is a compile time constant)
    if (rkind == AST::Fa_Expr::Kind::LITERAL) {
        auto lit_rhs = static_cast<AST::Fa_LiteralExpr const*>(rhs);
        if (lit_rhs->is_number() && bin_op == AST::Fa_BinaryOp::OP_MUL) {
            // x * 0 = 0
            if ((lit_rhs->is_integer() && lit_rhs->get_int() == 0) || (lit_rhs->is_float() && lit_rhs->get_float() == 0.0f))
                return AST::Fa_make_literal_int(0, e->get_location());

            // x * 1 = x
            if ((lit_rhs->is_integer() && lit_rhs->get_int() == 1) || (lit_rhs->is_float() && lit_rhs->get_float() == 1.0f))
                return lhs;

            // x * 2
            if ((lit_rhs->is_integer() && lit_rhs->get_int() == 2) || (lit_rhs->is_float() && lit_rhs->get_float() == 2.0f)) {
                // if lhs is int then : x * 2 = x << 1
                if (lkind == AST::Fa_Expr::Kind::LITERAL && static_cast<AST::Fa_LiteralExpr const*>(lhs)->is_integer()) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_LSHIFT);
                    bin_clone->set_right(AST::Fa_make_literal_int(1, rhs->get_location()));
                    return bin_clone;
                }

                // else return x + x
                AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                bin_clone->set_operator(AST::Fa_BinaryOp::OP_ADD);
                bin_clone->set_right(lhs->clone());
                return bin_clone;
            }

            // if lhs is int literal
            if (lkind == AST::Fa_Expr::Kind::LITERAL) {
                auto lit_lhs = static_cast<AST::Fa_LiteralExpr const*>(lhs);

                // x * 4 = x << 2
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 4) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_LSHIFT);
                    bin_clone->set_right(AST::Fa_make_literal_int(2, rhs->get_location()));
                    return bin_clone;
                }

                // x * 8 = x << 3
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 8) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_LSHIFT);
                    bin_clone->set_right(AST::Fa_make_literal_int(3, rhs->get_location()));
                    return bin_clone;
                }

                // if x is int : x * 3 = x + (x << 1)
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 3) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_ADD);
                    bin_clone->set_right(
                        AST::Fa_make_binary(
                            lhs->clone(),
                            AST::Fa_make_literal_int(1, { }),
                            AST::Fa_BinaryOp::OP_LSHIFT,
                            rhs->get_location()));
                    return bin_clone;
                }

                // x * 3 = x + x + x (for non-integer or fallback)
                if (lit_rhs->is_integer() && lit_rhs->get_int() == 3) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_ADD);
                    bin_clone->set_right(
                        AST::Fa_make_binary(
                            lhs->clone(),
                            lhs->clone(),
                            AST::Fa_BinaryOp::OP_ADD,
                            rhs->get_location()));
                    return bin_clone;
                }

                // if x is int : x * 5 = (x << 2) + x
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 5) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_ADD);
                    bin_clone->set_right(
                        AST::Fa_make_binary(
                            lhs->clone(),
                            AST::Fa_make_literal_int(2, { }),
                            AST::Fa_BinaryOp::OP_LSHIFT,
                            rhs->get_location()));
                    return bin_clone;
                }

                // if x is int : x * 6 = (x << 1) + (x << 2)
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 6) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_left(
                        AST::Fa_make_binary(
                            lhs->clone(),
                            AST::Fa_make_literal_int(1, { }),
                            AST::Fa_BinaryOp::OP_LSHIFT,
                            lhs->get_location()));
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_ADD);
                    bin_clone->set_right(
                        AST::Fa_make_binary(
                            lhs->clone(),
                            AST::Fa_make_literal_int(2, { }),
                            AST::Fa_BinaryOp::OP_LSHIFT,
                            rhs->get_location()));
                    return bin_clone;
                }

                // FIXED: if x is int : x * 7 = (x << 3) - x
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 7) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_left(
                        AST::Fa_make_binary(
                            lhs->clone(),
                            AST::Fa_make_literal_int(3, { }),
                            AST::Fa_BinaryOp::OP_LSHIFT,
                            lhs->get_location()));
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_SUB);
                    bin_clone->set_right(lhs->clone());
                    return bin_clone;
                }

                // FIXED: if x is int : x * 9 = (x << 3) + x
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 9) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_left(
                        AST::Fa_make_binary(
                            lhs->clone(),
                            AST::Fa_make_literal_int(3, { }),
                            AST::Fa_BinaryOp::OP_LSHIFT,
                            lhs->get_location()));
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_ADD);
                    bin_clone->set_right(lhs->clone());
                    return bin_clone;
                }
            }
        }

        if (lit_rhs->is_number() && bin_op == AST::Fa_BinaryOp::OP_DIV) {
            // x / 1 = x
            if ((lit_rhs->is_integer() && lit_rhs->get_int() == 1) || (lit_rhs->is_float() && lit_rhs->get_float() == 1.0f))
                return lhs->clone();

            // x / -1 = -x
            if ((lit_rhs->is_integer() && lit_rhs->get_int() == -1) || (lit_rhs->is_float() && lit_rhs->get_float() == -1.0f))
                return AST::Fa_make_unary(lhs->clone(), AST::Fa_UnaryOp::OP_NEG, lhs->get_location());

            if (lkind == AST::Fa_Expr::Kind::LITERAL) {
                auto lit_lhs = static_cast<AST::Fa_LiteralExpr const*>(lhs);

                // x / 2 = x >> 1
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 2) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_RSHIFT);
                    bin_clone->set_right(AST::Fa_make_literal_int(1, rhs->get_location()));
                    return bin_clone;
                }

                // x / 4 = x >> 2
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 4) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_RSHIFT);
                    bin_clone->set_right(AST::Fa_make_literal_int(2, rhs->get_location()));
                    return bin_clone;
                }

                // x / 8 = x >> 3
                if (lit_lhs->is_integer() && lit_rhs->is_integer() && lit_rhs->get_int() == 8) {
                    AST::Fa_BinaryExpr* bin_clone = binary_expr->clone();
                    bin_clone->set_operator(AST::Fa_BinaryOp::OP_RSHIFT);
                    bin_clone->set_right(AST::Fa_make_literal_int(3, rhs->get_location()));
                    return bin_clone;
                }
            }
        }

        if (lit_rhs->is_integer()) {
            // &
            if (bin_op == AST::Fa_BinaryOp::OP_BITAND) {
                // x & 0 = 0
                if (lit_rhs->get_int() == 0)
                    return AST::Fa_make_literal_int(0, { });

                // x & -1 = x
                if (lit_rhs->get_int() == -1)
                    return lhs->clone();
            }

            // |
            if (bin_op == AST::Fa_BinaryOp::OP_BITOR) {
                // x | 0 = x
                if (lit_rhs->get_int() == 0)
                    return lhs->clone();

                // x | -1 = -1
                if (lit_rhs->get_int() == -1)
                    return AST::Fa_make_literal_int(-1, { });
            }

            // ^
            if (bin_op == AST::Fa_BinaryOp::OP_BITXOR) {
                // x ^ 0 = x
                if (lit_rhs->get_int() == 0)
                    return lhs->clone();

                // x ^ -1 = ~x
                if (lit_rhs->get_int() == -1)
                    return AST::Fa_make_unary(lhs->clone(), AST::Fa_UnaryOp::OP_BITNOT, binary_expr->get_location());
            }

            // x << 0 = x , x >> 0 = x
            if ((bin_op == AST::Fa_BinaryOp::OP_LSHIFT || bin_op == AST::Fa_BinaryOp::OP_RSHIFT) && lit_rhs->get_int() == 0)
                return lhs->clone();
        }
    }

    if (bin_op == AST::Fa_BinaryOp::OP_AND) {
        if (rkind == AST::Fa_Expr::Kind::LITERAL) {
            auto lit_rhs = static_cast<AST::Fa_LiteralExpr const*>(rhs);
            if (lit_rhs->is_bool())
                return lit_rhs->get_bool() ? lhs->clone()
                                           : AST::Fa_make_literal_bool(false, binary_expr->get_location());
        }

        if (lkind == AST::Fa_Expr::Kind::LITERAL) {
            auto lit_lhs = static_cast<AST::Fa_LiteralExpr const*>(lhs);
            if (lit_lhs->is_bool())
                return lit_lhs->get_bool() ? rhs->clone()
                                           : AST::Fa_make_literal_bool(false, binary_expr->get_location());
        }
    }

    if (bin_op == AST::Fa_BinaryOp::OP_OR) {
        if (rkind == AST::Fa_Expr::Kind::LITERAL) {
            auto lit_rhs = static_cast<AST::Fa_LiteralExpr const*>(rhs);
            if (lit_rhs->is_bool())
                return lit_rhs->get_bool() ? AST::Fa_make_literal_bool(true, binary_expr->get_location())
                                           : lhs->clone();
        }

        if (lkind == AST::Fa_Expr::Kind::LITERAL) {
            auto lit_lhs = static_cast<AST::Fa_LiteralExpr const*>(lhs);
            if (lit_lhs->is_bool())
                return lit_lhs->get_bool() ? AST::Fa_make_literal_bool(true, binary_expr->get_location())
                                           : rhs->clone();
        }
    }

    return std::nullopt;
}

std::optional<AST::Fa_Expr const*> try_strength_reduce_unary(AST::Fa_Expr const* e)
{
    if (e == nullptr || e->get_kind() != AST::Fa_Expr::Kind::UNARY)
        return std::nullopt;

    auto unary_expr = static_cast<AST::Fa_UnaryExpr const*>(e);
    AST::Fa_Expr const* operand = unary_expr->get_operand();
    AST::Fa_UnaryOp un_op = unary_expr->get_operator();

    // ~
    if (un_op == AST::Fa_UnaryOp::OP_BITNOT) {
        // ~~x = x != 0
        if (operand->get_kind() == AST::Fa_Expr::Kind::UNARY) {
            auto inner = static_cast<AST::Fa_UnaryExpr const*>(operand);
            if (inner->get_operator() == AST::Fa_UnaryOp::OP_BITNOT) {
                auto inner_operand = inner->get_operand();
                if (!inner_operand)
                    return std::nullopt;
                    
                return AST::Fa_make_binary(
                    inner_operand->clone(),
                    AST::Fa_make_literal_int(0, { }),
                    AST::Fa_BinaryOp::OP_NEQ,
                    unary_expr->get_location());
            }
        }

        if (operand->get_kind() == AST::Fa_Expr::Kind::BINARY) {
            auto inner = static_cast<AST::Fa_BinaryExpr const*>(operand);

            // ~(x == y) = x != y
            if (inner->get_operator() == AST::Fa_BinaryOp::OP_EQ) {
                AST::Fa_BinaryExpr* inner_clone = inner->clone();
                inner_clone->set_operator(AST::Fa_BinaryOp::OP_NEQ);
                return inner_clone;
            }

            // ~(x != y) = x == y
            if (inner->get_operator() == AST::Fa_BinaryOp::OP_NEQ) {
                AST::Fa_BinaryExpr* inner_clone = inner->clone();
                inner_clone->set_operator(AST::Fa_BinaryOp::OP_EQ);
                return inner_clone;
            }

            // ~(x < y) = x >= y
            if (inner->get_operator() == AST::Fa_BinaryOp::OP_LT) {
                AST::Fa_BinaryExpr* inner_clone = inner->clone();
                inner_clone->set_operator(AST::Fa_BinaryOp::OP_GTE);
                return inner_clone;
            }

            // ~(x > y) = x <= y
            if (inner->get_operator() == AST::Fa_BinaryOp::OP_GT) {
                AST::Fa_BinaryExpr* inner_clone = inner->clone();
                inner_clone->set_operator(AST::Fa_BinaryOp::OP_LTE);
                return inner_clone;
            }

            // ~(x <= y) = x > y
            if (inner->get_operator() == AST::Fa_BinaryOp::OP_LTE) {
                AST::Fa_BinaryExpr* inner_clone = inner->clone();
                inner_clone->set_operator(AST::Fa_BinaryOp::OP_GT);
                return inner_clone;
            }

            // ~(x >= y) = x < y
            if (inner->get_operator() == AST::Fa_BinaryOp::OP_GTE) {
                AST::Fa_BinaryExpr* inner_clone = inner->clone();
                inner_clone->set_operator(AST::Fa_BinaryOp::OP_LT);
                return inner_clone;
            }
        }
    }

    return std::nullopt;
}

std::optional<AST::Fa_Stmt const*> try_strength_reduce_loop(AST::Fa_Stmt const* s)
{
    if (s == nullptr || s->get_kind() != AST::Fa_Stmt::Kind::FOR)
        return std::nullopt;

    /// TODO: loop strength reduction
    
    return std::nullopt;
}

} // namespace fairuz::runtime
