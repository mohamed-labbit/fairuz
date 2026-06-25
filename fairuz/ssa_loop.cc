/// SSA Loop Analysis and Optimization
/// Induction variable detection and array bounds checking

#include "ssa_loop.hpp"
#include "ast.hpp"
#include "loop_header.hpp"

#include <cmath>
#include <optional>

namespace fairuz {

namespace {

bool is_name(AST::Fa_Expr const* expr, Fa_StringRef name)
{
    return expr != nullptr
        && expr->get_kind() == AST::Fa_Expr::Kind::NAME
        && AS_CONST_NAME(expr)->get_value() == name;
}

AST::Fa_ListExpr const* list_expr_for_object(AST::Fa_Expr const* object, LoopHeader const& loop_header)
{
    if (object == nullptr)
        return nullptr;

    if (object->get_kind() == AST::Fa_Expr::Kind::LIST)
        return AS_CONST_LIST(object);

    if (object->get_kind() != AST::Fa_Expr::Kind::NAME)
        return nullptr;

    auto const* name = AS_CONST_NAME(object);
    auto header_value = loop_header.get_value(name->get_value());
    if (!header_value.has_value() || header_value->expr == nullptr)
        return nullptr;

    if (header_value->expr->get_kind() != AST::Fa_Expr::Kind::LIST)
        return nullptr;

    return AS_CONST_LIST(header_value->expr);
}

std::optional<Range> derive_range_for_var(AST::Fa_Expr const* cond, Fa_StringRef var_name)
{
    if (cond == nullptr || cond->get_kind() != AST::Fa_Expr::Kind::BINARY)
        return std::nullopt;

    auto const* bin = AS_CONST_BINARY(cond);
    AST::Fa_BinaryOp op = bin->get_operator();
    AST::Fa_Expr const* lhs = bin->get_left();
    AST::Fa_Expr const* rhs = bin->get_right();

    if (op == AST::Fa_BinaryOp::OP_AND || op == AST::Fa_BinaryOp::OP_OR) {
        auto left = derive_range_for_var(lhs, var_name);
        auto right = derive_range_for_var(rhs, var_name);
        if (!left)
            return right;
        if (!right)
            return left;
        if (op == AST::Fa_BinaryOp::OP_AND)
            return Range::intersection(*left, *right);
        return Range::range_union(*left, *right);
    }

    auto literal_range = [](i64 val, AST::Fa_BinaryOp comp_op) -> std::optional<Range> {
        switch (comp_op) {
        case AST::Fa_BinaryOp::OP_EQ: return Range(LinearBound::make_const(val), LinearBound::make_const(val));
        case AST::Fa_BinaryOp::OP_LT: return Range(LinearBound::make_const(KNegInf), LinearBound::make_const(val - 1));
        case AST::Fa_BinaryOp::OP_LTE: return Range(LinearBound::make_const(KNegInf), LinearBound::make_const(val));
        case AST::Fa_BinaryOp::OP_GT: return Range(LinearBound::make_const(val + 1), LinearBound::make_const(KInf));
        case AST::Fa_BinaryOp::OP_GTE: return Range(LinearBound::make_const(val), LinearBound::make_const(KInf));
        default: return std::nullopt;
        }
    };

    auto literal_int = [](AST::Fa_Expr const* expr) -> std::optional<i64> {
        if (expr == nullptr || expr->get_kind() != AST::Fa_Expr::Kind::LITERAL)
            return std::nullopt;
        auto const* lit = AS_CONST_LITERAL(expr);
        if (!lit->is_integer())
            return std::nullopt;
        return lit->get_int();
    };

    if (is_name(lhs, var_name)) {
        auto value = literal_int(rhs);
        if (!value)
            return std::nullopt;
        return literal_range(*value, op);
    }

    if (!is_name(rhs, var_name))
        return std::nullopt;

    auto value = literal_int(lhs);
    if (!value)
        return std::nullopt;

    switch (op) {
    case AST::Fa_BinaryOp::OP_LT: return literal_range(*value, AST::Fa_BinaryOp::OP_GT);
    case AST::Fa_BinaryOp::OP_LTE: return literal_range(*value, AST::Fa_BinaryOp::OP_GTE);
    case AST::Fa_BinaryOp::OP_GT: return literal_range(*value, AST::Fa_BinaryOp::OP_LT);
    case AST::Fa_BinaryOp::OP_GTE: return literal_range(*value, AST::Fa_BinaryOp::OP_LTE);
    default: return literal_range(*value, op);
    }
}

Range induction_range_from_condition(AST::Fa_Expr const* cond, InductionVar const& ind_var)
{
    auto condition_range = derive_range_for_var(cond, ind_var.var_name->get_value());
    if (!condition_range)
        return Range::empty();

    auto lower = condition_range->lower().get_const();
    auto upper = condition_range->upper().get_const();
    if (!lower || !upper)
        return Range::empty();

    i64 bounded_lower = *lower;
    i64 bounded_upper = *upper;

    if (ind_var.step > 0)
        bounded_lower = std::max(bounded_lower, ind_var.base);
    else if (ind_var.step < 0)
        bounded_upper = std::min(bounded_upper, ind_var.base);
    else
        return Range::empty();

    return Range(LinearBound::make_const(bounded_lower), LinearBound::make_const(bounded_upper));
}

} // namespace

/// Detect and collect induction variables in a loop body
/// Handles patterns like: i = i + c, where c is a constant
void collect_inductions(AST::Fa_Stmt const* s, Fa_Array<InductionVar>& ind_vars, LoopHeader const& loop_header, int const cond_direction)
{
    if (s == nullptr)
        return;

    /*
        if the binary expr is of the form 'a + c' or 'a - c' where 'a' is a name and 'c'
        is constant then return an int.
        0 if lhs is name and rhs is lit
        1 if lhs is lit and rhs is name
        nullopt if none
    */
    auto is_constant_increment = [](AST::Fa_BinaryExpr const* e) -> std::optional<int> {
        if (e == nullptr)
            return std::nullopt;

        AST::Fa_BinaryOp op = e->get_operator();
        AST::Fa_Expr *lhs = e->get_left(), *rhs = e->get_right();
        AST::Fa_Expr::Kind l_kind = lhs->get_kind(), r_kind = rhs->get_kind();

        if (!((l_kind == AST::Fa_Expr::Kind::LITERAL && r_kind == AST::Fa_Expr::Kind::NAME)
                || (l_kind == AST::Fa_Expr::Kind::NAME && r_kind == AST::Fa_Expr::Kind::LITERAL)))
            return std::nullopt;

        auto lit = l_kind == AST::Fa_Expr::Kind::LITERAL ? AS_CONST_LITERAL(lhs)
                                                         : AS_CONST_LITERAL(rhs);

        if (op != AST::Fa_BinaryOp::OP_ADD && op != AST::Fa_BinaryOp::OP_SUB)
            return std::nullopt;
        if (!lit->is_integer())
            return std::nullopt;

        if (l_kind == AST::Fa_Expr::Kind::NAME)
            return 0;
        return 1;
    };

    /// cond_direction = 1  -> ascending
    /// cond_direction = -1 -> descending
    /// cond_direction = 0  -> constant
    auto collect_assignment = [&](AST::Fa_Expr* target, AST::Fa_Expr* value) {
        if (target == nullptr || value == nullptr)
            return;

        // Induction variable must be a simple name
        if (target->get_kind() != AST::Fa_Expr::Kind::NAME)
            return;

        auto name = AS_CONST_NAME(target);
        auto opt_variable_at_header = loop_header.get_value(name->get_value());

        // Variable must be defined at loop entry
        if (!opt_variable_at_header.has_value())
            return;

        auto variable_at_header = opt_variable_at_header.value();

        // We only handle constant initial values
        if (!variable_at_header.is_const())
            return;

        // Avoid duplicate induction variables
        for (InductionVar const& v : ind_vars) {
            if (v.var_name->get_value() == name->get_value())
                return;
        }

        // Handle: i = i OP c (where OP is + or -)
        if (value->get_kind() == AST::Fa_Expr::Kind::BINARY) {
            auto bin = AS_CONST_BINARY(value);

            auto op_indices = is_constant_increment(bin);
            if (!op_indices)
                return;

            i64 base_val = *variable_at_header.constant_value;
            i64 step_val = 0;
            LinearBound::Op step_op = LinearBound::NONE;

            // Pattern: i = i + c
            if (bin->get_operator() == AST::Fa_BinaryOp::OP_ADD && (cond_direction == 1 || cond_direction == 0)) {
                auto lhs = bin->get_left();
                auto rhs = bin->get_right();

                auto bin_lit = *op_indices == 0 ? AS_CONST_LITERAL(rhs) : AS_CONST_LITERAL(lhs);
                auto bin_name = *op_indices == 0 ? AS_CONST_NAME(lhs) : AS_CONST_NAME(rhs);

                if (!bin_name->equals(name))
                    return;

                step_val = bin_lit->get_int();
                step_op = LinearBound::ADD;
            }
            // Pattern: i = i - c
            else if (bin->get_operator() == AST::Fa_BinaryOp::OP_SUB && (cond_direction == -1 || cond_direction == 0)) {
                auto left = bin->get_left();
                auto right = bin->get_right();

                // Only: i = i - c (not c - i)
                if (*op_indices == 0) {
                    auto left_name = AS_CONST_NAME(left)->get_value();
                    auto right_lit = AS_CONST_LITERAL(right);

                    if (left_name != name->get_value() || !right_lit->is_integer())
                        return;

                    step_val = -right_lit->get_int(); // Negative step
                    step_op = LinearBound::SUB;
                } else {
                    return;
                }
            } else {
                return;
            }

            // Step must be non-zero for valid induction
            if (step_val == 0)
                return;

            // Create induction variable with computed range
            InductionVar ind_var = {
                .var_name = name,
                .base = base_val,
                .base_expr = nullptr,
                .step = step_val,
                .range = Range::empty(),
            };

            ind_vars.push(ind_var);
        }
    };

    AST::Fa_Stmt::Kind s_kind = s->get_kind();

    if (s_kind == AST::Fa_Stmt::Kind::BLOCK) {

        auto block = AS_CONST_BLOCK(s);
        for (auto stmt : block->get_statements())
            collect_inductions(stmt, ind_vars, loop_header, cond_direction);
    } else if (AST::Fa_AssignmentExpr const* assign_expr = AST::as_assignment(s)) {
        collect_assignment(assign_expr->get_target(), assign_expr->get_value());
    }
}

static bool check_expr_for_writes(AST::Fa_Expr const* expr, AST::Fa_Expr const* obj)
{
    if (expr == nullptr || obj == nullptr)
        return false;

    using SK = AST::Fa_Stmt::Kind;
    using EK = AST::Fa_Expr::Kind;

    switch (expr->get_kind()) {

    case EK::ASSIGNMENT: {
        auto assign_expr = AS_CONST_ASSIGNMENT_EXPR(expr);
        auto [target, value] = AST::assignment_parts(assign_expr);

        if (check_expr_for_writes(target, obj))
            return true;
        if (check_expr_for_writes(value, obj))
            return true;

        if (target->equals(obj))
            return true;

        if (target->get_kind() == EK::INDEX && AS_CONST_INDEX(target)->get_object()->equals(obj))
            return true;

        return false;
    }
    case EK::BINARY: {
        auto bin_expr = AS_CONST_BINARY(expr);
        auto *lhs = bin_expr->get_left(), *rhs = bin_expr->get_right();
        if (check_expr_for_writes(lhs, obj))
            return true;
        if (check_expr_for_writes(rhs, obj))
            return true;
        return false;
    }
    case EK::CALL: {
        auto call_expr = AS_CONST_CALL(expr);
        if (call_expr->get_callee()->equals(obj))
            return true;

        return false;
    }
    default:
        return false;
    }

    return false;
}

static bool check_stmt_for_writes(AST::Fa_Stmt const* stmt, AST::Fa_Expr const* obj)
{
    if (stmt == nullptr || obj == nullptr)
        return false;

    using SK = AST::Fa_Stmt::Kind;
    using EK = AST::Fa_Expr::Kind;

    switch (stmt->get_kind()) {
    case SK::ASSIGNMENT:
        return check_expr_for_writes(AS_CONST_ASSIGNMENT_STMT(stmt)->get_expr(), obj);
    case SK::BLOCK: {
        auto block_stmt = AS_CONST_BLOCK(stmt);
        for (AST::Fa_Stmt const* s : block_stmt->get_statements()) {
            if (check_stmt_for_writes(s, obj))
                return true;
        }

        return false;
    }
    case SK::RETURN: {
        auto return_stmt = AS_CONST_RETURN(stmt);
        if (check_expr_for_writes(return_stmt->get_value(), obj))
            return true;

        return false;
    }
    case SK::EXPR: {
        auto expr_stmt = AS_CONST_EXPR_STMT(stmt);
        if (check_expr_for_writes(expr_stmt->get_expr(), obj))
            return true;

        return false;
    }
    case SK::FOR: {
        auto for_stmt = AS_CONST_FOR(stmt);
        if (check_stmt_for_writes(for_stmt->get_body(), obj))
            return true;

        return false;
    }
    case SK::IF: {
        auto if_stmt = AS_CONST_IF(stmt);
        auto cond = if_stmt->get_condition();
        auto *then_stmt = if_stmt->get_then(), *else_stmt = if_stmt->get_else();
        if (cond && check_expr_for_writes(cond, obj))
            return true;
        if (then_stmt && check_stmt_for_writes(then_stmt, obj))
            return true;
        if (else_stmt && check_stmt_for_writes(else_stmt, obj))
            return true;

        return false;
    }
    case SK::WHILE: {
        auto while_stmt = AS_CONST_WHILE(stmt);
        if (check_expr_for_writes(while_stmt->get_condition(), obj))
            return true;
        if (check_stmt_for_writes(while_stmt->get_body(), obj))
            return true;

        return false;
    }
    default:
        return false;
    }

    return false; /// unreachable
}

static bool assert_no_writes_to_object(AST::Fa_Stmt const* body, AST::Fa_Expr const* obj)
{
    return !check_stmt_for_writes(body, obj);
}

/// Recursively traverse AST and mark array accesses as safe if provably in bounds
void mark_access_safety_in_assigns(AST::Fa_ASTNode* ast_node, InductionVar const& idx_var, AST::Fa_Stmt const* loop_body, LoopHeader const& loop_header)
{
    if (ast_node == nullptr)
        return;

    AST::Fa_ASTNode::NodeType node_type = ast_node->get_node_type();

    if (node_type == AST::Fa_ASTNode::NodeType::EXPRESSION) {
        auto expr = static_cast<AST::Fa_Expr*>(ast_node);
        switch (expr->get_kind()) {
        case AST::Fa_Expr::Kind::ASSIGNMENT: {
            auto assign_expr = AS_ASSIGNMENT_EXPR(expr);
            mark_access_safety_in_assigns(assign_expr->get_target(), idx_var, loop_body, loop_header);
            mark_access_safety_in_assigns(assign_expr->get_value(), idx_var, loop_body, loop_header);
            return;
        }

        case AST::Fa_Expr::Kind::BINARY: {
            auto bin_expr = AS_BINARY(expr);
            mark_access_safety_in_assigns(bin_expr->get_left(), idx_var, loop_body, loop_header);
            mark_access_safety_in_assigns(bin_expr->get_right(), idx_var, loop_body, loop_header);
            return;
        }

        case AST::Fa_Expr::Kind::CALL: {
            auto call_expr = AS_CALL(expr);
            for (auto arg : call_expr->get_args())
                mark_access_safety_in_assigns(arg, idx_var, loop_body, loop_header);
            return;
        }

        case AST::Fa_Expr::Kind::INDEX: {
            auto index_expr = AS_INDEX(expr);
            auto idx = index_expr->get_index();
            auto obj = index_expr->get_object();

            if (idx->equals(idx_var.var_name)) {
                auto const* list_expr = list_expr_for_object(obj, loop_header);

                if (!assert_no_writes_to_object(loop_body, list_expr))
                    return;

                if (list_expr != nullptr) {
                    i64 list_size = list_expr->get_elements().size();

                    // Array bounds: [0, list_size - 1]
                    Range array_bounds(LinearBound::make_const(0), LinearBound::make_const(list_size - 1));
                    // Check if index range is fully contained in array bounds
                    Range intersection = Range::intersection(array_bounds, idx_var.range);

                    if (intersection == idx_var.range)
                        // Index provably in bounds
                        index_expr->make_safe();
                }
            }

            mark_access_safety_in_assigns(obj, idx_var, loop_body, loop_header);
            mark_access_safety_in_assigns(idx, idx_var, loop_body, loop_header);
            return;
        }

        case AST::Fa_Expr::Kind::LIST: {
            auto list_expr = AS_LIST(expr);
            for (auto elem : list_expr->get_elements())
                mark_access_safety_in_assigns(elem, idx_var, loop_body, loop_header);
            return;
        }

        case AST::Fa_Expr::Kind::UNARY: {
            auto unary_expr = AS_UNARY(expr);
            mark_access_safety_in_assigns(unary_expr->get_operand(), idx_var, loop_body, loop_header);
            return;
        }

        default:
            return;
        }
    } else if (node_type == AST::Fa_ASTNode::NodeType::STATEMENT) {
        auto stmt = static_cast<AST::Fa_Stmt*>(ast_node);
        switch (stmt->get_kind()) {
        case AST::Fa_Stmt::Kind::ASSIGNMENT: {
            auto assign_stmt = AS_ASSIGNMENT_STMT(stmt);
            mark_access_safety_in_assigns(assign_stmt->get_expr(), idx_var, loop_body, loop_header);
            return;
        }

        case AST::Fa_Stmt::Kind::BLOCK: {
            auto block_stmt = AS_BLOCK(stmt);
            for (auto s : block_stmt->get_statements())
                mark_access_safety_in_assigns(s, idx_var, loop_body, loop_header);
            return;
        }

        case AST::Fa_Stmt::Kind::EXPR: {
            auto expr_stmt = AS_EXPR_STMT(stmt);
            mark_access_safety_in_assigns(expr_stmt->get_expr(), idx_var, loop_body, loop_header);
            return;
        }

        case AST::Fa_Stmt::Kind::IF: {
            auto if_stmt = AS_IF(stmt);
            mark_access_safety_in_assigns(if_stmt->get_condition(), idx_var, loop_body, loop_header);
            mark_access_safety_in_assigns(if_stmt->get_then(), idx_var, loop_body, loop_header);
            if (if_stmt->get_else())
                mark_access_safety_in_assigns(if_stmt->get_else(), idx_var, loop_body, loop_header);
            return;
        }

        case AST::Fa_Stmt::Kind::FOR: {
            auto for_stmt = AS_FOR(stmt);
            check_access_in_bounds_in_for(for_stmt);
            return;
        }

        case AST::Fa_Stmt::Kind::WHILE: {
            auto while_stmt = AS_WHILE(stmt);
            check_access_in_bounds_in_while(while_stmt);
            return;
        }

        default:
            return;
        }
    }
}

static int get_loop_condition_direction(AST::Fa_Expr const* cond)
{
    if (cond == nullptr || cond->get_kind() != AST::Fa_Expr::Kind::BINARY)
        return 0;

    auto bin_cond = AS_CONST_BINARY(cond);
    AST::Fa_BinaryOp op = bin_cond->get_operator();
    AST::Fa_Expr *lhs = bin_cond->get_left(), *rhs = bin_cond->get_right();
    AST::Fa_Expr::Kind l_kind = lhs->get_kind(), r_kind = rhs->get_kind();

    bool is_canonical = (l_kind == AST::Fa_Expr::Kind::LITERAL && r_kind == AST::Fa_Expr::Kind::NAME)
        || (l_kind == AST::Fa_Expr::Kind::NAME && r_kind == AST::Fa_Expr::Kind::LITERAL);
    if (!is_canonical)
        return 0;

    switch (op) {
    case AST::Fa_BinaryOp::OP_LT:
    case AST::Fa_BinaryOp::OP_LTE: {
        // i < bound -> ascending (+1)
        // bound < i -> descending (-1)
        return l_kind == AST::Fa_Expr::Kind::NAME ? 1 : -1;
    }

    case AST::Fa_BinaryOp::OP_GT:
    case AST::Fa_BinaryOp::OP_GTE: {
        // i > bound -> descending (-1)
        // bound > i -> ascending (+1)
        return l_kind == AST::Fa_Expr::Kind::NAME ? -1 : 1;
    }

    default:
        return 0;
    }
}

/// Analyze for-loop with container iteration
/// for i in container(example : [1, 5, 10, 20]): arr[i]
void check_access_in_bounds_in_for(AST::Fa_ForStmt* for_loop)
{
    if (for_loop == nullptr)
        return;

    auto loop_iter = for_loop->get_iter();
    auto loop_body = for_loop->get_body();
    auto container = for_loop->get_container();

    if (!container || container->get_kind() != AST::Fa_Expr::Kind::LIST)
        return;

    auto list_expr = AS_LIST(container);
    if (list_expr->is_empty())
        return;

    // Extract bounds from container literals
    Interval container_interval = interval_from_list(list_expr->get_elements());

    // Loop iterator ranges over all values in container
    InductionVar iter_ind_var;
    iter_ind_var.var_name = AS_CONST_NAME(loop_iter);
    iter_ind_var.base = 0;
    iter_ind_var.base_expr = nullptr;
    iter_ind_var.step = 0; // Step not applicable for container iteration
    iter_ind_var.range = Range(container_interval.L, container_interval.U);

    // Check all array accesses with this induction variable
    mark_access_safety_in_assigns(loop_body, iter_ind_var, loop_body, for_loop->get_header());
}

/// Analyze while-loop with induction variables and loop condition
void check_access_in_bounds_in_while(AST::Fa_WhileStmt* while_loop)
{
    if (!while_loop)
        return;

    auto loop_body = while_loop->get_body();
    auto loop_cond = while_loop->get_condition();
    auto loop_header = while_loop->get_header();

    int cond_direction = get_loop_condition_direction(loop_cond);

    Fa_Array<InductionVar> ind_vars;
    collect_inductions(loop_body, ind_vars, loop_header, cond_direction);

    if (ind_vars.empty())
        return;

    for (auto& ind_var : ind_vars) {
        ind_var.range = induction_range_from_condition(loop_cond, ind_var);

        if (ind_var.range.is_empty())
            continue;

        // Mark all safe array accesses using this induction variable
        mark_access_safety_in_assigns(loop_body, ind_var, loop_body, loop_header);
    }
}

/// Handles: i < c, i <= c, i > c, i >= c, i == c, i != c
std::optional<Range> derive_range_from_loop_condition(AST::Fa_Expr const* cond)
{
    if (!cond || cond->get_kind() != AST::Fa_Expr::Kind::BINARY)
        return std::nullopt;

    auto bin = AS_CONST_BINARY(cond);
    AST::Fa_BinaryOp op = bin->get_operator();
    AST::Fa_Expr const *lhs = bin->get_left(), *rhs = bin->get_right();
    AST::Fa_Expr::Kind lkind = lhs->get_kind(), rkind = rhs->get_kind();

    // Handle binary logical combinations: (a < 5) || (a > 10)
    if (lkind == AST::Fa_Expr::Kind::BINARY && rkind == AST::Fa_Expr::Kind::BINARY) {
        auto left_range = derive_range_from_loop_condition(lhs);
        auto right_range = derive_range_from_loop_condition(rhs);

        if (left_range && right_range) {
            // For OR: union ranges
            if (op == AST::Fa_BinaryOp::OP_OR)
                return Range::range_union(*left_range, *right_range);
            // For AND: intersect ranges
            else if (op == AST::Fa_BinaryOp::OP_AND)
                return Range::intersection(*left_range, *right_range);
        }
    }

    // Helper: process comparison with literal
    auto range_from_comparison = [](AST::Fa_LiteralExpr const* lit, AST::Fa_Expr const* var, AST::Fa_BinaryOp comp_op)
        -> std::optional<Range> {
        if (!lit->is_integer()) {
            // Handle float literals
            if (lit->is_float()) {
                f64 val_f = lit->get_float();
                switch (comp_op) {
                case AST::Fa_BinaryOp::OP_EQ:
                    return Range(LinearBound::make_const(static_cast<i64>(std::round(val_f))),
                        LinearBound::make_const(static_cast<i64>(std::round(val_f))));
                case AST::Fa_BinaryOp::OP_NEQ: {
                    i64 val_i = static_cast<i64>(std::round(val_f));
                    Fa_Array<Interval> intervals;
                    intervals.push(Interval { LinearBound::make_const(KNegInf), LinearBound::make_const(val_i - 1) });
                    intervals.push(Interval { LinearBound::make_const(val_i + 1), LinearBound::make_const(KInf) });
                    return Range(intervals);
                }
                case AST::Fa_BinaryOp::OP_LT: return Range(LinearBound::make_const(KNegInf), LinearBound::make_const(static_cast<i64>(std::floor(val_f))));
                case AST::Fa_BinaryOp::OP_LTE: return Range(LinearBound::make_const(KNegInf), LinearBound::make_const(static_cast<i64>(std::floor(val_f))));
                case AST::Fa_BinaryOp::OP_GT: return Range(LinearBound::make_const(static_cast<i64>(std::ceil(val_f))), LinearBound::make_const(KInf));
                case AST::Fa_BinaryOp::OP_GTE: return Range(LinearBound::make_const(static_cast<i64>(std::ceil(val_f))), LinearBound::make_const(KInf));
                default:
                    return std::nullopt;
                }
            }
            return std::nullopt;
        }

        i64 val = lit->get_int();
        switch (comp_op) {
        case AST::Fa_BinaryOp::OP_EQ:
            return Range(LinearBound::make_const(val), LinearBound::make_const(val));

        case AST::Fa_BinaryOp::OP_NEQ: {
            Fa_Array<Interval> intervals;
            intervals.push(Interval { LinearBound::make_const(KNegInf), LinearBound::make_const(val - 1) });
            intervals.push(Interval { LinearBound::make_const(val + 1), LinearBound::make_const(KInf) });
            return Range(intervals);
        }

        case AST::Fa_BinaryOp::OP_LT: return Range(LinearBound::make_const(KNegInf), LinearBound::make_const(val - 1));
        case AST::Fa_BinaryOp::OP_LTE: return Range(LinearBound::make_const(KNegInf), LinearBound::make_const(val));
        case AST::Fa_BinaryOp::OP_GT: return Range(LinearBound::make_const(val + 1), LinearBound::make_const(KInf));
        case AST::Fa_BinaryOp::OP_GTE: return Range(LinearBound::make_const(val), LinearBound::make_const(KInf));

        default:
            return std::nullopt;
        }
    };

    // Extract range based on which side has the literal
    if (rkind == AST::Fa_Expr::Kind::LITERAL && lkind == AST::Fa_Expr::Kind::NAME) {
        return range_from_comparison(AS_CONST_LITERAL(rhs), lhs, op);
    } else if (lkind == AST::Fa_Expr::Kind::LITERAL && rkind == AST::Fa_Expr::Kind::NAME) {
        // Flip comparison: c < var becomes var > c
        AST::Fa_BinaryOp flipped_op = op;
        switch (op) {
        case AST::Fa_BinaryOp::OP_LT: flipped_op = AST::Fa_BinaryOp::OP_GT; break;
        case AST::Fa_BinaryOp::OP_LTE: flipped_op = AST::Fa_BinaryOp::OP_GTE; break;
        case AST::Fa_BinaryOp::OP_GT: flipped_op = AST::Fa_BinaryOp::OP_LT; break;
        case AST::Fa_BinaryOp::OP_GTE: flipped_op = AST::Fa_BinaryOp::OP_LTE; break;
        default: break;
        }

        return range_from_comparison(AS_CONST_LITERAL(lhs), rhs, flipped_op);
    }

    return std::nullopt;
}

} // namespace fairuz
