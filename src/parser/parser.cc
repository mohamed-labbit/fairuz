#include "../../include/parser/parser.hpp"
#include "../../include/diag/diagnostic.hpp"

#include <cassert>


namespace mylang {
namespace parser {

ast::Expr* Parser::parseParenthesizedExpr()
{
  consume(lex::tok::TokenType::LPAREN, u"Expected '('");

  std::vector<ast::Expr*> elements;

  // If empty parentheses
  if (!check(lex::tok::TokenType::RPAREN))
  {
    do
    {
      ast::Expr* elem = parseExpression();
      if (!elem) return nullptr;
      elements.push_back(elem);
    } while (match(lex::tok::TokenType::COMMA));
  }

  consume(lex::tok::TokenType::RPAREN, u"Expected ')' after expression(s)");

  // Decide what to return
  ast::Expr* expr = nullptr;
  if (elements.size() == 0)
  {
    // ()
    expr = ast::AST_allocator.make<ast::ListExpr>(elements);
  }
  else if (elements.size() == 1)
  {
    // (single expr)
    expr = elements[0];
  }
  else
  {
    // (tuple)
    expr = ast::AST_allocator.make<ast::ListExpr>(elements);
  }

  // Handle potential call: (expr)(args)
  return parseCallExpr(expr);
}

// Returns either a single Expr* or a ListExpr* (tuple)
ast::Expr* Parser::parseParenthesizedExprContent()
{
  // Empty ()
  if (check(lex::tok::TokenType::RPAREN)) return ast::AST_allocator.make<ast::ListExpr>(std::vector<ast::Expr*>{});
  ast::Expr* first = parseExpression();
  if (!first) return nullptr;
  // If there’s a comma, parse a tuple
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
    callee = ast::AST_allocator.make<ast::CallExpr>(dynamic_cast<ast::Expr*>(callee), args_expr);
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

bool Parser::isBinaryOp(const lex::tok::Token tok)
{
  lex::tok::TokenType tt = tok.type();
  return tt == lex::tok::TokenType::OP_BITAND || tt == lex::tok::TokenType::OP_BITXOR || tt == lex::tok::TokenType::OP_BITOR
    || tt == lex::tok::TokenType::OP_PLUS || tt == lex::tok::TokenType::OP_STAR || tt == lex::tok::TokenType::OP_SLASH
    || tt == lex::tok::TokenType::KW_AND || tt == lex::tok::TokenType::KW_OR;
}

int Parser::getLogicalOperatorPrecedence(lex::tok::TokenType tt)
{
  switch (tt)
  {
  case lex::tok::TokenType::OP_BITOR :  // '|'
  case lex::tok::TokenType::KW_OR :     // 'or'
    return 1;
  case lex::tok::TokenType::OP_BITAND :  // '&'
  case lex::tok::TokenType::KW_AND :     // 'and'
    return 2;
  case lex::tok::TokenType::OP_BITNOT :
  case lex::tok::TokenType::KW_NOT :  // '!'
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
  if (check(lex::tok::TokenType::OP_PLUS) || check(lex::tok::TokenType::OP_MINUS) || check(lex::tok::TokenType::OP_BITNOT))
  {
    lex::tok::TokenType op = Lexer_.current().type();
    advance();                           // consume operator
    ast::Expr* expr = parseUnaryExpr();  // parse right side
    if (!expr) return nullptr;
    return dynamic_cast<ast::Expr*>(ast::AST_allocator.make<ast::UnaryExpr>(expr, op));
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
        // IMPORTANT: parseAssignmentExpr, NOT parseExpression
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
  return dynamic_cast<ast::Expr*>(ast::AST_allocator.make<ast::ListExpr>(elements));
}

// Helper methods

bool Parser::match(const lex::tok::TokenType type)
{
  if (check(type))
  {
    advance();
    return true;
  }
  return false;
}

}  // namespace parser
}  // namespace mylang