#pragma once

#include "../opcode/chunk.hpp"
#include "../opcode/opcode.hpp"
#include "forward.hpp"
#include "value.hpp"

#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mylang {
namespace runtime {

// CompileError — carries a human-readable message + source location.
struct CompileError {
    StringRef message;
    uint32_t line;
    StringRef context; // name of enclosing function

    StringRef format() const;
};

using Reg = uint8_t;

// Local variable descriptor
struct Local {
    StringRef name;
    int depth; // scope depth (0 = global-ish top level, 1+ = nested)
    Reg reg;
    bool isCaptured; // needs upvalue when the scope closes
    bool isConst;    // declared with 'const' — write is a compile error
};

// Upvalue descriptor (per-function)
struct UpvalueDesc {
    bool isLocal;  // true = captures a local in the immediately enclosing function
    uint8_t index; // local register (is_local=true) or upvalue index (false)
};

// CompilerState — per-function compilation state, stacked for nested fns.
struct CompilerState {
    Chunk* chunk; // owned by the caller
    std::vector<Local> locals;
    std::vector<UpvalueDesc> upvalues;
    int scopeDepth = 0;
    Reg nextReg = 0; // watermark-style allocator
    Reg maxReg = 0;  // high-water mark → local_count
    StringRef funcName;
    bool isTopLevel = false;

    // break/continue patch lists: indices into chunk->code that need patching
    // Each loop pushes a new frame; break/continue targets are collected here.
    struct LoopContext {
        std::vector<uint32_t> breakPatches;    // JUMP instrs to patch to loop exit
        std::vector<uint32_t> continuePatches; // JUMP instrs to patch to loop step
        uint32_t loopStart;                    // instruction index of loop header
    };
    std::vector<LoopContext> loopStack;

    CompilerState* enclosing = nullptr; // linked list for upvalue resolution

    Reg allocReg()
    {
        Reg r = nextReg++;
        if (nextReg > maxReg)
            maxReg = nextReg;

        return r;
    }

    void freeReg()
    {
        if (nextReg > 0)
            nextReg--;
    }

    // Free back down to a watermark (end of expression temporaries)
    void freeRegsTo(Reg mark)
    {
        nextReg = mark;
    }
};

// Compiler
//
// Implements a single-pass recursive-descent compiler from AST to bytecode.
//
// Optimizations performed during lowering (no separate pass needed):
//   1. Constant folding: binary/unary ops on compile-time-known values are
//      evaluated immediately; no instruction is emitted.
//   2. Dead code elimination: code after RETURN/BREAK/CONTINUE is suppressed.
//   3. Short-circuit evaluation: && and || skip RHS if LHS determines result.
//   4. LOAD_INT for small integers (avoids constant table entry).
//   5. Tail-call detection: CALL in tail position → CALL_TAIL.
//   6. For-loop integer fast path: integer step/limit → FOR_PREP/FOR_STEP.
//   7. String interning: string literals are deduplicated in the constant pool.
//
// Type specialization / inline caches:
//   IC slots are allocated on polymorphic CALL sites and binary ops.
//   The interpreter fills ICSlot::seen_* at runtime; the JIT reads them.
class Compiler {
public:
    Compiler() = default;
    ~Compiler() = default;

    // Compile a top-level AST into a Chunk.  Returns nullptr on error.
    // Errors are stored in errors() and can be pretty-printed.
    Chunk* compile(std::vector<ast::Stmt*> const& root);

    std::vector<CompileError> const& errors() const { return Errors_; }

    bool had_error() const { return !Errors_.empty(); }

private:
    // ---- state stack ----
    CompilerState* Current_ = nullptr; // innermost function being compiled
    std::vector<CompileError> Errors_;
    // String interning table: char data → constant index (across entire compilation)
    std::unordered_map<StringRef, uint16_t> StringCache_;

    // ---- entry points per AST node category ----
    void compileStmt(ast::Stmt const* s);
    void compileBlock(ast::BlockStmt const* s);
    void compileExprStmt(ast::ExprStmt const* s);
    void compileAssignmentStmt(ast::AssignmentStmt const* s);
    void compileIf(ast::IfStmt const* s);
    void compileWhile(ast::WhileStmt const* s);
    void compileFor(ast::ForStmt const* s);
    void compileFunctionDef(ast::FunctionDef const* s);
    void compileReturn(ast::ReturnStmt const* s);

    // Compile expr, place result in *dst_hint* register if provided,
    // otherwise allocate a fresh temporary.  Returns the register holding
    // the result (which may equal dst_hint).
    Reg compileExpr(ast::Expr const* e, Reg* dst = nullptr);
    Reg compileBinary(ast::BinaryExpr const* e, Reg* dst);
    Reg compileUnary(ast::UnaryExpr const* e, Reg* dst);
    Reg compileLiteral(ast::LiteralExpr const* e, Reg* dst);
    Reg compileName(ast::NameExpr const* e, Reg* dst);
    Reg compileAssignmentExpr(ast::AssignmentExpr const* e, Reg* dst);
    Reg compileCall(ast::CallExpr const* e, Reg* dst, bool tail_pos = false);
    Reg compileList(ast::ListExpr const* e, Reg* dst);

    // ---- constant folding ----
    // Returns a folded Value if the expression is a compile-time constant,
    // nullopt otherwise.
    std::optional<Value> _tryFoldBinary(ast::BinaryExpr const* e); // single pass, no recursion
    std::optional<Value> tryFoldBinary(ast::BinaryExpr const* e); // recurive
    std::optional<Value> tryFoldUnary(ast::UnaryExpr const* e);
    std::optional<Value> constValue(ast::Expr const* e); // returns const if trivially known

    // ---- register allocation helpers ----
    Reg allocReg();

    void freeReg();
    void freeRegsTo(Reg mark);

    Reg ensureDst(Reg* dst); // alloc if not provided

    // ---- emit helpers ----
    uint32_t emit(Instruction i, uint32_t line);
    uint32_t emitJump(OpCode op, Reg cond, uint32_t line); // emits with placeholder offset

    void patchJump(uint32_t idx);
    void patchJumpTo(uint32_t idx, uint32_t target);

    uint32_t currentOffset() const;

    // Emit a constant load into dst; chooses LOAD_INT vs LOAD_CONST automatically.
    void emitLoadValue(Reg dst, Value v, uint32_t line);

    // ---- scope / variable management ----
    void beginScope();
    void endScope(uint32_t line);

    void declareLocal(StringRef const& name, Reg reg, bool is_const = false);

    // Resolve a name: returns (reg, is_upvalue, upvalue_idx) or signals global.
    struct VarInfo {
        enum class Kind {
            LOCAL,
            UPVALUE,
            GLOBAL
        } kind;

        uint8_t index; // reg for LOCAL, upvalue idx for UPVALUE, unused for GLOBAL
    };

    VarInfo resolveName(StringRef const& name);
    int resolveUpValue(CompilerState* state, StringRef const& name);

    int addUpValue(CompilerState* state, bool is_local, uint8_t index);

    // ---- function compilation ----
    // Pushes a new CompilerState, compiles the body, pops it.
    // Returns the Chunk* (stored in parent chunk's functions list).
    Chunk* compileFunction(StringRef const& name, std::vector<StringRef> const& params, ast::Stmt const* body, uint32_t line);

    // ---- loop context ----
    void pushLoop(uint32_t loop_start);
    void popLoop(uint32_t loop_exit_instr, uint32_t continue_target, uint32_t line);

    void emitBreak(uint32_t line);
    void emitContinue(uint32_t line);

    // ---- dead code suppression ----
    bool isDead_ = false; // set after RETURN/unconditional jump; cleared at labels

    void markDead() { isDead_ = true; }
    void clearDead() { isDead_ = false; }

    // ---- error reporting ----
    void error(StringRef const& msg, uint32_t line);

    Reg errorReg(); // returns 0 after recording an error

    // ---- utilities ----
    Chunk const* currentChunk() const { return Current_->chunk; }
    Chunk* currentChunk() { return Current_->chunk; }

    uint32_t internString(StringRef const& s, uint32_t line);
};

} // namespace runtime
} // namespace mylang
