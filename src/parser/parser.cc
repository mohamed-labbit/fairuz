#include "../../include/parser/parser.hpp"
#include "../../include/diag/diagnostic.hpp"

#include <cassert>

namespace mylang {
namespace parser {

std::vector<ast::Stmt*> Parser::parseProgram()
{
    std::vector<ast::Stmt*> statements;

    while (!weDone()) {
        skipNewlines();
        if (weDone())
            break;

        ast::Stmt* stmt = parseStatement();

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

ast::Stmt* Parser::parseStatement()
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

ast::Stmt* Parser::parseReturnStmt()
{
    if (!consume(tok::TokenType::KW_RETURN, "Expected 'return' statement"))
        return nullptr;

    if (check(tok::TokenType::NEWLINE) || weDone())
        return nullptr;

    ast::Expr* value = parseExpression();
    return ast::AST_allocator.make<ast::ReturnStmt>(value);
}

ast::Stmt* Parser::parseWhileStmt()
{
    if (!consume(tok::TokenType::KW_WHILE, "Expected 'while' keyword"))
        return nullptr;

    ast::Expr* condition = parseExpression();
    if (!condition) {
        diagnostic::engine.emit("Expected condition expression after 'while'", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    if (!consume(tok::TokenType::COLON, "Expected ':' after while condition"))
        return nullptr;

    ast::BlockStmt* while_block = parseIndentedBlock();
    if (!while_block) {
        diagnostic::engine.emit("Expected indented block after while statement", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    return ast::AST_allocator.make<ast::WhileStmt>(condition, while_block);
}

ast::BlockStmt* Parser::parseIndentedBlock()
{
    if (check(tok::TokenType::NEWLINE))
        advance();

    if (!consume(tok::TokenType::INDENT, "Expected indented block"))
        return nullptr;

    std::vector<ast::Stmt*> statements;

    if (check(tok::TokenType::DEDENT)) {
        advance();
        return ast::AST_allocator.make<ast::BlockStmt>(statements);
    }

    while (!check(tok::TokenType::DEDENT) && !weDone()) {
        skipNewlines();
        if (check(tok::TokenType::DEDENT))
            break;

        ast::Stmt* stmt = parseStatement();

        if (stmt)
            statements.push_back(stmt);
        else {
            synchronize();
            if (check(tok::TokenType::DEDENT) || weDone())
                break;
        }
    }

    if (check(tok::TokenType::ENDMARKER))
        return ast::AST_allocator.make<ast::BlockStmt>(statements);
    else if (!consume(tok::TokenType::DEDENT, "Expected dedent after block"))
        return nullptr;

    return ast::AST_allocator.make<ast::BlockStmt>(statements);
}

ast::ListExpr* Parser::parseParametersList()
{
    if (!consume(tok::TokenType::LPAREN, "Expected '(' before parameters"))
        return nullptr;

    std::vector<ast::Expr*> parameters;
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

            StringRef param_name = Lexer_.current()->lexeme();
            advance();
            parameters.push_back(ast::AST_allocator.make<ast::NameExpr>(param_name));
            skipNewlines();
        } while (match(tok::TokenType::COMMA));
    }

    if (!consume(tok::TokenType::RPAREN, "Expected ')' after parameters"))
        return nullptr;

    return ast::AST_allocator.make<ast::ListExpr>(std::move(parameters));
}

ast::Stmt* Parser::parseFunctionDef()
{
    if (!consume(tok::TokenType::KW_FN, "Expected 'fn' keyword"))
        return nullptr;

    if (!check(tok::TokenType::IDENTIFIER)) {
        diagnostic::engine.emit("Expected function name after 'fn'", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    StringRef function_name = Lexer_.current()->lexeme();
    advance();

    ast::ListExpr* parameters_list = parseParametersList();
    if (!parameters_list) {
        diagnostic::engine.emit("Failed to parse parameter list", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    if (!consume(tok::TokenType::COLON, "Expected ':' after function parameters"))
        return nullptr;

    ast::BlockStmt* function_body = parseIndentedBlock();
    if (!function_body) {
        diagnostic::engine.emit("Failed to parse function body", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    ast::NameExpr* name_expr = ast::AST_allocator.make<ast::NameExpr>(function_name);
    return ast::AST_allocator.make<ast::FunctionDef>(name_expr, parameters_list, function_body);
}

ast::Stmt* Parser::parseIfStmt()
{
    if (!consume(tok::TokenType::KW_IF, "Expected 'if' keyword"))
        return nullptr;

    ast::Expr* condition = parseExpression();
    if (!condition) {
        diagnostic::engine.emit("Expected condition expression after 'if'", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    if (!consume(tok::TokenType::COLON, "Expected ':' after if condition"))
        return nullptr;

    ast::BlockStmt* then_block = parseIndentedBlock();
    if (!then_block) {
        diagnostic::engine.emit("Expected indented block after if statement", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    ast::BlockStmt* else_block = nullptr;
    skipNewlines();

    return ast::AST_allocator.make<ast::IfStmt>(condition, then_block, else_block);
}

ast::Stmt* Parser::parseExpressionStmt()
{
    ast::Expr* expr = parseExpression();
    if (!expr)
        return nullptr;

    return ast::AST_allocator.make<ast::ExprStmt>(expr);
}

ast::Expr* Parser::parseAssignmentExpr()
{
    ast::Expr* left = parseConditionalExpr();
    if (!left)
        return nullptr;

    if (check(tok::TokenType::OP_ASSIGN)) {
        if (left->getKind() != ast::Expr::Kind::NAME) {
            diagnostic::engine.emit("Invalid assignment target", diagnostic::DiagnosticEngine::Severity::ERROR);
            return nullptr;
        }

        advance();
        ast::Expr* right = parseAssignmentExpr();
        if (!right)
            return nullptr;

        return ast::AST_allocator.make<ast::AssignmentExpr>(static_cast<ast::NameExpr*>(left), right);
    }

    return left;
}

ast::Expr* Parser::parseLogicalExprPrecedence(unsigned int min_precedence)
{
    ast::Expr* left = parseComparisonExpr();
    if (!left)
        return nullptr;

    for (;;) {
        auto* tok = currentToken();
        int precedence = tok->getLogicalOpPrecedence();
        if (precedence < 0 || precedence < static_cast<int>(min_precedence))
            break;

        tok::TokenType op = Lexer_.current()->type();
        advance();

        ast::Expr* right = parseLogicalExprPrecedence(precedence + 1);
        if (!right) {
            diagnostic::engine.emit("Expected expression after logical operator", diagnostic::DiagnosticEngine::Severity::ERROR);
            return nullptr;
        }

        left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
    }

    return left;
}

ast::Expr* Parser::parseComparisonExpr()
{
    ast::Expr* left = parseBinaryExpr();
    if (!left)
        return nullptr;

    if (currentToken()->isComparisonOp()) {
        tok::TokenType op = Lexer_.current()->type();
        advance();
        ast::Expr* right = parseBinaryExpr();
        if (!right) {
            diagnostic::engine.emit("Expected expression after comparison operator", diagnostic::DiagnosticEngine::Severity::ERROR);
            return nullptr;
        }
        left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
    }

    return left;
}

ast::Expr* Parser::parseBinaryExprPrecedence(unsigned int min_precedence)
{
    ast::Expr* left = parseUnaryExpr();
    if (!left)
        return nullptr;

    for (;;) {
        auto* tok = currentToken();
        int precedence = tok->getArithmeticOpPrecedence();
        if (precedence < 0 || precedence < static_cast<int>(min_precedence))
            break;

        tok::TokenType op = Lexer_.current()->type();
        advance();

        int nextMin = precedence + 1;
        ast::Expr* right = parseBinaryExprPrecedence(nextMin);
        if (!right) {
            diagnostic::engine.emit("Expected expression after binary operator", diagnostic::DiagnosticEngine::Severity::ERROR);
            return nullptr;
        }
        left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
    }
    return left;
}

ast::Expr* Parser::parseUnaryExpr()
{
    auto* tok = currentToken();
    if (tok->isUnaryOp()) {
        tok::TokenType op = Lexer_.current()->type();
        advance();
        ast::Expr* expr = parseUnaryExpr();
        if (!expr) {
            diagnostic::engine.emit("Expected expression after unary operator", diagnostic::DiagnosticEngine::Severity::ERROR);
            return nullptr;
        }
        return ast::AST_allocator.make<ast::UnaryExpr>(expr, op);
    }
    return parsePostfixExpr();
}

ast::Expr* Parser::parsePostfixExpr()
{
    ast::Expr* expr = parsePrimaryExpr();
    if (!expr)
        return nullptr;

    while (check(tok::TokenType::LPAREN)) {
        advance();

        std::vector<ast::Expr*> args;
        args.reserve(4);

        if (!check(tok::TokenType::RPAREN)) {
            do {
                skipNewlines();
                if (check(tok::TokenType::RPAREN))
                    break;

                ast::Expr* arg = parseExpression();
                if (!arg) {
                    diagnostic::engine.emit("Expected expression in argument list", diagnostic::DiagnosticEngine::Severity::ERROR);
                    return nullptr;
                }
                args.push_back(arg);
                skipNewlines();
            } while (match(tok::TokenType::COMMA));
        }

        if (!consume(tok::TokenType::RPAREN, "Expected ')' after arguments"))
            return nullptr;

        expr = ast::AST_allocator.make<ast::CallExpr>(expr, ast::AST_allocator.make<ast::ListExpr>(std::move(args)));
    }

    return expr;
}

ast::Expr* Parser::parsePrimaryExpr()
{
    if (weDone()) {
        diagnostic::engine.emit("Unexpected end of input", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
    }

    auto* tok = Lexer_.current();

    if (check(tok::TokenType::NUMBER)) {
        auto v = tok->lexeme();
        advance();
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, v);
    }

    if (check(tok::TokenType::STRING)) {
        auto v = tok->lexeme();
        advance();
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::STRING, v);
    }

    if (check(tok::TokenType::KW_TRUE) || check(tok::TokenType::KW_FALSE)) {
        auto v = tok->lexeme();
        advance();
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::BOOLEAN, v);
    }

    if (check(tok::TokenType::KW_NONE)) {
        advance();
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NONE, "");
    }

    if (check(tok::TokenType::IDENTIFIER)) {
        auto name = tok->lexeme();
        advance();
        return ast::AST_allocator.make<ast::NameExpr>(name);
    }

    if (check(tok::TokenType::LPAREN)) {
        advance();
        if (check(tok::TokenType::RPAREN)) {
            advance();
            return ast::AST_allocator.make<ast::ListExpr>(std::vector<ast::Expr*> {});
        }

        ast::Expr* expr = parseExpression();

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

    diagnostic::engine.emit("Expected expression", diagnostic::DiagnosticEngine::Severity::ERROR);
    return nullptr;
}

ast::Expr* Parser::parseListLiteral()
{
    std::vector<ast::Expr*> elements;
    elements.reserve(4);

    if (!check(tok::TokenType::RBRACKET)) {
        do {
            skipNewlines();
            if (check(tok::TokenType::RBRACKET))
                break;

            ast::Expr* elem = parseExpression();
            if (!elem) {
                diagnostic::engine.emit("Expected expression in list literal", diagnostic::DiagnosticEngine::Severity::ERROR);
                return nullptr;
            }
            elements.push_back(elem);
            skipNewlines();
        } while (match(tok::TokenType::COMMA));
    }

    if (!consume(tok::TokenType::RBRACKET, "Expected ']' after list elements"))
        return nullptr;

    return ast::AST_allocator.make<ast::ListExpr>(std::move(elements));
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
