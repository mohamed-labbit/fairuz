#pragma once

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


// ============================================================================
// ADVANCED BYTECODE COMPILER
// ============================================================================

namespace mylang {
namespace runtime {

// Basic block for optimization
struct BytecodeBlock
{
  std::int32_t id;
  std::vector<bytecode::Instruction> instructions;
  std::vector<std::int32_t> predecessors;
  std::vector<std::int32_t> successors;
  bool isLoopHeader = false;
  bool isLoopExit = false;
  // Data flow analysis
  std::unordered_set<std::int32_t> liveIn;
  std::unordered_set<std::int32_t> liveOut;
  std::unordered_set<std::int32_t> defsIn;
  std::unordered_set<std::int32_t> usesIn;
};

class BytecodeCompiler
{
 public:
  struct CompilationUnit
  {
    std::vector<bytecode::Instruction> instructions;
    std::vector<object::Value> constants;
    std::vector<std::string> names;  // Variable names
    std::int32_t numLocals;
    std::int32_t numCellVars;  // Closure variables
    std::int32_t stackSize;    // Maximum stack depth
    // Metadata
    std::string filename;
    std::vector<std::int32_t> lineNumbers;  // PC -> line number mapping
    // Debug info
    std::unordered_map<std::int32_t, std::string> pcToSourceMap;
  };

 private:
  CompilationUnit unit_;
  ConstantPool constants_;
  JumpResolver jumps_;
  PeepholeOptimizer peephole_;
  LoopAnalyzer loopAnalyzer_;
  std::unique_ptr<CompilerSymbolTable> currentScope_;
  std::stack<CompilerSymbolTable*> scopeStack_;
  std::vector<BytecodeBlock> blocks_;
  std::int32_t currentBlock_{0};
  // Stack depth tracking for optimization
  std::int32_t currentStackDepth_{0};
  std::int32_t maxStackDepth_{0};

  // Loop context
  struct LoopContext
  {
    std::int32_t breakLabel;
    std::int32_t continueLabel;
    std::int32_t startPC;
  };
  std::stack<LoopContext> loopStack_;

  // Statistics
  struct Stats
  {
    std::int32_t instructionsGenerated = 0;
    std::int32_t constantsPoolSize = 0;
    std::int32_t jumpsResolved = 0;
    std::int32_t peepholeOptimizations = 0;
    std::int32_t loopsDetected = 0;
  } stats_;

  void emit(bytecode::OpCode op, std::int32_t arg = 0, std::int32_t line = 0);
  void updateStackDepth(bytecode::OpCode op);
  std::int32_t getCurrentPC() const;
  void patchJump(std::int32_t jumpIndex);
  void enterScope();
  void exitScope();
  void compileExpr(const parser::ast::Expr* expr);
  void compileStmt(const parser::ast::Stmt* stmt);

 public:
  BytecodeCompiler() { currentScope_ = std::make_unique<CompilerSymbolTable>(); }

  CompilationUnit compile(const std::vector<parser::ast::StmtPtr>& ast);
  void disassemble(const CompilationUnit& unit, std::ostream& out) const;
  void optimizationReport(std::ostream& out) const;

 private:
  std::string opcodeToString(bytecode::OpCode op) const;
  bool needsArg(bytecode::OpCode op) const;
};  // BytecodeCompiler

}
}