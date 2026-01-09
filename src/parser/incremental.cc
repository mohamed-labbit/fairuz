#include "../../include/parser/incremental.hpp"


namespace mylang {
namespace parser {

std::size_t IncrementalParser::hashRegion(const StringType& source, std::size_t start, std::size_t end)
{
  std::hash<StringType> hasher;
  return hasher(source.substr(start, end - start));
}

// Only reparse changed regions
std::vector<ast::Stmt*> IncrementalParser::parseIncremental(const StringType& newSource, const std::vector<std::size_t>& changedLines)
{
  // Identify unchanged regions and reuse cached AST
  std::vector<ast::Stmt*> result;
  // Implementation would diff source and reparse only changes
  return result;
}

}
}