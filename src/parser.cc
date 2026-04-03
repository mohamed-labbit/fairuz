/// parser.cc

#include "../include/parser.hpp"
#include "../include/util.hpp"
#include "array.hpp"
#include "ast.hpp"
#include "table.hpp"
#include "token.hpp"

#include <iostream>

namespace fairuz::parser {

using ErrorCode = diagnostic::errc::parser::Code;
using SemaCode = diagnostic::errc::m_sema::Code;

static bool stmt_definitely_returns(AST::Fa_Stmt const* stmt)
{
    if (stmt == nullptr)
        return false;

    switch (stmt->get_kind()) {
    case AST::Fa_Stmt::Kind::RETURN:
        return true;
    case AST::Fa_Stmt::Kind::BLOCK: {
        auto block = static_cast<AST::Fa_BlockStmt const*>(stmt);
        for (AST::Fa_Stmt const* child : block->get_statements()) {
            if (stmt_definitely_returns(child))
                return true;
        }
        return false;
    }
    case AST::Fa_Stmt::Kind::IF: {
        auto if_stmt = static_cast<AST::Fa_IfStmt const*>(stmt);
        return stmt_definitely_returns(if_stmt->get_then()) && stmt_definitely_returns(if_stmt->get_else());
    }
    default:
        return false;
    }
}

template<typename T>
static T* stamp_from_token(T* node, tok::Fa_Token const* token)
{
    if (node != nullptr && token != nullptr) {
        node->set_line(token->line());
        node->set_column(token->column());
    }
    return node;
}

template<typename T, typename U>
static T* stamp_from_node(T* node, U const* other)
{
    if (node != nullptr && other != nullptr) {
        node->set_line(other->get_line());
        node->set_column(other->get_column());
    }
    return node;
}

Fa_Error Fa_Parser::report_error(ErrorCode err_code)
{
    auto err_tok = current_token();
    Fa_SourceLocation loc = { err_tok->line(), err_tok->column(), static_cast<u16>(err_tok->lexeme().len()) };
    return fairuz::report_error(err_code, loc, &m_lexer);
}

// symbol table

Fa_SymbolTable::Fa_SymbolTable(Fa_SymbolTable* p, i32 level)
    : m_parent(p)
    , m_scope_level(level)
{
}

void Fa_SymbolTable::define(Fa_StringRef const& m_name, Symbol symbol)
{
    if (lookup_local(m_name))
        return;

    symbol.m_name = m_name;
    m_symbols[m_name] = symbol;
}

typename Fa_SymbolTable::Symbol* Fa_SymbolTable::lookup(Fa_StringRef const& m_name)
{
    if (m_symbols.contains(m_name))
        return &m_symbols[m_name];

    return m_parent ? m_parent->lookup(m_name) : nullptr;
}

typename Fa_SymbolTable::Symbol* Fa_SymbolTable::lookup_local(Fa_StringRef const& m_name)
{
    return m_symbols.find_ptr(m_name);
}

bool Fa_SymbolTable::is_defined(Fa_StringRef const& m_name) const
{
    if (m_symbols.contains(m_name))
        return true;

    return m_parent ? m_parent->is_defined(m_name) : false;
}

void Fa_SymbolTable::mark_used(Fa_StringRef const& m_name, i32 m_line)
{
    if (Symbol* sym = lookup(m_name)) {
        sym->is_used = true;
        sym->usage_lines.push(m_line);
    }
}

Fa_SymbolTable* Fa_SymbolTable::create_child()
{
    Fa_SymbolTable* child = get_allocator().allocate_object<Fa_SymbolTable>(this, m_scope_level + 1);
    m_children.push(child);
    return child;
}

Fa_Array<typename Fa_SymbolTable::Symbol*> Fa_SymbolTable::get_unused_symbols()
{
    Fa_Array<Symbol*> unused;
    for (auto [m_name, sym] : m_symbols) {
        if (!sym.is_used && sym.symbol_type == SymbolType::VARIABLE)
            unused.push(&sym);
    }

    return unused;
}

Fa_HashTable<Fa_StringRef, typename Fa_SymbolTable::Symbol, Fa_StringRefHash, Fa_StringRefEqual> const&
Fa_SymbolTable::get_symbols() const { return m_symbols; }

// semantic analyzer

typename Fa_SymbolTable::DataType Fa_SemanticAnalyzer::infer_type(AST::Fa_Expr* expr)
{
    if (expr == nullptr)
        return Fa_SymbolTable::DataType::UNKNOWN;

    switch (expr->get_kind()) {
    case AST::Fa_Expr::Kind::LITERAL: {
        auto lit = static_cast<AST::Fa_LiteralExpr const*>(expr);

        if (lit->is_string())
            return Fa_SymbolTable::DataType::STRING;
        if (lit->is_float())
            return Fa_SymbolTable::DataType::FLOAT;
        if (lit->is_integer())
            return Fa_SymbolTable::DataType::INTEGER;
        if (lit->is_bool())
            return Fa_SymbolTable::DataType::BOOLEAN;

        return Fa_SymbolTable::DataType::NONE;
    }

    case AST::Fa_Expr::Kind::NAME: {
        auto name = static_cast<AST::Fa_NameExpr const*>(expr);
        Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(name->get_value());
        if (sym != nullptr)
            return sym->data_type;
    } break;

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = static_cast<AST::Fa_BinaryExpr const*>(expr);

        Fa_SymbolTable::DataType left_type = infer_type(bin->get_left());
        Fa_SymbolTable::DataType right_type = infer_type(bin->get_right());

        // type promotion rules
        if (left_type == Fa_SymbolTable::DataType::FLOAT || right_type == Fa_SymbolTable::DataType::FLOAT)
            return Fa_SymbolTable::DataType::FLOAT;
        if (left_type == Fa_SymbolTable::DataType::INTEGER && right_type == Fa_SymbolTable::DataType::INTEGER)
            return Fa_SymbolTable::DataType::INTEGER;
        if (bin->get_operator() == AST::Fa_BinaryOp::OP_ADD && left_type == Fa_SymbolTable::DataType::STRING)
            return Fa_SymbolTable::DataType::STRING;
        if (bin->get_operator() == AST::Fa_BinaryOp::OP_AND || bin->get_operator() == AST::Fa_BinaryOp::OP_OR)
            return Fa_SymbolTable::DataType::BOOLEAN;
    } break;
    
    case AST::Fa_Expr::Kind::INDEX: {
        auto index_expr = static_cast<const AST::Fa_IndexExpr*>(expr);
        Fa_SymbolTable::DataType container_type = infer_type(index_expr->get_object());
        switch (container_type) {
        case Fa_SymbolTable::DataType::LIST:
        {
            auto container_expr = static_cast<const AST::Fa_ListExpr*>(index_expr->get_object());
            if (container_expr->is_empty())
                return Fa_SymbolTable::DataType::ANY;

            return infer_type(container_expr->get_elements()[0]);
        } 
        case Fa_SymbolTable::DataType::STRING:
            return Fa_SymbolTable::DataType::STRING;
        case Fa_SymbolTable::DataType::DICT:
        {
            auto dict_expr = static_cast<const AST::Fa_DictExpr*>(expr);
            if (dict_expr->get_content().empty())
                return Fa_SymbolTable::DataType::ANY;
            
            for (const auto& [k, v] : dict_expr->get_content()) {
                if (k->equals(index_expr))
                    return infer_type(v);
            }

            return Fa_SymbolTable::DataType::ANY;
        }
        default:
            return Fa_SymbolTable::DataType::UNKNOWN;
        }
    }

    case AST::Fa_Expr::Kind::UNARY:
        return infer_type(static_cast<const AST::Fa_UnaryExpr*>(expr)->get_operand());
    case AST::Fa_Expr::Kind::ASSIGNMENT: 
        return infer_type(static_cast<const AST::Fa_AssignmentExpr*>(expr)->get_value());
        
    // terminals
    case AST::Fa_Expr::Kind::LIST:
        return Fa_SymbolTable::DataType::LIST;
    case AST::Fa_Expr::Kind::CALL:
        return Fa_SymbolTable::DataType::ANY;
    case AST::Fa_Expr::Kind::DICT:
        return Fa_SymbolTable::DataType::DICT;
    
    default:
        return Fa_SymbolTable::DataType::UNKNOWN;
    }

    return Fa_SymbolTable::DataType::UNKNOWN; // unreachable
}

void Fa_SemanticAnalyzer::report_issue(Issue::Severity sev, SemaCode code, Fa_StringRef msg, i32 m_line, Fa_StringRef const& sugg)
{
    m_issues.push({ sev, static_cast<u16>(code), msg, m_line, sugg });
}

void Fa_SemanticAnalyzer::analyze_fa_expr(AST::Fa_Expr* expr)
{
    if (expr == nullptr)
        return;

    switch (expr->get_kind()) {
    case AST::Fa_Expr::Kind::NAME: {
        auto m_name = static_cast<AST::Fa_NameExpr const*>(expr);

        if (!m_current_scope->is_defined(m_name->get_value()))
            report_issue(Issue::Severity::ERROR, SemaCode::UNDEFINED_VARIABLE, "Undefined variable: " + m_name->get_value(), expr->get_line(), "Did you forget to initialize it?");
        else
            m_current_scope->mark_used(m_name->get_value(), expr->get_line());
    } break;

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = static_cast<AST::Fa_BinaryExpr const*>(expr);

        analyze_fa_expr(bin->get_left());
        analyze_fa_expr(bin->get_right());

        Fa_SymbolTable::DataType left_type = infer_type(bin->get_left());
        Fa_SymbolTable::DataType right_type = infer_type(bin->get_right());

        if (left_type != right_type && left_type != Fa_SymbolTable::DataType::UNKNOWN && right_type != Fa_SymbolTable::DataType::UNKNOWN
            && left_type != Fa_SymbolTable::DataType::ANY && right_type != Fa_SymbolTable::DataType::ANY)
            report_issue(Issue::Severity::ERROR, SemaCode::TYPE_MISMATCH, "Type mismatch in binary Fa_Expression", expr->get_line(), "Left and right operands must have same type");

        if (left_type == Fa_SymbolTable::DataType::STRING || right_type == Fa_SymbolTable::DataType::STRING) {
            AST::Fa_BinaryOp op = bin->get_operator();
            if (op != AST::Fa_BinaryOp::OP_ADD && op != AST::Fa_BinaryOp::OP_EQ && op != AST::Fa_BinaryOp::OP_NEQ && op != AST::Fa_BinaryOp::OP_LT 
                && op != AST::Fa_BinaryOp::OP_LTE && op != AST::Fa_BinaryOp::OP_GT && op != AST::Fa_BinaryOp::OP_GTE)
                report_issue(Issue::Severity::ERROR, SemaCode::INVALID_STRING_OP, "Invalid operation on string", expr->get_line());
        }

        if (bin->get_operator() == AST::Fa_BinaryOp::OP_DIV && bin->get_right()->get_kind() == AST::Fa_Expr::Kind::LITERAL) {
            auto lit = static_cast<AST::Fa_LiteralExpr const*>(bin->get_right());
            if (lit->is_numeric() && lit->is_number() == 0)
                report_issue(Issue::Severity::ERROR, SemaCode::DIVISION_BY_ZERO_CONST, "Division by zero", expr->get_line(), "This will cause a runtime error");
        }
    } break;

    case AST::Fa_Expr::Kind::UNARY: {
        auto un = static_cast<AST::Fa_UnaryExpr const*>(expr);
        analyze_fa_expr(un->get_operand());
    } break;

    case AST::Fa_Expr::Kind::CALL: {
        auto call = static_cast<AST::Fa_CallExpr const*>(expr);
        analyze_fa_expr(call->get_callee());

        for (AST::Fa_Expr* arg : call->get_args())
            analyze_fa_expr(arg);

        if (call->get_callee()->get_kind() == AST::Fa_Expr::Kind::NAME) {
            auto m_name = static_cast<AST::Fa_NameExpr const*>(call->get_callee());

            if (Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(m_name->get_value())) {
                if (sym->symbol_type != Fa_SymbolTable::SymbolType::FUNCTION)
                    report_issue(Issue::Severity::ERROR, SemaCode::NOT_CALLABLE, "'" + m_name->get_value() + "' is not callable", expr->get_line());
            }
        }

        if (call->get_callee()->get_kind() == AST::Fa_Expr::Kind::NAME) {
            auto m_name = static_cast<AST::Fa_NameExpr const*>(call->get_callee());
            Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(m_name->get_value());

            if (sym == nullptr)
                report_issue(Issue::Severity::ERROR, SemaCode::UNDEFINED_FUNCTION, "Undefined function: " + m_name->get_value(), expr->get_line());
            else if (sym->symbol_type != Fa_SymbolTable::SymbolType::FUNCTION)
                report_issue(Issue::Severity::ERROR, SemaCode::NOT_CALLABLE, "'" + m_name->get_value() + "' is not callable", expr->get_line());
        }
    } break;

    case AST::Fa_Expr::Kind::LIST: {
        auto list = static_cast<AST::Fa_ListExpr const*>(expr);
        for (AST::Fa_Expr* elem : list->get_elements())
            analyze_fa_expr(elem);
    } break;

    case AST::Fa_Expr::Kind::INDEX: {
        auto idx = static_cast<AST::Fa_IndexExpr const*>(expr);
        analyze_fa_expr(idx->get_object());
        analyze_fa_expr(idx->get_index());
    } break;

    case AST::Fa_Expr::Kind::DICT: {
        auto dict = static_cast<AST::Fa_DictExpr const*>(expr);
        for (auto const& [key, value] : dict->get_content()) {
            analyze_fa_expr(key);
            analyze_fa_expr(value);
        }
    } break;

    case AST::Fa_Expr::Kind::ASSIGNMENT: {
        auto assign = static_cast<AST::Fa_AssignmentExpr*>(expr);
        analyze_fa_expr(assign->get_value());

        AST::Fa_Expr* m_target = assign->get_target();
        if (m_target == nullptr)
            break;

        if (m_target->get_kind() == AST::Fa_Expr::Kind::INDEX) {
            analyze_fa_expr(m_target);
            break;
        }

        if (m_target->get_kind() == AST::Fa_Expr::Kind::NAME) {
            Fa_StringRef target_name = static_cast<AST::Fa_NameExpr*>(m_target)->get_value();
            bool const local_exists = m_current_scope->lookup_local(target_name) != nullptr;
            bool const visible_exists = m_current_scope->lookup(target_name) != nullptr;
            bool const global_exists = m_global_scope->lookup_local(target_name) != nullptr;
            bool const can_shadow_global = m_current_scope != m_global_scope && visible_exists && global_exists;

            if (!visible_exists && !global_exists)
                assign->set_decl();
    
            if (!local_exists && (!visible_exists || can_shadow_global)) {
                Fa_SymbolTable::Symbol sym;
                sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
                sym.data_type = infer_type(assign->get_value());
                sym.definition_line = expr->get_line();
                m_current_scope->define(target_name, sym);
            } else {
                m_current_scope->mark_used(target_name, expr->get_line());
            }
        }
    } break;

    default:
        break;
    }
}

void Fa_SemanticAnalyzer::analyze_stmt(AST::Fa_Stmt* stmt)
{
    if (stmt == nullptr)
        return;

    switch (stmt->get_kind()) {
    case AST::Fa_Stmt::Kind::BLOCK: {
        auto block = static_cast<AST::Fa_BlockStmt const*>(stmt);
        Fa_SymbolTable* previous = m_current_scope;
        m_current_scope = m_current_scope->create_child();

        for (AST::Fa_Stmt* child : block->get_statements())
            analyze_stmt(child);

        m_current_scope = previous;
    } break;

    case AST::Fa_Stmt::Kind::ASSIGNMENT:
        analyze_fa_expr(static_cast<AST::Fa_AssignmentStmt*>(stmt)->get_expr());
        break;

    case AST::Fa_Stmt::Kind::EXPR: {
        auto Fa_Expr_stmt = static_cast<AST::Fa_ExprStmt const*>(stmt);
        analyze_fa_expr(Fa_Expr_stmt->get_expr());
        if (Fa_Expr_stmt->get_expr()->get_kind() != AST::Fa_Expr::Kind::CALL
            && Fa_Expr_stmt->get_expr()->get_kind() != AST::Fa_Expr::Kind::ASSIGNMENT)
            report_issue(Issue::Severity::INFO, SemaCode::UNUSED_EXPR_RESULT, "Fa_Expression result not used", stmt->get_line());
    } break;

    case AST::Fa_Stmt::Kind::IF: {
        auto if_stmt = static_cast<AST::Fa_IfStmt const*>(stmt);
        analyze_fa_expr(if_stmt->get_condition());

        AST::Fa_Stmt* then_block = if_stmt->get_then();
        AST::Fa_Stmt* else_block = if_stmt->get_else();

        if (if_stmt->get_condition()->get_kind() == AST::Fa_Expr::Kind::LITERAL)
            report_issue(Issue::Severity::WARNING, SemaCode::CONSTANT_CONDITION, "Condition is always constant", stmt->get_line(), "Consider removing if statement");

        if (then_block != nullptr)
            analyze_stmt(then_block);
        if (else_block != nullptr)
            analyze_stmt(else_block);
    } break;

    case AST::Fa_Stmt::Kind::WHILE: {
        auto while_stmt = static_cast<AST::Fa_WhileStmt*>(stmt);
        analyze_fa_expr(while_stmt->get_condition());

        // Detect infinite loops
        if (while_stmt->get_condition()->get_kind() == AST::Fa_Expr::Kind::LITERAL) {
            AST::Fa_LiteralExpr const* lit = static_cast<AST::Fa_LiteralExpr const*>(while_stmt->get_condition());
            if (lit->is_bool() && lit->get_bool() == true)
                report_issue(Issue::Severity::WARNING, SemaCode::INFINITE_LOOP, "Infinite loop detected", stmt->get_line(), "Add a break condition");
        }

        analyze_stmt(while_stmt->get_body());
    } break;

    case AST::Fa_Stmt::Kind::FOR: {
        auto for_stmt = static_cast<AST::Fa_ForStmt const*>(stmt);
        analyze_fa_expr(for_stmt->get_iter());

        m_current_scope = m_current_scope->create_child();
        Fa_SymbolTable::Symbol loop_var;
        loop_var.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
        loop_var.data_type = Fa_SymbolTable::DataType::ANY;
        m_current_scope->define(for_stmt->get_target()->get_value(), loop_var);

        analyze_stmt(for_stmt->get_body());

        if (m_current_scope->m_parent && m_current_scope->m_parent->lookup_local(for_stmt->get_target()->get_value()))
            report_issue(Issue::Severity::WARNING, SemaCode::LOOP_VAR_SHADOW, "Loop variable shadows outer variable", stmt->get_line());

        m_current_scope = m_current_scope->m_parent;
    } break;

    case AST::Fa_Stmt::Kind::FUNC: {
        auto func_def = static_cast<AST::Fa_FunctionDef const*>(stmt);
        Fa_SymbolTable::Symbol func_sym;
        func_sym.symbol_type = Fa_SymbolTable::SymbolType::FUNCTION;
        func_sym.data_type = Fa_SymbolTable::DataType::FUNCTION;
        func_sym.definition_line = stmt->get_line();
        m_current_scope->define(func_def->get_name()->get_value(), func_sym);

        m_current_scope = m_current_scope->create_child();

        for (AST::Fa_Expr const* const& param : func_def->get_parameters()) {
            Fa_SymbolTable::Symbol param_sym;
            param_sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
            param_sym.data_type = Fa_SymbolTable::DataType::ANY;
            m_current_scope->define(static_cast<AST::Fa_NameExpr const*>(param)->get_value(), param_sym);
        }

        analyze_stmt(func_def->get_body());
        if (!stmt_definitely_returns(func_def->get_body()))
            report_issue(Issue::Severity::INFO, SemaCode::MISSING_RETURN, "Function may not return a value", stmt->get_line());
        // Exit function scope
        m_current_scope = m_current_scope->m_parent;
    } break;

    case AST::Fa_Stmt::Kind::RETURN: {
        auto ret = static_cast<AST::Fa_ReturnStmt*>(stmt);
        analyze_fa_expr(ret->get_value());
    } break;

    case AST::Fa_Stmt::Kind::BREAK:
    case AST::Fa_Stmt::Kind::CONTINUE:
        break;

    default:
        break;
    }
}

Fa_SemanticAnalyzer::Fa_SemanticAnalyzer()
{
    m_global_scope = get_allocator().allocate_object<Fa_SymbolTable>();
    m_current_scope = m_global_scope;

    auto register_builtin = [this](Fa_StringRef name) {
        Fa_SymbolTable::Symbol sym;
        sym.m_name = name;
        sym.symbol_type = Fa_SymbolTable::SymbolType::FUNCTION;
        sym.data_type = Fa_SymbolTable::DataType::FUNCTION;
        m_global_scope->define(name, sym);
    };

    register_builtin("print");
    register_builtin("اكتب");
    register_builtin("len");
    register_builtin("حجم");
    register_builtin("append");
    register_builtin("اضف");
    register_builtin("احذف");
    register_builtin("مقطع");
    register_builtin("input");
    register_builtin("نوع");
    register_builtin("طبيعي");
    register_builtin("حقيقي");
    register_builtin("str");
    register_builtin("bool");
    register_builtin("قائمة");
    register_builtin("جدول");
    register_builtin("dict");
    register_builtin("اقسم");
    register_builtin("join");
    register_builtin("substr");
    register_builtin("contains");
    register_builtin("trim");
    register_builtin("floor");
    register_builtin("ceil");
    register_builtin("round");
    register_builtin("abs");
    register_builtin("min");
    register_builtin("max");
    register_builtin("pow");
    register_builtin("sqrt");
    register_builtin("assert");
    register_builtin("clock");
    register_builtin("error");
    register_builtin("time");
}

void Fa_SemanticAnalyzer::analyze(Fa_Array<AST::Fa_Stmt*> const& m_statements)
{
    for (AST::Fa_Stmt* stmt : m_statements)
        analyze_stmt(stmt);

    Fa_Array<Fa_SymbolTable::Symbol*> unused = m_global_scope->get_unused_symbols();
    for (Fa_SymbolTable::Symbol* sym : unused)
        report_issue(Issue::Severity::WARNING, SemaCode::UNUSED_VARIABLE, "Unused variable: " + sym->m_name, sym->definition_line, "Consider removing if not needed");

    for (Issue const& issue : m_issues) {
        diagnostic::Severity sev = diagnostic::Severity::NOTE;
        switch (issue.severity) {
        case Issue::Severity::ERROR:
            sev = diagnostic::Severity::ERROR;
            break;
        case Issue::Severity::WARNING:
            sev = diagnostic::Severity::WARNING;
            break;
        case Issue::Severity::INFO:
            sev = diagnostic::Severity::NOTE;
            break;
        }

        diagnostic::report(sev, issue.m_line, 0, issue.code);
        if (!issue.suggestion.empty())
            diagnostic::engine.add_suggestion(std::string(issue.suggestion.data(), issue.suggestion.len()));
    }
}

Fa_Array<typename Fa_SemanticAnalyzer::Issue> const& Fa_SemanticAnalyzer::get_issues() const { return m_issues; }

Fa_SymbolTable const* Fa_SemanticAnalyzer::get_global_scope() const { return m_global_scope; }
Fa_SymbolTable const* Fa_SemanticAnalyzer::get_current_scope() const { return m_current_scope; }

void Fa_SemanticAnalyzer::print_report() const
{
    if (m_issues.empty()) {
        std::cout << "✓ No issues found\n";
        return;
    }

    std::cout << "Found " << m_issues.size() << " issue(s):\n\n";
    for (Fa_SemanticAnalyzer::Issue const& issue : m_issues) {
        Fa_StringRef sev_str;
        switch (issue.severity) {
        case Issue::Severity::ERROR:
            sev_str = "ERROR";
            break;
        case Issue::Severity::WARNING:
            sev_str = "WARNING";
            break;
        case Issue::Severity::INFO:
            sev_str = "INFO";
            break;
        }

        std::cout << "[" << sev_str << "] Line " << issue.m_line << ": " << issue.message << "\n";

        if (!issue.suggestion.empty())
            std::cout << "  → " << issue.suggestion << "\n";

        std::cout << "\n";
    }
}

AST::Fa_BinaryOp to_binary_op(tok::Fa_TokenType const op)
{
    switch (op) {
    case tok::Fa_TokenType::OP_PLUS:
        return AST::Fa_BinaryOp::OP_ADD;
    case tok::Fa_TokenType::OP_MINUS:
        return AST::Fa_BinaryOp::OP_SUB;
    case tok::Fa_TokenType::OP_STAR:
        return AST::Fa_BinaryOp::OP_MUL;
    case tok::Fa_TokenType::OP_SLASH:
        return AST::Fa_BinaryOp::OP_DIV;
    case tok::Fa_TokenType::OP_PERCENT:
        return AST::Fa_BinaryOp::OP_MOD;
    case tok::Fa_TokenType::OP_POWER:
        return AST::Fa_BinaryOp::OP_POW;
    case tok::Fa_TokenType::OP_EQ:
        return AST::Fa_BinaryOp::OP_EQ;
    case tok::Fa_TokenType::OP_NEQ:
        return AST::Fa_BinaryOp::OP_NEQ;
    case tok::Fa_TokenType::OP_LT:
        return AST::Fa_BinaryOp::OP_LT;
    case tok::Fa_TokenType::OP_GT:
        return AST::Fa_BinaryOp::OP_GT;
    case tok::Fa_TokenType::OP_LTE:
        return AST::Fa_BinaryOp::OP_LTE;
    case tok::Fa_TokenType::OP_GTE:
        return AST::Fa_BinaryOp::OP_GTE;
    case tok::Fa_TokenType::OP_BITAND:
        return AST::Fa_BinaryOp::OP_BITAND;
    case tok::Fa_TokenType::OP_BITOR:
        return AST::Fa_BinaryOp::OP_BITOR;
    case tok::Fa_TokenType::OP_BITXOR:
        return AST::Fa_BinaryOp::OP_BITXOR;
    case tok::Fa_TokenType::OP_LSHIFT:
        return AST::Fa_BinaryOp::OP_LSHIFT;
    case tok::Fa_TokenType::OP_RSHIFT:
        return AST::Fa_BinaryOp::OP_RSHIFT;
    case tok::Fa_TokenType::OP_AND:
        return AST::Fa_BinaryOp::OP_AND;
    case tok::Fa_TokenType::OP_OR:
        return AST::Fa_BinaryOp::OP_OR;
    default:
        return AST::Fa_BinaryOp::INVALID;
    }
}

AST::Fa_UnaryOp to_unary_op(tok::Fa_TokenType const op)
{
    switch (op) {
    case tok::Fa_TokenType::OP_PLUS:
        return AST::Fa_UnaryOp::OP_PLUS;
    case tok::Fa_TokenType::OP_MINUS:
        return AST::Fa_UnaryOp::OP_NEG;
    case tok::Fa_TokenType::OP_BITNOT:
        return AST::Fa_UnaryOp::OP_BITNOT;
    case tok::Fa_TokenType::OP_NOT:
        return AST::Fa_UnaryOp::OP_NOT;
    default:
        return AST::Fa_UnaryOp::INVALID;
    }
}

Fa_Array<AST::Fa_Stmt*> Fa_Parser::parse_program()
{
    Fa_Array<AST::Fa_Stmt*> m_statements;

    while (!we_done()) {
        skip_newlines();
        if (we_done())
            break;

        auto stmt = parse_statement();
        if (stmt.has_value()) {
            m_statements.push(stmt.value());
        } else {
            if (diagnostic::is_saturated())
                break;
            synchronize();
            if (we_done())
                break;
        }
    }

    m_sema.analyze(m_statements);

    if (diagnostic::has_errors())
        diagnostic::dump();

    return m_statements;
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_statement()
{
    skip_newlines();

    if (check(tok::Fa_TokenType::KW_IF))
        return parse_if_stmt();
    if (check(tok::Fa_TokenType::KW_WHILE))
        return parse_while_stmt();
    if (check(tok::Fa_TokenType::KW_FOR))
        return parse_for_stmt();
    if (check(tok::Fa_TokenType::KW_RETURN))
        return parse_return_stmt();
    if (check(tok::Fa_TokenType::KW_BREAK))
        return parse_break_stmt();
    if (check(tok::Fa_TokenType::KW_CONTINUE))
        return parse_continue_stmt();
    if (check(tok::Fa_TokenType::KW_FN))
        return parse_function_def();

    return parse_expression_stmt();
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_return_stmt()
{
    tok::Fa_Token const* start = current_token();
    if (!consume(tok::Fa_TokenType::KW_RETURN))
        return report_error(ErrorCode::EXPECTED_RETURN);

    if (check(tok::Fa_TokenType::NEWLINE) || we_done())
        return stamp_from_token(AST::Fa_makeReturn(), start);

    auto ret = parse_expression();
    if (ret.has_error())
        return ret.error();

    return stamp_from_token(AST::Fa_makeReturn(ret.value()), start);
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_break_stmt()
{
    tok::Fa_Token const* start = current_token();
    advance();
    return stamp_from_token(AST::Fa_makeBreak(), start);
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_continue_stmt()
{
    tok::Fa_Token const* start = current_token();
    advance();
    return stamp_from_token(AST::Fa_makeContinue(), start);
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_while_stmt()
{
    tok::Fa_Token const* start = current_token();
    if (!consume(tok::Fa_TokenType::KW_WHILE))
        return report_error(ErrorCode::EXPECTED_WHILE_KEYWORD);

    auto m_condition = parse_expression();
    if (m_condition.has_error())
        return m_condition.error();

    if (!consume(tok::Fa_TokenType::COLON))
        return report_error(ErrorCode::EXPECTED_COLON_WHILE);

    auto while_block = parse_indented_block();
    if (while_block.has_error())
        return while_block.error();

    return stamp_from_token(Fa_makeWhile(m_condition.value(), static_cast<AST::Fa_BlockStmt*>(while_block.value())), start);
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_for_stmt()
{
    tok::Fa_Token const* start = current_token();
    if (!consume(tok::Fa_TokenType::KW_FOR))
        return report_error(ErrorCode::UNEXPECTED_TOKEN);

    if (!check(tok::Fa_TokenType::IDENTIFIER))
        return report_error(ErrorCode::EXPECTED_FOR_TARGET);

    auto* target = stamp_from_token(AST::Fa_makeName(current_token()->lexeme()), current_token());
    advance();

    bool saw_in = consume(tok::Fa_TokenType::KW_IN);
    if (!saw_in && check(tok::Fa_TokenType::IDENTIFIER) && current_token()->lexeme() == "في") {
        advance();
        saw_in = true;
    }
    if (!saw_in)
        return report_error(ErrorCode::EXPECTED_IN_KEYWORD);

    auto iter = parse_expression();
    if (iter.has_error())
        return iter.error();

    if (!consume(tok::Fa_TokenType::COLON))
        return report_error(ErrorCode::EXPECTED_COLON_FOR);

    auto body = parse_indented_block();
    if (body.has_error())
        return body.error();

    return stamp_from_token(AST::Fa_makeFor(target, iter.value(), body.value()), start);
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_indented_block()
{
    tok::Fa_Token const* start = current_token();
    if (check(tok::Fa_TokenType::NEWLINE))
        advance();

    if (!consume(tok::Fa_TokenType::INDENT))
        return report_error(ErrorCode::EXPECTED_INDENT);

    Fa_Array<AST::Fa_Stmt*> m_statements;

    if (check(tok::Fa_TokenType::DEDENT)) {
        advance();
        return stamp_from_token(Fa_makeBlock(m_statements), start);
    }

    while (!check(tok::Fa_TokenType::DEDENT) && !we_done()) {
        skip_newlines();
        if (check(tok::Fa_TokenType::DEDENT))
            break;

        auto stmt = parse_statement();

        if (stmt.has_value()) {
            m_statements.push(stmt.value());
        } else {
            synchronize();
            if (check(tok::Fa_TokenType::DEDENT) || we_done())
                break;
        }
    }

    if (check(tok::Fa_TokenType::ENDMARKER))
        return stamp_from_token(Fa_makeBlock(m_statements), start);
    else if (!consume(tok::Fa_TokenType::DEDENT))
        return report_error(ErrorCode::EXPECTED_DEDENT);

    auto* block = Fa_makeBlock(m_statements);
    if (!m_statements.empty())
        return stamp_from_node(block, m_statements[0]);
    return stamp_from_token(block, start);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_parameters_list()
{
    if (!consume(tok::Fa_TokenType::LPAREN))
        return report_error(ErrorCode::EXPECTED_LPAREN);

    Fa_Array<AST::Fa_Expr*> parameters = Fa_Array<AST::Fa_Expr*>::with_capacity(4); // typical small function

    if (!check(tok::Fa_TokenType::RPAREN)) {
        do {
            skip_newlines();
            if (check(tok::Fa_TokenType::RPAREN))
                break;

            if (!check(tok::Fa_TokenType::IDENTIFIER))
                return report_error(ErrorCode::EXPECTED_PARAM_NAME);

            tok::Fa_Token const* param_tok = current_token();
            Fa_StringRef param_name = param_tok->lexeme();
            advance();
            parameters.push(stamp_from_token(AST::Fa_makeName(param_name), param_tok));
            skip_newlines();
        } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RPAREN));
    }

    if (!consume(tok::Fa_TokenType::RPAREN))
        return report_error(ErrorCode::EXPECTED_RPAREN_EXPR);

    if (parameters.empty())
        return stamp_from_token(Fa_makeList(Fa_Array<AST::Fa_Expr*> { }), current_token());

    return stamp_from_node(Fa_makeList(parameters), parameters[0]);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_expression() { return parse_assignment_expr(); }

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_function_def()
{
    tok::Fa_Token const* start = current_token();
    if (!consume(tok::Fa_TokenType::KW_FN))
        return report_error(ErrorCode::EXPECTED_FN_KEYWORD);

    if (!check(tok::Fa_TokenType::IDENTIFIER))
        return report_error(ErrorCode::EXPECTED_FN_NAME);

    tok::Fa_Token const* name_tok = current_token();
    Fa_StringRef function_name = name_tok->lexeme();
    advance();

    auto parameters_list = parse_parameters_list();
    if (parameters_list.has_error())
        return parameters_list.error();

    if (!consume(tok::Fa_TokenType::COLON))
        return report_error(ErrorCode::EXPECTED_COLON_FN);

    auto function_body = parse_indented_block();
    if (function_body.has_error())
        return function_body.error();

    return stamp_from_token(
        Fa_makeFunction(stamp_from_token(AST::Fa_makeName(function_name), name_tok),
            static_cast<AST::Fa_ListExpr*>(parameters_list.value()),
            static_cast<AST::Fa_BlockStmt*>(function_body.value())),
        start);
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_if_stmt()
{
    tok::Fa_Token const* start = current_token();
    if (!consume(tok::Fa_TokenType::KW_IF))
        return report_error(ErrorCode::EXPECTED_IF_KEYWORD);

    auto m_condition = parse_expression();
    if (m_condition.has_error())
        return m_condition.error();

    if (!consume(tok::Fa_TokenType::COLON))
        return report_error(ErrorCode::EXPECTED_COLON_IF);

    auto then_block = parse_indented_block();
    if (then_block.has_error())
        return then_block.error();

    AST::Fa_Stmt* else_block = nullptr;

    skip_newlines();

    if (consume(tok::Fa_TokenType::KW_ELSE)) {
        skip_newlines();

        if (check(tok::Fa_TokenType::KW_IF)) {
            // else-if: treat as nested if inside the else clause
            auto nested = parse_if_stmt();
            if (nested.has_error())
                return nested.error();

            else_block = nested.value();
        } else {
            if (!consume(tok::Fa_TokenType::COLON))
                return report_error(ErrorCode::EXPECTED_COLON_IF);

            auto m_else_stmt = parse_indented_block();
            if (m_else_stmt.has_error())
                return m_else_stmt.error();

            else_block = m_else_stmt.value();
        }
    }

    return stamp_from_token(
        Fa_makeIf(
            m_condition.value(),
            static_cast<AST::Fa_BlockStmt*>(then_block.value()),
            else_block),
        start);
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_expression_stmt()
{
    auto expr = parse_expression();
    if (expr.has_error())
        return expr.error();

    return stamp_from_node(Fa_makeExprStmt(expr.value()), expr.value());
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_assignment_expr()
{
    auto L = parse_conditional_expr();
    if (L.has_error())
        return L.error();

    if (check(tok::Fa_TokenType::OP_ASSIGN)) {
        AST::Fa_Expr* m_target = L.value();
        AST::Fa_Expr::Kind kind = m_target->get_kind();

        if (kind != AST::Fa_Expr::Kind::NAME && kind != AST::Fa_Expr::Kind::INDEX)
            return report_error(ErrorCode::INVALID_ASSIGN_TARGET);

        advance();

        auto R = parse_assignment_expr();
        if (R.has_error())
            return R.error();

        if (kind == AST::Fa_Expr::Kind::NAME) {
            auto name_target = static_cast<AST::Fa_NameExpr*>(m_target);
            return Fa_ErrorOr<AST::Fa_Expr*>::from_value(
                stamp_from_node(Fa_makeAssignmentExpr(name_target, R.value(), /*decl=*/ false), name_target)); // let the sema detect decl vs assign
        }

        return Fa_ErrorOr<AST::Fa_Expr*>::from_value(
            stamp_from_node(Fa_makeAssignmentExpr(m_target, R.value(), /*decl=*/false), m_target));
    }

    return L.value();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_logical_expr_precedence(unsigned int min_precedence)
{
    auto L = parse_comparison_expr();
    if (L.has_error())
        return L.error();

    for (;;) {
        tok::Fa_Token const* tok = current_token();
        if (!tok->is_binary_op())
            break;

        unsigned int precedence = tok->get_precedence();
        if (precedence == tok::PREC_NONE || precedence < min_precedence)
            break;

        tok::Fa_TokenType op = m_lexer.m_current()->type();
        advance();

        auto R = parse_logical_expr_precedence(precedence + 1);
        if (R.has_error())
            return R.error();

        AST::Fa_Expr* lhs = L.value();
        L.set_value(stamp_from_node(Fa_makeBinary(lhs, R.value(), to_binary_op(op)), lhs));
    }

    return L.value();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_comparison_expr()
{
    auto L = parse_binary_expr();
    if (L.has_error())
        return L.error();

    if (current_token()->is_comparison_op()) {
        tok::Fa_TokenType op = m_lexer.m_current()->type();
        advance();

        auto R = parse_binary_expr();
        if (R.has_error())
            return R.error();

        AST::Fa_Expr* lhs = L.value();
        L.set_value(stamp_from_node(Fa_makeBinary(lhs, R.value(), to_binary_op(op)), lhs));
    }

    return L.value();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_binary_expr() { return parse_binary_expr_precedence(0); }

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_binary_expr_precedence(unsigned int min_precedence)
{
    auto L = parse_unary_expr();
    if (L.has_error())
        return L.error();

    for (;;) {
        tok::Fa_Token const* tok = current_token();
        if (!tok->is_binary_op() || tok->is(tok::Fa_TokenType::OP_ASSIGN))
            break;

        unsigned int precedence = tok->get_precedence();
        if (precedence == tok::PREC_NONE || precedence < min_precedence)
            break;

        tok::Fa_TokenType op = m_lexer.m_current()->type();
        advance();

        unsigned int next_min = precedence + 1;
        auto R = parse_binary_expr_precedence(next_min);
        if (R.has_error())
            return R.error();

        L.set_value(Fa_makeBinary(L.value(), R.value(), to_binary_op(op)));
    }

    return L.value();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_unary_expr()
{
    tok::Fa_Token const* tok = current_token();
    if (tok->is_unary_op()) {
        tok::Fa_TokenType op = m_lexer.m_current()->type();
        advance();

        auto expr = parse_unary_expr();
        if (expr.has_error())
            return expr.error();

        return stamp_from_token(Fa_makeUnary(expr.value(), to_unary_op(op)), tok);
    }
    return parse_postfix_expr();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_postfix_expr()
{
    auto expr_or = parse_primary_expr();
    if (expr_or.has_error())
        return expr_or.error();

    AST::Fa_Expr* expr = expr_or.value();

    for (;;) {
        if (check(tok::Fa_TokenType::LPAREN)) {
            tok::Fa_Token const* start = current_token();
            advance();

            Fa_Array<AST::Fa_Expr*> m_args = Fa_Array<AST::Fa_Expr*>::with_capacity(4);

            if (!check(tok::Fa_TokenType::RPAREN)) {
                do {
                    skip_newlines();
                    if (check(tok::Fa_TokenType::RPAREN))
                        break;

                    auto arg = parse_expression();
                    if (arg.has_error())
                        return arg.error();

                    m_args.push(arg.value());
                    skip_newlines();
                } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RPAREN));
            }

            if (!consume(tok::Fa_TokenType::RPAREN))
                return report_error(ErrorCode::EXPECTED_RPAREN_EXPR);

            expr = stamp_from_node(Fa_makeCall(expr, stamp_from_token(Fa_makeList(std::move(m_args)), start)), expr);
            continue;
        }

        if (check(tok::Fa_TokenType::LBRACKET)) {
            advance();

            auto m_index = parse_expression();
            if (m_index.has_error())
                return m_index.error();

            if (!consume(tok::Fa_TokenType::RBRACKET))
                return report_error(ErrorCode::EXPECTED_RBRACKET);

            expr = stamp_from_node(Fa_makeIndex(expr, m_index.value()), expr);
            continue;
        }

        break;
    }

    return expr;
}

bool Fa_Parser::we_done() const { return m_lexer.m_current()->is(tok::Fa_TokenType::ENDMARKER); }

bool Fa_Parser::check(tok::Fa_TokenType m_type) { return m_lexer.m_current()->is(m_type); }

tok::Fa_Token const* Fa_Parser::current_token() { return m_lexer.m_current(); }

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse() { return parse_expression(); }

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_primary_expr()
{
    tok::Fa_Token const* tok = current_token();

    if (current_token()->is_numeric()) {
        Fa_StringRef v = tok->lexeme();
        advance();

        tok::Fa_TokenType tt = tok->type();

        if (tt == tok::Fa_TokenType::DECIMAL)
            return stamp_from_token(AST::Fa_makeLiteralFloat(v.to_double()), tok);

        if (tt == tok::Fa_TokenType::INTEGER || tt == tok::Fa_TokenType::HEX || tt == tok::Fa_TokenType::OCTAL || tt == tok::Fa_TokenType::BINARY) {
            static constexpr int BASE_FOR_TYPE[] = { 2, 8, 10, 16 };
            return stamp_from_token(AST::Fa_makeLiteralInt(util::parse_integer_literal(v,
                /*base=*/BASE_FOR_TYPE[static_cast<int>(tt) - static_cast<int>(tok::Fa_TokenType::BINARY)])), tok);
        }
    }

    if (check(tok::Fa_TokenType::STRING)) {
        Fa_StringRef v = tok->lexeme();
        advance();
        return stamp_from_token(AST::Fa_makeLiteralString(v), tok);
    }

    if (check(tok::Fa_TokenType::KW_TRUE) || check(tok::Fa_TokenType::KW_FALSE)) {
        Fa_StringRef v = tok->lexeme();
        advance();
        return stamp_from_token(AST::Fa_makeLiteralBool(v == "صحيح" ? true : false), tok);
    }

    if (check(tok::Fa_TokenType::KW_NIL)) {
        advance();
        return stamp_from_token(AST::Fa_makeLiteralNil(), tok);
    }

    if (check(tok::Fa_TokenType::IDENTIFIER)) {
        Fa_StringRef m_name = tok->lexeme();
        advance();
        return stamp_from_token(AST::Fa_makeName(m_name), tok);
    }

    if (check(tok::Fa_TokenType::LPAREN)) {
        advance();
        if (check(tok::Fa_TokenType::RPAREN)) {
            advance();
            return stamp_from_token(Fa_makeList(Fa_Array<AST::Fa_Expr*> { }), tok);
        }

        auto expr = parse_expression();

        if (expr.has_error())
            return expr.error();

        if (!consume(tok::Fa_TokenType::RPAREN))
            return report_error(ErrorCode::EXPECTED_RPAREN_EXPR);

        return expr.value();
    }

    if (check(tok::Fa_TokenType::LBRACKET)) {
        advance();
        return parse_list_literal();
    }

    if (check(tok::Fa_TokenType::LBRACE)) {
        advance();
        return parse_dict_literal();
    }

    if (we_done())
        return report_error(ErrorCode::UNEXPECTED_EOF);

    skip_newlines();
    return report_error(ErrorCode::UNEXPECTED_TOKEN);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_list_literal()
{
    tok::Fa_Token const* start = current_token();
    Fa_Array<AST::Fa_Expr*> m_elements = Fa_Array<AST::Fa_Expr*>::with_capacity(4);

    if (!check(tok::Fa_TokenType::RBRACKET)) {
        do {
            skip_newlines();
            if (check(tok::Fa_TokenType::RBRACKET))
                break;

            auto elem = parse_expression();
            if (elem.has_error())
                return elem.error();

            m_elements.push(elem.value());
            skip_newlines();
        } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RBRACKET));
    }

    if (!consume(tok::Fa_TokenType::RBRACKET))
        return report_error(ErrorCode::EXPECTED_RBRACKET);

    AST::Fa_Expr* first = m_elements.empty() ? nullptr : m_elements[0];
    if (m_elements.empty())
        return stamp_from_token(Fa_makeList(std::move(m_elements)), start);
    return stamp_from_node(Fa_makeList(std::move(m_elements)), first);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_dict_literal()
{
    tok::Fa_Token const* start = current_token();
    Fa_Array<std::pair<AST::Fa_Expr*, AST::Fa_Expr*>> content;

    if (!check(tok::Fa_TokenType::RBRACE)) {
        do {
            skip_newlines();
            if (check(tok::Fa_TokenType::RBRACE))
                break;

            auto first = parse_expression();
            if (first.has_error())
                return first.error();

            if (!consume(tok::Fa_TokenType::COLON))
                return report_error(ErrorCode::EXPECTED_COLON_DICT);

            auto second = parse_expression();
            if (second.has_error())
                return second.error();

            content.push({ first.value(), second.value() });
            skip_newlines();
        } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RBRACE));
    }

    if (!consume(tok::Fa_TokenType::RBRACE))
        return report_error(ErrorCode::EXPECTED_RBRACE_EXPR);

    AST::Fa_Expr* first_key = content.empty() ? nullptr : content[0].first;
    if (content.empty())
        return stamp_from_token(AST::Fa_makeDict(std::move(content)), start);
    return stamp_from_node(AST::Fa_makeDict(std::move(content)), first_key);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_conditional_expr()
{
    return parse_logical_expr();
    /// TODO: Ternary?
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_logical_expr() { return parse_logical_expr_precedence(0); }

bool Fa_Parser::match(tok::Fa_TokenType const m_type)
{
    if (check(m_type)) {
        advance();
        return true;
    }

    return false;
}

void Fa_Parser::synchronize()
{
    while (!we_done()) {
        if (check(tok::Fa_TokenType::NEWLINE) || check(tok::Fa_TokenType::DEDENT)) {
            advance();
            return;
        }

        if (check(tok::Fa_TokenType::KW_IF) || check(tok::Fa_TokenType::KW_WHILE) || check(tok::Fa_TokenType::KW_FOR)
            || check(tok::Fa_TokenType::KW_RETURN) || check(tok::Fa_TokenType::KW_BREAK)
            || check(tok::Fa_TokenType::KW_CONTINUE) || check(tok::Fa_TokenType::KW_FN))
            return;

        advance();
    }
}

} // namespace fairuz::parser
