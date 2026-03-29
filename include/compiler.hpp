#ifndef COMPILER_HPP
#define COMPILER_HPP

#include "ast.hpp"
#include "opcode.hpp"
#include "string.hpp"
#include "value.hpp"
#include "table.hpp"

#include <utility>

namespace fairuz::runtime {

struct LocalVar {
    Fa_StringRef name;
    unsigned int depth;
    u8 reg;
}; // struct LocalVar

struct UpvalueDesc {
    bool isLocal;
    u8 index;
}; // struct UpvalueDesc

struct CompilerState {
    Fa_Chunk* chunk { nullptr };
    Fa_Array<LocalVar> locals;
    unsigned int scopeDepth { 0 };
    u8 nextReg { 0 };
    u8 maxReg { 0 };
    Fa_StringRef funcName;
    bool isTopLevel { false };
    bool isDead_ { false };

    struct LoopContext {
        Fa_Array<u32> breakPatches;
        Fa_Array<u32> continuePatches;
        u32 loopStart;
    };
    Fa_Array<LoopContext> loopStack;
    CompilerState* enclosing { nullptr };

    u8 allocRegister()
    {
        u8 reg = nextReg++;
        if (nextReg > maxReg)
            maxReg = nextReg;
        return reg;
    }
    void freeRegister()
    {
        if (nextReg > 0)
            nextReg--;
    }
    void freeRegsTo(u8 m) { nextReg = m; }
}; // struct CompilerState

struct RegMark {
    CompilerState* state;
    u8 mark;

    explicit RegMark(CompilerState* s)
        : state(s)
        , mark(s->nextReg)
    {
    }

    ~RegMark()
    {
        state->freeRegsTo(mark);
    }
}; // struct RegMark

struct Fa_ExprResult {
    enum class Kind : u8 {
        REG,
        RELOC,
        KINT,
        KFLOAT,
        KBOOL,
        KNIL
    } kind;

    union {
        u8 reg_;
        u32 reloc_pc;
        i64 ival;
        f64 dval;
        bool bval;
    }; // union

    static Fa_ExprResult reg(u8 r)
    {
        Fa_ExprResult e;
        e.kind = Kind::REG;
        e.reg_ = r;
        return e;
    }
    static Fa_ExprResult reloc(u32 p)
    {
        Fa_ExprResult e;
        e.kind = Kind::RELOC;
        e.reloc_pc = p;
        return e;
    }
    static Fa_ExprResult kint(i64 v)
    {
        Fa_ExprResult e;
        e.kind = Kind::KINT;
        e.ival = v;
        return e;
    }
    static Fa_ExprResult kfloat(f64 v)
    {
        Fa_ExprResult e;
        e.kind = Kind::KFLOAT;
        e.dval = v;
        return e;
    }
    static Fa_ExprResult kbool(bool v)
    {
        Fa_ExprResult e;
        e.kind = Kind::KBOOL;
        e.bval = v;
        return e;
    }
    static Fa_ExprResult knil()
    {
        Fa_ExprResult e;
        e.kind = Kind::KNIL;
        e.ival = 0;
        return e;
    }

    bool isConst() const
    {
        return kind == Kind::KINT || kind == Kind::KFLOAT || kind == Kind::KBOOL || kind == Kind::KNIL;
    }

    bool isReg() const { return kind == Kind::REG; }
    bool isReloc() const { return kind == Kind::RELOC; }
}; // struct Fa_ExprResult

class Compiler {
public:
    Compiler() = default;
    ~Compiler() = default;

    Fa_Chunk* compile(Fa_Array<AST::Fa_Stmt*> const& stmts);

private:
    CompilerState* Current_ { nullptr };

    struct PairHash {
        size_t operator()(std::pair<Fa_StringRef, Fa_Chunk*> const& p) const noexcept
        {
            size_t h1 = std::hash<Fa_StringRef> { }(p.first);
            size_t h2 = std::hash<Fa_Chunk*> { }(p.second);
            return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };
    struct PairEqual {
        bool operator()(std::pair<Fa_StringRef, Fa_Chunk*> lhs, std::pair<Fa_StringRef, Fa_Chunk*> rhs) const noexcept 
        {
            return lhs.first == rhs.first && lhs.second == rhs.second;
        }
    };
    Fa_HashTable<std::pair<Fa_StringRef, Fa_Chunk*>, u16, PairHash, PairEqual> StringCache_;

    struct VarInfo {
        enum class Kind {
            LOCAL,
            GLOBAL
        } kind;
        u8 index;
    };

    void compileStmt(AST::Fa_Stmt const* s);
    void compileBlock(AST::Fa_BlockStmt const* s);
    void compileExprStmt(AST::Fa_ExprStmt const* s);
    void compileAssignmentStmt(AST::Fa_AssignmentStmt const* s);
    void compileIf(AST::Fa_IfStmt const* s);
    void compileWhile(AST::Fa_WhileStmt const* s);
    void compileFunctionDef(AST::Fa_FunctionDef const* f);
    void compileReturn(AST::Fa_ReturnStmt const* s);
    void compileFor(AST::Fa_ForStmt const* s);

    Fa_ExprResult compileExprI(AST::Fa_Expr const* e);
    Fa_ExprResult compileLiteralI(AST::Fa_LiteralExpr const* e);
    Fa_ExprResult compileNameI(AST::Fa_NameExpr const* e);
    Fa_ExprResult compileUnaryI(AST::Fa_UnaryExpr const* e);
    Fa_ExprResult compileBinaryI(AST::Fa_BinaryExpr const* e);
    Fa_ExprResult compileAssignI(AST::Fa_AssignmentExpr const* e);
    Fa_ExprResult compileCallImpl(AST::Fa_CallExpr const* e, u8* dst, bool tail = false);
    Fa_ExprResult compileListI(AST::Fa_ListExpr const* e);
    Fa_ExprResult compileIndexI(AST::Fa_IndexExpr const* e);

    void discharge(Fa_ExprResult const& r, u8 dst, Fa_SourceLocation loc);

    u8 anyReg(Fa_ExprResult const& r, Fa_SourceLocation loc);

    u8 compileExpr(AST::Fa_Expr const* e, u8* dst = nullptr);
    u8 compileLiteral(AST::Fa_LiteralExpr const* e, u8* dst);
    u8 compileName(AST::Fa_NameExpr const* e, u8* dst);
    u8 compileUnary(AST::Fa_UnaryExpr const* e, u8* dst);
    u8 compileBinary(AST::Fa_BinaryExpr const* e, u8* dst);
    u8 compileAssignmentExpr(AST::Fa_AssignmentExpr const* e, u8* dst);
    u8 compileCall(AST::Fa_CallExpr const* e, u8* dst, bool tail = false);
    u8 compileList(AST::Fa_ListExpr const* e, u8* dst);
    u8 compileIndex(AST::Fa_IndexExpr const* e, u8* dst);

    u8 errorReg() const;
    u8 ensureReg(u8 const* reg);
    u8 allocRegister();

    void declareLocal(Fa_StringRef const& name, u8 reg);
    VarInfo resolveName(Fa_StringRef const& name);

    int resolveUpvalue(CompilerState* state, Fa_StringRef const& name);
    int addUpvalue(CompilerState* state, bool is_local, u8 index);

    u32 emit(u32 instr, Fa_SourceLocation loc);
    u32 emitJump(Fa_OpCode op, u8 cond, Fa_SourceLocation loc);

    void patchJump(u32 idx);
    void pushLoop(u32 loop_start);
    void popLoop(u32 loop_exit, u32 continue_target, u32 line);
    void patchJumpTo(u32 instr_idx, u32 target);
    void emitLoadValue(u8 dst, Fa_Value v, Fa_SourceLocation loc);

    Fa_Chunk* currentChunk() const;
    u32 currentOffset() const;

    void beginScope();
    void endScope(Fa_SourceLocation loc);

    u32 internString(Fa_StringRef const& str);
}; // class Compiler

} // namespace fairuz::runtime

#endif // COMPILER_HPP
