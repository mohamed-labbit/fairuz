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

#include "Compiler/bytecode_emitter.hpp"
#include "Compiler/chunk.hpp"
#include "Compiler/opcode.hpp"
#include "Compiler/register_allocator.hpp"
#include "Diag/runtime_error.hpp"
#include "Interpreter/value.hpp"
#include "Macros.hpp"
#include "Parser/ast.hpp"
#include "SourceManager.hpp"
#include "Support/array.hpp"
#include "Support/error.hpp"
#include "Support/string.hpp"
#include "Support/table.hpp"

#include <algorithm>
#include <utility>

namespace fairuz::runtime {

static constexpr char kClassInstanceName[] = "__class$instance";

struct LocalVar {
    StringRef name { "" };
    u32 depth { 0 };
    RegType reg { 0 };
    StringRef known_class { "" };
}; // struct LocalVar

struct CompilerState {
    Chunk* chunk { nullptr };
    Array<LocalVar> locals;
    u32 scope_depth { 0 };
    StringRef func_name { "" };
    bool is_top_level { false };
    bool is_dead { false };
    bool is_class_method { false };
    bool class_layout_dynamic { false };
    Array<StringRef> class_field_names;
    Array<StringRef> class_method_names;
    RegisterAllocator registers;

    struct LoopContext {
        Array<PCType> break_patches;
        Array<PCType> continue_patches;
        PCType loop_start { 0 };
    }; // struct LoopContext
    Array<LoopContext> loop_stack;
    CompilerState* enclosing { nullptr };

    CompilerState() = default;
    CompilerState(Chunk* ch, StringRef const name, bool const is_top_level = false, CompilerState* enclosing = nullptr)
        : chunk(ch)
        , func_name(name)
        , is_top_level(is_top_level)
        , enclosing(enclosing)
    {
    }
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
        if (current != previous) current = previous;
    }
};

struct RegMark {
    CompilerState* state { nullptr };
    u16 mark { 0 };
    size_t locals_mark { 0 };

    explicit RegMark(CompilerState* s)
        : state(s)
        , mark(s->registers.next_reg())
        , locals_mark(s->locals.size())
    {
    }

    ~RegMark()
    {
        // If a local was declared inside this RegMark's scope, its
        // register must survive the rewind.
        u16 floor = mark;
        for (size_t i = locals_mark; i < state->locals.size(); ++i)
            floor = std::max<u16>(floor, static_cast<u16>(state->locals[i].reg.value) + 1);
        state->registers.rewind(floor);
    }
};

struct ExprResult {
    enum class Kind : u8 { REG, RELOC, KINT, KFLOAT, KBOOL, KNIL } kind;

    union {
        RegType reg_;
        PCType reloc_pc;
        i64 ival;
        f64 dval;
        bool bval;
    }; // union

    ExprResult()
        : kind(Kind::KNIL)
        , ival(0)
    {
    }

    static ExprResult reg(RegType r) { return ExprResult(r); }
    static ExprResult reloc(PCType p) { return ExprResult(p); }
    static ExprResult kint(i64 v) { return ExprResult(v); }
    static ExprResult kfloat(f64 v) { return ExprResult(v); }
    static ExprResult kbool(bool v) { return ExprResult(v); }
    static ExprResult knil() { return ExprResult(); }

private:
    // Initialize the selected union member directly. Strong identities have
    // default constructors; assigning into an inactive member is not how we
    // establish its lifetime.
    explicit ExprResult(RegType r)
        : kind(Kind::REG)
        , reg_(r)
    {
    }

    explicit ExprResult(PCType p)
        : kind(Kind::RELOC)
        , reloc_pc(p)
    {
    }

    explicit ExprResult(i64 v)
        : kind(Kind::KINT)
        , ival(v)
    {
    }

    explicit ExprResult(f64 v)
        : kind(Kind::KFLOAT)
        , dval(v)
    {
    }

    explicit ExprResult(bool v)
        : kind(Kind::KBOOL)
        , bval(v)
    {
    }
}; // struct ExprResult

class Compiler {
public:
    using CompileExprResult = ErrorOr<ExprResult, RuntimeError>;
    using CompileStmtResult = ErrorOr<void, RuntimeError>;

    explicit Compiler(SourcePtr source)
        : m_source(std::move(source))
        , m_bytecode_emitter(this)
    {
        assert(m_source != nullptr);
    }
    ~Compiler() = default;

    ErrorOr<Chunk*, RuntimeError> compile(Array<AST::StmtPtr> const& stmts);
    static ErrorOr<bool, RuntimeError> compile_function_deferred(Chunk* chunk);
    static ErrorOr<bool, RuntimeError> compile_all(Chunk* chunk);

    Chunk* current_chunk() const { return m_current->chunk; }

private:
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
            while (pop_from > 0 && locals[pop_from - 1].depth > depth) pop_from -= 1;
            if (pop_from < locals.size()) cs->registers.rewind(locals[pop_from].reg.value);
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

    SourcePtr m_source;
    CompilerState* m_current { nullptr };
    ByteCodeEmitter m_bytecode_emitter;
    HashTable<std::pair<StringRef, Chunk*>, ConstantIndex, PairHash, PairEqual> m_string_cache;
    HashTable<StringRef, bool, StringRefHash, StringRefEqual> m_globals;
    HashTable<StringRef, bool, StringRefHash, StringRefEqual> m_module_names;
    HashTable<StringRef, ClassDescriptor, StringRefHash, StringRefEqual> m_class_registry;

    struct VarInfo {
        enum class Kind { LOCAL, GLOBAL } kind;
        RegType reg { 0 };
    };

    CompileStmtResult compile_stmt(AST::Stmt* s);
    CompileStmtResult compile_block(AST::BlockStmt* s);
    CompileStmtResult compile_expr_stmt(AST::ExprStmt* s);
    CompileStmtResult compile_assignment_stmt(AST::AssignStmt* s);
    CompileStmtResult compile_if(AST::IfElseStmt* s);
    CompileStmtResult compile_while(AST::WhileStmt* s);
    CompileStmtResult compile_function_def(AST::FuncDefStmt* f);
    void defer_function_body(
        Chunk* chunk, AST::FuncDefStmt const* node, CompilerState state, StringRef receiver_class = { });
    CompileStmtResult compile_function_body(
        AST::FuncDefStmt const* node, CompilerState& state, StringRef receiver_class);
    CompileStmtResult compile_return(AST::ReturnStmt* s);
    CompileStmtResult compile_for(AST::ForStmt* s);
    CompileStmtResult compile_break(AST::BreakStmt* s);
    CompileStmtResult compile_continue(AST::ContinueStmt* s);
    CompileStmtResult compile_class_def(AST::ClassDefStmt* s);
    CompileStmtResult compile_import_single(StringRef const& module, StringRef const& name, StringRef const& alias,
        SourceLocation loc, bool imports_member);
    CompileStmtResult compile_import(AST::ImportStmt* s);
    CompileStmtResult compile_class_method(AST::Stmt* s);

    CompileExprResult compile_expr_impl(AST::ExprPtr);
    CompileExprResult compile_literal_int_impl(AST::IntLiteralExpr* e);
    CompileExprResult compile_literal_float_impl(AST::FloatLiteralExpr* e);
    CompileExprResult compile_literal_bool_impl(AST::BoolLiteralExpr* e);
    CompileExprResult compile_literal_string_impl(AST::StringLiteralExpr* e);
    CompileExprResult compile_nil_impl(AST::NilExpr* e);
    CompileExprResult compile_identifier_impl(AST::IdentifierExpr* e);
    CompileExprResult compile_unary_impl(AST::UnaryExpr* e);
    CompileExprResult compile_binary_impl(AST::BinaryExpr* e);
    CompileExprResult compile_assign_impl(AST::AssignExpr* e);
    CompileExprResult compile_call_impl(AST::CallExpr* e, RegType* dst, bool tail = false);
    CompileExprResult compile_list_impl(AST::ListExpr* e);
    CompileExprResult compile_index_impl(AST::IndexExpr* e);
    CompileExprResult compile_dict_impl(AST::DictExpr* e);
    CompileExprResult compile_get_impl(AST::GetExpr* e);

    ErrorOr<RegType, RuntimeError> compile_expr(AST::ExprPtr e, RegType* dst = nullptr);

    ErrorOr<RegType, RuntimeError> any_reg(ExprResult const& r, SourceLocation loc);
    ErrorOr<RegType, RuntimeError> alloc_register(SourceLocation loc);

    void declare_local(StringRef const& name, RegType reg) { declare_local(name, reg, ""); }
    void declare_local(StringRef const& name, RegType reg, StringRef const& known_class)
    {
        m_current->locals.push({ name, m_current->scope_depth, reg, known_class });
    }

    LocalVar const* lookup_local(StringRef const& name) const;

    VarInfo resolve_name(StringRef const& name);

    StringRef infer_constructed_class(AST::ExprPtr e) const;

    int current_method_field_index(StringRef const& name) const;

    int current_method_slot(StringRef const& name) const;

    void push_loop(PCType loop_start) { m_current->loop_stack.push({ { }, { }, loop_start }); }

    void pop_loop(PCType loop_exit, PCType continue_target, u32 line);

    PCType current_offset() const { return PCType { current_chunk()->code.size() }; }

    ConstantIndex intern_string(StringRef const& str);

    ClassDescriptor const* resolve_receiver_class(AST::ExprPtr e) const;

    bool is_declaration(AST::AssignExpr const* e) const;

    ErrorOr<void> reserve_register(RegType reg) { return m_current->registers.reserve_register(reg); }

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
        return e->get_kind() == AST::ExprKind::IDENTIFIER && AST::as_identifier(e)->spelling == kClassInstanceName;
    }

    RuntimeError raise_compile_error(Error const& err)
    {
        return raise_compile_error(err.get_code(), err.get_location());
    }

    RuntimeError raise_compile_error(ErrorCode errc, SourceLocation loc)
    {
        return RuntimeError { errc, m_source, loc };
    }
}; // class Compiler

} // namespace fairuz::runtime

#endif // FAIRUZ_COMPILER_COMPILER_HPP
