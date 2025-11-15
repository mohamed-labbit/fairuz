#include "../../../include/runtime/vm/vm.hpp"

#include <iostream>

namespace mylang {
namespace runtime {

void JITCompiler::recordExecution(std::int32_t pc)
{
    auto it = hotSpots_.find(pc);
    if (it != hotSpots_.end())
    {
        it->second.executionCount++;
        if (it->second.executionCount >= hotThreshold_ && !it->second.compiled)
        {
            compileHotSpot(it->second);
        }
    }
}

bool JITCompiler::isHotSpot(std::int32_t pc) const
{
    auto it = hotSpots_.find(pc);
    return it != hotSpots_.end() && it->second.compiled;
}

void JITCompiler::compileHotSpot(HotSpot& spot)
{
    // JIT compile to native code
    // This would generate x86-64/ARM assembly for the hot loop
    spot.compiled = true;
    std::cout << "[JIT] Compiled hot spot at PC " << spot.startPC << "\n";
}

}
}