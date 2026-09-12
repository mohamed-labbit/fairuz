//
// foptim.cc
//

#include "foptim.hpp"
#include "fAST.hpp"
#include <optional>

namespace fairuz::runtime {

class Fa_PurityVisitor : public AST::Fa_ExprVisitor {
private:
    bool m_is_pure { true };

public:
    bool is_pure() const { return m_is_pure; }

    void visit(AST::Fa_BinaryExpr& v) override
    {
        v.get_left()->accept(*this);
        if (!m_is_pure)
            return;
        v.get_right()->accept(*this);
    }
    void visit(AST::Fa_UnaryExpr& e) override { e.get_operand()->accept(*this); }
    void visit(AST::Fa_LiteralExpr&) override { /* pure - no op */ }
    void visit(AST::Fa_NameExpr&) override { /* pure - no op */ }
    void visit(AST::Fa_CallExpr&) override { m_is_pure = false; }
    void visit(AST::Fa_AssignmentExpr&) override { m_is_pure = false; }
    void visit(AST::Fa_ListExpr& e) override
    {
        for (auto expr : e.get_elements()) {
            expr->accept(*this);
            if (!m_is_pure)
                return;
        }
    }
    void visit(AST::Fa_IndexExpr& e) override
    {
        e.get_index()->accept(*this);
        if (!m_is_pure)
            return;
        e.get_object()->accept(*this);
    }
    void visit(AST::Fa_DictExpr& e) override
    {
        for (auto [k, v] : e.get_content()) {
            k->accept(*this);
            if (!m_is_pure)
                return;
            v->accept(*this);
        }
    }
    void visit(AST::Fa_GetExpr& e) override
    {
        e.get_member()->accept(*this); // for methods are in members here
        if (!m_is_pure)
            return;
        e.get_object()->accept(*this);
    }
};

bool Fa_is_pure(AST::Fa_Expr* e)
{
    Fa_PurityVisitor visitor;
    e->accept(visitor);
    return visitor.is_pure();
}

std::optional<Fa_Value> const_value(AST::Fa_Expr const* e)
{
    if (e == nullptr || e->get_kind() != AST::Fa_Expr::Kind::LITERAL)
        return std::nullopt;

    auto lit = as_literal(e);

    if (lit->is_nil())
        return Fa_Value::nil();
    if (lit->is_bool())
        return Fa_Value::from_bool(lit->get_bool());
    if (lit->is_integer())
        return Fa_Value::from_int(lit->get_int());
    if (lit->is_float())
        return Fa_Value::from_real(lit->get_float());

    return std::nullopt;
}

std::optional<Fa_Value> try_fold_unary(AST::Fa_UnaryExpr const* e)
{
    std::optional<Fa_Value> cv = const_value(e->get_operand());
    if (!cv)
        return std::nullopt;

    switch (e->get_operator()) {
    case AST::Fa_UnaryOp::OP_NEG:
        if (cv->is_int() && cv->as_int() != Fa_Value::int_min())
            return Fa_Value::from_int(-cv->as_int());
        if (cv->is_double())
            return Fa_Value::from_real(-cv->as_double());
        return std::nullopt;
    case AST::Fa_UnaryOp::OP_NOT:
        return Fa_Value::from_bool(!cv->is_truthy());
    case AST::Fa_UnaryOp::OP_BITNOT:
        if (cv->is_int())
            return Fa_Value::from_int(~cv->as_int());
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

    bool both_ints = L->is_int() && R->is_int();
    bool both_numbers = L->is_number() && R->is_number();

    if (op == AST::Fa_BinaryOp::OP_EQ || op == AST::Fa_BinaryOp::OP_NEQ) {
        bool equal;
        if (L->is_nil() || R->is_nil())
            equal = L->is_nil() && R->is_nil();
        else if (both_numbers)
            equal = L->as_double_any() == R->as_double_any();
        else
            return std::nullopt;
        return Fa_Value::from_bool(op == AST::Fa_BinaryOp::OP_EQ ? equal : !equal);
    }

    if (!both_numbers)
        return std::nullopt;

    f64 ld = L->as_double_any();
    f64 rd = R->as_double_any();
    i64 li = both_ints ? L->as_int() : 0;
    i64 ri = both_ints ? R->as_int() : 0;

    auto checked_int = [](auto value) -> std::optional<Fa_Value> {
        if (value < static_cast<__int128>(Fa_Value::int_min()) || value > static_cast<__int128>(Fa_Value::int_max()))
            return std::nullopt;
        return Fa_Value::from_int(static_cast<i64>(value));
    };

    switch (op) {
    case AST::Fa_BinaryOp::OP_ADD:
        return both_ints ? checked_int(static_cast<__int128>(li) + ri)
                         : std::optional<Fa_Value> { Fa_Value::from_real(ld + rd) };
    case AST::Fa_BinaryOp::OP_SUB:
        return both_ints ? checked_int(static_cast<__int128>(li) - ri)
                         : std::optional<Fa_Value> { Fa_Value::from_real(ld - rd) };
    case AST::Fa_BinaryOp::OP_MUL:
        return both_ints ? checked_int(static_cast<__int128>(li) * ri)
                         : std::optional<Fa_Value> { Fa_Value::from_real(ld * rd) };
    case AST::Fa_BinaryOp::OP_DIV:
        if (rd == 0.0)
            return std::nullopt;
        if (both_ints && li % ri == 0)
            return Fa_Value::from_int(li / ri);
        return Fa_Value::from_real(ld / rd);
    case AST::Fa_BinaryOp::OP_MOD: {
        if (rd == 0.0)
            return std::nullopt;
        if (both_ints)
            return Fa_Value::from_real(static_cast<f64>(li % ri));
        return Fa_Value::from_real(std::fmod(ld, rd));
    }
    case AST::Fa_BinaryOp::OP_POW: return Fa_Value::from_real(std::pow(ld, rd));
    case AST::Fa_BinaryOp::OP_LT: return Fa_Value::from_bool(ld < rd);
    case AST::Fa_BinaryOp::OP_GT: return Fa_Value::from_bool(ld > rd);
    case AST::Fa_BinaryOp::OP_LTE: return Fa_Value::from_bool(ld <= rd);
    case AST::Fa_BinaryOp::OP_GTE: return Fa_Value::from_bool(ld >= rd);
    case AST::Fa_BinaryOp::OP_BITAND: return both_ints ? Fa_Value::from_int(li & ri) : std::optional<Fa_Value> { };
    case AST::Fa_BinaryOp::OP_BITOR: return both_ints ? Fa_Value::from_int(li | ri) : std::optional<Fa_Value> { };
    case AST::Fa_BinaryOp::OP_BITXOR: return both_ints ? Fa_Value::from_int(li ^ ri) : std::optional<Fa_Value> { };
    case AST::Fa_BinaryOp::OP_LSHIFT:
        if (!both_ints || ri < 0 || ri >= 64)
            return std::nullopt;
        return checked_int(static_cast<__int128>(li) * (static_cast<__int128>(1) << ri));
    case AST::Fa_BinaryOp::OP_RSHIFT: {
        if (!both_ints || ri < 0 || ri >= 64)
            return std::nullopt;
        u64 shifted = static_cast<u64>(li) >> ri;
        if (li < 0)
            return Fa_Value::from_real(static_cast<f64>(shifted));
        return Fa_Value::from_int(static_cast<i64>(shifted));
    }
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
        L = try_fold_binary(as_binary(LE));
    else if (LE->get_kind() == AST::Fa_Expr::Kind::UNARY)
        L = try_fold_unary(as_unary(LE));

    if (RE->get_kind() == AST::Fa_Expr::Kind::BINARY)
        R = try_fold_binary(as_binary(RE));
    else if (RE->get_kind() == AST::Fa_Expr::Kind::UNARY)
        R = try_fold_unary(as_unary(RE));

    if (!R && !L)
        return std::nullopt;

    AST::Fa_BinaryExpr* ce = e->clone();

    auto make_literal_from_val = [](Fa_Value const v, Fa_SourceLocation loc) {
        if (v.is_double())
            return AST::Fa_make_literal_float(v.as_double(), loc);
        if (v.is_int())
            return AST::Fa_make_literal_int(v.as_int(), loc);
        if (v.is_bool())
            return AST::Fa_make_literal_bool(v.as_bool(), loc);

        return AST::Fa_make_literal_nil(loc);
    };

    if (L)
        ce->set_left(make_literal_from_val(*L, ce->get_location()));
    if (R)
        ce->set_right(make_literal_from_val(*R, ce->get_location()));

    return _try_fold_binary(ce);
}

class Fa_ConstFoldVisitor : public AST::Fa_ExprVisitor {
private:
    std::optional<Fa_Value> m_result { std::nullopt };

public:
    std::optional<Fa_Value> result() const { return m_result; }

    void visit(AST::Fa_BinaryExpr& e) override { m_result = try_fold_binary(&e); }
    void visit(AST::Fa_UnaryExpr& e) override { m_result = try_fold_unary(&e); }
    void visit(AST::Fa_LiteralExpr& e) override { m_result = const_value(&e); }
    void visit(AST::Fa_NameExpr&) override { /* no op */ }
    void visit(AST::Fa_CallExpr&) override { /* no op */ }
    void visit(AST::Fa_AssignmentExpr&) override { /* no op */ }
    void visit(AST::Fa_ListExpr&) override { /* no op */ }
    void visit(AST::Fa_IndexExpr& e) override { e.get_index()->accept(*this); }
    void visit(AST::Fa_DictExpr&) override { /* no op */ }
    void visit(AST::Fa_GetExpr&) override { /* no op */ }
};

std::optional<Fa_Value> try_fold_expr(AST::Fa_Expr* e)
{
    if (e == nullptr)
        return std::nullopt;

    Fa_ConstFoldVisitor visitor;
    e->accept(visitor);
    return visitor.result();
}

std::optional<AST::Fa_Expr*> try_strength_reduce_binary(AST::Fa_Expr*)
{
    // Algebraic identities are not generally semantics-preserving in a
    // dynamic language: evaluating a discarded operand may throw, and an
    // instance may overload the original operator. Keep this pass disabled
    // until type/effect information proves an individual rewrite safe.
    return std::nullopt;
}

std::optional<AST::Fa_Expr*> try_strength_reduce_unary(AST::Fa_Expr*)
{
    // Bitwise complement is not logical negation, and dynamic operands may
    // dispatch user code. No untyped unary rewrite is safe here.
    return std::nullopt;
}

} // namespace fairuz::runtime
