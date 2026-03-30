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

static bool isValidNode(AST::Fa_Expr const* e) { return e->getKind() != AST::Fa_Expr::Kind::INVALID; }
static bool isValidNode(AST::Fa_Stmt const* s) { return s->getKind() != AST::Fa_Stmt::Kind::INVALID; }

static bool stmtDefinitelyReturns(AST::Fa_Stmt const* stmt)
{
    if (!stmt)
        return false;

    switch (stmt->getKind()) {
    case AST::Fa_Stmt::Kind::RETURN:
        return true;
    case AST::Fa_Stmt::Kind::BLOCK: {
        auto block = static_cast<AST::Fa_BlockStmt const*>(stmt);
        for (AST::Fa_Stmt const* child : block->getStatements()) {
            if (stmtDefinitelyReturns(child))
                return true;
        }
        return false;
    }
    case AST::Fa_Stmt::Kind::IF: {
        auto if_stmt = static_cast<AST::Fa_IfStmt const*>(stmt);
        return stmtDefinitelyReturns(if_stmt->getThen()) && stmtDefinitelyReturns(if_stmt->getElse());
    }
    default:
        return false;
    }
}

Fa_Error Fa_Parser::reportError(ErrorCode err_code)
{
    auto err_tok = currentToken();
    Fa_SourceLocation loc = { err_tok->line(), err_tok->column(), static_cast<u16>(err_tok->lexeme().len()) };
    return fairuz::reportError(err_code, loc, &Lexer_);
}

// symbol table

Fa_SymbolTable::Fa_SymbolTable(Fa_SymbolTable* p, i32 level)
    : Parent_(p)
    , ScopeLevel_(level)
{
}

void Fa_SymbolTable::define(Fa_StringRef const& name, Symbol symbol)
{
    if (lookupLocal(name))
        return;

    symbol.name = name;
    Symbols_[name] = symbol;
}

typename Fa_SymbolTable::Symbol* Fa_SymbolTable::lookup(Fa_StringRef const& name)
{
    if (Symbols_.contains(name))
        return &Symbols_[name];

    return Parent_ ? Parent_->lookup(name) : nullptr;
}

typename Fa_SymbolTable::Symbol* Fa_SymbolTable::lookupLocal(Fa_StringRef const& name)
{
    return Symbols_.findPtr(name);
}

bool Fa_SymbolTable::isDefined(Fa_StringRef const& name) const
{
    if (Symbols_.contains(name))
        return true;

    return Parent_ ? Parent_->isDefined(name) : false;
}

void Fa_SymbolTable::markUsed(Fa_StringRef const& name, i32 line)
{
    if (Symbol* sym = lookup(name)) {
        sym->isUsed = true;
        sym->usageLines.push(line);
    }
}

Fa_SymbolTable* Fa_SymbolTable::createChild()
{
    Fa_SymbolTable* child = getAllocator().allocateObject<Fa_SymbolTable>(this, ScopeLevel_ + 1);
    Children_.push(child);
    return child;
}

Fa_Array<typename Fa_SymbolTable::Symbol*> Fa_SymbolTable::getUnusedSymbols()
{
    Fa_Array<Symbol*> unused;
    for (auto [name, sym] : Symbols_) {
        if (!sym.isUsed && sym.symbolType == SymbolType::VARIABLE)
            unused.push(&sym);
    }

    return unused;
}

Fa_HashTable<Fa_StringRef, typename Fa_SymbolTable::Symbol, Fa_StringRefHash, Fa_StringRefEqual> const&
Fa_SymbolTable::getSymbols() const
{
    return Symbols_;
}

// semantic analyzer

typename Fa_SymbolTable::DataType_t Fa_SemanticAnalyzer::inferType(AST::Fa_Expr const* expr)
{
    if (!expr)
        return Fa_SymbolTable::DataType_t::UNKNOWN;

    switch (expr->getKind()) {
    case AST::Fa_Expr::Kind::LITERAL: {
        auto lit = static_cast<AST::Fa_LiteralExpr const*>(expr);

        if (lit->isString())
            return Fa_SymbolTable::DataType_t::STRING;
        if (lit->isDecimal())
            return Fa_SymbolTable::DataType_t::FLOAT;
        if (lit->isInteger())
            return Fa_SymbolTable::DataType_t::INTEGER;
        if (lit->isBoolean())
            return Fa_SymbolTable::DataType_t::BOOLEAN;

        return Fa_SymbolTable::DataType_t::NONE;
    }

    case AST::Fa_Expr::Kind::NAME: {
        auto name = static_cast<AST::Fa_NameExpr const*>(expr);
        Fa_SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue());
        if (sym)
            return sym->dataType;
    } break;

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = static_cast<AST::Fa_BinaryExpr const*>(expr);

        Fa_SymbolTable::DataType_t leftType = inferType(bin->getLeft());
        Fa_SymbolTable::DataType_t rightType = inferType(bin->getRight());

        // Type promotion rules
        if (leftType == Fa_SymbolTable::DataType_t::FLOAT || rightType == Fa_SymbolTable::DataType_t::FLOAT)
            return Fa_SymbolTable::DataType_t::FLOAT;
        if (leftType == Fa_SymbolTable::DataType_t::INTEGER && rightType == Fa_SymbolTable::DataType_t::INTEGER)
            return Fa_SymbolTable::DataType_t::INTEGER;
        if (bin->getOperator() == AST::Fa_BinaryOp::OP_ADD && leftType == Fa_SymbolTable::DataType_t::STRING)
            return Fa_SymbolTable::DataType_t::STRING;
        if (bin->getOperator() == AST::Fa_BinaryOp::OP_AND || bin->getOperator() == AST::Fa_BinaryOp::OP_OR)
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

void Fa_SemanticAnalyzer::reportIssue(Issue::Severity sev, Fa_StringRef msg, i32 line, Fa_StringRef const& sugg)
{
    Issues_.push({ sev, msg, line, sugg });
}

void Fa_SemanticAnalyzer::analyzeFa_Expr(AST::Fa_Expr const* expr)
{
    if (!expr)
        return;

    switch (expr->getKind()) {
    case AST::Fa_Expr::Kind::NAME: {
        auto name = static_cast<AST::Fa_NameExpr const*>(expr);

        if (!CurrentScope_->isDefined(name->getValue()))
            reportIssue(Issue::Severity::ERROR, "Undefined variable: " + name->getValue(), expr->getLine(), "Did you forget to initialize it?");
        else
            CurrentScope_->markUsed(name->getValue(), expr->getLine());
    } break;

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = static_cast<AST::Fa_BinaryExpr const*>(expr);

        analyzeFa_Expr(bin->getLeft());
        analyzeFa_Expr(bin->getRight());

        Fa_SymbolTable::DataType_t leftType = inferType(bin->getLeft());
        Fa_SymbolTable::DataType_t rightType = inferType(bin->getRight());

        if (leftType != rightType && leftType != Fa_SymbolTable::DataType_t::UNKNOWN && rightType != Fa_SymbolTable::DataType_t::UNKNOWN)
            reportIssue(Issue::Severity::ERROR, "Type mismatch in binary Fa_Expression", expr->getLine(), "Left and right operands must have same type");

        if (leftType == Fa_SymbolTable::DataType_t::STRING || rightType == Fa_SymbolTable::DataType_t::STRING) {
            if (bin->getOperator() != AST::Fa_BinaryOp::OP_ADD)
                reportIssue(Issue::Severity::ERROR, "Invalid operation on string", expr->getLine(), "Only '+' is allowed for strings");
        }

        if (bin->getOperator() == AST::Fa_BinaryOp::OP_DIV && bin->getRight()->getKind() == AST::Fa_Expr::Kind::LITERAL) {
            auto lit = static_cast<AST::Fa_LiteralExpr const*>(bin->getRight());
            if (lit->isNumeric() && lit->toNumber() == 0)
                reportIssue(Issue::Severity::ERROR, "Division by zero", expr->getLine(), "This will cause a runtime error");
        }
    } break;

    case AST::Fa_Expr::Kind::UNARY: {
        auto un = static_cast<AST::Fa_UnaryExpr const*>(expr);
        analyzeFa_Expr(un->getOperand());
    } break;

    case AST::Fa_Expr::Kind::CALL: {
        auto call = static_cast<AST::Fa_CallExpr const*>(expr);
        analyzeFa_Expr(call->getCallee());

        for (AST::Fa_Expr const* const& arg : call->getArgs())
            analyzeFa_Expr(arg);

        if (call->getCallee()->getKind() == AST::Fa_Expr::Kind::NAME) {
            auto name = static_cast<AST::Fa_NameExpr const*>(call->getCallee());

            if (Fa_SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue())) {
                if (sym->symbolType != Fa_SymbolTable::SymbolType::FUNCTION)
                    reportIssue(Issue::Severity::ERROR, "'" + name->getValue() + "' is not callable", expr->getLine());
            }
        }

        if (call->getCallee()->getKind() == AST::Fa_Expr::Kind::NAME) {
            auto name = static_cast<AST::Fa_NameExpr const*>(call->getCallee());
            Fa_SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue());

            if (!sym)
                reportIssue(Issue::Severity::ERROR, "Undefined function: " + name->getValue(), expr->getLine());
            else if (sym->symbolType != Fa_SymbolTable::SymbolType::FUNCTION)
                reportIssue(Issue::Severity::ERROR, "'" + name->getValue() + "' is not callable", expr->getLine());
        }
    } break;

    case AST::Fa_Expr::Kind::LIST: {
        auto list = static_cast<AST::Fa_ListExpr const*>(expr);
        for (AST::Fa_Expr const* const& elem : list->getElements())
            analyzeFa_Expr(elem);
    } break;

    case AST::Fa_Expr::Kind::INDEX: {
        auto idx = static_cast<AST::Fa_IndexExpr const*>(expr);
        analyzeFa_Expr(idx->getObject());
        analyzeFa_Expr(idx->getIndex());
    } break;

    default:
        break;
    }
}

void Fa_SemanticAnalyzer::analyzeStmt(AST::Fa_Stmt const* stmt)
{
    if (!stmt)
        return;

    switch (stmt->getKind()) {
    case AST::Fa_Stmt::Kind::ASSIGNMENT: {
        auto assign = static_cast<AST::Fa_AssignmentStmt const*>(stmt);
        analyzeFa_Expr(assign->getValue());

        AST::Fa_Expr* target = assign->getTarget();
        assert(target);

        if (target->getKind() == AST::Fa_Expr::Kind::INDEX) {
            analyzeFa_Expr(target);
            break;
        }

        if (target->getKind() == AST::Fa_Expr::Kind::NAME) {
            Fa_StringRef target_name = static_cast<AST::Fa_NameExpr*>(target)->getValue();

            if (!CurrentScope_->lookupLocal(target_name)) {
                // First assignment in this scope — declare it
                Fa_SymbolTable::Symbol sym;
                sym.symbolType = Fa_SymbolTable::SymbolType::VARIABLE;
                sym.dataType = inferType(assign->getValue());
                sym.definitionLine = stmt->getLine();
                CurrentScope_->define(target_name, sym);
            } else {
                CurrentScope_->markUsed(target_name, stmt->getLine());
            }
        }
    } break;

    case AST::Fa_Stmt::Kind::EXPR: {
        auto Fa_Expr_stmt = static_cast<AST::Fa_ExprStmt const*>(stmt);
        analyzeFa_Expr(Fa_Expr_stmt->getExpr());
        if (Fa_Expr_stmt->getExpr()->getKind() != AST::Fa_Expr::Kind::CALL)
            reportIssue(Issue::Severity::INFO, "Fa_Expression result not used", stmt->getLine());
    } break;

    case AST::Fa_Stmt::Kind::IF: {
        auto ifStmt = static_cast<AST::Fa_IfStmt const*>(stmt);
        analyzeFa_Expr(ifStmt->getCondition());

        AST::Fa_Stmt const* then_block = ifStmt->getThen();
        AST::Fa_Stmt const* else_block = ifStmt->getElse();

        if (ifStmt->getCondition()->getKind() == AST::Fa_Expr::Kind::LITERAL)
            reportIssue(Issue::Severity::WARNING, "Condition is always constant", stmt->getLine(), "Consider removing if statement");

        if (then_block)
            analyzeStmt(then_block);
        if (else_block)
            analyzeStmt(else_block);
    } break;

    case AST::Fa_Stmt::Kind::WHILE: {
        auto while_stmt = static_cast<AST::Fa_WhileStmt const*>(stmt);
        analyzeFa_Expr(while_stmt->getCondition());

        // Detect infinite loops
        if (while_stmt->getCondition()->getKind() == AST::Fa_Expr::Kind::LITERAL) {
            AST::Fa_LiteralExpr const* lit = static_cast<AST::Fa_LiteralExpr const*>(while_stmt->getCondition());
            if (lit->isBoolean() && lit->getBool() == true)
                reportIssue(Issue::Severity::WARNING, "Infinite loop detected", stmt->getLine(), "Add a break condition");
        }

        analyzeStmt(while_stmt->getBody());
    } break;

    case AST::Fa_Stmt::Kind::FOR: {
        auto for_stmt = static_cast<AST::Fa_ForStmt const*>(stmt);
        analyzeFa_Expr(for_stmt->getIter());

        CurrentScope_ = CurrentScope_->createChild();
        Fa_SymbolTable::Symbol loopVar;
        loopVar.symbolType = Fa_SymbolTable::SymbolType::VARIABLE;
        loopVar.dataType = Fa_SymbolTable::DataType_t::ANY;
        CurrentScope_->define(for_stmt->getTarget()->getValue(), loopVar);

        analyzeStmt(for_stmt->getBody());

        if (CurrentScope_->Parent_ && CurrentScope_->Parent_->lookupLocal(for_stmt->getTarget()->getValue()))
            reportIssue(Issue::Severity::WARNING, "Loop variable shadows outer variable", stmt->getLine());

        CurrentScope_ = CurrentScope_->Parent_;
    } break;

    case AST::Fa_Stmt::Kind::FUNC: {
        auto func_def = static_cast<AST::Fa_FunctionDef const*>(stmt);
        Fa_SymbolTable::Symbol func_sym;
        func_sym.symbolType = Fa_SymbolTable::SymbolType::FUNCTION;
        func_sym.dataType = Fa_SymbolTable::DataType_t::FUNCTION;
        func_sym.definitionLine = stmt->getLine();
        CurrentScope_->define(func_def->getName()->getValue(), func_sym);

        CurrentScope_ = CurrentScope_->createChild();

        for (AST::Fa_Expr const* const& param : func_def->getParameters()) {
            Fa_SymbolTable::Symbol param_sym;
            param_sym.symbolType = Fa_SymbolTable::SymbolType::VARIABLE;
            param_sym.dataType = Fa_SymbolTable::DataType_t::ANY;
            CurrentScope_->define(static_cast<AST::Fa_NameExpr const*>(param)->getValue(), param_sym);
        }

        analyzeStmt(func_def->getBody());
        if (!stmtDefinitelyReturns(func_def->getBody()))
            reportIssue(Issue::Severity::INFO, "Function may not return a value", stmt->getLine());
        // Exit function scope
        CurrentScope_ = CurrentScope_->Parent_;
    } break;

    case AST::Fa_Stmt::Kind::RETURN: {
        auto ret = static_cast<AST::Fa_ReturnStmt const*>(stmt);
        analyzeFa_Expr(ret->getValue());
    } break;

    default:
        break;
    }
}

Fa_SemanticAnalyzer::Fa_SemanticAnalyzer()
{
    GlobalScope_ = getAllocator().allocateObject<Fa_SymbolTable>();
    CurrentScope_ = GlobalScope_;

    Fa_SymbolTable::Symbol printSym;
    printSym.name = "print";
    printSym.symbolType = Fa_SymbolTable::SymbolType::FUNCTION;
    printSym.dataType = Fa_SymbolTable::DataType_t::FUNCTION;
    GlobalScope_->define("print", printSym);
}

void Fa_SemanticAnalyzer::analyze(Fa_Array<AST::Fa_Stmt*> const& Statements_)
{
    for (AST::Fa_Stmt const* const& stmt : Statements_)
        analyzeStmt(stmt);

    Fa_Array<Fa_SymbolTable::Symbol*> unused = GlobalScope_->getUnusedSymbols();
    for (Fa_SymbolTable::Symbol* sym : unused)
        reportIssue(Issue::Severity::WARNING, "Unused variable: " + sym->name, sym->definitionLine, "Consider removing if not needed");
}

Fa_Array<typename Fa_SemanticAnalyzer::Issue> const& Fa_SemanticAnalyzer::getIssues() const { return Issues_; }

Fa_SymbolTable const* Fa_SemanticAnalyzer::getGlobalScope() const { return GlobalScope_; }
Fa_SymbolTable const* Fa_SemanticAnalyzer::getCurrentScope() const { return CurrentScope_; }

void Fa_SemanticAnalyzer::printReport() const
{
    if (Issues_.empty()) {
        std::cout << "✓ No issues found\n";
        return;
    }

    std::cout << "Found " << Issues_.size() << " issue(s):\n\n";
    for (Fa_SemanticAnalyzer::Issue const& issue : Issues_) {
        Fa_StringRef sevStr;
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

AST::Fa_BinaryOp toBinaryOp(tok::Fa_TokenType const op)
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

AST::Fa_UnaryOp toUnaryOp(tok::Fa_TokenType const op)
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

Fa_Array<AST::Fa_Stmt*> Fa_Parser::parseProgram()
{
    Fa_Array<AST::Fa_Stmt*> statements;

    while (!weDone()) {
        skipNewlines();
        if (weDone())
            break;

        auto stmt = parseStatement();
        if (stmt.hasValue()) {
            statements.push(stmt.value());
        } else {
            if (diagnostic::isSaturated())
                break;
            synchronize();
            if (weDone())
                break;
        }
    }

    Sema_.analyze(statements);

    if (diagnostic::hasErrors())
        diagnostic::dump();

    return statements;
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parseStatement()
{
    skipNewlines();

    if (check(tok::Fa_TokenType::KW_IF))
        return parseIfStmt();
    if (check(tok::Fa_TokenType::KW_WHILE))
        return parseWhileStmt();
    if (check(tok::Fa_TokenType::KW_RETURN))
        return parseReturnStmt();
    if (check(tok::Fa_TokenType::KW_FN))
        return parseFunctionDef();

    return parseExpressionStmt();
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parseReturnStmt()
{
    if (!consume(tok::Fa_TokenType::KW_RETURN))
        return reportError(ErrorCode::EXPECTED_RETURN);

    if (check(tok::Fa_TokenType::NEWLINE) || weDone())
        return AST::Fa_makeReturn();

    auto ret = parseExpression();
    if (ret.hasError())
        return ret.error();

    return AST::Fa_makeReturn(ret.value());
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parseWhileStmt()
{
    if (!consume(tok::Fa_TokenType::KW_WHILE))
        return reportError(ErrorCode::EXPECTED_WHILE_KEYWORD);

    auto condition = parseExpression();
    if (condition.hasError())
        return condition.error();

    if (!consume(tok::Fa_TokenType::COLON))
        return reportError(ErrorCode::EXPECTED_COLON_WHILE);

    auto while_block = parseIndentedBlock();
    if (while_block.hasError())
        return while_block.error();

    return Fa_makeWhile(condition.value(), static_cast<AST::Fa_BlockStmt*>(while_block.value()));
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parseIndentedBlock()
{
    if (check(tok::Fa_TokenType::NEWLINE))
        advance();

    if (!consume(tok::Fa_TokenType::INDENT))
        return reportError(ErrorCode::EXPECTED_INDENT);

    Fa_Array<AST::Fa_Stmt*> statements;

    if (check(tok::Fa_TokenType::DEDENT)) {
        advance();
        return Fa_makeBlock(statements);
    }

    while (!check(tok::Fa_TokenType::DEDENT) && !weDone()) {
        skipNewlines();
        if (check(tok::Fa_TokenType::DEDENT))
            break;

        auto stmt = parseStatement();

        if (stmt.hasValue())
            statements.push(stmt.value());
        else {
            synchronize();
            if (check(tok::Fa_TokenType::DEDENT) || weDone())
                break;
        }
    }

    if (check(tok::Fa_TokenType::ENDMARKER))
        return Fa_makeBlock(statements);
    else if (!consume(tok::Fa_TokenType::DEDENT))
        return reportError(ErrorCode::EXPECTED_DEDENT);

    return Fa_makeBlock(statements);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseParametersList()
{
    if (!consume(tok::Fa_TokenType::LPAREN))
        return reportError(ErrorCode::EXPECTED_LPAREN);

    Fa_Array<AST::Fa_Expr*> parameters = Fa_Array<AST::Fa_Expr*>::withCapacity(4); // typical small function

    if (!check(tok::Fa_TokenType::RPAREN)) {
        do {
            skipNewlines();
            if (check(tok::Fa_TokenType::RPAREN))
                break;

            if (!check(tok::Fa_TokenType::IDENTIFIER))
                return reportError(ErrorCode::EXPECTED_PARAM_NAME);

            Fa_StringRef param_name = currentToken()->lexeme();
            advance();
            parameters.push(AST::Fa_makeName(param_name));
            skipNewlines();
        } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RPAREN));
    }

    if (!consume(tok::Fa_TokenType::RPAREN))
        return reportError(ErrorCode::EXPECTED_RPAREN_EXPR);

    if (parameters.empty())
        return Fa_makeList(Fa_Array<AST::Fa_Expr*> { });

    return Fa_makeList(parameters);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseExpression() { return parseAssignmentExpr(); }

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parseFunctionDef()
{
    if (!consume(tok::Fa_TokenType::KW_FN))
        return reportError(ErrorCode::EXPECTED_FN_KEYWORD);

    if (!check(tok::Fa_TokenType::IDENTIFIER))
        return reportError(ErrorCode::EXPECTED_FN_NAME);

    Fa_StringRef function_name = currentToken()->lexeme();
    advance();

    auto parameters_list = parseParametersList();
    if (parameters_list.hasError())
        return parameters_list.error();

    if (!consume(tok::Fa_TokenType::COLON))
        return reportError(ErrorCode::EXPECTED_COLON_FN);

    auto function_body = parseIndentedBlock();
    if (function_body.hasError())
        return function_body.error();

    return Fa_makeFunction(AST::Fa_makeName(function_name), static_cast<AST::Fa_ListExpr*>(parameters_list.value()), static_cast<AST::Fa_BlockStmt*>(function_body.value()));
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parseIfStmt()
{
    if (!consume(tok::Fa_TokenType::KW_IF))
        return reportError(ErrorCode::EXPECTED_IF_KEYWORD);

    auto condition = parseExpression();
    if (condition.hasError())
        return condition.error();

    if (!consume(tok::Fa_TokenType::COLON))
        return reportError(ErrorCode::EXPECTED_COLON_IF);

    auto then_block = parseIndentedBlock();
    if (then_block.hasError())
        return then_block.error();

    AST::Fa_Stmt* else_block = nullptr;

    skipNewlines();

    if (consume(tok::Fa_TokenType::KW_ELSE)) {
        skipNewlines();

        if (check(tok::Fa_TokenType::KW_IF)) {
            // else-if: treat as nested if inside the else clause
            auto nested = parseIfStmt();
            if (nested.hasError())
                return nested.error();
            else_block = nested.value();
        } else {
            if (!consume(tok::Fa_TokenType::COLON))
                return reportError(ErrorCode::EXPECTED_COLON_IF);
            auto else_stmt = parseIndentedBlock();
            if (else_stmt.hasError())
                return else_stmt.error();
            else_block = else_stmt.value();
        }
    }

    return Fa_makeIf(
        condition.value(),
        static_cast<AST::Fa_BlockStmt*>(then_block.value()),
        else_block);
}

Fa_ErrorOr<AST::Fa_Stmt*> Fa_Parser::parseExpressionStmt()
{
    auto expr = parseExpression();
    if (expr.hasError())
        return expr.error();

    return Fa_makeExprStmt(expr.value());
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseAssignmentExpr()
{
    auto L = parseConditionalExpr();
    if (L.hasError())
        return L.error();

    if (check(tok::Fa_TokenType::OP_ASSIGN)) {
        AST::Fa_Expr* target = L.value();
        AST::Fa_Expr::Kind kind = target->getKind();

        if (kind != AST::Fa_Expr::Kind::NAME && kind != AST::Fa_Expr::Kind::INDEX)
            return reportError(ErrorCode::INVALID_ASSIGN_TARGET);

        advance();

        auto R = parseAssignmentExpr();
        if (R.hasError())
            return R.error();

        if (kind == AST::Fa_Expr::Kind::NAME) {
            auto name_target = static_cast<AST::Fa_NameExpr*>(target);
            bool decl = !Sema_.getCurrentScope()->isDefined(name_target->getValue());
            return Fa_ErrorOr<AST::Fa_Expr*>::fromValue(Fa_makeAssignmentExpr(name_target, R.value(), decl));
        }
        return Fa_ErrorOr<AST::Fa_Expr*>::fromValue(Fa_makeAssignmentExpr(target, R.value(), /*decl=*/false));
    }

    return L.value();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseLogicalExprPrecedence(unsigned int min_precedence)
{
    auto L = parseComparisonExpr();
    if (L.hasError())
        return L.error();

    for (;;) {
        tok::Fa_Token const* tok = currentToken();
        if (!tok->isBinaryOp())
            break;

        unsigned int precedence = tok->getPrecedence();
        if (precedence == tok::PREC_NONE || precedence < min_precedence)
            break;

        tok::Fa_TokenType op = Lexer_.current()->type();
        advance();

        auto R = parseLogicalExprPrecedence(precedence + 1);
        if (R.hasError())
            return R.error();

        L.setValue(Fa_makeBinary(L.value(), R.value(), toBinaryOp(op)));
    }

    return L.value();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseComparisonExpr()
{
    auto L = parseBinaryExpr();
    if (L.hasError())
        return L.error();

    if (currentToken()->isComparisonOp()) {
        tok::Fa_TokenType op = Lexer_.current()->type();
        advance();
        auto R = parseBinaryExpr();
        if (R.hasError())
            return R.error();

        L.setValue(Fa_makeBinary(L.value(), R.value(), toBinaryOp(op)));
    }

    return L.value();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseBinaryExpr() { return parseBinaryExprPrecedence(0); }

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseBinaryExprPrecedence(unsigned int min_precedence)
{
    auto L = parseUnaryExpr();
    if (L.hasError())
        return L.error();

    for (;;) {
        tok::Fa_Token const* tok = currentToken();
        if (!tok->isBinaryOp() || tok->is(tok::Fa_TokenType::OP_ASSIGN))
            break;

        unsigned int precedence = tok->getPrecedence();
        if (precedence == tok::PREC_NONE || precedence < min_precedence)
            break;

        tok::Fa_TokenType op = Lexer_.current()->type();
        advance();

        unsigned int nextMin = precedence + 1;
        auto R = parseBinaryExprPrecedence(nextMin);
        if (R.hasError())
            return R.error();

        L.setValue(Fa_makeBinary(L.value(), R.value(), toBinaryOp(op)));
    }

    return L.value();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseUnaryExpr()
{
    tok::Fa_Token const* tok = currentToken();
    if (tok->isUnaryOp()) {
        tok::Fa_TokenType op = Lexer_.current()->type();
        advance();

        auto expr = parseUnaryExpr();
        if (expr.hasError())
            return expr.error();

        return Fa_makeUnary(expr.value(), toUnaryOp(op));
    }
    return parsePostfixExpr();
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parsePostfixExpr()
{
    auto expr_or = parsePrimaryExpr();
    if (expr_or.hasError())
        return expr_or.error();

    AST::Fa_Expr* expr = expr_or.value();

    for (;;) {
        if (check(tok::Fa_TokenType::LPAREN)) {
            advance();

            Fa_Array<AST::Fa_Expr*> args = Fa_Array<AST::Fa_Expr*>::withCapacity(4);

            if (!check(tok::Fa_TokenType::RPAREN)) {
                do {
                    skipNewlines();
                    if (check(tok::Fa_TokenType::RPAREN))
                        break;

                    auto arg = parseExpression();
                    if (arg.hasError())
                        return arg.error();

                    args.push(arg.value());
                    skipNewlines();
                } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RPAREN));
            }

            if (!consume(tok::Fa_TokenType::RPAREN))
                return reportError(ErrorCode::EXPECTED_RPAREN_EXPR);

            expr = Fa_makeCall(expr, Fa_makeList(std::move(args)));
            continue;
        }

        // dict
        if (check(tok::Fa_TokenType::LBRACE)) {
            advance();

            Fa_Array<std::pair<AST::Fa_Expr*, AST::Fa_Expr*>> content;

            if (!check(tok::Fa_TokenType::RBRACE))
            {
                do {
                    skipNewlines();
                    if (check(tok::Fa_TokenType::RBRACE))
                        break;

                    auto first = parseExpression();
                    if (first.hasError())
                        return first.error();

                    if (!consume(tok::Fa_TokenType::COLON))
                        return reportError(ErrorCode::EXPECTED_COLON_DICT);

                    auto second = parseExpression();
                    if (second.hasError())
                        return second.error();

                    content.push({first.value(), second.value()});
                } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RBRACE));
            }

            if (!consume(tok::Fa_TokenType::RBRACE))
                return reportError(ErrorCode::EXPECTED_RBRACE_EXPR);

            expr = AST::Fa_makeDict(content);
            continue;
        }

        if (check(tok::Fa_TokenType::LBRACKET)) {
            advance();

            auto index = parseExpression();
            if (index.hasError())
                return index.error();

            if (!consume(tok::Fa_TokenType::RBRACKET))
                return reportError(ErrorCode::EXPECTED_RBRACKET);

            expr = Fa_makeIndex(expr, index.value());
            continue;
        }

        break;
    }

    return expr;
}

bool Fa_Parser::weDone() const { return Lexer_.current()->is(tok::Fa_TokenType::ENDMARKER); }

bool Fa_Parser::check(tok::Fa_TokenType type) { return Lexer_.current()->is(type); }

tok::Fa_Token const* Fa_Parser::currentToken() { return Lexer_.current(); }

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parse() { return parseExpression(); }

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parsePrimaryExpr()
{
    tok::Fa_Token const* tok = currentToken();

    if (currentToken()->isNumeric()) {
        Fa_StringRef v = tok->lexeme();
        advance();

        tok::Fa_TokenType tt = tok->type();

        if (tt == tok::Fa_TokenType::DECIMAL)
            return AST::Fa_makeLiteralFloat(v.toDouble());

        if (tt == tok::Fa_TokenType::INTEGER || tt == tok::Fa_TokenType::HEX || tt == tok::Fa_TokenType::OCTAL || tt == tok::Fa_TokenType::BINARY) {
            static constexpr int BASE_FOR_TYPE[] = { 2, 8, 10, 16 };
            return AST::Fa_makeLiteralInt(util::parseIntegerLiteral(v,
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
        Fa_StringRef name = tok->lexeme();
        advance();
        return AST::Fa_makeName(name);
    }

    if (check(tok::Fa_TokenType::LPAREN)) {
        advance();
        if (check(tok::Fa_TokenType::RPAREN)) {
            advance();
            return Fa_makeList(Fa_Array<AST::Fa_Expr*> { });
        }

        auto expr = parseExpression();

        if (expr.hasError())
            return expr.error();

        if (!consume(tok::Fa_TokenType::RPAREN))
            return reportError(ErrorCode::EXPECTED_RPAREN_EXPR);

        return expr.value();
    }

    if (check(tok::Fa_TokenType::LBRACKET)) {
        advance();
        return parseListLiteral();
    }

    if (weDone())
        return reportError(ErrorCode::UNEXPECTED_EOF);

    skipNewlines();
    return reportError(ErrorCode::UNEXPECTED_TOKEN);
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseListLiteral()
{
    Fa_Array<AST::Fa_Expr*> elements = Fa_Array<AST::Fa_Expr*>::withCapacity(4);

    if (!check(tok::Fa_TokenType::RBRACKET)) {
        do {
            skipNewlines();
            if (check(tok::Fa_TokenType::RBRACKET))
                break;

            auto elem = parseExpression();
            if (elem.hasError())
                return elem.error();

            elements.push(elem.value());
            skipNewlines();
        } while (match(tok::Fa_TokenType::COMMA) && !check(tok::Fa_TokenType::RBRACKET));
    }

    if (!consume(tok::Fa_TokenType::RBRACKET))
        return reportError(ErrorCode::EXPECTED_RBRACKET);

    return Fa_makeList(std::move(elements));
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseConditionalExpr()
{
    return parseLogicalExpr();
    /// TODO: Ternary?
}

Fa_ErrorOr<AST::Fa_Expr*> Fa_Parser::parseLogicalExpr() { return parseLogicalExprPrecedence(0); }

bool Fa_Parser::match(tok::Fa_TokenType const type)
{
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

void Fa_Parser::synchronize()
{
    while (!weDone()) {
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
