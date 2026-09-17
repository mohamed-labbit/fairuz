//
// foptim.cc
//

#include "foptim.hpp"
#include "fAST.hpp"
#include <optional>

namespace fairuz::runtime {

class PurityVisitor : public AST::ExprVisitor {
private:
    bool m_is_pure { true };

public:
    bool is_pure() const { return m_is_pure; }

    void visit(AST::BinaryExpr& v) override
    {
        v.get_left()->accept(*this);
        if (!m_is_pure)
            return;
        v.get_right()->accept(*this);
    }
    void visit(AST::UnaryExpr& e) override { e.get_operand()->accept(*this); }
    void visit(AST::LiteralExpr&) override { /* pure - no op */ }
    void visit(AST::NameExpr&) override { /* pure - no op */ }
    void visit(AST::CallExpr&) override { m_is_pure = false; }
    void visit(AST::AssignmentExpr&) override { m_is_pure = false; }
    void visit(AST::ListExpr& e) override
    {
        for (auto expr : e.get_elements()) {
            expr->accept(*this);
            if (!m_is_pure)
                return;
        }
    }
    void visit(AST::IndexExpr& e) override
    {
        e.get_index()->accept(*this);
        if (!m_is_pure)
            return;
        e.get_object()->accept(*this);
    }
    void visit(AST::DictExpr& e) override
    {
        for (auto [k, v] : e.get_content()) {
            k->accept(*this);
            if (!m_is_pure)
                return;
            v->accept(*this);
        }
    }
    void visit(AST::GetExpr& e) override
    {
        e.get_member()->accept(*this); // for methods are in members here
        if (!m_is_pure)
            return;
        e.get_object()->accept(*this);
    }
};

bool is_pure(AST::Expr* e)
{
    PurityVisitor visitor;
    e->accept(visitor);
    return visitor.is_pure();
}

std::optional<Value> const_value(AST::Expr const* e)
{
    if (e == nullptr || e->get_kind() != AST::Expr::Kind::LITERAL)
        return std::nullopt;

    auto lit = as_literal(e);

    if (lit->is_nil())
        return Value::nil();
    if (lit->is_bool())
        return Value::from_bool(lit->get_bool());
    if (lit->is_integer())
        return Value::from_int(lit->get_int());
    if (lit->is_float())
        return Value::from_real(lit->get_float());

    return std::nullopt;
}

std::optional<Value> try_fold_unary(AST::UnaryExpr const* e)
{
    std::optional<Value> cv = const_value(e->get_operand());
    if (!cv)
        return std::nullopt;

    switch (e->get_operator()) {
    case AST::UnaryOp::OP_NEG:
        if (cv->is_int() && cv->as_int() != Value::int_min())
            return Value::from_int(-cv->as_int());
        if (cv->is_double())
            return Value::from_real(-cv->as_double());
        return std::nullopt;
    case AST::UnaryOp::OP_NOT:
        return Value::from_bool(!cv->is_truthy());
    case AST::UnaryOp::OP_BITNOT:
        if (cv->is_int())
            return Value::from_int(~cv->as_int());
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

std::optional<Value> _try_fold_binary(AST::BinaryExpr const* e)
{
    auto L = const_value(e->get_left());
    auto R = const_value(e->get_right());

    if (!L || !R)
        return std::nullopt;

    AST::BinaryOp op = e->get_operator();

    bool both_ints = L->is_int() && R->is_int();
    bool both_numbers = L->is_number() && R->is_number();

    if (op == AST::BinaryOp::OP_EQ || op == AST::BinaryOp::OP_NEQ) {
        bool equal;
        if (L->is_nil() || R->is_nil())
            equal = L->is_nil() && R->is_nil();
        else if (both_numbers)
            equal = L->as_double_any() == R->as_double_any();
        else
            return std::nullopt;
        return Value::from_bool(op == AST::BinaryOp::OP_EQ ? equal : !equal);
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
    case AST::BinaryOp::OP_ADD:
        return both_ints ? checked_int(li + ri)
                         : std::optional<Value> { Value::from_real(ld + rd) };
    case AST::BinaryOp::OP_SUB:
        return both_ints ? checked_int(li - ri)
                         : std::optional<Value> { Value::from_real(ld - rd) };
    case AST::BinaryOp::OP_MUL:
        return both_ints ? checked_int(static_cast<i64>(li) * ri)
                         : std::optional<Value> { Value::from_real(ld * rd) };
    case AST::BinaryOp::OP_DIV:
        if (rd == 0.0)
            return std::nullopt;
        if (both_ints && li % ri == 0)
            return Value::from_int(li / ri);
        return Value::from_real(ld / rd);
    case AST::BinaryOp::OP_MOD: {
        if (rd == 0.0)
            return std::nullopt;
        if (both_ints)
            return Value::from_real(static_cast<f64>(li % ri));
        return Value::from_real(std::fmod(ld, rd));
    }
    case AST::BinaryOp::OP_POW: return Value::from_real(std::pow(ld, rd));
    case AST::BinaryOp::OP_LT: return Value::from_bool(ld < rd);
    case AST::BinaryOp::OP_GT: return Value::from_bool(ld > rd);
    case AST::BinaryOp::OP_LTE: return Value::from_bool(ld <= rd);
    case AST::BinaryOp::OP_GTE: return Value::from_bool(ld >= rd);
    case AST::BinaryOp::OP_BITAND: return both_ints ? Value::from_int(li & ri) : std::optional<Value> { };
    case AST::BinaryOp::OP_BITOR: return both_ints ? Value::from_int(li | ri) : std::optional<Value> { };
    case AST::BinaryOp::OP_BITXOR: return both_ints ? Value::from_int(li ^ ri) : std::optional<Value> { };
    case AST::BinaryOp::OP_LSHIFT:
        if (!both_ints || ri < 0 || ri >= 64)
            return std::nullopt;
        return checked_int(static_cast<i64>(li) * (static_cast<i64>(1) << ri));
    case AST::BinaryOp::OP_RSHIFT: {
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
    if (e == nullptr)
        return std::nullopt;

    AST::Expr* LE = e->get_left();
    AST::Expr* RE = e->get_right();

    if (!LE || !RE)
        return std::nullopt;

    if (LE->get_kind() == AST::Expr::Kind::LITERAL && RE->get_kind() == AST::Expr::Kind::LITERAL)
        return _try_fold_binary(e);

    std::optional<Value> L, R;

    if (LE->get_kind() == AST::Expr::Kind::BINARY)
        L = try_fold_binary(as_binary(LE));
    else if (LE->get_kind() == AST::Expr::Kind::UNARY)
        L = try_fold_unary(as_unary(LE));

    if (RE->get_kind() == AST::Expr::Kind::BINARY)
        R = try_fold_binary(as_binary(RE));
    else if (RE->get_kind() == AST::Expr::Kind::UNARY)
        R = try_fold_unary(as_unary(RE));

    if (!R && !L)
        return std::nullopt;

    AST::BinaryExpr* ce = e->clone();

    auto make_literal_from_val = [](Value const v, SourceLocation loc) {
        if (v.is_double())
            return AST::make_literal_float(v.as_double(), loc);
        if (v.is_int())
            return AST::make_literal_int(v.as_int(), loc);
        if (v.is_bool())
            return AST::make_literal_bool(v.as_bool(), loc);

        return AST::make_literal_nil(loc);
    };

    if (L)
        ce->set_left(make_literal_from_val(*L, ce->get_location()));
    if (R)
        ce->set_right(make_literal_from_val(*R, ce->get_location()));

    return _try_fold_binary(ce);
}

class ConstFoldVisitor : public AST::ExprVisitor {
private:
    std::optional<Value> m_result { std::nullopt };

public:
    std::optional<Value> result() const { return m_result; }

    void visit(AST::BinaryExpr& e) override { m_result = try_fold_binary(&e); }
    void visit(AST::UnaryExpr& e) override { m_result = try_fold_unary(&e); }
    void visit(AST::LiteralExpr& e) override { m_result = const_value(&e); }
    void visit(AST::NameExpr&) override { /* no op */ }
    void visit(AST::CallExpr&) override { /* no op */ }
    void visit(AST::AssignmentExpr&) override { /* no op */ }
    void visit(AST::ListExpr&) override { /* no op */ }
    void visit(AST::IndexExpr& e) override { e.get_index()->accept(*this); }
    void visit(AST::DictExpr&) override { /* no op */ }
    void visit(AST::GetExpr&) override { /* no op */ }
};

std::optional<Value> try_fold_expr(AST::Expr* e)
{
    if (e == nullptr)
        return std::nullopt;

    ConstFoldVisitor visitor;
    e->accept(visitor);
    return visitor.result();
}

std::optional<AST::Expr*> try_strength_reduce_binary(AST::Expr*)
{
    // Algebraic identities are not generally semantics-preserving in a
    // dynamic language: evaluating a discarded operand may throw, and an
    // instance may overload the original operator. Keep this pass disabled
    // until type/effect information proves an individual rewrite safe.
    return std::nullopt;
}

std::optional<AST::Expr*> try_strength_reduce_unary(AST::Expr*)
{
    // Bitwise complement is not logical negation, and dynamic operands may
    // dispatch user code. No untyped unary rewrite is safe here.
    return std::nullopt;
}

} // namespace fairuz::runtime
