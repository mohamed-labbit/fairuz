#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "bytecode.hpp"


// ============================================================================
// JUMP TARGET RESOLVER - Handle forward/backward jumps
// ============================================================================

namespace mylang {
namespace runtime {

class JumpResolver
{
 private:
  struct PendingJump
  {
    std::int32_t InstructionIndex;
    std::string LabelName;
  };

  std::unordered_map<std::string, std::int32_t> Labels_;
  std::vector<PendingJump> PendingJumps_;

 public:
  void defineLabel(const std::string& name, std::int32_t position);
  void addJump(std::int32_t instrIndex, const std::string& target);
  void resolveJumps(std::vector<bytecode::Instruction>& instructions);
  std::int32_t getLabel(const std::string& name) const;
};  // JumpResolver

}
}