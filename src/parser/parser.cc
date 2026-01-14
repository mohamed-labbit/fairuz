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
    if (stmt != nullptr)
      statements.push_back(stmt);
    else
    {
      // Error recovery: skip to next statement
      synchronize();
      if (weDone()) break;
    }
  }

  return statements;
}

ast::Stmt* Parser::parseStatement()
{
  skipNewlines();

  if (check(lex::tok::TokenType::KW_IF)) return parseIfStmt();
  if (check(lex::tok::TokenType::KW_WHILE)) return parseWhileStmt();
  if (check(lex::tok::TokenType::KW_RETURN)) return parseReturnStmt();
  if (check(lex::tok::TokenType::KW_FN)) return parseFunctionDef();

  // For now, treat everything else as ExprStmt
  return parseExpressionStmt();
}

ast::Stmt* Parser::parseReturnStmt()
{
  consume(lex::tok::TokenType::KW_RETURN, u"Expected 'return' statement");
  
  // Handle return with no value (return None implicitly)
  if (check(lex::tok::TokenType::NEWLINE) || weDone())
  {
    return ast::AST_allocator.make<ast::ReturnStmt>(nullptr);
  }
  
  ast::Expr* value = parseExpression();
  return ast::AST_allocator.make<ast::ReturnStmt>(value);
}

ast::Stmt* Parser::parseWhileStmt()
{
  consume(lex::tok::TokenType::KW_WHILE, u"Expected 'while' keyword");

  ast::Expr* condition = parseExpression();
  if (condition == nullptr)
  {
    reportError(u"Expected condition expression after 'while'");
    return nullptr;
  }

  consume(lex::tok::TokenType::COLON, u"Expected ':' after while condition");
  
  ast::BlockStmt* while_block = parseIndentedBlock();
  if (while_block == nullptr)
  {
    reportError(u"Expected indented block after while statement");
    return nullptr;
  }

  return ast::AST_allocator.make<ast::WhileStmt>(condition, while_block);
}

ast::BlockStmt* Parser::parseIndentedBlock()
{
  consume(lex::tok::TokenType::INDENT, u"Expected indented block");

  std::vector<ast::Stmt*> statements;

  // Check for empty block (immediate DEDENT after INDENT)
  if (check(lex::tok::TokenType::DEDENT))
  {
    advance();  // consume DEDENT
    return ast::AST_allocator.make<ast::BlockStmt>(statements);  // Empty block
  }

  while (!check(lex::tok::TokenType::DEDENT) && !weDone())
  {
    skipNewlines();
    if (check(lex::tok::TokenType::DEDENT)) break;

    ast::Stmt* stmt = parseStatement();
    if (stmt != nullptr)
    {
      statements.push_back(stmt);
    }
    else
    {
      // Error recovery: skip to next statement in block
      synchronize();
      if (check(lex::tok::TokenType::DEDENT) || weDone()) break;
    }
  }

  consume(lex::tok::TokenType::DEDENT, u"Expected dedent after block");

  return ast::AST_allocator.make<ast::BlockStmt>(statements);
}

ast::ListExpr* Parser::parseParametersList()
{
  consume(lex::tok::TokenType::LPAREN, u"Expected '(' before parameters");

  std::vector<ast::Expr*> parameters;

  // Handle empty parameter list: fn foo()
  if (!check(lex::tok::TokenType::RPAREN))
  {
    do
    {
      skipNewlines();
      if (check(lex::tok::TokenType::RPAREN)) break;

      // Each parameter must be an identifier
      if (!check(lex::tok::TokenType::IDENTIFIER))
      {
        reportError(u"Expected parameter name");
        return nullptr;
      }

      std::u16string param_name = Lexer_.current().lexeme();
      advance();

      ast::NameExpr* param = ast::AST_allocator.make<ast::NameExpr>(param_name);
      parameters.push_back(param);

      skipNewlines();
    } while (match(lex::tok::TokenType::COMMA));
  }

  consume(lex::tok::TokenType::RPAREN, u"Expected ')' after parameters");

  return ast::AST_allocator.make<ast::ListExpr>(parameters);
}

ast::Stmt* Parser::parseFunctionDef()
{
  consume(lex::tok::TokenType::KW_FN, u"Expected 'fn' keyword");

  // Parse function name (must be an identifier)
  if (!check(lex::tok::TokenType::IDENTIFIER))
  {
    reportError(u"Expected function name after 'fn'");
    return nullptr;
  }
  std::u16string function_name = Lexer_.current().lexeme();
  advance();

  // Parse parameter list
  ast::ListExpr* parameters_list = parseParametersList();
  if (parameters_list == nullptr)
  {
    reportError(u"Failed to parse parameter list");
    return nullptr;
  }

  // Expect colon after parameters
  consume(lex::tok::TokenType::COLON, u"Expected ':' after function parameters");

  // Parse function body block
  ast::BlockStmt* function_body = parseIndentedBlock();
  if (function_body == nullptr)
  {
    reportError(u"Failed to parse function body");
    return nullptr;
  }

  // Create NameExpr from the function name
  ast::NameExpr* name_expr = ast::AST_allocator.make<ast::NameExpr>(function_name);

  return ast::AST_allocator.make<ast::FunctionDef>(name_expr, parameters_list, function_body);
}

ast::Stmt* Parser::parseIfStmt()
{
  consume(lex::tok::TokenType::KW_IF, u"Expected 'if' keyword");

  ast::Expr* condition = parseExpression();
  if (condition == nullptr)
  {
    reportError(u"Expected condition expression after 'if'");
    return nullptr;
  }

  consume(lex::tok::TokenType::COLON, u"Expected ':' after if condition");

  // Parse the then-block
  ast::BlockStmt* then_block = parseIndentedBlock();
  if (then_block == nullptr)
  {
    reportError(u"Expected indented block after if statement");
    return nullptr;
  }

  // Handle else clause
  ast::BlockStmt* else_block = nullptr;
  skipNewlines();

  /*
  if (match(lex::tok::TokenType::KW_ELSE))
  {
    consume(lex::tok::TokenType::COLON, u"Expected ':' after 'else'");
    else_block = parseIndentedBlock();
    if (else_block == nullptr)
    {
      reportError(u"Expected indented block after else statement");
      return nullptr;
    }
  }
  */

  return ast::AST_allocator.make<ast::IfStmt>(condition, then_block, else_block);
}

ast::Stmt* Parser::parseExpressionStmt()
{
  ast::Expr* expr = parseExpression();
  if (expr == nullptr) return nullptr;
  // Wrap the expression in an ExprStmt node
  return ast::AST_allocator.make<ast::ExprStmt>(expr);
}

ast::Expr* Parser::parseExpression()
{
  return parseAssignmentExpr();
}

ast::Expr* Parser::parseAssignmentExpr()
{
  ast::Expr* left = parseConditionalExpr();
  if (left == nullptr) return nullptr;

  // Check for assignment operators
  if (check(lex::tok::TokenType::OP_ASSIGN))
  {
    // Left side must be a valid lvalue (for now, just NameExpr)
    ast::NameExpr* left_casted = dynamic_cast<ast::NameExpr*>(left);
    if (left_casted == nullptr)
    {
      reportError(u"Invalid assignment target");
      return nullptr;
    }
    
    advance(); // consume '='
    ast::Expr* right = parseAssignmentExpr();  // Right associative
    if (right == nullptr) return nullptr;
    
    return ast::AST_allocator.make<ast::AssignmentExpr>(left_casted, right);
  }
  
  return left;
}

ast::Expr* Parser::parseLogicalExprPrecedence(unsigned min_precedence)
{
  ast::Expr* left = parseComparisonExpr();
  if (left == nullptr) return nullptr;

  while (true)
  {
    int precedence = getLogicalOperatorPrecedence(Lexer_.current().type());
    if (precedence < 0 || precedence < static_cast<int>(min_precedence)) break;

    lex::tok::TokenType op = Lexer_.current().type();
    advance();

    // All logical operators are left associative
    ast::Expr* right = parseLogicalExprPrecedence(precedence + 1);
    if (right == nullptr)
    {
      reportError(u"Expected expression after logical operator");
      return nullptr;
    }

    left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
  }
  return left;
}

int Parser::getLogicalOperatorPrecedence(lex::tok::TokenType tt)
{
  switch (tt)
  {
  case lex::tok::TokenType::OP_BITOR:   // '|'
  case lex::tok::TokenType::KW_OR:      // 'or'
    return 1;
  case lex::tok::TokenType::OP_BITXOR:  // '^'
    return 2;
  case lex::tok::TokenType::OP_BITAND:  // '&'
  case lex::tok::TokenType::KW_AND:     // 'and'
    return 3;
  default:
    return -1;  // Not a logical operator
  }
}

ast::Expr* Parser::parseComparisonExpr()
{
  ast::Expr* left = parseBinaryExpr();
  if (left == nullptr) return nullptr;

  // Comparison operators are non-associative (a < b < c is parsed as (a < b) and (b < c) in Python)
  // For simplicity, we only allow single comparison for now
  if (check(lex::tok::TokenType::OP_EQ) || check(lex::tok::TokenType::OP_NEQ) ||
      check(lex::tok::TokenType::OP_LT) || check(lex::tok::TokenType::OP_GT) ||
      check(lex::tok::TokenType::OP_LTE) || check(lex::tok::TokenType::OP_GTE))
  {
    lex::tok::TokenType op = Lexer_.current().type();
    advance();
    ast::Expr* right = parseBinaryExpr();
    if (right == nullptr)
    {
      reportError(u"Expected expression after comparison operator");
      return nullptr;
    }

    left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
  }
  
  return left;
}

ast::Expr* Parser::parseBinaryExprPrecedence(unsigned min_precedence)
{
  ast::Expr* left = parseUnaryExpr();
  if (left == nullptr) return nullptr;

  while (true)
  {
    int precedence = getArithmeticOperatorPrecedence(Lexer_.current().type());
    if (precedence < 0 || precedence < static_cast<int>(min_precedence)) break;

    lex::tok::TokenType op = Lexer_.current().type();
    advance();

    // Left associative: higher precedence for next level
    int nextMinPrecedence = precedence + 1;
    ast::Expr* right = parseBinaryExprPrecedence(nextMinPrecedence);
    if (right == nullptr)
    {
      reportError(u"Expected expression after binary operator");
      return nullptr;
    }

    left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
  }
  return left;
}

int Parser::getArithmeticOperatorPrecedence(const lex::tok::TokenType type)
{
  switch (type)
  {
  case lex::tok::TokenType::OP_STAR:    // *
  case lex::tok::TokenType::OP_SLASH:   // /
    return 3;
  case lex::tok::TokenType::OP_PLUS:    // +
  case lex::tok::TokenType::OP_MINUS:   // -
    return 2;
  default:
    return -1;  // Not an arithmetic operator
  }
}

ast::Expr* Parser::parseUnaryExpr()
{
  // Check CURRENT token for unary operators
  if (check(lex::tok::TokenType::OP_PLUS) || check(lex::tok::TokenType::OP_MINUS) ||
      check(lex::tok::TokenType::OP_BITNOT) || check(lex::tok::TokenType::KW_NOT))
  {
    lex::tok::TokenType op = Lexer_.current().type();
    advance();  // consume operator
    ast::Expr* expr = parseUnaryExpr();  // parse right side recursively
    if (expr == nullptr)
    {
      reportError(u"Expected expression after unary operator");
      return nullptr;
    }
    return ast::AST_allocator.make<ast::UnaryExpr>(expr, op);
  }
  return parsePostfixExpr();
}

ast::Expr* Parser::parsePostfixExpr()
{
  ast::Expr* expr = parsePrimaryExpr();
  if (expr == nullptr) return nullptr;

  // Handle function calls
  while (check(lex::tok::TokenType::LPAREN))
  {
    advance();  // consume '('
    std::vector<ast::Expr*> args;

    if (!check(lex::tok::TokenType::RPAREN))
    {
      do
      {
        skipNewlines();
        if (check(lex::tok::TokenType::RPAREN)) break;
        
        ast::Expr* arg = parseExpression();
        if (arg == nullptr)
        {
          reportError(u"Expected expression in argument list");
          return nullptr;
        }
        args.push_back(arg);
        
        skipNewlines();
      } while (match(lex::tok::TokenType::COMMA));
    }

    consume(lex::tok::TokenType::RPAREN, u"Expected ')' after arguments");

    expr = ast::AST_allocator.make<ast::CallExpr>(
      expr, 
      ast::AST_allocator.make<ast::ListExpr>(args)
    );
  }

  return expr;
}

ast::Expr* Parser::parsePrimaryExpr()
{
  if (weDone())
  {
    reportError(u"Unexpected end of input");
    return nullptr;
  }

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

  // Handle parenthesized expressions
  if (check(lex::tok::TokenType::LPAREN))
  {
    advance();  // consume '('
    
    // Empty parentheses - empty tuple
    if (check(lex::tok::TokenType::RPAREN))
    {
      advance();
      return ast::AST_allocator.make<ast::ListExpr>(std::vector<ast::Expr*>{});
    }
    
    ast::Expr* expr = parseExpression();
    if (expr == nullptr) return nullptr;
    
    consume(lex::tok::TokenType::RPAREN, u"Expected ')' after expression");
    return expr;
  }

  // Handle list literals
  if (check(lex::tok::TokenType::LBRACKET))
  {
    advance();  // consume '['
    return parseListLiteral();
  }

  reportError(u"Expected expression");
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
      if (elem == nullptr)
      {
        reportError(u"Expected expression in list literal");
        return nullptr;
      }
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
  if (check(type))
  {
    advance();
    return true;
  }
  return false;
}

void Parser::synchronize()
{
  // Skip tokens until we find a statement boundary
  while (!weDone())
  {
    // Stop at newline or dedent (statement boundaries)
    if (check(lex::tok::TokenType::NEWLINE) || 
        check(lex::tok::TokenType::DEDENT))
    {
      advance();
      return;
    }
    
    // Stop before statement keywords
    if (check(lex::tok::TokenType::KW_IF) ||
        check(lex::tok::TokenType::KW_WHILE) ||
        check(lex::tok::TokenType::KW_RETURN) ||
        check(lex::tok::TokenType::KW_FN))
    {
      return;
    }
    
    advance();
  }
}

void Parser::reportError(const std::u16string& message)
{
  // Implement error reporting - this is a placeholder
  // In a real implementation, this would use the diagnostic system
  // For now, we'll assume there's a mechanism to report errors
  // that's part of the Parser class or accessible to it
}

}  // namespace parser
}  // namespace mylang