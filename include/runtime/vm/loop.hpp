#pragma once

#include <vector>
#include <unordered_set>

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
    std::int32_t headerPC;
    std::int32_t exitPC;
    std::vector<std::int32_t> bodyPCs;
    std::unordered_set<std::int32_t> invariants;  // Loop-invariant variables
    bool isInnerLoop;
    std::int32_t nestingLevel;
  };

 private:
  std::vector<Loop> loops_;

 public:
  void detectLoops(const std::vector<bytecode::Instruction>& instructions);
  void findInvariants(const std::vector<bytecode::Instruction>& instructions, const CompilerSymbolTable& symbols);
  const std::vector<Loop>& getLoops() const;
};  // LoopAnalyzer

}
}