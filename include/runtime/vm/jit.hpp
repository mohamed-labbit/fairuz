#pragma once

#include <unordered_map>


namespace mylang {
namespace runtime {

// ============================================================================
// ENHANCED BYTECODE INSTRUCTION SET
// ============================================================================
// ============================================================================
// JIT COMPILATION SUPPORT - Hot path detection and native code generation
// ============================================================================
class JITCompiler
{
   private:
    struct HotSpot
    {
        unsigned startPC;
        unsigned endPC;
        unsigned executionCount;
        bool compiled;
        void* nativeCode;
    };

    std::unordered_map<std::int32_t, HotSpot> hotSpots_;
    std::int32_t hotThreshold_ = 100;  // Compile after 100 executions

   public:
    void recordExecution(std::int32_t pc);

    bool isHotSpot(std::int32_t pc) const;

   private:
    void compileHotSpot(HotSpot& spot);
};

}
}