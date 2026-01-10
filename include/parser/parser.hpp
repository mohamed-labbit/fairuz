// parser rewrite
// recursive descent parser
//
// This header defines the recursive-descent parser for mylang.
// The parser consumes tokens produced by the lexer and builds
// an abstract syntax tree (AST) representing expressions and statements.

#pragma once

#include "../input/file_manager.hpp"
#include "../lex/lexer.hpp"
#include "../lex/token.hpp"
#include "ast/ast.hpp"

#include <optional>
#include <sstream>


namespace mylang {
namespace parser {

/**
 * @brief Exception type representing a parse-time error.
 *
 * ParseError carries precise source location information (line and column),
 * optional source context, and a list of suggestions to aid error recovery
 * or diagnostics.
 */
class ParseError: public std::runtime_error
{
 public:
  std::int32_t Line_, Column_;           // Source location of the error
  StringType Context_;                   // Source line where the error occurred
  std::vector<StringType> Suggestions_;  // Optional recovery suggestions

  /**
   * @brief Constructs a parse error.
   *
   * @param msg  Error message (UTF-16)
   * @param l    Line number
   * @param c    Column number
   * @param ctx  Optional source line context
   * @param sugg Optional list of suggestions
   */
  ParseError(const StringType& msg, unsigned l, unsigned c, StringType ctx = u"", std::vector<StringType> sugg = {}) :
      std::runtime_error(utf8::utf16to8(msg)),
      Line_(l),
      Column_(c),
      Context_(std::move(ctx)),
      Suggestions_(std::move(sugg))
  {
  }

  /**
   * @brief Formats the parse error into a user-readable message.
   *
   * The output includes:
   *  - Line and column
   *  - Error message
   *  - Source line with a caret pointing at the error location
   *  - Optional suggestions
   *
   * @return UTF-16 formatted error message
   */
  StringType format() const
  {
    std::stringstream ss;
    ss << "Line " << Line_ << ":" << Column_ << " - " << what() << "\n";

    if (!Context_.empty())
    {
      ss << "  | " << utf8::utf16to8(Context_) << "\n";
      ss << "  | " << std::string(Column_ - 1, ' ') << "^\n";
    }

    if (!Suggestions_.empty())
    {
      ss << "Suggestions:\n";
      for (const StringType& s : Suggestions_) ss << "  - " << utf8::utf16to8(s) << "\n";
    }

    return utf8::utf8to16(ss.str());
  }
};

/**
 * @brief Recursive-descent parser for mylang.
 *
 * The Parser consumes tokens from the lexer and produces AST nodes.
 * It implements expression parsing using a combination of:
 *  - Recursive descent
 *  - Operator precedence climbing
 *  - Specialized routines for different grammar constructs
 */
class Parser
{
 public:
  /// @brief Constructs an empty parser
  explicit Parser() = default;

  /// @brief Constructs a parser bound to a file manager
  explicit Parser(input::FileManager* fm) :
      Lexer_(fm)
  {
    if (fm == nullptr) diagnostic::engine.panic("file_manager is NULL!");
    // Advance to the first real token
    Lexer_.next();
  }


  /// @brief Constructs a parser from a pre-existing token sequence
  explicit Parser(std::vector<lex::tok::Token> seq, std::optional<std::size_t> s = std::nullopt);

  ast::Expr* parseParenthesizedExprContent();

  // Expression parsing entry points

  /// @brief does exactly what it says
  ast::ListExpr* parseFunctionArguments();

  ast::Expr* parseCallExpr(ast::Expr* callee);

  /// @brief Parses a primary expression
  ast::Expr* parsePrimary();

  /// @brief Parses a unary expression
  ast::Expr* parseUnary();

  /// @brief Parses a parenthesized expression
  ast::Expr* parseParenthesizedExpr();

  /// @brief Parses a general expression
  ast::Expr* parseExpression();

  /// @brief Parses an assignment expression
  ast::Expr* parseAssignmentExpr();

  /// @brief Parses a list literal expression
  ast::Expr* parseListLiteral();

  /// @brief Parses a conditional (ternary-like) expression
  ast::Expr* parseConditionalExpr()
  {
    return parseLogicalExpr();
    /// TODO: Add ternary operator support here
  }

  /// @brief Parses logical expressions (AND / OR)
  ast::Expr* parseLogicalExpr() { return parseLogicalExprPrecedence(0); }

  /// @brief Parses logical expressions using precedence climbing
  ast::Expr* parseLogicalExprPrecedence(int min_precedence);

  /// @brief Parses binary expressions using precedence climbing
  ast::Expr* parseBinaryExprPrecedence(int min_precedence);

  /// @brief Parses comparison expressions
  ast::Expr* parseComparisonExpr();

  /// @brief Parses binary expressions
  ast::Expr* parseBinaryExpr() { return parseBinaryExprPrecedence(0); }

  /// @brief Parses unary expressions
  ast::Expr* parseUnaryExpr();

  /// @brief Parses primary expressions
  ast::Expr* parsePrimaryExpr();

  /// @brief Parses postfix expressions (calls, indexing, etc.)
  ast::Expr* parsePostfixExpr();
  /**
   * @brief Parses the entire input into a sequence of statements.
   *
   * @return Vector of parsed statement AST nodes
   */
  // std::vector<ast::Stmt*> parse();
  /// TODO: not sure if these should be private
  /// @brief check wether or not we reached the end of the file so not to bother lookin for stuff to parse
  bool weDone() const { return Lexer_.current().is(lex::tok::TokenType::ENDMARKER); }

  /// @brief Checks whether the current token is of the given type
  bool check(lex::tok::TokenType type)
  {
    // if (weDone()) return false;
    return Lexer_.current().is(type);
  }

  ast::Expr* parse() { return parseExpression(); }

 private:
  lex::Lexer Lexer_;  // Underlying lexer providing tokens

  /// @brief Checks whether a token represents a unary operator
  static bool isUnaryOp(const lex::tok::Token tok)
  {
    return tok.type() == lex::tok::TokenType::OP_MINUS || tok.type() == lex::tok::TokenType::OP_BITNOT || tok.type() == lex::tok::TokenType::OP_PLUS;
  }


  /// @brief Checks whether a token represents a binary operator
  static bool isBinaryOp(const lex::tok::Token tok);

  /// @brief Peeks ahead in the token stream without consuming
  lex::tok::Token peek(std::size_t offset = 1)
  {
    return Lexer_.peek(offset);
  }

  /// @brief Advances and returns the next token
  lex::tok::Token advance()
  {
    return Lexer_.next();
  }


  /// @brief Matches and consumes a token if it is of the given type
  bool match(const lex::tok::TokenType type);
  /**
   * @brief Consumes a token of the expected type or throws a ParseError.
   *
   * @param type Expected token type
   * @param msg  Error message if the token does not match
   */
  lex::tok::Token consume(lex::tok::TokenType type, const StringType& msg)
  {
    if (check(type)) return advance();
    /// TODO: Implement proper error reporting
    // For now, return empty token
    return lex::tok::Token();
  }

  /// @brief Skips newline tokens during parsing
  void skipNewlines() { while (match(lex::tok::TokenType::NEWLINE)); }

  /// @brief Retrieves a source line for diagnostics
  StringType getSourceLine(std::size_t line)
  {
    /// TODO: Use the file manager to retrieve actual source line
    return peek().lexeme();
  }

  /// @brief Returns precedence of logical operators
  int getLogicalOperatorPrecedence(lex::tok::TokenType tt);

  /// @brief Returns precedence of arithmetic operators
  int getArithmeticOperatorPrecedence(const lex::tok::TokenType type);

  /// @brief Enters a new scope (currently a no-op)
  void enterScope() {}
};

}  // namespace parser
}  // namespace mylang