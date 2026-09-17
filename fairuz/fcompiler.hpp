#ifndef FA_COMPILER_HPP
#define FA_COMPILER_HPP

#include "fAST.hpp"
#include "farray.hpp"
#include "ferror.hpp"
#include "fmacros.hpp"
#include "fopcode.hpp"
#include "fstring.hpp"
#include "ftable.hpp"
#include "fvalue.hpp"

#include <utility>

namespace fairuz::runtime {

struct LocalVar {
    StringRef name { "" };
    u32 depth { 0 };
    u8 reg { 0 };
    StringRef known_class { "" };
}; // struct LocalVar

struct CompilerState {
    Chunk* chunk { nullptr };
    Array<LocalVar> locals;
    u32 scope_depth { 0 };
    u8 next_reg { 0 };
    u8 max_reg { 0 };
    StringRef func_name { "" };
    bool is_top_level { false };
    bool is_dead { false };
    bool is_class_method { false };
    bool class_layout_dynamic { false };
    Array<StringRef> class_field_names;
    Array<StringRef> class_method_names;

    struct LoopContext {
        Array<u32> break_patches;
        Array<u32> continue_patches;
        u32 loop_start { 0 };
    }; // struct LoopContext
    Array<LoopContext> loop_stack;
    CompilerState* enclosing { nullptr };

    u8 alloc_register()
    {
        u8 reg = next_reg;
        next_reg++;
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
}; // struct CompilerState

struct CompilerStateGuard {
    CompilerState*& current;
    CompilerState* previous;

    CompilerStateGuard(CompilerState*& slot, CompilerState* next)
        : current(slot)
        , previous(slot)
    {
        current = next;
    }

    ~CompilerStateGuard() { restore(); }

    void restore()
    {
        if (current != previous)
            current = previous;
    }
};

struct RegMark {
    CompilerState* state { nullptr };
    u8 mark { 0 };
    size_t locals_mark { 0 };

    explicit RegMark(CompilerState* s)
        : state(s)
        , mark(s->next_reg)
        , locals_mark(s->locals.size())
    {
    }

    ~RegMark()
    {
        // If a local was declared inside this RegMark's scope, its
        // register must survive the rewind.
        u8 floor = mark;
        for (size_t i = locals_mark; i < state->locals.size(); ++i)
            floor = std::max<u8>(floor, state->locals[i].reg + 1);
        state->free_regs_to(floor);
    }
};

struct ExprResult {
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

    static ExprResult reg(u8 r)
    {
        ExprResult e;
        e.kind = Kind::REG;
        e.reg_ = r;
        return e;
    }
    static ExprResult reloc(u32 p)
    {
        ExprResult e;
        e.kind = Kind::RELOC;
        e.reloc_pc = p;
        return e;
    }
    static ExprResult kint(i64 v)
    {
        ExprResult e;
        e.kind = Kind::KINT;
        e.ival = v;
        return e;
    }
    static ExprResult kfloat(f64 v)
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

    bool is_const() const
    {
        return kind == Kind::KINT || kind == Kind::KFLOAT || kind == Kind::KBOOL || kind == Kind::KNIL;
    }

    bool is_reg() const { return kind == Kind::REG; }
    bool is_reloc() const { return kind == Kind::RELOC; }
}; // struct ExprResult

class Compiler {
public:
    Compiler() = default;
    ~Compiler() = default;

    Chunk* compile(Array<AST::Stmt*> const& stmts);

private:
    CompilerState* m_current { nullptr };

    struct ScopeGuard {
        CompilerState* cs { nullptr };

        ScopeGuard(CompilerState* c)
            : cs(c)
        {
            assert(cs != nullptr);
            cs->scope_depth++;
        }
        ~ScopeGuard()
        {
            cs->scope_depth -= 1;
            u32 depth = cs->scope_depth;
            Array<LocalVar>& locals = cs->locals;
            size_t pop_from = locals.size();
            while (pop_from > 0 && locals[pop_from - 1].depth > depth)
                pop_from -= 1;
            if (pop_from < locals.size())
                cs->next_reg = locals[pop_from].reg;
            locals.resize(static_cast<u32>(pop_from));
        }
    };
    struct PairHash {
        size_t operator()(std::pair<StringRef, Chunk*> const& p) const noexcept
        {
            size_t h1 = std::hash<StringRef> { }(p.first);
            size_t h2 = std::hash<Chunk*> { }(p.second);
            return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };
    struct PairEqual {
        bool operator()(std::pair<StringRef, Chunk*> lhs, std::pair<StringRef, Chunk*> rhs) const noexcept
        {
            return lhs.first == rhs.first && lhs.second == rhs.second;
        }
    };
    HashTable<std::pair<StringRef, Chunk*>, u16, PairHash, PairEqual> m_string_cache;
    HashTable<StringRef, bool, StringRefHash, StringRefEqual> m_globals;
    HashTable<StringRef, bool, StringRefHash, StringRefEqual> m_module_names;

    struct VarInfo {
        enum class Kind {
            LOCAL,
            GLOBAL
        } kind;
        u8 index { 0 };
    };

    struct ClassDesc {
        StringRef name;
        Array<StringRef> field_names;
        Array<StringRef> method_names;

        using IndexTable = HashTable<StringRef, u32, StringRefHash, StringRefEqual>;

        IndexTable field_map;
        IndexTable method_map;

        int field_index(StringRef name) const
        {
            u32 const* p = field_map.find_ptr(name);
            return LIKELY(p != nullptr) ? static_cast<int>(*p) : -1;
        }

        int method_slot(StringRef name) const
        {
            u32 const* p = method_map.find_ptr(name);
            return LIKELY(p != nullptr) ? static_cast<int>(*p) : -1;
        }
    };

    HashTable<StringRef, ClassDesc, StringRefHash, StringRefEqual> m_class_registry;

    ErrorOr<bool> compile_stmt(AST::Stmt const* s);
    ErrorOr<bool> compile_block(AST::BlockStmt const* s);
    ErrorOr<bool> compile_expr_stmt(AST::ExprStmt const* s);
    ErrorOr<bool> compile_assignment_stmt(AST::AssignStmt const* s);
    ErrorOr<bool> compile_if(AST::IfElseStmt const* s);
    ErrorOr<bool> compile_while(AST::WhileStmt const* s);
    ErrorOr<bool> compile_function_def(AST::FunctionDef const* f);
    ErrorOr<bool> compile_return(AST::ReturnStmt const* s);
    ErrorOr<bool> compile_for(AST::ForStmt const* s);
    ErrorOr<bool> compile_break(AST::BreakStmt const* s);
    ErrorOr<bool> compile_continue(AST::ContinueStmt const* s);
    ErrorOr<bool> compile_class_def(AST::ClassDef const* s);
    ErrorOr<bool> compile_import_single(
        StringRef const& module, StringRef const& name, StringRef const& alias, SourceLocation loc, bool imports_member);
    ErrorOr<bool> compile_import(AST::ImportStmt const* s);
    ErrorOr<bool> compile_class_method(AST::Stmt const* s);
    ErrorOr<ExprResult> compile_expr_impl(AST::Expr const* e);
    ErrorOr<ExprResult> compile_literal_int_impl(AST::IntLiteralExpr const* e);
    ErrorOr<ExprResult> compile_literal_float_impl(AST::FloatLiteralExpr const* e);
    ErrorOr<ExprResult> compile_literal_bool_impl(AST::BoolLiteralExpr const* e);
    ErrorOr<ExprResult> compile_literal_string_impl(AST::StringLiteralExpr const* e);
    ErrorOr<ExprResult> compile_nil_impl(AST::NilExpr const* e);
    ErrorOr<ExprResult> compile_identifier_impl(AST::IdentifierExpr const* e);
    ErrorOr<ExprResult> compile_unary_impl(AST::UnaryExpr const* e);
    ErrorOr<ExprResult> compile_binary_impl(AST::BinaryExpr const* e);
    ErrorOr<ExprResult> compile_assign_impl(AST::AssignExpr const* e);
    ErrorOr<ExprResult> compile_call_impl(AST::CallExpr const* e, u8* dst, bool tail = false);
    ErrorOr<ExprResult> compile_list_impl(AST::ListExpr const* e);
    ErrorOr<ExprResult> compile_index_impl(AST::IndexExpr const* e);
    ErrorOr<ExprResult> compile_dict_impl(AST::DictExpr const* e);
    ErrorOr<ExprResult> compile_get_impl(AST::GetExpr const* e);
    ErrorOr<ExprResult> compile_get_impl_(AST::GetExpr const* e);
    ErrorOr<u8> compile_expr(AST::Expr const* e, u8* dst = nullptr);
    ErrorOr<u8> compile_literal_int(AST::IntLiteralExpr const* e, u8* dst);
    ErrorOr<u8> compile_literal_float(AST::FloatLiteralExpr const* e, u8* dst);
    ErrorOr<u8> compile_literal_bool(AST::BoolLiteralExpr const* e, u8* dst);
    ErrorOr<u8> compile_literal_string(AST::StringLiteralExpr const* e, u8* dst);
    ErrorOr<u8> compile_nil(AST::NilExpr const* e, u8* dst);
    ErrorOr<u8> compile_identifier(AST::IdentifierExpr const* e, u8* dst);
    ErrorOr<u8> compile_unary(AST::UnaryExpr const* e, u8* dst);
    ErrorOr<u8> compile_binary(AST::BinaryExpr const* e, u8* dst);
    ErrorOr<u8> compile_assignment_expr(AST::AssignExpr const* e, u8* dst);
    ErrorOr<u8> compile_call(AST::CallExpr const* e, u8* dst, bool tail = false);
    ErrorOr<u8> compile_list(AST::ListExpr const* e, u8* dst);
    ErrorOr<u8> compile_index(AST::IndexExpr const* e, u8* dst);
    ErrorOr<u8> compile_dict(AST::DictExpr const* e, u8* dst);
    ErrorOr<u8> compile_get(AST::GetExpr const* e, u8* dst);

    void discharge(ExprResult const& r, u8 dst, SourceLocation loc);
    ErrorOr<u8> any_reg(ExprResult const& r, SourceLocation loc);
    u8 error_reg() const;
    ErrorOr<u8> alloc_register()
    {
        u8 reg = m_current->alloc_register();
        if (reg >= MAX_REGS)
            return report_error(ErrorCode::TOO_MANY_REGISTERS, { });
        return reg;
    }

    void declare_local(StringRef const& name, u8 reg)
    {
        declare_local(name, reg, "");
    }
    void declare_local(StringRef const& name, u8 reg, StringRef const& known_class)
    {
        m_current->locals.push({ name, m_current->scope_depth, reg, known_class });
    }

    LocalVar const* lookup_local(StringRef const& name) const;
    VarInfo resolve_name(StringRef const& name);
    StringRef infer_constructed_class(AST::Expr const* e) const;
    int current_method_field_index(StringRef const& name) const;
    int current_method_slot(StringRef const& name) const;

    u32 emit(u32 instr, SourceLocation loc)
    {
        return current_chunk()->emit(instr, loc);
    }

    u32 emit_jump(OpCode op, u8 cond, SourceLocation loc)
    {
        return emit(make_AsBx(op, cond, 0), loc);
    }

    void patch_jump(u32 idx)
    {
        if (!current_chunk()->patch_jump(idx))
            diagnostic::panic(ErrorCode::JUMP_OFFSET_OVERFLOW);
    }
    void push_loop(u32 loop_start)
    {
        m_current->loop_stack.push({ { }, { }, loop_start });
    }

    void pop_loop(u32 loop_exit, u32 continue_target, u32 line);
    void patch_jump_to(u32 instr_idx, u32 target);
    void emit_load_value(u8 dst, Value v, SourceLocation loc);

    Chunk* current_chunk() const { return m_current->chunk; }

    u32 current_offset() const { return current_chunk()->code.size(); }

    u32 intern_string(StringRef const& str);

    ClassDesc const* resolve_receiver_class(AST::Expr const* e) const;
    bool is_declaration(AST::AssignExpr const* e) const;

    // fcompiler.cc
    void reserve_register(u8 r)
    {
        if (r >= m_current->next_reg) {
            m_current->next_reg = r + 1;
            if (m_current->next_reg > m_current->max_reg)
                m_current->max_reg = m_current->next_reg;
        }
    }
}; // class Compiler

} // namespace fairuz::runtime

#endif // FA_COMPILER_HPP
