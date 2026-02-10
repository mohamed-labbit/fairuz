#include "../../include/parser/incremental.hpp"


namespace mylang {
namespace parser {

SizeType IncrementalParser::hashRegion(const StringRef& source, SizeType start, SizeType end)
{
  std::hash<StringRef> hasher;
  return hasher(source.substr(start, end - start));
}

// Only reparse changed regions
std::vector<ast::Stmt*> IncrementalParser::parseIncremental(const StringRef&             newSource,
                                                            const std::vector<SizeType>& changedLines)
{
  // Identify unchanged regions and reuse cached AST
  std::vector<ast::Stmt*> result;
  // Implementation would diff source and reparse only changes
  return result;
}

}
}