#ifndef COMPILER_HPP
#define COMPILER_HPP

#include "ast.hpp"
#include "value.hpp"

#include <unordered_map>
#include <utility>

namespace mylang::runtime {

using namespace ast;

struct LocalVar {
    StringRef name;
    unsigned int depth;
    uint8_t reg;
};

struct UpvalueDesc {
    bool isLocal;
    uint8_t index;
};

struct CompilerState {
    Chunk* chunk { nullptr };
    Array<LocalVar> locals;
    unsigned int scopeDepth { 0 };
    uint8_t nextReg { 0 };
    uint8_t maxReg { 0 };
    StringRef funcName;
    bool isTopLevel { false };
    bool isDead_ { false };

    struct LoopContext {
        Array<uint32_t> breakPatches;
        Array<uint32_t> continuePatches;
        uint32_t loopStart;
    };
    Array<LoopContext> loopStack;
    CompilerState* enclosing { nullptr };

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
    void freeRegsTo(uint8_t m) { nextReg = m; }
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

struct ExprResult {
    enum class Kind : uint8_t { REG,
        RELOC,
        KINT,
        KFLOAT,
        KBOOL,
        KNIL } kind;

    union {
        uint8_t reg_;
        uint32_t reloc_pc;
        int64_t ival;
        double dval;
        bool bval;
    };

    static ExprResult reg(uint8_t r)
    {
        ExprResult e;
        e.kind = Kind::REG;
        e.reg_ = r;
        return e;
    }
    static ExprResult reloc(uint32_t p)
    {
        ExprResult e;
        e.kind = Kind::RELOC;
        e.reloc_pc = p;
        return e;
    }
    static ExprResult kint(int64_t v)
    {
        ExprResult e;
        e.kind = Kind::KINT;
        e.ival = v;
        return e;
    }
    static ExprResult kfloat(double v)
    {
        ExprResult e;
        e.kind = Kind::KFLOAT;
        e.dval = v;
        return e;
    }
    static ExprResult kbool(bool v)
    {
        ExprResult e;
        e.kind = Kind::KBOOL;
        e.bval = v;
        return e;
    }
    static ExprResult knil()
    {
        ExprResult e;
        e.kind = Kind::KNIL;
        e.ival = 0;
        return e;
    }

    bool isConst() const { return kind == Kind::KINT || kind == Kind::KFLOAT || kind == Kind::KBOOL || kind == Kind::KNIL; }
    bool isReg() const { return kind == Kind::REG; }
    bool isReloc() const { return kind == Kind::RELOC; }
};

class Compiler {
public:
    Compiler() = default;
    ~Compiler() = default;

    Chunk* compile(Array<Stmt*> const& stmts);

private:
    CompilerState* Current_ { nullptr };

    struct PairHash {
        size_t operator()(std::pair<StringRef, Chunk*> const& p) const noexcept
        {
            size_t h1 = std::hash<StringRef> { }(p.first);
            size_t h2 = std::hash<Chunk*> { }(p.second);
            return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };
    std::unordered_map<std::pair<StringRef, Chunk*>, uint16_t, PairHash> StringCache_;

    struct VarInfo {
        enum class Kind {
            LOCAL,
            GLOBAL
        } kind;
        uint8_t index;
    };

    void compileStmt(Stmt const* s);
    void compileBlock(BlockStmt const* s);
    void compileExprStmt(ExprStmt const* s);
    void compileAssignmentStmt(AssignmentStmt const* s);
    void compileIf(IfStmt const* s);
    void compileWhile(WhileStmt const* s);
    void compileFunctionDef(FunctionDef const* f);
    void compileReturn(ReturnStmt const* s);
    void compileFor(ForStmt const* s);

    ExprResult compileExprI(Expr const* e);
    ExprResult compileLiteralI(LiteralExpr const* e);
    ExprResult compileNameI(NameExpr const* e);
    ExprResult compileUnaryI(UnaryExpr const* e);
    ExprResult compileBinaryI(BinaryExpr const* e);
    ExprResult compileAssignI(AssignmentExpr const* e);
    ExprResult compileCallImpl(CallExpr const* e, uint8_t* dst, bool tail = false);
    ExprResult compileListI(ListExpr const* e);
    ExprResult compileIndexI(IndexExpr const* e);

    void discharge(ExprResult const& r, uint8_t dst, SourceLocation loc);

    uint8_t anyReg(ExprResult const& r, SourceLocation loc);

    uint8_t compileExpr(Expr const* e, uint8_t* dst = nullptr);
    uint8_t compileLiteral(LiteralExpr const* e, uint8_t* dst);
    uint8_t compileName(NameExpr const* e, uint8_t* dst);
    uint8_t compileUnary(UnaryExpr const* e, uint8_t* dst);
    uint8_t compileBinary(BinaryExpr const* e, uint8_t* dst);
    uint8_t compileAssignmentExpr(AssignmentExpr const* e, uint8_t* dst);
    uint8_t compileCall(CallExpr const* e, uint8_t* dst, bool tail = false);
    uint8_t compileList(ListExpr const* e, uint8_t* dst);
    uint8_t compileIndex(IndexExpr const* e, uint8_t* dst);

    uint8_t errorReg() const;
    uint8_t ensureReg(uint8_t const* reg);
    uint8_t allocRegister();

    void declareLocal(StringRef const& name, uint8_t reg);
    VarInfo resolveName(StringRef const& name);

    int resolveUpvalue(CompilerState* state, StringRef const& name);
    int addUpvalue(CompilerState* state, bool is_local, uint8_t index);

    uint32_t emit(uint32_t instr, SourceLocation loc);
    uint32_t emitJump(OpCode op, uint8_t cond, SourceLocation loc);

    void patchJump(uint32_t idx);
    void pushLoop(uint32_t loop_start);
    void popLoop(uint32_t loop_exit, uint32_t continue_target, uint32_t line);
    void patchJumpTo(uint32_t instr_idx, uint32_t target);
    void emitLoadValue(uint8_t dst, Value v, SourceLocation loc);

    Chunk* currentChunk() const;
    uint32_t currentOffset() const;

    void beginScope();
    void endScope(SourceLocation loc);

    uint32_t internString(StringRef const& str);
};

} // namespace mylang::runtime

#endif // COMPILER_HPP
