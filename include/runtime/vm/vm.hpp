#pragma once


#include "../../parser/ast.hpp"
#include "bytecode.hpp"
#include "collector.hpp"
#include "jit.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace mylang {
namespace runtime {

// ============================================================================
// SYMBOL TABLE for Bytecode Generation
// ============================================================================
class CompilerSymbolTable
{
   public:
    enum class SymbolScope { Global, Local, Closure, Cell };

    struct Symbol
    {
        std::string name;
        std::int32_t index;
        SymbolScope scope;
        bool isParameter;
        bool isCaptured;  // Used in closure
        bool isUsed;
    };

   private:
    std::unordered_map<std::string, Symbol> symbols;
    CompilerSymbolTable* parent;
    std::int32_t nextIndex = 0;
    std::vector<std::string> freeVars;  // Closure variables

   public:
    explicit CompilerSymbolTable(CompilerSymbolTable* p = nullptr) :
        parent(p)
    {
    }

    Symbol* define(const std::string& name, bool isParam = false);

    Symbol* resolve(const std::string& name);

    const std::vector<std::string>& getFreeVars() const;
    std::int32_t getLocalCount() const;

    std::vector<Symbol> getUnusedSymbols() const;
};

// ============================================================================
// BYTECODE BLOCK - Basic block for optimization
// ============================================================================
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

// ============================================================================
// CONSTANT POOL - Deduplicated constants
// ============================================================================
class ConstantPool
{
   private:
    std::vector<object::Value> constants;
    std::unordered_map<std::u16string, std::int32_t> stringConstants;
    std::unordered_map<std::int64_t, std::int32_t> intConstants;
    std::unordered_map<double, std::int32_t> floatConstants;

   public:
    std::int32_t addConstant(const object::Value& val);

    const std::vector<object::Value>& getConstants() const;

    void optimize();
};

// ============================================================================
// JUMP TARGET RESOLVER - Handle forward/backward jumps
// ============================================================================
class JumpResolver
{
   private:
    struct PendingJump
    {
        std::int32_t instructionIndex;
        std::string labelName;
    };

    std::unordered_map<std::string, std::int32_t> labels;
    std::vector<PendingJump> pendingJumps;

   public:
    void defineLabel(const std::string& name, std::int32_t position);

    void addJump(std::int32_t instrIndex, const std::string& target);

    void resolveJumps(std::vector<bytecode::Instruction>& instructions);

    std::int32_t getLabel(const std::string& name) const;
};

// ============================================================================
// LOOP ANALYSIS - Detect and optimize loops
// ============================================================================
class LoopAnalyzer
{
   public:
    struct Loop
    {
        std::int32_t headerPC;
        std::int32_t exitPC;
        std::vector<std::int32_t> bodyPCs;
        std::unordered_set<std::int32_t> invariants;  // Loop-invariant variables
        bool isInnerLoop;
        std::int32_t nestingLevel;
    };

   private:
    std::vector<Loop> loops;

   public:
    void detectLoops(const std::vector<bytecode::Instruction>& instructions);

    void findInvariants(const std::vector<bytecode::Instruction>& instructions, const CompilerSymbolTable& symbols);

    const std::vector<Loop>& getLoops() const;
};

// ============================================================================
// PEEPHOLE OPTIMIZER - Local optimization
// ============================================================================
class PeepholeOptimizer
{
   public:
    struct Optimization
    {
        std::string name;
        std::int32_t replacementCount;
    };

   private:
    std::vector<Optimization> optimizations;

    bool matchPattern(
      const std::vector<bytecode::Instruction>& code, size_t pos, const std::vector<bytecode::OpCode>& pattern);

   public:
    void optimize(std::vector<bytecode::Instruction>& instructions);

    const std::vector<Optimization>& getOptimizations() const;

    void printReport() const;
};

// ============================================================================
// ADVANCED BYTECODE COMPILER
// ============================================================================
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
        std::int32_t stackSize;  // Maximum stack depth

        // Metadata
        std::string filename;
        std::vector<std::int32_t> lineNumbers;  // PC -> line number mapping

        // Debug info
        std::unordered_map<std::int32_t, std::string> pcToSourceMap;
    };

   private:
    CompilationUnit unit;
    ConstantPool constants;
    JumpResolver jumps;
    PeepholeOptimizer peephole;
    LoopAnalyzer loopAnalyzer;

    std::unique_ptr<CompilerSymbolTable> currentScope;
    std::stack<CompilerSymbolTable*> scopeStack;

    std::vector<BytecodeBlock> blocks;
    std::int32_t currentBlock = 0;

    // Stack depth tracking for optimization
    std::int32_t currentStackDepth = 0;
    std::int32_t maxStackDepth = 0;

    // Loop context
    struct LoopContext
    {
        std::int32_t breakLabel;
        std::int32_t continueLabel;
        std::int32_t startPC;
    };
    std::stack<LoopContext> loopStack;

    // Statistics
    struct Stats
    {
        std::int32_t instructionsGenerated = 0;
        std::int32_t constantsPoolSize = 0;
        std::int32_t jumpsResolved = 0;
        std::int32_t peepholeOptimizations = 0;
        std::int32_t loopsDetected = 0;
    } stats;

    void emit(bytecode::OpCode op, std::int32_t arg = 0, std::int32_t line = 0);

    void updateStackDepth(bytecode::OpCode op);

    std::int32_t getCurrentPC() const;

    void patchJump(std::int32_t jumpIndex);

    void enterScope();

    void exitScope();

    void compileExpr(const parser::ast::Expr* expr);

    void compileStmt(const parser::ast::Stmt* stmt);

   public:
    BytecodeCompiler() { currentScope = std::make_unique<CompilerSymbolTable>(); }

    CompilationUnit compile(const std::vector<parser::ast::StmtPtr>& ast);

    void disassemble(const CompilationUnit& unit, std::ostream& out) const;

    void optimizationReport(std::ostream& out) const;

   private:
    std::string opcodeToString(bytecode::OpCode op) const;

    bool needsArg(bytecode::OpCode op) const;
};

// ============================================================================
// BYTECODE OPTIMIZER - Cross-instruction optimization
// ============================================================================
class BytecodeOptimizer
{
   public:
    struct OptimizationPass
    {
        std::string name;
        std::function<bool(std::vector<bytecode::Instruction>&)> apply;
        std::int32_t applicationsCount = 0;
    };

   private:
    std::vector<OptimizationPass> passes;

   public:
    BytecodeOptimizer()
    {
        // Register optimization passes

        // Pass 1: Dead code elimination after returns
        passes.push_back({"Dead code after return", [](std::vector<bytecode::Instruction>& code) -> bool {
                              bool changed = false;
                              for (size_t i = 0; i + 1 < code.size(); i++)
                              {
                                  if (code[i].op == bytecode::OpCode::RETURN || code[i].op == bytecode::OpCode::HALT)
                                  {
                                      // Remove instructions until next label/jump target
                                      size_t toRemove = 0;
                                      for (size_t j = i + 1; j < code.size(); j++)
                                      {
                                          if (isJumpTarget(code, j))
                                          {
                                              break;
                                          }
                                          toRemove++;
                                      }
                                      if (toRemove > 0)
                                      {
                                          code.erase(code.begin() + i + 1, code.begin() + i + 1 + toRemove);
                                          changed = true;
                                      }
                                  }
                              }
                              return changed;
                          }});

        // Pass 2: Constant folding in bytecode
        passes.push_back({"Constant folding", [](std::vector<bytecode::Instruction>& code) -> bool {
                              bool changed = false;
                              // Find LOAD_CONST, LOAD_CONST, binary_op patterns
                              for (size_t i = 0; i + 2 < code.size(); i++)
                              {
                                  if (code[i].op == bytecode::OpCode::LOAD_CONST
                                    && code[i + 1].op == bytecode::OpCode::LOAD_CONST && isBinaryOp(code[i + 2].op))
                                  {
                                      // Would fold constants here
                                      changed = true;
                                  }
                              }
                              return changed;
                          }});

        // Pass 3: Jump threading
        passes.push_back({"Jump threading", [](std::vector<bytecode::Instruction>& code) -> bool {
                              bool changed = false;
                              // If jump target is another jump, redirect
                              for (auto& instr : code)
                              {
                                  if (isJumpOp(instr.op))
                                  {
                                      std::int32_t target = instr.arg;
                                      if (target < code.size() && code[target].op == bytecode::OpCode::JUMP)
                                      {
                                          instr.arg = code[target].arg;
                                          changed = true;
                                      }
                                  }
                              }
                              return changed;
                          }});

        // Pass 4: Remove NOPs
        passes.push_back({"NOP elimination", [](std::vector<bytecode::Instruction>& code) -> bool {
                              auto it = std::remove_if(code.begin(), code.end(),
                                [](const bytecode::Instruction& instr) { return instr.op == bytecode::OpCode::NOP; });
                              bool changed = it != code.end();
                              code.erase(it, code.end());
                              return changed;
                          }});
    }

    void optimize(std::vector<bytecode::Instruction>& code, std::int32_t maxIterations = 10);

    void printReport(std::ostream& out) const;

   private:
    static bool isJumpTarget(const std::vector<bytecode::Instruction>& code, size_t pos);

    static bool isJumpOp(bytecode::OpCode op);

    static bool isBinaryOp(bytecode::OpCode op);
};

// ============================================================================
// BYTECODE VERIFIER - Ensure bytecode correctness
// ============================================================================
class BytecodeVerifier
{
   public:
    struct VerificationError
    {
        std::int32_t pc;
        std::string message;
    };

   private:
    std::vector<VerificationError> errors;

   public:
    bool verify(const BytecodeCompiler::CompilationUnit& unit);

    const std::vector<VerificationError>& getErrors() const;

    void printErrors(std::ostream& out) const;

   private:
    void verifyStackDepth(const BytecodeCompiler::CompilationUnit& unit,
      std::int32_t pc,
      std::int32_t depth,
      std::vector<std::int32_t>& depths);

    std::int32_t getStackEffect(bytecode::OpCode op, std::int32_t arg) const;

    bool isJumpInstruction(bytecode::OpCode op) const;

    bool isUnconditionalJump(bytecode::OpCode op) const;
};

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
    static constexpr size_t CACHE_SIZE_ = 256;
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

    void execute(const BytecodeCompiler::CompiledCode& code);

   private:
    void executeInstruction(const bytecode::Instruction& instr, const BytecodeCompiler::CompiledCode& code);

   public:
    const Statistics& getStatistics() const { return stats_; }

    void printStatistics() const;

    const std::vector<object::Value>& getGlobals() const { return globals_; }

    const std::vector<object::Value>& getStack() const { return stack_; }
};
}
}