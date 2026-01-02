#pragma once

// bytecode compiler for mylang.
//
// This header defines the BytecodeCompiler responsible for lowering the AST
// into executable bytecode. It includes control-flow analysis, symbol
// resolution, peephole optimizations, loop analysis, and stack-depth tracking.

#include <stack>
#include <unordered_map>
#include <vector>

#include "../../parser/ast/ast.hpp"
#include "../object/object.hpp"
#include "bytecode.hpp"
#include "constant_pool.hpp"
#include "jump.hpp"
#include "loop.hpp"
#include "peephole.hpp"
#include "symbol_table.hpp"


// BYTECODE COMPILER

namespace mylang {
namespace runtime {

/**
 * @brief Represents a basic block in the bytecode control-flow graph.
 *
 * Bytecode blocks are used during optimization passes such as
 * liveness analysis, loop detection, and control-flow restructuring.
 */
struct BytecodeBlock
{
  std::int32_t id;  // Unique block identifier

  // Bytecode instructions belonging to this block
  std::vector<bytecode::Instruction> instructions;

  // Control-flow relationships
  std::vector<std::int32_t> predecessors;
  std::vector<std::int32_t> successors;

  // Loop-related metadata
  bool isLoopHeader = false;
  bool isLoopExit = false;

  // Data-flow analysis sets

  std::unordered_set<std::int32_t> liveIn;   // Live variables at block entry
  std::unordered_set<std::int32_t> liveOut;  // Live variables at block exit
  std::unordered_set<std::int32_t> defsIn;   // Variables defined in this block
  std::unordered_set<std::int32_t> usesIn;   // Variables used in this block
};

/**
 * @brief Compiles AST nodes into bytecode instructions.
 *
 * The BytecodeCompiler performs:
 *  - AST traversal
 *  - Symbol resolution
 *  - Control-flow construction
 *  - Jump patching
 *  - Loop analysis
 *  - Peephole optimizations
 *  - Stack depth tracking
 */
class BytecodeCompiler
{
 public:
  /**
   * @brief Result of a compilation pass.
   *
   * A CompilationUnit contains executable bytecode along with all metadata
   * required by the virtual machine.
   */
  struct CompilationUnit
  {
    std::vector<bytecode::Instruction> instructions;  // Bytecode instruction stream
    std::vector<object::Value> constants;             // Constant pool values
    std::vector<std::string> names;                   // Variable and symbol names

    std::int32_t numLocals;    // Number of local variables
    std::int32_t numCellVars;  // Number of closure variables
    std::int32_t stackSize;    // Maximum operand stack depth

    // Metadata and debug information

    std::string filename;                   // Source filename
    std::vector<std::int32_t> lineNumbers;  // PC -> source line mapping

    // Program counter to source snippet mapping (debugging)
    std::unordered_map<std::int32_t, std::string> pcToSourceMap;
  };

 private:
  // Compilation state

  CompilationUnit unit_;        // Output compilation unit
  ConstantPool constants_;      // Constant pool manager
  JumpResolver jumps_;          // Jump target resolution
  PeepholeOptimizer peephole_;  // Peephole optimization pass
  LoopAnalyzer loopAnalyzer_;   // Loop detection and analysis

  // Symbol table management
  std::unique_ptr<CompilerSymbolTable> currentScope_;
  std::stack<CompilerSymbolTable*> scopeStack_;

  // Control-flow graph blocks
  std::vector<BytecodeBlock> blocks_;
  std::int32_t currentBlock_{0};

  // Stack depth tracking for optimization

  std::int32_t currentStackDepth_{0};
  std::int32_t maxStackDepth_{0};

  // Loop context management

  struct LoopContext
  {
    std::int32_t breakLabel;     // Jump target for break statements
    std::int32_t continueLabel;  // Jump target for continue statements
    std::int32_t startPC;        // Program counter at loop start
  };

  std::stack<LoopContext> loopStack_;

  // Compilation statistics

  struct Stats
  {
    std::int32_t instructionsGenerated = 0;
    std::int32_t constantsPoolSize = 0;
    std::int32_t jumpsResolved = 0;
    std::int32_t peepholeOptimizations = 0;
    std::int32_t loopsDetected = 0;
  } stats_;

  // Internal helpers

  /// @brief Emits a bytecode instruction
  void emit(bytecode::OpCode op, std::int32_t arg = 0, std::int32_t line = 0);
  /// @brief Updates the current and maximum stack depth
  void updateStackDepth(bytecode::OpCode op);
  /// @brief Returns the current program counter
  std::int32_t getCurrentPC() const;
  /// @brief Patches a previously emitted jump instruction
  void patchJump(std::int32_t jumpIndex);
  /// @brief Enters a new lexical scope
  void enterScope();
  /// @brief Exits the current lexical scope
  void exitScope();
  /// @brief Compiles an expression AST node
  void compileExpr(const parser::ast::Expr* expr);
  /// @brief Compiles a statement AST node
  void compileStmt(const parser::ast::Stmt* stmt);

 public:
  /**
   * @brief Constructs a bytecode compiler.
   *
   * Initializes the root symbol table.
   */
  BytecodeCompiler() { currentScope_ = std::make_unique<CompilerSymbolTable>(); }

  /**
   * @brief Compiles an AST into a bytecode compilation unit.
   *
   * @param ast Vector of statement AST nodes
   * @return Fully populated CompilationUnit
   */
  CompilationUnit compile(const std::vector<parser::ast::Stmt*>& ast);

  /// @brief Disassembles bytecode into a human-readable format
  void disassemble(const CompilationUnit& unit, std::ostream& out) const;
  /// @brief Prints an optimization and compilation report
  void optimizationReport(std::ostream& out) const;

 private:
  /// @brief Converts an opcode to a string representation
  std::string opcodeToString(bytecode::OpCode op) const;
  /// @brief Returns whether an opcode requires an argument
  bool needsArg(bytecode::OpCode op) const;
};  // BytecodeCompiler

}  // namespace runtime
}  // namespace mylang