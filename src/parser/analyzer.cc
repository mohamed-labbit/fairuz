#include "../../include/parser/analyzer.hpp"

#include <iostream>

namespace mylang {
namespace parser {

using namespace ast;

// semantic analyzer

// Type inference engine
typename SymbolTable::DataType_t SemanticAnalyzer::inferType(Expr const* expr)
{
    if (!expr)
        return SymbolTable::DataType_t::UNKNOWN;

    switch (expr->getKind()) {
    case Expr::Kind::LITERAL: {
        LiteralExpr const* lit = static_cast<LiteralExpr const*>(expr);

        if (lit->isString())
            return SymbolTable::DataType_t::STRING;
        if (lit->isDecimal())
            return SymbolTable::DataType_t::FLOAT;
        if (lit->isInteger())
            return SymbolTable::DataType_t::INTEGER;
        if (lit->isBoolean())
            return SymbolTable::DataType_t::BOOLEAN;
        else
            return SymbolTable::DataType_t::NONE;

        break;
    }

    case Expr::Kind::NAME: {
        NameExpr const* name = static_cast<NameExpr const*>(expr);
        SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue());
        if (sym)
            return sym->dataType;

        break;
    }

    case Expr::Kind::BINARY: {
        BinaryExpr const* bin = static_cast<BinaryExpr const*>(expr);

        SymbolTable::DataType_t leftType = inferType(bin->getLeft());
        SymbolTable::DataType_t rightType = inferType(bin->getRight());

        // Type promotion rules
        if (leftType == SymbolTable::DataType_t::FLOAT || rightType == SymbolTable::DataType_t::FLOAT)
            return SymbolTable::DataType_t::FLOAT;

        if (leftType == SymbolTable::DataType_t::INTEGER && rightType == SymbolTable::DataType_t::INTEGER)
            return SymbolTable::DataType_t::INTEGER;

        if (bin->getOperator() == BinaryOp::OP_ADD && leftType == SymbolTable::DataType_t::STRING)
            return SymbolTable::DataType_t::STRING;

        if (bin->getOperator() == BinaryOp::OP_AND || bin->getOperator() == BinaryOp::OP_OR)
            return SymbolTable::DataType_t::BOOLEAN;

        break;
    }

    case Expr::Kind::LIST:
        return SymbolTable::DataType_t::LIST;

    case Expr::Kind::CALL:
        return SymbolTable::DataType_t::ANY;

    default:
        break;
    }

    return SymbolTable::DataType_t::UNKNOWN;
}

void SemanticAnalyzer::reportIssue(Issue::Severity sev, StringRef const& msg, int32_t line, StringRef const& sugg)
{
    Issues_.push_back({ sev, msg, line, sugg });
}

void SemanticAnalyzer::analyzeExpr(Expr const* expr)
{
    if (!expr)
        return;

    switch (expr->getKind()) {
    case Expr::Kind::NAME: {
        NameExpr const* name = static_cast<NameExpr const*>(expr);

        if (!CurrentScope_->isDefined(name->getValue()))
            reportIssue(Issue::Severity::ERROR, "Undefined variable: " + name->getValue(), expr->getLine(), "Did you forget to initialize it?");
        else
            CurrentScope_->markUsed(name->getValue(), expr->getLine());

        break;
    }

    case Expr::Kind::BINARY: {
        BinaryExpr const* bin = static_cast<BinaryExpr const*>(expr);

        analyzeExpr(bin->getLeft());
        analyzeExpr(bin->getRight());

        // Type compatibility checking
        SymbolTable::DataType_t leftType = inferType(bin->getLeft());
        SymbolTable::DataType_t rightType = inferType(bin->getRight());

        if (leftType != rightType && leftType != SymbolTable::DataType_t::UNKNOWN && rightType != SymbolTable::DataType_t::UNKNOWN)
            reportIssue(
                Issue::Severity::ERROR, "Type mismatch in binary expression", expr->getLine(), "Left and right operands must have same type");

        if (leftType == SymbolTable::DataType_t::STRING || rightType == SymbolTable::DataType_t::STRING) {
            if (bin->getOperator() != BinaryOp::OP_ADD)
                reportIssue(Issue::Severity::ERROR, "Invalid operation on string", expr->getLine(), "Only '+' is allowed for strings");
        }

        // Division by zero detection (constant folding)
        if (bin->getOperator() == BinaryOp::OP_DIV && bin->getRight()->getKind() == Expr::Kind::LITERAL) {
            LiteralExpr const* lit = static_cast<LiteralExpr const*>(bin->getRight());
            if (lit->isNumeric() && lit->toNumber() == 0)
                reportIssue(Issue::Severity::ERROR, "Division by zero", expr->getLine(), "This will cause a runtime error");
        }

        break;
    }

    case Expr::Kind::UNARY: {
        UnaryExpr const* un = static_cast<UnaryExpr const*>(expr);
        analyzeExpr(un->getOperand());
        break;
    }

    case Expr::Kind::CALL: {
        CallExpr const* call = static_cast<CallExpr const*>(expr);
        analyzeExpr(call->getCallee());

        for (Expr const* const& arg : call->getArgs())
            analyzeExpr(arg);

        // Check if calling undefined function
        if (call->getCallee()->getKind() == Expr::Kind::NAME) {
            NameExpr const* name = static_cast<NameExpr const*>(call->getCallee());

            if (SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue())) {
                if (sym->symbolType != SymbolTable::SymbolType::FUNCTION)
                    reportIssue(Issue::Severity::ERROR, "'" + name->getValue() + "' is not callable", expr->getLine());
            }
        }

        if (call->getCallee()->getKind() == Expr::Kind::NAME) {
            NameExpr const* name = static_cast<NameExpr const*>(call->getCallee());

            SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue());

            if (!sym)
                reportIssue(Issue::Severity::ERROR, "Undefined function: " + name->getValue(), expr->getLine());
            else if (sym->symbolType != SymbolTable::SymbolType::FUNCTION)
                reportIssue(Issue::Severity::ERROR, "'" + name->getValue() + "' is not callable", expr->getLine());
        }
        break;
    }

    case Expr::Kind::LIST: {
        ListExpr const* list = static_cast<ListExpr const*>(expr);
        for (Expr const* const& elem : list->getElements())
            analyzeExpr(elem);

        break;
    }

    default:
        break;
    }
}

void SemanticAnalyzer::analyzeStmt(Stmt const* stmt)
{
    if (!stmt)
        return;

    switch (stmt->getKind()) {
    case Stmt::Kind::ASSIGNMENT: {
        AssignmentStmt const* assign = static_cast<AssignmentStmt const*>(stmt);
        analyzeExpr(assign->getValue());

        SymbolTable::DataType_t type = inferType(assign->getValue());
        SymbolTable::Symbol sym;
        sym.symbolType = SymbolTable::SymbolType::VARIABLE;
        sym.dataType = type;
        sym.definitionLine = stmt->getLine();

        Expr* target = assign->getTarget();
        assert(target);
        StringRef target_name = "";

        /// TODO: check other type of target expressions
        if (target->getKind() == Expr::Kind::NAME)
            target_name = dynamic_cast<NameExpr*>(target)->getValue();

        CurrentScope_->define(target_name, sym);
        break;
    }

    case Stmt::Kind::EXPR: {
        ExprStmt const* exprStmt = static_cast<ExprStmt const*>(stmt);
        analyzeExpr(exprStmt->getExpr());
        // Warn about unused expression results
        if (exprStmt->getExpr()->getKind() != Expr::Kind::CALL)
            reportIssue(Issue::Severity::INFO, "Expression result not used", stmt->getLine());

        break;
    }

    case Stmt::Kind::IF: {
        IfStmt const* ifStmt = static_cast<IfStmt const*>(stmt);
        analyzeExpr(ifStmt->getCondition());

        // Check for constant conditions
        if (ifStmt->getCondition()->getKind() == Expr::Kind::LITERAL)
            reportIssue(Issue::Severity::WARNING, "Condition is always constant", stmt->getLine(), "Consider removing if statement");

        if (ifStmt->getThenBlock()) {
            for (Stmt const* const& s : ifStmt->getThenBlock()->getStatements())
                analyzeStmt(s);
        }

        if (ifStmt->getElseBlock()) {
            for (Stmt const* const& s : ifStmt->getElseBlock()->getStatements())
                analyzeStmt(s);
        }

        break;
    }

    case Stmt::Kind::WHILE: {
        WhileStmt const* whileStmt = static_cast<WhileStmt const*>(stmt);
        analyzeExpr(whileStmt->getCondition());

        // Detect infinite loops
        if (whileStmt->getCondition()->getKind() == Expr::Kind::LITERAL) {
            LiteralExpr const* lit = static_cast<LiteralExpr const*>(whileStmt->getCondition());
            if (lit->isBoolean() && lit->getBool() == true)
                reportIssue(Issue::Severity::WARNING, "Infinite loop detected", stmt->getLine(), "Add a break condition");
        }

        for (Stmt const* const& s : whileStmt->getBlock()->getStatements())
            analyzeStmt(s);

        break;
    }

    case Stmt::Kind::FOR: {
        ForStmt const* forStmt = static_cast<ForStmt const*>(stmt);
        analyzeExpr(forStmt->getIter());

        // Create new scope for loop variable
        CurrentScope_ = CurrentScope_->createChild();
        SymbolTable::Symbol loopVar;
        loopVar.symbolType = SymbolTable::SymbolType::VARIABLE;
        loopVar.dataType = SymbolTable::DataType_t::ANY;
        CurrentScope_->define(forStmt->getTarget()->getValue(), loopVar);

        for (Stmt const* const& s : forStmt->getBlock()->getStatements())
            analyzeStmt(s);

        // Check if loop variable is shadowing
        if (CurrentScope_->Parent_ && CurrentScope_->Parent_->lookupLocal(forStmt->getTarget()->getValue()))
            reportIssue(Issue::Severity::WARNING, "Loop variable shadows outer variable", stmt->getLine());

        // Exit loop scope
        CurrentScope_ = CurrentScope_->Parent_;
        break;
    }

    case Stmt::Kind::FUNC: {
        FunctionDef const* funcDef = static_cast<FunctionDef const*>(stmt);
        SymbolTable::Symbol funcSym;
        funcSym.symbolType = SymbolTable::SymbolType::FUNCTION;
        funcSym.dataType = SymbolTable::DataType_t::FUNCTION;
        funcSym.definitionLine = stmt->getLine();
        CurrentScope_->define(funcDef->getName()->getValue(), funcSym);

        // Create function scope
        CurrentScope_ = CurrentScope_->createChild();

        for (Expr const* const& param : funcDef->getParameters()) {
            SymbolTable::Symbol paramSym;
            paramSym.symbolType = SymbolTable::SymbolType::VARIABLE;
            paramSym.dataType = SymbolTable::DataType_t::ANY;
            CurrentScope_->define(static_cast<NameExpr const*>(param)->getValue(), paramSym);
        }

        for (Stmt const* const& s : funcDef->getBody()->getStatements())
            analyzeStmt(s);

        // Check for missing return statement
        bool hasReturn = false;

        for (Stmt const* const& s : funcDef->getBody()->getStatements()) {
            if (s->getKind() == Stmt::Kind::RETURN) {
                hasReturn = true;
                break;
            }
        }

        if (!hasReturn)
            reportIssue(Issue::Severity::INFO, "Function may not return a value", stmt->getLine());

        // Exit function scope
        CurrentScope_ = CurrentScope_->Parent_;
        break;
    }

    case Stmt::Kind::RETURN: {
        ReturnStmt const* ret = static_cast<ReturnStmt const*>(stmt);
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
    printSym.name = "print";
    printSym.symbolType = SymbolTable::SymbolType::FUNCTION;
    printSym.dataType = SymbolTable::DataType_t::FUNCTION;
    GlobalScope_->define("print", printSym);
}

void SemanticAnalyzer::analyze(std::vector<Stmt*> const& Statements_)
{
    for (Stmt const* const& stmt : Statements_)
        analyzeStmt(stmt);

    // Check for unused variables
    std::vector<SymbolTable::Symbol*> unused = GlobalScope_->getUnusedSymbols();
    for (SymbolTable::Symbol* sym : unused)
        reportIssue(Issue::Severity::WARNING, "Unused variable: " + sym->name, sym->definitionLine, "Consider removing if not needed");
}

std::vector<typename SemanticAnalyzer::Issue> const& SemanticAnalyzer::getIssues() const
{
    return Issues_;
}

SymbolTable const* SemanticAnalyzer::getGlobalScope() const
{
    return GlobalScope_.get();
}

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
            sevStr = "ERROR";
            break;
        case Issue::Severity::WARNING:
            sevStr = "WARNING";
            break;
        case Issue::Severity::INFO:
            sevStr = "INFO";
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
