#pragma once

#include "ast/ast.hpp"


namespace mylang {
namespace parser {

class IncrementalParser
{
 private:
  struct ParseNode
  {
    std::size_t StartPos, EndPos;
    std::unique_ptr<ast::ASTNode> ast;
    std::size_t hash;
  };

  std::vector<ParseNode> Cache_;
  string_type LastSource_;

  std::size_t hashRegion(const string_type& source, std::size_t start, std::size_t end);

 public:
  // Only reparse changed regions
  std::vector<ast::Stmt*> parseIncremental(const string_type& newSource, const std::vector<std::size_t>& changedLines);
};

}
}