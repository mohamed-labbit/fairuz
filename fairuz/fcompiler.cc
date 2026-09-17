//
// fcompiler.cc
//

#include "fcompiler.hpp"
#include "fAST.hpp"
#include "farray.hpp"
#include "fdiagnostic.hpp"
#include "ferror.hpp"
#include "fmacros.hpp"
#include "fobj_header.hpp"
#include "fobject.hpp"
#include "fopcode.hpp"
#include "foptim.hpp"
#include "fstring.hpp"
#include "fvalue.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>

#define VERIFY_RESULT(r)            \
    do {                               \
        if (UNLIKELY((r).has_error())) \
            return (r).error();        \
    } while (0)
#define TRY_ASSIGN(out, expr) \
    do {                         \
        auto _r = (expr);        \
        VERIFY_RESULT(_r);    \
        *(out) = _r.value();     \
    } while (0)
#define TRY_DISCARD(expr)  \
    do {                      \
        auto _r = (expr);     \
        VERIFY_RESULT(_r); \
        (void)_r;             \
    } while (0)

#define COMPILE_EXPR_IMPL(e, r) TRY_ASSIGN(r, compile_expr_impl(e))
#define COMPILE_STMT_DISCARD(s) TRY_DISCARD(compile_stmt(s))
#define ANY_REG(v, l, out) TRY_ASSIGN(out, any_reg((v), (l)))
#define ALLOC_REG(out) TRY_ASSIGN(out, alloc_register())

namespace fairuz::runtime {

/// TODO: run an analysis of whether or not null checks for AST nodes
/// can be safely removed matching the AST validity invariant

using ExprPtr = AST::Expr*;
using StmtPtr = AST::Stmt*;
using ConstExprPtr = AST::Expr const*;
using ConstStmtPtr = AST::Stmt const*;
using cmp_ret = ErrorOr<ExprResult>;
using reg_t = u8;

static constexpr char kClassInstanceName[] = "__class$instance";

static bool is_terminal_top_level_call(ConstStmtPtr s)
{
    auto const* expr_stmt = dynamic_cast<AST::ExprStmt const*>(s);
    if (expr_stmt == nullptr)
        return false;

    return dynamic_cast<AST::CallExpr const*>(expr_stmt->expr) != nullptr;
}

static void patch_a(Chunk* chunk, u32 pc, reg_t a)
{
    u32 instr = chunk->code[pc];
    chunk->code[pc] = (instr & 0xFF00FFFFu) | (static_cast<u32>(a) << 16);
}

// Mirrors fairuz::parser::same_name (fparser.cc), which is file-local to
// that translation unit and not visible here. Used to detect `this.field`
// GET targets (object side is the synthetic kClassInstanceName NAME node)
// so we can fall back to current_method_field_index() instead of
// resolve_receiver_class(), which depends on m_class_registry — and the
// class currently being compiled is NOT YET in m_class_registry while its
// own methods are still being compiled (see compile_class_def: the
// registry insert happens only after the full method-compilation loop).
static bool is_this_reference(ConstExprPtr e)
{
    return e->get_kind() == AST::Expr::Kind::IDENTIFIER
        && AST::as_identifier(e)->spelling == kClassInstanceName;
}

Chunk* Compiler::compile(Array<AST::Stmt*> const& stmts)
{
    Chunk* chunk = make_chunk();
    chunk->name = "<main>";
    chunk->source = diagnostic::engine.source();
    if (chunk->source)
        chunk->source_path = chunk->source->path;

    CompilerState state;
    state.chunk = chunk;
    state.func_name = "<main>";
    state.is_top_level = true;
    state.enclosing = nullptr;
    m_current = &state;

    for (size_t i = 0; i < stmts.size(); i++) {
        AST::Stmt* stmt = stmts[i];
        if (i + 1 == stmts.size() && stmt && !state.is_dead && is_terminal_top_level_call(stmt)) {
            auto const* expr_stmt = as_expr_stmt(stmt);
            SourceLocation loc = expr_stmt->get_location();
            RegMark mark(m_current);
            auto expr_result = compile_expr_impl(expr_stmt->expr);
            if (expr_result.has_error())
                break;
            auto src = any_reg(expr_result.value(), loc);
            if (src.has_error())
                break;
            emit(make_ABC(OpCode::RETURN, src.value(), 1, 0), loc);
            state.is_dead = true;
            break;
        }
        auto stmt_result = compile_stmt(stmt);
        if (stmt_result.has_error())
            break;
    }

    SourceLocation loc = { 1, 1, 0 };
    if (!stmts.empty() && stmts.back())
        loc = stmts.back()->get_location();

    if (!state.is_dead)
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);

    chunk->local_count = state.max_reg;
    m_current = nullptr;

    if (diagnostic::has_errors())
        diagnostic::dump();
    return chunk;
}

ErrorOr<bool> Compiler::compile_stmt(ConstStmtPtr s)
{
    if (m_current->is_dead)
        return true;

    switch (s->get_kind()) {
    case AST::Stmt::Kind::BLOCK: return compile_block(as_block(s));
    case AST::Stmt::Kind::EXPR: return compile_expr_stmt(as_expr_stmt(s));
    case AST::Stmt::Kind::IF: return compile_if(as_if(s));
    case AST::Stmt::Kind::WHILE: return compile_while(as_while(s));
    case AST::Stmt::Kind::FUNC: return compile_function_def(as_function_def(s));
    case AST::Stmt::Kind::RETURN: return compile_return(as_return(s));
    case AST::Stmt::Kind::FOR: return compile_for(as_for(s));
    case AST::Stmt::Kind::BREAK: return compile_break(as_break(s));
    case AST::Stmt::Kind::CONTINUE: return compile_continue(as_continue(s));
    case AST::Stmt::Kind::CLASS_DEF: return compile_class_def(as_class_def(s));
    case AST::Stmt::Kind::IMPORT: return compile_import(as_import(s));
    case AST::Stmt::Kind::INVALID:
    default:
        return report_error(ErrorCode::INVALID_STATEMENT_NODE, s->get_location());
    }
}

ErrorOr<bool> Compiler::compile_import(AST::ImportStmt const* s)
{
    if (!m_current->is_top_level || m_current->scope_depth != 0)
        return report_error(ErrorCode::INVALID_STATEMENT_NODE, s->get_location());

    SourceLocation loc = s->get_location();
    StringRef const& module = s->get_module();
    Array<StringRef> const& names = s->get_names();
    Array<StringRef> const& aliases = s->get_aliases();
    if (!s->imports_member()) {
        // Whole-module imports have no member names, but still bind one alias.
        if (aliases.size() != 1)
            return report_error(ErrorCode::INVALID_STATEMENT_NODE, loc);
        return compile_import_single(module, { }, aliases[0], loc, false);
    }
    if (names.size() != aliases.size())
        return report_error(ErrorCode::INVALID_STATEMENT_NODE, loc);
    for (size_t i = 0; i < names.size(); ++i)
        TRY_DISCARD(compile_import_single(module, names[i], aliases[i], loc, true));

    return true;
}

ErrorOr<bool> Compiler::compile_import_single(
    StringRef const& module, StringRef const& name, StringRef const& alias, SourceLocation loc, bool imports_member)
{
    // Imported bindings live in the module's global environment. Keeping a
    // permanent local register for every import makes a large module exhaust
    // the 8-bit register file even though those temporary values are already
    // stored globally. Rewind the load/extract temporaries after this
    // statement and resolve later references through LOAD_GLOBAL.
    RegMark mark(m_current);
    reg_t module_reg;
    ALLOC_REG(&module_reg);
    emit(make_ABx(OpCode::IMPORT_MODULE, module_reg, intern_string(module)), loc);

    reg_t value_reg = module_reg;
    if (imports_member) {
        ALLOC_REG(&value_reg);
        emit(make_ABC(OpCode::GET_FIELD, value_reg, module_reg, 0xFF), loc);
        emit(make_ABx(OpCode::NOP, 0, intern_string(name)), loc);
    }

    emit(make_ABx(OpCode::STORE_GLOBAL, value_reg, intern_string(alias)), loc);
    m_globals[alias] = true;
    if (!imports_member)
        m_module_names[alias] = true;
    return true;
}

ErrorOr<bool> Compiler::compile_block(AST::BlockStmt const* s)
{
    ScopeGuard scope(m_current);

    for (AST::Stmt* child : s->stmts) {
        auto r = compile_stmt(child);
        VERIFY_RESULT(r);
    }

    SourceLocation loc = { 1, 1, 0 };
    if (!s->stmts.empty() && s->stmts.back())
        loc = s->stmts.back()->get_location();
    return true;
}

ErrorOr<bool> Compiler::compile_expr_stmt(AST::ExprStmt const* s)
{
    RegMark mark(m_current);
    ExprResult r;
    COMPILE_EXPR_IMPL(s->expr, &r);
    reg_t tmp;
    ANY_REG(r, s->get_location(), &tmp);
    return true;
}

ErrorOr<bool> Compiler::compile_if(AST::IfElseStmt const* s)
{
    SourceLocation loc = s->get_location();
    ScopeGuard scope(m_current);
    bool incoming_dead = m_current->is_dead;

    if (auto folded = try_fold_expr(s->condition)) {
        if (folded->is_truthy()) {
            auto ret = compile_stmt(s->then_stmt);
            m_current->is_dead = incoming_dead;
            return ret;
        } else if (s->else_stmt) {
            auto ret = compile_stmt(s->else_stmt);
            m_current->is_dead = incoming_dead;
            return ret;
        }
        return true;
    }

    RegMark mark(m_current);
    ExprResult expr_result;
    reg_t cond;
    COMPILE_EXPR_IMPL(s->condition, &expr_result);
    ANY_REG(expr_result, loc, &cond);
    u32 jump_false = emit_jump(OpCode::JUMP_IF_FALSE, cond, loc);
    COMPILE_STMT_DISCARD(s->then_stmt);

    if (s->else_stmt) {
        u32 jump_end = emit_jump(OpCode::JUMP, 0, loc);
        patch_jump(jump_false);
        auto else_ret = compile_stmt(s->else_stmt);
        VERIFY_RESULT(else_ret);
        (void)else_ret;
        patch_jump(jump_end);
    } else {
        patch_jump(jump_false);
    }

    m_current->is_dead = incoming_dead;
    return true;
}

ErrorOr<bool> Compiler::compile_while(AST::WhileStmt const* s)
{
    if (s == nullptr)
        return true;

    SourceLocation loc = s->get_location();
    ScopeGuard scope(m_current);

    bool incoming_dead = m_current->is_dead;
    if (auto folded = try_fold_expr(s->condition)) {
        if (folded->is_truthy()) {
            u32 loop_start = current_offset();
            push_loop(loop_start);
            COMPILE_STMT_DISCARD(s->body);
            u32 continue_target = current_offset();
            emit(make_AsBx(OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(current_offset()) - 1), loc);
            pop_loop(current_offset(), continue_target, loc.line);
        }

        m_current->is_dead = incoming_dead;
        return true;
    }

    u32 loop_start = current_offset();
    push_loop(loop_start);

    {
        RegMark mark(m_current);
        ExprResult expr_result;
        reg_t cond;
        COMPILE_EXPR_IMPL(s->condition, &expr_result);
        ANY_REG(expr_result, loc, &cond);
        u32 exit_jump = emit_jump(OpCode::JUMP_IF_FALSE, cond, loc);
        COMPILE_STMT_DISCARD(s->body);
        u32 continue_target = current_offset();
        emit(make_AsBx(OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(current_offset()) - 1), loc);
        patch_jump(exit_jump);
        pop_loop(current_offset(), continue_target, loc.line);
    }
    m_current->is_dead = incoming_dead;
    return true;
}

ErrorOr<bool> Compiler::compile_function_def(AST::FunctionDef const* f)
{
    SourceLocation loc = f->get_location();
    if (!m_current->is_top_level || m_current->scope_depth != 0)
        return report_error(ErrorCode::NESTED_FUNCTION_UNSUPPORTED, f->get_location());

    if (current_chunk()->functions.size() > MAX_CONSTANTS)
        return report_error(ErrorCode::TOO_MANY_FUNCTIONS, loc);

    Chunk* fn_chunk = make_chunk();
    fn_chunk->source = current_chunk()->source;
    fn_chunk->source_path = current_chunk()->source_path;
    fn_chunk->name = f->name->spelling;
    fn_chunk->arity = f->has_parameters() ? static_cast<int>(f->params->size()) : 0;

    auto fn_idx = static_cast<u16>(current_chunk()->functions.size());
    current_chunk()->functions.push(fn_chunk);

    CompilerState fn_state;
    fn_state.chunk = fn_chunk;
    fn_state.func_name = f->name->spelling;
    fn_state.enclosing = m_current;
    CompilerStateGuard state_guard(m_current, &fn_state);
    ScopeGuard scope(m_current);

    if (f->has_parameters()) {
        for (AST::Expr* param : f->params->elements) {
            auto param_name = dynamic_cast<AST::IdentifierExpr*>(param);
            if (param_name == nullptr)
                return report_error(ErrorCode::INVALID_FUNCTION_PARAMETER,
                    param ? param->get_location() : f->get_location());

            reg_t reg;
            ALLOC_REG(&reg);
            declare_local(param_name->spelling, reg);
        }
    }

    COMPILE_STMT_DISCARD(f->body);

    if (!fn_state.is_dead)
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);

    fn_chunk->local_count = fn_state.max_reg;
    state_guard.restore();

    reg_t dst;
    ALLOC_REG(&dst);
    emit(make_ABx(OpCode::CLOSURE, dst, fn_idx), loc);

    if (m_current != nullptr && m_current->is_top_level) {
        u16 name_idx = intern_string(f->name->spelling);
        emit(make_ABx(OpCode::STORE_GLOBAL, dst, name_idx), loc);
    }

    declare_local(f->name->spelling, dst);
    return true;
}

ErrorOr<bool> Compiler::compile_return(AST::ReturnStmt const* s)
{
    SourceLocation loc = s->get_location();

    if (!s->has_value()) {
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);
        m_current->is_dead = true;
        return true;
    }

    ConstExprPtr value = s->value;
    if (AST::is_nil(value)) {
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);
        m_current->is_dead = true;
        return true;
    }

    if (AST::is_call(value) && !m_current->is_top_level) {
        auto* call_expr = as_call(value);
        bool is_ctor_call = !AST::is_get(call_expr->callee)
            && AST::is_identifier(call_expr->callee)
            && m_class_registry.find_ptr(AST::as_identifier(call_expr->callee)->spelling) != nullptr;

        RegMark mark(m_current);
        auto call_ret = compile_call_impl(call_expr, nullptr, /*tail=*/!is_ctor_call);
        VERIFY_RESULT(call_ret);
        (void)call_ret;

        if (is_ctor_call) {
            // compile_call_impl didn't emit a RETURN for a non-tail call; do it here.
            reg_t src;
            ANY_REG(call_ret.value(), loc, &src);
            emit(make_ABC(OpCode::RETURN, src, 1, 0), loc);
        }
        m_current->is_dead = true;
        return true;
    }

    RegMark mark(m_current);
    ExprResult expr_result;
    reg_t src;
    COMPILE_EXPR_IMPL(value, &expr_result);
    ANY_REG(expr_result, loc, &src);
    emit(make_ABC(OpCode::RETURN, src, 1, 0), loc);
    m_current->is_dead = true;
    return true;
}

ErrorOr<bool> Compiler::compile_for(AST::ForStmt const* s)
{
    SourceLocation loc = s->get_location();
    ScopeGuard scope(m_current);

    auto target = AST::as_identifier(s->container);
    bool incoming_dead = m_current->is_dead;

    reg_t iter_reg;
    ALLOC_REG(&iter_reg);
    {
        declare_local("__for_iter", iter_reg);
        RegMark mark(m_current);
        ExprResult expr_result;
        COMPILE_EXPR_IMPL(s->iter, &expr_result);
        discharge(expr_result, iter_reg, loc);
    }

    reg_t len_reg, index_reg, target_reg, cond_reg, step_reg;

    ALLOC_REG(&len_reg);
    ALLOC_REG(&index_reg);
    ALLOC_REG(&target_reg);
    ALLOC_REG(&cond_reg);
    ALLOC_REG(&step_reg);

    declare_local("__for_len", len_reg);
    declare_local("__for_index", index_reg);
    declare_local(target->spelling, target_reg);
    declare_local("__for_cond", cond_reg);
    declare_local("__for_step", step_reg);

    emit(make_ABC(OpCode::LIST_LEN, len_reg, iter_reg, 0), loc);
    emit_load_value(index_reg, Value::from_int(0), loc);
    emit_load_value(step_reg, Value::from_int(1), loc);

    u32 loop_start = current_offset();
    push_loop(loop_start);
    emit(make_ABC(OpCode::OP_LT, cond_reg, index_reg, len_reg), loc);
    emit(make_ABC(OpCode::NOP, current_chunk()->alloc_ic_slot(), 0, 0), loc);

    u32 exit_jump = emit_jump(OpCode::JUMP_IF_FALSE, cond_reg, loc);
    emit(make_ABC(OpCode::LIST_GET, target_reg, iter_reg, index_reg), loc);
    COMPILE_STMT_DISCARD(s->body);

    u32 continue_target = current_offset();
    emit(make_ABC(OpCode::OP_ADD, index_reg, index_reg, step_reg), loc);
    emit(make_ABC(OpCode::NOP, current_chunk()->alloc_ic_slot(), 0, 0), loc);
    emit(make_AsBx(OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(current_offset()) - 1), loc);

    patch_jump(exit_jump);
    pop_loop(current_offset(), continue_target, loc.line);

    m_current->is_dead = incoming_dead;
    return true;
}

ErrorOr<bool> Compiler::compile_break(AST::BreakStmt const* s)
{
    if (m_current->loop_stack.empty())
        return report_error(ErrorCode::BREAK_OUTSIDE_LOOP, s->get_location());

    SourceLocation loc = s->get_location();
    m_current->loop_stack.back().break_patches.push(emit_jump(OpCode::JUMP, 0, loc));
    m_current->is_dead = true;
    return true;
}

ErrorOr<bool> Compiler::compile_continue(AST::ContinueStmt const* s)
{
    if (m_current->loop_stack.empty())
        return report_error(ErrorCode::CONTINUE_OUTSIDE_LOOP, s->get_location());

    SourceLocation loc = s->get_location();
    m_current->loop_stack.back().continue_patches.push(emit_jump(OpCode::JUMP, 0, loc));
    m_current->is_dead = true;
    return true;
}

ErrorOr<bool> Compiler::compile_class_def(AST::ClassDef const* s)
{
    if (s == nullptr)
        return true;

    SourceLocation loc = s->get_location();
    if (!m_current->is_top_level || m_current->scope_depth != 0)
        return report_error(ErrorCode::NESTED_CLASS_UNSUPPORTED, loc);

    Array<AST::Expr*> fields = s->get_members();
    Array<AST::Stmt*> methods = s->get_methods();
    StringRef class_name = AST::as_identifier(s->get_name())->spelling;
    StringRef parent_name;
    ClassDesc const* parent_desc = nullptr;
    if (s->get_parent() != nullptr) {
        parent_name = AST::as_identifier(s->get_parent())->spelling;
        parent_desc = m_class_registry.find_ptr(parent_name);
    }

    Array<StringRef> own_field_names;
    Array<StringRef> field_names;
    Array<StringRef> method_names(static_cast<u32>(ObjClass::_COUNT), StringRef { });
    if (parent_desc != nullptr) {
        field_names = parent_desc->field_names;
        method_names = parent_desc->method_names;
    }

    for (AST::Expr* field : fields) {
        auto* name = AST::as_identifier(field);
        StringRef fname = name->spelling;

        bool seen = false;
        for (auto& existing : own_field_names) {
            if (existing == fname) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            own_field_names.push(fname);
            bool inherited = false;
            for (auto& existing : field_names) {
                if (existing == fname) {
                    inherited = true;
                    break;
                }
            }
            if (!inherited)
                field_names.push(fname);
        }
    }

    auto compile_method_closure = [&](AST::FunctionDef* method) -> ErrorOr<std::tuple<reg_t, Chunk*>> {
        SourceLocation method_loc = method->get_location();

        if (current_chunk()->functions.size() > MAX_CONSTANTS)
            return report_error(ErrorCode::TOO_MANY_FUNCTIONS, method_loc);

        Chunk* ch = make_chunk();
        ch->source = current_chunk()->source;
        ch->source_path = current_chunk()->source_path;
        ch->name = class_name + "." + method->name->spelling;
        int ex_param_count = method->has_parameters() ? static_cast<int>(method->params->size()) : 0;
        ch->arity = ex_param_count + 1;

        auto fn_idx = static_cast<u16>(current_chunk()->functions.size());
        current_chunk()->functions.push(ch);

        CompilerState state;
        state.chunk = ch;
        state.func_name = method->name->spelling;
        state.enclosing = m_current;
        state.is_class_method = true;
        state.class_field_names = field_names;
        state.class_layout_dynamic = s->get_parent() != nullptr && parent_desc == nullptr;
        state.class_method_names = method_names;
        CompilerStateGuard state_guard(m_current, &state);
        ScopeGuard scope(m_current);

        reg_t inst_reg;
        ALLOC_REG(&inst_reg);
        declare_local(kClassInstanceName, inst_reg, class_name);

        if (method->has_parameters()) {
            for (AST::Expr* p : method->params->elements) {
                auto* p_name = dynamic_cast<AST::IdentifierExpr*>(p);
                if (p_name == nullptr)
                    return report_error(ErrorCode::INVALID_FUNCTION_PARAMETER,
                        p ? p->get_location() : method_loc);

                reg_t reg;
                ALLOC_REG(&reg);
                declare_local(p_name->spelling, reg);
            }
        }

        COMPILE_STMT_DISCARD(method->body);
        if (!state.is_dead)
            emit(make_ABC(OpCode::RETURN, inst_reg, 1, 0), method_loc);

        ch->local_count = state.max_reg;
        state_guard.restore();

        reg_t dst;
        ALLOC_REG(&dst);
        emit(make_ABx(OpCode::CLOSURE, dst, fn_idx), method_loc);
        return std::tuple<reg_t, Chunk*> { dst, ch };
    };

    // Map a method name to its reserved special slot, or -1 if it's an
    // ordinary user method. This is the single source of truth for the
    // fixed-slot layout — both the slot-resolution pass and the vtable
    // build below must agree with it.
    auto special_slot_for = [](StringRef const& name) -> int {
        if (name == "بداية")
            return ObjClass::INIT;
        if (name == "نداء")
            return ObjClass::CALL;
        if (name == "عملية+")
            return ObjClass::ADD;
        if (name == "عملية-")
            return ObjClass::SUB;
        if (name == "عملية*")
            return ObjClass::MUL;
        if (name == "عملية/")
            return ObjClass::DIV;
        if (name == "عملية%" || name == "عملية٪")
            return ObjClass::MOD;
        if (name == "سالب")
            return ObjClass::NEG;
        if (name == "يساوي")
            return ObjClass::EQ;
        if (name == "لا_يساوي")
            return ObjClass::NEQ;
        if (name == "اصغر_من")
            return ObjClass::LT;
        if (name == "اصغر_او_يساوي")
            return ObjClass::LTE;
        if (name == "اكبر_من")
            return ObjClass::GT;
        if (name == "اكبر_او_يساوي")
            return ObjClass::GTE;
        if (name == "كتابة")
            return ObjClass::REPR;
        return -1;
    };

    // resolve every method's name -> vtable slot. No codegen
    // happens here. This has to fully finish before any method body
    // compiles: a method calling a sibling — forward, backward, or
    // itself — needs the complete name/slot table to resolve through
    // current_method_slot(), not just whatever happened to compile earlier.
    Array<StringRef> seen_names; // dedup guard across BOTH special and ordinary methods
    Array<int> method_slots;        // parallel to `methods`: final vtable slot per method

    for (AST::Stmt* m : methods) {
        if (m->get_kind() != AST::Stmt::Kind::FUNC)
            return report_error(ErrorCode::INVALID_STATEMENT_NODE, m->get_location());

        auto* method = as_function_def(m);
        StringRef method_name = method->name->spelling;

        bool seen = false;
        for (auto& existing : seen_names) {
            if (existing == method_name) {
                seen = true;
                break;
            }
        }

        if (seen)
            return report_error(ErrorCode::INVALID_STATEMENT_NODE, method->get_location());

        seen_names.push(method_name);

        int special = special_slot_for(method_name);
        if (special >= 0) {
            method_names[static_cast<u32>(special)] = method_name;
            method_slots.push(special);
        } else {
            int inherited_slot = -1;
            for (u32 i = 0; i < method_names.size(); ++i) {
                if (method_names[i] == method_name) {
                    inherited_slot = static_cast<int>(i);
                    break;
                }
            }
            if (inherited_slot >= 0) {
                method_slots.push(inherited_slot);
            } else {
                method_slots.push(static_cast<int>(method_names.size()));
                method_names.push(method_name);
            }
        }
    }

    // compile bodies. `method_names` is complete now, and
    // compile_method_closure captures it by reference, so every method
    // body — including e.g. بداية calling a sibling declared later in the
    // class — sees the full sibling table via current_method_slot(). ---
    Array<Chunk*> vtable(static_cast<u32>(method_names.size()), /* fill_v= */ nullptr);

    for (u32 i = 0, n = static_cast<u32>(methods.size()); i < n; i++) {
        auto* method = as_function_def(methods[i]);
        auto result = compile_method_closure(method);
        VERIFY_RESULT(result);
        auto [reg, chunk] = result.value();

        if (chunk == nullptr)
            continue;

        vtable[static_cast<u32>(method_slots[i])] = chunk;
    }

    // Build the descriptor from the same arrays already computed above.
    // vtable_indices[i] is the index into current_chunk()->functions[] of the
    // chunk that compile_method_closure() pushed there.  The parallel between
    // vtable[] (Chunk*) and current_chunk()->functions[] is exact because
    // compile_method_closure() does:
    //   auto fn_idx = static_cast<u16>(current_chunk()->functions.size());
    //   current_chunk()->functions.push(ch);
    // so we reconstruct those indices here by scanning for each chunk pointer.
    Array<u32> vtable_indices;
    for (u32 i = 0; i < vtable.size(); ++i) {
        if (vtable[i] == nullptr) {
            vtable_indices.push(ClassDescriptor::NULL_SLOT);
            continue;
        }
        u32 fn_idx = UINT32_MAX;
        for (u32 j = 0; j < current_chunk()->functions.size(); ++j) {
            if (current_chunk()->functions[j] == vtable[i]) {
                fn_idx = j;
                break;
            }
        }
        assert(fn_idx != UINT32_MAX && "vtable chunk not found in functions[]");
        vtable_indices.push(fn_idx);
    }

    ClassDescriptor desc_data;
    desc_data.name = StringRef(class_name);
    desc_data.parent_name = parent_name;
    desc_data.field_count = static_cast<u32>(own_field_names.size());
    desc_data.field_names = own_field_names;
    desc_data.vtable_size = static_cast<u32>(vtable.size());
    desc_data.method_names = method_names;
    desc_data.vtable_indices = std::move(vtable_indices);

    u16 desc_idx = current_chunk()->add_class_descriptor(std::move(desc_data));
    reg_t class_reg;

    ALLOC_REG(&class_reg);
    emit(make_ABx(OpCode::NEW_CLASS, class_reg, desc_idx), loc);
    u16 name_idx = intern_string(class_name);
    emit(make_ABx(OpCode::STORE_GLOBAL, class_reg, name_idx), loc);
    declare_local(class_name, class_reg);

    // ClassDesc registration — unchanged
    ClassDesc cdesc;
    cdesc.name = class_name;
    if (s->get_parent() == nullptr || parent_desc != nullptr)
        cdesc.field_names = field_names;
    cdesc.method_names = method_names;

    for (size_t i = 0; i < cdesc.field_names.size(); i++)
        cdesc.field_map[cdesc.field_names[i]] = static_cast<int>(i);
    for (size_t i = 0; i < method_names.size(); i++) {
        if (!method_names[i].empty())
            cdesc.method_map[method_names[i]] = static_cast<int>(i);
    }

    m_class_registry[class_name] = std::move(cdesc);
    return true;
}

ErrorOr<ExprResult> Compiler::compile_expr_impl(ConstExprPtr e)
{
    if (e == nullptr)
        return ExprResult::knil();

    if (AST::is_unary(e))
        return compile_unary_impl(AST::as_unary(e));

    if (AST::is_binary(e))
        return compile_binary_impl(AST::as_binary(e));

    switch (e->get_kind()) {
    case AST::Expr::Kind::INT_LITERAL: return compile_literal_int_impl(AST::as_literal_int(e));
    case AST::Expr::Kind::FLOAT_LITERAL: return compile_literal_float_impl(AST::as_literal_float(e));
    case AST::Expr::Kind::BOOL_LITERAL: return compile_literal_bool_impl(AST::as_literal_bool(e));
    case AST::Expr::Kind::STRING_LITERAL: return compile_literal_string_impl(AST::as_literal_string(e));
    case AST::Expr::Kind::NIL: return compile_nil_impl(AST::as_nil(e));
    case AST::Expr::Kind::IDENTIFIER: return compile_identifier_impl(AST::as_identifier(e));
    case AST::Expr::Kind::ASSIGNMENT: return compile_assign_impl(AST::as_assignment_expr(e));
    case AST::Expr::Kind::CALL: return compile_call_impl(AST::as_call(e), nullptr, false);
    case AST::Expr::Kind::LIST: return compile_list_impl(AST::as_list(e));
    case AST::Expr::Kind::DICT: return compile_dict_impl(AST::as_dict(e));
    case AST::Expr::Kind::INDEX_READ: return compile_index_impl(as_index(e));
    case AST::Expr::Kind::GET: return compile_get_impl_(as_get(e));
    default:
        return report_error(ErrorCode::INVALID_EXPRESSION_NODE, e->get_location());
    }

    return ExprResult::knil();
}

ErrorOr<ExprResult> Compiler::compile_literal_int_impl(AST::IntLiteralExpr const* e)
{
    return ExprResult::kint(e->value);
}
ErrorOr<ExprResult> Compiler::compile_literal_float_impl(AST::FloatLiteralExpr const* e)
{
    return ExprResult::kfloat(e->value);
}
ErrorOr<ExprResult> Compiler::compile_literal_bool_impl(AST::BoolLiteralExpr const* e)
{
    return ExprResult::kbool(e->value);
}
ErrorOr<ExprResult> Compiler::compile_literal_string_impl(AST::StringLiteralExpr const* e)
{
    u16 kidx = intern_string(e->str);
    u32 pc = emit(make_ABx(OpCode::LOAD_CONST, 0, kidx), e->get_location());
    return ExprResult::reloc(pc);
}
ErrorOr<ExprResult> Compiler::compile_nil_impl(AST::NilExpr const* e)
{
    (void)e;
    return ExprResult::knil();
}

ErrorOr<ExprResult> Compiler::compile_identifier_impl(AST::IdentifierExpr const* e)
{
    SourceLocation loc = e->get_location();
    VarInfo vi = resolve_name(e->spelling);

    if (vi.kind == VarInfo::Kind::LOCAL)
        return ExprResult::reg(vi.index);

    if (int field_idx = current_method_field_index(e->spelling); field_idx >= 0) {
        LocalVar const* self = lookup_local(kClassInstanceName);
        if (self == nullptr)
            return report_error(ErrorCode::INVALID_EXPRESSION_NODE, e->get_location());

        u32 pc = emit(make_ABC(OpCode::GET_FIELD, 0, self->reg, static_cast<reg_t>(field_idx)), loc);
        return ExprResult::reloc(pc);
    }

    u16 kidx = intern_string(e->spelling);
    u32 pc = emit(make_ABx(OpCode::LOAD_GLOBAL, 0, kidx), loc);
    return ExprResult::reloc(pc);
}

ErrorOr<ExprResult> Compiler::compile_unary_impl(AST::UnaryExpr const* e)
{
    SourceLocation loc = e->get_location();

    if (auto folded = try_fold_unary(e)) {
        Value v = *folded;
        if (v.is_int())
            return ExprResult::kint(v.as_int());
        if (v.is_double())
            return ExprResult::kfloat(v.as_double());
        if (v.is_bool())
            return ExprResult::kbool(v.as_bool());
        if (v.is_nil())
            return ExprResult::knil();
    }

    if (auto reduced = try_strength_reduce_unary(e))
        return compile_expr_impl(*reduced);

    OpCode op = OpCode::NOP;
    switch (e->get_kind()) {
    case AST::Expr::Kind::OP_NEG: op = OpCode::OP_NEG; break;
    case AST::Expr::Kind::OP_BITNOT: op = OpCode::OP_BITNOT; break;
    case AST::Expr::Kind::OP_NOT: op = OpCode::OP_NOT; break;
    default:
        return report_error(ErrorCode::UNKNOWN_UNARY_OPERATOR, e->get_location());
    }

    RegMark mark(m_current);
    ExprResult expr_result;
    COMPILE_EXPR_IMPL(e->operand, &expr_result);
    reg_t src;
    ANY_REG(expr_result, loc, &src);
    u32 pc = emit(make_ABC(op, 0, src, 0), loc);
    return ExprResult::reloc(pc);
}

ErrorOr<ExprResult> Compiler::compile_binary_impl(AST::BinaryExpr const* e)
{
    SourceLocation loc = e->get_location();

    if (auto folded = try_fold_binary(e)) {
        Value v = *folded;
        if (v.is_int())
            return ExprResult::kint(v.as_int());
        if (v.is_double())
            return ExprResult::kfloat(v.as_double());
        if (v.is_bool())
            return ExprResult::kbool(v.as_bool());
        if (v.is_nil())
            return ExprResult::knil();
    }

    if (auto reduced = try_strength_reduce_binary(e))
        return compile_expr_impl(*reduced);

    AST::Expr::Kind op = e->get_kind();
    if (op == AST::Expr::Kind::OP_AND) {
        reg_t dst;
        ALLOC_REG(&dst);

        {
            RegMark mark(m_current);
            ExprResult expr_result;
            COMPILE_EXPR_IMPL(e->lhs, &expr_result);
            discharge(expr_result, dst, loc);
        }

        u32 skip = emit_jump(OpCode::JUMP_IF_FALSE, dst, loc);

        {
            RegMark mark(m_current);
            ExprResult expr_result;
            COMPILE_EXPR_IMPL(e->rhs, &expr_result);
            discharge(expr_result, dst, loc);
        }

        patch_jump(skip);
        return ExprResult::reg(dst);
    }

    if (op == AST::Expr::Kind::OP_OR) {
        reg_t dst;
        ALLOC_REG(&dst);

        {
            RegMark mark(m_current);
            ExprResult expr_result;
            COMPILE_EXPR_IMPL(e->lhs, &expr_result);
            discharge(expr_result, dst, loc);
        }

        u32 skip = emit_jump(OpCode::JUMP_IF_TRUE, dst, loc);

        {
            RegMark mark(m_current);
            ExprResult expr_result;
            COMPILE_EXPR_IMPL(e->rhs, &expr_result);
            discharge(expr_result, dst, loc);
        }

        patch_jump(skip);
        return ExprResult::reg(dst);
    }

    OpCode bc_op = OpCode::NOP;
    bool swapped = false;

    switch (op) {
    case AST::Expr::Kind::OP_ADD: bc_op = OpCode::OP_ADD; break;
    case AST::Expr::Kind::OP_SUB: bc_op = OpCode::OP_SUB; break;
    case AST::Expr::Kind::OP_MUL: bc_op = OpCode::OP_MUL; break;
    case AST::Expr::Kind::OP_DIV: bc_op = OpCode::OP_DIV; break;
    case AST::Expr::Kind::OP_MOD: bc_op = OpCode::OP_MOD; break;
    case AST::Expr::Kind::OP_POW: bc_op = OpCode::OP_POW; break;
    case AST::Expr::Kind::OP_EQ: bc_op = OpCode::OP_EQ; break;
    case AST::Expr::Kind::OP_NEQ: bc_op = OpCode::OP_NEQ; break;
    case AST::Expr::Kind::OP_LT: bc_op = OpCode::OP_LT; break;
    case AST::Expr::Kind::OP_LTE: bc_op = OpCode::OP_LTE; break;
    case AST::Expr::Kind::OP_GT: bc_op = OpCode::OP_LT, swapped = true; break;
    case AST::Expr::Kind::OP_GTE: bc_op = OpCode::OP_LTE, swapped = true; break;
    case AST::Expr::Kind::OP_BITAND: bc_op = OpCode::OP_BITAND; break;
    case AST::Expr::Kind::OP_BITOR: bc_op = OpCode::OP_BITOR; break;
    case AST::Expr::Kind::OP_BITXOR: bc_op = OpCode::OP_BITXOR; break;
    case AST::Expr::Kind::OP_LSHIFT: bc_op = OpCode::OP_LSHIFT; break;
    case AST::Expr::Kind::OP_RSHIFT: bc_op = OpCode::OP_RSHIFT; break;
    default:
        return report_error(ErrorCode::UNKNOWN_BINARY_OPERATOR, e->get_location());
    }

    if ((bc_op == OpCode::OP_LSHIFT || bc_op == OpCode::OP_RSHIFT) && AST::is_literal_int(e->rhs)) {
        auto* amount_expr = AST::as_literal_int(e->rhs);
        i64 amount = amount_expr->value;
        if (amount < 0 || amount > 63)
            return report_error(ErrorCode::SHIFT_AMOUNT_OUT_OF_RANGE, amount_expr->get_location());

        RegMark mark(m_current);
        ExprResult expr_result;
        reg_t lhs;

        COMPILE_EXPR_IMPL(e->lhs, &expr_result);
        ANY_REG(expr_result, loc, &lhs);
        u32 pc = emit(make_ABC(bc_op, 0, lhs, static_cast<reg_t>(amount)), loc);
        reg_t ic = current_chunk()->alloc_ic_slot();
        emit(make_ABC(OpCode::NOP, ic, 0, 0), loc);
        return ExprResult::reloc(pc);
    }

    RegMark mark(m_current);
    ExprResult lhs_ret, rhs_ret;
    reg_t lhs, rhs;

    COMPILE_EXPR_IMPL(e->lhs, &lhs_ret);
    ANY_REG(lhs_ret, loc, &lhs);
    COMPILE_EXPR_IMPL(e->rhs, &rhs_ret);
    ANY_REG(rhs_ret, loc, &rhs);

    if (swapped)
        std::swap(lhs, rhs);

    u32 pc = emit(make_ABC(bc_op, 0, lhs, rhs), loc);
    reg_t ic = current_chunk()->alloc_ic_slot();
    emit(make_ABC(OpCode::NOP, ic, 0, 0), loc);
    return ExprResult::reloc(pc);
}

bool Compiler::is_declaration(AST::AssignExpr const* e) const
{
    if (!AST::is_identifier(e->target))
        return false;

    auto name = AST::as_identifier(e->target);
    if (lookup_local(name->spelling))
        return false;
    if (m_globals.find_ptr(name->spelling) != nullptr)
        return false;
    return true;
}

ErrorOr<ExprResult> Compiler::compile_assign_impl(AST::AssignExpr const* e)
{
    if (e == nullptr || e->target == nullptr || e->value == nullptr)
        return report_error(ErrorCode::INVALID_EXPRESSION_NODE,
            e ? e->get_location() : SourceLocation { });

    SourceLocation loc = e->get_location();

    // Indexed assignment: evaluate the object and index before the value,
    // then write the resulting value into the computed location.
    if (AST::is_index(e->target)) {
        auto index_expr = as_index(e->target);
        RegMark mark(m_current);
        ExprResult object_expr_result, index_expr_result, value_expr_result;
        reg_t target_object_reg, index_reg, value_reg;

        COMPILE_EXPR_IMPL(index_expr->object, &object_expr_result);
        ANY_REG(object_expr_result, loc, &target_object_reg);
        COMPILE_EXPR_IMPL(index_expr->index, &index_expr_result);
        ANY_REG(index_expr_result, loc, &index_reg);
        COMPILE_EXPR_IMPL(e->value, &value_expr_result);
        ANY_REG(value_expr_result, loc, &value_reg);

        emit(make_ABC(OpCode::INDEX_WRITE, target_object_reg, index_reg, value_reg), loc);
        return ExprResult::reg(value_reg);
    }

    // Member assignment. Prefer a statically known field slot, but retain a
    // named payload for receivers whose class is only known at runtime.
    if (AST::is_get(e->target)) {
        auto get_expr = as_get(e->target);
        auto* member_name = AST::as_identifier(get_expr->member);
        if (member_name == nullptr)
            return report_error(ErrorCode::INVALID_EXPRESSION_NODE, loc);

        int field_idx = -1;
        if (ClassDesc const* desc = resolve_receiver_class(get_expr->object))
            field_idx = desc->field_index(member_name->spelling);
        if (field_idx < 0 && is_this_reference(get_expr->object))
            field_idx = current_method_field_index(member_name->spelling);

        RegMark mark(m_current);
        ExprResult object_expr_result, value_expr_result;
        reg_t object_reg, value_reg;

        COMPILE_EXPR_IMPL(get_expr->object, &object_expr_result);
        ANY_REG(object_expr_result, loc, &object_reg);
        COMPILE_EXPR_IMPL(e->value, &value_expr_result);
        ANY_REG(value_expr_result, loc, &value_reg);

        if (field_idx >= 0) {
            emit(make_ABC(OpCode::SET_FIELD, object_reg,
                     static_cast<reg_t>(field_idx), value_reg),
                loc);
        } else {
            emit(make_ABC(OpCode::SET_FIELD, object_reg, 0xFF, value_reg), loc);
            emit(make_ABx(OpCode::NOP, 0,
                     static_cast<u16>(intern_string(member_name->spelling))),
                loc);
        }
        return ExprResult::reg(value_reg);
    }

    // Name assignment: write into an existing local, create a local inside a
    // function/method, or store a top-level name globally.
    if (!AST::is_identifier(e->target))
        return report_error(ErrorCode::INVALID_EXPRESSION_NODE, loc);

    StringRef const& name_value = AST::as_identifier(e->target)->spelling;

    if (is_declaration(e)) {
        if (m_current->is_top_level && m_current->scope_depth == 0
            && infer_constructed_class(e->value).empty()) {
            RegMark mark(m_current);
            ExprResult value_result;
            reg_t src;

            COMPILE_EXPR_IMPL(e->value, &value_result);
            ANY_REG(value_result, loc, &src);
            emit(make_ABx(OpCode::STORE_GLOBAL, src,
                     static_cast<u16>(intern_string(name_value))),
                loc);
            m_globals[name_value] = true;
            return ExprResult::reg(src);
        }

        reg_t dst;
        ALLOC_REG(&dst);

        ExprResult value_result;
        COMPILE_EXPR_IMPL(e->value, &value_result);
        discharge(value_result, dst, loc);
        declare_local(name_value, dst, infer_constructed_class(e->value));
        return ExprResult::reg(dst);
    }

    // In a method, an assignment to a parameter/local that shares a field
    // name targets the instance field. The value expression still resolves
    // the local normally (for example, `x := x` in an initializer).
    if (int field_idx = current_method_field_index(name_value); field_idx >= 0) {
        LocalVar const* self = lookup_local(kClassInstanceName);
        if (self == nullptr)
            return report_error(ErrorCode::INVALID_EXPRESSION_NODE, loc);

        RegMark mark(m_current);
        ExprResult value_result;
        reg_t value_reg;
        COMPILE_EXPR_IMPL(e->value, &value_result);
        ANY_REG(value_result, loc, &value_reg);
        emit(make_ABC(OpCode::SET_FIELD, self->reg,
                 static_cast<reg_t>(field_idx), value_reg),
            loc);
        return ExprResult::reg(value_reg);
    }

    if (LocalVar const* local = lookup_local(name_value)) {
        reg_t dst = local->reg;
        auto result = compile_expr(e->value, &dst);
        VERIFY_RESULT(result);
        return ExprResult::reg(dst);
    }

    // Assignment inside a function introduces a local even when a global of
    // the same name already exists. Globals are only written at top level.
    if (!m_current->is_top_level) {
        reg_t dst;
        ALLOC_REG(&dst);

        ExprResult value_result;
        COMPILE_EXPR_IMPL(e->value, &value_result);
        discharge(value_result, dst, loc);
        declare_local(name_value, dst, infer_constructed_class(e->value));
        return ExprResult::reg(dst);
    }

    RegMark mark(m_current);
    ExprResult value_result;
    COMPILE_EXPR_IMPL(e->value, &value_result);
    reg_t src;
    ANY_REG(value_result, loc, &src);

    emit(make_ABx(OpCode::STORE_GLOBAL, src,
             static_cast<u16>(intern_string(name_value))),
        loc);
    m_globals[name_value] = true;
    return ExprResult::reg(src);
}

ErrorOr<ExprResult> Compiler::compile_call_impl(AST::CallExpr const* e, reg_t* dst, bool tail)
{
    SourceLocation loc = e->get_location();
    auto fn_reg_ret = dst == nullptr ? alloc_register() : *dst;
    VERIFY_RESULT(fn_reg_ret);
    reg_t fn_reg = fn_reg_ret.value();

    auto compile_args = [&]() -> ErrorOr<bool> {
        for (ConstExprPtr arg : e->args->elements) {
            reg_t arg_reg;
            ExprResult arg_cmp_ret;
            ALLOC_REG(&arg_reg);
            COMPILE_EXPR_IMPL(arg, &arg_cmp_ret);
            discharge(arg_cmp_ret, arg_reg, arg->get_location());
            m_current->free_regs_to(arg_reg + 1);
        }
        return true;
    };

    bool module_member_call = false;
    if (AST::is_get(e->callee)) {
        ConstExprPtr object = AST::as_get(e->callee)->object;
        module_member_call = AST::is_identifier(object)
            && m_module_names.find_ptr(AST::as_identifier(object)->spelling) != nullptr;
    }

    /// calling a method (module attributes are ordinary callable values)
    if (AST::is_get(e->callee) && !module_member_call) {
        AST::GetExpr const* get_expr = AST::as_get(e->callee);
        /// NOTE: we don't have to verify if this is in fact a method call
        /// and not a semantic error of calling a non-callable plain field
        /// because the parser already will enforce this for us
        ConstExprPtr object = get_expr->object;
        ConstExprPtr member = get_expr->member;

        if (AST::is_identifier(object) && AST::as_identifier(object)->spelling == kClassInstanceName) {
            /// internal method call 'this.method(implicit this, ...)'
            auto method_name = AST::as_identifier(member)->spelling;
            int slot = current_method_slot(method_name);
            if (slot >= 0) {
                reg_t object_reg;
                reg_t reserved_reg;
                ExprResult object_cmp_ret;

                ALLOC_REG(&object_reg);
                COMPILE_EXPR_IMPL(object, &object_cmp_ret);
                discharge(object_cmp_ret, object_reg, object->get_location());
                // Nested receiver expressions may temporarily allocate well above
                // object_reg. Method calling convention requires self and explicit
                // arguments to occupy consecutive registers, so discard those
                // temporaries before reserving the self slot and arguments.
                m_current->free_regs_to(object_reg + 1);
                ALLOC_REG(&reserved_reg);

                for (AST::Expr* arg : e->args->elements) {
                    reg_t arg_reg;
                    ExprResult arg_cmp_ret;
                    ALLOC_REG(&arg_reg);
                    COMPILE_EXPR_IMPL(arg, &arg_cmp_ret);
                    discharge(arg_cmp_ret, arg_reg, loc);
                }

                u8 argc = static_cast<u8>(e->args->size() + 1);
                emit(make_ABC(OpCode::INVOKE, object_reg, static_cast<reg_t>(slot), argc), loc);
                emit(make_ABC(OpCode::NOP, current_chunk()->alloc_ic_slot(), 0, 0), loc);

                if (tail && !m_current->is_top_level)
                    emit(make_ABC(OpCode::RETURN, object_reg, 1, 0), loc);

                m_current->free_regs_to(object_reg + 1);
                return ExprResult::reg(object_reg);
            }
            // A member stored in a field can itself be callable. When no
            // method slot exists, load the member value and use the ordinary
            // CALL path instead of rejecting valid higher-order code.
            ExprResult callee_cmp_ret;
            COMPILE_EXPR_IMPL(e->callee, &callee_cmp_ret);
            discharge(callee_cmp_ret, fn_reg, loc);
            m_current->free_regs_to(fn_reg + 1);
        } else {
            /// external method call 'obj.method()'
            ExprResult object_cmp_ret;
            ExprResult member_cmp_ret;
            reg_t object_reg;
            reg_t member_reg;

            ALLOC_REG(&object_reg);
            COMPILE_EXPR_IMPL(object, &object_cmp_ret);
            discharge(object_cmp_ret, object_reg, object->get_location());
            // Keep the runtime call frame contiguous even when `object` is itself
            // a call such as value.first().second(arg).
            m_current->free_regs_to(object_reg + 1);
            ALLOC_REG(&member_reg);

            if (AST::is_identifier(member)) {
                /// if it's a simple name then load it from the constant table
                emit(make_ABx(OpCode::LOAD_CONST, member_reg, intern_string(AST::as_identifier(member)->spelling)),
                    member->get_location());
            } else {
                /// compile complex member expression
                COMPILE_EXPR_IMPL(member, &member_cmp_ret);
                discharge(member_cmp_ret, member_reg, member->get_location());
            }

            auto args_cmp_ret = compile_args();
            VERIFY_RESULT(args_cmp_ret);

            u8 argc = static_cast<u8>(e->args->size() + 1); // +1 for implicit 'this'
            /// emit INVOKE_NAMED to invoke this method by it's name from the vtable of the instance class
            u16 idx = static_cast<u16>(intern_string(AST::as_identifier(member)->spelling));
            emit(make_ABC(OpCode::INVOKE_NAMED, object_reg, 0, argc), loc);
            emit(make_ABx(OpCode::NOP, 0, idx), loc);
            if (tail && !m_current->is_top_level)
                emit(make_ABC(OpCode::RETURN, object_reg, 1, 0), loc);
            /// move the cursor back where it was before compiling 'member'
            m_current->free_regs_to(object_reg + 1);
            return ExprResult::reg(object_reg);
        }
    } else {
        /// plain function call 'expr()'
        ExprResult callee_cmp_ret;
        COMPILE_EXPR_IMPL(e->callee, &callee_cmp_ret);
        discharge(callee_cmp_ret, fn_reg, loc);
        // A computed callee can leave temporary registers live. Arguments must
        // begin immediately after fn_reg for CALL/IC_CALL.
        m_current->free_regs_to(fn_reg + 1);
    }

    auto args_cmp_ret = compile_args();
    VERIFY_RESULT(args_cmp_ret);

    u8 argc = static_cast<u8>(e->args->size());
    if (tail && !m_current->is_top_level) {
        emit(make_ABC(OpCode::CALL_TAIL, fn_reg, argc, 0), loc);
        m_current->free_regs_to(fn_reg);
        return ExprResult::reg(fn_reg);
    }

    reg_t ic = current_chunk()->alloc_ic_slot();
    emit(make_ABC(OpCode::IC_CALL, fn_reg, argc, ic), loc);
    m_current->free_regs_to(fn_reg + 1);
    return ExprResult::reg(fn_reg);
}

ErrorOr<ExprResult> Compiler::compile_list_impl(AST::ListExpr const* e)
{
    SourceLocation loc = e->get_location();

    if (e->size() > 0xFF)
        return report_error(ErrorCode::TOO_MANY_LIST_ELEMENTS, loc);

    reg_t dst;
    ALLOC_REG(&dst);

    reg_t list_reg;
    ALLOC_REG(&list_reg);
    auto cap = static_cast<reg_t>(e->size());
    emit(make_ABC(OpCode::LIST_NEW, list_reg, cap, 0), loc);

    for (AST::Expr* elem : e->elements) {
        ExprResult expr_result;
        reg_t reg;
        ALLOC_REG(&reg);
        COMPILE_EXPR_IMPL(elem, &expr_result);
        discharge(expr_result, reg, loc);
        emit(make_ABC(OpCode::LIST_APPEND, list_reg, reg, 0), loc);
        m_current->free_regs_to(reg);
    }

    if (list_reg != dst)
        emit(make_ABC(OpCode::MOVE, dst, list_reg, 0), loc);

    m_current->free_regs_to(dst + 1);
    return ExprResult::reg(dst);
}

ErrorOr<ExprResult> Compiler::compile_index_impl(AST::IndexExpr const* e)
{
    SourceLocation loc = e->get_location();
    RegMark mark(m_current);
    ExprResult object_expr_result, index_expr_result;
    reg_t object_reg, index_reg;
    COMPILE_EXPR_IMPL(e->object, &object_expr_result);
    ANY_REG(object_expr_result, loc, &object_reg);
    COMPILE_EXPR_IMPL(e->index, &index_expr_result);
    ANY_REG(index_expr_result, loc, &index_reg);
    u32 pc = emit(make_ABC(OpCode::INDEX_READ, 0, object_reg, index_reg), loc);
    return ExprResult::reloc(pc);
}

ErrorOr<ExprResult> Compiler::compile_dict_impl(AST::DictExpr const* e)
{
    SourceLocation loc = e->get_location();

    reg_t dst;
    ALLOC_REG(&dst); // reserve the expression's result register FIRST

    reg_t fn_reg;
    ALLOC_REG(&fn_reg);
    u16 kidx = intern_string("قاموس");
    emit(make_ABx(OpCode::LOAD_GLOBAL, fn_reg, kidx), loc);

    for (auto const& [key, value] : e->get_content()) {
        reg_t key_reg;
        ExprResult expr_result;
        ALLOC_REG(&key_reg);
        COMPILE_EXPR_IMPL(key, &expr_result);
        discharge(expr_result, key_reg, key->get_location());
        m_current->free_regs_to(key_reg + 1);

        reg_t value_reg;
        ExprResult val_expr_result;
        ALLOC_REG(&value_reg);
        COMPILE_EXPR_IMPL(value, &val_expr_result);
        discharge(val_expr_result, value_reg, value->get_location());
        m_current->free_regs_to(value_reg + 1);
    }

    reg_t argc = static_cast<reg_t>(e->get_content().size() * 2);
    reg_t ic = current_chunk()->alloc_ic_slot();
    emit(make_ABC(OpCode::IC_CALL, fn_reg, argc, ic), loc);

    if (fn_reg != dst)
        emit(make_ABC(OpCode::MOVE, dst, fn_reg, 0), loc);

    m_current->free_regs_to(dst + 1);
    return ExprResult::reg(dst);
}

ErrorOr<ExprResult> Compiler::compile_get_impl_(AST::GetExpr const* e)
{
    SourceLocation loc = e->get_location();
    /// the parser should guarantee that this is an IdentifierExpr
    if (AST::is_identifier(e->object) && AST::as_identifier(e->object)->spelling == kClassInstanceName) {
        int idx = current_method_field_index(e->member->spelling);
        LocalVar const* self = lookup_local(kClassInstanceName);
        if (self != nullptr) {
            if (idx >= 0) {
                u32 pc = emit(make_ABC(OpCode::GET_FIELD, 0, self->reg, static_cast<u8>(idx)), loc);
                return ExprResult::reloc(pc);
            }
            u32 name_idx = intern_string(e->member->spelling);
            u32 pc = emit(make_ABC(OpCode::GET_FIELD, 0, self->reg, 0xFF), loc);
            emit(make_ABx(OpCode::NOP, 0, static_cast<u16>(name_idx)), loc);
            return ExprResult::reloc(pc);
        }
        return report_error(ErrorCode::UNDEFINED_LOCAL, e->member->get_location());
    }

    u32 name_idx = intern_string(e->member->spelling);
    reg_t object_reg;
    ExprResult object_cmp_ret;
    ALLOC_REG(&object_reg);
    COMPILE_EXPR_IMPL(e->object, &object_cmp_ret);
    discharge(object_cmp_ret, object_reg, e->object->get_location());
    u32 pc = emit(make_ABC(OpCode::GET_FIELD, 0, object_reg, 0xFF), loc);
    emit(make_ABx(OpCode::NOP, 0, static_cast<u16>(name_idx)), loc);
    return ExprResult::reloc(pc);
}

ErrorOr<ExprResult> Compiler::compile_get_impl(AST::GetExpr const* e)
{
    SourceLocation loc = e->get_location();

    if (ClassDesc const* desc = resolve_receiver_class(e->object)) {
        int idx = desc->field_index(e->member->spelling);
        if (idx >= 0) {
            RegMark mark(m_current);
            ExprResult expr_result;
            reg_t obj_reg;

            COMPILE_EXPR_IMPL(e->object, &expr_result);
            ANY_REG(expr_result, loc, &obj_reg);

            u32 pc = emit(make_ABC(OpCode::GET_FIELD, 0, obj_reg, static_cast<reg_t>(idx)), loc);
            return ExprResult::reloc(pc);
        }
        // Name matches the class but isn't a field — could be a bound
        // method reference; fall through to the slow path below.
    }

    // `this.field` read inside the class's own method body, while that
    // class is still being compiled — same m_class_registry-not-yet-
    // populated situation as compile_assign_impl/compile_assignment_stmt.
    // Use current_method_field_index() instead, which was pre-populated
    // before any method body started compiling.
    if (is_this_reference(e->object)) {
        int idx = current_method_field_index(e->member->spelling);
        if (idx >= 0) {
            LocalVar const* self = lookup_local(kClassInstanceName);
            if (self != nullptr) {
                u32 pc = emit(make_ABC(OpCode::GET_FIELD, 0, self->reg, static_cast<reg_t>(idx)), loc);
                return ExprResult::reloc(pc);
            }
        }
    }

    RegMark mark(m_current);
    ExprResult object_expr_result;
    reg_t object_reg;

    COMPILE_EXPR_IMPL(e->object, &object_expr_result);
    ANY_REG(object_expr_result, loc, &object_reg);

    reg_t member_reg;
    ALLOC_REG(&member_reg);

    emit(make_ABx(OpCode::LOAD_CONST, member_reg,
                intern_string(e->member->spelling)),
        e->member->get_location());

    u32 pc = emit(make_ABC(OpCode::INDEX_READ, 0, object_reg, member_reg), loc);
    return ExprResult::reloc(pc);
}

void Compiler::discharge(ExprResult const& r, reg_t dst, SourceLocation loc)
{
    switch (r.kind) {
    case ExprResult::Kind::REG:
        if (r.reg_ != dst)
            emit(make_ABC(OpCode::MOVE, dst, r.reg_, 0), loc);
        break;
    case ExprResult::Kind::RELOC: patch_a(current_chunk(), r.reloc_pc, dst); break;
    case ExprResult::Kind::KINT: emit_load_value(dst, Value::from_int(r.ival), loc); break;
    case ExprResult::Kind::KFLOAT: emit_load_value(dst, Value::from_real(r.dval), loc); break;
    case ExprResult::Kind::KBOOL: emit_load_value(dst, Value::from_bool(r.bval), loc); break;
    case ExprResult::Kind::KNIL: emit_load_value(dst, Value::nil(), loc); break;
    }
}

ErrorOr<reg_t> Compiler::any_reg(ExprResult const& r, SourceLocation loc)
{
    if (r.kind == ExprResult::Kind::REG)
        return r.reg_;

    reg_t dst;
    ALLOC_REG(&dst);
    discharge(r, dst, loc);
    return dst;
}

ErrorOr<reg_t> Compiler::compile_expr(ConstExprPtr e, reg_t* dst)
{
    if (e == nullptr)
        /// TODO: report error
        return 0;

    if (dst != nullptr)
        reserve_register(*dst);

    SourceLocation loc = e->get_location();
    ExprResult r;
    COMPILE_EXPR_IMPL(e, &r);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

LocalVar const* Compiler::lookup_local(StringRef const& name) const
{
    auto const& locals = m_current->locals;
    for (auto i = static_cast<int>(locals.size()) - 1; i >= 0; i -= 1) {
        if (locals[i].name == name)
            return &locals[i];
    }

    return nullptr;
}

Compiler::VarInfo Compiler::resolve_name(StringRef const& name)
{
    if (LocalVar const* local = lookup_local(name))
        return { VarInfo::Kind::LOCAL, local->reg };

    return VarInfo {
        .kind = VarInfo::Kind::GLOBAL,
        .index = 0
    };
}

void Compiler::pop_loop(u32 loop_exit, u32 continue_target, u32 line)
{
    (void)line;
    assert(!m_current->loop_stack.empty());
    auto& ctx = m_current->loop_stack.back();

    for (u32 idx : ctx.break_patches)
        patch_jump_to(idx, loop_exit);
    for (u32 idx : ctx.continue_patches)
        patch_jump_to(idx, continue_target);

    m_current->loop_stack.pop();
}

void Compiler::patch_jump_to(u32 instr_idx, u32 target)
{
    auto offset = static_cast<i32>(target) - static_cast<i32>(instr_idx) - 1;
    if (offset > JUMP_OFFSET || offset < -JUMP_OFFSET)
        diagnostic::panic(ErrorCode::LOOP_JUMP_OFFSET_OVERFLOW);

    u32 word = current_chunk()->code[instr_idx];
    current_chunk()->code[instr_idx] = make_AsBx(instr_op(word), instr_A(word), offset);
}

void Compiler::emit_load_value(reg_t dst, Value v, SourceLocation loc)
{
    if (v.is_nil()) {
        emit(make_ABC(OpCode::LOAD_NIL, dst, dst, 1), loc);
        return;
    }

    if (v.is_bool()) {
        emit(make_ABC(v.as_bool() ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE, dst, 0, 0), loc);
        return;
    }

    if (v.is_int()) {
        i64 iv = v.as_int();
        if (iv >= -JUMP_OFFSET && iv <= JUMP_OFFSET) {
            emit(make_ABx(OpCode::LOAD_INT, dst, static_cast<u16>(iv + JUMP_OFFSET)), loc);
            return;
        }
    }

    emit(make_ABx(OpCode::LOAD_CONST, dst, current_chunk()->add_constant(v)), loc);
}

u32 Compiler::intern_string(StringRef const& str)
{
    Chunk* chunk = current_chunk();
    auto key = std::make_pair(str, chunk);
    if (u16* idx = m_string_cache.find_ptr(key))
        return *idx;

    ObjString* obj = get_allocator().allocate_object<ObjString>();
    obj->str = str;
    u16 idx = chunk->add_constant(Value::from_obj(reinterpret_cast<ObjHeader*>(obj)));
    m_string_cache[key] = idx;
    return idx;
}

Compiler::ClassDesc const* Compiler::resolve_receiver_class(ConstExprPtr e) const
{
    using EK = AST::Expr::Kind;
    if (!AST::is_identifier(e))
        return nullptr;

    StringRef const name = AST::as_identifier(e)->spelling;

    // Case 1: the expression IS the class name itself (e.g. كلب.بداية()).
    if (auto* d = m_class_registry.find_ptr(name))
        return d;

    // Case 2: the expression is a local variable known to hold an
    // instance of some class (e.g. obj after obj = كلب.بداية()).
    if (LocalVar const* local = lookup_local(name)) {
        if (!local->known_class.empty()) {
            if (auto* d = m_class_registry.find_ptr(local->known_class))
                return d;
        }
    }

    return nullptr;
}

StringRef Compiler::infer_constructed_class(ConstExprPtr e) const
{
    if (e == nullptr || e->get_kind() != AST::Expr::Kind::CALL)
        return "";

    auto const* call = as_call(e);
    if (call->callee == nullptr || !AST::is_identifier(call->callee))
        return "";

    StringRef name = AST::as_identifier(call->callee)->spelling;
    return m_class_registry.find_ptr(name) != nullptr ? name : StringRef { "" };
}

int Compiler::current_method_slot(StringRef const& name) const
{
    if (m_current == nullptr || !m_current->is_class_method)
        return -1;

    for (u32 i = 0, n = m_current->class_method_names.size(); i < n; i++) {
        if (m_current->class_method_names[i] == name)
            return static_cast<int>(i);
    }

    return -1;
}

int Compiler::current_method_field_index(StringRef const& name) const
{
    if (m_current == nullptr || !m_current->is_class_method || m_current->class_layout_dynamic)
        return -1;

    for (u32 i = 0, n = m_current->class_field_names.size(); i < n; i++) {
        if (m_current->class_field_names[i] == name)
            return static_cast<int>(i);
    }

    return -1;
}

} // namespace fairuz::runtime
