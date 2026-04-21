/// parser.cc

#include "parser.hpp"
#include "macros.hpp"
#include "util.hpp"

#include <iostream>

namespace fairuz::parser {

#define Fa_VERIFY_TOKEN(expected, errc) \
    do {                                \
        if (UNLIKELY(!match(expected))) \
            return report_error(errc);  \
    } while (0)

#define Fa_VERIFY_NODE(n)            \
    do {                             \
        if (UNLIKELY(n.has_error())) \
            return n.error();        \
    } while (0)

using TokType = tok::Fa_TokenType;
using StmtPtr = AST::Fa_Stmt*;
using ExprPtr = AST::Fa_Expr*;
using TokenPtr = tok::Fa_Token const*;
using Fa_Type = Fa_SymbolTable::DataType;
using ParserCode = diagnostic::errc::parser::Code;
using SemaCode = diagnostic::errc::sema::Code;

static constexpr char kClassInstanceName[] = "__class$instance";

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

Fa_Error Fa_Parser::report_error(ParserCode err_code)
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

void Fa_SymbolTable::define(Fa_StringRef const& name, Symbol symbol)
{
    if (lookup_local(name))
        return;

    symbol.name = name;
    m_symbols[name] = symbol;
}

typename Fa_SymbolTable::Symbol* Fa_SymbolTable::lookup(Fa_StringRef const& name)
{
    if (m_symbols.contains(name))
        return &m_symbols[name];

    return m_parent ? m_parent->lookup(name) : nullptr;
}

typename Fa_SymbolTable::Symbol* Fa_SymbolTable::lookup_local(Fa_StringRef const& name)
{
    return m_symbols.find_ptr(name);
}

bool Fa_SymbolTable::is_defined(Fa_StringRef const& name) const
{
    if (m_symbols.contains(name))
        return true;

    return m_parent ? m_parent->is_defined(name) : false;
}

void Fa_SymbolTable::mark_used(Fa_StringRef const& name, i32 line)
{
    if (Symbol* sym = lookup(name)) {
        sym->is_used = true;
        sym->usage_lines.push(line);
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
    for (auto [_, sym] : m_symbols) {
        if (!sym.is_used && sym.symbol_type == SymbolType::VARIABLE)
            unused.push(&sym);
    }

    return unused;
}

Fa_HashTable<Fa_StringRef, typename Fa_SymbolTable::Symbol, Fa_StringRefHash, Fa_StringRefEqual> const&
Fa_SymbolTable::get_symbols() const { return m_symbols; }

// semantic analyzer

Fa_Type Fa_SemanticAnalyzer::infer_type(ExprPtr expr)
{
    if (expr == nullptr)
        return Fa_Type::UNKNOWN;

    switch (expr->get_kind()) {
    case AST::Fa_Expr::Kind::LITERAL: {
        auto lit = static_cast<AST::Fa_LiteralExpr const*>(expr);

        if (lit->is_string())
            return Fa_Type::STRING;
        if (lit->is_float())
            return Fa_Type::FLOAT;
        if (lit->is_integer())
            return Fa_Type::INTEGER;
        if (lit->is_bool())
            return Fa_Type::BOOLEAN;

        return Fa_Type::NONE;
    }

    case AST::Fa_Expr::Kind::NAME: {
        auto name = static_cast<AST::Fa_NameExpr const*>(expr);
        Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(name->get_value());
        if (sym != nullptr)
            return sym->data_type;
    } break;

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = static_cast<AST::Fa_BinaryExpr const*>(expr);

        Fa_Type left_type = infer_type(bin->get_left());
        Fa_Type right_type = infer_type(bin->get_right());

        // type promotion rules
        if (left_type == Fa_Type::FLOAT || right_type == Fa_Type::FLOAT)
            return Fa_Type::FLOAT;
        if (left_type == Fa_Type::INTEGER && right_type == Fa_Type::INTEGER)
            return Fa_Type::INTEGER;
        if (bin->get_operator() == AST::Fa_BinaryOp::OP_ADD && left_type == Fa_Type::STRING)
            return Fa_Type::STRING;
        if (bin->get_operator() == AST::Fa_BinaryOp::OP_AND || bin->get_operator() == AST::Fa_BinaryOp::OP_OR)
            return Fa_Type::BOOLEAN;
    } break;

    case AST::Fa_Expr::Kind::INDEX: {
        auto index_expr = static_cast<AST::Fa_IndexExpr const*>(expr);
        Fa_Type container_type = infer_type(index_expr->get_object());
        switch (container_type) {
        case Fa_Type::LIST: {
            auto container_expr = static_cast<AST::Fa_ListExpr const*>(index_expr->get_object());
            if (container_expr->is_empty())
                return Fa_Type::ANY;

            return infer_type(container_expr->get_elements()[0]);
        }
        case Fa_Type::STRING:
            return Fa_Type::STRING;
        case Fa_Type::DICT: {
            auto dict_expr = static_cast<AST::Fa_DictExpr const*>(expr);
            if (dict_expr->get_content().empty())
                return Fa_Type::ANY;

            for (auto const& [k, v] : dict_expr->get_content()) {
                if (k->equals(index_expr))
                    return infer_type(v);
            }

            return Fa_Type::ANY;
        }
        default:
            return Fa_Type::UNKNOWN;
        }
    }

    case AST::Fa_Expr::Kind::UNARY:
        return infer_type(static_cast<AST::Fa_UnaryExpr const*>(expr)->get_operand());
    case AST::Fa_Expr::Kind::ASSIGNMENT:
        return infer_type(static_cast<AST::Fa_AssignmentExpr const*>(expr)->get_value());

    // terminals
    case AST::Fa_Expr::Kind::LIST:
        return Fa_Type::LIST;
    case AST::Fa_Expr::Kind::CALL:
        return Fa_Type::ANY;
    case AST::Fa_Expr::Kind::DICT:
        return Fa_Type::DICT;

    default:
        return Fa_Type::UNKNOWN;
    }

    return Fa_Type::UNKNOWN; // unreachable
}

void Fa_SemanticAnalyzer::report_issue(Issue::Severity sev, SemaCode code, Fa_StringRef msg, Fa_SourceLocation loc, Fa_StringRef const& sugg)
{
    m_issues.push({ sev, static_cast<u16>(code), msg, loc, sugg });
}

void Fa_SemanticAnalyzer::analyze_expr(ExprPtr expr)
{
    if (expr == nullptr)
        return;

    switch (expr->get_kind()) {
    case AST::Fa_Expr::Kind::NAME: {
        auto name = static_cast<AST::Fa_NameExpr const*>(expr);

        if (!m_current_scope->is_defined(name->get_value()))
            report_issue(Issue::Severity::ERROR, SemaCode::UNDEFINED_VARIABLE, "Undefined variable: " + name->get_value(), expr->get_location(), "Did you forget to initialize it?");
        else
            m_current_scope->mark_used(name->get_value(), expr->get_location().line);
    } break;

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = static_cast<AST::Fa_BinaryExpr const*>(expr);

        analyze_expr(bin->get_left());
        analyze_expr(bin->get_right());

        Fa_Type left_type = infer_type(bin->get_left());
        Fa_Type right_type = infer_type(bin->get_right());

        if (left_type != right_type && left_type != Fa_Type::UNKNOWN && right_type != Fa_Type::UNKNOWN
            && left_type != Fa_Type::ANY && right_type != Fa_Type::ANY)
            report_issue(Issue::Severity::ERROR, SemaCode::TYPE_MISMATCH, "Type mismatch in binary Fa_Expression", expr->get_location(), "Left and right operands must have same type");

        if (left_type == Fa_Type::STRING || right_type == Fa_Type::STRING) {
            AST::Fa_BinaryOp op = bin->get_operator();
            if (op != AST::Fa_BinaryOp::OP_ADD && op != AST::Fa_BinaryOp::OP_EQ && op != AST::Fa_BinaryOp::OP_NEQ && op != AST::Fa_BinaryOp::OP_LT
                && op != AST::Fa_BinaryOp::OP_LTE && op != AST::Fa_BinaryOp::OP_GT && op != AST::Fa_BinaryOp::OP_GTE)
                report_issue(Issue::Severity::ERROR, SemaCode::INVALID_STRING_OP, "Invalid operation on string", expr->get_location());
        }

        if (bin->get_operator() == AST::Fa_BinaryOp::OP_DIV && bin->get_right()->get_kind() == AST::Fa_Expr::Kind::LITERAL) {
            auto lit = static_cast<AST::Fa_LiteralExpr const*>(bin->get_right());
            if (lit->is_numeric() && lit->is_number() == 0)
                report_issue(Issue::Severity::ERROR, SemaCode::DIVISION_BY_ZERO_CONST, "Division by zero", expr->get_location(), "This will cause a runtime error");
        }
    } break;

    case AST::Fa_Expr::Kind::UNARY: {
        auto un = static_cast<AST::Fa_UnaryExpr const*>(expr);
        analyze_expr(un->get_operand());
    } break;

    case AST::Fa_Expr::Kind::CALL: {
        auto call = static_cast<AST::Fa_CallExpr const*>(expr);
        analyze_expr(call->get_callee());

        for (ExprPtr arg : call->get_args())
            analyze_expr(arg);

        if (call->get_callee()->get_kind() == AST::Fa_Expr::Kind::NAME) {
            auto name = static_cast<AST::Fa_NameExpr const*>(call->get_callee());

            if (Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(name->get_value())) {
                if (sym->symbol_type != Fa_SymbolTable::SymbolType::FUNCTION)
                    report_issue(Issue::Severity::ERROR, SemaCode::NOT_CALLABLE, "'" + name->get_value() + "' is not callable", expr->get_location());
            }
        }

        if (call->get_callee()->get_kind() == AST::Fa_Expr::Kind::NAME) {
            auto name = static_cast<AST::Fa_NameExpr const*>(call->get_callee());
            Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(name->get_value());

            if (sym == nullptr)
                report_issue(Issue::Severity::ERROR, SemaCode::UNDEFINED_FUNCTION, "Undefined function: " + name->get_value(), expr->get_location());
            else if (sym->symbol_type != Fa_SymbolTable::SymbolType::FUNCTION)
                report_issue(Issue::Severity::ERROR, SemaCode::NOT_CALLABLE, "'" + name->get_value() + "' is not callable", expr->get_location());
        }
    } break;

    case AST::Fa_Expr::Kind::LIST: {
        auto list = static_cast<AST::Fa_ListExpr const*>(expr);
        for (ExprPtr elem : list->get_elements())
            analyze_expr(elem);
    } break;

    case AST::Fa_Expr::Kind::INDEX: {
        auto idx = static_cast<AST::Fa_IndexExpr const*>(expr);
        analyze_expr(idx->get_object());
        analyze_expr(idx->get_index());
    } break;

    case AST::Fa_Expr::Kind::DICT: {
        auto dict = static_cast<AST::Fa_DictExpr const*>(expr);
        for (auto const& [key, value] : dict->get_content()) {
            analyze_expr(key);
            analyze_expr(value);
        }
    } break;

    case AST::Fa_Expr::Kind::ASSIGNMENT: {
        auto assign = static_cast<AST::Fa_AssignmentExpr*>(expr);
        analyze_expr(assign->get_value());

        ExprPtr target = assign->get_target();
        if (target == nullptr)
            break;

        if (target->get_kind() == AST::Fa_Expr::Kind::INDEX) {
            analyze_expr(target);
            break;
        }

        if (target->get_kind() == AST::Fa_Expr::Kind::NAME) {
            Fa_StringRef target_name = static_cast<AST::Fa_NameExpr*>(target)->get_value();
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
                sym.definition_loc = expr->get_location();
                m_current_scope->define(target_name, sym);
            } else {
                m_current_scope->mark_used(target_name, expr->get_location().line);
            }
        }
    } break;

    default:
        break;
    }
}

void Fa_SemanticAnalyzer::analyze_stmt(StmtPtr stmt)
{
    if (stmt == nullptr)
        return;

    switch (stmt->get_kind()) {
    case AST::Fa_Stmt::Kind::BLOCK: {
        auto block = static_cast<AST::Fa_BlockStmt const*>(stmt);
        Fa_SymbolTable* previous = m_current_scope;
        m_current_scope = m_current_scope->create_child();

        for (StmtPtr child : block->get_statements())
            analyze_stmt(child);

        m_current_scope = previous;
    } break;

    case AST::Fa_Stmt::Kind::ASSIGNMENT:
        analyze_expr(static_cast<AST::Fa_AssignmentStmt*>(stmt)->get_expr());
        break;

    case AST::Fa_Stmt::Kind::EXPR: {
        auto Fa_Expr_stmt = static_cast<AST::Fa_ExprStmt const*>(stmt);
        analyze_expr(Fa_Expr_stmt->get_expr());
        if (Fa_Expr_stmt->get_expr()->get_kind() != AST::Fa_Expr::Kind::CALL
            && Fa_Expr_stmt->get_expr()->get_kind() != AST::Fa_Expr::Kind::ASSIGNMENT)
            report_issue(Issue::Severity::INFO, SemaCode::UNUSED_EXPR_RESULT, "Fa_Expression result not used", stmt->get_location());
    } break;

    case AST::Fa_Stmt::Kind::IF: {
        auto if_stmt = static_cast<AST::Fa_IfStmt const*>(stmt);
        analyze_expr(if_stmt->get_condition());

        StmtPtr then_block = if_stmt->get_then();
        StmtPtr else_block = if_stmt->get_else();

        if (if_stmt->get_condition()->get_kind() == AST::Fa_Expr::Kind::LITERAL)
            report_issue(Issue::Severity::WARNING, SemaCode::CONSTANT_CONDITION, "Condition is always constant", stmt->get_location(), "Consider removing if statement");

        if (then_block != nullptr)
            analyze_stmt(then_block);
        if (else_block != nullptr)
            analyze_stmt(else_block);
    } break;

    case AST::Fa_Stmt::Kind::WHILE: {
        auto while_stmt = static_cast<AST::Fa_WhileStmt*>(stmt);
        analyze_expr(while_stmt->get_condition());

        // Detect infinite loops
        if (while_stmt->get_condition()->get_kind() == AST::Fa_Expr::Kind::LITERAL) {
            AST::Fa_LiteralExpr const* lit = static_cast<AST::Fa_LiteralExpr const*>(while_stmt->get_condition());
            if (lit->is_bool() && lit->get_bool() == true)
                report_issue(Issue::Severity::WARNING, SemaCode::INFINITE_LOOP, "Infinite loop detected", stmt->get_location(), "Add a break condition");
        }

        analyze_stmt(while_stmt->get_body());
    } break;

    case AST::Fa_Stmt::Kind::FOR: {
        auto for_stmt = static_cast<AST::Fa_ForStmt const*>(stmt);
        analyze_expr(for_stmt->get_iter());

        m_current_scope = m_current_scope->create_child();
        Fa_SymbolTable::Symbol loop_var;
        loop_var.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
        loop_var.data_type = Fa_Type::ANY;
        m_current_scope->define(for_stmt->get_target()->get_value(), loop_var);

        analyze_stmt(for_stmt->get_body());

        if (m_current_scope->m_parent && m_current_scope->m_parent->lookup_local(for_stmt->get_target()->get_value()))
            report_issue(Issue::Severity::WARNING, SemaCode::LOOP_VAR_SHADOW, "Loop variable shadows outer variable", stmt->get_location());

        m_current_scope = m_current_scope->m_parent;
    } break;

    case AST::Fa_Stmt::Kind::FUNC: {
        auto func_def = static_cast<AST::Fa_FunctionDef const*>(stmt);
        Fa_SymbolTable::Symbol func_sym;
        func_sym.symbol_type = Fa_SymbolTable::SymbolType::FUNCTION;
        func_sym.data_type = Fa_Type::FUNCTION;
        func_sym.definition_loc = stmt->get_location();
        m_current_scope->define(func_def->get_name()->get_value(), func_sym);

        m_current_scope = m_current_scope->create_child();

        for (AST::Fa_Expr const* const& param : func_def->get_parameters()) {
            Fa_SymbolTable::Symbol param_sym;
            param_sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
            param_sym.data_type = Fa_Type::ANY;
            m_current_scope->define(static_cast<AST::Fa_NameExpr const*>(param)->get_value(), param_sym);
        }

        analyze_stmt(func_def->get_body());
        if (!stmt_definitely_returns(func_def->get_body()))
            report_issue(Issue::Severity::INFO, SemaCode::MISSING_RETURN, "Function may not return a value", stmt->get_location());
        // Exit function scope
        m_current_scope = m_current_scope->m_parent;
    } break;

    case AST::Fa_Stmt::Kind::RETURN: {
        auto ret = static_cast<AST::Fa_ReturnStmt*>(stmt);
        analyze_expr(ret->get_value());
    } break;

    case AST::Fa_Stmt::Kind::BREAK:
    case AST::Fa_Stmt::Kind::CONTINUE:
        break;

    case AST::Fa_Stmt::Kind::CLASS_DEF: {
        auto class_def = static_cast<AST::Fa_ClassDef const*>(stmt);

        if (auto const* class_name = dynamic_cast<AST::Fa_NameExpr const*>(class_def->get_name())) {
            Fa_SymbolTable::Symbol class_sym;
            class_sym.symbol_type = Fa_SymbolTable::SymbolType::CLASS;
            class_sym.data_type = Fa_Type::ANY;
            class_sym.definition_loc = stmt->get_location();
            m_current_scope->define(class_name->get_value(), class_sym);
        }

        for (StmtPtr method_stmt : class_def->get_methods()) {
            auto const* method = dynamic_cast<AST::Fa_FunctionDef const*>(method_stmt);
            if (method == nullptr)
                continue;

            Fa_SymbolTable* previous = m_current_scope;
            m_current_scope = m_current_scope->create_child();

            Fa_SymbolTable::Symbol instance_sym;
            instance_sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
            instance_sym.data_type = Fa_Type::DICT;
            m_current_scope->define(kClassInstanceName, instance_sym);

            for (AST::Fa_Expr const* const& param : method->get_parameters()) {
                auto const* param_name = static_cast<AST::Fa_NameExpr const*>(param);
                Fa_SymbolTable::Symbol param_sym;
                param_sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
                param_sym.data_type = Fa_Type::ANY;
                m_current_scope->define(param_name->get_value(), param_sym);
            }

            analyze_stmt(method->get_body());
            m_current_scope = previous;
        }
    } break;

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
        sym.name = name;
        sym.symbol_type = Fa_SymbolTable::SymbolType::FUNCTION;
        sym.data_type = Fa_Type::FUNCTION;
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

void Fa_SemanticAnalyzer::analyze(Fa_Array<StmtPtr> const& stmts)
{
    for (StmtPtr stmt : stmts)
        analyze_stmt(stmt);

    Fa_Array<Fa_SymbolTable::Symbol*> unused = m_global_scope->get_unused_symbols();
    for (Fa_SymbolTable::Symbol* sym : unused)
        report_issue(Issue::Severity::WARNING, SemaCode::UNUSED_VARIABLE, "Unused variable: " + sym->name, sym->definition_loc, "Consider removing if not needed");

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

        diagnostic::report(sev, issue.loc, issue.code);
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

        std::cout << "[" << sev_str << "] Line " << issue.loc.line << ": " << issue.message << "\n";

        if (!issue.suggestion.empty())
            std::cout << "  → " << issue.suggestion << "\n";

        std::cout << "\n";
    }
}

AST::Fa_BinaryOp to_binary_op(TokType const op)
{
    switch (op) {
    case TokType::OP_PLUS:
        return AST::Fa_BinaryOp::OP_ADD;
    case TokType::OP_MINUS:
        return AST::Fa_BinaryOp::OP_SUB;
    case TokType::OP_STAR:
        return AST::Fa_BinaryOp::OP_MUL;
    case TokType::OP_SLASH:
        return AST::Fa_BinaryOp::OP_DIV;
    case TokType::OP_PERCENT:
        return AST::Fa_BinaryOp::OP_MOD;
    case TokType::OP_POWER:
        return AST::Fa_BinaryOp::OP_POW;
    case TokType::OP_EQ:
        return AST::Fa_BinaryOp::OP_EQ;
    case TokType::OP_NEQ:
        return AST::Fa_BinaryOp::OP_NEQ;
    case TokType::OP_LT:
        return AST::Fa_BinaryOp::OP_LT;
    case TokType::OP_GT:
        return AST::Fa_BinaryOp::OP_GT;
    case TokType::OP_LTE:
        return AST::Fa_BinaryOp::OP_LTE;
    case TokType::OP_GTE:
        return AST::Fa_BinaryOp::OP_GTE;
    case TokType::OP_BITAND:
        return AST::Fa_BinaryOp::OP_BITAND;
    case TokType::OP_BITOR:
        return AST::Fa_BinaryOp::OP_BITOR;
    case TokType::OP_BITXOR:
        return AST::Fa_BinaryOp::OP_BITXOR;
    case TokType::OP_LSHIFT:
        return AST::Fa_BinaryOp::OP_LSHIFT;
    case TokType::OP_RSHIFT:
        return AST::Fa_BinaryOp::OP_RSHIFT;
    case TokType::OP_AND:
        return AST::Fa_BinaryOp::OP_AND;
    case TokType::OP_OR:
        return AST::Fa_BinaryOp::OP_OR;
    default:
        return AST::Fa_BinaryOp::INVALID;
    }
}

AST::Fa_UnaryOp to_unary_op(TokType const op)
{
    switch (op) {
    case TokType::OP_PLUS:
        return AST::Fa_UnaryOp::OP_PLUS;
    case TokType::OP_MINUS:
        return AST::Fa_UnaryOp::OP_NEG;
    case TokType::OP_BITNOT:
        return AST::Fa_UnaryOp::OP_BITNOT;
    case TokType::OP_NOT:
        return AST::Fa_UnaryOp::OP_NOT;
    default:
        return AST::Fa_UnaryOp::INVALID;
    }
}

Fa_Array<StmtPtr> Fa_Parser::parse_program()
{
    Fa_Array<StmtPtr> stmts;

    while (!we_done()) {
        skip_newlines();
        if (we_done())
            break;

        auto stmt = parse_statement();
        if (stmt.has_value()) {
            stmts.push(stmt.value());
        } else {
            if (diagnostic::is_saturated())
                break;
            synchronize();
            if (we_done())
                break;
        }
    }

    m_sema.analyze(stmts);

    if (diagnostic::has_errors())
        diagnostic::dump();

    return stmts;
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_statement()
{
    skip_newlines();

    if (check(TokType::KW_IF))
        return parse_if_stmt();
    if (check(TokType::KW_WHILE))
        return parse_while_stmt();
    if (check(TokType::KW_FOR))
        return parse_for_stmt();
    if (check(TokType::KW_RETURN))
        return parse_return_stmt();
    if (check(TokType::KW_BREAK))
        return parse_break_stmt();
    if (check(TokType::KW_CONTINUE))
        return parse_continue_stmt();
    if (check(TokType::KW_FN))
        return parse_function_def();
    if (check(TokType::KW_CLASS))
        return parse_class_def();

    return parse_expression_stmt();
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_return_stmt()
{
    TokenPtr start = current_token();
    Fa_SourceLocation start_loc = start->location();

    Fa_VERIFY_TOKEN(TokType::KW_RETURN, ParserCode::EXPECTED_RETURN);

    if (check(TokType::NEWLINE) || we_done())
        return AST::Fa_makeReturn(start_loc);

    auto ret = parse_expression();
    Fa_VERIFY_NODE(ret);

    return AST::Fa_makeReturn(start_loc, ret.value());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_break_stmt()
{
    TokenPtr start = current_token();
    advance();
    return AST::Fa_makeBreak(start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_continue_stmt()
{
    TokenPtr start = current_token();
    advance();
    return AST::Fa_makeContinue(start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_while_stmt()
{
    TokenPtr start = current_token();

    Fa_VERIFY_TOKEN(TokType::KW_WHILE, ParserCode::EXPECTED_WHILE_KEYWORD);

    auto condition = parse_expression();
    Fa_VERIFY_NODE(condition);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_WHILE);

    auto while_block = parse_indented_block();
    Fa_VERIFY_NODE(while_block);

    return Fa_makeWhile(condition.value(), static_cast<AST::Fa_BlockStmt*>(while_block.value()), start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_for_stmt()
{
    TokenPtr start = current_token();
    Fa_SourceLocation start_loc = start->location();

    Fa_VERIFY_TOKEN(TokType::KW_FOR, ParserCode::UNEXPECTED_TOKEN);

    if (!check(TokType::IDENTIFIER))
        return report_error(ParserCode::EXPECTED_FOR_TARGET);

    auto* target = AST::Fa_makeName(current_token()->lexeme(), current_token()->location());
    advance();

    bool saw_in = match(TokType::KW_IN);
    if (!saw_in && check(TokType::IDENTIFIER) && current_token()->lexeme() == "في") {
        advance();
        saw_in = true;
    }
    if (!saw_in)
        return report_error(ParserCode::EXPECTED_IN_KEYWORD);

    auto iter = parse_expression();
    Fa_VERIFY_NODE(iter);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_FOR);

    auto body = parse_indented_block();
    Fa_VERIFY_NODE(body);

    return AST::Fa_makeFor(target, iter.value(), body.value(), start_loc);
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_indented_block()
{
    TokenPtr start = current_token();
    Fa_SourceLocation start_loc = start->location();

    match(TokType::NEWLINE);

    Fa_VERIFY_TOKEN(TokType::INDENT, ParserCode::EXPECTED_INDENT);

    Fa_Array<StmtPtr> stmts;

    if (match(TokType::DEDENT))
        return Fa_makeBlock(stmts, start_loc);

    while (!check(TokType::DEDENT) && !we_done()) {
        skip_newlines();
        if (check(TokType::DEDENT))
            break;

        auto stmt = parse_statement();

        if (stmt.has_value()) {
            stmts.push(stmt.value());
        } else {
            synchronize();
            if (check(TokType::DEDENT) || we_done())
                break;
        }
    }

    if (check(TokType::ENDMARKER))
        return Fa_makeBlock(stmts, start->location());

    Fa_VERIFY_TOKEN(TokType::DEDENT, ParserCode::EXPECTED_DEDENT);

    auto* block = Fa_makeBlock(stmts, start->location());
    if (!stmts.empty())
        return block;
    return block;
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_class_def()
{
    TokenPtr start = current_token();

    Fa_VERIFY_TOKEN(TokType::KW_CLASS, ParserCode::EXPECTED_CLASS_KEYWORD);

    Fa_Array<ExprPtr> members = Fa_Array<ExprPtr>::with_capacity(4);
    Fa_Array<StmtPtr> methods = Fa_Array<StmtPtr>::with_capacity(4);

    auto class_name = parse_expression();
    Fa_VERIFY_NODE(class_name);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_CLASS);
    skip_newlines();
    Fa_VERIFY_TOKEN(TokType::INDENT, ParserCode::EXPECTED_INDENT);

    do {
        auto method = parse_class_method(members);
        Fa_VERIFY_NODE(method);
        methods.push(method.value());
    } while (!match(TokType::DEDENT));

    return AST::Fa_makeClassDef(class_name.value(), members, methods, start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_class_method(Fa_Array<ExprPtr>& members)
{
    TokenPtr start = current_token();

    Fa_VERIFY_TOKEN(TokType::KW_FN, ParserCode::EXPECTED_FN_KEYWORD);

    TokenPtr name_tok = current_token();
    Fa_VERIFY_TOKEN(TokType::IDENTIFIER, ParserCode::EXPECTED_FN_NAME);

    auto parameter_list = parse_parameters_list();
    Fa_VERIFY_NODE(parameter_list);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_FN);
    skip_newlines();
    Fa_VERIFY_TOKEN(TokType::INDENT, ParserCode::EXPECTED_INDENT);

    Fa_Array<StmtPtr> stmts;
    while (!check(TokType::DEDENT) && !we_done()) {
        skip_newlines();
        if (check(TokType::DEDENT))
            break;

        if (match(TokType::DOT)) {
            if (!check(TokType::IDENTIFIER))
                return report_error(ParserCode::INVALID_ASSIGN_TARGET);

            TokenPtr member_tok = current_token();
            Fa_StringRef member_name = member_tok->lexeme();
            advance();

            AST::Fa_Expr* target = AST::Fa_makeIndex(
                AST::Fa_makeName(kClassInstanceName, member_tok->location()),
                AST::Fa_makeLiteralString(member_name, member_tok->location()),
                member_tok->location());

            AST::Fa_AssignmentExpr* member_assign = nullptr;

            if (check(TokType::OP_ASSIGN)) {
                advance();
                auto rhs = parse_assignment_expr();
                Fa_VERIFY_NODE(rhs);
                member_assign = AST::Fa_makeAssignmentExpr(target, rhs.value(), member_tok->location(), false);
            } else if (check(TokType::OP_PLUSEQ) || check(TokType::OP_MINUSEQ) || check(TokType::OP_STAREQ)
                || check(TokType::OP_SLASHEQ) || check(TokType::OP_PERCENTEQ)) {
                TokenPtr op_tok = current_token();
                advance();

                auto rhs = parse_assignment_expr();
                Fa_VERIFY_NODE(rhs);

                AST::Fa_BinaryOp bin_ops[] = {
                    AST::Fa_BinaryOp::OP_ADD, AST::Fa_BinaryOp::OP_SUB,
                    AST::Fa_BinaryOp::OP_MUL, AST::Fa_BinaryOp::OP_DIV,
                    AST::Fa_BinaryOp::OP_MOD
                };

                AST::Fa_BinaryOp op = bin_ops[static_cast<int>(op_tok->type()) - static_cast<int>(TokType::OP_PLUSEQ)];
                AST::Fa_BinaryExpr* bin = AST::Fa_makeBinary(target->clone(), rhs.value(), op, target->get_location());
                member_assign = AST::Fa_makeAssignmentExpr(target, bin, member_tok->location(), false);
            } else {
                return report_error(ParserCode::INVALID_ASSIGN_TARGET);
            }

            members.push(AST::Fa_makeName(member_name, member_tok->location()));
            stmts.push(AST::Fa_makeExprStmt(member_assign, member_tok->location()));
            continue;
        }

        auto stmt = parse_statement();
        Fa_VERIFY_NODE(stmt);
        stmts.push(stmt.value());
    }

    if (check(TokType::ENDMARKER)) {
        return AST::Fa_makeFunction(
            AST::Fa_makeName(name_tok->lexeme(), name_tok->location()),
            static_cast<AST::Fa_ListExpr*>(parameter_list.value()),
            AST::Fa_makeBlock(stmts, stmts[0]->get_location()),
            start->location());
    }

    Fa_VERIFY_TOKEN(TokType::DEDENT, ParserCode::EXPECTED_DEDENT);

    AST::Fa_BlockStmt* block = AST::Fa_makeBlock(stmts, stmts[0]->get_location());
    return AST::Fa_makeFunction(
        AST::Fa_makeName(name_tok->lexeme(), name_tok->location()),
        static_cast<AST::Fa_ListExpr*>(parameter_list.value()),
        block,
        start->location());
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_parameters_list()
{
    Fa_VERIFY_TOKEN(TokType::LPAREN, ParserCode::EXPECTED_LPAREN);

    Fa_Array<ExprPtr> parameters = Fa_Array<ExprPtr>::with_capacity(4); // typical small function

    if (!check(TokType::RPAREN)) {
        do {
            skip_newlines();
            if (check(TokType::RPAREN))
                break;

            if (!check(TokType::IDENTIFIER))
                return report_error(ParserCode::EXPECTED_PARAM_NAME);

            TokenPtr param_tok = current_token();
            Fa_StringRef param_name = param_tok->lexeme();
            advance();
            parameters.push(AST::Fa_makeName(param_name, param_tok->location()));
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RPAREN));
    }

    Fa_VERIFY_TOKEN(TokType::RPAREN, ParserCode::EXPECTED_RPAREN_EXPR);

    if (parameters.empty()) {
        TokenPtr cur_tok = current_token();
        return Fa_makeList(Fa_Array<ExprPtr> { }, cur_tok->location());
    }

    return Fa_makeList(parameters, parameters[0]->get_location());
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_expression() { return parse_assignment_expr(); }

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_function_def()
{
    TokenPtr start = current_token();

    Fa_VERIFY_TOKEN(TokType::KW_FN, ParserCode::EXPECTED_FN_KEYWORD);

    if (!check(TokType::IDENTIFIER))
        return report_error(ParserCode::EXPECTED_FN_NAME);

    TokenPtr name_tok = current_token();
    Fa_StringRef function_name = name_tok->lexeme();
    advance();

    auto parameters_list = parse_parameters_list();
    Fa_VERIFY_NODE(parameters_list);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_FN);

    auto function_body = parse_indented_block();
    Fa_VERIFY_NODE(function_body);

    return Fa_makeFunction(AST::Fa_makeName(function_name, name_tok->location()),
        static_cast<AST::Fa_ListExpr*>(parameters_list.value()),
        static_cast<AST::Fa_BlockStmt*>(function_body.value()),
        start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_if_stmt()
{
    TokenPtr start = current_token();

    Fa_VERIFY_TOKEN(TokType::KW_IF, ParserCode::EXPECTED_IF_KEYWORD);

    auto condition = parse_expression();
    Fa_VERIFY_NODE(condition);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_IF);

    auto then_block = parse_indented_block();
    Fa_VERIFY_NODE(then_block);

    StmtPtr else_block = nullptr;

    skip_newlines();

    if (match(TokType::KW_ELSE)) {
        skip_newlines();

        if (check(TokType::KW_IF)) {
            // else-if: treat as nested if inside the else clause
            auto nested = parse_if_stmt();
            Fa_VERIFY_NODE(nested);

            else_block = nested.value();
        } else {
            Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_IF);

            auto else_stmt = parse_indented_block();
            Fa_VERIFY_NODE(else_stmt);

            else_block = else_stmt.value();
        }
    }

    return Fa_makeIf(
        condition.value(),
        static_cast<AST::Fa_BlockStmt*>(then_block.value()),
        start->location(),
        else_block);
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_expression_stmt()
{
    auto expr = parse_expression();
    Fa_VERIFY_NODE(expr);
    ExprPtr expr_node = expr.value();
    return Fa_makeExprStmt(expr_node, expr_node->get_location());
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_assignment_expr()
{
    auto lhs = parse_conditional_expr();
    Fa_VERIFY_NODE(lhs);

    if (check(TokType::OP_ASSIGN) || /*augmented assign */ check(TokType::OP_PLUSEQ) || check(TokType::OP_MINUSEQ)
        || check(TokType::OP_STAREQ) || check(TokType::OP_SLASHEQ) || check(TokType::OP_PERCENTEQ)) {
        ExprPtr target = lhs.value();
        AST::Fa_Expr::Kind kind = target->get_kind();

        if (kind != AST::Fa_Expr::Kind::NAME && kind != AST::Fa_Expr::Kind::INDEX)
            return report_error(ParserCode::INVALID_ASSIGN_TARGET);

        if (check(TokType::OP_PLUSEQ) || check(TokType::OP_MINUSEQ) || check(TokType::OP_STAREQ)
            || check(TokType::OP_SLASHEQ) || check(TokType::OP_PERCENTEQ)) {
            TokenPtr op_tok = current_token();
            advance();

            auto rhs = parse_assignment_expr();
            Fa_VERIFY_NODE(rhs);

            AST::Fa_BinaryOp bin_ops[] = {
                AST::Fa_BinaryOp::OP_ADD, AST::Fa_BinaryOp::OP_SUB,
                AST::Fa_BinaryOp::OP_MUL, AST::Fa_BinaryOp::OP_DIV,
                AST::Fa_BinaryOp::OP_MOD
            };

            AST::Fa_BinaryOp op = bin_ops[static_cast<int>(op_tok->type()) - static_cast<int>(TokType::OP_PLUSEQ)];
            AST::Fa_Expr* c_lhs = lhs.value()->clone();
            AST::Fa_BinaryExpr* bin = AST::Fa_makeBinary(c_lhs, rhs.value(), op, c_lhs->get_location());

            return Fa_ErrorOr<ExprPtr>::from_value(
                Fa_makeAssignmentExpr(target, bin, target->get_location(), /*decl=*/false));
        }

        advance();

        auto rhs = parse_assignment_expr();
        Fa_VERIFY_NODE(rhs);

        return Fa_ErrorOr<ExprPtr>::from_value(
            Fa_makeAssignmentExpr(target, rhs.value(), target->get_location(), /*decl=*/false));
    }

    return lhs.value();
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_logical_expr_precedence(unsigned int min_precedence)
{
    auto lhs = parse_comparison_expr();
    Fa_VERIFY_NODE(lhs);

    for (;;) {
        TokenPtr cur_tok = current_token();
        if (!cur_tok->is_binary_op())
            break;

        unsigned int precedence = cur_tok->get_precedence();
        if (precedence == tok::PREC_NONE || precedence < min_precedence)
            break;

        TokType op = cur_tok->type();
        advance();

        auto rhs = parse_logical_expr_precedence(precedence + 1);
        Fa_VERIFY_NODE(rhs);

        return Fa_makeBinary(lhs.value(), rhs.value(), to_binary_op(op), lhs.value()->get_location());
    }

    return lhs.value();
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_comparison_expr()
{
    auto lhs = parse_binary_expr();
    Fa_VERIFY_NODE(lhs);

    TokenPtr cur_tok = current_token();
    if (cur_tok->is_comparison_op()) {
        TokType op = cur_tok->type();
        advance();

        auto rhs = parse_binary_expr();
        Fa_VERIFY_NODE(rhs);

        return Fa_makeBinary(lhs.value(), rhs.value(), to_binary_op(op), lhs.value()->get_location());
    }

    return lhs.value();
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_binary_expr() { return parse_binary_expr_precedence(0); }

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_binary_expr_precedence(unsigned int min_precedence)
{
    auto lhs = parse_unary_expr();
    Fa_VERIFY_NODE(lhs);

    for (;;) {
        TokenPtr cur_tok = current_token();
        if (!cur_tok->is_binary_op() || cur_tok->is(TokType::OP_ASSIGN))
            break;

        unsigned int precedence = cur_tok->get_precedence();
        if (precedence == tok::PREC_NONE || precedence < min_precedence)
            break;

        TokType op = cur_tok->type();
        advance();

        unsigned int next_min = precedence + 1;
        auto rhs = parse_binary_expr_precedence(next_min);
        Fa_VERIFY_NODE(rhs);

        return Fa_makeBinary(lhs.value(), rhs.value(), to_binary_op(op), lhs.value()->get_location());
    }

    return lhs.value();
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_unary_expr()
{
    TokenPtr cur_tok = current_token();
    if (cur_tok->is_unary_op()) {
        TokType op = cur_tok->type();
        advance();

        auto expr = parse_unary_expr();
        Fa_VERIFY_NODE(expr);

        return Fa_makeUnary(expr.value(), to_unary_op(op), expr.value()->get_location());
    }

    return parse_postfix_expr();
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_postfix_expr()
{
    auto expr_or = parse_primary_expr();
    Fa_VERIFY_NODE(expr_or);

    ExprPtr expr = expr_or.value();

    for (;;) {
        if (check(TokType::LPAREN)) {
            advance();

            Fa_Array<ExprPtr> args = Fa_Array<ExprPtr>::with_capacity(4);

            if (!check(TokType::RPAREN)) {
                do {
                    skip_newlines();
                    if (check(TokType::RPAREN))
                        break;

                    auto arg = parse_expression();
                    Fa_VERIFY_NODE(arg);

                    args.push(arg.value());
                    skip_newlines();
                } while (match(TokType::COMMA) && !check(TokType::RPAREN));
            }

            Fa_VERIFY_TOKEN(TokType::RPAREN, ParserCode::EXPECTED_RPAREN_EXPR);
            Fa_SourceLocation arg_loc = !args.empty() && args[0] ? args[0]->get_location() : Fa_SourceLocation { };
            expr = Fa_makeCall(expr, Fa_makeList(std::move(args), arg_loc), expr ? expr->get_location() : Fa_SourceLocation { });
            continue;
        }

        if (match(TokType::LBRACKET)) {
            auto index = parse_expression();
            Fa_VERIFY_NODE(index);

            Fa_VERIFY_TOKEN(TokType::RBRACKET, ParserCode::EXPECTED_RBRACKET);

            expr = Fa_makeIndex(expr, index.value(), expr ? expr->get_location() : Fa_SourceLocation { });
            continue;
        }

        break;
    }

    return expr;
}

bool Fa_Parser::we_done() const { return current_token()->is(TokType::ENDMARKER); }

bool Fa_Parser::check(TokType m_type) { return current_token()->is(m_type); }

TokenPtr Fa_Parser::current_token() const { return m_lexer.current(); }

Fa_ErrorOr<ExprPtr> Fa_Parser::parse() { return parse_expression(); }

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_primary_expr()
{
    TokenPtr cur_tok = current_token();

    if (cur_tok->is_numeric()) {
        Fa_StringRef v = cur_tok->lexeme();
        advance();

        TokType tt = cur_tok->type();

        if (tt == TokType::DECIMAL)
            return AST::Fa_makeLiteralFloat(v.to_double(), cur_tok->location());

        if (tt == TokType::INTEGER || tt == TokType::HEX || tt == TokType::OCTAL || tt == TokType::BINARY) {
            static constexpr int BASE_FOR_TYPE[] = { 2, 8, 10, 16 };
            return AST::Fa_makeLiteralInt(util::parse_integer_literal(v,
                                              /*base=*/BASE_FOR_TYPE[static_cast<int>(tt) - static_cast<int>(TokType::BINARY)]),
                cur_tok->location());
        }
    }

    if (match(TokType::STRING))
        return AST::Fa_makeLiteralString(cur_tok->lexeme(), cur_tok->location());

    if (check(TokType::KW_TRUE) || check(TokType::KW_FALSE)) {
        advance();
        return AST::Fa_makeLiteralBool(cur_tok->lexeme() == "صحيح" ? true : false, cur_tok->location());
    }

    if (match(TokType::KW_NIL))
        return AST::Fa_makeLiteralNil(cur_tok->location());

    if (match(TokType::IDENTIFIER))
        return AST::Fa_makeName(cur_tok->lexeme(), cur_tok->location());

    if (match(TokType::LPAREN)) {
        if (match(TokType::RPAREN))
            return Fa_makeList(Fa_Array<ExprPtr> { }, cur_tok->location());

        auto expr = parse_expression();
        Fa_VERIFY_NODE(expr);

        Fa_VERIFY_TOKEN(TokType::RPAREN, ParserCode::EXPECTED_RPAREN_EXPR);

        return expr.value();
    }

    if (match(TokType::LBRACKET))
        return parse_list_literal();

    if (match(TokType::LBRACE))
        return parse_dict_literal();

    if (we_done())
        return report_error(ParserCode::UNEXPECTED_EOF);

    skip_newlines();
    return report_error(ParserCode::UNEXPECTED_TOKEN);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_list_literal()
{
    TokenPtr start = current_token();
    Fa_Array<ExprPtr> elements = Fa_Array<ExprPtr>::with_capacity(4);

    if (!check(TokType::RBRACKET)) {
        do {
            skip_newlines();
            if (check(TokType::RBRACKET))
                break;

            auto elem = parse_expression();
            Fa_VERIFY_NODE(elem);

            elements.push(elem.value());
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RBRACKET));
    }

    Fa_VERIFY_TOKEN(TokType::RBRACKET, ParserCode::EXPECTED_RBRACKET);

    ExprPtr first = elements.empty() ? nullptr : elements[0];
    if (elements.empty())
        return Fa_makeList(std::move(elements), start->location());
    return Fa_makeList(std::move(elements), first->get_location());
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_dict_literal()
{
    TokenPtr start = current_token();
    Fa_Array<std::pair<ExprPtr, ExprPtr>> content;

    if (!check(TokType::RBRACE)) {
        do {
            skip_newlines();
            if (check(TokType::RBRACE))
                break;

            auto first = parse_expression();
            Fa_VERIFY_NODE(first);

            Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_DICT);

            auto second = parse_expression();
            Fa_VERIFY_NODE(second);

            content.push({ first.value(), second.value() });
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RBRACE));
    }

    Fa_VERIFY_TOKEN(TokType::RBRACE, ParserCode::EXPECTED_RBRACE_EXPR);

    ExprPtr first_key = content.empty() ? nullptr : content[0].first;
    if (content.empty())
        return AST::Fa_makeDict(std::move(content), start->location());
    return AST::Fa_makeDict(std::move(content), first_key->get_location());
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_conditional_expr()
{
    return parse_logical_expr();
    /// TODO: Ternary?
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_logical_expr()
{
    return parse_logical_expr_precedence(0);
}

bool Fa_Parser::match(TokType const type)
{
    if (check(type)) {
        advance();
        return true;
    }

    return false;
}

void Fa_Parser::synchronize()
{
    while (!we_done()) {
        if (check(TokType::NEWLINE) || check(TokType::DEDENT)) {
            advance();
            return;
        }

        if (check(TokType::KW_IF) || check(TokType::KW_WHILE) || check(TokType::KW_FOR)
            || check(TokType::KW_RETURN) || check(TokType::KW_BREAK) || check(TokType::KW_CONTINUE) || check(TokType::KW_FN))
            return;

        advance();
    }
}

} // namespace fairuz::parser
