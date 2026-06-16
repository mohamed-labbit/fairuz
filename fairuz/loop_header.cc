#include "loop_header.hpp"
#include "parser.hpp"

namespace fairuz {

LoopHeader build_for_header_with_values(
    AST::Fa_ForStmt const* for_stmt,
    parser::Fa_SymbolTable const* outer_scope,
    Fa_Array<AST::Fa_Stmt const*> const& preceding_stmts)
{
    LoopHeader header;

    if (for_stmt == nullptr || outer_scope == nullptr)
        return header;

    // Step 1: Collect all variables used in loop body
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> body_vars;
    collect_from_stmt(for_stmt->get_body(), body_vars);
    collect_from_expr(for_stmt->get_container(), body_vars);

    // Step 2: Get loop variable (defined BY the loop)
    Fa_StringRef loop_var = for_stmt->get_target()->get_value();

    // Step 3: Track assignments in preceding statements
    AssignmentTracker tracker;
    track_assignments_in_stmts(preceding_stmts, tracker);

    // Step 4: For each body variable, find its last assigned value
    for (auto const& var : body_vars) {
        // Skip the loop variable itself
        if (var == loop_var)
            continue;

        // Check if variable is defined in outer scope
        if (!outer_scope->is_defined(var))
            continue;

        header.external_vars.insert(var);

        // Get the last assignment to this variable
        auto last_assign = tracker.get_latest(var);
        if (last_assign.has_value()) {
            VariableValue val;
            val.expr = last_assign.value().value_expr;
            // Try to extract constant value
            val.constant_value = extract_int(last_assign.value().value_expr);
            // Try to extract variable reference
            val.var_reference = extract_var(last_assign.value().value_expr);
            header.var_to_value[var] = val;
        }
    }

    return header;
}

/// Create loop header for a while loop, including variable values
LoopHeader build_while_header_with_values(
    AST::Fa_WhileStmt const* while_stmt,
    parser::Fa_SymbolTable const* outer_scope,
    Fa_Array<AST::Fa_Stmt const*> const& preceding_stmts)
{
    LoopHeader header;

    if (!while_stmt || !outer_scope)
        return header;

    // Collect all variables used in loop
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loop_vars;
    collect_from_stmt(while_stmt->get_body(), loop_vars);
    collect_from_expr(while_stmt->get_condition(), loop_vars);

    // Track assignments in preceding statements
    AssignmentTracker tracker;
    track_assignments_in_stmts(preceding_stmts, tracker);

    // For each variable used in loop, get its value before loop
    for (auto const& var : loop_vars) {
        if (!outer_scope->is_defined(var))
            continue;

        header.external_vars.insert(var);

        auto last_assign = tracker.get_latest(var);
        if (last_assign.has_value()) {
            VariableValue val;
            val.expr = last_assign.value().value_expr;
            val.constant_value = extract_int(last_assign.value().value_expr);
            val.var_reference = extract_var(last_assign.value().value_expr);
            header.var_to_value[var] = val;
        }
    }

    return header;
}

/// Track all assignments in a list of statements
void track_assignments_in_stmts(Fa_Array<AST::Fa_Stmt const*> const& stmts, AssignmentTracker& tracker)
{
    for (auto const& stmt : stmts)
        track_assignments_in_stmt(stmt, tracker);
}

/// Recursively track assignments in a statement
void track_assignments_in_stmt(AST::Fa_Stmt const* stmt, AssignmentTracker& tracker)
{
    if (!stmt)
        return;

    switch (stmt->get_kind()) {
    case AST::Fa_Stmt::Kind::ASSIGNMENT:
    case AST::Fa_Stmt::Kind::EXPR: {
        if (AST::Fa_AssignmentExpr const* assign_expr = AST::as_assignment(stmt)) {
            AST::Fa_Expr* target = assign_expr->get_target();
            AST::Fa_Expr* value = assign_expr->get_value();

            if (target != nullptr && target->get_kind() == AST::Fa_Expr::Kind::NAME) {
                auto name_expr = AS_CONST_NAME(target);
                tracker.record_assignment(name_expr->get_value(), value, stmt->get_location().line);
            }
        }
    } break;

    case AST::Fa_Stmt::Kind::BLOCK: {
        auto block = AS_CONST_BLOCK(stmt);
        for (auto const& child : block->get_statements())
            track_assignments_in_stmt(child, tracker);
        break;
    }

    case AST::Fa_Stmt::Kind::IF: {
        auto if_stmt = AS_CONST_IF(stmt);
        // Only track assignments from the "then" branch if condition is constant true
        // Otherwise assignments are uncertain
        if (if_stmt->get_then()) {
            // Conservative: don't track assignments in conditional branches
            // (variable may not be assigned depending on condition)
        }
        break;
    }

    case AST::Fa_Stmt::Kind::WHILE:
    case AST::Fa_Stmt::Kind::FOR:
        // Don't track assignments from nested loops
        // (they modify variables inside loop, not before it)
        break;

    default:
        // Other statement types don't affect variable values
        break;
    }
}

std::optional<int64_t> extract_int(AST::Fa_Expr const* expr)
{
    if (!expr)
        return std::nullopt;

    if (expr->get_kind() != AST::Fa_Expr::Kind::LITERAL)
        return std::nullopt;

    auto lit = AS_CONST_LITERAL(expr);

    if (lit->is_integer())
        return lit->get_int();

    if (lit->is_float())
        return static_cast<int64_t>(lit->get_float());

    return std::nullopt;
}

/// Extract variable name if expr is a simple identifier
std::optional<Fa_StringRef> extract_var(AST::Fa_Expr const* expr)
{
    if (!expr)
        return std::nullopt;

    if (expr->get_kind() != AST::Fa_Expr::Kind::NAME)
        return std::nullopt;

    auto name_expr = AS_CONST_NAME(expr);
    return name_expr->get_value();
}

void collect_from_expr(
    AST::Fa_Expr const* expr,
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual>& out_vars)
{
    if (!expr)
        return;

    switch (expr->get_kind()) {
    case AST::Fa_Expr::Kind::NAME: {
        auto name_expr = AS_CONST_NAME(expr);
        out_vars.insert(name_expr->get_value());
        break;
    }

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin_expr = AS_CONST_BINARY(expr);
        collect_from_expr(bin_expr->get_left(), out_vars);
        collect_from_expr(bin_expr->get_right(), out_vars);
        break;
    }

    case AST::Fa_Expr::Kind::UNARY: {
        auto un_expr = AS_CONST_UNARY(expr);
        collect_from_expr(un_expr->get_operand(), out_vars);
        break;
    }

    case AST::Fa_Expr::Kind::CALL: {
        auto call_expr = AS_CONST_CALL(expr);
        collect_from_expr(call_expr->get_callee(), out_vars);
        for (auto const& arg : call_expr->get_args())
            collect_from_expr(arg, out_vars);
        break;
    }

    case AST::Fa_Expr::Kind::LIST: {
        auto list_expr = AS_CONST_LIST(expr);
        for (auto const& elem : list_expr->get_elements())
            collect_from_expr(elem, out_vars);
        break;
    }

    case AST::Fa_Expr::Kind::INDEX: {
        auto idx_expr = AS_CONST_INDEX(expr);
        collect_from_expr(idx_expr->get_object(), out_vars);
        collect_from_expr(idx_expr->get_index(), out_vars);
        break;
    }

    case AST::Fa_Expr::Kind::DICT: {
        auto dict_expr = AS_CONST_DICT(expr);
        for (auto const& [key, value] : dict_expr->get_content()) {
            collect_from_expr(key, out_vars);
            collect_from_expr(value, out_vars);
        }
        break;
    }

    case AST::Fa_Expr::Kind::ASSIGNMENT: {
        auto assign_expr = AS_CONST_ASSIGNMENT_EXPR(expr);
        collect_from_expr(assign_expr->get_value(), out_vars);
        if (assign_expr->get_target()->get_kind() != AST::Fa_Expr::Kind::NAME)
            collect_from_expr(assign_expr->get_target(), out_vars);
        break;
    }

    default:
        break;
    }
}

void collect_from_stmt(
    AST::Fa_Stmt const* stmt,
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual>& out_vars)
{
    if (!stmt)
        return;

    switch (stmt->get_kind()) {
    case AST::Fa_Stmt::Kind::BLOCK: {
        auto block = AS_CONST_BLOCK(stmt);
        for (auto const& child : block->get_statements())
            collect_from_stmt(child, out_vars);
        break;
    }

    case AST::Fa_Stmt::Kind::EXPR: {
        auto expr_stmt = AS_CONST_EXPR_STMT(stmt);
        collect_from_expr(expr_stmt->get_expr(), out_vars);
        break;
    }

    case AST::Fa_Stmt::Kind::ASSIGNMENT: {
        auto assign_stmt = AS_CONST_ASSIGNMENT_STMT(stmt);
        collect_from_expr(assign_stmt->get_expr(), out_vars);
        break;
    }

    case AST::Fa_Stmt::Kind::IF: {
        auto if_stmt = AS_CONST_IF(stmt);
        collect_from_expr(if_stmt->get_condition(), out_vars);
        if (if_stmt->get_then())
            collect_from_stmt(if_stmt->get_then(), out_vars);
        if (if_stmt->get_else())
            collect_from_stmt(if_stmt->get_else(), out_vars);
        break;
    }

    case AST::Fa_Stmt::Kind::WHILE: {
        auto while_stmt = AS_CONST_WHILE(stmt);
        collect_from_expr(while_stmt->get_condition(), out_vars);
        collect_from_stmt(while_stmt->get_body(), out_vars);
        break;
    }

    case AST::Fa_Stmt::Kind::FOR: {
        auto for_stmt = AS_CONST_FOR(stmt);
        collect_from_expr(for_stmt->get_container(), out_vars);
        collect_from_stmt(for_stmt->get_body(), out_vars);
        out_vars.erase(for_stmt->get_target()->get_value());
        break;
    }

    case AST::Fa_Stmt::Kind::RETURN: {
        auto ret_stmt = AS_CONST_RETURN(stmt);
        if (ret_stmt->get_value())
            collect_from_expr(ret_stmt->get_value(), out_vars);
        break;
    }

    default:
        break;
    }
}

} // fairus
