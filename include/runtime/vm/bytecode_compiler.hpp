#pragma once

#include "../../parser/ast.hpp"
#include "vm.hpp"

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
        int index;
        SymbolScope scope;
        bool isParameter;
        bool isCaptured;  // Used in closure
        bool isUsed;
    };

   private:
    std::unordered_map<std::string, Symbol> symbols;
    CompilerSymbolTable* parent;
    int nextIndex = 0;
    std::vector<std::string> freeVars;  // Closure variables

   public:
    explicit CompilerSymbolTable(CompilerSymbolTable* p = nullptr) :
        parent(p)
    {
    }

    Symbol* define(const std::string& name, bool isParam = false)
    {
        if (symbols.count(name))
        {
            return &symbols[name];
        }

        Symbol sym;
        sym.name = name;
        sym.index = nextIndex++;
        sym.scope = parent ? SymbolScope::Local : SymbolScope::Global;
        sym.isParameter = isParam;
        sym.isCaptured = false;
        sym.isUsed = false;

        symbols[name] = sym;
        return &symbols[name];
    }

    Symbol* resolve(const std::string& name)
    {
        // Check local scope
        auto it = symbols.find(name);
        if (it != symbols.end())
        {
            it->second.isUsed = true;
            return &it->second;
        }

        // Check parent scopes
        if (parent)
        {
            Symbol* parentSym = parent->resolve(name);
            if (parentSym)
            {
                // Mark as captured for closure
                parentSym->isCaptured = true;

                // Create closure reference
                Symbol closureSym;
                closureSym.name = name;
                closureSym.index = freeVars.size();
                closureSym.scope = SymbolScope::Closure;
                closureSym.isUsed = true;

                freeVars.push_back(name);
                symbols[name] = closureSym;
                return &symbols[name];
            }
        }

        return nullptr;
    }

    const std::vector<std::string>& getFreeVars() const { return freeVars; }
    int getLocalCount() const { return nextIndex; }

    std::vector<Symbol> getUnusedSymbols() const
    {
        std::vector<Symbol> unused;
        for (const auto& [name, sym] : symbols)
        {
            if (!sym.isUsed && !sym.isParameter)
            {
                unused.push_back(sym);
            }
        }
        return unused;
    }
};

// ============================================================================
// BYTECODE BLOCK - Basic block for optimization
// ============================================================================
struct BytecodeBlock
{
    int id;
    std::vector<bytecode::Instruction> instructions;
    std::vector<int> predecessors;
    std::vector<int> successors;
    bool isLoopHeader = false;
    bool isLoopExit = false;

    // Data flow analysis
    std::unordered_set<int> liveIn;
    std::unordered_set<int> liveOut;
    std::unordered_set<int> defsIn;
    std::unordered_set<int> usesIn;
};

// ============================================================================
// CONSTANT POOL - Deduplicated constants
// ============================================================================
class ConstantPool
{
   private:
    std::vector<object::Value> constants;
    std::unordered_map<std::u16string, int> stringConstants;
    std::unordered_map<long long, int> intConstants;
    std::unordered_map<double, int> floatConstants;

   public:
    int addConstant(const object::Value& val)
    {
        if (val.isInt())
        {
            long long v = val.asInt();
            auto it = intConstants.find(v);
            if (it != intConstants.end())
            {
                return it->second;
            }

            int idx = constants.size();
            constants.push_back(val);
            intConstants[v] = idx;
            return idx;
        }

        if (val.isFloat())
        {
            double v = val.asFloat();
            auto it = floatConstants.find(v);
            if (it != floatConstants.end())
            {
                return it->second;
            }

            int idx = constants.size();
            constants.push_back(val);
            floatConstants[v] = idx;
            return idx;
        }

        if (val.isString())
        {
            const std::u16string& v = val.asString();
            auto it = stringConstants.find(v);
            if (it != stringConstants.end())
            {
                return it->second;
            }

            int idx = constants.size();
            constants.push_back(val);
            stringConstants[v] = idx;
            return idx;
        }

        // Other types - no deduplication
        int idx = constants.size();
        constants.push_back(val);
        return idx;
    }

    const std::vector<object::Value>& getConstants() const { return constants; }

    void optimize()
    {
        // Remove unused constants (requires usage tracking)
    }
};

// ============================================================================
// JUMP TARGET RESOLVER - Handle forward/backward jumps
// ============================================================================
class JumpResolver
{
   private:
    struct PendingJump
    {
        int instructionIndex;
        std::string labelName;
    };

    std::unordered_map<std::string, int> labels;
    std::vector<PendingJump> pendingJumps;

   public:
    void defineLabel(const std::string& name, int position) { labels[name] = position; }

    void addJump(int instrIndex, const std::string& target)
    {
        auto it = labels.find(target);
        if (it != labels.end())
        {
            // Label already defined - immediate resolution
            // (Would patch instruction here)
        }
        else
        {
            pendingJumps.push_back({instrIndex, target});
        }
    }

    void resolveJumps(std::vector<bytecode::Instruction>& instructions)
    {
        for (const auto& jump : pendingJumps)
        {
            auto it = labels.find(jump.labelName);
            if (it != labels.end())
            {
                instructions[jump.instructionIndex].arg = it->second;
            }
            else
            {
                throw std::runtime_error("Undefined label: " + jump.labelName);
            }
        }
    }

    int getLabel(const std::string& name) const
    {
        auto it = labels.find(name);
        return it != labels.end() ? it->second : -1;
    }
};

// ============================================================================
// LOOP ANALYSIS - Detect and optimize loops
// ============================================================================
class LoopAnalyzer
{
   public:
    struct Loop
    {
        int headerPC;
        int exitPC;
        std::vector<int> bodyPCs;
        std::unordered_set<int> invariants;  // Loop-invariant variables
        bool isInnerLoop;
        int nestingLevel;
    };

   private:
    std::vector<Loop> loops;

   public:
    void detectLoops(const std::vector<bytecode::Instruction>& instructions)
    {
        // Detect back-edges (jumps to earlier instructions)
        for (size_t i = 0; i < instructions.size(); i++)
        {
            const auto& instr = instructions[i];
            if ((instr.op == bytecode::OpCode::JUMP_BACKWARD || instr.op == bytecode::OpCode::FOR_ITER)
              && instr.arg < i)
            {

                Loop loop;
                loop.headerPC = instr.arg;
                loop.exitPC = i;
                loop.isInnerLoop = true;
                loop.nestingLevel = 1;

                // Collect loop body
                for (int pc = instr.arg; pc <= i; pc++)
                {
                    loop.bodyPCs.push_back(pc);
                }

                loops.push_back(loop);
            }
        }

        // Calculate nesting levels
        for (auto& outer : loops)
        {
            for (const auto& inner : loops)
            {
                if (inner.headerPC > outer.headerPC && inner.exitPC < outer.exitPC)
                {
                    outer.isInnerLoop = false;
                    // inner is nested in outer
                }
            }
        }
    }

    void findInvariants(const std::vector<bytecode::Instruction>& instructions, const CompilerSymbolTable& symbols)
    {
        for (auto& loop : loops)
        {
            // Variables that are:
            // 1. Defined outside loop
            // 2. Not modified inside loop
            // 3. Used inside loop
            // Can be hoisted out

            std::unordered_set<int> modifiedVars;
            std::unordered_set<int> usedVars;

            for (int pc : loop.bodyPCs)
            {
                const auto& instr = instructions[pc];
                if (instr.op == bytecode::OpCode::STORE_VAR || instr.op == bytecode::OpCode::STORE_FAST)
                {
                    modifiedVars.insert(instr.arg);
                }
                if (instr.op == bytecode::OpCode::LOAD_VAR || instr.op == bytecode::OpCode::LOAD_FAST)
                {
                    usedVars.insert(instr.arg);
                }
            }

            for (int var : usedVars)
            {
                if (!modifiedVars.count(var))
                {
                    loop.invariants.insert(var);
                }
            }
        }
    }

    const std::vector<Loop>& getLoops() const { return loops; }
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
        int replacementCount;
    };

   private:
    std::vector<Optimization> optimizations;

    bool matchPattern(
      const std::vector<bytecode::Instruction>& code, size_t pos, const std::vector<bytecode::OpCode>& pattern)
    {
        if (pos + pattern.size() > code.size())
        {
            return false;
        }

        for (size_t i = 0; i < pattern.size(); i++)
        {
            if (code[pos + i].op != pattern[i])
            {
                return false;
            }
        }
        return true;
    }

   public:
    void optimize(std::vector<bytecode::Instruction>& instructions)
    {
        int replacements = 0;

        // Pattern 1: LOAD_CONST followed by POP -> remove both
        for (size_t i = 0; i + 1 < instructions.size();)
        {
            if (instructions[i].op == bytecode::OpCode::LOAD_CONST && instructions[i + 1].op == bytecode::OpCode::POP)
            {
                instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
                replacements++;
                continue;
            }
            i++;
        }
        if (replacements > 0)
        {
            optimizations.push_back({"Const-Pop elimination", replacements});
        }

        // Pattern 2: LOAD x, STORE x -> remove (redundant load-store)
        replacements = 0;
        for (size_t i = 0; i + 1 < instructions.size();)
        {
            auto& instr1 = instructions[i];
            auto& instr2 = instructions[i + 1];

            if ((instr1.op == bytecode::OpCode::LOAD_VAR && instr2.op == bytecode::OpCode::STORE_VAR)
              || (instr1.op == bytecode::OpCode::LOAD_FAST && instr2.op == bytecode::OpCode::STORE_FAST))
            {
                if (instr1.arg == instr2.arg)
                {
                    instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
                    replacements++;
                    continue;
                }
            }
            i++;
        }
        if (replacements > 0)
        {
            optimizations.push_back({"Load-Store elimination", replacements});
        }

        // Pattern 3: DUP, POP -> remove both (useless dup)
        replacements = 0;
        for (size_t i = 0; i + 1 < instructions.size();)
        {
            if (instructions[i].op == bytecode::OpCode::DUP && instructions[i + 1].op == bytecode::OpCode::POP)
            {
                instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
                replacements++;
                continue;
            }
            i++;
        }
        if (replacements > 0)
        {
            optimizations.push_back({"Dup-Pop elimination", replacements});
        }

        // Pattern 4: JUMP to next instruction -> remove
        replacements = 0;
        for (size_t i = 0; i < instructions.size();)
        {
            if (instructions[i].op == bytecode::OpCode::JUMP && instructions[i].arg == i + 1)
            {
                instructions.erase(instructions.begin() + i);
                replacements++;
                continue;
            }
            i++;
        }
        if (replacements > 0)
        {
            optimizations.push_back({"Redundant jump elimination", replacements});
        }

        // Pattern 5: NOT, NOT -> remove both (double negation)
        replacements = 0;
        for (size_t i = 0; i + 1 < instructions.size();)
        {
            if (instructions[i].op == bytecode::OpCode::NOT && instructions[i + 1].op == bytecode::OpCode::NOT)
            {
                instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
                replacements++;
                continue;
            }
            i++;
        }
        if (replacements > 0)
        {
            optimizations.push_back({"Double negation elimination", replacements});
        }

        // Pattern 6: Use fast opcodes for common operations
        replacements = 0;
        for (auto& instr : instructions)
        {
            if (instr.op == bytecode::OpCode::ADD)
            {
                instr.op = bytecode::OpCode::ADD_FAST;
                replacements++;
            }
            else if (instr.op == bytecode::OpCode::SUB)
            {
                instr.op = bytecode::OpCode::SUB_FAST;
                replacements++;
            }
            else if (instr.op == bytecode::OpCode::MUL)
            {
                instr.op = bytecode::OpCode::MUL_FAST;
                replacements++;
            }
        }
        if (replacements > 0)
        {
            optimizations.push_back({"Fast opcode substitution", replacements});
        }
    }

    const std::vector<Optimization>& getOptimizations() const { return optimizations; }

    void printReport() const
    {
        if (optimizations.empty())
        {
            std::cout << "  No peephole optimizations applied\n";
            return;
        }

        std::cout << "  Peephole Optimizations:\n";
        for (const auto& opt : optimizations)
        {
            std::cout << "    • " << opt.name << ": " << opt.name << " replacements\n";
        }
    }
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
        int numLocals;
        int numCellVars;  // Closure variables
        int stackSize;  // Maximum stack depth

        // Metadata
        std::string filename;
        std::vector<int> lineNumbers;  // PC -> line number mapping

        // Debug info
        std::unordered_map<int, std::string> pcToSourceMap;
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
    int currentBlock = 0;

    // Stack depth tracking for optimization
    int currentStackDepth = 0;
    int maxStackDepth = 0;

    // Loop context
    struct LoopContext
    {
        int breakLabel;
        int continueLabel;
        int startPC;
    };
    std::stack<LoopContext> loopStack;

    // Statistics
    struct Stats
    {
        int instructionsGenerated = 0;
        int constantsPoolSize = 0;
        int jumpsResolved = 0;
        int peepholeOptimizations = 0;
        int loopsDetected = 0;
    } stats;

    void emit(bytecode::OpCode op, int arg = 0, int line = 0)
    {
        bytecode::Instruction instr(op, arg, line);
        unit.instructions.push_back(instr);
        stats.instructionsGenerated++;

        // Track stack depth
        updateStackDepth(op);

        // Track line numbers
        if (line > 0)
        {
            unit.lineNumbers.push_back(line);
        }
    }

    void updateStackDepth(bytecode::OpCode op)
    {
        // Update stack depth based on operation
        switch (op)
        {
        case bytecode::OpCode::LOAD_CONST :
        case bytecode::OpCode::LOAD_VAR :
        case bytecode::OpCode::LOAD_GLOBAL :
        case bytecode::OpCode::LOAD_FAST :
        case bytecode::OpCode::DUP :
            currentStackDepth++;
            break;

        case bytecode::OpCode::POP :
        case bytecode::OpCode::STORE_VAR :
        case bytecode::OpCode::STORE_GLOBAL :
        case bytecode::OpCode::STORE_FAST :
            currentStackDepth--;
            break;

        case bytecode::OpCode::ADD :
        case bytecode::OpCode::SUB :
        case bytecode::OpCode::MUL :
        case bytecode::OpCode::DIV :
        case bytecode::OpCode::MOD :
        case bytecode::OpCode::POW :
        case bytecode::OpCode::EQ :
        case bytecode::OpCode::NE :
        case bytecode::OpCode::LT :
        case bytecode::OpCode::GT :
        case bytecode::OpCode::LE :
        case bytecode::OpCode::GE :
        case bytecode::OpCode::AND :
        case bytecode::OpCode::OR :
            currentStackDepth--;  // Two operands -> one result
            break;

        case bytecode::OpCode::CALL :
            // Function + args -> result
            // Stack depth change handled separately
            break;

        default :
            break;
        }

        maxStackDepth = std::max(maxStackDepth, currentStackDepth);
    }

    int getCurrentPC() const { return unit.instructions.size(); }

    void patchJump(int jumpIndex) { unit.instructions[jumpIndex].arg = getCurrentPC(); }

    void enterScope()
    {
        auto* newScope = new CompilerSymbolTable(currentScope.get());
        if (currentScope)
        {
            scopeStack.push(currentScope.release());
        }
        currentScope.reset(newScope);
    }

    void exitScope()
    {
        if (!scopeStack.empty())
        {
            currentScope.reset(scopeStack.top());
            scopeStack.pop();
        }
    }

    void compileExpr(const parser::ast::Expr* expr)
    {
        if (!expr)
            return;

        switch (expr->kind)
        {
        case parser::ast::Expr::Kind::LITERAL : {
            auto* lit = static_cast<const parser::ast::LiteralExpr*>(expr);
            object::Value val;

            switch (lit->litType)
            {
            case parser::ast::LiteralExpr::Type::NUMBER : {
                if (lit->value.find('.') != std::string::npos)
                {
                    val = object::Value(std::stod(utf8::utf16to8(lit->value)));
                }
                else
                {
                    val = object::Value(std::stoll(utf8::utf16to8(lit->value)));
                }
                break;
            }
            case parser::ast::LiteralExpr::Type::STRING :
                val = object::Value(lit->value);
                break;
            case parser::ast::LiteralExpr::Type::BOOLEAN :
                val = object::Value(lit->value == u"true" || lit->value == u"صحيح");
                break;
            case parser::ast::LiteralExpr::Type::NONE :
                val = object::Value();
                break;
            }

            int idx = constants.addConstant(val);
            emit(bytecode::OpCode::LOAD_CONST, idx, expr->line);
            break;
        }

        case parser::ast::Expr::Kind::NAME : {
            auto* name = static_cast<const parser::ast::NameExpr*>(expr);
            auto* sym = currentScope->resolve(utf8::utf16to8(name->name));

            if (!sym)
            {
                throw std::runtime_error("Undefined variable: " + utf8::utf16to8(name->name));
            }

            switch (sym->scope)
            {
            case CompilerSymbolTable::SymbolScope::Global :
                emit(bytecode::OpCode::LOAD_GLOBAL, sym->index, expr->line);
                break;
            case CompilerSymbolTable::SymbolScope::Local :
                emit(bytecode::OpCode::LOAD_FAST, sym->index, expr->line);
                break;
            case CompilerSymbolTable::SymbolScope::Closure :
                emit(bytecode::OpCode::LOAD_CLOSURE, sym->index, expr->line);
                break;
            default :
                emit(bytecode::OpCode::LOAD_VAR, sym->index, expr->line);
                break;
            }
            break;
        }

        case parser::ast::Expr::Kind::BINARY : {
            auto* bin = static_cast<const parser::ast::BinaryExpr*>(expr);

            // Short-circuit evaluation for logical operators
            if (bin->op == u"و" || bin->op == u"and")
            {
                compileExpr(bin->left.get());
                emit(bytecode::OpCode::DUP, 0, expr->line);
                int jumpIfFalse = getCurrentPC();
                emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, expr->line);
                emit(bytecode::OpCode::POP, 0, expr->line);
                compileExpr(bin->right.get());
                patchJump(jumpIfFalse);
                break;
            }

            if (bin->op == u"او" || bin->op == u"or")
            {
                compileExpr(bin->left.get());
                emit(bytecode::OpCode::DUP, 0, expr->line);
                int jumpIfTrue = getCurrentPC();
                emit(bytecode::OpCode::POP_JUMP_IF_TRUE, 0, expr->line);
                emit(bytecode::OpCode::POP, 0, expr->line);
                compileExpr(bin->right.get());
                patchJump(jumpIfTrue);
                break;
            }

            // Regular binary operations
            compileExpr(bin->left.get());
            compileExpr(bin->right.get());

            if (bin->op == u"+")
            {
                emit(bytecode::OpCode::ADD, 0, expr->line);
            }
            else if (bin->op == u"-")
            {
                emit(bytecode::OpCode::SUB, 0, expr->line);
            }
            else if (bin->op == u"*")
            {
                emit(bytecode::OpCode::MUL, 0, expr->line);
            }
            else if (bin->op == u"/")
            {
                emit(bytecode::OpCode::DIV, 0, expr->line);
            }
            else if (bin->op == u"//")
            {
                emit(bytecode::OpCode::FLOOR_DIV, 0, expr->line);
            }
            else if (bin->op == u"%")
            {
                emit(bytecode::OpCode::MOD, 0, expr->line);
            }
            else if (bin->op == u"**")
            {
                emit(bytecode::OpCode::POW, 0, expr->line);
            }
            else if (bin->op == u"==")
            {
                emit(bytecode::OpCode::EQ, 0, expr->line);
            }
            else if (bin->op == u"!=")
            {
                emit(bytecode::OpCode::NE, 0, expr->line);
            }
            else if (bin->op == u"<")
            {
                emit(bytecode::OpCode::LT, 0, expr->line);
            }
            else if (bin->op == u">")
            {
                emit(bytecode::OpCode::GT, 0, expr->line);
            }
            else if (bin->op == u"<=")
            {
                emit(bytecode::OpCode::LE, 0, expr->line);
            }
            else if (bin->op == u">=")
            {
                emit(bytecode::OpCode::GE, 0, expr->line);
            }
            else if (bin->op == u"&")
            {
                emit(bytecode::OpCode::BITAND, 0, expr->line);
            }
            else if (bin->op == u"|")
            {
                emit(bytecode::OpCode::BITOR, 0, expr->line);
            }
            else if (bin->op == u"^")
            {
                emit(bytecode::OpCode::BITXOR, 0, expr->line);
            }
            else if (bin->op == u"<<")
            {
                emit(bytecode::OpCode::LSHIFT, 0, expr->line);
            }
            else if (bin->op == u">>")
            {
                emit(bytecode::OpCode::RSHIFT, 0, expr->line);
            }
            else if (bin->op == u"في" || bin->op == u"in")
            {
                emit(bytecode::OpCode::IN, 0, expr->line);
            }
            break;
        }

        case parser::ast::Expr::Kind::UNARY : {
            auto* un = static_cast<const parser::ast::UnaryExpr*>(expr);
            compileExpr(un->operand.get());

            if (un->op == u"-")
            {
                emit(bytecode::OpCode::NEG, 0, expr->line);
            }
            else if (un->op == u"+")
            {
                emit(bytecode::OpCode::POS, 0, expr->line);
            }
            else if (un->op == u"~")
            {
                emit(bytecode::OpCode::BITNOT, 0, expr->line);
            }
            else if (un->op == u"ليس" || un->op == u"not")
            {
                emit(bytecode::OpCode::NOT, 0, expr->line);
            }
            break;
        }

        case parser::ast::Expr::Kind::CALL : {
            auto* call = static_cast<const parser::ast::CallExpr*>(expr);

            // Compile arguments first
            for (const auto& arg : call->args)
            {
                compileExpr(arg.get());
            }

            // Compile callee
            compileExpr(call->callee.get());

            // Emit call instruction
            emit(bytecode::OpCode::CALL, call->args.size(), expr->line);

            // Adjust stack depth
            currentStackDepth -= call->args.size();
            break;
        }

        case parser::ast::Expr::Kind::LIST : {
            auto* list = static_cast<const parser::ast::ListExpr*>(expr);

            // Compile all elements
            for (const auto& elem : list->elements)
            {
                compileExpr(elem.get());
            }

            // Build list from stack
            emit(bytecode::OpCode::BUILD_LIST, list->elements.size(), expr->line);

            // Adjust stack depth
            currentStackDepth -= list->elements.size();
            currentStackDepth++;
            break;
        }

        case parser::ast::Expr::Kind::TERNARY : {
            auto* tern = static_cast<const parser::ast::TernaryExpr*>(expr);

            // Compile condition
            compileExpr(tern->condition.get());

            int jumpIfFalse = getCurrentPC();
            emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, expr->line);

            // True branch
            compileExpr(tern->trueExpr.get());
            int jumpEnd = getCurrentPC();
            emit(bytecode::OpCode::JUMP, 0, expr->line);

            // False branch
            patchJump(jumpIfFalse);
            if (tern->falseExpr)
            {
                compileExpr(tern->falseExpr.get());
            }

            patchJump(jumpEnd);
            break;
        }

        case parser::ast::Expr::Kind::ASSIGNMENT : {
            auto* assign = static_cast<const parser::ast::AssignmentExpr*>(expr);

            // Compile value
            compileExpr(assign->value.get());

            // Duplicate for expression result
            emit(bytecode::OpCode::DUP, 0, expr->line);

            // Store
            auto* sym = currentScope->define(utf8::utf16to8(assign->target));
            if (sym->scope == CompilerSymbolTable::SymbolScope::Global)
            {
                emit(bytecode::OpCode::STORE_GLOBAL, sym->index, expr->line);
            }
            else
            {
                emit(bytecode::OpCode::STORE_FAST, sym->index, expr->line);
            }
            break;
        }

        default :
            throw std::runtime_error("Unsupported expression type");
        }
    }

    void compileStmt(const parser::ast::Stmt* stmt)
    {
        if (!stmt)
        {
            return;
        }

        switch (stmt->kind)
        {
        case parser::ast::Stmt::Kind::ASSIGNMENT : {
            auto* assign = static_cast<const parser::ast::AssignmentStmt*>(stmt);

            // Compile value
            compileExpr(assign->value.get());

            // Store to variable
            auto* sym = currentScope->define(utf8::utf16to8(assign->target));

            if (sym->scope == CompilerSymbolTable::SymbolScope::Global)
            {
                emit(bytecode::OpCode::STORE_GLOBAL, sym->index, stmt->line);
            }
            else
            {
                emit(bytecode::OpCode::STORE_FAST, sym->index, stmt->line);
            }
            break;
        }

        case parser::ast::Stmt::Kind::EXPRESSION : {
            auto* exprStmt = static_cast<const parser::ast::ExprStmt*>(stmt);
            compileExpr(exprStmt->expression.get());
            emit(bytecode::OpCode::POP, 0, stmt->line);  // Discard result
            break;
        }

        case parser::ast::Stmt::Kind::IF : {
            auto* ifStmt = static_cast<const parser::ast::IfStmt*>(stmt);

            // Compile condition
            compileExpr(ifStmt->condition.get());

            int jumpIfFalse = getCurrentPC();
            emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, stmt->line);

            // Then block
            for (const auto& s : ifStmt->thenBlock)
            {
                compileStmt(s.get());
            }

            if (!ifStmt->elseBlock.empty())
            {
                int jumpEnd = getCurrentPC();
                emit(bytecode::OpCode::JUMP, 0, stmt->line);

                patchJump(jumpIfFalse);

                // Else block
                for (const auto& s : ifStmt->elseBlock)
                {
                    compileStmt(s.get());
                }

                patchJump(jumpEnd);
            }
            else
            {
                patchJump(jumpIfFalse);
            }
            break;
        }

        case parser::ast::Stmt::Kind::WHILE : {
            auto* whileStmt = static_cast<const parser::ast::WhileStmt*>(stmt);

            int loopStart = getCurrentPC();

            // Mark as potential hot loop
            emit(bytecode::OpCode::HOT_LOOP_START, 0, stmt->line);

            // Compile condition
            compileExpr(whileStmt->condition.get());

            int jumpIfFalse = getCurrentPC();
            emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, stmt->line);

            // Loop body
            LoopContext ctx;
            ctx.startPC = loopStart;
            ctx.continueLabel = loopStart;
            ctx.breakLabel = -1;  // Will be patched
            loopStack.push(ctx);

            for (const auto& s : whileStmt->body)
            {
                compileStmt(s.get());
            }

            // Jump back to loop start
            emit(bytecode::OpCode::JUMP_BACKWARD, loopStart, stmt->line);

            patchJump(jumpIfFalse);
            emit(bytecode::OpCode::HOT_LOOP_END, 0, stmt->line);

            // Patch break statements
            if (loopStack.top().breakLabel != -1)
            {
                patchJump(loopStack.top().breakLabel);
            }
            loopStack.pop();

            stats.loopsDetected++;
            break;
        }

        case parser::ast::Stmt::Kind::FOR : {
            auto* forStmt = static_cast<const parser::ast::ForStmt*>(stmt);

            // Compile iterator
            compileExpr(forStmt->iter.get());
            emit(bytecode::OpCode::GET_ITER, 0, stmt->line);

            int loopStart = getCurrentPC();
            emit(bytecode::OpCode::HOT_LOOP_START, 0, stmt->line);

            // FOR_ITER gets next item or jumps to end
            int forIter = getCurrentPC();
            emit(bytecode::OpCode::FOR_ITER_FAST, 0, stmt->line);

            // Store loop variable
            auto* sym = currentScope->define(utf8::utf16to8(forStmt->target));
            emit(bytecode::OpCode::STORE_FAST, sym->index, stmt->line);

            // Loop body
            LoopContext ctx;
            ctx.startPC = loopStart;
            ctx.continueLabel = loopStart;
            ctx.breakLabel = -1;
            loopStack.push(ctx);

            for (const auto& s : forStmt->body)
            {
                compileStmt(s.get());
            }

            // Jump back
            emit(bytecode::OpCode::JUMP_BACKWARD, loopStart, stmt->line);

            // Patch FOR_ITER to jump here when exhausted
            patchJump(forIter);
            emit(bytecode::OpCode::HOT_LOOP_END, 0, stmt->line);

            if (loopStack.top().breakLabel != -1)
            {
                patchJump(loopStack.top().breakLabel);
            }
            loopStack.pop();

            stats.loopsDetected++;
            break;
        }

        case parser::ast::Stmt::Kind::FUNCTION_DEF : {
            auto* funcDef = static_cast<const parser::ast::FunctionDef*>(stmt);

            // Enter new scope for function
            enterScope();

            // Define parameters
            for (const auto& param : funcDef->params)
            {
                currentScope->define(utf8::utf16to8(param), true);
            }

            // Save current compilation state
            auto savedInstructions = std::move(unit.instructions);
            auto savedConstants = constants;
            int savedStackDepth = currentStackDepth;
            int savedMaxStackDepth = maxStackDepth;

            unit.instructions.clear();
            currentStackDepth = 0;
            maxStackDepth = 0;

            // Compile function body
            for (const auto& s : funcDef->body)
            {
                compileStmt(s.get());
            }

            // Implicit return None if no return statement
            if (unit.instructions.empty() || unit.instructions.back().op != bytecode::OpCode::RETURN)
            {
                int noneIdx = constants.addConstant(object::Value());
                emit(bytecode::OpCode::LOAD_CONST, noneIdx, stmt->line);
                emit(bytecode::OpCode::RETURN, 0, stmt->line);
            }

            // Create function object
            auto funcInstructions = std::move(unit.instructions);
            int funcStackSize = maxStackDepth;

            // Restore compilation state
            unit.instructions = std::move(savedInstructions);
            constants = savedConstants;
            currentStackDepth = savedStackDepth;
            maxStackDepth = savedMaxStackDepth;

            // Store function object as constant
            object::Value funcObj;  // Would create FunctionObject here
            int funcIdx = constants.addConstant(funcObj);

            emit(bytecode::OpCode::LOAD_CONST, funcIdx, stmt->line);
            emit(bytecode::OpCode::MAKE_FUNCTION, funcDef->params.size(), stmt->line);

            // Store function
            auto* sym = currentScope->define(utf8::utf16to8(funcDef->name));
            emit(bytecode::OpCode::STORE_FAST, sym->index, stmt->line);

            exitScope();
            break;
        }

        case parser::ast::Stmt::Kind::RETURN : {
            auto* ret = static_cast<const parser::ast::ReturnStmt*>(stmt);

            if (ret->value)
            {
                compileExpr(ret->value.get());
            }
            else
            {
                int noneIdx = constants.addConstant(object::Value());
                emit(bytecode::OpCode::LOAD_CONST, noneIdx, stmt->line);
            }

            emit(bytecode::OpCode::RETURN, 0, stmt->line);
            break;
        }

        case parser::ast::Stmt::Kind::BLOCK : {
            auto* block = static_cast<const parser::ast::BlockStmt*>(stmt);
            for (const auto& s : block->statements)
            {
                compileStmt(s.get());
            }
            break;
        }

        default :
            throw std::runtime_error("Unsupported statement type");
        }
    }

   public:
    BytecodeCompiler() { currentScope = std::make_unique<CompilerSymbolTable>(); }

    CompilationUnit compile(const std::vector<parser::ast::StmtPtr>& ast)
    {
        // Reset state
        unit = CompilationUnit();
        stats = Stats();
        currentStackDepth = 0;
        maxStackDepth = 0;

        // Compile all statements
        for (const auto& stmt : ast)
        {
            compileStmt(stmt.get());
        }

        // Add HALT at end
        emit(bytecode::OpCode::HALT, 0, 0);

        // Finalize constant pool
        unit.constants = constants.getConstants();
        stats.constantsPoolSize = unit.constants.size();

        // Resolve all jumps
        jumps.resolveJumps(unit.instructions);

        // Detect loops for optimization
        loopAnalyzer.detectLoops(unit.instructions);
        loopAnalyzer.findInvariants(unit.instructions, *currentScope);

        // Apply peephole optimizations
        peephole.optimize(unit.instructions);
        stats.peepholeOptimizations = peephole.getOptimizations().size();

        // Set metadata
        unit.numLocals = currentScope->getLocalCount();
        unit.stackSize = maxStackDepth;

        // Report unused variables
        auto unused = currentScope->getUnusedSymbols();
        if (!unused.empty())
        {
            std::cout << "[Compiler] Warning: " << unused.size() << " unused variables detected\n";
        }

        return unit;
    }

    void disassemble(const CompilationUnit& unit, std::ostream& out) const
    {
        out << "=== Bytecode Disassembly ===\n\n";
        out << "Constants Pool (" << unit.constants.size() << " entries):\n";
        for (size_t i = 0; i < unit.constants.size(); i++)
        {
            out << "  [" << i << "] " << unit.constants[i].toString() << "\n";
        }

        out << "\nCode (" << unit.instructions.size() << " instructions):\n";
        out << "Stack size: " << unit.stackSize << "\n";
        out << "Locals: " << unit.numLocals << "\n\n";

        for (size_t i = 0; i < unit.instructions.size(); i++)
        {
            const auto& instr = unit.instructions[i];

            out << std::setw(6) << i << "  ";

            // Line number
            if (instr.lineNumber > 0)
            {
                out << std::setw(4) << instr.lineNumber << "  ";
            }
            else
            {
                out << "      ";
            }

            // Opcode name
            out << std::left << std::setw(20) << opcodeToString(instr.op);

            // Argument
            if (needsArg(instr.op))
            {
                out << std::setw(6) << instr.arg;

                // Show constant value for LOAD_CONST
                if (instr.op == bytecode::OpCode::LOAD_CONST && instr.arg < unit.constants.size())
                {
                    out << "  ; " << unit.constants[instr.arg].toString();
                }
            }

            out << "\n";
        }

        out << "\n=== Compilation Statistics ===\n";
        out << "Instructions generated: " << stats.instructionsGenerated << "\n";
        out << "Constants in pool: " << stats.constantsPoolSize << "\n";
        out << "Loops detected: " << stats.loopsDetected << "\n";
        out << "Peephole optimizations: " << stats.peepholeOptimizations << "\n";

        // Loop analysis
        const auto& loops = loopAnalyzer.getLoops();
        if (!loops.empty())
        {
            out << "\nLoop Analysis:\n";
            for (size_t i = 0; i < loops.size(); i++)
            {
                const auto& loop = loops[i];
                out << "  Loop " << i << ": PC " << loop.headerPC << " -> " << loop.exitPC
                    << " (nesting: " << loop.nestingLevel << ")\n";
                out << "    Invariants: " << loop.invariants.size() << " variables\n";
            }
        }

        // Peephole report
        out << "\n";
        peephole.printReport();
    }

    void optimizationReport(std::ostream& out) const
    {
        out << "\n╔═══════════════════════════════════════╗\n";
        out << "║    Bytecode Optimization Report       ║\n";
        out << "╚═══════════════════════════════════════╝\n\n";

        out << "Code Size:\n";
        out << "  Instructions: " << stats.instructionsGenerated << "\n";
        out << "  Constants: " << stats.constantsPoolSize << "\n";
        out << "  Stack size: " << maxStackDepth << "\n\n";

        out << "Loop Optimizations:\n";
        out << "  Loops detected: " << stats.loopsDetected << "\n";
        const auto& loops = loopAnalyzer.getLoops();
        int totalInvariants = 0;
        for (const auto& loop : loops)
        {
            totalInvariants += loop.invariants.size();
        }
        out << "  Hoistable invariants: " << totalInvariants << "\n\n";

        out << "Peephole Optimizations:\n";
        if (stats.peepholeOptimizations > 0)
        {
            peephole.printReport();
        }
        else
        {
            out << "  None applied\n";
        }
    }

   private:
    std::string opcodeToString(bytecode::OpCode op) const
    {
        switch (op)
        {
        case bytecode::OpCode::LOAD_CONST :
            return "LOAD_CONST";
        case bytecode::OpCode::LOAD_VAR :
            return "LOAD_VAR";
        case bytecode::OpCode::LOAD_GLOBAL :
            return "LOAD_GLOBAL";
        case bytecode::OpCode::LOAD_FAST :
            return "LOAD_FAST";
        case bytecode::OpCode::STORE_VAR :
            return "STORE_VAR";
        case bytecode::OpCode::STORE_GLOBAL :
            return "STORE_GLOBAL";
        case bytecode::OpCode::STORE_FAST :
            return "STORE_FAST";
        case bytecode::OpCode::POP :
            return "POP";
        case bytecode::OpCode::DUP :
            return "DUP";
        case bytecode::OpCode::SWAP :
            return "SWAP";
        case bytecode::OpCode::ROT_THREE :
            return "ROT_THREE";
        case bytecode::OpCode::ADD :
            return "ADD";
        case bytecode::OpCode::ADD_FAST :
            return "ADD_FAST";
        case bytecode::OpCode::SUB :
            return "SUB";
        case bytecode::OpCode::SUB_FAST :
            return "SUB_FAST";
        case bytecode::OpCode::MUL :
            return "MUL";
        case bytecode::OpCode::MUL_FAST :
            return "MUL_FAST";
        case bytecode::OpCode::DIV :
            return "DIV";
        case bytecode::OpCode::FLOOR_DIV :
            return "FLOOR_DIV";
        case bytecode::OpCode::MOD :
            return "MOD";
        case bytecode::OpCode::POW :
            return "POW";
        case bytecode::OpCode::NEG :
            return "NEG";
        case bytecode::OpCode::POS :
            return "POS";
        case bytecode::OpCode::BITAND :
            return "BITAND";
        case bytecode::OpCode::BITOR :
            return "BITOR";
        case bytecode::OpCode::BITXOR :
            return "BITXOR";
        case bytecode::OpCode::BITNOT :
            return "BITNOT";
        case bytecode::OpCode::LSHIFT :
            return "LSHIFT";
        case bytecode::OpCode::RSHIFT :
            return "RSHIFT";
        case bytecode::OpCode::EQ :
            return "EQ";
        case bytecode::OpCode::NE :
            return "NE";
        case bytecode::OpCode::LT :
            return "LT";
        case bytecode::OpCode::GT :
            return "GT";
        case bytecode::OpCode::LE :
            return "LE";
        case bytecode::OpCode::GE :
            return "GE";
        case bytecode::OpCode::IN :
            return "IN";
        case bytecode::OpCode::NOT_IN :
            return "NOT_IN";
        case bytecode::OpCode::IS :
            return "IS";
        case bytecode::OpCode::IS_NOT :
            return "IS_NOT";
        case bytecode::OpCode::AND :
            return "AND";
        case bytecode::OpCode::OR :
            return "OR";
        case bytecode::OpCode::NOT :
            return "NOT";
        case bytecode::OpCode::JUMP :
            return "JUMP";
        case bytecode::OpCode::JUMP_FORWARD :
            return "JUMP_FORWARD";
        case bytecode::OpCode::JUMP_BACKWARD :
            return "JUMP_BACKWARD";
        case bytecode::OpCode::JUMP_IF_FALSE :
            return "JUMP_IF_FALSE";
        case bytecode::OpCode::JUMP_IF_TRUE :
            return "JUMP_IF_TRUE";
        case bytecode::OpCode::POP_JUMP_IF_FALSE :
            return "POP_JUMP_IF_FALSE";
        case bytecode::OpCode::POP_JUMP_IF_TRUE :
            return "POP_JUMP_IF_TRUE";
        case bytecode::OpCode::FOR_ITER :
            return "FOR_ITER";
        case bytecode::OpCode::FOR_ITER_FAST :
            return "FOR_ITER_FAST";
        case bytecode::OpCode::CALL :
            return "CALL";
        case bytecode::OpCode::CALL_FAST :
            return "CALL_FAST";
        case bytecode::OpCode::RETURN :
            return "RETURN";
        case bytecode::OpCode::YIELD :
            return "YIELD";
        case bytecode::OpCode::BUILD_LIST :
            return "BUILD_LIST";
        case bytecode::OpCode::BUILD_DICT :
            return "BUILD_DICT";
        case bytecode::OpCode::BUILD_TUPLE :
            return "BUILD_TUPLE";
        case bytecode::OpCode::BUILD_SET :
            return "BUILD_SET";
        case bytecode::OpCode::UNPACK_SEQUENCE :
            return "UNPACK_SEQUENCE";
        case bytecode::OpCode::GET_ITEM :
            return "GET_ITEM";
        case bytecode::OpCode::SET_ITEM :
            return "SET_ITEM";
        case bytecode::OpCode::GET_ITER :
            return "GET_ITER";
        case bytecode::OpCode::MAKE_FUNCTION :
            return "MAKE_FUNCTION";
        case bytecode::OpCode::LOAD_CLOSURE :
            return "LOAD_CLOSURE";
        case bytecode::OpCode::PRINT :
            return "PRINT";
        case bytecode::OpCode::NOP :
            return "NOP";
        case bytecode::OpCode::HALT :
            return "HALT";
        case bytecode::OpCode::HOT_LOOP_START :
            return "HOT_LOOP_START";
        case bytecode::OpCode::HOT_LOOP_END :
            return "HOT_LOOP_END";
        default :
            return "UNKNOWN";
        }
    }

    bool needsArg(bytecode::OpCode op) const
    {
        return op != bytecode::OpCode::POP && op != bytecode::OpCode::DUP && op != bytecode::OpCode::ADD
          && op != bytecode::OpCode::SUB && op != bytecode::OpCode::MUL && op != bytecode::OpCode::DIV
          && op != bytecode::OpCode::NEG && op != bytecode::OpCode::NOT && op != bytecode::OpCode::RETURN
          && op != bytecode::OpCode::HALT && op != bytecode::OpCode::NOP;
    }
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
        int applicationsCount = 0;
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
                                              break;
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
                                      int target = instr.arg;
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

    void optimize(std::vector<bytecode::Instruction>& code, int maxIterations = 10)
    {
        bool changed = true;
        int iteration = 0;

        while (changed && iteration < maxIterations)
        {
            changed = false;

            for (auto& pass : passes)
            {
                if (pass.apply(code))
                {
                    pass.applicationsCount++;
                    changed = true;
                }
            }

            iteration++;
        }
    }

    void printReport(std::ostream& out) const
    {
        out << "\nBytecode Optimizer Report:\n";
        for (const auto& pass : passes)
        {
            if (pass.applicationsCount > 0)
            {
                out << "  • " << pass.name << ": " << pass.applicationsCount << " applications\n";
            }
        }
    }

   private:
    static bool isJumpTarget(const std::vector<bytecode::Instruction>& code, size_t pos)
    {
        // Check if any instruction jumps to this position
        for (const auto& instr : code)
        {
            if (isJumpOp(instr.op) && instr.arg == pos)
            {
                return true;
            }
        }
        return false;
    }

    static bool isJumpOp(bytecode::OpCode op)
    {
        return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD
          || op == bytecode::OpCode::JUMP_BACKWARD || op == bytecode::OpCode::JUMP_IF_FALSE
          || op == bytecode::OpCode::JUMP_IF_TRUE || op == bytecode::OpCode::POP_JUMP_IF_FALSE
          || op == bytecode::OpCode::POP_JUMP_IF_TRUE;
    }

    static bool isBinaryOp(bytecode::OpCode op)
    {
        return op == bytecode::OpCode::ADD || op == bytecode::OpCode::SUB || op == bytecode::OpCode::MUL
          || op == bytecode::OpCode::DIV || op == bytecode::OpCode::MOD || op == bytecode::OpCode::POW;
    }
};

// ============================================================================
// BYTECODE VERIFIER - Ensure bytecode correctness
// ============================================================================
class BytecodeVerifier
{
   public:
    struct VerificationError
    {
        int pc;
        std::string message;
    };

   private:
    std::vector<VerificationError> errors;

   public:
    bool verify(const BytecodeCompiler::CompilationUnit& unit)
    {
        errors.clear();

        // Check 1: Valid jump targets
        for (size_t i = 0; i < unit.instructions.size(); i++)
        {
            const auto& instr = unit.instructions[i];

            if (isJumpInstruction(instr.op))
            {
                if (instr.arg < 0 || instr.arg >= unit.instructions.size())
                {
                    errors.push_back({static_cast<int>(i), "Jump target out of bounds: " + std::to_string(instr.arg)});
                }
            }
        }

        // Check 2: Stack balance
        std::vector<int> stackDepths(unit.instructions.size(), -1);
        verifyStackDepth(unit, 0, 0, stackDepths);

        // Check 3: Constant pool bounds
        for (size_t i = 0; i < unit.instructions.size(); i++)
        {
            const auto& instr = unit.instructions[i];

            if (instr.op == bytecode::OpCode::LOAD_CONST)
            {
                if (instr.arg < 0 || instr.arg >= unit.constants.size())
                {
                    errors.push_back(
                      {static_cast<int>(i), "Constant index out of bounds: " + std::to_string(instr.arg)});
                }
            }
        }

        // Check 4: All paths return (for functions)
        // Would implement dataflow analysis here

        return errors.empty();
    }

    const std::vector<VerificationError>& getErrors() const { return errors; }

    void printErrors(std::ostream& out) const
    {
        if (errors.empty())
        {
            out << "✓ Bytecode verification passed\n";
            return;
        }

        out << "✗ Bytecode verification failed with " << errors.size() << " error(s):\n";
        for (const auto& err : errors)
        {
            out << "  PC " << err.pc << ": " << err.message << "\n";
        }
    }

   private:
    void verifyStackDepth(const BytecodeCompiler::CompilationUnit& unit, int pc, int depth, std::vector<int>& depths)
    {
        if (pc >= unit.instructions.size())
        {
            return;
        }

        // Already visited with same or greater depth
        if (depths[pc] >= depth)
        {
            return;
        }

        depths[pc] = depth;

        const auto& instr = unit.instructions[pc];
        int newDepth = depth + getStackEffect(instr.op, instr.arg);

        if (newDepth < 0)
        {
            errors.push_back({pc, "Stack underflow"});
            return;
        }

        if (newDepth > unit.stackSize)
        {
            errors.push_back(
              {pc, "Stack overflow (depth " + std::to_string(newDepth) + " > " + std::to_string(unit.stackSize) + ")"});
        }

        // Follow control flow
        if (instr.op == bytecode::OpCode::RETURN || instr.op == bytecode::OpCode::HALT)
        {
            return;
        }

        if (isJumpInstruction(instr.op))
        {
            verifyStackDepth(unit, instr.arg, newDepth, depths);
            if (!isUnconditionalJump(instr.op))
            {
                verifyStackDepth(unit, pc + 1, newDepth, depths);
            }
        }
        else
        {
            verifyStackDepth(unit, pc + 1, newDepth, depths);
        }
    }

    int getStackEffect(bytecode::OpCode op, int arg) const
    {
        switch (op)
        {
        case bytecode::OpCode::LOAD_CONST :
        case bytecode::OpCode::LOAD_VAR :
        case bytecode::OpCode::LOAD_FAST :
        case bytecode::OpCode::LOAD_GLOBAL :
        case bytecode::OpCode::DUP :
            return 1;

        case bytecode::OpCode::POP :
        case bytecode::OpCode::STORE_VAR :
        case bytecode::OpCode::STORE_FAST :
        case bytecode::OpCode::STORE_GLOBAL :
            return -1;

        case bytecode::OpCode::ADD :
        case bytecode::OpCode::SUB :
        case bytecode::OpCode::MUL :
        case bytecode::OpCode::DIV :
        case bytecode::OpCode::MOD :
        case bytecode::OpCode::POW :
        case bytecode::OpCode::EQ :
        case bytecode::OpCode::NE :
        case bytecode::OpCode::LT :
        case bytecode::OpCode::GT :
        case bytecode::OpCode::LE :
        case bytecode::OpCode::GE :
            return -1;  // 2 operands -> 1 result

        case bytecode::OpCode::BUILD_LIST :
        case bytecode::OpCode::BUILD_TUPLE :
        case bytecode::OpCode::BUILD_SET :
            return 1 - arg;  // n items -> 1 collection

        case bytecode::OpCode::CALL :
            return -arg;  // func + n args -> result

        default :
            return 0;
        }
    }

    bool isJumpInstruction(bytecode::OpCode op) const
    {
        return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD
          || op == bytecode::OpCode::JUMP_BACKWARD || op == bytecode::OpCode::JUMP_IF_FALSE
          || op == bytecode::OpCode::JUMP_IF_TRUE || op == bytecode::OpCode::POP_JUMP_IF_FALSE
          || op == bytecode::OpCode::POP_JUMP_IF_TRUE || op == bytecode::OpCode::FOR_ITER;
    }

    bool isUnconditionalJump(bytecode::OpCode op) const
    {
        return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD
          || op == bytecode::OpCode::JUMP_BACKWARD;
    }
};
}
}