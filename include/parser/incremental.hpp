#pragma once

#include "../ast/ast.hpp"

namespace mylang {
namespace parser {

class IncrementalParser {
private:
    struct ParseNode {
        std::size_t StartPos, EndPos;
        std::unique_ptr<ast::ASTNode> ast;
        std::size_t hash;
    };

    std::vector<ParseNode> Cache_;
    StringRef LastSource_;

    std::size_t hashRegion(StringRef const& source, std::size_t start, std::size_t end);

public:
    // Only reparse changed regions
    std::vector<ast::Stmt*> parseIncremental(StringRef const& newSource, std::vector<std::size_t> const& changedLines);
};

}
}
