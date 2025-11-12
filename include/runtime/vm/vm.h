#pragma once


#include "bytecode.h"
#include "collector.h"
#include "jit.h"


namespace mylang {
namespace runtime {


// ============================================================================
// ENHANCED VIRTUAL MACHINE with optimizations
// ============================================================================
class VirtualMachine
{
   private:
    // Execution state
    std::vector<object::Value> stack_;
    std::vector<object::Value> globals_;
    std::vector<std::unordered_map<std::string, object::Value>> frames_;  // Call frames
    int ip_ = 0;

    // Advanced features
    JITCompiler jit_;
    GarbageCollector gc_;

    // Performance monitoring
    struct Statistics
    {
        long long instructionsExecuted = 0;
        long long functionsCalled = 0;
        long long gcCollections = 0;
        long long jitCompilations = 0;
        std::chrono::microseconds executionTime{0};
    } stats_;

    // Instruction cache for faster dispatch
    static constexpr size_t CACHE_SIZE_ = 256;
    std::array<void*, CACHE_SIZE_> dispatchTable_;

    // Stack manipulation (with bounds checking)
    void push(const object::Value& val);

    object::Value pop();

    object::Value& top();

    object::Value& peek(int offset);

    // Fast integer operations (avoid type checking overhead)
    object::Value fastAdd(const object::Value& a, const object::Value& b);

    object::Value fastSub(const object::Value& a, const object::Value& b);

    object::Value fastMul(const object::Value& a, const object::Value& b);

    // Native function registration
    std::unordered_map<std::string, std::function<object::Value(const std::vector<object::Value>&)>> nativeFunctions;

    void registerNativeFunctions();

   public:
    VirtualMachine() { registerNativeFunctions(); }

    void execute(const BytecodeCompiler::CompiledCode& code);

   private:
    void executeInstruction(const bytecode::Instruction& instr, const BytecodeCompiler::CompiledCode& code);

   public:
    const Statistics& getStatistics() const { return stats_; }

    void printStatistics() const;

    const std::vector<object::Value>& getGlobals() const { return globals_; }

    const std::vector<object::Value>& getStack() const { return stack_; }
};}}