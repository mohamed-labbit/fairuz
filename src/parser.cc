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

static bool is_valid_node(AST::Fa_Expr const* e) { return e->get_kind() != AST::Fa_Expr::Kind::INVALID; }
static bool is_valid_node(AST::Fa_Stmt const* s) { return s->get_kind() != AST::Fa_Stmt::Kind::INVALID; }

static bool stmt_definitely_returns(AST::Fa_Stmt const* stmt)
{
    if (!stmt)
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
Fa_SymbolTable::get_symbols() const
{
    return m_symbols;
}

// semantic analyzer

typename Fa_SymbolTable::DataType_t Fa_SemanticAnalyzer::infer_type(AST::Fa_Expr const* m_expr)
{
    if (!m_expr)
        return Fa_SymbolTable::DataType_t::UNKNOWN;

    switch (m_expr->get_kind()) {
    case AST::Fa_Expr::Kind::LITERAL: {
        auto lit = static_cast<AST::Fa_LiteralExpr const*>(m_expr);

        if (lit->is_string())
            return Fa_SymbolTable::DataType_t::STRING;
        if (lit->is_float())
            return Fa_SymbolTable::DataType_t::FLOAT;
        if (lit->is_integer())
            return Fa_SymbolTable::DataType_t::INTEGER;
        if (lit->is_bool())
            return Fa_SymbolTable::DataType_t::BOOLEAN;

        return Fa_SymbolTable::DataType_t::NONE;
    }

    case AST::Fa_Expr::Kind::NAME: {
        auto m_name = static_cast<AST::Fa_NameExpr const*>(m_expr);
        Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(m_name->get_value());
        if (sym)
            return sym->data_type;
    } break;

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = static_cast<AST::Fa_BinaryExpr const*>(m_expr);

        Fa_SymbolTable::DataType_t left_type = infer_type(bin->get_left());
        Fa_SymbolTable::DataType_t right_type = infer_type(bin->get_right());

        // Type promotion rules
        if (left_type == Fa_SymbolTable::DataType_t::FLOAT || right_type == Fa_SymbolTable::DataType_t::FLOAT)
            return Fa_SymbolTable::DataType_t::FLOAT;
        if (left_type == Fa_SymbolTable::DataType_t::INTEGER && right_type == Fa_SymbolTable::DataType_t::INTEGER)
            return Fa_SymbolTable::DataType_t::INTEGER;
        if (bin->get_operator() == AST::Fa_BinaryOp::OP_ADD && left_type == Fa_SymbolTable::DataType_t::STRING)
            return Fa_SymbolTable::DataType_t::STRING;
        if (bin->get_operator() == AST::Fa_BinaryOp::OP_AND || bin->get_operator() == AST::Fa_BinaryOp::OP_OR)
            return Fa_SymbolTable::DataType_t::BOOLEAN;
    } break;

    case AST::Fa_Expr::Kind::LIST:
        return Fa_SymbolTable::DataType_t::LIST;
    case AST::Fa_Expr::Kind::CALL:
        return Fa_SymbolTable::DataType_t::ANY;

    default:
        break;
    }

    return Fa_SymbolTable::DataType_t::UNKNOWN;
}

void Fa_SemanticAnalyzer::report_issue(Issue::Severity sev, Fa_StringRef msg, i32 m_line, Fa_StringRef const& sugg)
{
    m_issues.push({ sev, msg, m_line, sugg });
}

void Fa_SemanticAnalyzer::analyze_fa_expr(AST::Fa_Expr const* m_expr)
{
    if (!m_expr)
        return;

    switch (m_expr->get_kind()) {
    case AST::Fa_Expr::Kind::NAME: {
        auto m_name = static_cast<AST::Fa_NameExpr const*>(m_expr);

        if (!m_current_scope->is_defined(m_name->get_value()))
            report_issue(Issue::Severity::ERROR, "Undefined variable: " + m_name->get_value(), m_expr->get_line(), "Did you forget to initialize it?");
        else
            m_current_scope->mark_used(m_name->get_value(), m_expr->get_line());
    } break;

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = static_cast<AST::Fa_BinaryExpr const*>(m_expr);

        analyze_fa_expr(bin->get_left());
        analyze_fa_expr(bin->get_right());

        Fa_SymbolTable::DataType_t left_type = infer_type(bin->get_left());
        Fa_SymbolTable::DataType_t right_type = infer_type(bin->get_right());

        if (left_type != right_type && left_type != Fa_SymbolTable::DataType_t::UNKNOWN && right_type != Fa_SymbolTable::DataType_t::UNKNOWN)
            report_issue(Issue::Severity::ERROR, "Type mismatch in binary Fa_Expression", m_expr->get_line(), "Left and right operands must have same type");

        if (left_type == Fa_SymbolTable::DataType_t::STRING || right_type == Fa_SymbolTable::DataType_t::STRING) {
            if (bin->get_operator() != AST::Fa_BinaryOp::OP_ADD)
                report_issue(Issue::Severity::ERROR, "Invalid operation on string", m_expr->get_line(), "Only '+' is allowed for strings");
        }

        if (bin->get_operator() == AST::Fa_BinaryOp::OP_DIV && bin->get_right()->get_kind() == AST::Fa_Expr::Kind::LITERAL) {
            auto lit = static_cast<AST::Fa_LiteralExpr const*>(bin->get_right());
            if (lit->is_numeric() && lit->is_number() == 0)
                report_issue(Issue::Severity::ERROR, "Division by zero", m_expr->get_line(), "This will cause a runtime error");
        }
    } break;

    case AST::Fa_Expr::Kind::UNARY: {
        auto un = static_cast<AST::Fa_UnaryExpr const*>(m_expr);
        analyze_fa_expr(un->get_operand());
    } break;

    case AST::Fa_Expr::Kind::CALL: {
        auto call = static_cast<AST::Fa_CallExpr const*>(m_expr);
        analyze_fa_expr(call->get_callee());

        for (AST::Fa_Expr const* const& arg : call->get_args())
            analyze_fa_expr(arg);

        if (call->get_callee()->get_kind() == AST::Fa_Expr::Kind::NAME) {
            auto m_name = static_cast<AST::Fa_NameExpr const*>(call->get_callee());

            if (Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(m_name->get_value())) {
                if (sym->symbol_type != Fa_SymbolTable::SymbolType::FUNCTION)
                    report_issue(Issue::Severity::ERROR, "'" + m_name->get_value() + "' is not callable", m_expr->get_line());
            }
        }

        if (call->get_callee()->get_kind() == AST::Fa_Expr::Kind::NAME) {
            auto m_name = static_cast<AST::Fa_NameExpr const*>(call->get_callee());
            Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(m_name->get_value());

            if (!sym)
                report_issue(Issue::Severity::ERROR, "Undefined function: " + m_name->get_value(), m_expr->get_line());
            else if (sym->symbol_type != Fa_SymbolTable::SymbolType::FUNCTION)
                report_issue(Issue::Severity::ERROR, "'" + m_name->get_value() + "' is not callable", m_expr->get_line());
        }
    } break;

    case AST::Fa_Expr::Kind::LIST: {
        auto list = static_cast<AST::Fa_ListExpr const*>(m_expr);
        for (AST::Fa_Expr const* const& elem : list->get_elements())
            analyze_fa_expr(elem);
    } break;

    case AST::Fa_Expr::Kind::INDEX: {
        auto idx = static_cast<AST::Fa_IndexExpr const*>(m_expr);
        analyze_fa_expr(idx->get_object());
        analyze_fa_expr(idx->get_index());
    } break;

    default:
        break;
    }
}

void Fa_SemanticAnalyzer::analyze_stmt(AST::Fa_Stmt const* stmt)
{
    if (!stmt)
        return;

    switch (stmt->get_kind()) {
    case AST::Fa_Stmt::Kind::ASSIGNMENT: {
        auto assign = static_cast<AST::Fa_AssignmentStmt const*>(stmt);
        analyze_fa_expr(assign->get_value());

        AST::Fa_Expr* m_target = assign->get_target();
        assert(m_target);

        if (m_target->get_kind() == AST::Fa_Expr::Kind::INDEX) {
            analyze_fa_expr(m_target);
            break;
        }

        if (m_target->get_kind() == AST::Fa_Expr::Kind::NAME) {
            Fa_StringRef target_name = static_cast<AST::Fa_NameExpr*>(m_target)->get_value();

            if (!m_current_scope->lookup_local(target_name)) {
                // First assignment in this scope — declare it
                Fa_SymbolTable::Symbol sym;
                sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
                sym.data_type = infer_type(assign->get_value());
                sym.definition_line = stmt->get_line();
                m_current_scope->define(target_name, sym);
            } else {
                m_current_scope->mark_used(target_name, stmt->get_line());
            }
        }
    } break;

    case AST::Fa_Stmt::Kind::EXPR: {
        auto Fa_Expr_stmt = static_cast<AST::Fa_ExprStmt const*>(stmt);
        analyze_fa_expr(Fa_Expr_stmt->get_expr());
        if (Fa_Expr_stmt->get_expr()->get_kind() != AST::Fa_Expr::Kind::CALL)
            report_issue(Issue::Severity::INFO, "Fa_Expression result not used", stmt->get_line());
    } break;

    case AST::Fa_Stmt::Kind::IF: {
        auto if_stmt = static_cast<AST::Fa_IfStmt const*>(stmt);
        analyze_fa_expr(if_stmt->get_condition());

        AST::Fa_Stmt const* then_block = if_stmt->get_then();
        AST::Fa_Stmt const* else_block = if_stmt->get_else();

        if (if_stmt->get_condition()->get_kind() == AST::Fa_Expr::Kind::LITERAL)
            report_issue(Issue::Severity::WARNING, "Condition is always constant", stmt->get_line(), "Consider removing if statement");

        if (then_block)
            analyze_stmt(then_block);
        if (else_block)
            analyze_stmt(else_block);
    } break;

    case AST::Fa_Stmt::Kind::WHILE: {
        auto while_stmt = static_cast<AST::Fa_WhileStmt const*>(stmt);
        analyze_fa_expr(while_stmt->get_condition());

        // Detect infinite loops
        if (while_stmt->get_condition()->get_kind() == AST::Fa_Expr::Kind::LITERAL) {
            AST::Fa_LiteralExpr const* lit = static_cast<AST::Fa_LiteralExpr const*>(while_stmt->get_condition());
            if (lit->is_bool() && lit->get_bool() == true)
                report_issue(Issue::Severity::WARNING, "Infinite loop detected", stmt->get_line(), "Add a break condition");
        }

        analyze_stmt(while_stmt->get_body());
    } break;

    case AST::Fa_Stmt::Kind::FOR: {
        auto for_stmt = static_cast<AST::Fa_ForStmt const*>(stmt);
        analyze_fa_expr(for_stmt->get_iter());

        m_current_scope = m_current_scope->create_child();
        Fa_SymbolTable::Symbol loop_var;
        loop_var.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
        loop_var.data_type = Fa_SymbolTable::DataType_t::ANY;
        m_current_scope->define(for_stmt->get_target()->get_value(), loop_var);

        analyze_stmt(for_stmt->get_body());

        if (m_current_scope->m_parent && m_current_scope->m_parent->lookup_local(for_stmt->get_target()->get_value()))
            report_issue(Issue::Severity::WARNING, "Loop variable shadows outer variable", stmt->get_line());

        m_current_scope = m_current_scope->m_parent;
    } break;

    case AST::Fa_Stmt::Kind::FUNC: {
        auto func_def = static_cast<AST::Fa_FunctionDef const*>(stmt);
        Fa_SymbolTable::Symbol func_sym;
        func_sym.symbol_type = Fa_SymbolTable::SymbolType::FUNCTION;
        func_sym.data_type = Fa_SymbolTable::DataType_t::FUNCTION;
        func_sym.definition_line = stmt->get_line();
        m_current_scope->define(func_def->get_name()->get_value(), func_sym);

        m_current_scope = m_current_scope->create_child();

        for (AST::Fa_Expr const* const& param : func_def->get_parameters()) {
            Fa_SymbolTable::Symbol param_sym;
            param_sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
            param_sym.data_type = Fa_SymbolTable::DataType_t::ANY;
            m_current_scope->define(static_cast<AST::Fa_NameExpr const*>(param)->get_value(), param_sym);
        }

        analyze_stmt(func_def->get_body());
        if (!stmt_definitely_returns(func_def->get_body()))
            report_issue(Issue::Severity::INFO, "Function may not return a value", stmt->get_line());
        // Exit function scope
        m_current_scope = m_current_scope->m_parent;
    } break;

    case AST::Fa_Stmt::Kind::RETURN: {
        auto ret = static_cast<AST::Fa_ReturnStmt const*>(stmt);
        analyze_fa_expr(ret->get_value());
    } break;

    default:
        break;
    }
}

Fa_SemanticAnalyzer::Fa_SemanticAnalyzer()
{
    m_global_scope = get_allocator().allocate_object<Fa_SymbolTable>();
    m_current_scope = m_global_scope;

    Fa_SymbolTable::Symbol print_sym;
    print_sym.m_name = "print";
    print_sym.symbol_type = Fa_SymbolTable::SymbolType::FUNCTION;
    print_sym.data_type = Fa_SymbolTable::DataType_t::FUNCTION;
    m_global_scope->define("print", print_sym);
}

void Fa_SemanticAnalyzer::analyze(Fa_Array<AST::Fa_Stmt*> const& m_statements)
{
    for (AST::Fa_Stmt const* const& stmt : m_statements)
        analyze_stmt(stmt);

    Fa_Array<Fa_SymbolTable::Symbol*> unused = m_global_scope->get_unused_symbols();
    for (Fa_SymbolTable::Symbol* sym : unused)
        report_issue(Issue::Severity::WARNING, "Unused variable: " + sym->m_name, sym->definition_line, "Consider removing if not needed");
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
            m_statements.push(stmt.m_value());
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
    if (check(tok::Fa_TokenType::KW_RETURN))
        return parse_return_stmt();
    if (check(tok::Fa_TokenType::KW_FN))
        return parse_function_def();

    return parse_expression_stmt();
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_return_stmt()
{
    if (!consume(tok::Fa_TokenType::KW_RETURN))
        return report_error(ErrorCode::EXPECTED_RETURN);

    if (check(tok::Fa_TokenType::NEWLINE) || we_done())
        return AST::Fa_makeReturn();

    auto ret = parse_expression();
    if (ret.has_error())
        return ret.error();

    return AST::Fa_makeReturn(ret.m_value());
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_while_stmt()
{
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

    return Fa_makeWhile(m_condition.m_value(), static_cast<AST::Fa_BlockStmt*>(while_block.m_value()));
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_indented_block()
{
    if (check(tok::Fa_TokenType::NEWLINE))
        advance();

    if (!consume(tok::Fa_TokenType::INDENT))
        return report_error(ErrorCode::EXPECTED_INDENT);

    Fa_Array<AST::Fa_Stmt*> m_statements;

    if (check(tok::Fa_TokenType::DEDENT)) {
        advance();
        return Fa_makeBlock(m_statements);
    }

    while (!check(tok::Fa_TokenType::DEDENT) && !we_done()) {
        skip_newlines();
        if (check(tok::Fa_TokenType::DEDENT))
            break;

        auto stmt = parse_statement();

        if (stmt.has_value())
            m_statements.push(stmt.m_value());
        else {
            synchronize();
            if (check(tok::Fa_TokenType::DEDENT) || we_done())
                break;
        }
    }

    if (check(tok::Fa_TokenType::ENDMARKER))
        return Fa_makeBlock(m_statements);
    else if (!consume(tok::Fa_TokenType::DEDENT))
        return report_error(ErrorCode::EXPECTED_DEDENT);

    return Fa_makeBlock(m_statements);
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

            Fa_StringRef param_name = current_token()->lexeme();
            advance();
            parameters.push(AST::Fa_makeName(param_name));
            skip_newlines();
        } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RPAREN));
    }

    if (!consume(tok::Fa_TokenType::RPAREN))
        return report_error(ErrorCode::EXPECTED_RPAREN_EXPR);

    if (parameters.empty())
        return Fa_makeList(Fa_Array<AST::Fa_Expr*> { });

    return Fa_makeList(parameters);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_expression() { return parse_assignment_expr(); }

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_function_def()
{
    if (!consume(tok::Fa_TokenType::KW_FN))
        return report_error(ErrorCode::EXPECTED_FN_KEYWORD);

    if (!check(tok::Fa_TokenType::IDENTIFIER))
        return report_error(ErrorCode::EXPECTED_FN_NAME);

    Fa_StringRef function_name = current_token()->lexeme();
    advance();

    auto parameters_list = parse_parameters_list();
    if (parameters_list.has_error())
        return parameters_list.error();

    if (!consume(tok::Fa_TokenType::COLON))
        return report_error(ErrorCode::EXPECTED_COLON_FN);

    auto function_body = parse_indented_block();
    if (function_body.has_error())
        return function_body.error();

    return Fa_makeFunction(AST::Fa_makeName(function_name), static_cast<AST::Fa_ListExpr*>(parameters_list.m_value()), static_cast<AST::Fa_BlockStmt*>(function_body.m_value()));
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_if_stmt()
{
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
            else_block = nested.m_value();
        } else {
            if (!consume(tok::Fa_TokenType::COLON))
                return report_error(ErrorCode::EXPECTED_COLON_IF);
            auto m_else_stmt = parse_indented_block();
            if (m_else_stmt.has_error())
                return m_else_stmt.error();
            else_block = m_else_stmt.m_value();
        }
    }

    return Fa_makeIf(
        m_condition.m_value(),
        static_cast<AST::Fa_BlockStmt*>(then_block.m_value()),
        else_block);
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parse_expression_stmt()
{
    auto m_expr = parse_expression();
    if (m_expr.has_error())
        return m_expr.error();

    return Fa_makeExprStmt(m_expr.m_value());
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_assignment_expr()
{
    auto L = parse_conditional_expr();
    if (L.has_error())
        return L.error();

    if (check(tok::Fa_TokenType::OP_ASSIGN)) {
        AST::Fa_Expr* m_target = L.m_value();
        AST::Fa_Expr::Kind kind = m_target->get_kind();

        if (kind != AST::Fa_Expr::Kind::NAME && kind != AST::Fa_Expr::Kind::INDEX)
            return report_error(ErrorCode::INVALID_ASSIGN_TARGET);

        advance();

        auto R = parse_assignment_expr();
        if (R.has_error())
            return R.error();

        if (kind == AST::Fa_Expr::Kind::NAME) {
            auto name_target = static_cast<AST::Fa_NameExpr*>(m_target);
            bool decl = !m_sema.get_current_scope()->is_defined(name_target->get_value());
            return Fa_ErrorOr<AST::Fa_Expr*>::from_value(Fa_makeAssignmentExpr(name_target, R.m_value(), decl));
        }
        return Fa_ErrorOr<AST::Fa_Expr*>::from_value(Fa_makeAssignmentExpr(m_target, R.m_value(), /*decl=*/false));
    }

    return L.m_value();
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

        L.set_value(Fa_makeBinary(L.m_value(), R.m_value(), to_binary_op(op)));
    }

    return L.m_value();
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

        L.set_value(Fa_makeBinary(L.m_value(), R.m_value(), to_binary_op(op)));
    }

    return L.m_value();
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

        L.set_value(Fa_makeBinary(L.m_value(), R.m_value(), to_binary_op(op)));
    }

    return L.m_value();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_unary_expr()
{
    tok::Fa_Token const* tok = current_token();
    if (tok->is_unary_op()) {
        tok::Fa_TokenType op = m_lexer.m_current()->type();
        advance();

        auto m_expr = parse_unary_expr();
        if (m_expr.has_error())
            return m_expr.error();

        return Fa_makeUnary(m_expr.m_value(), to_unary_op(op));
    }
    return parse_postfix_expr();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_postfix_expr()
{
    auto expr_or = parse_primary_expr();
    if (expr_or.has_error())
        return expr_or.error();

    AST::Fa_Expr* m_expr = expr_or.m_value();

    for (;;) {
        if (check(tok::Fa_TokenType::LPAREN)) {
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

                    m_args.push(arg.m_value());
                    skip_newlines();
                } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RPAREN));
            }

            if (!consume(tok::Fa_TokenType::RPAREN))
                return report_error(ErrorCode::EXPECTED_RPAREN_EXPR);

            m_expr = Fa_makeCall(m_expr, Fa_makeList(std::move(m_args)));
            continue;
        }

        // dict
        if (check(tok::Fa_TokenType::LBRACE)) {
            advance();

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

                    content.push({ first.m_value(), second.m_value() });
                } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RBRACE));
            }

            if (!consume(tok::Fa_TokenType::RBRACE))
                return report_error(ErrorCode::EXPECTED_RBRACE_EXPR);

            m_expr = AST::Fa_makeDict(content);
            continue;
        }

        if (check(tok::Fa_TokenType::LBRACKET)) {
            advance();

            auto m_index = parse_expression();
            if (m_index.has_error())
                return m_index.error();

            if (!consume(tok::Fa_TokenType::RBRACKET))
                return report_error(ErrorCode::EXPECTED_RBRACKET);

            m_expr = Fa_makeIndex(m_expr, m_index.m_value());
            continue;
        }

        break;
    }

    return m_expr;
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
            return AST::Fa_makeLiteralFloat(v.to_double());

        if (tt == tok::Fa_TokenType::INTEGER || tt == tok::Fa_TokenType::HEX || tt == tok::Fa_TokenType::OCTAL || tt == tok::Fa_TokenType::BINARY) {
            static constexpr int BASE_FOR_TYPE[] = { 2, 8, 10, 16 };
            return AST::Fa_makeLiteralInt(util::parse_integer_literal(v,
                /*base=*/BASE_FOR_TYPE[static_cast<int>(tt) - static_cast<int>(tok::Fa_TokenType::BINARY)]));
        }
    }

    if (check(tok::Fa_TokenType::STRING)) {
        Fa_StringRef v = tok->lexeme();
        advance();
        return AST::Fa_makeLiteralString(v);
    }

    if (check(tok::Fa_TokenType::KW_TRUE) || check(tok::Fa_TokenType::KW_FALSE)) {
        Fa_StringRef v = tok->lexeme();
        advance();
        return AST::Fa_makeLiteralBool(v == "صحيح" ? true : false);
    }

    if (check(tok::Fa_TokenType::KW_NONE)) {
        advance();
        return AST::Fa_makeLiteralNil();
    }

    if (check(tok::Fa_TokenType::IDENTIFIER)) {
        Fa_StringRef m_name = tok->lexeme();
        advance();
        return AST::Fa_makeName(m_name);
    }

    if (check(tok::Fa_TokenType::LPAREN)) {
        advance();
        if (check(tok::Fa_TokenType::RPAREN)) {
            advance();
            return Fa_makeList(Fa_Array<AST::Fa_Expr*> { });
        }

        auto m_expr = parse_expression();

        if (m_expr.has_error())
            return m_expr.error();

        if (!consume(tok::Fa_TokenType::RPAREN))
            return report_error(ErrorCode::EXPECTED_RPAREN_EXPR);

        return m_expr.m_value();
    }

    if (check(tok::Fa_TokenType::LBRACKET)) {
        advance();
        return parse_list_literal();
    }

    if (we_done())
        return report_error(ErrorCode::UNEXPECTED_EOF);

    skip_newlines();
    return report_error(ErrorCode::UNEXPECTED_TOKEN);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse_list_literal()
{
    Fa_Array<AST::Fa_Expr*> m_elements = Fa_Array<AST::Fa_Expr*>::with_capacity(4);

    if (!check(tok::Fa_TokenType::RBRACKET)) {
        do {
            skip_newlines();
            if (check(tok::Fa_TokenType::RBRACKET))
                break;

            auto elem = parse_expression();
            if (elem.has_error())
                return elem.error();

            m_elements.push(elem.m_value());
            skip_newlines();
        } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RBRACKET));
    }

    if (!consume(tok::Fa_TokenType::RBRACKET))
        return report_error(ErrorCode::EXPECTED_RBRACKET);

    return Fa_makeList(std::move(m_elements));
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

        if (check(tok::Fa_TokenType::KW_IF) || check(tok::Fa_TokenType::KW_WHILE) || check(tok::Fa_TokenType::KW_RETURN) || check(tok::Fa_TokenType::KW_FN))
            return;

        advance();
    }
}

} // namespace fairuz::parser
