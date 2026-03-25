/// parser.cc

#include "../include/parser.hpp"
#include "../include/util.hpp"

#include <iostream>

namespace mylang::parser {

using namespace ast;
using ErrorCode = diagnostic::errc::parser::Code;

static bool isValidNode(Expr const* e) { return e->getKind() != Expr::Kind::INVALID; }
static bool isValidNode(Stmt const* s) { return s->getKind() != Stmt::Kind::INVALID; }

static bool stmtDefinitelyReturns(Stmt const* stmt)
{
    if (!stmt)
        return false;

    switch (stmt->getKind()) {
    case Stmt::Kind::RETURN:
        return true;
    case Stmt::Kind::BLOCK: {
        auto const* block = static_cast<BlockStmt const*>(stmt);
        for (Stmt const* child : block->getStatements()) {
            if (stmtDefinitelyReturns(child))
                return true;
        }
        return false;
    }
    case Stmt::Kind::IF: {
        auto const* if_stmt = static_cast<IfStmt const*>(stmt);
        return stmtDefinitelyReturns(if_stmt->getThen()) && stmtDefinitelyReturns(if_stmt->getElse());
    }
    default:
        return false;
    }
}

Error Parser::reportError(ErrorCode err_code)
{
    auto err_tok = currentToken();
    SourceLocation loc = { err_tok->line(), err_tok->column(), static_cast<uint16_t>(err_tok->lexeme().len()) };
    return mylang::reportError(err_code, loc, &Lexer_);
}

// symbol table

SymbolTable::SymbolTable(SymbolTable* p, int32_t level)
    : Parent_(p)
    , ScopeLevel_(level)
{
}

void SymbolTable::define(StringRef const& name, Symbol symbol)
{
    if (lookupLocal(name))
        return;

    symbol.name = name;
    Symbols_.emplace(name, std::move(symbol));
}

typename SymbolTable::Symbol* SymbolTable::lookup(StringRef const& name)
{
    auto it = Symbols_.find(name);
    if (it != Symbols_.end())
        return &it->second;

    return Parent_ ? Parent_->lookup(name) : nullptr;
}

typename SymbolTable::Symbol* SymbolTable::lookupLocal(StringRef const& name)
{
    auto it = Symbols_.find(name);
    return it != Symbols_.end() ? &it->second : nullptr;
}

bool SymbolTable::isDefined(StringRef const& name) const
{
    if (Symbols_.find(name) != Symbols_.end())
        return true;

    return Parent_ ? Parent_->isDefined(name) : false;
}

void SymbolTable::markUsed(StringRef const& name, int32_t line)
{
    if (Symbol* sym = lookup(name)) {
        sym->isUsed = true;
        sym->usageLines.push(line);
    }
}

SymbolTable* SymbolTable::createChild()
{
    std::unique_ptr<SymbolTable> child = std::make_unique<SymbolTable>(this, ScopeLevel_ + 1);
    SymbolTable* ptr = child.get();
    Children_.push(std::move(child));
    return ptr;
}

Array<typename SymbolTable::Symbol*> SymbolTable::getUnusedSymbols()
{
    Array<Symbol*> unused;
    for (auto& [name, sym] : Symbols_) {
        if (!sym.isUsed && sym.symbolType == SymbolType::VARIABLE)
            unused.push(&sym);
    }

    return unused;
}

std::unordered_map<StringRef, typename SymbolTable::Symbol, StringRefHash, StringRefEqual> const& SymbolTable::getSymbols() const { return Symbols_; }

// semantic analyzer

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

        return SymbolTable::DataType_t::NONE;
    }

    case Expr::Kind::NAME: {
        NameExpr const* name = static_cast<NameExpr const*>(expr);
        SymbolTable::Symbol* sym = CurrentScope_->lookup(name->getValue());
        if (sym)
            return sym->dataType;
    } break;

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
    } break;

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
    Issues_.push({ sev, msg, line, sugg });
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
    } break;

    case Expr::Kind::BINARY: {
        BinaryExpr const* bin = static_cast<BinaryExpr const*>(expr);

        analyzeExpr(bin->getLeft());
        analyzeExpr(bin->getRight());

        SymbolTable::DataType_t leftType = inferType(bin->getLeft());
        SymbolTable::DataType_t rightType = inferType(bin->getRight());

        if (leftType != rightType && leftType != SymbolTable::DataType_t::UNKNOWN && rightType != SymbolTable::DataType_t::UNKNOWN)
            reportIssue(Issue::Severity::ERROR, "Type mismatch in binary expression", expr->getLine(), "Left and right operands must have same type");

        if (leftType == SymbolTable::DataType_t::STRING || rightType == SymbolTable::DataType_t::STRING) {
            if (bin->getOperator() != BinaryOp::OP_ADD)
                reportIssue(Issue::Severity::ERROR, "Invalid operation on string", expr->getLine(), "Only '+' is allowed for strings");
        }

        if (bin->getOperator() == BinaryOp::OP_DIV && bin->getRight()->getKind() == Expr::Kind::LITERAL) {
            LiteralExpr const* lit = static_cast<LiteralExpr const*>(bin->getRight());
            if (lit->isNumeric() && lit->toNumber() == 0)
                reportIssue(Issue::Severity::ERROR, "Division by zero", expr->getLine(), "This will cause a runtime error");
        }
    } break;

    case Expr::Kind::UNARY: {
        UnaryExpr const* un = static_cast<UnaryExpr const*>(expr);
        analyzeExpr(un->getOperand());
    } break;

    case Expr::Kind::CALL: {
        CallExpr const* call = static_cast<CallExpr const*>(expr);
        analyzeExpr(call->getCallee());

        for (Expr const* const& arg : call->getArgs())
            analyzeExpr(arg);

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
    } break;

    case Expr::Kind::LIST: {
        ListExpr const* list = static_cast<ListExpr const*>(expr);
        for (Expr const* const& elem : list->getElements())
            analyzeExpr(elem);
    } break;

    case Expr::Kind::INDEX: {
        auto const* idx = static_cast<IndexExpr const*>(expr);
        analyzeExpr(idx->getObject());
        analyzeExpr(idx->getIndex());
    } break;

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

        Expr* target = assign->getTarget();
        assert(target);

        if (target->getKind() == Expr::Kind::INDEX) {
            analyzeExpr(target);
            break;
        }

        if (target->getKind() == Expr::Kind::NAME) {
            StringRef target_name = static_cast<NameExpr*>(target)->getValue();

            if (!CurrentScope_->lookupLocal(target_name)) {
                // First assignment in this scope — declare it
                SymbolTable::Symbol sym;
                sym.symbolType = SymbolTable::SymbolType::VARIABLE;
                sym.dataType = inferType(assign->getValue());
                sym.definitionLine = stmt->getLine();
                CurrentScope_->define(target_name, sym);
            } else {
                CurrentScope_->markUsed(target_name, stmt->getLine());
            }
        }
    } break;

    case Stmt::Kind::EXPR: {
        ExprStmt const* expr_stmt = static_cast<ExprStmt const*>(stmt);
        analyzeExpr(expr_stmt->getExpr());
        if (expr_stmt->getExpr()->getKind() != Expr::Kind::CALL)
            reportIssue(Issue::Severity::INFO, "Expression result not used", stmt->getLine());
    } break;

    case Stmt::Kind::IF: {
        IfStmt const* ifStmt = static_cast<IfStmt const*>(stmt);
        analyzeExpr(ifStmt->getCondition());

        Stmt const* then_block = ifStmt->getThen();
        Stmt const* else_block = ifStmt->getElse();

        if (ifStmt->getCondition()->getKind() == Expr::Kind::LITERAL)
            reportIssue(Issue::Severity::WARNING, "Condition is always constant", stmt->getLine(), "Consider removing if statement");

        if (then_block)
            analyzeStmt(then_block);
        if (else_block)
            analyzeStmt(else_block);
    } break;

    case Stmt::Kind::WHILE: {
        WhileStmt const* while_stmt = static_cast<WhileStmt const*>(stmt);
        analyzeExpr(while_stmt->getCondition());

        // Detect infinite loops
        if (while_stmt->getCondition()->getKind() == Expr::Kind::LITERAL) {
            LiteralExpr const* lit = static_cast<LiteralExpr const*>(while_stmt->getCondition());
            if (lit->isBoolean() && lit->getBool() == true)
                reportIssue(Issue::Severity::WARNING, "Infinite loop detected", stmt->getLine(), "Add a break condition");
        }

        analyzeStmt(while_stmt->getBody());
    } break;

    case Stmt::Kind::FOR: {
        ForStmt const* for_stmt = static_cast<ForStmt const*>(stmt);
        analyzeExpr(for_stmt->getIter());

        CurrentScope_ = CurrentScope_->createChild();
        SymbolTable::Symbol loopVar;
        loopVar.symbolType = SymbolTable::SymbolType::VARIABLE;
        loopVar.dataType = SymbolTable::DataType_t::ANY;
        CurrentScope_->define(for_stmt->getTarget()->getValue(), loopVar);

        analyzeStmt(for_stmt->getBody());

        if (CurrentScope_->Parent_ && CurrentScope_->Parent_->lookupLocal(for_stmt->getTarget()->getValue()))
            reportIssue(Issue::Severity::WARNING, "Loop variable shadows outer variable", stmt->getLine());

        CurrentScope_ = CurrentScope_->Parent_;
    } break;

    case Stmt::Kind::FUNC: {
        FunctionDef const* func_def = static_cast<FunctionDef const*>(stmt);
        SymbolTable::Symbol func_sym;
        func_sym.symbolType = SymbolTable::SymbolType::FUNCTION;
        func_sym.dataType = SymbolTable::DataType_t::FUNCTION;
        func_sym.definitionLine = stmt->getLine();
        CurrentScope_->define(func_def->getName()->getValue(), func_sym);

        CurrentScope_ = CurrentScope_->createChild();

        for (Expr const* const& param : func_def->getParameters()) {
            SymbolTable::Symbol param_sym;
            param_sym.symbolType = SymbolTable::SymbolType::VARIABLE;
            param_sym.dataType = SymbolTable::DataType_t::ANY;
            CurrentScope_->define(static_cast<NameExpr const*>(param)->getValue(), param_sym);
        }

        analyzeStmt(func_def->getBody());
        if (!stmtDefinitelyReturns(func_def->getBody()))
            reportIssue(Issue::Severity::INFO, "Function may not return a value", stmt->getLine());
        // Exit function scope
        CurrentScope_ = CurrentScope_->Parent_;
    } break;

    case Stmt::Kind::RETURN: {
        ReturnStmt const* ret = static_cast<ReturnStmt const*>(stmt);
        analyzeExpr(ret->getValue());
    } break;

    default:
        break;
    }
}

SemanticAnalyzer::SemanticAnalyzer()
{
    GlobalScope_ = std::make_unique<SymbolTable>();
    CurrentScope_ = GlobalScope_.get();

    SymbolTable::Symbol printSym;
    printSym.name = "print";
    printSym.symbolType = SymbolTable::SymbolType::FUNCTION;
    printSym.dataType = SymbolTable::DataType_t::FUNCTION;
    GlobalScope_->define("print", printSym);
}

void SemanticAnalyzer::analyze(Array<Stmt*> const& Statements_)
{
    for (Stmt const* const& stmt : Statements_)
        analyzeStmt(stmt);

    Array<SymbolTable::Symbol*> unused = GlobalScope_->getUnusedSymbols();
    for (SymbolTable::Symbol* sym : unused)
        reportIssue(Issue::Severity::WARNING, "Unused variable: " + sym->name, sym->definitionLine, "Consider removing if not needed");
}

Array<typename SemanticAnalyzer::Issue> const& SemanticAnalyzer::getIssues() const { return Issues_; }

SymbolTable const* SemanticAnalyzer::getGlobalScope() const { return GlobalScope_.get(); }

SymbolTable const* SemanticAnalyzer::getCurrentScope() const { return CurrentScope_; }

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

BinaryOp toBinaryOp(tok::TokenType const op)
{
    switch (op) {
    case tok::TokenType::OP_PLUS:
        return BinaryOp::OP_ADD;
    case tok::TokenType::OP_MINUS:
        return BinaryOp::OP_SUB;
    case tok::TokenType::OP_STAR:
        return BinaryOp::OP_MUL;
    case tok::TokenType::OP_SLASH:
        return BinaryOp::OP_DIV;
    case tok::TokenType::OP_PERCENT:
        return BinaryOp::OP_MOD;
    case tok::TokenType::OP_POWER:
        return BinaryOp::OP_POW;
    case tok::TokenType::OP_EQ:
        return BinaryOp::OP_EQ;
    case tok::TokenType::OP_NEQ:
        return BinaryOp::OP_NEQ;
    case tok::TokenType::OP_LT:
        return BinaryOp::OP_LT;
    case tok::TokenType::OP_GT:
        return BinaryOp::OP_GT;
    case tok::TokenType::OP_LTE:
        return BinaryOp::OP_LTE;
    case tok::TokenType::OP_GTE:
        return BinaryOp::OP_GTE;
    case tok::TokenType::OP_BITAND:
        return BinaryOp::OP_BITAND;
    case tok::TokenType::OP_BITOR:
        return BinaryOp::OP_BITOR;
    case tok::TokenType::OP_BITXOR:
        return BinaryOp::OP_BITXOR;
    case tok::TokenType::OP_LSHIFT:
        return BinaryOp::OP_LSHIFT;
    case tok::TokenType::OP_RSHIFT:
        return BinaryOp::OP_RSHIFT;
    case tok::TokenType::OP_AND:
        return BinaryOp::OP_AND;
    case tok::TokenType::OP_OR:
        return BinaryOp::OP_OR;
    default:
        return BinaryOp::INVALID;
    }
}

UnaryOp toUnaryOp(tok::TokenType const op)
{
    switch (op) {
    case tok::TokenType::OP_PLUS:
        return UnaryOp::OP_PLUS;
    case tok::TokenType::OP_MINUS:
        return UnaryOp::OP_NEG;
    case tok::TokenType::OP_BITNOT:
        return UnaryOp::OP_BITNOT;
    case tok::TokenType::OP_NOT:
        return UnaryOp::OP_NOT;
    default:
        return UnaryOp::INVALID;
    }
}

Array<Stmt*> Parser::parseProgram()
{
    Array<Stmt*> statements;

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

ErrorOr<Stmt*> Parser::parseStatement()
{
    skipNewlines();

    if (check(tok::TokenType::KW_IF))
        return parseIfStmt();
    if (check(tok::TokenType::KW_WHILE))
        return parseWhileStmt();
    if (check(tok::TokenType::KW_RETURN))
        return parseReturnStmt();
    if (check(tok::TokenType::KW_FN))
        return parseFunctionDef();

    return parseExpressionStmt();
}

ErrorOr<Stmt*> Parser::parseReturnStmt()
{
    if (!consume(tok::TokenType::KW_RETURN))
        return reportError(ErrorCode::EXPECTED_RETURN);

    if (check(tok::TokenType::NEWLINE) || weDone())
        return makeReturn();

    auto ret = parseExpression();
    if (ret.hasError())
        return ret.error();

    return makeReturn(ret.value());
}

ErrorOr<Stmt*> Parser::parseWhileStmt()
{
    if (!consume(tok::TokenType::KW_WHILE))
        return reportError(ErrorCode::EXPECTED_WHILE_KEYWORD);

    auto condition = parseExpression();
    if (condition.hasError())
        return condition.error();

    if (!consume(tok::TokenType::COLON))
        return reportError(ErrorCode::EXPECTED_COLON_WHILE);

    auto while_block = parseIndentedBlock();
    if (while_block.hasError())
        return while_block.error();

    return makeWhile(condition.value(), static_cast<BlockStmt*>(while_block.value()));
}

ErrorOr<Stmt*> Parser::parseIndentedBlock()
{
    if (check(tok::TokenType::NEWLINE))
        advance();

    if (!consume(tok::TokenType::INDENT))
        return reportError(ErrorCode::EXPECTED_INDENT);

    Array<Stmt*> statements;

    if (check(tok::TokenType::DEDENT)) {
        advance();
        return makeBlock(statements);
    }

    while (!check(tok::TokenType::DEDENT) && !weDone()) {
        skipNewlines();
        if (check(tok::TokenType::DEDENT))
            break;

        auto stmt = parseStatement();

        if (stmt.hasValue())
            statements.push(stmt.value());
        else {
            synchronize();
            if (check(tok::TokenType::DEDENT) || weDone())
                break;
        }
    }

    if (check(tok::TokenType::ENDMARKER))
        return makeBlock(statements);
    else if (!consume(tok::TokenType::DEDENT))
        return reportError(ErrorCode::EXPECTED_DEDENT);

    return makeBlock(statements);
}

ErrorOr<Expr*> Parser::parseParametersList()
{
    if (!consume(tok::TokenType::LPAREN))
        return reportError(ErrorCode::EXPECTED_LPAREN);

    Array<Expr*> parameters = Array<Expr*>::withCapacity(4); // typical small function

    if (!check(tok::TokenType::RPAREN)) {
        do {
            skipNewlines();
            if (check(tok::TokenType::RPAREN))
                break;

            if (!check(tok::TokenType::IDENTIFIER))
                return reportError(ErrorCode::EXPECTED_PARAM_NAME);

            StringRef param_name = currentToken()->lexeme();
            advance();
            parameters.push(makeName(param_name));
            skipNewlines();
        } while (match(tok::TokenType::COMMA) && !check(tok::TokenType::RPAREN));
    }

    if (!consume(tok::TokenType::RPAREN))
        return reportError(ErrorCode::EXPECTED_RPAREN_EXPR);

    if (parameters.empty())
        return makeList(Array<Expr*> { });

    return makeList(parameters);
}

ErrorOr<Expr*> Parser::parseExpression() { return parseAssignmentExpr(); }

ErrorOr<Stmt*> Parser::parseFunctionDef()
{
    if (!consume(tok::TokenType::KW_FN))
        return reportError(ErrorCode::EXPECTED_FN_KEYWORD);

    if (!check(tok::TokenType::IDENTIFIER))
        return reportError(ErrorCode::EXPECTED_FN_NAME);

    StringRef function_name = currentToken()->lexeme();
    advance();

    auto parameters_list = parseParametersList();
    if (parameters_list.hasError())
        return parameters_list.error();

    if (!consume(tok::TokenType::COLON))
        return reportError(ErrorCode::EXPECTED_COLON_FN);

    auto function_body = parseIndentedBlock();
    if (function_body.hasError())
        return function_body.error();

    return makeFunction(makeName(function_name), static_cast<ListExpr*>(parameters_list.value()), static_cast<BlockStmt*>(function_body.value()));
}

ErrorOr<Stmt*> Parser::parseIfStmt()
{
    if (!consume(tok::TokenType::KW_IF))
        return reportError(ErrorCode::EXPECTED_IF_KEYWORD);

    auto condition = parseExpression();
    if (condition.hasError())
        return condition.error();

    if (!consume(tok::TokenType::COLON))
        return reportError(ErrorCode::EXPECTED_COLON_IF);

    auto then_block = parseIndentedBlock();
    if (then_block.hasError())
        return then_block.error();

    Stmt* else_block = nullptr;

    skipNewlines();

    if (consume(tok::TokenType::KW_ELSE)) {
        skipNewlines();

        if (check(tok::TokenType::KW_IF)) {
            // else-if: treat as nested if inside the else clause
            auto nested = parseIfStmt();
            if (nested.hasError())
                return nested.error();
            else_block = nested.value();
        } else {
            if (!consume(tok::TokenType::COLON))
                return reportError(ErrorCode::EXPECTED_COLON_IF);
            auto else_stmt = parseIndentedBlock();
            if (else_stmt.hasError())
                return else_stmt.error();
            else_block = else_stmt.value();
        }
    }

    return makeIf(
        condition.value(),
        static_cast<BlockStmt*>(then_block.value()),
        else_block);
}

ErrorOr<Stmt*> Parser::parseExpressionStmt()
{
    auto expr = parseExpression();
    if (expr.hasError())
        return expr.error();

    return makeExprStmt(expr.value());
}

ErrorOr<Expr*> Parser::parseAssignmentExpr()
{
    auto L = parseConditionalExpr();
    if (L.hasError())
        return L.error();

    if (check(tok::TokenType::OP_ASSIGN)) {
        Expr* target = L.value();
        Expr::Kind kind = target->getKind();

        if (kind != Expr::Kind::NAME && kind != Expr::Kind::INDEX)
            return reportError(ErrorCode::INVALID_ASSIGN_TARGET);

        advance();

        auto R = parseAssignmentExpr();
        if (R.hasError())
            return R.error();

        if (kind == Expr::Kind::NAME) {
            NameExpr* name_target = static_cast<NameExpr*>(target);
            bool decl = !Sema_.getCurrentScope()->isDefined(name_target->getValue());
            return ErrorOr<Expr*>::fromValue(makeAssignmentExpr(name_target, R.value(), decl));
        }
        return ErrorOr<Expr*>::fromValue(makeAssignmentExpr(target, R.value(), /*decl=*/false));
    }

    return L.value();
}

ErrorOr<Expr*> Parser::parseLogicalExprPrecedence(unsigned int min_precedence)
{
    auto L = parseComparisonExpr();
    if (L.hasError())
        return L.error();

    for (;;) {
        tok::Token const* tok = currentToken();
        if (!tok->isBinaryOp())
            break;

        unsigned int precedence = tok->getPrecedence();
        if (precedence == tok::PREC_NONE || precedence < min_precedence)
            break;

        tok::TokenType op = Lexer_.current()->type();
        advance();

        auto R = parseLogicalExprPrecedence(precedence + 1);
        if (R.hasError())
            return R.error();

        L.setValue(makeBinary(L.value(), R.value(), toBinaryOp(op)));
    }

    return L.value();
}

ErrorOr<Expr*> Parser::parseComparisonExpr()
{
    auto L = parseBinaryExpr();
    if (L.hasError())
        return L.error();

    if (currentToken()->isComparisonOp()) {
        tok::TokenType op = Lexer_.current()->type();
        advance();
        auto R = parseBinaryExpr();
        if (R.hasError())
            return R.error();

        L.setValue(makeBinary(L.value(), R.value(), toBinaryOp(op)));
    }

    return L.value();
}

ErrorOr<Expr*> Parser::parseBinaryExpr() { return parseBinaryExprPrecedence(0); }

ErrorOr<Expr*> Parser::parseBinaryExprPrecedence(unsigned int min_precedence)
{
    auto L = parseUnaryExpr();
    if (L.hasError())
        return L.error();

    for (;;) {
        tok::Token const* tok = currentToken();
        if (!tok->isBinaryOp() || tok->is(tok::TokenType::OP_ASSIGN))
            break;

        unsigned int precedence = tok->getPrecedence();
        if (precedence == tok::PREC_NONE || precedence < min_precedence)
            break;

        tok::TokenType op = Lexer_.current()->type();
        advance();

        unsigned int nextMin = precedence + 1;
        auto R = parseBinaryExprPrecedence(nextMin);
        if (R.hasError())
            return R.error();

        L.setValue(makeBinary(L.value(), R.value(), toBinaryOp(op)));
    }

    return L.value();
}

ErrorOr<Expr*> Parser::parseUnaryExpr()
{
    tok::Token const* tok = currentToken();
    if (tok->isUnaryOp()) {
        tok::TokenType op = Lexer_.current()->type();
        advance();

        auto expr = parseUnaryExpr();
        if (expr.hasError())
            return expr.error();

        return makeUnary(expr.value(), toUnaryOp(op));
    }
    return parsePostfixExpr();
}

ErrorOr<Expr*> Parser::parsePostfixExpr()
{
    auto expr_or = parsePrimaryExpr();
    if (expr_or.hasError())
        return expr_or.error();

    Expr* expr = expr_or.value();

    for (;;) {
        if (check(tok::TokenType::LPAREN)) {
            advance();

            Array<Expr*> args = Array<Expr*>::withCapacity(4);

            if (!check(tok::TokenType::RPAREN)) {
                do {
                    skipNewlines();
                    if (check(tok::TokenType::RPAREN))
                        break;

                    auto arg = parseExpression();
                    if (arg.hasError())
                        return arg.error();

                    args.push(arg.value());
                    skipNewlines();
                } while (match(tok::TokenType::COMMA) && !check(tok::TokenType::RPAREN));
            }

            if (!consume(tok::TokenType::RPAREN))
                return reportError(ErrorCode::EXPECTED_RPAREN_EXPR);

            expr = makeCall(expr, makeList(std::move(args)));
            continue;
        }

        if (check(tok::TokenType::LBRACKET)) {
            advance();

            auto index = parseExpression();
            if (index.hasError())
                return index.error();

            if (!consume(tok::TokenType::RBRACKET))
                return reportError(ErrorCode::EXPECTED_RBRACKET);

            expr = makeIndex(expr, index.value());
            continue;
        }

        break;
    }

    return expr;
}

bool Parser::weDone() const { return Lexer_.current()->is(tok::TokenType::ENDMARKER); }

bool Parser::check(tok::TokenType type) { return Lexer_.current()->is(type); }

tok::Token const* Parser::currentToken() { return Lexer_.current(); }

ErrorOr<Expr*> Parser::parse() { return parseExpression(); }

ErrorOr<Expr*> Parser::parsePrimaryExpr()
{
    tok::Token const* tok = currentToken();

    if (currentToken()->isNumeric()) {
        StringRef v = tok->lexeme();
        advance();

        LiteralExpr::Type type;
        tok::TokenType tt = tok->type();

        if (tt == tok::TokenType::DECIMAL)
            return makeLiteralFloat(v.toDouble());

        if (tt == tok::TokenType::INTEGER || tt == tok::TokenType::HEX || tt == tok::TokenType::OCTAL || tt == tok::TokenType::BINARY)
            return makeLiteralInt(util::parseIntegerLiteral(v));
    }

    if (check(tok::TokenType::STRING)) {
        StringRef v = tok->lexeme();
        advance();
        return makeLiteralString(v);
    }

    if (check(tok::TokenType::KW_TRUE) || check(tok::TokenType::KW_FALSE)) {
        StringRef v = tok->lexeme();
        advance();
        return makeLiteralBool(v == "صحيح" ? true : false);
    }

    if (check(tok::TokenType::KW_NONE)) {
        advance();
        return makeLiteralNil();
    }

    if (check(tok::TokenType::IDENTIFIER)) {
        StringRef name = tok->lexeme();
        advance();
        return makeName(name);
    }

    if (check(tok::TokenType::LPAREN)) {
        advance();
        if (check(tok::TokenType::RPAREN)) {
            advance();
            return makeList(Array<Expr*> { });
        }

        auto expr = parseExpression();

        if (expr.hasError())
            return expr.error();

        if (!consume(tok::TokenType::RPAREN))
            return reportError(ErrorCode::EXPECTED_RPAREN_EXPR);

        return expr.value();
    }

    if (check(tok::TokenType::LBRACKET)) {
        advance();
        return parseListLiteral();
    }

    if (weDone())
        return reportError(ErrorCode::UNEXPECTED_EOF);
    skipNewlines();
    return reportError(ErrorCode::UNEXPECTED_TOKEN);
}

ErrorOr<Expr*> Parser::parseListLiteral()
{
    Array<Expr*> elements = Array<Expr*>::withCapacity(4);

    if (!check(tok::TokenType::RBRACKET)) {
        do {
            skipNewlines();
            if (check(tok::TokenType::RBRACKET))
                break;

            auto elem = parseExpression();
            if (elem.hasError())
                return elem.error();

            elements.push(elem.value());
            skipNewlines();
        } while (match(tok::TokenType::COMMA) && !check(tok::TokenType::RBRACKET));
    }

    if (!consume(tok::TokenType::RBRACKET))
        return reportError(ErrorCode::EXPECTED_RBRACKET);

    return makeList(std::move(elements));
}

ErrorOr<Expr*> Parser::parseConditionalExpr()
{
    return parseLogicalExpr();
    /// TODO: Ternary?
}

ErrorOr<Expr*> Parser::parseLogicalExpr() { return parseLogicalExprPrecedence(0); }

bool Parser::match(tok::TokenType const type)
{
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

void Parser::synchronize()
{
    while (!weDone()) {
        if (check(tok::TokenType::NEWLINE) || check(tok::TokenType::DEDENT)) {
            advance();
            return;
        }

        if (check(tok::TokenType::KW_IF) || check(tok::TokenType::KW_WHILE) || check(tok::TokenType::KW_RETURN) || check(tok::TokenType::KW_FN))
            return;

        advance();
    }
}

namespace {

bool isDefinitelyIntegerExpr(Expr const* expr)
{
    if (!expr)
        return false;

    switch (expr->getKind()) {
    case Expr::Kind::LITERAL: {
        auto const* lit = static_cast<LiteralExpr const*>(expr);
        return lit->isInteger();
    }
    case Expr::Kind::UNARY: {
        auto const* un = static_cast<UnaryExpr const*>(expr);
        UnaryOp op = un->getOperator();
        if (op != UnaryOp::OP_PLUS && op != UnaryOp::OP_NEG)
            return false;

        return isDefinitelyIntegerExpr(un->getOperand());
    }
    case Expr::Kind::BINARY: {
        auto const* bin = static_cast<BinaryExpr const*>(expr);
        if (!isDefinitelyIntegerExpr(bin->getLeft()) || !isDefinitelyIntegerExpr(bin->getRight()))
            return false;

        BinaryOp op = bin->getOperator();
        return op == BinaryOp::OP_ADD || op == BinaryOp::OP_SUB || op == BinaryOp::OP_MUL || op == BinaryOp::OP_MOD;
    }
    default:
        return false;
    }
}

} // namespace

std::optional<double> ASTOptimizer::evaluateConstant(Expr const* expr)
{
    if (!expr)
        return std::nullopt;

    if (expr->getKind() == Expr::Kind::LITERAL) {
        auto lit = dynamic_cast<LiteralExpr const*>(expr);

        if (lit->isNumeric())
            return lit->toNumber();
    } else if (expr->getKind() == Expr::Kind::BINARY) {
        auto bin = dynamic_cast<BinaryExpr const*>(expr);

        std::optional<double> L = evaluateConstant(bin->getLeft());
        std::optional<double> R = evaluateConstant(bin->getRight());

        if (!L || !R)
            return std::nullopt;

        switch (bin->getOperator()) {
        case BinaryOp::OP_ADD:
            return *L + *R;
        case BinaryOp::OP_SUB:
            return *L - *R;
        case BinaryOp::OP_MUL:
            return *L * *R;
        case BinaryOp::OP_POW:
            return std::pow(*L, *R);
        case BinaryOp::OP_DIV:
            if (*R == 0.0)
                return std::nullopt;

            return *L / *R;
        case BinaryOp::OP_MOD:
            if (*R == 0.0)
                return std::nullopt;

            return std::fmod(*L, *R);
        default:
            return std::nullopt;
        }
    } else if (expr->getKind() == Expr::Kind::UNARY) {
        auto un = dynamic_cast<UnaryExpr const*>(expr);
        std::optional<double> operand = evaluateConstant(un->getOperand());

        if (!operand)
            return std::nullopt;
        if (un->getOperator() == UnaryOp::OP_PLUS)
            return *operand;
        if (un->getOperator() == UnaryOp::OP_NEG)
            return -*operand;
    }

    return std::nullopt;
}

Expr* ASTOptimizer::optimizeConstantFolding(Expr* expr)
{
    if (!expr)
        return nullptr;

    if (expr->getKind() == Expr::Kind::BINARY) {
        auto bin = dynamic_cast<BinaryExpr*>(expr);

        bin->setLeft(optimizeConstantFolding(bin->getLeft()));
        bin->setRight(optimizeConstantFolding(bin->getRight()));

        if (std::optional<double> val = evaluateConstant(expr)) {
            ++Stats_.ConstantFolds;
            return makeLiteralFloat(*val);
        }

        Expr* L = bin->getLeft();
        Expr* R = bin->getRight();

        if (!L || !R)
            return expr->clone();

        BinaryOp op = bin->getOperator();

        Expr::Kind r_kind = R->getKind();
        Expr::Kind l_kind = L->getKind();

        // x + 0 = x, x - 0 = x
        if ((op == BinaryOp::OP_ADD || op == BinaryOp::OP_SUB) && r_kind == Expr::Kind::LITERAL) {
            auto lit = static_cast<LiteralExpr*>(R);
            if (lit->isNumeric() && lit->toNumber() == 0) {
                ++Stats_.StrengthReductions;
                return L->clone();
            }
        }

        // inverse (0 - x != x, but 0 + x = x)
        if (op == BinaryOp::OP_ADD && l_kind == Expr::Kind::LITERAL) {
            auto lit = static_cast<LiteralExpr*>(L);
            if (lit->isNumeric() && lit->toNumber() == 0) {
                ++Stats_.StrengthReductions;
                return R->clone();
            }
        }

        // x * 1 = x, x / 1 = x
        if ((op == BinaryOp::OP_MUL || op == BinaryOp::OP_DIV) && r_kind == Expr::Kind::LITERAL) {
            auto lit = static_cast<LiteralExpr*>(R);
            if (lit->isNumeric() && lit->toNumber() == 1) {
                ++Stats_.StrengthReductions;
                return L->clone();
            }
        }

        // 1 * x
        if (op == BinaryOp::OP_MUL && l_kind == Expr::Kind::LITERAL) {
            auto lit = static_cast<LiteralExpr*>(L);
            if (lit->isNumeric() && lit->toNumber() == 1) {
                ++Stats_.StrengthReductions;
                return R->clone();
            }
        }

        // x * 0 = 0 (only when L side is definitely integer; IEEE floats can yield
        // NaN for inf * 0)
        if (op == BinaryOp::OP_MUL && r_kind == Expr::Kind::LITERAL) {
            auto r_lit = static_cast<LiteralExpr*>(R);
            if (r_lit->isNumeric() && r_lit->toNumber() == 0 /*&& isDefinitelyIntegerExpr(L)*/) {
                ++Stats_.StrengthReductions;
                return makeLiteralInt(0);
            }
        }

        // x * 2 = x + x (strength reduction)
        if (op == BinaryOp::OP_MUL && r_kind == Expr::Kind::LITERAL) {
            auto lit = static_cast<LiteralExpr*>(R);
            if (lit->isNumeric() && lit->toNumber() == 2) {
                ++Stats_.StrengthReductions;
                return makeBinary(L->clone(), L->clone(), BinaryOp::OP_ADD);
            }
        }

        if (op == BinaryOp::OP_MUL && l_kind == Expr::Kind::LITERAL) {
            auto lit = static_cast<LiteralExpr*>(L);
            if (lit->isNumeric() && lit->toNumber() == 2) {
                ++Stats_.StrengthReductions;
                return makeBinary(R->clone(), R->clone(), BinaryOp::OP_ADD);
            }
        }

        // x - x = 0
        if (op == BinaryOp::OP_SUB) {
            if (L->equals(R)) {
                ++Stats_.StrengthReductions;
                return makeLiteralInt(0);
            }
        }

        // string concatenation
        if (l_kind == Expr::Kind::LITERAL && r_kind == Expr::Kind::LITERAL && op == BinaryOp::OP_ADD) {
            auto l_lit = static_cast<LiteralExpr*>(L);
            auto r_lit = static_cast<LiteralExpr*>(R);

            if (l_lit->getType() == LiteralExpr::Type::STRING && r_lit->getType() == LiteralExpr::Type::STRING)
                return makeLiteralString(l_lit->getStr() + r_lit->getStr());
        }
    } else if (expr->getKind() == Expr::Kind::UNARY) {
        auto outer = static_cast<UnaryExpr*>(expr);
        Expr* optimizedOperand = optimizeConstantFolding(outer->getOperand());
        bool operandChanged = (optimizedOperand != outer->getOperand());
        UnaryExpr* rebuiltUnary = nullptr;

        if (operandChanged)
            rebuiltUnary = makeUnary(optimizedOperand, outer->getOperator());
        else
            rebuiltUnary = outer->clone();

        if (auto val = evaluateConstant(rebuiltUnary)) {
            ++Stats_.ConstantFolds;
            return makeLiteralFloat(*val);
        }

        if (rebuiltUnary->getOperator() == UnaryOp::OP_NEG) {
            if (auto inner = static_cast<UnaryExpr*>(optimizedOperand)) {
                if (inner->getOperator() == UnaryOp::OP_NEG) {
                    ++Stats_.StrengthReductions;
                    return inner->getOperand()->clone();
                }
            }
        }

        return rebuiltUnary;
    } else if (expr->getKind() == Expr::Kind::CALL) {
        auto call = static_cast<CallExpr*>(expr);
        for (Expr*& arg : call->getArgs())
            arg = optimizeConstantFolding(arg);
    } else if (expr->getKind() == Expr::Kind::LIST) {
        auto list = static_cast<ListExpr*>(expr);
        for (Expr*& elem : list->getElements())
            elem = optimizeConstantFolding(elem);
    }

    return expr->clone();
}

Stmt* ASTOptimizer::eliminateDeadCode(Stmt* stmt)
{
    if (!stmt)
        return nullptr;

    if (stmt->getKind() == Stmt::Kind::IF) {
        auto ifStmt = static_cast<IfStmt*>(stmt);

        if (ifStmt->getCondition()->getKind() == Expr::Kind::LITERAL) {
            auto lit = static_cast<LiteralExpr*>(ifStmt->getCondition());
            if (lit->getType() == LiteralExpr::Type::BOOLEAN) {
                ++Stats_.DeadCodeEliminations;
                if (lit->getBool() == true)
                    return ifStmt->getThen()->clone();
                else {
                    auto* elseBlock = ifStmt->getElse();
                    if (elseBlock)
                        return ifStmt->getElse()->clone();
                    else
                        return nullptr;
                }
            }
        }

        IfStmt* clone = ifStmt->clone();

        clone->setThen(eliminateDeadCode(clone->getThen()));
        clone->setElse(eliminateDeadCode(clone->getElse()));

        return clone;
    } else if (stmt->getKind() == Stmt::Kind::WHILE) {
        auto while_stmt = static_cast<WhileStmt*>(stmt);

        if (while_stmt->getCondition()->getKind() == Expr::Kind::LITERAL) {
            auto lit = static_cast<LiteralExpr*>(while_stmt->getCondition());
            if (lit->getType() == LiteralExpr::Type::BOOLEAN && lit->getBool() == false) {
                ++Stats_.DeadCodeEliminations;
                return nullptr; // kill loop
            }
        }

        WhileStmt* clone = while_stmt->clone();
        clone->setBody(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getBody())));
        return clone;
    } else if (stmt->getKind() == Stmt::Kind::FOR) {
        ForStmt* for_stmt = dynamic_cast<ForStmt*>(stmt);
        /// TODO: evaluate for loop condition and kill it if it doesn't run
        ForStmt* clone = for_stmt->clone();
        clone->setBody(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getBody())));
        return clone;
    } else if (stmt->getKind() == Stmt::Kind::FUNC) {
        FunctionDef* funcDef = dynamic_cast<FunctionDef*>(stmt);
        FunctionDef* clone = funcDef->clone();
        clone->setBody(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getBody())));
        return clone;
    } else if (stmt->getKind() == Stmt::Kind::BLOCK) {
        BlockStmt* block_stmt = dynamic_cast<BlockStmt*>(stmt);
        Array<Stmt*> new_stmts = Array<Stmt*>::withCapacity(block_stmt->getStatements().size());
        bool seen_return = false;
        for (Stmt* s : block_stmt->getStatements()) {
            if (seen_return) {
                Stats_.DeadCodeEliminations++;
                break;
            }

            if (s->getKind() == Stmt::Kind::RETURN)
                seen_return = true;

            new_stmts.push(eliminateDeadCode(s));
        }

        return makeBlock(new_stmts);
    }

    return stmt->clone();
}

StringRef ASTOptimizer::CSEPass::exprToString(Expr const* expr)
{
    if (!expr)
        return "";

    switch (expr->getKind()) {
    case Expr::Kind::LITERAL: {
        LiteralExpr const* lit = dynamic_cast<LiteralExpr const*>(expr);
        if (lit->isString())
            return lit->getStr();
        else if (lit->isNumeric())
            return std::to_string(lit->toNumber()).data();
        else if (lit->isBoolean())
            return lit->getBool() ? "true" : "false";
        else
            return "";
    }
    case Expr::Kind::NAME: {
        NameExpr const* name = dynamic_cast<NameExpr const*>(expr);
        return name->getValue();
    }
    case Expr::Kind::BINARY: {
    } break;
    case Expr::Kind::UNARY: {
    } break;
    default:
        return "";
    }

    return "";
}

StringRef ASTOptimizer::CSEPass::getTempVar() { return StringRef("__cse_temp_") + static_cast<char>(TempCounter_++); }

std::optional<StringRef> ASTOptimizer::CSEPass::findCSE(Expr const* expr)
{
    StringRef exprStr = exprToString(expr);
    if (exprStr.empty())
        return std::nullopt;

    auto it = ExprCache_.find(exprStr);
    if (it != ExprCache_.end())
        return it->second;

    return std::nullopt;
}

void ASTOptimizer::CSEPass::recordExpr(Expr const* expr, StringRef const& var)
{
    StringRef exprStr = exprToString(expr);
    if (!exprStr.empty())
        ExprCache_[exprStr] = var;
}

bool ASTOptimizer::isLoopInvariant(Expr const* expr, std::unordered_set<StringRef, StringRefHash, StringRefEqual> const& loopVars)
{
    if (!expr)
        return true;

    if (expr->getKind() == Expr::Kind::NAME) {
        NameExpr const* name = dynamic_cast<NameExpr const*>(expr);
        return !loopVars.count(name->getValue());
    } else if (expr->getKind() == Expr::Kind::BINARY) {
        BinaryExpr const* bin = dynamic_cast<BinaryExpr const*>(expr);
        return isLoopInvariant(bin->getLeft(), loopVars) && isLoopInvariant(bin->getRight(), loopVars);
    } else if (expr->getKind() == Expr::Kind::UNARY) {
        UnaryExpr const* un = dynamic_cast<UnaryExpr const*>(expr);
        return isLoopInvariant(un->getOperand(), loopVars);
    } else if (expr->getKind() == Expr::Kind::LITERAL)
        return true;

    return false;
}

Array<Stmt*> ASTOptimizer::optimize(Array<Stmt*> statements, int32_t level)
{
    Array<Stmt*> result;
    for (Stmt*& stmt : statements) {
        if (level >= 1) {
            if (stmt->getKind() == Stmt::Kind::ASSIGNMENT) {
                AssignmentStmt* assign = dynamic_cast<AssignmentStmt*>(stmt);
                assign->setValue(optimizeConstantFolding(assign->getValue()));
            } else if (stmt->getKind() == Stmt::Kind::EXPR) {
                ExprStmt* expr_stmt = dynamic_cast<ExprStmt*>(stmt);
                expr_stmt->setExpr(optimizeConstantFolding(expr_stmt->getExpr()));
            }
        }

        if (level >= 2)
            stmt = eliminateDeadCode(stmt);
        if (stmt)
            result.push(stmt);
    }
    return result;
}

typename ASTOptimizer::OptimizationStats const& ASTOptimizer::getStats() const { return Stats_; }

void ASTOptimizer::printStats() const
{
    std::cout << "\nOptimization Statistics :\n";
    std::cout << "Constant folds             : " << Stats_.ConstantFolds << "\n";
    std::cout << "Dead code eliminations     : " << Stats_.DeadCodeEliminations << "\n";
    std::cout << "Strength reductions        : " << Stats_.StrengthReductions << "\n";
    std::cout << "Common subexpr eliminations: " << Stats_.CommonSubexprEliminations << "\n";
    std::cout << "Loop invariants moved      : " << Stats_.LoopInvariants << "\n";
    std::cout << "Total optimizations        : "
              << (Stats_.ConstantFolds + Stats_.DeadCodeEliminations + Stats_.StrengthReductions + Stats_.CommonSubexprEliminations + Stats_.LoopInvariants)
              << "\n";
}

} // namespace mylang::parser
