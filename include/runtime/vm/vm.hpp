#pragma once

// Virtual Machine interface for mylang.
//
// This header defines the core runtime VirtualMachine responsible for executing
// bytecode produced by the compiler. The VM supports advanced features such as
// JIT compilation, garbage collection, native function calls, and performance
// monitoring.

#include <array>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../object/object.hpp"
#include "btcompiler.hpp"
#include "bytecode.hpp"
#include "gc.hpp"
#include "jit.hpp"


namespace mylang {
namespace runtime {

// VIRTUAL MACHINE with optimizations
//
// The VirtualMachine executes bytecode instructions in a stack-based execution
// model. It maintains execution state, supports call frames, integrates a JIT
// compiler, and tracks detailed runtime statistics.
class VirtualMachine
{
 private:
  // Execution state

  std::vector<object::Value> Stack_;    // Operand stack for expression evaluation
  std::vector<object::Value> Globals_;  // Global variables storage

  // Call frames representing nested function scopes.
  // Each frame maps variable names to their corresponding values.
  std::vector<std::unordered_map<std::string, object::Value>> Frames_;

  std::int32_t Ip_ = 0;  // Instruction pointer (index into bytecode)

  // Advanced runtime features

  JITCompiler      Jit_;  // Just-In-Time compiler for hot code paths
  GarbageCollector Gc_;   // Garbage collector for managed objects

  // Performance monitoring and statistics

  /**
   * @brief Runtime execution statistics.
   *
   * This structure aggregates metrics useful for profiling and debugging
   * runtime performance.
   */
  struct Statistics
  {
    std::int64_t              InstructionsExecuted{0};  // Total number of executed instructions
    std::int64_t              FunctionsCalled{0};       // Number of function calls
    std::int64_t              GcCollections{0};         // Garbage collection runs
    std::int64_t              JitCompilations{0};       // JIT compilation events
    std::chrono::microseconds ExecutionTime{0};         // Total execution time
  } Stats_;

  // Instruction dispatch optimization

  // Size of the instruction dispatch cache.
  static constexpr SizeType CACHE_SIZE_ = 256;

  // Cached dispatch table used to speed up instruction execution.
  std::array<void*, CACHE_SIZE_> DispatchTable_;

  // Stack manipulation helpers (with bounds checking)

  /// @brief Pushes a value onto the VM stack
  void push(const object::Value& val);

  /// @brief Pops the top value from the VM stack
  object::Value pop();

  /// @brief Returns a reference to the top stack value
  object::Value& top();

  /// @brief Returns a reference to a value at a given offset from the top
  object::Value& peek(std::int32_t offset);

  // Fast-path arithmetic operations

  // These helpers implement optimized arithmetic assuming compatible operand
  // types, avoiding repeated dynamic type checks.

  object::Value fastAdd(const object::Value& a, const object::Value& b);
  
  object::Value fastSub(const object::Value& a, const object::Value& b);
  
  object::Value fastMul(const object::Value& a, const object::Value& b);

  // Native function support

  // Registry of native functions callable from bytecode.
  std::unordered_map<std::string, std::function<object::Value(const std::vector<object::Value>&)>> nativeFunctions;

  /// @brief Registers all built-in native functions
  void registerNativeFunctions();

 public:
  /**
   * @brief Constructs a virtual machine instance.
   *
   * Native functions are registered during construction.
   */
  VirtualMachine() { registerNativeFunctions(); }

  /**
   * @brief Executes a compiled bytecode unit.
   *
   * @param code The compiled bytecode and associated metadata
   */
  void execute(const BytecodeCompiler::CompilationUnit& code);

 private:
  /**
   * @brief Executes a single bytecode instruction.
   *
   * @param instr Current instruction to execute
   * @param code  Owning compilation unit
   */
  void executeInstruction(const bytecode::Instruction& instr, const BytecodeCompiler::CompilationUnit& code);

 public:
  /// @brief Returns collected runtime statistics
  const Statistics& getStatistics() const { return Stats_; }

  /// @brief Prints runtime statistics to the output
  void printStatistics() const;

  /// @brief Returns the current global variable table
  const std::vector<object::Value>& getGlobals() const { return Globals_; }

  /// @brief Returns the current VM operand stack
  const std::vector<object::Value>& getStack() const { return Stack_; }
};  // VirtualMachine

}  // namespace runtime
}  // namespace mylang
