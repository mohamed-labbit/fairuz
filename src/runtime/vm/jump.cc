#include "../../../include/runtime/vm/jump.hpp"
#include "../../../include/diag/diagnostic.hpp"


namespace mylang {
namespace runtime {

void JumpResolver::defineLabel(const std::string& name, std::int32_t position) { Labels_[name] = position; }

void JumpResolver::addJump(std::int32_t instrIndex, const std::string& target)
{
  auto it = Labels_.find(target);
  if (it != Labels_.end())
  {
    /// @todo:
    // Label already defined - immediate resolution
    // (Would patch instruction here)
  }
  else
  {
    PendingJumps_.push_back({instrIndex, target});
  }
}

void JumpResolver::resolveJumps(std::vector<bytecode::Instruction>& instructions)
{
  for (const PendingJump& jump : PendingJumps_)
  {
    auto it = Labels_.find(jump.LabelName);
    if (it != Labels_.end())
      instructions[jump.InstructionIndex].arg = it->second;
    else
      diagnostic::engine.panic("Undefined label: " + jump.LabelName);
  }
}

std::int32_t JumpResolver::getLabel(const std::string& name) const
{
  auto it = Labels_.find(name);
  return it != Labels_.end() ? it->second : -1;
}

}
}