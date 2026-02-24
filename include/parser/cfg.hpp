#pragma once

#include "../ast/ast.hpp"
#include "../macros.hpp"

#include <unordered_set>
#include <vector>

namespace mylang {
namespace parser {

// control flow graph for advanced analysis
class ControlFlowGraph {
public:
    struct BasicBlock {
        int32_t id;

        std::vector<ast::Stmt*> statements;
        std::vector<int32_t> predecessors;
        std::vector<int32_t> successors;

        bool IsReachable = false;

        // data flow analysis
        std::unordered_set<StringRef, StringRefHash, StringRefEqual> DefVars; // variables defined
        std::unordered_set<StringRef, StringRefHash, StringRefEqual> UseVars; // variables used
        std::unordered_set<StringRef, StringRefHash, StringRefEqual> LiveIn;  // live at entry
        std::unordered_set<StringRef, StringRefHash, StringRefEqual> LiveOut; // live at exit
    };

private:
    std::vector<BasicBlock> Blocks_;
    int32_t EntryBlock_ = 0;
    int32_t ExitBlock_ = -1;

public:
    void addBlock(BasicBlock block);

    void addEdge(int32_t from, int32_t to);

    void computeReachability();

    // dead code detection
    void computeLiveness();

    std::vector<int32_t> getUnreachableBlocks() const;

    std::vector<BasicBlock> const& getBlocks() const;
};

} // parser
} // mylang
