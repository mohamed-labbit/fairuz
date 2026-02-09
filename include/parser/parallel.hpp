#pragma once

#include "../lex/token.hpp"
#include "../ast/ast.hpp"
#include <vector>


namespace mylang {
namespace parser {

class ParallelParser
{
 public:
  static std::vector<ast::Stmt*> parseParallel(const std::vector<tok::Token>& tokens, std::int32_t threadCount = 4);

 private:
  static std::vector<std::vector<tok::Token>> splitIntoChunks(const std::vector<tok::Token>& tokens);
};

}
}