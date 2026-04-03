#ifndef COMPILER_HPP
#define COMPILER_HPP

#include "ast.hpp"
#include "opcode.hpp"
#include "string.hpp"
#include "table.hpp"
#include "value.hpp"

#include <utility>

namespace fairuz::runtime {

struct LocalVar {
    Fa_StringRef m_name;
    unsigned int depth;
    u8 m_reg;
}; // struct LocalVar

struct CompilerState {
    Fa_Chunk* chunk { nullptr };
    Fa_Array<LocalVar> locals;
    unsigned int scope_depth { 0 };
    u8 next_reg { 0 };
    u8 max_reg { 0 };
    Fa_StringRef func_name;
    bool is_top_level { false };
    bool m_is_dead { false };

    struct LoopContext {
        Fa_Array<u32> break_patches;
        Fa_Array<u32> continue_patches;
        u32 loop_start;
    };
    Fa_Array<LoopContext> loop_stack;
    CompilerState* enclosing { nullptr };

    u8 alloc_register()
    {
        u8 m_reg = next_reg;
        next_reg += 1;
        if (next_reg > max_reg)
            max_reg = next_reg;
        return m_reg;
    }
    void free_register()
    {
        if (next_reg > 0)
            next_reg -= 1;
    }
    void free_regs_to(u8 m) { next_reg = m; }
}; // struct CompilerState

struct RegMark {
    CompilerState* state;
    u8 mark;

    explicit RegMark(CompilerState* s)
        : state(s)
        , mark(s->next_reg)
    {
    }

    ~RegMark()
    {
        state->free_regs_to(mark);
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

    bool is_const() const
    {
        return kind == Kind::KINT || kind == Kind::KFLOAT || kind == Kind::KBOOL || kind == Kind::KNIL;
    }

    bool is_reg() const { return kind == Kind::REG; }
    bool is_reloc() const { return kind == Kind::RELOC; }
}; // struct Fa_ExprResult

class Compiler {
public:
    Compiler() = default;
    ~Compiler() = default;

    Fa_Chunk* compile(Fa_Array<AST::Fa_Stmt*> const& stmts);

private:
    CompilerState* m_current { nullptr };

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
    Fa_HashTable<std::pair<Fa_StringRef, Fa_Chunk*>, u16, PairHash, PairEqual> m_string_cache;

    struct VarInfo {
        enum class Kind {
            LOCAL,
            GLOBAL
        } kind;
        u8 m_index;
    };

    void compile_stmt(AST::Fa_Stmt const* s);
    void compile_block(AST::Fa_BlockStmt const* s);
    void compile_expr_stmt(AST::Fa_ExprStmt const* s);
    void compile_assignment_stmt(AST::Fa_AssignmentStmt const* s);
    void compile_if(AST::Fa_IfStmt const* s);
    void compile_while(AST::Fa_WhileStmt const* s);
    void compile_function_def(AST::Fa_FunctionDef const* f);
    void compile_return(AST::Fa_ReturnStmt const* s);
    void compile_for(AST::Fa_ForStmt const* s);
    void compile_break(AST::Fa_BreakStmt const* s);
    void compile_continue(AST::Fa_ContinueStmt const* s);

    Fa_ExprResult compile_expr_i(AST::Fa_Expr const* e);
    Fa_ExprResult compile_literal_i(AST::Fa_LiteralExpr const* e);
    Fa_ExprResult compile_name_i(AST::Fa_NameExpr const* e);
    Fa_ExprResult compile_unary_i(AST::Fa_UnaryExpr const* e);
    Fa_ExprResult compile_binary_i(AST::Fa_BinaryExpr const* e);
    Fa_ExprResult compile_assign_i(AST::Fa_AssignmentExpr const* e);
    Fa_ExprResult compile_call_impl(AST::Fa_CallExpr const* e, u8* dst, bool tail = false);
    Fa_ExprResult compile_list_i(AST::Fa_ListExpr const* e);
    Fa_ExprResult compile_index_i(AST::Fa_IndexExpr const* e);
    Fa_ExprResult compile_dict_i(AST::Fa_DictExpr const* e);

    void discharge(Fa_ExprResult const& r, u8 dst, Fa_SourceLocation loc);

    u8 any_reg(Fa_ExprResult const& r, Fa_SourceLocation loc);

    u8 compile_expr(AST::Fa_Expr const* e, u8* dst = nullptr);
    u8 compile_literal(AST::Fa_LiteralExpr const* e, u8* dst);
    u8 compile_name(AST::Fa_NameExpr const* e, u8* dst);
    u8 compile_unary(AST::Fa_UnaryExpr const* e, u8* dst);
    u8 compile_binary(AST::Fa_BinaryExpr const* e, u8* dst);
    u8 compile_assignment_expr(AST::Fa_AssignmentExpr const* e, u8* dst);
    u8 compile_call(AST::Fa_CallExpr const* e, u8* dst, bool tail = false);
    u8 compile_list(AST::Fa_ListExpr const* e, u8* dst);
    u8 compile_index(AST::Fa_IndexExpr const* e, u8* dst);
    u8 compile_dict(AST::Fa_DictExpr const* e, u8* dst);

    u8 error_reg() const;
    u8 ensure_reg(u8 const* m_reg);
    u8 alloc_register();

    void declare_local(Fa_StringRef const& m_name, u8 m_reg);
    LocalVar const* lookup_local(Fa_StringRef const& m_name) const;
    VarInfo resolve_name(Fa_StringRef const& m_name);

    u32 emit(u32 instr, Fa_SourceLocation loc);
    u32 emit_jump(Fa_OpCode op, u8 cond, Fa_SourceLocation loc);

    void patch_jump(u32 idx);
    void push_loop(u32 loop_start);
    void pop_loop(u32 loop_exit, u32 continue_target, u32 m_line);
    void patch_jump_to(u32 instr_idx, u32 m_target);
    void emit_load_value(u8 dst, Fa_Value v, Fa_SourceLocation loc);

    Fa_Chunk* current_chunk() const;
    u32 current_offset() const;

    void begin_scope();
    void end_scope(Fa_SourceLocation loc);

    u32 intern_string(Fa_StringRef const& str);
}; // class Compiler

} // namespace fairuz::runtime

#endif // COMPILER_HPP
