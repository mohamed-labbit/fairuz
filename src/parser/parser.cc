#include "../../include/parser/parser.hpp"
#include "../../include/diag/diagnostic.hpp"

#include <cassert>


namespace mylang {
namespace parser {

// constructors
Parser::Parser(input::FileManager* file_manager) :
    lex_(file_manager)
{
  if (file_manager == nullptr)
    // diagnostic::engine.panic("file_manager is NULL!");
    diagnostic::engine.panic("file_manager is NULL!");

  // assert(lex_.next().type() == lex::tok::TokenType::BEGINMARKER);

  // Check that we START at beginmarker
  // assert(lex_.current().type() == lex::tok::TokenType::BEGINMARKER);

  // Then advance to the first real token
  lex_.next();

#if DEBUG_PRINT
  std::cout << "-- DEBUG : lex_.next() = " << std::to_string(static_cast<int>(lex_.current().type())) << std::endl;
  std::cout << "-- DEBUG : parser initialized successfully!" << std::endl;
#endif
  // ...
}

ast::Expr* Parser::parseUnary()
{
  auto type = peek();

  if (isUnaryOp(type))
  {
    auto op = advance();
    auto self = parseUnary();
    return new ast::UnaryExpr(std::move(self), op.type());
  }

  return parsePrimaryExpr();
}
/*
ast::Expr* Parser::parsePrimary()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : parse primary started!" << std::endl;
  #endif
  ast::Expr* ret;
  
  // number literal
  if (match(lex::tok::TokenType::NUMBER))
  {
    auto tok = lex_.prev();
    ret      = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::NUMBER, tok.lexeme()));
    }
  // string literal
  else if (match(lex::tok::TokenType::STRING))
  {
    auto tok = advance();
    ret      = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::STRING, tok.lexeme()));
    }
  // keyword true
  else if (match(lex::tok::TokenType::KW_TRUE))
  {
    auto tok = advance();
    ret      = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::BOOLEAN, tok.lexeme()));
    }
  // keyword false
  else if (match(lex::tok::TokenType::KW_FALSE))
  {
    auto tok = advance();
    ret      = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::BOOLEAN, tok.lexeme()));
    }
  // keyword none
  else if (match(lex::tok::TokenType::KW_NONE))
  {
    auto tok = advance();
    ret      = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::NONE, tok.lexeme()));
    }
  // parenthesized expression
  /// @note parsing tuples isn't implemented yet
  else if (match(lex::tok::TokenType::LPAREN)) { ret = parseParenthesizedExpr(); }
  // list
  else if (match(lex::tok::TokenType::LBRACKET)) { ret = parseListLiteral(); }
  // ... 'dict'
  else
  {
    throw ParseError(u"Expected expression, got '" + peek().lexeme() + u"'", peek().line(), peek().column(), getSourceLine(peek().line()));
  }

  /// @todo : add caching
  return ret;
}
*/
/*
/// @todo: remove
ast::Expr* Parser::parseParenthesizedExpr()
{
  if (check(lex::tok::TokenType::RPAREN))
  {
    advance();
    return std::make_unique<ast::ListExpr>(std::vector<ast::Expr*>());
  }

  ast::Expr* expr = parseExpression();
  
  // check for tuple
  // ...
  
  consume(lex::tok::TokenType::RPAREN, u"Expected ')' after expression");
  return expr;
}

ast::Expr* Parser::parseExpression()
{
  /// @todo : starred
  // ...
  return parseAssignmentExpr();
}

ast::Expr* Parser::parseAssignmentExpr()
{
  /// @todo
  // ...
  return ast::Expr*();
}
*/

ast::Expr* Parser::parseParenthesizedExpr()
{
  if (check(lex::tok::TokenType::RPAREN))
  {
    advance();
    ast::ListExpr ret = ast::ListExpr(std::vector<ast::Expr*>());
    return &ret;
  }

  ast::Expr* expr = parseExpression();

  /// @todo: check for tuple (comma-separated expressions)
  // if (check(lex::tok::TokenType::COMMA)) {
  //   std::vector<ast::Expr*> elements;
  //   elements.push_back(std::move(expr));
  //   while (match(lex::tok::TokenType::COMMA)) {
  //     if (check(lex::tok::TokenType::RPAREN)) break; // trailing comma
  //     elements.push_back(parseExpression());
  //   }
  //   consume(lex::tok::TokenType::RPAREN, u"Expected ')' after tuple");
  //   return std::make_unique<ast::TupleExpr>(std::move(elements));
  // }

  consume(lex::tok::TokenType::RPAREN, u"Expected ')' after expression");
  return expr;
}

ast::Expr* Parser::parseExpression()
{
  // Handle starred expressions
  if (check(lex::tok::TokenType::OP_STAR))
  {
    advance();
    return parseExpression();
  }

  return parseAssignmentExpr();
}

ast::Expr* Parser::parseAssignmentExpr()
{
  ast::Expr* left = parseConditionalExpr();
  ast::NameExpr* left_casted = dynamic_cast<ast::NameExpr*>(left);

  if (!left_casted) return left;

  // Check for assignment operators
  if (check(lex::tok::TokenType::OP_ASSIGN) /* ||
      check(lex::tok::TokenType::PLUS_ASSIGN) ||
      check(lex::tok::TokenType::MINUS_ASSIGN) ||
      check(lex::tok::TokenType::STAR_ASSIGN) ||
      check(lex::tok::TokenType::SLASH_ASSIGN) ||
      check(lex::tok::TokenType::PERCENT_ASSIGN)*/)
  {
    lex::tok::TokenType op = lex_.current().type();
    advance();
    ast::Expr* right = parseAssignmentExpr();  // Right associative
    auto* ret = new ast::AssignmentExpr(std::move(left_casted), std::move(right));
    return ret;
  }

  return left;
}

ast::Expr* Parser::parseConditionalExpr()
{
  ast::Expr* expr = parseLogicalExpr();

  /*
  // Check for ternary: a if b else c
  if (check(lex::tok::TokenType::KW_IF))
  {
    advance();
    ast::Expr* condition = parseLogicalExpr();
    // consume(lex::tok::TokenType::KW_ELSE, u"Expected 'else' in conditional expression");
    ast::Expr* elseExpr = parseConditionalExpr();
    return std::make_unique<ast::Expr>(std::move(expr), std::move(condition));
  }
  */

  return expr;
}

ast::Expr* Parser::parseLogicalExpr() { return parseLogicalExprPrecedence(0); }

ast::Expr* Parser::parseLogicalExprPrecedence(int min_precedence)
{
  // Handle unary NOT at the beginning
  /*
  /// @todo: 
  if (check(lex::tok::TokenType::OP_NOT))
  {
    lex::tok::TokenType op = currentToken().type;
    advance();
    ast::Expr* expr = parseLogicalExprPrecedence(getLogicalOperatorPrecedence(op));
    return std::make_unique<ast::UnaryExpr>(op, std::move(expr));
  }
  */

  ast::Expr* left = parseComparisonExpr();

  while (true)
  {
    int precedence = getLogicalOperatorPrecedence(lex_.current().type());
    if (precedence < min_precedence) break;
    lex::tok::TokenType op = lex_.current().type();
    advance();

    // All logical operators are left associative
    ast::Expr* right = parseLogicalExprPrecedence(precedence + 1);
    if (left) delete left;
    left = new ast::BinaryExpr(std::move(left), std::move(right), op);
  }

  return left;
}

bool Parser::isBinaryOp(const lex::tok::Token tok)
{
  auto tt = tok.type();
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
  case lex::tok::TokenType::OP_BITAND :  // &
  case lex::tok::TokenType::KW_AND :     // 'and'
    return 2;
  case lex::tok::TokenType::OP_BITNOT :
  case lex::tok::TokenType::KW_NOT :  // !
    return 3;
  default : return -1;  // Not a logical operator
  }
}

ast::Expr* Parser::parseComparisonExpr()
{
  ast::Expr* left = parseBinaryExpr();

  while (check(lex::tok::TokenType::OP_EQ) || check(lex::tok::TokenType::OP_NEQ) || check(lex::tok::TokenType::OP_LT)
         || check(lex::tok::TokenType::OP_GT) || check(lex::tok::TokenType::OP_LTE)
         || check(lex::tok::TokenType::OP_GTE) /*|| check(lex::tok::TokenType::OP_IN) || check(lex::tok::TokenType::IS)*/)
  {
    lex::tok::TokenType op = lex_.current().type();
    advance();
    ast::Expr* right = parseBinaryExpr();
    if (left) delete left;
    left = new ast::BinaryExpr(std::move(left), std::move(right), op);
  }

  return left;
}

ast::Expr* Parser::parseBinaryExpr() { return parseBinaryExprPrecedence(0); }

ast::Expr* Parser::parseBinaryExprPrecedence(int min_precedence)
{
  ast::Expr* left = parseUnaryExpr();

  while (true)
  {
    int precedence = getArithmeticOperatorPrecedence(lex_.current().type());
    if (precedence < min_precedence) break;

    lex::tok::TokenType op = lex_.current().type();
    advance();

    // Handle right associativity for power operator
    int nextMinPrecedence;
    // if (op == lex::tok::TokenType::DOUBLE_STAR)
    // nextMinPrecedence = precedence;  // Right associative: same precedence
    // else
    nextMinPrecedence = precedence + 1;  // Left associative: higher precedence

    ast::Expr* right = parseBinaryExprPrecedence(nextMinPrecedence);
    if (left) delete left;
    left = new ast::BinaryExpr(std::move(left), std::move(right), op);
  }

  return left;
}

int Parser::getArithmeticOperatorPrecedence(const lex::tok::TokenType type)
{
  switch (type)
  {
  // case lex::tok::TokenType::DOUBLE_STAR :  // **
  // return 4;
  case lex::tok::TokenType::OP_STAR :   // *
  case lex::tok::TokenType::OP_SLASH :  // /
                                        // case lex::tok::TokenType::PERCENT :       // %
    return 3;
  case lex::tok::TokenType::OP_PLUS :   // +
  case lex::tok::TokenType::OP_MINUS :  // -
    return 2;
  default : return -1;  // Not an arithmetic operator
  }
}

ast::Expr* Parser::parseUnaryExpr()
{
  if (check(lex::tok::TokenType::OP_PLUS) || check(lex::tok::TokenType::OP_MINUS) /*|| check(lex::tok::TokenType::TILDE)*/)
  {
    lex::tok::TokenType op = lex_.current().type();
    advance();
    ast::Expr* expr = parseUnaryExpr();
    return new ast::UnaryExpr(std::move(expr), op);
  }

  return parsePostfixExpr();
}

ast::Expr* Parser::parsePostfixExpr()
{
  ast::Expr* expr = parsePrimaryExpr();

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
          args.push_back(parseExpression());
        } while (match(lex::tok::TokenType::COMMA));
      }

      consume(lex::tok::TokenType::RPAREN, u"Expected ')' after arguments");
      ast::CallExpr call_expr = ast::CallExpr(std::move(expr), std::move(args));
      expr = dynamic_cast<ast::Expr*>(&call_expr);
    }
    /*
    else if (check(lex::tok::TokenType::LBRACKET))
    {
      // Indexing
      advance();
      ast::Expr* index = parseExpression();
      consume(lex::tok::TokenType::RBRACKET, u"Expected ']' after index");
      expr = std::make_unique<ast::IndexExpr>(std::move(expr), std::move(index));
    }
    else if (check(lex::tok::TokenType::DOT))
    {
      // Member access
      advance();
      if (!check(lex::tok::TokenType::IDENTIFIER)) { error(u"Expected identifier after '.'"); }
      std::u32string member = currentToken().value;
      advance();
      expr = std::make_unique<ast::MemberExpr>(std::move(expr), member);
    }
    */
    else
    {
      break;
    }
  }

  return expr;
}

ast::Expr* Parser::parsePrimaryExpr()
{
  // Literals
  if (check(lex::tok::TokenType::NUMBER))
  {
    string_type value = lex_.current().lexeme();
    advance();
    return new ast::LiteralExpr(ast::LiteralExpr::Type::NUMBER, new ast::Expr(value));
  }

  /*
  if (check(lex::tok::TokenType::FLOAT))
  {
    std::u32string value = currentToken().value;
    advance();
    return std::make_unique<ast::FloatLiteral>(value);
  }
  */

  if (check(lex::tok::TokenType::STRING))
  {
    string_type value = lex_.current().lexeme();
    advance();
    return new ast::LiteralExpr(ast::LiteralExpr::Type::STRING, new ast::Expr(value));
  }

  if (check(lex::tok::TokenType::KW_TRUE) || check(lex::tok::TokenType::KW_FALSE))
  {
    // bool value = check(lex::tok::TokenType::KW_TRUE);
    string_type value = lex_.current().lexeme();
    advance();
    return new ast::LiteralExpr(ast::LiteralExpr::Type::BOOLEAN, new ast::Expr(value));
  }

  if (check(lex::tok::TokenType::KW_NONE))
  {
    advance();
    return new ast::LiteralExpr(ast::LiteralExpr::Type::NONE, new ast::Expr(u""));
  }

  // Identifier
  if (check(lex::tok::TokenType::IDENTIFIER))
  {
    string_type name = lex_.current().lexeme();
    advance();
    return new ast::NameExpr(new ast::Expr(name));
  }

  // Parenthesized expression
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
        elements.push_back(parseExpression());
      } while (match(lex::tok::TokenType::COMMA));
    }

    consume(lex::tok::TokenType::RBRACKET, u"Expected ']' after list elements");
    return new ast::ListExpr(std::move(elements));
  }

  // Dict literal
  /*
  if (check(lex::tok::TokenType::LBRACE))
  {
    advance();
    std::vector<std::pair<ast::Expr*, ast::Expr*>> pairs;
    
    if (!check(lex::tok::TokenType::RBRACE))
    {
      do
      {
        ast::Expr* key = parseExpression();
        consume(lex::tok::TokenType::COLON, u"Expected ':' in dictionary literal");
        ast::Expr* value = parseExpression();
        pairs.push_back({std::move(key), std::move(value)});
      } while (match(lex::tok::TokenType::COMMA));
    }
    
    consume(lex::tok::TokenType::RBRACE, u"Expected '}' after dictionary elements");
    return std::make_unique<ast::DictExpr>(std::move(pairs));
  }
  */

  // error(u"Expected expression");
  return nullptr;
}

ast::Expr* Parser::parseListLiteral()
{
  std::vector<ast::Expr*> elements;
  if (!check(lex::tok::TokenType::RBRACKET))
  {
    // ... 'list comprehension'

    while (match(lex::tok::TokenType::COMMA))
    {
      skipNewlines();
      if (check(lex::tok::TokenType::RBRACKET)) break;

      // ... 'unpacking'

      elements.push_back(parseExpression());
      skipNewlines();
    }
  }

  /// @todo : hell find a better way to report errors
  consume(lex::tok::TokenType::RBRACKET, u"Expected ']' after list elements");
  return new ast::ListExpr(std::move(elements));
}

// helpers
bool Parser::isUnaryOp(const lex::tok::Token tok)
{
  return tok.type() == lex::tok::TokenType::OP_MINUS || tok.type() == lex::tok::TokenType::OP_BITNOT;
}

lex::tok::Token Parser::peek(std::size_t offset)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : peek() called!" << std::endl;
#endif
  return lex_.peek(offset);
}

lex::tok::Token Parser::advance()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : advance() called!" << std::endl;
#endif
  return lex_.next();
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
  return lex_.current().is(type);
}

lex::tok::Token Parser::consume(lex::tok::TokenType type, const string_type& msg)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : Parser::consume() called!" << std::endl;
#endif
  if (check(type)) return advance();
  /// @todo: error ...
  // throw ParseError(msg, peek().line(), peek().column(), context, suggestions);
  // Placeholder to suppress warnings
  return lex::tok::Token();
}

void Parser::skipNewlines()
{
  while (match(lex::tok::TokenType::NEWLINE))
    ;
}

string_type Parser::getSourceLine(std::size_t line)
{
  // this would retrieve the actual source line
  // simplified for now
  /// @todo: use the file manager
  return peek().lexeme();
}

}
}
