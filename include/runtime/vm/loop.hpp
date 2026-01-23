#pragma once

#include <unordered_set>
#include <vector>

#include "bytecode.hpp"
#include "symbol_table.hpp"


namespace mylang {
namespace runtime {

// ============================================================================
// LOOP ANALYSIS - Detect and optimize loops
// ============================================================================
class LoopAnalyzer
{
 public:
  struct Loop
  {
    std::int32_t                     HeaderPC;
    std::int32_t                     ExitPC;
    std::vector<std::int32_t>        BodyPCs;
    std::unordered_set<std::int32_t> invariants;  // Loop-invariant variables
    bool                             IsInnerLoop;
    std::int32_t                     NestingLevel;
  };

 private:
  std::vector<Loop> Loops_;

 public:
  void                     detectLoops(const std::vector<bytecode::Instruction>& instructions);
  
  void                     findInvariants(const std::vector<bytecode::Instruction>& instructions, const CompilerSymbolTable& symbols);
  
  const std::vector<Loop>& getLoops() const;
};  // LoopAnalyzer

}
}