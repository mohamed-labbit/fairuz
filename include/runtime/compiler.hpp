#ifndef COMPILER_HPP
#define COMPILER_HPP

#include "../ast.hpp"
#include "../util.hpp"
#include "opcode.hpp"
#include "optim.hpp"

namespace mylang::runtime {

using namespace ast;

// local var descriptor
struct LocalVariableDesc {
    StringRef name;
    unsigned int depth;
    uint8_t reg;
    bool isCaptured;
};

struct UpvalueDesc {
    bool isLocal;
    uint8_t index;
};

struct CompilerState {
    Chunk* chunk { nullptr };
    Array<LocalVariableDesc> locals;
    Array<UpvalueDesc> upvalues;
    unsigned int scopeDepth { 0 };
    uint8_t nextReg { 0 };
    uint8_t maxReg { 0 };
    StringRef funcName;
    bool isTopLevel { false };
    bool isDead_ { false };

    struct LoopContext {
        Array<uint32_t> breakPatches;    // jump instruction to patch to loop exit
        Array<uint32_t> continuePatches; // jump instr to patch to loop step
        uint32_t loopStart;              // instruction index of loop header
    };
    Array<LoopContext> loopStack;

    CompilerState* enclosing { nullptr }; // linked list for upvalue resolution

    uint8_t allocRegister()
    {
        uint8_t reg = nextReg++;
        if (nextReg > maxReg)
            maxReg = nextReg;
        return reg;
    }

    void freeRegister()
    {
        if (nextReg > 0)
            nextReg--;
    }

    void freeRegsTo(uint8_t mark)
    {
        nextReg = mark;
    }
};

struct RegMark {
    CompilerState* state;
    uint8_t mark;

    explicit RegMark(CompilerState* s)
        : state(s)
        , mark(s->nextReg)
    {
    }

    ~RegMark()
    {
        state->freeRegsTo(mark);
    }
};

class Compiler {
public:
    Compiler() = default;
    ~Compiler() = default;

    Chunk* compile(Array<Stmt*> const& stmts);

private:
    CompilerState* Current_ { nullptr };
    std::unordered_map<StringRef, uint16_t> StringCache_;

    struct VarInfo {
        enum class Kind {
            LOCAL,
            UPVALUE,
            GLOBAL
        } kind;

        uint8_t index;
    };

    void compileStmt(Stmt const* s);
    void compileBlock(BlockStmt const* s);
    void compileExprStmt(ExprStmt const* s);

    /// NOTE: this doesn't support a stmt like : a[i] := x yet
    /// the expression has to be : x := y
    void compileAssignmentStmt(AssignmentStmt const* s);
    void compileIf(IfStmt const* s);
    void compileWhile(WhileStmt const* s);
    void compileFunctionDef(FunctionDef const* f);
    void compileReturn(ReturnStmt const* s);

    uint8_t compileExpr(Expr const* e, uint8_t* dst = nullptr);
    uint8_t compileLiteral(LiteralExpr const* e, uint8_t* dst);
    uint8_t compileName(NameExpr const* e, uint8_t* dst);
    uint8_t compileUnary(UnaryExpr const* e, uint8_t* dst);
    uint8_t compileBinary(BinaryExpr const* e, uint8_t* dst);
    uint8_t compileAssignmentExpr(AssignmentExpr const* e, uint8_t* dst);
    uint8_t compileCall(CallExpr const* e, uint8_t* dst, bool tail = false);
    uint8_t compileList(ListExpr const* e, uint8_t* dst);

    // helpers

    uint8_t errorReg() const;
    uint8_t ensureReg(uint8_t const* reg);
    uint8_t allocRegister();

    void declareLocal(StringRef const& name, uint8_t const reg);

    // searches from inner scope all the way to globals
    VarInfo resolveName(StringRef const& name);

    int resolveUpvalue(CompilerState* state, StringRef const& name);
    int addUpvalue(CompilerState* state, bool const is_local, uint8_t const index);

    uint32_t emit(uint32_t const instr, uint32_t const line);
    uint32_t emitJump(OpCode const op, uint8_t const cond, uint32_t const line);

    void patchJump(uint32_t const idx);
    void pushLoop(uint32_t const loop_start);
    void popLoop(uint32_t const loop_exit, uint32_t const continue_target, uint32_t const line);
    void patchJumpTo(uint32_t const instr_idx, uint32_t const target);
    void emitLoadValue(uint8_t const dst, Value const v, uint32_t const line);

    Chunk* currentChunk() const;

    uint32_t currentOffset() const;

    void beginScope();
    void endScope(uint32_t const line);

    uint32_t internString(StringRef const& str, uint32_t const line);
};

}

#endif // COMPILER_HPP
