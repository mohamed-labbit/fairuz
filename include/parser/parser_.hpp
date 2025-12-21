// parser rewrite
// recursive descent parser

#pragma once

#include "../input/file_manager.hpp"
#include "../lex/lexer.hpp"
#include "../lex/token.hpp"
#include "ast/ast_.hpp"
#include <sstream>


namespace mylang {
namespace parser {

class ParseError: public std::runtime_error
{
 public:
  std::int32_t line, column;
  string_type context;
  std::vector<string_type> suggestions;

  ParseError(const string_type& msg, unsigned l, unsigned c, string_type ctx = u"", std::vector<string_type> sugg = {}) :
      std::runtime_error(utf8::utf16to8(msg)),
      line(l),
      column(c),
      context(std::move(ctx)),
      suggestions(std::move(sugg))
  {
  }

  string_type format() const
  {
    std::stringstream ss;
    ss << "Line " << line << ":" << column << " - " << what() << "\n";
    if (!context.empty())
    {
      ss << "  | " << utf8::utf16to8(context) << "\n";
      ss << "  | " << std::string(column - 1, ' ') << "^\n";
    } 
    if (!suggestions.empty())
    {
      ss << "Suggestions:\n";
      for (const string_type& s : suggestions)
        ss << "  - " << utf8::utf16to8(s) << "\n";
    }
    return utf8::utf8to16(ss.str());
  }
};

class Parser
{
 public:
  // constructors
  explicit Parser() = default;
  explicit Parser(input::FileManager* file_manager);

  ast::ExprPtr parsePrimary();
  ast::ExprPtr parseUnary();
  ast::ExprPtr parseParenthesizedExpr();
  ast::ExprPtr parseExpression();
  ast::ExprPtr parseAssignmentExpr();
  ast::ExprPtr parseListLiteral();

 private:
  lex::Lexer lex_;

  bool isUnaryOp(lex::tok::Token tok) const;
  lex::tok::Token peek(std::size_t offset = 1);
  lex::tok::Token advance();
  bool match(const lex::tok::TokenType type);
  bool check(lex::tok::TokenType type);
  lex::tok::Token consume(lex::tok::TokenType type, const string_type& msg);
  void skipNewlines();
  string_type getSourceLine(std::size_t line);


  void enterScope() {}
};

}
}