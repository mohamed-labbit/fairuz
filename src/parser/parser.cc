#include "../../include/parser/parser.hpp"
#include "../../include/diag/diagnostic.hpp"

#include <cassert>


namespace mylang {
namespace parser {

// constructors
Parser::Parser(input::FileManager* file_manager) :
    Lexer_(file_manager)
{
  if (file_manager == nullptr) diagnostic::engine.panic("file_manager is NULL!");

  // Advance to the first real token
  Lexer_.next();
#if DEBUG_PRINT
  std::cout << "-- DEBUG : Lexer_.next() = " << std::to_string(static_cast<int>(Lexer_.current().type())) << std::endl;
  std::cout << "-- DEBUG : parser initialized successfully!" << std::endl;
#endif
}

// REMOVED: parseUnary() - this was unused and buggy
// The correct unary parsing is in parseUnaryExpr()

ast::Expr* Parser::parseParenthesizedExpr()
{
  // Handle empty parentheses - empty tuple
  if (check(lex::tok::TokenType::RPAREN))
  {
    advance();
    std::vector<ast::Expr*> no_elems;
    return ast::AST_allocator.make<ast::ListExpr>(no_elems);
  }

  ast::Expr* expr = parseExpression();
  if (!expr) return nullptr;
  
  // Check for tuple (comma-separated values)
  if (check(lex::tok::TokenType::COMMA))
  {
    std::vector<ast::Expr*> elements;
    elements.push_back(expr);

    while (match(lex::tok::TokenType::COMMA))
    {
      if (check(lex::tok::TokenType::RPAREN)) break;  // trailing comma
      ast::Expr* elem = parseExpression();
      if (!elem) return nullptr;
      elements.push_back(elem);
    }

    consume(lex::tok::TokenType::RPAREN, u"Expected ')' after tuple");
    return ast::AST_allocator.make<ast::ListExpr>(elements);
  }

  consume(lex::tok::TokenType::RPAREN, u"Expected ')' after expression");

  // Handle nested call expression: (expr)(args)
  while (check(lex::tok::TokenType::LPAREN))
  {
    advance();  // consume '('
    std::vector<ast::Expr*> args;

    if (!check(lex::tok::TokenType::RPAREN))
    {
      ast::Expr* arg = parseExpression();
      if (!arg) return nullptr;
      args.push_back(arg);

      while (match(lex::tok::TokenType::COMMA))
      {
        if (check(lex::tok::TokenType::RPAREN)) break;  // trailing comma
        arg = parseExpression();
        if (!arg) return nullptr;
        args.push_back(arg);
      }
    }

    consume(lex::tok::TokenType::RPAREN, u"Expected ')' after arguments");

    // Create call expression
    ast::NameExpr* callee = dynamic_cast<ast::NameExpr*>(expr);
    if (!callee)
      // If expr is not a NameExpr, treat as generic callable
      callee = ast::AST_allocator.make<ast::NameExpr>(u"<lambda>");
  
    ast::ListExpr* args_expr = ast::AST_allocator.make<ast::ListExpr>(args);
    expr = ast::AST_allocator.make<ast::CallExpr>(callee, args_expr);
  }
  
  return expr;
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

ast::Expr* Parser::parseConditionalExpr()
{
  ast::Expr* expr = parseLogicalExpr();
  // Future: Add ternary operator support here
  return expr;
}

ast::Expr* Parser::parseLogicalExpr() { return parseLogicalExprPrecedence(0); }

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

ast::Expr* Parser::parseBinaryExpr() { return parseBinaryExprPrecedence(0); }

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
    return static_cast<ast::Expr*>(ast::AST_allocator.make<ast::UnaryExpr>(expr, op));
  }
  return parsePostfixExpr();
}

ast::Expr* Parser::parsePostfixExpr()
{
  std::cerr << "Unary sees token: " << utf8::utf16to8(Lexer_.current().lexeme()) << "\n";
  ast::Expr* expr = parsePrimaryExpr();
  if (!expr) return nullptr;

  while (true)
  {
    if (check(lex::tok::TokenType::LPAREN))
    {
      // Function call
      advance();
      std::vector<ast::Expr*> args;
      if (!check(lex::tok::TokenType::RPAREN))
      {
        do
        {
          ast::Expr* arg = parseExpression();
          if (!arg) return nullptr;
          args.push_back(arg);
          advance();
        } while (check(lex::tok::TokenType::COMMA));
      }

      consume(lex::tok::TokenType::RPAREN, u"Expected ')' after arguments");

      // Create call expression properly
      ast::NameExpr* callee = dynamic_cast<ast::NameExpr*>(expr);
      if (!callee)
      {
        // Handle complex callees (like member access, etc.)
        callee = ast::AST_allocator.make<ast::NameExpr>(u"<callable>");
      }
      ast::ListExpr* args_list = ast::AST_allocator.make<ast::ListExpr>(args);
      expr = ast::AST_allocator.make<ast::CallExpr>(callee, args_list);
    }
    else
    {
      break;
    }
  }
  return expr;
}

ast::Expr* Parser::parsePrimaryExpr()
{
  if (weDone()) return nullptr;

  // NUMBER literal
  if (check(lex::tok::TokenType::NUMBER))
  {
    StringType value = Lexer_.current().lexeme();
    advance();
    return static_cast<ast::Expr*>(ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, value));
  }

  // STRING literal
  if (check(lex::tok::TokenType::STRING))
  {
    StringType value = Lexer_.current().lexeme();
    advance();
    return static_cast<ast::Expr*>(ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::STRING, value));
  }

  // BOOLEAN literal
  if (check(lex::tok::TokenType::KW_TRUE) || check(lex::tok::TokenType::KW_FALSE))
  {
    StringType value = Lexer_.current().lexeme();
    advance();
    return static_cast<ast::Expr*>(ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::BOOLEAN, value));
  }

  // NONE literal
  if (check(lex::tok::TokenType::KW_NONE))
  {
    advance();
    return static_cast<ast::Expr*>(ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NONE, u""));
  }

  // IDENTIFIER (variable or function name)
  if (check(lex::tok::TokenType::IDENTIFIER))
  {
    StringType name = Lexer_.current().lexeme();
    advance();
    // Just an identifier
    return static_cast<ast::Expr*>(ast::AST_allocator.make<ast::NameExpr>(name));
  }
  
  // Parenthesized expression or tuple
  if (check(lex::tok::TokenType::LPAREN))
  {
    advance();
    return parseParenthesizedExpr();
  }

  // List literal
  if (check(lex::tok::TokenType::LBRACKET))
  {
    advance();
    std::vector<ast::Expr*> elements;
    if (!check(lex::tok::TokenType::RBRACKET))
    {
      do
      {
        ast::Expr* elem = parseExpression();
        if (!elem) return nullptr;
        elements.push_back(elem);
      } while (match(lex::tok::TokenType::COMMA));
    }
    consume(lex::tok::TokenType::RBRACKET, u"Expected ']' after list elements");
    return static_cast<ast::Expr*>(ast::AST_allocator.make<ast::ListExpr>(elements));
  }
  
  // No valid primary expression found
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
  return static_cast<ast::Expr*>(ast::AST_allocator.make<ast::ListExpr>(elements));
}

// Helper methods
bool Parser::isUnaryOp(const lex::tok::Token tok)
{
  return tok.type() == lex::tok::TokenType::OP_MINUS || tok.type() == lex::tok::TokenType::OP_BITNOT || tok.type() == lex::tok::TokenType::OP_PLUS;
}

lex::tok::Token Parser::peek(std::size_t offset)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : peek() called!" << std::endl;
#endif
  return Lexer_.peek(offset);
}

lex::tok::Token Parser::advance()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : advance() called!" << std::endl;
#endif
  // std::cerr << "ADVANCE from " << utf8::utf16to8(Lexer_.current().lexeme()) << "\n";
  return Lexer_.next();
}

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

bool Parser::check(lex::tok::TokenType type)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : check(" << std::to_string(static_cast<int>(type)) << ") called!" << std::endl;
#endif
  // if (weDone()) return false;
  return Lexer_.current().is(type);
}

lex::tok::Token Parser::consume(lex::tok::TokenType type, const StringType& msg)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : Parser::consume() called!" << std::endl;
#endif
  if (check(type)) return advance();
  /// TODO: Implement proper error reporting
  // For now, return empty token
  return lex::tok::Token();
}

void Parser::skipNewlines() { while (match(lex::tok::TokenType::NEWLINE)); }

StringType Parser::getSourceLine(std::size_t line)
{
  /// TODO: Use the file manager to retrieve actual source line
  return peek().lexeme();
}

bool Parser::weDone() const { return Lexer_.current().is(lex::tok::TokenType::ENDMARKER); }

// Public entry point for parsing any expression
ast::Expr* Parser::parse() { return parseExpression(); }

}  // namespace parser
}  // namespace mylang