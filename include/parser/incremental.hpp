#pragma once

#include "ast/ast.hpp"


namespace mylang {
namespace parser {

class IncrementalParser
{
 private:
  struct ParseNode
  {
    SizeType                      StartPos, EndPos;
    std::unique_ptr<ast::ASTNode> ast;
    SizeType                      hash;
  };

  std::vector<ParseNode> Cache_;
  StringType             LastSource_;

  SizeType hashRegion(const StringType& source, SizeType start, SizeType end);

 public:
  // Only reparse changed regions
  std::vector<ast::Stmt*> parseIncremental(const StringType& newSource, const std::vector<SizeType>& changedLines);
};

}
}