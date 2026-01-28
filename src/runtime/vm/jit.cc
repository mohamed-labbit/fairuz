#include "../../../include/runtime/vm/vm.hpp"

#include <iostream>


namespace mylang {
namespace runtime {

void JITCompiler::recordExecution(std::int32_t pc)
{
  auto it = HotSpots_.find(pc);
  if (it != HotSpots_.end())
  {
    ++it->second.ExecutionCount;
    if (it->second.ExecutionCount >= HotThreshold_ && !it->second.Compiled)
      compileHotSpot(it->second);
  }
}

bool JITCompiler::isHotSpot(std::int32_t pc) const
{
  auto it = HotSpots_.find(pc);
  return it != HotSpots_.end() && it->second.Compiled;
}

void JITCompiler::compileHotSpot(HotSpot& spot)
{
  // JIT compile to native code
  // This would generate x86-64/ARM assembly for the hot loop
  spot.Compiled = true;
  std::cout << "[JIT] Compiled hot spot at PC " << spot.StartPC << "\n";
}

}
}