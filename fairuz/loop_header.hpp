// loop_header_analyzer_with_values.hpp

#pragma once

#include "array.hpp"
#include "string.hpp"

#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace fairuz {

// forward
namespace parser {

class Fa_SymbolTable;

}

namespace AST {

class Fa_Expr;
class Fa_Stmt;
class Fa_ForStmt;
class Fa_WhileStmt;

}

struct VariableValue {
    AST::Fa_Expr* expr = nullptr;
    std::optional<int64_t> constant_value = std::nullopt;
    std::optional<Fa_StringRef> var_reference = std::nullopt;
    std::string to_string() const;

    bool is_const() const { return constant_value.has_value(); }
    bool is_var_ref() const { return var_reference.has_value(); }
};

/// Enhanced loop header with variable values
struct LoopHeader {
    /// Set of variable names used in loop body from outer scope
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> external_vars;
    /// For each external variable, store its last assigned value before loop
    std::unordered_map<Fa_StringRef, VariableValue, Fa_StringRefHash, Fa_StringRefEqual> var_to_value;

    bool empty() const { return external_vars.empty(); }
    size_t size() const { return external_vars.size(); }

    /// Get the value assigned to a variable before loop
    std::optional<VariableValue> get_value(Fa_StringRef var_name) const
    {
        auto it = var_to_value.find(var_name);
        if (it != var_to_value.end())
            return it->second;
        return std::nullopt;
    }

    std::string to_string() const;
};

std::optional<int64_t> extract_int(AST::Fa_Expr const* expr);
std::optional<Fa_StringRef> extract_var(AST::Fa_Expr const* expr);

/// Tracks variable assignments within a scope
/// Used to find the last assignment before a loop
class AssignmentTracker {
public:
    /// Assignment record: variable = value
    struct Assignment {
        Fa_StringRef var_name;
        AST::Fa_Expr* value_expr = nullptr;
        int line_number = 0;
    };

private:
    /// All assignments seen in the current scope, in order
    Fa_Array<Assignment> assignments;
    /// Latest assignment for each variable
    std::unordered_map<Fa_StringRef, Assignment, Fa_StringRefHash, Fa_StringRefEqual> latest_assignment;

public:
    /// Record an assignment
    void record_assignment(Fa_StringRef var_name, AST::Fa_Expr* value, int line)
    {
        Assignment assign;
        assign.var_name = var_name;
        assign.value_expr = value;
        assign.line_number = line;

        assignments.push(assign);
        latest_assignment[var_name] = assign;
    }

    /// Get the last assignment for a variable (before the loop)
    std::optional<Assignment> get_latest(Fa_StringRef var_name) const
    {
        auto it = latest_assignment.find(var_name);
        if (it != latest_assignment.end())
            return it->second;
        return std::nullopt;
    }

    /// Get all assignments for a variable
    Fa_Array<Assignment> get_all(Fa_StringRef var_name) const
    {
        Fa_Array<Assignment> result;
        for (auto const& assign : assignments) {
            if (assign.var_name == var_name)
                result.push(assign);
        }
        return result;
    }

    void clear()
    {
        assignments.clear();
        latest_assignment.clear();
    }
};

LoopHeader build_for_header_with_values(
    AST::Fa_ForStmt const* for_stmt, parser::Fa_SymbolTable const* outer_scope, Fa_Array<AST::Fa_Stmt const*> const& preceding_stmts);
LoopHeader build_while_header_with_values(
    AST::Fa_WhileStmt const* while_stmt, parser::Fa_SymbolTable const* outer_scope, Fa_Array<AST::Fa_Stmt const*> const& preceding_stmts);

void track_assignments_in_stmts(Fa_Array<AST::Fa_Stmt const*> const& stmts, AssignmentTracker& tracker);
void track_assignments_in_stmt(AST::Fa_Stmt const* stmt, AssignmentTracker& tracker);

void collect_from_expr(
    AST::Fa_Expr const* expr, std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual>& out_vars);
void collect_from_stmt(
    AST::Fa_Stmt const* stmt, std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual>& out_vars);

} // namespace fairuz

namespace fairuz {

inline std::string VariableValue::to_string() const
{
    if (constant_value.has_value())
        return std::to_string(constant_value.value());
    if (var_reference.has_value())
        return std::string(var_reference.value().data());
    if (expr)
        return "<expr>"; // Would need full AST-to-string for this
    return "unknown";
}

inline std::string LoopHeader::to_string() const
{
    std::ostringstream oss;
    oss << "LoopHeader{\n";
    oss << "  variables: " << size() << "\n";

    for (auto const& var : external_vars) {
        oss << "  " << var.data();
        auto it = var_to_value.find(var);
        if (it != var_to_value.end())
            oss << " = " << it->second.to_string();
        else
            oss << " = <uninitialized>";
        oss << "\n";
    }

    oss << "}\n";
    return oss.str();
}

} // namespace fairuz
