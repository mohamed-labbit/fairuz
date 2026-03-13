#include "../include/parser.hpp"
#include "../include/diagnostic.hpp"
#include "../include/util.hpp"

#include <cassert>

namespace mylang::parser {

using namespace ast;

// symbol table

SymbolTable::SymbolTable(SymbolTable* p, int32_t level)
    : Parent_(p)
    , ScopeLevel_(level)
{
}

void SymbolTable::define(StringRef const& name, Symbol symbol)
{
    if (lookupLocal(name))
        // emit redeclaration error
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

std::unordered_map<StringRef, typename SymbolTable::Symbol, StringRefHash, StringRefEqual> const& SymbolTable::getSymbols() const
{
    return Symbols_;
}

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
        FunctionDef const* func_def = static_cast<FunctionDef const*>(stmt);
        SymbolTable::Symbol func_sym;
        func_sym.symbolType = SymbolTable::SymbolType::FUNCTION;
        func_sym.dataType = SymbolTable::DataType_t::FUNCTION;
        func_sym.definitionLine = stmt->getLine();
        CurrentScope_->define(func_def->getName()->getValue(), func_sym);

        // Create function scope
        CurrentScope_ = CurrentScope_->createChild();

        for (Expr const* const& param : func_def->getParameters()) {
            SymbolTable::Symbol param_sym;
            param_sym.symbolType = SymbolTable::SymbolType::VARIABLE;
            param_sym.dataType = SymbolTable::DataType_t::ANY;
            CurrentScope_->define(static_cast<NameExpr const*>(param)->getValue(), param_sym);
        }

        for (Stmt const* const& s : func_def->getBody()->getStatements())
            analyzeStmt(s);

        // Check for missing return statement
        bool has_return = false;

        for (Stmt const* const& s : func_def->getBody()->getStatements()) {
            if (s->getKind() == Stmt::Kind::RETURN) {
                has_return = true;
                break;
            }
        }

        if (!has_return)
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

void SemanticAnalyzer::analyze(Array<Stmt*> const& Statements_)
{
    for (Stmt const* const& stmt : Statements_)
        analyzeStmt(stmt);

    // Check for unused variables
    Array<SymbolTable::Symbol*> unused = GlobalScope_->getUnusedSymbols();
    for (SymbolTable::Symbol* sym : unused)
        reportIssue(Issue::Severity::WARNING, "Unused variable: " + sym->name, sym->definitionLine, "Consider removing if not needed");
}

Array<typename SemanticAnalyzer::Issue> const& SemanticAnalyzer::getIssues() const
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
    case tok::TokenType::OP_BITNOT:
        return BinaryOp::OP_BITNOT;
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

        Stmt* stmt = parseStatement();

        if (stmt)
            statements.push(stmt);
        else {
            synchronize();
            if (weDone())
                break;
        }
    }

    Sema_.analyze(statements);

    return statements;
}

Stmt* Parser::parseStatement()
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

Stmt* Parser::parseReturnStmt()
{
    if (!consume(tok::TokenType::KW_RETURN, "Expected 'return' statement"))
        return nullptr;

    if (check(tok::TokenType::NEWLINE) || weDone())
        return nullptr;

    Expr* value = parseExpression();
    return makeReturn(value);
}

Stmt* Parser::parseWhileStmt()
{
    if (!consume(tok::TokenType::KW_WHILE, "Expected 'while' keyword"))
        return nullptr;

    Expr* condition = parseExpression();
    if (!condition) {
        diagnostic::emit("Expected condition expression after 'while'", diagnostic::Severity::ERROR);
        return nullptr;
    }

    if (!consume(tok::TokenType::COLON, "Expected ':' after while condition"))
        return nullptr;

    BlockStmt* while_block = parseIndentedBlock();
    if (!while_block) {
        diagnostic::emit("Expected indented block after while statement", diagnostic::Severity::ERROR);
        return nullptr;
    }

    return makeWhile(condition, while_block);
}

BlockStmt* Parser::parseIndentedBlock()
{
    if (check(tok::TokenType::NEWLINE))
        advance();

    if (!consume(tok::TokenType::INDENT, "Expected indented block"))
        return nullptr;

    Array<Stmt*> statements;

    if (check(tok::TokenType::DEDENT)) {
        advance();
        return makeBlock(statements);
    }

    while (!check(tok::TokenType::DEDENT) && !weDone()) {
        skipNewlines();
        if (check(tok::TokenType::DEDENT))
            break;

        Stmt* stmt = parseStatement();

        if (stmt)
            statements.push(stmt);
        else {
            synchronize();
            if (check(tok::TokenType::DEDENT) || weDone())
                break;
        }
    }

    if (check(tok::TokenType::ENDMARKER))
        return makeBlock(statements);
    else if (!consume(tok::TokenType::DEDENT, "Expected dedent after block"))
        return nullptr;

    return makeBlock(statements);
}

ListExpr* Parser::parseParametersList()
{
    if (!consume(tok::TokenType::LPAREN, "Expected '(' before parameters"))
        return nullptr;

    Array<Expr*> parameters = Array<Expr*>::withCapacity(4); // typical small function

    if (!check(tok::TokenType::RPAREN)) {
        do {
            skipNewlines();
            if (check(tok::TokenType::RPAREN))
                break;

            if (!check(tok::TokenType::IDENTIFIER)) {
                diagnostic::emit("Expected parameter name", diagnostic::Severity::ERROR);
                return nullptr;
            }

            StringRef param_name = currentToken()->lexeme();
            advance();
            parameters.push(makeName(param_name));
            skipNewlines();
        } while (match(tok::TokenType::COMMA) && !check(tok::TokenType::RPAREN));
    }

    if (!consume(tok::TokenType::RPAREN, "Expected ')' after parameters"))
        return nullptr;

    if (parameters.empty())
        return makeList(Array<Expr*> { });

    return makeList(parameters);
}

Expr* Parser::parseExpression()
{
    return parseAssignmentExpr();
}

Stmt* Parser::parseFunctionDef()
{
    if (!consume(tok::TokenType::KW_FN, "Expected 'fn' keyword"))
        return nullptr;

    if (!check(tok::TokenType::IDENTIFIER)) {
        diagnostic::emit("Expected function name after 'fn'", diagnostic::Severity::ERROR);
        return nullptr;
    }

    StringRef function_name = currentToken()->lexeme();
    advance();

    ListExpr* parameters_list = parseParametersList();
    if (!parameters_list) {
        diagnostic::emit("Failed to parse parameter list", diagnostic::Severity::ERROR);
        return nullptr;
    }

    if (!consume(tok::TokenType::COLON, "Expected ':' after function parameters"))
        return nullptr;

    BlockStmt* function_body = parseIndentedBlock();
    if (!function_body) {
        diagnostic::emit("Failed to parse function body", diagnostic::Severity::ERROR);
        return nullptr;
    }

    NameExpr* name_expr = makeName(function_name);
    return makeFunction(name_expr, parameters_list, function_body);
}

Stmt* Parser::parseIfStmt()
{
    if (!consume(tok::TokenType::KW_IF, "Expected 'if' keyword"))
        return nullptr;

    Expr* condition = parseExpression();
    if (!condition) {
        diagnostic::emit("Expected condition expression after 'if'", diagnostic::Severity::ERROR);
        return nullptr;
    }

    if (!consume(tok::TokenType::COLON, "Expected ':' after if condition"))
        return nullptr;

    BlockStmt* then_block = parseIndentedBlock();
    if (!then_block) {
        diagnostic::emit("Expected indented block after if statement", diagnostic::Severity::ERROR);
        return nullptr;
    }

    BlockStmt* else_block = nullptr;
    skipNewlines();

    return makeIf(condition, then_block, else_block);
}

Stmt* Parser::parseExpressionStmt()
{
    Expr* expr = parseExpression();
    if (!expr)
        return nullptr;

    return makeExprStmt(expr);
}

Expr* Parser::parseAssignmentExpr()
{
    Expr* L = parseConditionalExpr();
    if (!L)
        return nullptr;

    if (check(tok::TokenType::OP_ASSIGN)) {
        if (L->getKind() != Expr::Kind::NAME) {
            diagnostic::emit("Invalid assignment target", diagnostic::Severity::ERROR);
            return nullptr;
        }

        advance();
        Expr* R = parseAssignmentExpr();
        if (!R)
            return nullptr;

        // push var to symbol table
        NameExpr* target = static_cast<NameExpr*>(L);
        bool decl = false;
        if (Sema_.getGlobalScope()->isDefined(target->getValue()))
            decl = true;

        return makeAssignmentExpr(target, R, decl);
    }

    return L;
}

Expr* Parser::parseLogicalExprPrecedence(unsigned int min_precedence)
{
    Expr* L = parseComparisonExpr();
    if (!L)
        return nullptr;

    for (;;) {
        tok::Token const* tok = currentToken();
        if (!tok->isBinaryOp())
            break;

        unsigned int precedence = tok->getPrecedence();
        if (precedence == tok::PREC_NONE || precedence < min_precedence)
            break;

        tok::TokenType op = Lexer_.current()->type();
        advance();

        Expr* R = parseLogicalExprPrecedence(precedence + 1);
        if (!R) {
            diagnostic::emit("Expected expression after logical operator", diagnostic::Severity::ERROR);
            return nullptr;
        }

        L = makeBinary(L, R, toBinaryOp(op));
    }

    return L;
}

Expr* Parser::parseComparisonExpr()
{
    Expr* L = parseBinaryExpr();
    if (!L)
        return nullptr;

    if (currentToken()->isComparisonOp()) {
        tok::TokenType op = Lexer_.current()->type();
        advance();
        Expr* R = parseBinaryExpr();
        if (!R) {
            diagnostic::emit("Expected expression after comparison operator", diagnostic::Severity::ERROR);
            return nullptr;
        }

        L = makeBinary(L, R, toBinaryOp(op));
    }

    return L;
}

Expr* Parser::parseBinaryExpr()
{
    return parseBinaryExprPrecedence(0);
}

Expr* Parser::parseBinaryExprPrecedence(unsigned int min_precedence)
{
    Expr* L = parseUnaryExpr();
    if (!L)
        return nullptr;

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
        Expr* R = parseBinaryExprPrecedence(nextMin);
        if (!R) {
            diagnostic::emit("Expected expression after binary operator", diagnostic::Severity::ERROR);
            return nullptr;
        }

        L = makeBinary(L, R, toBinaryOp(op));
    }

    return L;
}

Expr* Parser::parseUnaryExpr()
{
    tok::Token const* tok = currentToken();
    if (tok->isUnaryOp()) {
        tok::TokenType op = Lexer_.current()->type();
        advance();
        Expr* expr = parseUnaryExpr();
        if (!expr) {
            diagnostic::emit("Expected expression after unary operator", diagnostic::Severity::ERROR);
            return nullptr;
        }
        return makeUnary(expr, toUnaryOp(op));
    }
    return parsePostfixExpr();
}

Expr* Parser::parsePostfixExpr()
{
    Expr* expr = parsePrimaryExpr();
    if (!expr)
        return nullptr;

    while (check(tok::TokenType::LPAREN)) {
        advance();

        Array<Expr*> args = Array<Expr*>::withCapacity(4);

        if (!check(tok::TokenType::RPAREN)) {
            do {
                skipNewlines();
                if (check(tok::TokenType::RPAREN))
                    break;

                Expr* arg = parseExpression();
                if (!arg) {
                    diagnostic::emit("Expected expression in argument list", diagnostic::Severity::ERROR);
                    return nullptr;
                }
                args.push(arg);
                skipNewlines();
            } while (match(tok::TokenType::COMMA) && !check(tok::TokenType::RPAREN));
        }

        if (!consume(tok::TokenType::RPAREN, "Expected ')' after arguments"))
            return nullptr;

        expr = makeCall(expr, makeList(std::move(args)));
    }

    return expr;
}

bool Parser::weDone() const
{
    return Lexer_.current()->is(tok::TokenType::ENDMARKER);
}

bool Parser::check(tok::TokenType type)
{
    return Lexer_.current()->is(type);
}

tok::Token const* Parser::currentToken()
{
    return Lexer_.current();
}

Expr* Parser::parse()
{
    return parseExpression();
}

Expr* Parser::parsePrimaryExpr()
{
    tok::Token const* tok = currentToken();

    if (currentToken()->isNumeric()) {
        StringRef v = tok->lexeme();
        advance();

        LiteralExpr::Type type;
        tok::TokenType tt = tok->type();

        if (tt == tok::TokenType::DECIMAL)
            return makeLiteralFloat(v.toDouble());

        if (tt == tok::TokenType::INTEGER
            || tt == tok::TokenType::HEX
            || tt == tok::TokenType::OCTAL
            || tt == tok::TokenType::BINARY)
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
        return makeLiteralString("");
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

        Expr* expr = parseExpression();

        if (!expr)
            return nullptr;

        if (!consume(tok::TokenType::RPAREN, "Expected ')' after expression"))
            return nullptr;

        return expr;
    }

    if (check(tok::TokenType::LBRACKET)) {
        advance();
        return parseListLiteral();
    }

    if (Expecting_)
        diagnostic::emit("Expected expression", diagnostic::Severity::ERROR);

    return nullptr;
}

Expr* Parser::parseListLiteral()
{
    Array<Expr*> elements = Array<Expr*>::withCapacity(4);

    if (!check(tok::TokenType::RBRACKET)) {
        do {
            skipNewlines();
            if (check(tok::TokenType::RBRACKET))
                break;

            Expr* elem = parseExpression();
            if (!elem) {
                diagnostic::emit("Expected expression in list literal", diagnostic::Severity::ERROR);
                return nullptr;
            }

            elements.push(elem);
            skipNewlines();
        } while (match(tok::TokenType::COMMA) && !check(tok::TokenType::RBRACKET));
    }

    if (!consume(tok::TokenType::RBRACKET, "Expected ']' after list elements"))
        return nullptr;

    return makeList(std::move(elements));
}

Expr* Parser::parseConditionalExpr()
{
    return parseLogicalExpr();
    /// TODO: Ternary?
}

Expr* Parser::parseLogicalExpr()
{
    return parseLogicalExprPrecedence(0);
}

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
        return op == BinaryOp::OP_ADD
            || op == BinaryOp::OP_SUB
            || op == BinaryOp::OP_MUL
            || op == BinaryOp::OP_MOD;
    }
    default:
        return false;
    }
}

}

std::optional<double> ASTOptimizer::evaluateConstant(Expr const* expr)
{
    if (!expr)
        return std::nullopt;

    if (expr->getKind() == Expr::Kind::LITERAL) {
        LiteralExpr const* lit = dynamic_cast<LiteralExpr const*>(expr);

        if (lit->isNumeric())
            return lit->toNumber();
    } else if (expr->getKind() == Expr::Kind::BINARY) {
        BinaryExpr const* bin = dynamic_cast<BinaryExpr const*>(expr);

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
        UnaryExpr const* un = dynamic_cast<UnaryExpr const*>(expr);
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

    // First, optimize children
    if (expr->getKind() == Expr::Kind::BINARY) {
        BinaryExpr* bin = dynamic_cast<BinaryExpr*>(expr);

        bin->setLeft(optimizeConstantFolding(bin->getLeft()));
        bin->setRight(optimizeConstantFolding(bin->getRight()));

        // Try to evaluate
        if (std::optional<double> val = evaluateConstant(expr)) {
            ++Stats_.ConstantFolds;
            return makeLiteralFloat(*val);
        }

        // Algebraic simplifications
        Expr* L = bin->getLeft();
        Expr* R = bin->getRight();

        if (!L || !R)
            return expr->clone();

        BinaryOp op = bin->getOperator();

        Expr::Kind r_kind = R->getKind();
        Expr::Kind l_kind = L->getKind();

        // x + 0 = x, x - 0 = x
        if ((op == BinaryOp::OP_ADD || op == BinaryOp::OP_SUB) && r_kind == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(R);
            if (lit->isNumeric() && lit->toNumber() == 0) {
                ++Stats_.StrengthReductions;
                return L->clone();
            }
        }

        // inverse (0 - x != x, but 0 + x = x)
        if (op == BinaryOp::OP_ADD && l_kind == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(L);
            if (lit->isNumeric() && lit->toNumber() == 0) {
                ++Stats_.StrengthReductions;
                return R->clone();
            }
        }

        // x * 1 = x, x / 1 = x
        if ((op == BinaryOp::OP_MUL || op == BinaryOp::OP_DIV) && r_kind == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(R);
            if (lit->isNumeric() && lit->toNumber() == 1) {
                ++Stats_.StrengthReductions;
                return L->clone();
            }
        }

        // 1 * x
        if (op == BinaryOp::OP_MUL && l_kind == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(L);
            if (lit->isNumeric() && lit->toNumber() == 1) {
                ++Stats_.StrengthReductions;
                return R->clone();
            }
        }

        // x * 0 = 0 (only when L side is definitely integer; IEEE floats can yield NaN for inf * 0)
        if (op == BinaryOp::OP_MUL && r_kind == Expr::Kind::LITERAL) {
            auto* r_lit = static_cast<LiteralExpr*>(R);
            if (r_lit->isNumeric() && r_lit->toNumber() == 0 /*&& isDefinitelyIntegerExpr(L)*/) {
                ++Stats_.StrengthReductions;
                return makeLiteralInt(0);
            }
        }

        // x * 2 = x + x (strength reduction)
        if (op == BinaryOp::OP_MUL && r_kind == Expr::Kind::LITERAL) {
            auto* lit = dynamic_cast<LiteralExpr*>(R);
            if (lit->isNumeric() && lit->toNumber() == 2) {
                ++Stats_.StrengthReductions;
                return makeBinary(L->clone(), L->clone(), BinaryOp::OP_ADD);
            }
        }

        if (op == BinaryOp::OP_MUL && l_kind == Expr::Kind::LITERAL) {
            auto* lit = dynamic_cast<LiteralExpr*>(L);
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
            LiteralExpr* l_lit = dynamic_cast<LiteralExpr*>(L);
            LiteralExpr* r_lit = dynamic_cast<LiteralExpr*>(R);

            if (l_lit->getType() == LiteralExpr::Type::STRING && r_lit->getType() == LiteralExpr::Type::STRING)
                return makeLiteralString(l_lit->getStr() + r_lit->getStr());
        }
    } else if (expr->getKind() == Expr::Kind::UNARY) {
        auto* outer = static_cast<UnaryExpr*>(expr);
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
            if (auto* inner = dynamic_cast<UnaryExpr*>(optimizedOperand)) {
                if (inner->getOperator() == UnaryOp::OP_NEG) {
                    ++Stats_.StrengthReductions;
                    return inner->getOperand()->clone();
                }
            }
        }

        return rebuiltUnary;
    } else if (expr->getKind() == Expr::Kind::CALL) {
        CallExpr* call = dynamic_cast<CallExpr*>(expr);
        for (Expr*& arg : call->getArgs())
            arg = optimizeConstantFolding(arg);
    } else if (expr->getKind() == Expr::Kind::LIST) {
        ListExpr* list = dynamic_cast<ListExpr*>(expr);
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
        IfStmt* ifStmt = dynamic_cast<IfStmt*>(stmt);

        // Constant condition elimination
        if (ifStmt->getCondition()->getKind() == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(ifStmt->getCondition());
            if (lit->getType() == LiteralExpr::Type::BOOLEAN) {
                ++Stats_.DeadCodeEliminations;
                if (lit->getBool() == true)
                    // Return then block as block statement
                    return ifStmt->getThenBlock()->clone();
                else {
                    // Return else block or nothing
                    auto* elseBlock = ifStmt->getElseBlock();
                    if (elseBlock && !elseBlock->getStatements().empty())
                        return ifStmt->getElseBlock()->clone();
                    else
                        return nullptr;
                }
            }
        }

        IfStmt* clone = ifStmt->clone();

        clone->setThenBlock(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getThenBlock())));
        clone->setElseBlock(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getElseBlock())));

        return clone;
    } else if (stmt->getKind() == Stmt::Kind::WHILE) {
        WhileStmt* whileStmt = dynamic_cast<WhileStmt*>(stmt);

        // Infinite loop with false condition
        if (whileStmt->getCondition()->getKind() == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(whileStmt->getCondition());
            if (lit->getType() == LiteralExpr::Type::BOOLEAN && lit->getBool() == false) {
                ++Stats_.DeadCodeEliminations;
                return nullptr; // kill loop
            }
        }

        WhileStmt* clone = whileStmt->clone();
        clone->setBlock(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getBlock())));
        return clone;
    } else if (stmt->getKind() == Stmt::Kind::FOR) {
        ForStmt* forStmt = dynamic_cast<ForStmt*>(stmt);
        /// TODO: evaluate for loop condition and kill it if it doesn't run
        ForStmt* clone = forStmt->clone();
        clone->setBlock(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getBlock())));
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
        /*
        BinaryExpr const* bin = dynamic_cast<BinaryExpr const*>(expr);
        return "(" + exprToString(bin->getLeft()) + " " + tok::Token::toString(bin->getOperator()) + " " + exprToString(bin->getRight()) + ")";
        */
        break;
    }
    case Expr::Kind::UNARY: {
        /*
        UnaryExpr const* un = dynamic_cast<UnaryExpr const*>(expr);
        return tok::Token::toString(un->getOperator()) + exprToString(un->getOperand());
        */
        break;
    }
    default:
        return "";
    }

    return "";
}

StringRef ASTOptimizer::CSEPass::getTempVar()
{
    return StringRef("__cse_temp_") + static_cast<char>(TempCounter_++);
}

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
        // Apply optimizations based on level
        if (level >= 1) {
            // O1: Basic optimizations
            if (stmt->getKind() == Stmt::Kind::ASSIGNMENT) {
                AssignmentStmt* assign = dynamic_cast<AssignmentStmt*>(stmt);
                assign->setValue(optimizeConstantFolding(assign->getValue()));
            } else if (stmt->getKind() == Stmt::Kind::EXPR) {
                ExprStmt* exprStmt = dynamic_cast<ExprStmt*>(stmt);
                exprStmt->setExpr(optimizeConstantFolding(exprStmt->getExpr()));
            }
        }

        // O2: Dead code elimination
        if (level >= 2)
            stmt = eliminateDeadCode(stmt);

        if (stmt)
            result.push(stmt);
    }
    return result;
}

typename ASTOptimizer::OptimizationStats const& ASTOptimizer::getStats() const
{
    return Stats_;
}

void ASTOptimizer::printStats() const
{
    std::cout << "\n=== Optimization Statistics ===\n";
    std::cout << "Constant folds: " << Stats_.ConstantFolds << "\n";
    std::cout << "Dead code eliminations: " << Stats_.DeadCodeEliminations << "\n";
    std::cout << "Strength reductions: " << Stats_.StrengthReductions << "\n";
    std::cout << "Common subexpr eliminations: " << Stats_.CommonSubexprEliminations << "\n";
    std::cout << "Loop invariants moved: " << Stats_.LoopInvariants << "\n";
    std::cout << "Total optimizations: "
              << (Stats_.ConstantFolds + Stats_.DeadCodeEliminations + Stats_.StrengthReductions + Stats_.CommonSubexprEliminations
                     + Stats_.LoopInvariants)
              << "\n";
}

} // namespace mylang::parser
