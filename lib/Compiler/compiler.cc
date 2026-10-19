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
#include "Compiler/opcode.hpp"
#include "Diag/runtime_error.hpp"
#include "Macros.hpp"
#include "compile_macros.hpp"

namespace fairuz::runtime {

namespace {

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
            m_bytecode_emitter.emit_ABC(OpCode::RETURN, src.value, 1, 0, loc);
            state.is_dead = true;
            break;
        }
        FA_COMPILE_DISCARD_TRY(compile_stmt(stmt));
    }

    SourceLocation loc = { 1, 1, 0 };
    if (!stmts.empty() && stmts.back())
        loc = stmts.back()->get_location();

    if (!state.is_dead)
        m_bytecode_emitter.return_nil(loc);

    chunk->local_count = state.registers.max_reg();
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

void Compiler::pop_loop(PCType loop_exit, PCType continue_target, u32 line)
{
    (void)line;
    assert(!m_current->loop_stack.empty());
    auto& ctx = m_current->loop_stack.back();

    for (PCType idx : ctx.break_patches)
        m_bytecode_emitter.patch_jump_to(idx, loop_exit);
    for (PCType idx : ctx.continue_patches)
        m_bytecode_emitter.patch_jump_to(idx, continue_target);

    m_current->loop_stack.pop();
}

ErrorOr<RegType, RuntimeError> Compiler::any_reg(ExprResult const& r, SourceLocation loc)
{
    if (r.kind == ExprResult::Kind::REG)
        return r.reg_;

    auto allocated = alloc_register(loc);
    if (allocated.has_error())
        return raise_compile_error(allocated.error().errc, loc);
    RegType dst = allocated.value();
    m_bytecode_emitter.discharge(r, dst, loc);
    return dst;
}

ErrorOr<RegType, RuntimeError> Compiler::alloc_register(SourceLocation loc)
{
    auto alloc_result = m_current->registers.alloc_register();
    if (alloc_result.has_error())
        return raise_compile_error(alloc_result.error().get_code(), loc);
    return alloc_result.value();
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
        .reg = RegType { 0 }
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

ConstantIndex Compiler::intern_string(StringRef const& str)
{
    Chunk* chunk = current_chunk();
    auto key = std::make_pair(str, chunk);
    if (ConstantIndex* idx = m_string_cache.find_ptr(key))
        return *idx;

    ObjString* obj = get_allocator().allocate_object<ObjString>();
    obj->str = str;
    ConstantIndex idx = chunk->add_constant(Value::from_obj(reinterpret_cast<ObjHeader*>(obj)));
    m_string_cache[key] = idx;
    return idx;
}

} // namespace fairuz::runtime
