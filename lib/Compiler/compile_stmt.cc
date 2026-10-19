//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Compiles Fairuz statement AST nodes into bytecode.
//===----------------------------------------------------------------------===//

#include "Compiler/chunk.hpp"
#include "Compiler/compiler.hpp"
#include "Compiler/opcode.hpp"
#include "Diag/runtime_error.hpp"
#include "Macros.hpp"
#include "Parser/ast_builder.hpp"
#include "compile_macros.hpp"
#include <cassert>
#include <codecvt>

namespace fairuz::runtime {

Compiler::CompileStmtResult Compiler::compile_stmt(AST::StmtPtr s)
{
    if (m_current->is_dead)
        return { };

    switch (s->get_kind()) {
    case AST::Stmt::Kind::EXPR: return compile_expr_stmt(AST::as_expr_stmt(s));
    case AST::Stmt::Kind::IF: return compile_if(as_if(s));
    case AST::Stmt::Kind::WHILE: return compile_while(as_while(s));
    case AST::Stmt::Kind::FOR: return compile_for(as_for(s));
    case AST::Stmt::Kind::FUNC: return compile_function_def(as_function_def(s));
    case AST::Stmt::Kind::FUNCTION_STUB: return { };
    case AST::Stmt::Kind::RETURN: return compile_return(as_return(s));
    case AST::Stmt::Kind::BREAK: return compile_break(as_break(s));
    case AST::Stmt::Kind::CONTINUE: return compile_continue(as_continue(s));
    case AST::Stmt::Kind::BLOCK: return compile_block(as_block(s));
    case AST::Stmt::Kind::CLASS_DEF: return compile_class_def(as_class_def(s));
    case AST::Stmt::Kind::IMPORT: return compile_import(as_import(s));
    case AST::Stmt::Kind::INVALID:
        return raise_compile_error(ErrorCode::INVALID_STATEMENT_NODE, s->get_location());
    }
    return raise_compile_error(ErrorCode::INVALID_STATEMENT_NODE, s->get_location());
}

Compiler::CompileStmtResult Compiler::compile_expr_stmt(AST::ExprStmt* s)
{
    RegMark mark(m_current);
    FA_COMPILE_TRY(r, compile_expr_impl(s->expr));
    ANY_REG_BIND(r, s->get_location(), tmp);
    (void)tmp;
    return { };
}

Compiler::CompileStmtResult Compiler::compile_if(AST::IfElseStmt* s)
{
    SourceLocation loc = s->get_location();
    ScopeGuard scope(m_current);
    bool incoming_dead = m_current->is_dead;

    RegMark mark(m_current);
    FA_COMPILE_TRY(expr_result, compile_expr_impl(s->condition));
    ANY_REG_BIND(expr_result, loc, cond);
    PCType jump_false = m_bytecode_emitter.jump_if_false(cond, loc);
    FA_COMPILE_DISCARD_TRY(compile_stmt(s->then_stmt));

    if (s->else_stmt) {
        PCType jump_end = m_bytecode_emitter.jump(loc);
        m_bytecode_emitter.patch_jump(jump_false);
        // A return/break/continue in the then branch does not make else dead.
        m_current->is_dead = incoming_dead;
        FA_COMPILE_DISCARD_TRY(compile_stmt(s->else_stmt));
        m_bytecode_emitter.patch_jump(jump_end);
    } else {
        m_bytecode_emitter.patch_jump(jump_false);
    }

    m_current->is_dead = incoming_dead;
    return { };
}

Compiler::CompileStmtResult Compiler::compile_while(AST::WhileStmt* s)
{
    SourceLocation loc = s->get_location();
    ScopeGuard scope(m_current);

    bool incoming_dead = m_current->is_dead;
    PCType loop_start = current_offset();
    push_loop(loop_start);

    {
        RegMark mark(m_current);
        FA_COMPILE_TRY(expr_result, compile_expr_impl(s->condition));
        ANY_REG_BIND(expr_result, loc, cond);
        PCType exit_jump = m_bytecode_emitter.jump_if_false(cond, loc);
        FA_COMPILE_DISCARD_TRY(compile_stmt(s->body));
        PCType continue_target = current_offset();
        m_bytecode_emitter.emit_AsBx(OpCode::LOOP, 0, static_cast<i64>(loop_start.value) - current_offset().value - 1, loc);
        m_bytecode_emitter.patch_jump(exit_jump);
        pop_loop(current_offset(), continue_target, loc.line);
    }
    m_current->is_dead = incoming_dead;
    return { };
}

Compiler::CompileStmtResult Compiler::compile_for(AST::ForStmt* s)
{
    SourceLocation loc = s->get_location();
    ScopeGuard scope(m_current);

    auto target = AST::as_identifier(s->container);
    bool incoming_dead = m_current->is_dead;

    ALLOC_REG(iter_reg, loc);
    {
        declare_local("__for_iter", iter_reg);
        RegMark mark(m_current);
        FA_COMPILE_TRY(expr_result, compile_expr_impl(s->iter));
        m_bytecode_emitter.discharge(expr_result, iter_reg, loc);
    }

    ALLOC_REG(len_reg, loc);
    ALLOC_REG(index_reg, loc);
    ALLOC_REG(target_reg, loc);
    ALLOC_REG(cond_reg, loc);
    ALLOC_REG(step_reg, loc);

    declare_local("__for_len", len_reg);
    declare_local("__for_index", index_reg);
    declare_local(target->spelling, target_reg);
    declare_local("__for_cond", cond_reg);
    declare_local("__for_step", step_reg);

    m_bytecode_emitter.emit_ABC(OpCode::LIST_LEN, len_reg.value, iter_reg.value, 0, loc);
    m_bytecode_emitter.emit_load_value(index_reg, Value::from_int(0), loc);
    m_bytecode_emitter.emit_load_value(step_reg, Value::from_int(1), loc);

    PCType loop_start = current_offset();
    push_loop(loop_start);
    m_bytecode_emitter.binary(OpCode::OP_LT, cond_reg, index_reg, len_reg, loc);

    PCType exit_jump = m_bytecode_emitter.jump_if_false(cond_reg, loc);
    m_bytecode_emitter.emit_ABC(OpCode::LIST_GET, target_reg.value, iter_reg.value, index_reg.value, loc);
    FA_COMPILE_DISCARD_TRY(compile_stmt(s->body));

    PCType continue_target = current_offset();
    m_bytecode_emitter.binary(OpCode::OP_ADD, index_reg, index_reg, step_reg, loc);
    m_bytecode_emitter.emit_AsBx(OpCode::LOOP, 0, static_cast<i64>(loop_start.value) - current_offset().value - 1, loc);

    m_bytecode_emitter.patch_jump(exit_jump);
    pop_loop(current_offset(), continue_target, loc.line);

    m_current->is_dead = incoming_dead;
    return { };
}

Compiler::CompileStmtResult Compiler::compile_function_def(AST::FuncDefStmt* f)
{
    SourceLocation loc = f->get_location();
    if (!m_current->is_top_level || m_current->scope_depth != 0)
        return raise_compile_error(ErrorCode::NESTED_FUNCTION_UNSUPPORTED, f->get_location());

    if (current_chunk()->functions.size() > MAX_CONSTANTS)
        return raise_compile_error(ErrorCode::TOO_MANY_FUNCTIONS, loc);

    Chunk* fn_chunk = make_chunk();
    fn_chunk->source = current_chunk()->source;
    fn_chunk->source_path = current_chunk()->source_path;
    fn_chunk->name = f->name->spelling;
    fn_chunk->arity = f->has_parameters() ? static_cast<int>(f->params.size()) : 0;

    FunctionIndex fn_idx = current_chunk()->add_function(fn_chunk);

    CompilerState fn_state { fn_chunk, f->name->spelling, false, m_current };
    defer_function_body(fn_chunk, f, fn_state);

    ALLOC_REG(dst, loc);
    m_bytecode_emitter.closure(dst, fn_idx, loc);

    if (m_current != nullptr && m_current->is_top_level) {
        ConstantIndex name_idx = intern_string(f->name->spelling);
        m_bytecode_emitter.store_global(dst, name_idx, loc);
    }

    declare_local(f->name->spelling, dst);
    return { };
}

Compiler::CompileStmtResult Compiler::compile_function_body(AST::FuncDefStmt const* node,
    CompilerState& state, StringRef receiver_class)
{
    SourceLocation loc = node->get_location();
    CompilerStateGuard state_guard(m_current, &state);
    ScopeGuard scope(m_current);
    RegType receiver;
    if (state.is_class_method) {
        TRY_SET(&receiver, alloc_register(loc));
        declare_local(kClassInstanceName, receiver, receiver_class);
    }
    for (AST::ExprPtr param : node->params) {
        ALLOC_REG(reg, loc);
        declare_local(AST::as_identifier(param)->spelling, reg);
    }
    AST::StmtPtr body = node->body;
    if (AST::is_function_stub(body)) {
        AST::ASTBuilder builder;
        auto parsed = builder.materialize(*AST::as_function_stub(body));
        if (parsed.has_error())
            return raise_compile_error(parsed.error());
        body = parsed.value();
    }
    FA_COMPILE_DISCARD_TRY(compile_stmt(body));
    if (!state.is_dead) {
        if (state.is_class_method)
            m_bytecode_emitter.emit_ABC(OpCode::RETURN, receiver.value, 1, 0, node->get_location());
        else
            m_bytecode_emitter.return_nil(node->get_location());
    }
    state.chunk->local_count = state.registers.max_reg();
    return { };
}

void Compiler::defer_function_body(Chunk* chunk, AST::FuncDefStmt const* node,
    CompilerState state, StringRef receiver_class)
{
    // AST nodes and their spellings live in the same arena as chunks. Capture
    // compiler tables by value so neither stack state nor the original Compiler
    // needs to survive (in particular, for imported modules).
    state.chunk = nullptr;
    state.enclosing = nullptr;
    chunk->deferred_body = [node, state, receiver_class,
                               globals = m_globals, modules = m_module_names,
                               classes = m_class_registry](Chunk* target) {
        Compiler compiler(target->source);
        compiler.m_globals = globals;
        compiler.m_module_names = modules;
        compiler.m_class_registry = classes;
        CompilerState function_state = state;
        function_state.chunk = target;
        return compiler.compile_function_body(node, function_state, receiver_class);
    };
}

ErrorOr<bool, RuntimeError> Compiler::compile_function_deferred(Chunk* chunk)
{
    if (!chunk->deferred_body)
        return true;

    diagnostic::SourceScope source_scope(chunk->source);
    // Compile transactionally: a failed attempt must leave a callable stub,
    // never partial bytecode or stale constant-cache indices.
    Chunk compiled;
    compiled.name = chunk->name;
    compiled.source = chunk->source;
    compiled.source_path = chunk->source_path;
    compiled.globals = chunk->globals;
    compiled.arity = chunk->arity;
    auto result = chunk->deferred_body(&compiled);
    if (result.has_error())
        return result.error();
    *chunk = std::move(compiled);
    return true;
}

Compiler::CompileStmtResult Compiler::compile_return(AST::ReturnStmt* s)
{
    SourceLocation loc = s->get_location();

    if (!s->has_value()) {
        m_bytecode_emitter.return_nil(loc);
        m_current->is_dead = true;
        return { };
    }

    AST::ExprPtr value = s->value;
    if (AST::is_nil(value)) {
        m_bytecode_emitter.return_nil(loc);
        m_current->is_dead = true;
        return { };
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
            ANY_REG_BIND(call_ret.value(), loc, src);
            m_bytecode_emitter.emit_ABC(OpCode::RETURN, src.value, 1, 0, loc);
        }
        m_current->is_dead = true;
        return { };
    }

    RegMark mark(m_current);
    FA_COMPILE_TRY(expr_result, compile_expr_impl(value));
    ANY_REG_BIND(expr_result, loc, src);
    m_bytecode_emitter.emit_ABC(OpCode::RETURN, src.value, 1, 0, loc);
    m_current->is_dead = true;
    return { };
}

Compiler::CompileStmtResult Compiler::compile_break(AST::BreakStmt* s)
{
    if (m_current->loop_stack.empty())
        return raise_compile_error(ErrorCode::BREAK_OUTSIDE_LOOP, s->get_location());

    SourceLocation loc = s->get_location();
    m_current->loop_stack.back().break_patches.push(m_bytecode_emitter.jump(loc));
    m_current->is_dead = true;
    return { };
}

Compiler::CompileStmtResult Compiler::compile_continue(AST::ContinueStmt* s)
{
    if (m_current->loop_stack.empty())
        return raise_compile_error(ErrorCode::CONTINUE_OUTSIDE_LOOP, s->get_location());

    SourceLocation loc = s->get_location();
    m_current->loop_stack.back().continue_patches.push(m_bytecode_emitter.jump(loc));
    m_current->is_dead = true;
    return { };
}

Compiler::CompileStmtResult Compiler::compile_block(AST::BlockStmt* s)
{
    ScopeGuard scope(m_current);

    for (AST::StmtPtr child : s->stmts) {
        FA_COMPILE_DISCARD_TRY(compile_stmt(child));
    }

    SourceLocation loc = { 1, 1, 0 };
    if (!s->stmts.empty() && s->stmts.back())
        loc = s->stmts.back()->get_location();
    return { };
}

Compiler::CompileStmtResult Compiler::compile_class_def(AST::ClassDefStmt* s)
{
    SourceLocation loc = s->get_location();
    if (!m_current->is_top_level || m_current->scope_depth != 0)
        return raise_compile_error(ErrorCode::NESTED_CLASS_UNSUPPORTED, loc);

    StringRef class_name = AST::as_identifier(s->name)->spelling;
    StringRef parent_name;
    ClassDescriptor const* parent_desc = nullptr;
    if (s->parent != nullptr) {
        parent_name = AST::as_identifier(s->parent)->spelling;
        parent_desc = m_class_registry.find_ptr(parent_name);
    }

    Array<StringRef> own_field_names;
    Array<StringRef> field_names;
    Array<StringRef> method_names(static_cast<u32>(ObjClass::_COUNT), StringRef { });
    if (parent_desc != nullptr) {
        field_names = parent_desc->field_names;
        method_names = parent_desc->method_names;
    }

    for (AST::ExprPtr field : s->members) {
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

    auto compile_method_closure = [&](AST::FuncDefStmt const* method) -> ErrorOr<std::tuple<RegType, Chunk*>, RuntimeError> {
        SourceLocation method_loc = method->get_location();

        if (current_chunk()->functions.size() > MAX_CONSTANTS)
            return raise_compile_error(ErrorCode::TOO_MANY_FUNCTIONS, method_loc);

        Chunk* ch = make_chunk();
        ch->source = current_chunk()->source;
        ch->source_path = current_chunk()->source_path;
        ch->name = class_name + "." + method->name->spelling;
        int ex_param_count = method->has_parameters() ? static_cast<int>(method->params.size()) : 0;
        ch->arity = ex_param_count + 1;

        FunctionIndex fn_idx = current_chunk()->add_function(ch);

        CompilerState state { ch, method->name->spelling, false, m_current };
        state.is_class_method = true;
        state.class_field_names = field_names;
        state.class_layout_dynamic = s->parent != nullptr && parent_desc == nullptr;
        state.class_method_names = method_names;
        defer_function_body(ch, method, state, class_name);

        ALLOC_REG(dst, loc);
        m_bytecode_emitter.closure(dst, fn_idx, method_loc);
        return std::tuple<RegType, Chunk*> { dst, ch };
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
    Array<int> method_slots;     // parallel to `methods`: final vtable slot per method

    for (AST::StmtPtr m : s->methods) {
        if (m->get_kind() != AST::Stmt::Kind::FUNC)
            return raise_compile_error(ErrorCode::INVALID_STATEMENT_NODE, m->get_location());

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
            return raise_compile_error(ErrorCode::INVALID_STATEMENT_NODE, method->get_location());

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

    for (u32 i = 0, n = static_cast<u32>(s->methods.size()); i < n; i++) {
        auto* method = as_function_def(s->methods[i]);
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
    //   FunctionIndex fn_idx = current_chunk()->add_function(ch);
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

    ClassIndex desc_idx = current_chunk()->add_class_descriptor(std::move(desc_data));

    ALLOC_REG(class_reg, loc);
    m_bytecode_emitter.new_class(class_reg, desc_idx, loc);
    m_bytecode_emitter.store_global(class_reg, intern_string(class_name), loc);
    declare_local(class_name, class_reg);

    // ClassDesc registration — unchanged
    ClassDescriptor cdesc;
    cdesc.name = class_name;
    if (s->parent == nullptr || parent_desc != nullptr)
        cdesc.field_names = field_names;
    cdesc.method_names = method_names;

    m_class_registry[class_name] = std::move(cdesc);
    return { };
}

Compiler::CompileStmtResult Compiler::compile_import(AST::ImportStmt* s)
{
    if (!m_current->is_top_level || m_current->scope_depth != 0)
        return raise_compile_error(ErrorCode::INVALID_STATEMENT_NODE, s->get_location());

    SourceLocation loc = s->get_location();
    StringRef const& module = s->get_module();
    Array<StringRef> const& names = s->get_names();
    Array<StringRef> const& aliases = s->get_aliases();
    if (!s->imports_member()) {
        // Whole-module imports have no member names, but still bind one alias.
        if (aliases.size() != 1)
            return raise_compile_error(ErrorCode::INVALID_STATEMENT_NODE, loc);
        return compile_import_single(module, { }, aliases[0], loc, false);
    }
    if (names.size() != aliases.size())
        return raise_compile_error(ErrorCode::INVALID_STATEMENT_NODE, loc);
    for (size_t i = 0; i < names.size(); ++i)
        TRY_DISCARD(compile_import_single(module, names[i], aliases[i], loc, true));

    return { };
}

Compiler::CompileStmtResult Compiler::compile_import_single(
    StringRef const& module, StringRef const& name, StringRef const& alias, SourceLocation loc, bool imports_member)
{
    // Imported bindings live in the module's global environment. Keeping a
    // permanent local register for every import makes a large module exhaust
    // the 8-bit register file even though those temporary values are already
    // stored globally. Rewind the load/extract temporaries after this
    // statement and resolve later references through LOAD_GLOBAL.
    RegMark mark(m_current);

    ALLOC_REG(module_reg, loc);
    m_bytecode_emitter.import_module(module_reg, intern_string(module), loc);

    RegType value_reg = module_reg;
    if (imports_member) {
        /// not using ALLOC_REG to not overshadow the existing 'value_reg'
        TRY_SET(&value_reg, alloc_register(loc));
        m_bytecode_emitter.get_named_field(value_reg, module_reg,
            intern_string(name), loc);
    }

    m_bytecode_emitter.store_global(value_reg, intern_string(alias), loc);
    m_globals[alias] = true;
    if (!imports_member)
        m_module_names[alias] = true;
    return { };
}

ClassDescriptor const* Compiler::resolve_receiver_class(AST::ExprPtr e) const
{
    // Ordinary bindings can change class across branches and loop backedges.
    // Without flow analysis their declaration-time class is not a slot proof.
    // Only the implicit method receiver has a stable class layout here.
    if (!is_this_reference(e))
        return nullptr;

    StringRef const name = AST::as_identifier(e)->spelling;

    // Its class may already be registered; otherwise the caller uses the
    // current method's field table (or named lookup for dynamic layouts).
    if (LocalVar const* local = lookup_local(name)) {
        if (!local->known_class.empty()) {
            if (auto* d = m_class_registry.find_ptr(local->known_class))
                return d;
        }
    }

    return nullptr;
}

} // namespace fairuz
