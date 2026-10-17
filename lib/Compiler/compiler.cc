//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Fairuz register-based bytecode compiler implementation.
//===----------------------------------------------------------------------===//

#include "Compiler/compiler.hpp"
#include "Diag/runtime_error.hpp"
#include "Macros.hpp"
#include "compile_macros.hpp"

namespace fairuz::runtime {

namespace {

static void patch_a(Chunk* chunk, u32 pc, u8 a)
{
    u32 instr = chunk->code[pc];
    chunk->code[pc] = (instr & 0xFF00FFFFu) | (static_cast<u32>(a) << 16);
}

static bool is_terminal_top_level_call(AST::StmtPtr s)
{
    auto const* expr_stmt = dynamic_cast<AST::ExprStmt const*>(s);
    if (expr_stmt == nullptr)
        return false;

    return dynamic_cast<AST::CallExpr const*>(expr_stmt->expr) != nullptr;
}

} // anonymous namespace

ErrorOr<Chunk*, RuntimeError> Compiler::compile(Array<AST::StmtPtr> const& stmts)
{
    Chunk* chunk = make_chunk();
    chunk->name = MAIN_PROGRAM;
    chunk->source = m_source;
    chunk->source_path = m_source->path;

    CompilerState state { chunk, MAIN_PROGRAM, true, nullptr };
    CompilerStateGuard state_guard(m_current, &state);

    for (size_t i = 0; i < stmts.size(); i++) {
        AST::StmtPtr stmt = stmts[i];
        if (i + 1 == stmts.size() && stmt && !state.is_dead && is_terminal_top_level_call(stmt)) {
            auto const* expr_stmt = as_expr_stmt(stmt);
            SourceLocation loc = expr_stmt->get_location();
            RegMark mark(m_current);
            FA_COMPILE_TRY(expr_result, compile_expr_impl(expr_stmt->expr));
            ANY_REG_BIND(expr_result, loc, src);
            emit(make_ABC(OpCode::RETURN, src, 1, 0), loc);
            state.is_dead = true;
            break;
        }
        FA_COMPILE_DISCARD_TRY(stmt_result, compile_stmt(stmt));
    }

    SourceLocation loc = { 1, 1, 0 };
    if (!stmts.empty() && stmts.back())
        loc = stmts.back()->get_location();

    if (!state.is_dead)
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);

    chunk->local_count = state.max_reg;
    state_guard.restore();

    if (diagnostic::has_errors())
        diagnostic::dump();
    return chunk;
}

ErrorOr<bool, RuntimeError> Compiler::compile_all(Chunk* chunk)
{
    auto compile_result = compile_function_deferred(chunk);
    if (compile_result.has_error())
        return compile_result.error();
    for (Chunk* function : chunk->functions) {
        auto comp_all = compile_all(function);
        if (comp_all.has_error())
            return comp_all.error();
    }
    return true;
}

void Compiler::discharge(ExprResult const& r, RegType dst, SourceLocation loc)
{
    switch (r.kind) {
    case ExprResult::Kind::REG:
        if (r.reg_ != dst)
            emit(make_ABC(OpCode::MOVE, dst, r.reg_, 0), loc);
        return;
    case ExprResult::Kind::RELOC: patch_a(current_chunk(), r.reloc_pc, dst); return;
    case ExprResult::Kind::KINT: {
#if FA_USE_NANBOX
        /// r.ival only holds 64 bit int, so this is only applicable on NAN-BOXed Value
        if (r.ival > Value::int_max() || r.ival < Value::int_min()) {
            emit(make_ABx(OpCode::LOAD_BIG_INT, dst, current_chunk()->add_big_int(r.ival)), loc);
            return;
        }
#endif
        emit_load_value(dst, Value::from_int(r.ival), loc);
        return;
    }
    case ExprResult::Kind::KFLOAT: emit_load_value(dst, Value::from_real(r.dval), loc); return;
    case ExprResult::Kind::KBOOL: emit_load_value(dst, Value::from_bool(r.bval), loc); return;
    case ExprResult::Kind::KNIL: emit_load_value(dst, Value::nil(), loc); return;
    }
}

void Compiler::emit_load_value(u8 dst, Value v, SourceLocation loc)
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

ErrorOr<Compiler::RegType, RuntimeError> Compiler::any_reg(ExprResult const& r, SourceLocation loc)
{
    if (r.kind == ExprResult::Kind::REG)
        return r.reg_;

    auto allocated = alloc_register();
    if (allocated.has_error())
        return raise_compile_error(allocated.error().errc, loc);
    RegType dst = allocated.value();
    discharge(r, dst, loc);
    return dst;
}

ErrorOr<Compiler::RegType, RuntimeError> Compiler::alloc_register()
{
    u8 reg = m_current->alloc_register();
    if (reg >= MAX_REGS)
        return raise_compile_error(ErrorCode::TOO_MANY_REGISTERS, { });
    return reg;
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

StringRef Compiler::infer_constructed_class(AST::ExprPtr e) const
{
    if (e->get_kind() != AST::ExprKind::CALL)
        return "";

    auto const* call = as_call(e);
    if (call->callee == nullptr || !AST::is_identifier(call->callee))
        return "";

    StringRef name = AST::as_identifier(call->callee)->spelling;
    return m_class_registry.find_ptr(name) != nullptr ? name : StringRef { "" };
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

void Compiler::reserve_register(RegType r)
{
    if (r >= m_current->next_reg) {
        m_current->next_reg = r + 1;
        if (m_current->next_reg > m_current->max_reg)
            m_current->max_reg = m_current->next_reg;
    }
}

} // namespace fairuz::runtime
