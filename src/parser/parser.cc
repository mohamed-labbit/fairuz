#include "../../include/parser/parser.hpp"
#include "../../include/diag/diagnostic.hpp"

#include <cassert>


namespace mylang {
namespace parser {


std::vector<ast::Stmt*> Parser::parseProgram()
{
  std::vector<ast::Stmt*> statements;

  while (!weDone())
  {
    skipNewlines();
    if (weDone()) break;

    ast::Stmt* stmt = parseStatement();
    if (stmt)
      statements.push_back(stmt);
    else
      // Error recovery: skip to next statement
      // synchronize();
      return {};
  }

  return statements;
}

// ============================================================================
// STATEMENT PARSING
// ============================================================================

ast::Stmt* Parser::parseStatement()
{
  skipNewlines();

  // TODO: Add other statement types as you implement them:
  // - if (check(lex::tok::TokenType::KW_IF)) return parseIfStmt();
  // - if (check(lex::tok::TokenType::KW_WHILE)) return parseWhileStmt();
  // - if (check(lex::tok::TokenType::KW_RETURN)) return parseReturnStmt();
  // - if (check(lex::tok::TokenType::KW_DEF)) return parseFunctionDef();
  // etc.

  // For now, treat everything as an expression statement
  return parseExpressionStmt();
}

ast::Stmt* Parser::parseExpressionStmt()
{
  ast::Expr* expr = parseExpression();
  if (!expr) return nullptr;

  // Wrap the expression in an ExprStmt node
  return ast::AST_allocator.make<ast::ExprStmt>(expr);
}


// ============================================================================
// EXPRESSION PARSING (Keep existing logic - no changes needed)
// ============================================================================

ast::Expr* Parser::parseParenthesizedExpr()
{
  consume(lex::tok::TokenType::LPAREN, u"Expected '('");

  ast::Expr* content = nullptr;
  // If empty parentheses - empty tuple
  if (!check(lex::tok::TokenType::RPAREN))
  {
    content = parseExpression();
    if (!content) return nullptr;
  }

  consume(lex::tok::TokenType::RPAREN, u"Expected ')' after expression(s)");

  // Handle chained calls: (expr)(args)
  return parseCallExpr(content);
}

// Returns either a single Expr* or a ListExpr* (tuple)
ast::Expr* Parser::parseParenthesizedExprContent()
{
  if (check(lex::tok::TokenType::RPAREN))
    // Empty ()
    return ast::AST_allocator.make<ast::ListExpr>(std::vector<ast::Expr*>{});

  ast::Expr* first = parseExpression();
  if (!first) return nullptr;

  // If there's a comma, parse a tuple
  if (match(lex::tok::TokenType::COMMA))
  {
    std::vector<ast::Expr*> elements;
    elements.push_back(first);

    while (!check(lex::tok::TokenType::RPAREN))
    {
      ast::Expr* elem = parseExpression();
      if (!elem) return nullptr;
      elements.push_back(elem);

      if (!match(lex::tok::TokenType::COMMA)) break;
    }

    return ast::AST_allocator.make<ast::ListExpr>(elements);
  }

  // Single expression (not a tuple)
  return first;
}

ast::Expr* Parser::parseCallExpr(ast::Expr* callee)
{
  while (check(lex::tok::TokenType::LPAREN))
  {
    advance();  // '('

    std::vector<ast::Expr*> args;

    if (!check(lex::tok::TokenType::RPAREN))
    {
      do
      {
        ast::Expr* arg = parseExpression();
        if (!arg) return nullptr;
        args.push_back(arg);
      } while (match(lex::tok::TokenType::COMMA));
    }

    consume(lex::tok::TokenType::RPAREN, u"Expected ')' after arguments");
    ast::ListExpr* args_expr = ast::AST_allocator.make<ast::ListExpr>(args);
    callee = ast::AST_allocator.make<ast::CallExpr>(callee, args_expr);
  }

  return callee;
}

ast::Expr* Parser::parseExpression()
{
  // Handle starred expressions (unpacking)
  if (check(lex::tok::TokenType::OP_STAR))
  {
    advance();
    ast::Expr* inner = parseExpression();
    if (!inner) return nullptr;
    // TODO: Create proper StarredExpr AST node
    return inner;  // For now, just return the inner expression
  }
  return parseAssignmentExpr();
}

ast::Expr* Parser::parseAssignmentExpr()
{
  ast::Expr* left = parseConditionalExpr();
  if (!left) return nullptr;

  ast::NameExpr* left_casted = dynamic_cast<ast::NameExpr*>(left);
  if (!left_casted) return left;

  // Check for assignment operators
  if (check(lex::tok::TokenType::OP_ASSIGN))
  {
    advance();
    ast::Expr* right = parseAssignmentExpr();  // Right associative
    if (!right) return nullptr;
    return ast::AST_allocator.make<ast::AssignmentExpr>(left_casted, right);
  }

  return left;
}

ast::Expr* Parser::parseLogicalExprPrecedence(int min_precedence)
{
  ast::Expr* left = parseComparisonExpr();
  if (!left) return nullptr;

  while (true)
  {
    int precedence = getLogicalOperatorPrecedence(Lexer_.current().type());
    if (precedence < min_precedence) break;

    lex::tok::TokenType op = Lexer_.current().type();
    advance();

    // All logical operators are left associative
    ast::Expr* right = parseLogicalExprPrecedence(precedence + 1);
    if (!right) return nullptr;

    left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
  }
  return left;
}

int Parser::getLogicalOperatorPrecedence(lex::tok::TokenType tt)
{
  switch (tt)
  {
  case lex::tok::TokenType::OP_BITOR :  // '|'
  case lex::tok::TokenType::KW_OR :     // 'or'
    return 1;
  case lex::tok::TokenType::OP_BITXOR :  // '^'
    return 2;
  case lex::tok::TokenType::OP_BITAND :  // '&'
  case lex::tok::TokenType::KW_AND :     // 'and'
    return 3;
  default : return -1;  // Not a logical operator
  }
}

ast::Expr* Parser::parseComparisonExpr()
{
  ast::Expr* left = parseBinaryExpr();
  if (!left) return nullptr;

  while (check(lex::tok::TokenType::OP_EQ) || check(lex::tok::TokenType::OP_NEQ) || check(lex::tok::TokenType::OP_LT)
         || check(lex::tok::TokenType::OP_GT) || check(lex::tok::TokenType::OP_LTE) || check(lex::tok::TokenType::OP_GTE))
  {
    lex::tok::TokenType op = Lexer_.current().type();
    advance();
    ast::Expr* right = parseBinaryExpr();
    if (!right) return nullptr;

    left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
  }
  return left;
}

ast::Expr* Parser::parseBinaryExprPrecedence(int min_precedence)
{
  ast::Expr* left = parseUnaryExpr();
  if (!left) return nullptr;

  while (true)
  {
    int precedence = getArithmeticOperatorPrecedence(Lexer_.current().type());
    if (precedence < min_precedence) break;

    lex::tok::TokenType op = Lexer_.current().type();
    advance();

    // Left associative: higher precedence for next level
    int nextMinPrecedence = precedence + 1;
    ast::Expr* right = parseBinaryExprPrecedence(nextMinPrecedence);
    if (!right) return nullptr;

    left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
  }
  return left;
}

int Parser::getArithmeticOperatorPrecedence(const lex::tok::TokenType type)
{
  switch (type)
  {
  case lex::tok::TokenType::OP_STAR :   // *
  case lex::tok::TokenType::OP_SLASH :  // /
    return 3;
  case lex::tok::TokenType::OP_PLUS :   // +
  case lex::tok::TokenType::OP_MINUS :  // -
    return 2;
  default : return -1;  // Not an arithmetic operator
  }
}

ast::Expr* Parser::parseUnaryExpr()
{
  // Check CURRENT token for unary operators
  if (check(lex::tok::TokenType::OP_PLUS) || check(lex::tok::TokenType::OP_MINUS) || check(lex::tok::TokenType::OP_BITNOT)
      || check(lex::tok::TokenType::KW_NOT))
  {
    lex::tok::TokenType op = Lexer_.current().type();
    advance();                           // consume operator
    ast::Expr* expr = parseUnaryExpr();  // parse right side recursively
    if (!expr) return nullptr;
    return ast::AST_allocator.make<ast::UnaryExpr>(expr, op);
  }
  return parsePostfixExpr();
}

ast::Expr* Parser::parsePostfixExpr()
{
  ast::Expr* expr = parsePrimaryExpr();
  if (!expr) return nullptr;

  while (match(lex::tok::TokenType::LPAREN))
  {
    std::vector<ast::Expr*> args;

    if (!check(lex::tok::TokenType::RPAREN))
    {
      do
      {
        // Parse full expressions as arguments (including assignments in some contexts)
        ast::Expr* arg = parseAssignmentExpr();
        if (!arg) return nullptr;
        args.push_back(arg);
      } while (match(lex::tok::TokenType::COMMA));
    }

    consume(lex::tok::TokenType::RPAREN, u"Expected ')' after arguments");

    expr = ast::AST_allocator.make<ast::CallExpr>(expr, ast::AST_allocator.make<ast::ListExpr>(args));
  }

  return expr;
}

ast::Expr* Parser::parsePrimaryExpr()
{
  if (weDone()) return nullptr;

  if (check(lex::tok::TokenType::NUMBER))
  {
    auto v = Lexer_.current().lexeme();
    advance();
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, v);
  }

  if (check(lex::tok::TokenType::STRING))
  {
    auto v = Lexer_.current().lexeme();
    advance();
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::STRING, v);
  }

  if (check(lex::tok::TokenType::KW_TRUE) || check(lex::tok::TokenType::KW_FALSE))
  {
    auto v = Lexer_.current().lexeme();
    advance();
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::BOOLEAN, v);
  }

  if (check(lex::tok::TokenType::KW_NONE))
  {
    advance();
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NONE, u"");
  }

  if (check(lex::tok::TokenType::IDENTIFIER))
  {
    auto name = Lexer_.current().lexeme();
    advance();
    return ast::AST_allocator.make<ast::NameExpr>(name);
  }

  // FIXED: Handle parenthesized expressions
  if (check(lex::tok::TokenType::LPAREN)) { return parseParenthesizedExpr(); }

  // FIXED: Handle list literals
  if (check(lex::tok::TokenType::LBRACKET))
  {
    advance();
    return parseListLiteral();
  }

  return nullptr;
}

ast::Expr* Parser::parseListLiteral()
{
  std::vector<ast::Expr*> elements;

  if (!check(lex::tok::TokenType::RBRACKET))
  {
    do
    {
      skipNewlines();
      if (check(lex::tok::TokenType::RBRACKET)) break;
      ast::Expr* elem = parseExpression();
      if (!elem) return nullptr;
      elements.push_back(elem);
      skipNewlines();
    } while (match(lex::tok::TokenType::COMMA));
  }

  consume(lex::tok::TokenType::RBRACKET, u"Expected ']' after list elements");
  return ast::AST_allocator.make<ast::ListExpr>(elements);
}

// Helper methods

bool Parser::match(const lex::tok::TokenType type)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : match(" << std::to_string(static_cast<int>(type)) << ") called!" << std::endl;
#endif
  if (check(type))
  {
    advance();
    return true;
  }
  return false;
}

}  // namespace parser
}  // namespace mylang