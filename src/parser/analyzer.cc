#include "../../include/parser/analyzer.hpp"

#include <iostream>

namespace mylang {
namespace parser {

// semantic analyzer

// Type inference engine
typename SymbolTable::DataType_t SemanticAnalyzer::inferType(ast::Expr const* expr)
{
    if (!expr)
        return SymbolTable::DataType_t::UNKNOWN;

    switch (expr->getKind()) {
    case ast::Expr::Kind::LITERAL: {
        ast::LiteralExpr const* lit = static_cast<ast::LiteralExpr const*>(expr);

        switch (lit->getType()) {
        case ast::LiteralExpr::Type::NUMBER:
            return lit->getValue().find('.') ? SymbolTable::DataType_t::FLOAT : SymbolTable::DataType_t::INTEGER;
        case ast::LiteralExpr::Type::STRING:
            return SymbolTable::DataType_t::STRING;
        case ast::LiteralExpr::Type::BOOLEAN:
            return SymbolTable::DataType_t::BOOLEAN;
        case ast::LiteralExpr::Type::NONE:
            return SymbolTable::DataType_t::NONE;
        }

        break;
    }

    case ast::Expr::Kind::NAME: {
        ast::NameExpr const* name = static_cast<ast::NameExpr const*>(expr);
        SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue());
        if (sym)
            return sym->DataType;
        break;
    }

    case ast::Expr::Kind::BINARY: {
        ast::BinaryExpr const* bin = static_cast<ast::BinaryExpr const*>(expr);

        SymbolTable::DataType_t leftType = inferType(bin->getLeft());
        SymbolTable::DataType_t rightType = inferType(bin->getRight());

        // Type promotion rules
        if (leftType == SymbolTable::DataType_t::FLOAT || rightType == SymbolTable::DataType_t::FLOAT)
            return SymbolTable::DataType_t::FLOAT;

        if (leftType == SymbolTable::DataType_t::INTEGER && rightType == SymbolTable::DataType_t::INTEGER)
            return SymbolTable::DataType_t::INTEGER;

        if (bin->getOperator() == tok::TokenType::OP_PLUS && leftType == SymbolTable::DataType_t::STRING)
            return SymbolTable::DataType_t::STRING;

        if (bin->getOperator() == tok::TokenType::KW_AND || bin->getOperator() == tok::TokenType::KW_OR)
            return SymbolTable::DataType_t::BOOLEAN;

        break;
    }

    case ast::Expr::Kind::LIST:
        return SymbolTable::DataType_t::LIST;

    case ast::Expr::Kind::CALL:
        return SymbolTable::DataType_t::ANY;

    default:
        break;
    }

    return SymbolTable::DataType_t::UNKNOWN;
}

void SemanticAnalyzer::reportIssue(Issue::Severity sev, StringRef const& msg, std::int32_t line, StringRef const& sugg)
{
    Issues_.push_back({ sev, msg, line, sugg });
}

void SemanticAnalyzer::analyzeExpr(ast::Expr const* expr)
{
    if (!expr)
        return;

    switch (expr->getKind()) {
    case ast::Expr::Kind::NAME: {
        ast::NameExpr const* name = static_cast<ast::NameExpr const*>(expr);
        if (!CurrentScope_->isDefined(name->getValue()))
            reportIssue(Issue::Severity::ERROR, u"Undefined variable: " + name->getValue(), expr->getLine(),
                u"Did you forget to initialize it?");
        else
            CurrentScope_->markUsed(name->getValue(), expr->getLine());
        break;
    }

    case ast::Expr::Kind::BINARY: {
        ast::BinaryExpr const* bin = static_cast<ast::BinaryExpr const*>(expr);

        analyzeExpr(bin->getLeft());
        analyzeExpr(bin->getRight());

        // Type compatibility checking
        SymbolTable::DataType_t leftType = inferType(bin->getLeft());
        SymbolTable::DataType_t rightType = inferType(bin->getRight());

        if (leftType != rightType && leftType != SymbolTable::DataType_t::UNKNOWN
            && rightType != SymbolTable::DataType_t::UNKNOWN)
            reportIssue(Issue::Severity::ERROR, u"Type mismatch in binary expression", expr->getLine(),
                u"Left and right operands must have same type");

        if (leftType == SymbolTable::DataType_t::STRING || rightType == SymbolTable::DataType_t::STRING) {
            if (bin->getOperator() != tok::TokenType::OP_PLUS)
                reportIssue(Issue::Severity::ERROR, u"Invalid operation on string", expr->getLine(),
                    u"Only '+' is allowed for strings");
        }

        // Division by zero detection (constant folding)
        if (bin->getOperator() == tok::TokenType::OP_SLASH && bin->getRight()->getKind() == ast::Expr::Kind::LITERAL) {
            ast::LiteralExpr const* lit = static_cast<ast::LiteralExpr const*>(bin->getRight());
            if (lit->isNumeric() && lit->toNumber() == 0)
                reportIssue(Issue::Severity::ERROR, u"Division by zero", expr->getLine(), u"This will cause a runtime error");
        }

        break;
    }

    case ast::Expr::Kind::UNARY: {
        ast::UnaryExpr const* un = static_cast<ast::UnaryExpr const*>(expr);
        analyzeExpr(un->getOperand());
        break;
    }

    case ast::Expr::Kind::CALL: {
        ast::CallExpr const* call = static_cast<ast::CallExpr const*>(expr);
        analyzeExpr(call->getCallee());

        for (ast::Expr const* const& arg : call->getArgs())
            analyzeExpr(arg);

        // Check if calling undefined function
        if (call->getCallee()->getKind() == ast::Expr::Kind::NAME) {
            ast::NameExpr const* name = static_cast<ast::NameExpr const*>(call->getCallee());

            if (SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue()))
                if (sym->SymbolType != SymbolTable::SymbolType::FUNCTION)
                    reportIssue(Issue::Severity::ERROR, u"'" + name->getValue() + u"' is not callable", expr->getLine());
        }

        if (call->getCallee()->getKind() == ast::Expr::Kind::NAME) {
            ast::NameExpr const* name = static_cast<ast::NameExpr const*>(call->getCallee());

            SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue());

            if (!sym)
                reportIssue(Issue::Severity::ERROR, u"Undefined function: " + name->getValue(), expr->getLine());
            else if (sym->SymbolType != SymbolTable::SymbolType::FUNCTION)
                reportIssue(Issue::Severity::ERROR, u"'" + name->getValue() + u"' is not callable", expr->getLine());
        }
        break;
    }

    case ast::Expr::Kind::LIST: {
        ast::ListExpr const* list = static_cast<ast::ListExpr const*>(expr);
        for (ast::Expr const* const& elem : list->getElements())
            analyzeExpr(elem);
        break;
    }

    default:
        break;
    }
}

void SemanticAnalyzer::analyzeStmt(ast::Stmt const* stmt)
{
    if (!stmt)
        return;

    switch (stmt->getKind()) {
    case ast::Stmt::Kind::ASSIGNMENT: {
        ast::AssignmentStmt const* assign = static_cast<ast::AssignmentStmt const*>(stmt);
        analyzeExpr(assign->getValue());

        SymbolTable::DataType_t type = inferType(assign->getValue());
        SymbolTable::Symbol sym;
        sym.SymbolType = SymbolTable::SymbolType::VARIABLE;
        sym.DataType = type;
        sym.DefinitionLine = stmt->getLine();

        ast::Expr* target = assign->getTarget();
        assert(target);
        StringRef target_name = "";

        /// TODO: check other type of target expressions
        if (target->getKind() == ast::Expr::Kind::NAME)
            target_name = dynamic_cast<ast::NameExpr*>(target)->getValue();

        CurrentScope_->define(target_name, sym);
        break;
    }

    case ast::Stmt::Kind::EXPR: {
        ast::ExprStmt const* exprStmt = static_cast<ast::ExprStmt const*>(stmt);
        analyzeExpr(exprStmt->getExpr());
        // Warn about unused expression results
        if (exprStmt->getExpr()->getKind() != ast::Expr::Kind::CALL)
            reportIssue(Issue::Severity::INFO, u"Expression result not used", stmt->getLine());
        break;
    }

    case ast::Stmt::Kind::IF: {
        ast::IfStmt const* ifStmt = static_cast<ast::IfStmt const*>(stmt);
        analyzeExpr(ifStmt->getCondition());
        // Check for constant conditions
        if (ifStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL)
            reportIssue(Issue::Severity::WARNING, u"Condition is always constant", stmt->getLine(),
                u"Consider removing if statement");
        for (ast::Stmt const* const& s : ifStmt->getThenBlock()->getStatements())
            analyzeStmt(s);
        for (ast::Stmt const* const& s : ifStmt->getElseBlock()->getStatements())
            analyzeStmt(s);
        break;
    }

    case ast::Stmt::Kind::WHILE: {
        ast::WhileStmt const* whileStmt = static_cast<ast::WhileStmt const*>(stmt);
        analyzeExpr(whileStmt->getCondition());

        // Detect infinite loops
        if (whileStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL) {
            ast::LiteralExpr const* lit = static_cast<ast::LiteralExpr const*>(whileStmt->getCondition());
            if (lit->getType() == ast::LiteralExpr::Type::BOOLEAN && lit->getValue() == u"true")
                reportIssue(Issue::Severity::WARNING, u"Infinite loop detected", stmt->getLine(), u"Add a break condition");
        }

        for (ast::Stmt const* const& s : whileStmt->getBlock()->getStatements())
            analyzeStmt(s);

        break;
    }

    case ast::Stmt::Kind::FOR: {
        ast::ForStmt const* forStmt = static_cast<ast::ForStmt const*>(stmt);
        analyzeExpr(forStmt->getIter());

        // Create new scope for loop variable
        CurrentScope_ = CurrentScope_->createChild();
        SymbolTable::Symbol loopVar;
        loopVar.SymbolType = SymbolTable::SymbolType::VARIABLE;
        loopVar.DataType = SymbolTable::DataType_t::ANY;
        CurrentScope_->define(forStmt->getTarget()->getValue(), loopVar);

        for (ast::Stmt const* const& s : forStmt->getBlock()->getStatements())
            analyzeStmt(s);

        // Check if loop variable is shadowing
        if (CurrentScope_->Parent_ && CurrentScope_->Parent_->lookupLocal(forStmt->getTarget()->getValue()))
            reportIssue(Issue::Severity::WARNING, u"Loop variable shadows outer variable", stmt->getLine());

        // Exit loop scope
        CurrentScope_ = CurrentScope_->Parent_;
        break;
    }

    case ast::Stmt::Kind::FUNC: {
        ast::FunctionDef const* funcDef = static_cast<ast::FunctionDef const*>(stmt);
        SymbolTable::Symbol funcSym;
        funcSym.SymbolType = SymbolTable::SymbolType::FUNCTION;
        funcSym.DataType = SymbolTable::DataType_t::FUNCTION;
        funcSym.DefinitionLine = stmt->getLine();
        CurrentScope_->define(funcDef->getName()->getValue(), funcSym);

        // Create function scope
        CurrentScope_ = CurrentScope_->createChild();

        for (ast::Expr const* const& param : funcDef->getParameters()) {
            SymbolTable::Symbol paramSym;
            paramSym.SymbolType = SymbolTable::SymbolType::VARIABLE;
            paramSym.DataType = SymbolTable::DataType_t::ANY;
            CurrentScope_->define(static_cast<ast::NameExpr const*>(param)->getValue(), paramSym);
        }

        for (ast::Stmt const* const& s : funcDef->getBody()->getStatements())
            analyzeStmt(s);

        // Check for missing return statement
        bool hasReturn = false;

        for (ast::Stmt const* const& s : funcDef->getBody()->getStatements()) {
            if (s->getKind() == ast::Stmt::Kind::RETURN) {
                hasReturn = true;
                break;
            }
        }

        if (!hasReturn)
            reportIssue(Issue::Severity::INFO, u"Function may not return a value", stmt->getLine());

        // Exit function scope
        CurrentScope_ = CurrentScope_->Parent_;
        break;
    }

    case ast::Stmt::Kind::RETURN: {
        ast::ReturnStmt const* ret = static_cast<ast::ReturnStmt const*>(stmt);
        analyzeExpr(ret->getValue());
        break;
    }

    default:
        break;
    }
}

SemanticAnalyzer::SemanticAnalyzer()
{
    GlobalScope_ = std::make_unique<SymbolTable>();
    CurrentScope_ = GlobalScope_.get();

    // Add built-in functions
    SymbolTable::Symbol printSym;
    printSym.name = u"print";
    printSym.SymbolType = SymbolTable::SymbolType::FUNCTION;
    printSym.DataType = SymbolTable::DataType_t::FUNCTION;
    GlobalScope_->define(u"print", printSym);
}

void SemanticAnalyzer::analyze(std::vector<ast::Stmt*> const& Statements_)
{
    for (ast::Stmt const* const& stmt : Statements_)
        analyzeStmt(stmt);

    // Check for unused variables
    std::vector<SymbolTable::Symbol*> unused = GlobalScope_->getUnusedSymbols();
    for (SymbolTable::Symbol* sym : unused)
        reportIssue(Issue::Severity::WARNING, u"Unused variable: " + sym->name, sym->DefinitionLine,
            u"Consider removing if not needed");
}

std::vector<typename SemanticAnalyzer::Issue> const& SemanticAnalyzer::getIssues() const { return Issues_; }

SymbolTable const* SemanticAnalyzer::getGlobalScope() const { return GlobalScope_.get(); }

void SemanticAnalyzer::printReport() const
{
    if (Issues_.empty()) {
        std::cout << "✓ No issues found\n";
        return;
    }

    std::cout << "Found " << Issues_.size() << " issue(s):\n\n";
    for (SemanticAnalyzer::Issue const& issue : Issues_) {
        StringRef sevStr;
        switch (issue.severity) {
        case Issue::Severity::ERROR:
            sevStr = u"ERROR";
            break;
        case Issue::Severity::WARNING:
            sevStr = u"WARNING";
            break;
        case Issue::Severity::INFO:
            sevStr = u"INFO";
            break;
        }

        std::cout << "[" << sevStr << "] Line " << issue.line << ": " << issue.message << "\n";

        if (!issue.suggestion.empty())
            std::cout << "  → " << issue.suggestion << "\n";

        std::cout << "\n";
    }
}

}
}
