#include "../../include/parser/incremental.hpp"

namespace mylang {
namespace parser {

std::size_t IncrementalParser::hashRegion(StringRef const& source, std::size_t start, std::size_t end)
{
    std::hash<StringRef> hasher;
    return hasher(source.substr(start, end - start));
}

// Only reparse changed regions
std::vector<ast::Stmt*> IncrementalParser::parseIncremental(StringRef const& newSource, std::vector<std::size_t> const& changedLines)
{
    // Identify unchanged regions and reuse cached AST
    std::vector<ast::Stmt*> result;
    // Implementation would diff source and reparse only changes
    return result;
}

}
}
