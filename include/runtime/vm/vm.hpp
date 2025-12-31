#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>
#include <functional>
#include <array>

#include "../object/object.hpp"
#include "gc.hpp"
#include "jit.hpp"
#include "bytecode.hpp"
#include "btcompiler.hpp"


namespace mylang {
namespace runtime {

// ================================================
// ENHANCED VIRTUAL MACHINE with optimizations
// ============================================================================
class VirtualMachine
{
 private:
  // Execution state
  std::vector<object::Value> stack_;
  std::vector<object::Value> globals_;
  std::vector<std::unordered_map<std::string, object::Value>> frames_;  // Call frames
  std::int32_t ip_ = 0;

  // Advanced features
  JITCompiler jit_;
  GarbageCollector gc_;

  // Performance monitoring
  struct Statistics
  {
    std::int64_t instructionsExecuted = 0;
    std::int64_t functionsCalled = 0;
    std::int64_t gcCollections = 0;
    std::int64_t jitCompilations = 0;
    std::chrono::microseconds executionTime{0};
  } stats_;

  // Instruction cache for faster dispatch
  static constexpr std::size_t CACHE_SIZE_ = 256;
  std::array<void*, CACHE_SIZE_> dispatchTable_;

  // Stack manipulation (with bounds checking)
  void push(const object::Value& val);
  object::Value pop();
  object::Value& top();
  object::Value& peek(std::int32_t offset);

  // Fast integer operations (avoid type checking overhead)
  object::Value fastAdd(const object::Value& a, const object::Value& b);
  object::Value fastSub(const object::Value& a, const object::Value& b);
  object::Value fastMul(const object::Value& a, const object::Value& b);
  // Native function registration
  std::unordered_map<std::string, std::function<object::Value(const std::vector<object::Value>&)>> nativeFunctions;
  void registerNativeFunctions();

 public:
  VirtualMachine() { registerNativeFunctions(); }

  void execute(const BytecodeCompiler::CompilationUnit& code);

 private:
  void executeInstruction(const bytecode::Instruction& instr, const BytecodeCompiler::CompilationUnit& code);

 public:
  const Statistics& getStatistics() const { return stats_; }
  void printStatistics() const;
  const std::vector<object::Value>& getGlobals() const { return globals_; }
  const std::vector<object::Value>& getStack() const { return stack_; }
};  // VirtualMachine

}  // runtime
}  // mylang