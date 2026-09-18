//
// foptim.cc
//

#include "foptim.hpp"
#include "fAST.hpp"
#include <cstdio>
#include <optional>

namespace fairuz::runtime {

class PurityVisitor : public AST::ExprVisitor {
private:
    mutable bool m_is_pure { true };

public:
    bool is_pure() const { return m_is_pure; }

    void visit(AST::BinaryExpr const& v) const override
    {
        v.lhs->accept(*const_cast<PurityVisitor*>(this));
        if (!m_is_pure)
            return;
        v.rhs->accept(*const_cast<PurityVisitor*>(this));
    }
    void visit(AST::UnaryExpr const& e) const override { e.operand->accept(*const_cast<PurityVisitor*>(this)); }
    void visit(AST::IntLiteralExpr const&) const override { /* pure - no op */ }
    void visit(AST::FloatLiteralExpr const&) const override { /* pure - no op */ }
    void visit(AST::BoolLiteralExpr const&) const override { /* pure - no op */ }
    void visit(AST::StringLiteralExpr const&) const override { /* pure - no op */ }
    void visit(AST::NilExpr const&) const override { /* pure - no op */ }
    void visit(AST::IdentifierExpr const&) const override { /* pure - no op */ }
    void visit(AST::CallExpr const&) const override { m_is_pure = false; }
    void visit(AST::AssignExpr const&) const override { m_is_pure = false; }
    void visit(AST::ListExpr const& e) const override
    {
        for (auto expr : e.elements) {
            expr->accept(*const_cast<PurityVisitor*>(this));
            if (!m_is_pure)
                return;
        }
    }
    void visit(AST::IndexExpr const& e) const override
    {
        e.index->accept(*const_cast<PurityVisitor*>(this));
        if (!m_is_pure)
            return;
        e.object->accept(*const_cast<PurityVisitor*>(this));
    }
    void visit(AST::DictExpr const& e) const override
    {
        for (auto [k, v] : e.get_content()) {
            k->accept(*const_cast<PurityVisitor*>(this));
            if (!m_is_pure)
                return;
            v->accept(*const_cast<PurityVisitor*>(this));
        }
    }
    void visit(AST::GetExpr const& e) const override
    {
        e.member->accept(*const_cast<PurityVisitor*>(this)); // for methods are in members here
        if (!m_is_pure)
            return;
        e.object->accept(*const_cast<PurityVisitor*>(this));
    }
};

bool is_pure(AST::ConstExprPtr e)
{
    PurityVisitor visitor;
    e->accept(visitor);
    return visitor.is_pure();
}

std::optional<Value> const_value(AST::ConstExprPtr e)
{
    if (AST::is_nil(e))
        return Value::nil();
    if (AST::is_literal_bool(e))
        return Value::from_bool(AST::as_literal_bool(e)->value);
    if (AST::is_literal_int(e))
        return Value::from_int(AST::as_literal_int(e)->value);
    if (AST::is_literal_float(e))
        return Value::from_real(AST::as_literal_float(e)->value);

    return std::nullopt;
}

std::optional<Value> try_fold_unary(AST::UnaryExpr const* e)
{
    std::optional<Value> cv = const_value(e->operand);
    if (!cv)
        return std::nullopt;

    switch (e->get_kind()) {
    case AST::Expr::Kind::OP_NEG:
        if (cv->is_int() && cv->as_int() != Value::int_min())
            return Value::from_int(-cv->as_int());
        if (cv->is_double())
            return Value::from_real(-cv->as_double());
        return std::nullopt;
    case AST::Expr::Kind::OP_NOT:
        return Value::from_bool(!cv->is_truthy());
    case AST::Expr::Kind::OP_BITNOT:
        if (cv->is_int())
            return Value::from_int(~cv->as_int());
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

std::optional<Value> _try_fold_binary(AST::BinaryExpr const* e)
{
    auto L = const_value(e->lhs);
    auto R = const_value(e->rhs);

    if (!L || !R)
        return std::nullopt;

    AST::ExprKind op = e->get_kind();

    bool both_ints = L->is_int() && R->is_int();
    bool both_numbers = L->is_number() && R->is_number();

    if (op == AST::ExprKind::OP_EQ || op == AST::ExprKind::OP_NEQ) {
        bool equal;
        if (L->is_nil() || R->is_nil())
            equal = L->is_nil() && R->is_nil();
        else if (both_numbers)
            equal = L->as_double_any() == R->as_double_any();
        else
            return std::nullopt;
        return Value::from_bool(op == AST::ExprKind::OP_EQ ? equal : !equal);
    }

    if (!both_numbers)
        return std::nullopt;

    f64 ld = L->as_double_any();
    f64 rd = R->as_double_any();
    i64 li = both_ints ? L->as_int() : 0;
    i64 ri = both_ints ? R->as_int() : 0;

    auto checked_int = [](auto value) -> std::optional<Value> {
        if (value < Value::int_min() || value > Value::int_max())
            return std::nullopt;
        return Value::from_int(static_cast<i64>(value));
    };

    switch (op) {
    case AST::ExprKind::OP_ADD:
        return both_ints ? checked_int(li + ri)
                         : std::optional<Value> { Value::from_real(ld + rd) };
    case AST::ExprKind::OP_SUB:
        return both_ints ? checked_int(li - ri)
                         : std::optional<Value> { Value::from_real(ld - rd) };
    case AST::ExprKind::OP_MUL:
        return both_ints ? checked_int(static_cast<i64>(li) * ri)
                         : std::optional<Value> { Value::from_real(ld * rd) };
    case AST::ExprKind::OP_DIV:
        if (rd == 0.0)
            return std::nullopt;
        if (both_ints && li % ri == 0)
            return Value::from_int(li / ri);
        return Value::from_real(ld / rd);
    case AST::ExprKind::OP_MOD: {
        if (rd == 0.0)
            return std::nullopt;
        if (both_ints)
            return Value::from_real(static_cast<f64>(li % ri));
        return Value::from_real(std::fmod(ld, rd));
    }
    case AST::ExprKind::OP_POW: return Value::from_real(std::pow(ld, rd));
    case AST::ExprKind::OP_LT: return Value::from_bool(ld < rd);
    case AST::ExprKind::OP_GT: return Value::from_bool(ld > rd);
    case AST::ExprKind::OP_LTE: return Value::from_bool(ld <= rd);
    case AST::ExprKind::OP_GTE: return Value::from_bool(ld >= rd);
    case AST::ExprKind::OP_BITAND: return both_ints ? Value::from_int(li & ri) : std::optional<Value> { };
    case AST::ExprKind::OP_BITOR: return both_ints ? Value::from_int(li | ri) : std::optional<Value> { };
    case AST::ExprKind::OP_BITXOR: return both_ints ? Value::from_int(li ^ ri) : std::optional<Value> { };
    case AST::ExprKind::OP_LSHIFT:
        if (!both_ints || ri < 0 || ri >= 64)
            return std::nullopt;
        return checked_int(static_cast<i64>(li) * (static_cast<i64>(1) << ri));
    case AST::ExprKind::OP_RSHIFT: {
        if (!both_ints || ri < 0 || ri >= 64)
            return std::nullopt;
        u64 shifted = static_cast<u64>(li) >> ri;
        if (li < 0)
            return Value::from_real(static_cast<f64>(shifted));
        return Value::from_int(static_cast<i64>(shifted));
    }
    default:
        return std::nullopt;
    }
}

std::optional<Value> try_fold_binary(AST::BinaryExpr const* e)
{
    if (auto bin_folded = _try_fold_binary(e))
        return bin_folded;

    std::optional<Value> L, R;

    if (AST::is_binary(e->lhs))
        L = try_fold_binary(as_binary(e->lhs));
    else if (AST::is_unary(e->lhs))
        L = try_fold_unary(as_unary(e->lhs));

    if (AST::is_binary(e->rhs))
        R = try_fold_binary(as_binary(e->rhs));
    else if (AST::is_unary(e->rhs))
        R = try_fold_unary(as_unary(e->rhs));

    if (!L || !R)
        return std::nullopt;

    auto make_literal_from_val = [](Value const v, SrcLoc loc) -> AST::ExprPtr {
        if (v.is_double())
            return AST::make_literal_float(v.as_double(), loc);
        if (v.is_int())
            return AST::make_literal_int(v.as_int(), loc);
        if (v.is_bool())
            return AST::make_literal_bool(v.as_bool(), loc);

        return AST::make_nil(loc);
    };

    AST::BinaryExpr* ce = AST::make_binary(e->get_kind(), make_literal_from_val(*L, e->get_location()),
        make_literal_from_val(*R, e->get_location()), e->get_location());

    return _try_fold_binary(ce);
}

class ConstFoldVisitor : public AST::ExprVisitor {
private:
    mutable std::optional<Value> m_result { std::nullopt };

public:
    std::optional<Value> result() const { return m_result; }

    void visit(AST::BinaryExpr const& e) const override { m_result = try_fold_binary(&e); }
    void visit(AST::UnaryExpr const& e) const override { m_result = try_fold_unary(&e); }
    void visit(AST::IntLiteralExpr const& e) const override { m_result = const_value(&e); }
    void visit(AST::FloatLiteralExpr const& e) const override { m_result = const_value(&e); }
    void visit(AST::BoolLiteralExpr const& e) const override { m_result = const_value(&e); }
    void visit(AST::StringLiteralExpr const& e) const override { m_result = const_value(&e); }
    void visit(AST::NilExpr const& e) const override { m_result = const_value(&e); }
    void visit(AST::IdentifierExpr const&) const override { /* no op */ }
    void visit(AST::CallExpr const&) const override { /* no op */ }
    void visit(AST::AssignExpr const&) const override { /* no op */ }
    void visit(AST::ListExpr const&) const override { /* no op */ }
    void visit(AST::IndexExpr const& e) const override { e.index->accept(*const_cast<ConstFoldVisitor*>(this)); }
    void visit(AST::DictExpr const&) const override { /* no op */ }
    void visit(AST::GetExpr const&) const override { /* no op */ }
};

std::optional<Value> try_fold_expr(AST::ConstExprPtr e)
{
    if (e == nullptr)
        return std::nullopt;

    ConstFoldVisitor visitor;
    e->accept(visitor);
    return visitor.result();
}

std::optional<AST::ExprPtr> try_strength_reduce_binary(AST::ConstExprPtr)
{
    // Algebraic identities are not generally semantics-preserving in a
    // dynamic language: evaluating a discarded operand may throw, and an
    // instance may overload the original operator. Keep this pass disabled
    // until type/effect information proves an individual rewrite safe.
    return std::nullopt;
}

std::optional<AST::ExprPtr> try_strength_reduce_unary(AST::ConstExprPtr)
{
    // Bitwise complement is not logical negation, and dynamic operands may
    // dispatch user code. No untyped unary rewrite is safe here.
    return std::nullopt;
}

} // namespace fairuz::runtime
