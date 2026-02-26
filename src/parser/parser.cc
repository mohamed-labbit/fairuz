#include "../../include/parser/parser.hpp"
#include "../../include/diag/diagnostic.hpp"

#include <cassert>

namespace mylang {
namespace parser {

using namespace ast;

BinaryOp toBinaryOp(tok::TokenType const op)
{
    switch (op) {
    case tok::TokenType::OP_PLUS: return BinaryOp::OP_ADD;
    case tok::TokenType::OP_MINUS: return BinaryOp::OP_SUB;
    case tok::TokenType::OP_STAR: return BinaryOp::OP_MUL;
    case tok::TokenType::OP_SLASH: return BinaryOp::OP_DIV;
    case tok::TokenType::OP_PERCENT: return BinaryOp::OP_MOD;
    case tok::TokenType::OP_POWER: return BinaryOp::OP_POW;
    case tok::TokenType::OP_EQ: return BinaryOp::OP_EQ;
    case tok::TokenType::OP_NEQ: return BinaryOp::OP_NEQ;
    case tok::TokenType::OP_LT: return BinaryOp::OP_LT;
    case tok::TokenType::OP_GT: return BinaryOp::OP_GT;
    case tok::TokenType::OP_LTE: return BinaryOp::OP_LTE;
    case tok::TokenType::OP_GTE: return BinaryOp::OP_GTE;
    case tok::TokenType::OP_BITAND: return BinaryOp::OP_BITAND;
    case tok::TokenType::OP_BITOR: return BinaryOp::OP_BITOR;
    case tok::TokenType::OP_BITXOR: return BinaryOp::OP_BITXOR;
    case tok::TokenType::OP_BITNOT: return BinaryOp::OP_BITNOT;
    case tok::TokenType::OP_LSHIFT: return BinaryOp::OP_LSHIFT;
    case tok::TokenType::OP_RSHIFT: return BinaryOp::OP_RSHIFT;
    case tok::TokenType::OP_AND: return BinaryOp::OP_AND;
    case tok::TokenType::OP_OR: return BinaryOp::OP_OR;
    default:
        return BinaryOp::INVALID;
    }
}

UnaryOp toUnaryOp(tok::TokenType const op)
{
    switch (op) {
    case tok::TokenType::OP_PLUS: return UnaryOp::OP_PLUS;
    case tok::TokenType::OP_MINUS: return UnaryOp::OP_NEG;
    case tok::TokenType::OP_BITNOT: return UnaryOp::OP_BITNOT;
    case tok::TokenType::OP_NOT: return UnaryOp::OP_NOT;
    default:
        return UnaryOp::INVALID;
    }
}

std::vector<Stmt*> Parser::parseProgram()
{
    std::vector<Stmt*> statements;

    while (!weDone()) {
        skipNewlines();
        if (weDone())
            break;

        Stmt* stmt = parseStatement();

        if (stmt)
            statements.push_back(stmt);
        else {
            synchronize();
            if (weDone())
                break;
        }
    }

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
        diagnostic::engine.emit("Expected condition expression after 'while'", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    if (!consume(tok::TokenType::COLON, "Expected ':' after while condition"))
        return nullptr;

    BlockStmt* while_block = parseIndentedBlock();
    if (!while_block) {
        diagnostic::engine.emit("Expected indented block after while statement", diagnostic::DiagnosticEngine::Severity::ERROR);
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

    std::vector<Stmt*> statements;

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
            statements.push_back(stmt);
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

    std::vector<Expr*> parameters;
    parameters.reserve(4); // typical small function

    if (!check(tok::TokenType::RPAREN)) {
        do {
            skipNewlines();
            if (check(tok::TokenType::RPAREN))
                break;

            if (!check(tok::TokenType::IDENTIFIER)) {
                diagnostic::engine.emit("Expected parameter name", diagnostic::DiagnosticEngine::Severity::ERROR);
                return nullptr;
            }

            StringRef param_name = currentToken()->lexeme();
            advance();
            parameters.push_back(makeName(param_name));
            skipNewlines();
        } while (match(tok::TokenType::COMMA) && !check(tok::TokenType::RPAREN));
    }

    if (!consume(tok::TokenType::RPAREN, "Expected ')' after parameters"))
        return nullptr;

    if (parameters.empty())
        return makeList(std::vector<Expr*> {});

    return makeList(std::move(parameters));
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
        diagnostic::engine.emit("Expected function name after 'fn'", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    StringRef function_name = currentToken()->lexeme();
    advance();

    ListExpr* parameters_list = parseParametersList();
    if (!parameters_list) {
        diagnostic::engine.emit("Failed to parse parameter list", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    if (!consume(tok::TokenType::COLON, "Expected ':' after function parameters"))
        return nullptr;

    BlockStmt* function_body = parseIndentedBlock();
    if (!function_body) {
        diagnostic::engine.emit("Failed to parse function body", diagnostic::DiagnosticEngine::Severity::ERROR);
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
        diagnostic::engine.emit("Expected condition expression after 'if'", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    if (!consume(tok::TokenType::COLON, "Expected ':' after if condition"))
        return nullptr;

    BlockStmt* then_block = parseIndentedBlock();
    if (!then_block) {
        diagnostic::engine.emit("Expected indented block after if statement", diagnostic::DiagnosticEngine::Severity::ERROR);
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
    Expr* left = parseConditionalExpr();
    if (!left)
        return nullptr;

    if (check(tok::TokenType::OP_ASSIGN)) {
        if (left->getKind() != Expr::Kind::NAME) {
            diagnostic::engine.emit("Invalid assignment target", diagnostic::DiagnosticEngine::Severity::ERROR);
            return nullptr;
        }

        advance();
        Expr* right = parseAssignmentExpr();
        if (!right)
            return nullptr;

        return makeAssignmentExpr(static_cast<NameExpr*>(left), right);
    }

    return left;
}

Expr* Parser::parseLogicalExprPrecedence(unsigned int min_precedence)
{
    Expr* left = parseComparisonExpr();
    if (!left)
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

        Expr* right = parseLogicalExprPrecedence(precedence + 1);
        if (!right) {
            diagnostic::engine.emit("Expected expression after logical operator", diagnostic::DiagnosticEngine::Severity::ERROR);
            return nullptr;
        }

        left = makeBinary(left, right, toBinaryOp(op));
    }

    return left;
}

Expr* Parser::parseComparisonExpr()
{
    Expr* left = parseBinaryExpr();
    if (!left)
        return nullptr;

    if (currentToken()->isComparisonOp()) {
        tok::TokenType op = Lexer_.current()->type();
        advance();
        Expr* right = parseBinaryExpr();
        if (!right) {
            diagnostic::engine.emit("Expected expression after comparison operator", diagnostic::DiagnosticEngine::Severity::ERROR);
            return nullptr;
        }

        left = makeBinary(left, right, toBinaryOp(op));
    }

    return left;
}

Expr* Parser::parseBinaryExpr()
{
    return parseBinaryExprPrecedence(0);
}

Expr* Parser::parseBinaryExprPrecedence(unsigned int min_precedence)
{
    Expr* left = parseUnaryExpr();
    if (!left)
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
        Expr* right = parseBinaryExprPrecedence(nextMin);
        if (!right) {
            diagnostic::engine.emit("Expected expression after binary operator", diagnostic::DiagnosticEngine::Severity::ERROR);
            return nullptr;
        }

        left = makeBinary(left, right, toBinaryOp(op));
    }

    return left;
}

Expr* Parser::parseUnaryExpr()
{
    tok::Token const* tok = currentToken();
    if (tok->isUnaryOp()) {
        tok::TokenType op = Lexer_.current()->type();
        advance();
        Expr* expr = parseUnaryExpr();
        if (!expr) {
            diagnostic::engine.emit("Expected expression after unary operator", diagnostic::DiagnosticEngine::Severity::ERROR);
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

        std::vector<Expr*> args;
        args.reserve(4);

        if (!check(tok::TokenType::RPAREN)) {
            do {
                skipNewlines();
                if (check(tok::TokenType::RPAREN))
                    break;

                Expr* arg = parseExpression();
                if (!arg) {
                    diagnostic::engine.emit("Expected expression in argument list", diagnostic::DiagnosticEngine::Severity::ERROR);
                    return nullptr;
                }
                args.push_back(arg);
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

        switch (tok->type()) {
        case tok::TokenType::DECIMAL:
            return makeLiteralFloat(v.toDouble());
        case tok::TokenType::INTEGER:
        case tok::TokenType::HEX:
        case tok::TokenType::OCTAL:
        case tok::TokenType::BINARY:
            return makeLiteralInt(util::parseIntegerLiteral(v));
        }
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
            return makeList(std::vector<Expr*> {});
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
        diagnostic::engine.emit("Expected expression", diagnostic::DiagnosticEngine::Severity::ERROR);

    return nullptr;
}

Expr* Parser::parseListLiteral()
{
    std::vector<Expr*> elements;
    elements.reserve(4);

    if (!check(tok::TokenType::RBRACKET)) {
        do {
            skipNewlines();
            if (check(tok::TokenType::RBRACKET))
                break;

            Expr* elem = parseExpression();
            if (!elem) {
                diagnostic::engine.emit("Expected expression in list literal", diagnostic::DiagnosticEngine::Severity::ERROR);
                return nullptr;
            }

            elements.push_back(elem);
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

} // namespace parser
} // namespace mylang
