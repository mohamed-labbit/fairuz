#ifndef SSA_LOOP_HPP
#define SSA_LOOP_HPP

#include "ast.hpp"
#include <algorithm>
#include <variant>

namespace fairuz {

#define APPLY_BOUND_EXPR_OP(__op)                                            \
    do {                                                                     \
        auto this_expr = *get_expr();                                        \
        auto other_expr = *other.get_expr();                                 \
        if (this_expr.lhs == other_expr.lhs && op == other.op) {             \
            if (this_expr.rhs.index() == 1 && other_expr.rhs.index() == 1) { \
                i64 this_rhs = std::get<i64>(this_expr.rhs);                 \
                i64 other_rhs = std::get<i64>(other_expr.rhs);               \
                LinearOperand sum;                                           \
                sum.emplace<i64>(this_rhs __op other_rhs);                   \
                return make_expr(this_expr.lhs, sum, op);                    \
            }                                                                \
        }                                                                    \
    } while (0)

static constexpr i64 KInf = std::numeric_limits<i64>::max();
static constexpr i64 KNegInf = std::numeric_limits<i64>::min();

struct LinearBound {
    enum Op {
        ADD,
        SUB,
        NONE,
    };

    Op op { NONE }; // discriminator

    using LinearOperand = std::variant<AST::Fa_NameExpr*, i64>;

    struct LinearExpr {
        LinearOperand lhs;
        LinearOperand rhs;

        bool operator==(LinearExpr const& other) const { return lhs == other.lhs && rhs == other.rhs; }
    };

    std::variant<i64, LinearExpr> data;

    static LinearBound make_const(i64 v)
    {
        LinearBound b;
        b.op = NONE;
        b.data.emplace<i64>(v);
        return b;
    }

    static LinearBound make_expr(LinearOperand l, LinearOperand r, Op op)
    {
        LinearBound b;
        b.op = op;
        b.data.emplace<LinearExpr>(LinearExpr { l, r });
        return b;
    }

    std::optional<i64> get_const() const
    {
        if (LIKELY(op == NONE))
            return std::get<i64>(data);
        return std::nullopt;
    }

    std::optional<LinearExpr> get_expr() const
    {
        if (LIKELY(op != NONE))
            return std::get<LinearExpr>(data);
        return std::nullopt;
    }

    [[nodiscard]] bool operator==(LinearBound const& other) const
    {
        if (op != other.op)
            return false;

        if (op == NONE) {
            auto this_c = get_const();
            auto other_c = other.get_const();
            return this_c && other_c && *this_c == *other_c;
        } else {
            auto this_e = get_expr();
            auto other_e = other.get_expr();
            return this_e && other_e && *this_e == *other_e;
        }
    }

    [[nodiscard]] bool operator!=(LinearBound const& other) const
    {
        return !(*this == other);
    }

    [[nodiscard]] LinearBound operator+(LinearBound const& other) const
    {
        auto this_const = get_const();
        auto other_const = other.get_const();

        if (this_const && other_const)
            return make_const(*this_const + *other_const);

        if (this_const && other.op != NONE) {
            auto other_expr = *other.get_expr();

            if (other_expr.rhs.index() == 1) { // rhs is i64
                i64 rhs_const = std::get<i64>(other_expr.rhs);
                i64 new_rhs_v = rhs_const;

                if (other.op == ADD)
                    new_rhs_v += *this_const;
                else if (other.op == SUB)
                    new_rhs_v -= *this_const;

                LinearOperand new_rhs;
                new_rhs.emplace<i64>(new_rhs_v);
                return make_expr(other_expr.lhs, new_rhs, other.op);
            }

            return other;
        }

        if (op != NONE && other_const) {
            auto this_expr = *get_expr();

            if (this_expr.rhs.index() == 1) { // rhs is i64
                i64 rhs_const = std::get<i64>(this_expr.rhs);
                i64 new_rhs_v = rhs_const;

                if (op == ADD)
                    new_rhs_v += *other_const;
                else if (op == SUB)
                    new_rhs_v -= *other_const;

                LinearOperand new_rhs;
                new_rhs.emplace<i64>(new_rhs_v);
                return make_expr(this_expr.lhs, new_rhs, op);
            }

            return *this;
        }

        if (op != NONE && other.op != NONE) {
            APPLY_BOUND_EXPR_OP(+);
            assert(false && "Cannot add incompatible linear expressions");
            return *this;
        }

        return *this;
    }

    [[nodiscard]] LinearBound operator-(LinearBound const& other) const
    {
        auto this_const = get_const();
        auto other_const = other.get_const();

        // Case 1: Both constants
        if (this_const && other_const)
            return make_const(*this_const - *other_const);

        // Case 2: this is constant, other is expression
        if (this_const && other.op != NONE) {
            auto other_expr = *other.get_expr();

            if (other_expr.rhs.index() == 1) { // rhs is i64
                i64 rhs_const = std::get<i64>(other_expr.rhs);
                i64 new_rhs_v;
                Op new_op;

                // Flip the operation and adjust rhs
                if (other.op == ADD) {
                    new_op = SUB;
                    new_rhs_v = *this_const - rhs_const;
                } else {
                    new_op = ADD;
                    new_rhs_v = *this_const + rhs_const;
                }

                LinearOperand new_rhs;
                new_rhs.emplace<i64>(new_rhs_v);
                return make_expr(other_expr.lhs, new_rhs, new_op);
            }

            return other;
        }

        if (op != NONE && other_const) {
            auto this_expr = *get_expr();

            if (this_expr.rhs.index() == 1) { // rhs is i64
                i64 rhs_const = std::get<i64>(this_expr.rhs);
                i64 new_rhs_v = rhs_const;

                if (op == ADD)
                    new_rhs_v -= *other_const;
                else if (op == SUB)
                    new_rhs_v += *other_const;

                LinearOperand new_rhs;
                new_rhs.emplace<i64>(new_rhs_v);
                return make_expr(this_expr.lhs, new_rhs, op);
            }

            return *this;
        }

        // Case 4: Both are expressions
        if (op != NONE && other.op != NONE) {
            APPLY_BOUND_EXPR_OP(-);
            assert(false && "Cannot subtract incompatible linear expressions");
            return *this;
        }

        return *this;
    }

    [[nodiscard]] LinearBound operator*(LinearBound const& other) const
    {
        auto this_const = get_const();
        auto other_const = other.get_const();

        if (this_const && other_const)
            return make_const(*this_const * *other_const);

        if (this_const && other.op != NONE) {
            auto other_expr = *other.get_expr();

            if (other_expr.rhs.index() == 1) { // rhs is i64
                i64 rhs_const = std::get<i64>(other_expr.rhs);
                i64 new_rhs_v = rhs_const * (*this_const);
                LinearOperand new_rhs;
                new_rhs.emplace<i64>(new_rhs_v);
                return make_expr(other_expr.lhs, new_rhs, other.op);
            }

            return other;
        }

        if (op != NONE && other_const) {
            auto this_expr = *get_expr();

            if (this_expr.rhs.index() == 1) { // rhs is i64
                i64 rhs_const = std::get<i64>(this_expr.rhs);
                i64 new_rhs_v = rhs_const * (*other_const);
                LinearOperand new_rhs;
                new_rhs.emplace<i64>(new_rhs_v);
                return make_expr(this_expr.lhs, new_rhs, op);
            }

            return *this;
        }

        assert(false && "Cannot multiply two linear expressions");
        return *this;
    }

    [[nodiscard]] LinearBound operator/(LinearBound const& other) const
    {
        return { }; // mamsalich
    }

    [[nodiscard]] bool operator<(LinearBound const& other) const
    {
        auto this_const = get_const();
        auto other_const = other.get_const();

        if (this_const && other_const)
            return *this_const < *other_const;

        return false; // Conservative: cannot prove
    }

    [[nodiscard]] bool operator>(LinearBound const& other) const
    {
        auto this_const = get_const();
        auto other_const = other.get_const();

        if (this_const && other_const)
            return *this_const > *other_const;

        return false;
    }

    [[nodiscard]] bool operator<=(LinearBound const& other) const
    {
        auto this_const = get_const();
        auto other_const = other.get_const();

        if (this_const && other_const)
            return *this_const <= *other_const;

        return false;
    }

    [[nodiscard]] bool operator>=(LinearBound const& other) const
    {
        auto this_const = get_const();
        auto other_const = other.get_const();

        if (this_const && other_const)
            return *this_const >= *other_const;

        return false;
    }

    static LinearBound min(LinearBound const& a, LinearBound const& b)
    {
        auto a_const = a.get_const();
        auto b_const = b.get_const();

        if (a_const && b_const)
            return make_const(std::min(*a_const, *b_const));

        if (a_const && b.op != NONE)
            return a;

        if (a.op != NONE && b_const)
            return b;

        auto a_expr = a.get_expr();
        auto b_expr = b.get_expr();

        if (a_expr && b_expr && a_expr->lhs == b_expr->lhs && a.op == b.op) {
            if (a_expr->rhs.index() == 1 && b_expr->rhs.index() == 1) {
                i64 a_rhs = std::get<i64>(a_expr->rhs);
                i64 b_rhs = std::get<i64>(b_expr->rhs);
                i64 min_rhs_v = std::min(a_rhs, b_rhs);
                LinearOperand min_rhs;
                min_rhs.emplace<i64>(min_rhs_v);
                return make_expr(a_expr->lhs, min_rhs, a.op);
            }
        }

        return a; // Conservative
    }

    static LinearBound max(LinearBound const& a, LinearBound const& b)
    {
        auto a_const = a.get_const();
        auto b_const = b.get_const();

        if (a_const && b_const)
            return make_const(std::max(*a_const, *b_const));

        if (a_const && b.op != NONE)
            return a;

        if (a.op != NONE && b_const)
            return b;

        auto a_expr = a.get_expr();
        auto b_expr = b.get_expr();

        if (a_expr && b_expr && a_expr->lhs == b_expr->lhs && a.op == b.op) {
            if (a_expr->rhs.index() == 1 && b_expr->rhs.index() == 1) {
                i64 a_rhs = std::get<i64>(a_expr->rhs);
                i64 b_rhs = std::get<i64>(b_expr->rhs);
                i64 max_rhs_v = std::max(a_rhs, b_rhs);
                LinearOperand max_rhs;
                max_rhs.emplace<i64>(max_rhs_v);
                return make_expr(a_expr->lhs, max_rhs, a.op);
            }
        }

        return a; // Conservative
    }
};

struct Interval {
    LinearBound L { LinearBound::make_const(KInf) };
    LinearBound U { LinearBound::make_const(KNegInf) };

    bool operator==(Interval const& other) const { return L == other.L && U == other.U; }
    bool operator!=(Interval const& other) const { return L != other.L || U != other.U; }

    static Interval add(Interval const lhs, Interval const rhs) { return { lhs.L + rhs.L, lhs.U + rhs.U }; }
    static Interval sub(Interval const lhs, Interval const rhs) { return { lhs.L - rhs.U, lhs.U - rhs.L }; }

    static Interval mul(Interval const lhs, Interval const rhs)
    {
        LinearBound ll = lhs.L * rhs.L;
        LinearBound lu = lhs.L * rhs.U;
        LinearBound ul = lhs.U * rhs.L;
        LinearBound uu = lhs.U * rhs.U;

        return {
            LinearBound::min(ll, LinearBound::min(lu, LinearBound::min(ul, uu))),
            LinearBound::max(ll, LinearBound::max(lu, LinearBound::max(ul, uu))),
        };
    }

    static Interval intersection(Interval const lhs, Interval const rhs) { return { LinearBound::max(lhs.L, rhs.L), LinearBound::min(lhs.U, rhs.U) }; }
    static Interval interv_union(Interval const lhs, Interval const rhs) { return { LinearBound::min(lhs.L, rhs.L), LinearBound::max(lhs.U, rhs.U) }; }

    [[nodiscard]] bool is_empty() const
    {
        auto lower = L.get_const();
        auto upper = U.get_const();

        if (lower && upper)
            return *lower > *upper;

        // Conservative: assume non-empty if symbolic
        return false;
    }

    /// Check if interval contains a concrete value
    [[nodiscard]] bool contains(i64 const v) const
    {
        auto lower = L.get_const();
        auto upper = U.get_const();

        if (lower && upper)
            return v >= *lower && v <= *upper;

        // Cannot determine with symbolic bounds
        return false;
    }
};

class Range {
private:
    Fa_Array<Interval> m_intervals;
    LinearBound m_lower { LinearBound::make_const(KInf) };
    LinearBound m_upper { LinearBound::make_const(KNegInf) };

    /// Sort intervals by lower bound
    static void sort_intervals(Fa_Array<Interval>& inters)
    {
        std::sort(inters.begin(), inters.end(), [](Interval const& a, Interval const& b) {
            auto a_lower = a.L.get_const();
            auto b_lower = b.L.get_const();

            if (a_lower && b_lower)
                return *a_lower < *b_lower;

            return false;
        });
    }

    static void try_merge_intervals(Fa_Array<Interval>& inters)
    {
        if (inters.size() <= 1)
            return;

        Fa_Array<Interval> merged;
        merged.push(inters[0]);

        for (size_t i = 1; i < inters.size(); ++i) {
            Interval& current = merged.back();
            Interval const& next = inters[i];

            auto curr_upper = current.U.get_const();
            auto next_lower = next.L.get_const();
            auto next_upper = next.U.get_const();

            if (curr_upper && next_lower && *curr_upper >= *next_lower - 1)
                current.U = LinearBound::max(current.U, next.U);
            else
                merged.push(next);
        }

        inters = merged;
    }

    void update_bounds()
    {
        if (m_intervals.empty()) {
            m_lower = LinearBound::make_const(KInf);
            m_upper = LinearBound::make_const(KNegInf);
            return;
        }

        m_lower = m_intervals[0].L;
        m_upper = m_intervals[0].U;

        for (size_t i = 1; i < m_intervals.size(); ++i) {
            m_lower = LinearBound::min(m_lower, m_intervals[i].L);
            m_upper = LinearBound::max(m_upper, m_intervals[i].U);
        }
    }

public:
    [[nodiscard]] bool operator==(Range const& other) const
    {
        return m_lower == other.m_lower && m_upper == other.m_upper && m_intervals == other.m_intervals;
    }

    [[nodiscard]] bool operator!=(Range const& other) const { return !(*this == other); }

    static constexpr Range empty() { return Range { }; }

    static Range add(Range const lhs, Range const rhs)
    {
        Fa_Array<Interval> result_intervals;

        for (auto const& l_int : lhs.m_intervals) {
            for (auto const& r_int : rhs.m_intervals)
                result_intervals.push(Interval::add(l_int, r_int));
        }

        return Range(result_intervals);
    }

    static Range sub(Range const lhs, Range const rhs)
    {
        Fa_Array<Interval> result_intervals;

        for (auto const& l_int : lhs.m_intervals) {
            for (auto const& r_int : rhs.m_intervals)
                result_intervals.push(Interval::sub(l_int, r_int));
        }

        return Range(result_intervals);
    }

    static Range mul(Range const lhs, Range const rhs)
    {
        Fa_Array<Interval> result_intervals;

        for (auto const& l_int : lhs.m_intervals) {
            for (auto const& r_int : rhs.m_intervals)
                result_intervals.push(Interval::mul(l_int, r_int));
        }

        return Range(result_intervals);
    }

    static Range intersection(Range const lhs, Range const rhs)
    {
        Fa_Array<Interval> result_intervals;

        for (auto const& l_int : lhs.m_intervals) {
            for (auto const& r_int : rhs.m_intervals) {
                auto inter = Interval::intersection(l_int, r_int);
                if (!inter.is_empty())
                    result_intervals.push(inter);
            }
        }

        return Range(result_intervals);
    }

    static Range range_union(Range const lhs, Range const rhs)
    {
        Fa_Array<Interval> result_intervals = lhs.m_intervals;
        for (auto const& interval : rhs.m_intervals)
            result_intervals.push(interval);
        return Range(result_intervals);
    }

    Range(LinearBound lower, LinearBound upper)
        : m_lower(lower)
        , m_upper(upper)
    {
        m_intervals.push({ lower, upper });
    }

    Range(std::initializer_list<Interval> lst)
        : m_intervals(lst)
    {
        sort_intervals(m_intervals);
        try_merge_intervals(m_intervals);
        update_bounds();
    }

    explicit Range(Fa_Array<Interval> intervals)
        : m_intervals(std::move(intervals))
    {
        sort_intervals(m_intervals);
        try_merge_intervals(m_intervals);
        update_bounds();
    }

    Range() = default;

    [[nodiscard]] bool is_empty() const
    {
        return m_intervals.empty() || (m_intervals.size() == 1 && m_intervals[0].is_empty());
    }

    [[nodiscard]] LinearBound lower() const { return m_lower; }
    [[nodiscard]] LinearBound upper() const { return m_upper; }
    [[nodiscard]] Fa_Array<Interval> const& intervals() const { return m_intervals; }
};

struct InductionVar {
    AST::Fa_NameExpr const* var_name { nullptr };
    i64 base { 0 };
    AST::Fa_Expr const* base_expr { nullptr };
    i64 step { 1 };
    Range range;

    [[nodiscard]] bool is_valid() const { return var_name != nullptr; }
};

struct SymbolicExpr {
    enum Type {
        CONST,
        VAR,
        ADD,
        SUB,
        MUL,
        DIV,
        MOD,
    } op;

    union Data {
        i64 constant;
        InductionVar var;

        Data()
            : constant(0)
        {
        }
        ~Data() { }
    } data;

    SymbolicExpr const* left { nullptr };
    SymbolicExpr const* right { nullptr };

    SymbolicExpr()
        : op(CONST)
    {
    }

    SymbolicExpr(SymbolicExpr const& other)
        : op(other.op)
        , left(other.left)
        , right(other.right)
    {
        if (op == CONST)
            data.constant = other.data.constant;
        else
            data.var = other.data.var;

        // Shallow copy is fine since we don't own the child pointers
    }

    SymbolicExpr& operator=(SymbolicExpr const& other)
    {
        if (this != &other) {
            op = other.op;
            if (op == CONST)
                data.constant = other.data.constant;
            else
                data.var = other.data.var;
            left = other.left;
            right = other.right;
        }
        return *this;
    }

    /// Move assignment
    SymbolicExpr& operator=(SymbolicExpr&& other) noexcept
    {
        if (this != &other) {
            op = other.op;
            if (op == CONST)
                data.constant = other.data.constant;
            else
                data.var = other.data.var;
            left = other.left;
            right = other.right;
            other.left = nullptr;
            other.right = nullptr;
        }
        return *this;
    }

    ~SymbolicExpr()
    {
        /// NOTE: Does not recursively delete child nodes (arena takes care of them)
    }

    static SymbolicExpr make_const(i64 v)
    {
        SymbolicExpr e;
        e.op = CONST;
        e.data.constant = v;
        e.left = nullptr;
        e.right = nullptr;
        return e;
    }

    static SymbolicExpr make_var(InductionVar const& var)
    {
        SymbolicExpr e;
        e.op = VAR;
        e.data.var = var;
        e.left = nullptr;
        e.right = nullptr;
        return e;
    }

    static SymbolicExpr* make_binary(Type op_type, SymbolicExpr const* l, SymbolicExpr const* r)
    {
        auto* e = new SymbolicExpr();
        e->op = op_type;
        e->data.constant = 0;
        e->left = l;
        e->right = r;
        return e;
    }

    [[nodiscard]] bool is_const() const { return op == CONST; }
    [[nodiscard]] bool is_var() const { return op == VAR; }
    [[nodiscard]] bool is_expr() const { return !is_const() && !is_var(); }
};

static Range range_from_symbolic(SymbolicExpr const& E)
{
    switch (E.op) {
    case SymbolicExpr::Type::CONST:
        return Range(LinearBound::make_const(E.data.constant), LinearBound::make_const(E.data.constant));

    case SymbolicExpr::Type::VAR:
        return E.data.var.range;

    case SymbolicExpr::Type::ADD: {
        if (!E.left || !E.right)
            return Range::empty();

        Range lhs_range = range_from_symbolic(*E.left);
        Range rhs_range = range_from_symbolic(*E.right);
        return Range::add(lhs_range, rhs_range);
    }

    case SymbolicExpr::Type::SUB: {
        if (!E.left || !E.right)
            return Range::empty();

        Range lhs_range = range_from_symbolic(*E.left);
        Range rhs_range = range_from_symbolic(*E.right);
        return Range::sub(lhs_range, rhs_range);
    }

    case SymbolicExpr::Type::MUL: {
        if (!E.left || !E.right)
            return Range::empty();

        Range lhs_range = range_from_symbolic(*E.left);
        Range rhs_range = range_from_symbolic(*E.right);
        return Range::mul(lhs_range, rhs_range);
    }

    case SymbolicExpr::Type::DIV: {
        if (!E.left || !E.right)
            return Range::empty();

        Range lhs_range = range_from_symbolic(*E.left);
        Range rhs_range = range_from_symbolic(*E.right);

        auto divisor_lower = rhs_range.lower().get_const();
        auto divisor_upper = rhs_range.upper().get_const();

        // Division only valid if divisor is non-zero constant
        if (divisor_lower && divisor_upper && *divisor_lower == *divisor_upper && *divisor_lower != 0) {
            LinearBound divisor = LinearBound::make_const(*divisor_lower);
            return Range(lhs_range.lower() / divisor, lhs_range.upper() / divisor);
        }

        return Range::empty();
    }

    case SymbolicExpr::Type::MOD: {
        if (!E.left || !E.right)
            return Range::empty();

        Range rhs_range = range_from_symbolic(*E.right);

        auto divisor_lower = rhs_range.lower().get_const();
        auto divisor_upper = rhs_range.upper().get_const();

        // Modulo only valid if divisor is positive constant
        if (divisor_lower && divisor_upper && *divisor_lower == *divisor_upper && *divisor_lower > 0)
            return Range(LinearBound::make_const(0), LinearBound::make_const(*divisor_lower - 1));

        return Range::empty();
    }

    default:
        return Range::empty();
    }
}

static Interval interval_from_list(Fa_Array<AST::Fa_Expr*> l)
{
    i64 l_lower = KInf;
    i64 l_upper = KNegInf;

    for (auto e : l) {
        auto lit = AS_LITERAL(e)->get_int();
        l_lower = std::min<i64>(l_lower, lit);
        l_upper = std::max<i64>(l_upper, lit);
    }

    return { LinearBound::make_const(l_lower), LinearBound::make_const(l_upper) };
}

void collect_inductions(AST::Fa_Stmt const* s, Fa_Array<InductionVar>& ind_vars,
    std::unordered_map<AST::Fa_Expr const*, AST::Fa_LiteralExpr const*> loop_header);
void mark_access_safety_in_assigns(AST::Fa_ASTNode* ast_node, InductionVar const& idx_var, AST::Fa_Stmt const* loop_body, LoopHeader const& loop_header);
void check_access_in_bounds_in_for(AST::Fa_ForStmt* for_loop);
void check_access_in_bounds_in_while(AST::Fa_WhileStmt* while_loop);

} // namespace fairuz

#endif // SSA_LOOP_HPP
