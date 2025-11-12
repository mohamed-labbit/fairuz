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
        int startPC;
        int endPC;
        int executionCount;
        bool compiled;
        void* nativeCode;
    };

    std::unordered_map<int, HotSpot> hotSpots_;
    int hotThreshold_ = 100;  // Compile after 100 executions

   public:
    void recordExecution(int pc);

    bool isHotSpot(int pc) const;

   private:
    void compileHotSpot(HotSpot& spot);
};

}
}