#pragma once

#include "../ast/ast.hpp"

namespace mylang {
namespace parser {

class IncrementalParser {
private:
    struct ParseNode {
        SizeType StartPos, EndPos;
        std::unique_ptr<ast::ASTNode> ast;
        SizeType hash;
    };

    std::vector<ParseNode> Cache_;
    StringRef LastSource_;

    SizeType hashRegion(StringRef const& source, SizeType start, SizeType end);

public:
    // Only reparse changed regions
    std::vector<ast::Stmt*> parseIncremental(StringRef const& newSource, std::vector<SizeType> const& changedLines);
};

}
}
