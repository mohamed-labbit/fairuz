#pragma once

#include <string>
#include <vector>

#include "../../macros.hpp"
#include "bytecode.hpp"


// ============================================================================
// PEEPHOLE OPTIMIZER - Local optimization
// ============================================================================

namespace mylang {
namespace runtime {

class PeepholeOptimizer
{
 public:
  struct Optimization
  {
    std::string  name;
    std::int32_t ReplacementCount;
  };

 private:
  std::vector<Optimization> Optimizations_;
  bool                      matchPattern(const std::vector<bytecode::Instruction>& code, SizeType pos, const std::vector<bytecode::OpCode>& pattern);

 public:
  void                             optimize(std::vector<bytecode::Instruction>& instructions);
  
  const std::vector<Optimization>& getOptimizations() const;
  
  void                             printReport() const;
};  // PeepholeOptimizer

}
}