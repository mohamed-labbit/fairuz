#include "../../../include/runtime/vm/jump.hpp"
#include "../../../include/diag/diagnostic.hpp"


namespace mylang {
namespace runtime {

void JumpResolver::defineLabel(const std::string& name, std::int32_t position) { labels_[name] = position; }

void JumpResolver::addJump(std::int32_t instrIndex, const std::string& target)
{
  auto it = labels_.find(target);
  if (it != labels_.end())
  {
    /// @todo:
    // Label already defined - immediate resolution
    // (Would patch instruction here)
  }
  else
  {
    pendingJumps_.push_back({instrIndex, target});
  }
}

void JumpResolver::resolveJumps(std::vector<bytecode::Instruction>& instructions)
{
  for (const PendingJump& jump : pendingJumps_)
  {
    auto it = labels_.find(jump.labelName);
    if (it != labels_.end())
      instructions[jump.instructionIndex].arg = it->second;
    else
      diagnostic::engine.panic("Undefined label: " + jump.labelName);
  }
}

std::int32_t JumpResolver::getLabel(const std::string& name) const
{
  auto it = labels_.find(name);
  return it != labels_.end() ? it->second : -1;
}

}
}