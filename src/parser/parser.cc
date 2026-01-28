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
    if (weDone())
    {
      break;
    }

    ast::Stmt* stmt = parseStatement();
    if (stmt != nullptr)
    {
      statements.push_back(stmt);
    }
    else
    {
      // Error recovery: skip to next statement
      synchronize();
      if (weDone())
      {
        break;
      }
    }
  }

  return statements;
}

ast::Stmt* Parser::parseStatement()
{
  skipNewlines();
  if (check(tok::TokenType::KW_IF))
  {
    return parseIfStmt();
  }
  if (check(tok::TokenType::KW_WHILE))
  {
    return parseWhileStmt();
  }
  if (check(tok::TokenType::KW_RETURN))
  {
    return parseReturnStmt();
  }
  if (check(tok::TokenType::KW_FN))
  {
    return parseFunctionDef();
  }
  // For now, treat everything else as ExprStmt
  return parseExpressionStmt();
}

ast::Stmt* Parser::parseReturnStmt()
{
  if (!consume(tok::TokenType::KW_RETURN, u"Expected 'return' statement"))
  {
    return nullptr;
  }

  // Handle return with no value (return None implicitly)
  if (check(tok::TokenType::NEWLINE) || weDone())
  {
    return nullptr;
  }
  ast::Expr* value = parseExpression();
  return ast::AST_allocator.make<ast::ReturnStmt>(value);
}

ast::Stmt* Parser::parseWhileStmt()
{
  if (!consume(tok::TokenType::KW_WHILE, u"Expected 'while' keyword"))
  {
    return nullptr;
  }

  ast::Expr* condition = parseExpression();
  if (condition == nullptr)
  {
    diagnostic::engine.emit("Expected condition expression after 'while'", diagnostic::DiagnosticEngine::Severity::ERROR);
    return nullptr;
  }

  if (!consume(tok::TokenType::COLON, u"Expected ':' after while condition"))
  {
    return nullptr;
  }

  ast::BlockStmt* while_block = parseIndentedBlock();
  if (while_block == nullptr)
  {
    diagnostic::engine.emit("Expected indented block after while statement", diagnostic::DiagnosticEngine::Severity::ERROR);
    return nullptr;
  }

  return ast::AST_allocator.make<ast::WhileStmt>(condition, while_block);
}

ast::BlockStmt* Parser::parseIndentedBlock()
{
  if (check(tok::TokenType::NEWLINE))
  {
    advance();
  }

  if (!consume(tok::TokenType::INDENT, u"Expected indented block"))
  {
    return nullptr;
  }

  std::vector<ast::Stmt*> statements;

  // Check for empty block (immediate DEDENT after INDENT)
  if (check(tok::TokenType::DEDENT))
  {
    advance();                                                   // consume DEDENT
    return ast::AST_allocator.make<ast::BlockStmt>(statements);  // Empty block
  }

  while (!check(tok::TokenType::DEDENT) && !weDone())
  {
    skipNewlines();
    if (check(tok::TokenType::DEDENT))
    {
      break;
    }

    ast::Stmt* stmt = parseStatement();
    if (stmt != nullptr)
    {
      statements.push_back(stmt);
    }
    else
    {
      // Error recovery: skip to next statement in block
      synchronize();
      if (check(tok::TokenType::DEDENT) || weDone())
      {
        break;
      }
    }
  }

  if (check(tok::TokenType::ENDMARKER))
  {
    return ast::AST_allocator.make<ast::BlockStmt>(statements);
  }
  else
  {
    if (!consume(tok::TokenType::DEDENT, u"Expected dedent after block"))
    {
      return nullptr;
    }
  }

  return ast::AST_allocator.make<ast::BlockStmt>(statements);
}

ast::ListExpr* Parser::parseParametersList()
{
  if (!consume(tok::TokenType::LPAREN, u"Expected '(' before parameters"))
  {
    return nullptr;
  }

  std::vector<ast::Expr*> parameters;

  // Handle empty parameter list: fn foo()
  if (!check(tok::TokenType::RPAREN))
  {
    do
    {
      skipNewlines();
      if (check(tok::TokenType::RPAREN))
      {
        break;
      }

      // Each parameter must be an identifier
      if (!check(tok::TokenType::IDENTIFIER))
      {
        diagnostic::engine.emit("Expected parameter name", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
      }

      StringRef param_name = Lexer_.current().lexeme();
      advance();

      ast::NameExpr* param = ast::AST_allocator.make<ast::NameExpr>(param_name);
      parameters.push_back(param);

      skipNewlines();
    } while (match(tok::TokenType::COMMA));
  }

  if (!consume(tok::TokenType::RPAREN, u"Expected ')' after parameters"))
  {
    return nullptr;
  }

  return ast::AST_allocator.make<ast::ListExpr>(parameters);
}

ast::Stmt* Parser::parseFunctionDef()
{
  if (!consume(tok::TokenType::KW_FN, u"Expected 'fn' keyword"))
  {
    return nullptr;
  }

  // Parse function name (must be an identifier)
  if (!check(tok::TokenType::IDENTIFIER))
  {
    diagnostic::engine.emit("Expected function name after 'fn'", diagnostic::DiagnosticEngine::Severity::ERROR);
    return nullptr;
  }

  StringRef function_name = Lexer_.current().lexeme();
  advance();

  // Parse parameter list
  ast::ListExpr* parameters_list = parseParametersList();
  if (parameters_list == nullptr)
  {
    diagnostic::engine.emit("Failed to parse parameter list", diagnostic::DiagnosticEngine::Severity::ERROR);
    return nullptr;
  }

  // Expect colon after parameters
  if (!consume(tok::TokenType::COLON, u"Expected ':' after function parameters"))
  {
    return nullptr;
  }

  // Parse function body block
  ast::BlockStmt* function_body = parseIndentedBlock();
  if (function_body == nullptr)
  {
    diagnostic::engine.emit("Failed to parse function body", diagnostic::DiagnosticEngine::Severity::ERROR);
    return nullptr;
  }

  // Create NameExpr from the function name
  ast::NameExpr* name_expr = ast::AST_allocator.make<ast::NameExpr>(function_name);

  return ast::AST_allocator.make<ast::FunctionDef>(name_expr, parameters_list, function_body);
}

ast::Stmt* Parser::parseIfStmt()
{
  if (!consume(tok::TokenType::KW_IF, u"Expected 'if' keyword"))
  {
    return nullptr;
  }

  ast::Expr* condition = parseExpression();
  if (condition == nullptr)
  {
    diagnostic::engine.emit("Expected condition expression after 'if'", diagnostic::DiagnosticEngine::Severity::ERROR);
    return nullptr;
  }

  if (!consume(tok::TokenType::COLON, u"Expected ':' after if condition"))
  {
    return nullptr;
  }

  // Parse the then-block
  ast::BlockStmt* then_block = parseIndentedBlock();
  if (then_block == nullptr)
  {
    diagnostic::engine.emit("Expected indented block after if statement", diagnostic::DiagnosticEngine::Severity::ERROR);
    return nullptr;
  }

  // Handle else clause
  ast::BlockStmt* else_block = nullptr;
  skipNewlines();

  return ast::AST_allocator.make<ast::IfStmt>(condition, then_block, else_block);
}

ast::Stmt* Parser::parseExpressionStmt()
{
  ast::Expr* expr = parseExpression();
  if (expr == nullptr)
  {
    return nullptr;
  }
  // Wrap the expression in an ExprStmt node
  return ast::AST_allocator.make<ast::ExprStmt>(expr);
}

ast::Expr* Parser::parseAssignmentExpr()
{
  ast::Expr* left = parseConditionalExpr();
  if (left == nullptr)
  {
    return nullptr;
  }

  // Check for assignment operators
  if (check(tok::TokenType::OP_ASSIGN))
  {
    // Left side must be a valid lvalue (for now, just NameExpr)
    ast::NameExpr* left_casted = dynamic_cast<ast::NameExpr*>(left);
    if (left_casted == nullptr)
    {
      diagnostic::engine.emit("Invalid assignment target", diagnostic::DiagnosticEngine::Severity::ERROR);
      return nullptr;
    }

    advance();                                 // consume '='
    ast::Expr* right = parseAssignmentExpr();  // Right associative
    if (right == nullptr)
    {
      return nullptr;
    }

    return ast::AST_allocator.make<ast::AssignmentExpr>(left_casted, right);
  }

  return left;
}

ast::Expr* Parser::parseLogicalExprPrecedence(unsigned min_precedence)
{
  ast::Expr* left = parseComparisonExpr();
  if (left == nullptr)
  {
    return nullptr;
  }

  for (;;)
  {
    int precedence = currentToken().getLogicalOpPrecedence();
    if (precedence < 0 || precedence < static_cast<int>(min_precedence))
    {
      break;
    }

    tok::TokenType op = Lexer_.current().type();
    advance();

    // All logical operators are left associative
    ast::Expr* right = parseLogicalExprPrecedence(precedence + 1);
    if (right == nullptr)
    {
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
  if (left == nullptr)
  {
    return nullptr;
  }

  // Comparison operators are non-associative (a < b < c is parsed as (a < b) and (b < c) in Python)
  // For simplicity, we only allow single comparison for now
  if (currentToken().isComparisonOp())
  {
    tok::TokenType op = Lexer_.current().type();
    advance();
    ast::Expr* right = parseBinaryExpr();
    if (right == nullptr)
    {
      diagnostic::engine.emit("Expected expression after comparison operator", diagnostic::DiagnosticEngine::Severity::ERROR);
      return nullptr;
    }

    left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
  }

  return left;
}

ast::Expr* Parser::parseBinaryExprPrecedence(unsigned min_precedence)
{
  ast::Expr* left = parseUnaryExpr();
  if (left == nullptr)
  {
    return nullptr;
  }

  for (;;)
  {
    int precedence = currentToken().getArithmeticOpPrecedence();
    if (precedence < 0 || precedence < static_cast<int>(min_precedence))
    {
      break;
    }

    tok::TokenType op = Lexer_.current().type();
    advance();

    // Left associative: higher precedence for next level
    int        nextMinPrecedence = precedence + 1;
    ast::Expr* right             = parseBinaryExprPrecedence(nextMinPrecedence);
    if (right == nullptr)
    {
      diagnostic::engine.emit("Expected expression after binary operator", diagnostic::DiagnosticEngine::Severity::ERROR);
      return nullptr;
    }

    left = ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
  }
  return left;
}

ast::Expr* Parser::parseUnaryExpr()
{
  // Check CURRENT token for unary operators
  if (currentToken().isUnaryOp())
  {
    tok::TokenType op = Lexer_.current().type();
    advance();                           // consume operator
    ast::Expr* expr = parseUnaryExpr();  // parse right side recursively
    if (expr == nullptr)
    {
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
  if (expr == nullptr)
  {
    return nullptr;
  }

  // Handle function calls
  while (check(tok::TokenType::LPAREN))
  {
    advance();  // consume '('
    std::vector<ast::Expr*> args;

    if (!check(tok::TokenType::RPAREN))
    {
      do
      {
        skipNewlines();
        if (check(tok::TokenType::RPAREN))
        {
          break;
        }

        ast::Expr* arg = parseExpression();
        if (arg == nullptr)
        {
          diagnostic::engine.emit("Expected expression in argument list", diagnostic::DiagnosticEngine::Severity::ERROR);
          return nullptr;
        }
        args.push_back(arg);

        skipNewlines();
      } while (match(tok::TokenType::COMMA));
    }

    if (!consume(tok::TokenType::RPAREN, u"Expected ')' after arguments"))
    {
      return nullptr;
    }

    expr = ast::AST_allocator.make<ast::CallExpr>(expr, ast::AST_allocator.make<ast::ListExpr>(args));
  }

  return expr;
}

ast::Expr* Parser::parsePrimaryExpr()
{
  if (weDone())
  {
    diagnostic::engine.emit("Unexpected end of input", diagnostic::DiagnosticEngine::Severity::ERROR);
    return nullptr;
  }

  if (check(tok::TokenType::NUMBER))
  {
    auto v = Lexer_.current().lexeme();
    advance();
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, v);
  }

  if (check(tok::TokenType::STRING))
  {
    auto v = Lexer_.current().lexeme();
    advance();
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::STRING, v);
  }

  if (check(tok::TokenType::KW_TRUE) || check(tok::TokenType::KW_FALSE))
  {
    auto v = Lexer_.current().lexeme();
    advance();
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::BOOLEAN, v);
  }

  if (check(tok::TokenType::KW_NONE))
  {
    advance();
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NONE, u"");
  }

  if (check(tok::TokenType::IDENTIFIER))
  {
    auto name = Lexer_.current().lexeme();
    advance();
    return ast::AST_allocator.make<ast::NameExpr>(name);
  }

  // Handle parenthesized expressions
  if (check(tok::TokenType::LPAREN))
  {
    advance();  // consume '('

    // Empty parentheses - empty tuple
    if (check(tok::TokenType::RPAREN))
    {
      advance();
      return ast::AST_allocator.make<ast::ListExpr>(std::vector<ast::Expr*>{});
    }

    ast::Expr* expr = parseExpression();
    if (expr == nullptr)
    {
      return nullptr;
    }

    if (!consume(tok::TokenType::RPAREN, u"Expected ')' after expression"))
    {
      return nullptr;
    }

    return expr;
  }

  // Handle list literals
  if (check(tok::TokenType::LBRACKET))
  {
    advance();  // consume '['
    return parseListLiteral();
  }

  diagnostic::engine.emit("Expected expression", diagnostic::DiagnosticEngine::Severity::ERROR);
  return nullptr;
}

ast::Expr* Parser::parseListLiteral()
{
  std::vector<ast::Expr*> elements;

  if (!check(tok::TokenType::RBRACKET))
  {
    do
    {
      skipNewlines();
      if (check(tok::TokenType::RBRACKET))
      {
        break;
      }

      ast::Expr* elem = parseExpression();
      if (elem == nullptr)
      {
        diagnostic::engine.emit("Expected expression in list literal", diagnostic::DiagnosticEngine::Severity::ERROR);
        return nullptr;
      }
      elements.push_back(elem);

      skipNewlines();
    } while (match(tok::TokenType::COMMA));
  }

  if (!consume(tok::TokenType::RBRACKET, u"Expected ']' after list elements"))
  {
    return nullptr;
  }
  return ast::AST_allocator.make<ast::ListExpr>(elements);
}

// Helper methods

bool Parser::match(const tok::TokenType type)
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
    if (check(tok::TokenType::NEWLINE) || check(tok::TokenType::DEDENT))
    {
      advance();
      return;
    }

    // Stop before statement keywords
    if (check(tok::TokenType::KW_IF) || check(tok::TokenType::KW_WHILE) || check(tok::TokenType::KW_RETURN) || check(tok::TokenType::KW_FN))
    {
      return;
    }

    advance();
  }
}

}  // namespace parser
}  // namespace mylang