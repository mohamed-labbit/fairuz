#pragma once

#include "../ast.hpp"

namespace mylang {
namespace parser {

class IncrementalParser {
private:
    struct ParseNode {
        size_t StartPos, EndPos;
        std::unique_ptr<ast::ASTNode> ast;
        size_t hash;
    };

    std::vector<ParseNode> Cache_;
    StringRef LastSource_;

    size_t hashRegion(StringRef const& source, size_t start, size_t end);

public:
    // Only reparse changed regions
    std::vector<ast::Stmt*> parseIncremental(StringRef const& newSource, std::vector<size_t> const& changedLines);
};

}
}
