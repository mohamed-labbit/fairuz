//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Defines the AST-to-bytecode compiler interface.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_COMPILER_COMPILER_HPP
#define FAIRUZ_COMPILER_COMPILER_HPP

#include "Compiler/chunk.hpp"
#include "Compiler/opcode.hpp"
#include "Interpreter/value.hpp"
#include "Macros.hpp"
#include "Parser/ast.hpp"
#include "Support/array.hpp"
#include "Support/error.hpp"
#include "Support/string.hpp"
#include "Support/table.hpp"

#include <utility>

namespace fairuz::runtime {

static constexpr char kClassInstanceName[] = "__class$instance";

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
    using reg_type = u8;
    using compile_expr_result = ErrorOr<ExprResult>;
    using compile_stmt_result = ErrorOr<bool>;

    Compiler() = default;
    ~Compiler() = default;

    Chunk* compile(Array<AST::StmtPtr> const& stmts);
    static bool compile_function_deferred(Chunk* chunk);
    static bool compile_all(Chunk* chunk);

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

    compile_stmt_result compile_stmt(AST::Stmt* s);
    compile_stmt_result compile_block(AST::BlockStmt* s);
    compile_stmt_result compile_expr_stmt(AST::ExprStmt* s);
    compile_stmt_result compile_assignment_stmt(AST::AssignStmt* s);
    compile_stmt_result compile_if(AST::IfElseStmt* s);
    compile_stmt_result compile_while(AST::WhileStmt* s);
    compile_stmt_result compile_function_def(AST::FuncDefStmt* f);
    void defer_function_body(Chunk* chunk, AST::FuncDefStmt const* node,
        CompilerState state, StringRef receiver_class = { });
    compile_stmt_result compile_function_body(AST::FuncDefStmt const* node,
        CompilerState& state, StringRef receiver_class);
    compile_stmt_result compile_return(AST::ReturnStmt* s);
    compile_stmt_result compile_for(AST::ForStmt* s);
    compile_stmt_result compile_break(AST::BreakStmt* s);
    compile_stmt_result compile_continue(AST::ContinueStmt* s);
    compile_stmt_result compile_class_def(AST::ClassDefStmt* s);
    compile_stmt_result compile_import_single(
        StringRef const& module, StringRef const& name, StringRef const& alias, SourceLocation loc, bool imports_member);
    compile_stmt_result compile_import(AST::ImportStmt* s);
    compile_stmt_result compile_class_method(AST::Stmt* s);

    compile_expr_result compile_expr_impl(AST::ExprPtr);
    compile_expr_result compile_literal_int_impl(AST::IntLiteralExpr* e);
    compile_expr_result compile_literal_float_impl(AST::FloatLiteralExpr* e);
    compile_expr_result compile_literal_bool_impl(AST::BoolLiteralExpr* e);
    compile_expr_result compile_literal_string_impl(AST::StringLiteralExpr* e);
    compile_expr_result compile_nil_impl(AST::NilExpr* e);
    compile_expr_result compile_identifier_impl(AST::IdentifierExpr* e);
    compile_expr_result compile_unary_impl(AST::UnaryExpr* e);
    compile_expr_result compile_binary_impl(AST::BinaryExpr* e);
    compile_expr_result compile_assign_impl(AST::AssignExpr* e);
    compile_expr_result compile_call_impl(AST::CallExpr* e, u8* dst, bool tail = false);
    compile_expr_result compile_list_impl(AST::ListExpr* e);
    compile_expr_result compile_index_impl(AST::IndexExpr* e);
    compile_expr_result compile_dict_impl(AST::DictExpr* e);
    compile_expr_result compile_get_impl(AST::GetExpr* e);

    ErrorOr<reg_type> compile_expr(AST::ExprPtr e, u8* dst = nullptr);
    ErrorOr<reg_type> compile_literal_int(AST::IntLiteralExpr* e, u8* dst);
    ErrorOr<reg_type> compile_literal_float(AST::FloatLiteralExpr* e, u8* dst);
    ErrorOr<reg_type> compile_literal_bool(AST::BoolLiteralExpr* e, u8* dst);
    ErrorOr<reg_type> compile_literal_string(AST::StringLiteralExpr* e, u8* dst);
    ErrorOr<reg_type> compile_nil(AST::NilExpr* e, u8* dst);
    ErrorOr<reg_type> compile_identifier(AST::IdentifierExpr* e, u8* dst);
    ErrorOr<reg_type> compile_unary(AST::UnaryExpr* e, u8* dst);
    ErrorOr<reg_type> compile_binary(AST::BinaryExpr* e, u8* dst);
    ErrorOr<reg_type> compile_assignment_expr(AST::AssignExpr* e, u8* dst);
    ErrorOr<reg_type> compile_call(AST::CallExpr* e, u8* dst, bool tail = false);
    ErrorOr<reg_type> compile_list(AST::ListExpr* e, u8* dst);
    ErrorOr<reg_type> compile_index(AST::IndexExpr* e, u8* dst);
    ErrorOr<reg_type> compile_dict(AST::DictExpr* e, u8* dst);
    ErrorOr<reg_type> compile_get(AST::GetExpr* e, u8* dst);

    void discharge(ExprResult const& r, u8 dst, SourceLocation loc);
    ErrorOr<reg_type> any_reg(ExprResult const& r, SourceLocation loc);
    ErrorOr<reg_type> alloc_register();

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

    StringRef infer_constructed_class(AST::ExprPtr e) const;

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

    ClassDesc const* resolve_receiver_class(AST::ExprPtr e) const;

    bool is_declaration(AST::AssignExpr const* e) const;

    void reserve_register(u8 r);

    // Mirrors fairuz::parser::same_name (fparser.cc), which is file-local to
    // that translation unit and not visible here. Used to detect `this.field`
    // GET targets (object side is the synthetic kClassInstanceName NAME node)
    // so we can fall back to current_method_field_index() instead of
    // resolve_receiver_class(), which depends on m_class_registry — and the
    // class currently being compiled is NOT YET in m_class_registry while its
    // own methods are still being compiled (see compile_class_def: the
    // registry insert happens only after the full method-compilation loop).
    static bool is_this_reference(AST::ExprPtr e)
    {
        return e->get_kind() == AST::ExprKind::IDENTIFIER
            && AST::as_identifier(e)->spelling == kClassInstanceName;
    }

}; // class Compiler

} // namespace fairuz::runtime

#endif // FAIRUZ_COMPILER_COMPILER_HPP
