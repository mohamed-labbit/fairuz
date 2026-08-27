#ifndef FA_CFG_COMPILER_HPP
#define FA_CFG_COMPILER_HPP

#include "fAST.hpp"
#include "ferror.hpp"
#include "fopcode.hpp"
#include "fstring.hpp"
#include "ftable.hpp"
#include "fvalue.hpp"
#include "fcfg.hpp"

#include <utility>

namespace fairuz::runtime {

struct CFG_LocalVar {
    Fa_StringRef name { "" };
    unsigned int depth { 0 };
    u8 reg { 0 };
    Fa_StringRef known_class { "" };
}; // struct CFG_LocalVar

struct CFG_CompilerState {
    struct LoopContext {
        Fa_Array<u32> break_patches;
        Fa_Array<u32> continue_patches;
        u32 loop_start { 0 };
    }; // struct LoopContext

    Fa_Chunk* chunk { nullptr };
    Fa_Array<CFG_LocalVar> locals;
    unsigned int scope_depth { 0 };
    u8 next_reg { 0 };
    u8 max_reg { 0 };
    Fa_StringRef func_name;
    bool is_top_level { false };
    bool is_dead { false };
    bool is_class_method { false };
    Fa_Array<Fa_StringRef> class_field_names;
    Fa_Array<LoopContext> loop_stack;
    CFG_CompilerState* enclosing { nullptr };

    u8 alloc_register()
    {
        u8 reg = next_reg;
        next_reg += 1;
        if (next_reg > max_reg)
            max_reg = next_reg;
        return reg;
    }
    void free_register()
    {
        if (next_reg > 0)
            next_reg -= 1;
    }
    void free_regs_to(u8 m) { next_reg = m; }
}; // struct CFG_CompilerState

struct CFG_RegMark {
    CFG_CompilerState* state { nullptr };
    u8 mark { 0 };

    explicit CFG_RegMark(CFG_CompilerState* s)
        : state(s)
        , mark(s->next_reg)
    {
    }

    ~CFG_RegMark()
    {
        state->free_regs_to(mark);
    }
}; // struct CFG_RegMark

struct CFG_Fa_ExprResult {
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

    static CFG_Fa_ExprResult reg(u8 r)
    {
        CFG_Fa_ExprResult e;
        e.kind = Kind::REG;
        e.reg_ = r;
        return e;
    }
    static CFG_Fa_ExprResult reloc(u32 p)
    {
        CFG_Fa_ExprResult e;
        e.kind = Kind::RELOC;
        e.reloc_pc = p;
        return e;
    }
    static CFG_Fa_ExprResult kint(i64 v)
    {
        CFG_Fa_ExprResult e;
        e.kind = Kind::KINT;
        e.ival = v;
        return e;
    }
    static CFG_Fa_ExprResult kfloat(f64 v)
    {
        CFG_Fa_ExprResult e;
        e.kind = Kind::KFLOAT;
        e.dval = v;
        return e;
    }
    static CFG_Fa_ExprResult kbool(bool v)
    {
        CFG_Fa_ExprResult e;
        e.kind = Kind::KBOOL;
        e.bval = v;
        return e;
    }
    static CFG_Fa_ExprResult knil()
    {
        CFG_Fa_ExprResult e;
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
}; // struct CFG_Fa_ExprResult

class CFG_Compiler {
public:
    CFG_Compiler() = default;
    ~CFG_Compiler() = default;

    Fa_Chunk* compile(Fa_Program* program);
    void set_program(Fa_Program* p) { m_program = p; }

private:
    CFG_CompilerState* m_current { nullptr };
    Fa_Program* m_program { nullptr };

    struct PairHash {
        std::size_t operator()(std::pair<Fa_StringRef, Fa_Chunk*> const& p) const noexcept
        {
            std::size_t h1 = std::hash<Fa_StringRef> { }(p.first);
            std::size_t h2 = std::hash<Fa_Chunk*> { }(p.second);
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
        u8 index { 0 };
    };

    struct ClassDesc {
        Fa_StringRef name;
        Fa_Array<Fa_StringRef> field_names;
        Fa_Array<Fa_StringRef> method_names;

        using IndexTable = Fa_HashTable<Fa_StringRef, u32, Fa_StringRefHash, Fa_StringRefEqual>;

        IndexTable field_map;
        IndexTable method_map;

        int field_index(Fa_StringRef name) const
        {
            u32 const* p = field_map.find_ptr(name);
            return LIKELY(p != nullptr) ? static_cast<int>(*p) : -1;
        }

        int method_slot(Fa_StringRef name) const
        {
            u32 const* p = method_map.find_ptr(name);
            return LIKELY(p != nullptr) ? static_cast<int>(*p) : -1;
        }
    };

    Fa_HashTable<Fa_StringRef, ClassDesc, Fa_StringRefHash, Fa_StringRefEqual> m_class_registry;

    Fa_ErrorOr<bool> compile_stmt(AST::Fa_Stmt* s);
    Fa_ErrorOr<bool> compile_block(AST::Fa_BlockStmt* s);
    Fa_ErrorOr<bool> compile_expr_stmt(AST::Fa_ExprStmt* s);
    Fa_ErrorOr<bool> compile_assignment_stmt(AST::Fa_AssignmentStmt* s);
    Fa_ErrorOr<bool> compile_function_def(AST::Fa_FunctionDef* f);
    Fa_ErrorOr<bool> compile_function_def(Fa_CFG_Function* f);
    Fa_ErrorOr<bool> compile_return(AST::Fa_ReturnStmt* s);
    Fa_ErrorOr<bool> compile_class_def(AST::Fa_ClassDef* s);
    Fa_ErrorOr<bool> compile_class_method(AST::Fa_Stmt* s);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_expr_impl(AST::Fa_Expr* e);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_literal_impl(AST::Fa_LiteralExpr* e);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_name_impl(AST::Fa_NameExpr* e);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_unary_impl(AST::Fa_UnaryExpr* e);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_binary_impl(AST::Fa_BinaryExpr* e);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_assign_impl(AST::Fa_AssignmentExpr* e);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_call_impl(AST::Fa_CallExpr* e, u8* dst, bool tail = false);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_list_impl(AST::Fa_ListExpr* e);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_index_impl(AST::Fa_IndexExpr* e);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_dict_impl(AST::Fa_DictExpr* e);
    Fa_ErrorOr<CFG_Fa_ExprResult> compile_get_impl(AST::Fa_GetExpr* e);
    Fa_ErrorOr<u8> compile_expr(AST::Fa_Expr* e, u8* dst = nullptr);
    Fa_ErrorOr<u8> compile_literal(AST::Fa_LiteralExpr* e, u8* dst);
    Fa_ErrorOr<u8> compile_name(AST::Fa_NameExpr* e, u8* dst);
    Fa_ErrorOr<u8> compile_unary(AST::Fa_UnaryExpr* e, u8* dst);
    Fa_ErrorOr<u8> compile_binary(AST::Fa_BinaryExpr* e, u8* dst);
    Fa_ErrorOr<u8> compile_assignment_expr(AST::Fa_AssignmentExpr* e, u8* dst);
    Fa_ErrorOr<u8> compile_call(AST::Fa_CallExpr* e, u8* dst, bool tail = false);
    Fa_ErrorOr<u8> compile_list(AST::Fa_ListExpr* e, u8* dst);
    Fa_ErrorOr<u8> compile_index(AST::Fa_IndexExpr* e, u8* dst);
    Fa_ErrorOr<u8> compile_dict(AST::Fa_DictExpr* e, u8* dst);
    Fa_ErrorOr<u8> compile_get(AST::Fa_GetExpr* e, u8* dst);
    Fa_ErrorOr<bool> compile_cfg_body(Fa_CFG* cfg, bool is_top_level_script);

    void discharge(CFG_Fa_ExprResult const& r, u8 dst, Fa_SourceLocation loc);
    Fa_ErrorOr<u8> any_reg(CFG_Fa_ExprResult const& r, Fa_SourceLocation loc);
    Fa_ErrorOr<u8> alloc_register();

    void declare_local(Fa_StringRef const& name, u8 reg);
    void declare_local(Fa_StringRef const& name, u8 reg, Fa_StringRef const& known_class);
    CFG_LocalVar const* lookup_local(Fa_StringRef const& name) const;
    VarInfo resolve_name(Fa_StringRef const& name);
    Fa_StringRef infer_constructed_class(AST::Fa_Expr const* e) const;
    int current_method_field_index(Fa_StringRef const& name) const;

    u32 emit(u32 instr, Fa_SourceLocation loc);
    u32 emit_jump(Fa_OpCode op, u8 cond, Fa_SourceLocation loc);
    void emit_load_value(u8 dst, Fa_Value v, Fa_SourceLocation loc);

    Fa_Chunk* current_chunk() const;
    u32 current_offset() const;

    void begin_scope();
    void end_scope(Fa_SourceLocation loc);
    void end_scope_no_reclaim(Fa_SourceLocation loc);

    u32 intern_string(Fa_StringRef const& str);

    ClassDesc const* resolve_receiver_class(AST::Fa_Expr const* e) const;
    bool is_declaration(AST::Fa_AssignmentExpr const* e) const;

    // fcompiler.cc
    void reserve_register(u8 r)
    {
        if (r >= m_current->next_reg) {
            m_current->next_reg = r + 1;
            if (m_current->next_reg > m_current->max_reg)
                m_current->max_reg = m_current->next_reg;
        }
    }
}; // class CFG_Compiler

} // namespace fairuz::runtime

#endif // FA_COMPILER_HPP
