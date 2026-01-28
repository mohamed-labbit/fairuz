#pragma once

#include "../macros.hpp"
#include "ast/ast.hpp"

#include <unordered_set>
#include <vector>


namespace mylang {
namespace parser {

// Control flow graph for advanced analysis
class ControlFlowGraph
{
 public:
  struct BasicBlock
  {
    std::int32_t              id;
    std::vector<ast::Stmt*>   statements;
    std::vector<std::int32_t> predecessors;
    std::vector<std::int32_t> successors;
    bool                      IsReachable = false;
    // Data flow analysis
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> DefVars;  // Variables defined
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> UseVars;  // Variables used
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> LiveIn;   // Live at entry
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> LiveOut;  // Live at exit
  };

 private:
  std::vector<BasicBlock> Blocks_;
  std::int32_t            EntryBlock_ = 0;
  std::int32_t            ExitBlock_  = -1;

 public:
  void addBlock(BasicBlock block);

  void addEdge(std::int32_t from, std::int32_t to);

  // Compute reachability
  void computeReachability();

  // Liveness analysis for dead code detection
  void computeLiveness();

  std::vector<std::int32_t> getUnreachableBlocks() const;

  const std::vector<BasicBlock>& getBlocks() const;
};

}  // parser
}  // mylang