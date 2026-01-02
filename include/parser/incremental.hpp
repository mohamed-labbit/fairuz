#pragma once

#include "ast/ast.hpp"


namespace mylang {
namespace parser {

class IncrementalParser
{
 private:
  struct ParseNode
  {
    std::size_t startPos, endPos;
    std::unique_ptr<ast::ASTNode> ast;
    std::size_t hash;
  };

  std::vector<ParseNode> cache_;
  string_type lastSource_;

  std::size_t hashRegion(const string_type& source, std::size_t start, std::size_t end);

 public:
  // Only reparse changed regions
  std::vector<ast::Stmt*> parseIncremental(const string_type& newSource, const std::vector<std::size_t>& changedLines);
};

}
}