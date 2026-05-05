/// compiler.cc

#include "compiler.hpp"
#include "optim.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

namespace fairuz::runtime {

using CompilerError = diagnostic::errc::compiler::Code;

static constexpr char kClassInstanceName[] = "__class$instance";
static constexpr char kClassMetadataKey[] = "__class__";

static bool is_terminal_top_level_call(AST::Fa_Stmt const* s)
{
    auto const* expr_stmt = dynamic_cast<AST::Fa_ExprStmt const*>(s);
    if (expr_stmt == nullptr)
        return false;

    return dynamic_cast<AST::Fa_CallExpr const*>(expr_stmt->get_expr()) != nullptr;
}

static void patch_a(Fa_Chunk* chunk, u32 pc, u8 a)
{
    u32 instr = chunk->code[pc];
    chunk->code[pc] = (instr & 0xFF00FFFFu) | (static_cast<u32>(a) << 16);
}

Fa_Chunk* Compiler::compile(Fa_Array<AST::Fa_Stmt*> const& stmts)
{
    Fa_Chunk* chunk = make_chunk();
    chunk->name = "<main>";

    CompilerState state;
    state.chunk = chunk;
    state.func_name = "<main>";
    state.is_top_level = true;
    state.enclosing = nullptr;
    m_current = &state;

    for (size_t i = 0; i < stmts.size(); i += 1) {
        AST::Fa_Stmt const* stmt = stmts[i];
        if (i + 1 == stmts.size() && stmt && !state.is_dead && is_terminal_top_level_call(stmt)) {
            auto const* expr_stmt = static_cast<AST::Fa_ExprStmt const*>(stmt);
            Fa_SourceLocation loc = expr_stmt->get_location();
            RegMark mark(m_current);
            u8 src = any_reg(compile_expr_i(expr_stmt->get_expr()), loc);
            emit(Fa_make_ABC(Fa_OpCode::RETURN, src, 1, 0), loc);
            state.is_dead = true;
            break;
        }
        compile_stmt(stmt);
    }

    Fa_SourceLocation loc = { 1, 1, 0 };
    if (!stmts.empty() && stmts.back())
        loc = stmts.back()->get_location();

    if (!state.is_dead)
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);

    chunk->local_count = state.max_reg;
    m_current = nullptr;
    return chunk;
}

void Compiler::compile_stmt(AST::Fa_Stmt const* s)
{
    if (s == nullptr || m_current->is_dead)
        return;

    switch (s->get_kind()) {
    case AST::Fa_Stmt::Kind::BLOCK:
        compile_block(static_cast<AST::Fa_BlockStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::EXPR:
        compile_expr_stmt(static_cast<AST::Fa_ExprStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::ASSIGNMENT:
        compile_assignment_stmt(static_cast<AST::Fa_AssignmentStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::IF:
        compile_if(static_cast<AST::Fa_IfStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::WHILE:
        compile_while(static_cast<AST::Fa_WhileStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::FUNC:
        compile_function_def(static_cast<AST::Fa_FunctionDef const*>(s));
        break;
    case AST::Fa_Stmt::Kind::RETURN:
        compile_return(static_cast<AST::Fa_ReturnStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::FOR:
        compile_for(static_cast<AST::Fa_ForStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::BREAK:
        compile_break(static_cast<AST::Fa_BreakStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::CONTINUE:
        compile_continue(static_cast<AST::Fa_ContinueStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::CLASS_DEF:
        compile_class_def(static_cast<AST::Fa_ClassDef const*>(s));
        break;
    case AST::Fa_Stmt::Kind::INVALID:
        diagnostic::emit(CompilerError::INVALID_STATEMENT_NODE);
        break;
    }
}

void Compiler::compile_block(AST::Fa_BlockStmt const* s)
{
    begin_scope();

    for (AST::Fa_Stmt const* child : s->get_statements())
        compile_stmt(child);

    Fa_SourceLocation loc = { 1, 1, 0 };
    if (!s->get_statements().empty() && s->get_statements().back())
        loc = s->get_statements().back()->get_location();

    end_scope(loc);
}

void Compiler::compile_expr_stmt(AST::Fa_ExprStmt const* s)
{
    RegMark mark(m_current);
    u8 tmp = alloc_register();
    Fa_ExprResult r = compile_expr_i(s->get_expr());
    discharge(r, tmp, s->get_location());
}

void Compiler::compile_assignment_stmt(AST::Fa_AssignmentStmt const* s)
{
    Fa_SourceLocation loc = s->get_location();

    if (auto const* index_expr = dynamic_cast<AST::Fa_IndexExpr const*>(s->get_target())) {
        RegMark mark(m_current);
        u8 object_reg = any_reg(compile_expr_i(index_expr->get_object()), loc);
        u8 index_reg = any_reg(compile_expr_i(index_expr->get_index()), loc);
        u8 value_reg = any_reg(compile_expr_i(s->get_value()), loc);
        emit(Fa_make_ABC(Fa_OpCode::LIST_SET, object_reg, index_reg, value_reg), loc);
        return;
    }

    auto const* name = dynamic_cast<AST::Fa_NameExpr const*>(s->get_target());
    if (name == nullptr) {
        diagnostic::emit(CompilerError::INVALID_ASSIGNMENT_TARGET, "only simple name assignments are supported");
        return;
    }

    if (s->is_declaration()) {
        if (LocalVar const* local = lookup_local(name->get_value())) {
            u8 local_reg = local->reg;
            compile_expr(s->get_value(), &local_reg);
            return;
        }

        u8 reg = alloc_register();
        Fa_ExprResult value = compile_expr_i(s->get_value());
        discharge(value, reg, loc);
        declare_local(name->get_value(), reg);
        return;
    }

    VarInfo vi = resolve_name(name->get_value());
    if (vi.kind == VarInfo::Kind::LOCAL) {
        compile_expr(s->get_value(), &vi.index);
        return;
    }

    if (!m_current->is_top_level) {
        u8 reg = alloc_register();
        discharge(compile_expr_i(s->get_value()), reg, loc);
        declare_local(name->get_value(), reg);
        return;
    }

    RegMark mark(m_current);
    u8 src = any_reg(compile_expr_i(s->get_value()), loc);
    u16 kidx = intern_string(name->get_value());
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
}

void Compiler::compile_if(AST::Fa_IfStmt const* s)
{
    if (s == nullptr)
        return;

    Fa_SourceLocation loc = s->get_location();
    begin_scope();
    bool incoming_dead = m_current->is_dead;

    if (auto folded = try_fold_expr(s->get_condition())) {
        if (Fa_IS_TRUTHY(*folded))
            compile_stmt(s->get_then());
        else if (AST::Fa_Stmt* m_else_stmt = s->get_else())
            compile_stmt(m_else_stmt);

        m_current->is_dead = incoming_dead;
        return;
    }

    RegMark mark(m_current);
    u8 cond = any_reg(compile_expr_i(s->get_condition()), loc);
    u32 jump_false = emit_jump(Fa_OpCode::JUMP_IF_FALSE, cond, loc);
    compile_stmt(s->get_then());

    if (AST::Fa_Stmt* else_stmt = s->get_else()) {
        u32 jump_end = emit_jump(Fa_OpCode::JUMP, 0, loc);
        patch_jump(jump_false);
        compile_stmt(else_stmt);
        patch_jump(jump_end);
    } else {
        patch_jump(jump_false);
    }

    m_current->is_dead = incoming_dead;
    end_scope(loc);
}

void Compiler::compile_while(AST::Fa_WhileStmt const* s)
{
    if (s == nullptr)
        return;

    Fa_SourceLocation loc = s->get_location();
    begin_scope();
    bool incoming_dead = m_current->is_dead;
    if (auto folded = try_fold_expr(s->get_condition())) {
        if (Fa_IS_TRUTHY(*folded)) {
            u32 loop_start = current_offset();
            push_loop(loop_start);
            compile_stmt(s->get_body());
            u32 continue_target = current_offset();
            emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(current_offset()) - 1), loc);
            pop_loop(current_offset(), continue_target, loc.line);
        }

        m_current->is_dead = incoming_dead;
        return;
    }

    u32 loop_start = current_offset();
    push_loop(loop_start);

    {
        RegMark mark(m_current);
        u8 cond = any_reg(compile_expr_i(s->get_condition()), loc);
        u32 exit_jump = emit_jump(Fa_OpCode::JUMP_IF_FALSE, cond, loc);
        compile_stmt(s->get_body());
        u32 continue_target = current_offset();
        emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(current_offset()) - 1), loc);
        patch_jump(exit_jump);
        pop_loop(current_offset(), continue_target, loc.line);
    }
    m_current->is_dead = incoming_dead;
    end_scope(loc);
}

void Compiler::compile_function_def(AST::Fa_FunctionDef const* f)
{
    Fa_SourceLocation loc = f->get_location();
    if (!m_current->is_top_level || m_current->scope_depth != 0) {
        diagnostic::emit(CompilerError::NESTED_FUNCTION_UNSUPPORTED);
        return;
    }

    AST::Fa_NameExpr* name = f->get_name();
    if (name == nullptr) {
        diagnostic::emit(CompilerError::NULL_FUNCTION_NAME, diagnostic::Severity::FATAL);
        return;
    }

    Fa_Chunk* fn_chunk = make_chunk();
    fn_chunk->name = name->get_value();
    fn_chunk->arity = f->has_parameters() ? static_cast<int>(f->get_parameters().size()) : 0;

    auto fn_idx = static_cast<u16>(current_chunk()->functions.size());
    current_chunk()->functions.push(fn_chunk);

    CompilerState fn_state;
    fn_state.chunk = fn_chunk;
    fn_state.func_name = name->get_value();
    fn_state.enclosing = m_current;
    m_current = &fn_state;

    begin_scope();
    if (f->has_parameters()) {
        for (AST::Fa_Expr const* param : f->get_parameters()) {
            auto param_name = dynamic_cast<AST::Fa_NameExpr const*>(param);
            if (param_name == nullptr) {
                diagnostic::emit(CompilerError::INVALID_FUNCTION_PARAMETER);
                continue;
            }

            u8 reg = alloc_register();
            declare_local(param_name->get_value(), reg);
        }
    }

    compile_stmt(f->get_body());
    if (!fn_state.is_dead)
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);

    end_scope(loc);
    fn_chunk->local_count = fn_state.max_reg;
    m_current = fn_state.enclosing;

    u8 dst = alloc_register();
    emit(Fa_make_ABx(Fa_OpCode::CLOSURE, dst, fn_idx), loc);

    if (m_current != nullptr && m_current->is_top_level) {
        u16 name_idx = intern_string(name->get_value());
        emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, dst, name_idx), loc);
    }

    declare_local(name->get_value(), dst);
}

void Compiler::compile_return(AST::Fa_ReturnStmt const* s)
{
    Fa_SourceLocation loc = s->get_location();

    if (!s->has_value()) {
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);
        m_current->is_dead = true;
        return;
    }

    AST::Fa_Expr const* value = s->get_value();
    if (value->get_kind() == AST::Fa_Expr::Kind::LITERAL && static_cast<AST::Fa_LiteralExpr const*>(value)->is_nil()) {
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);
        m_current->is_dead = true;
        return;
    }

    if (value->get_kind() == AST::Fa_Expr::Kind::CALL && !m_current->is_top_level) {
        RegMark mark(m_current);
        compile_call_impl(static_cast<AST::Fa_CallExpr const*>(value), nullptr, true);
        m_current->is_dead = true;
        return;
    }

    RegMark mark(m_current);
    u8 src = any_reg(compile_expr_i(value), loc);
    emit(Fa_make_ABC(Fa_OpCode::RETURN, src, 1, 0), loc);
    m_current->is_dead = true;
}

void Compiler::compile_for(AST::Fa_ForStmt const* s)
{
    Fa_SourceLocation loc = s->get_location();
    auto target = dynamic_cast<AST::Fa_NameExpr const*>(s->get_target());
    if (target == nullptr) {
        diagnostic::emit(CompilerError::INVALID_ASSIGNMENT_TARGET, "for loop target must be a name");
        return;
    }

    bool incoming_dead = m_current->is_dead;

    begin_scope();
    u8 iter_reg = alloc_register();

    {
        declare_local("__for_iter", iter_reg);
        RegMark mark(m_current);
        discharge(compile_expr_i(s->get_iter()), iter_reg, loc);
    }

    u8 len_reg = alloc_register();
    declare_local("__for_len", len_reg);
    emit(Fa_make_ABC(Fa_OpCode::LIST_LEN, len_reg, iter_reg, 0), loc);

    u8 index_reg = alloc_register();
    declare_local("__for_index", index_reg);
    emit_load_value(index_reg, Fa_MAKE_INTEGER(0), loc);

    u8 target_reg = alloc_register();
    declare_local(target->get_value(), target_reg);

    u8 cond_reg = alloc_register();
    declare_local("__for_cond", cond_reg);

    u8 step_reg = alloc_register();
    declare_local("__for_step", step_reg);
    emit_load_value(step_reg, Fa_MAKE_INTEGER(1), loc);

    u32 loop_start = current_offset();
    push_loop(loop_start);
    emit(Fa_make_ABC(Fa_OpCode::OP_LT, cond_reg, index_reg, len_reg), loc);
    emit(Fa_make_ABC(Fa_OpCode::NOP, current_chunk()->alloc_ic_slot(), 0, 0), loc);

    u32 exit_jump = emit_jump(Fa_OpCode::JUMP_IF_FALSE, cond_reg, loc);
    emit(Fa_make_ABC(Fa_OpCode::LIST_GET, target_reg, iter_reg, index_reg), loc);
    compile_stmt(s->get_body());

    u32 continue_target = current_offset();
    emit(Fa_make_ABC(Fa_OpCode::OP_ADD, index_reg, index_reg, step_reg), loc);
    emit(Fa_make_ABC(Fa_OpCode::NOP, current_chunk()->alloc_ic_slot(), 0, 0), loc);
    emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(current_offset()) - 1), loc);
    patch_jump(exit_jump);
    pop_loop(current_offset(), continue_target, loc.line);
    end_scope(loc);

    m_current->is_dead = incoming_dead;
}

void Compiler::compile_break(AST::Fa_BreakStmt const* s)
{
    if (m_current->loop_stack.empty()) {
        diagnostic::emit(CompilerError::BREAK_OUTSIDE_LOOP);
        return;
    }

    Fa_SourceLocation loc = s->get_location();
    m_current->loop_stack.back().break_patches.push(emit_jump(Fa_OpCode::JUMP, 0, loc));
    m_current->is_dead = true;
}

void Compiler::compile_continue(AST::Fa_ContinueStmt const* s)
{
    if (m_current->loop_stack.empty()) {
        diagnostic::emit(CompilerError::CONTINUE_OUTSIDE_LOOP);
        return;
    }

    Fa_SourceLocation loc = s->get_location();
    m_current->loop_stack.back().continue_patches.push(emit_jump(Fa_OpCode::JUMP, 0, loc));
    m_current->is_dead = true;
}

void Compiler::compile_class_def(AST::Fa_ClassDef const* s)
{
    if (s == nullptr)
        return;

    Fa_SourceLocation loc = s->get_location();
    if (!m_current->is_top_level || m_current->scope_depth != 0) {
        diagnostic::emit(CompilerError::NESTED_FUNCTION_UNSUPPORTED, "classes must be declared at top level");
        return;
    }

    auto const* class_name = dynamic_cast<AST::Fa_NameExpr const*>(s->get_name());
    if (class_name == nullptr) {
        diagnostic::emit(CompilerError::INVALID_ASSIGNMENT_TARGET, "class name must be a simple name");
        return;
    }

    auto emit_empty_dict = [&](u8 dst, Fa_SourceLocation where) {
        u16 ctor_idx = intern_string("جدول");
        emit(Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL, dst, ctor_idx), where);
        emit(Fa_make_ABC(Fa_OpCode::IC_CALL, dst, 0, current_chunk()->alloc_ic_slot()), where);
    };

    auto compile_method_closure = [&](AST::Fa_FunctionDef const* method) -> u8 {
        Fa_SourceLocation method_loc = method->get_location();
        AST::Fa_NameExpr* method_name = method->get_name();
        if (method_name == nullptr) {
            diagnostic::emit(CompilerError::NULL_FUNCTION_NAME, diagnostic::Severity::FATAL);
            return error_reg();
        }

        Fa_Chunk* fn_chunk = make_chunk();
        fn_chunk->name = class_name->get_value() + "." + method_name->get_value();

        int explicit_param_count = method->has_parameters() ? static_cast<int>(method->get_parameters().size()) : 0;
        fn_chunk->arity = explicit_param_count;

        u16 fn_idx = static_cast<u16>(current_chunk()->functions.size());
        current_chunk()->functions.push(fn_chunk);

        CompilerState fn_state;
        fn_state.chunk = fn_chunk;
        fn_state.func_name = fn_chunk->name;
        fn_state.enclosing = m_current;
        m_current = &fn_state;

        begin_scope();

        if (method->has_parameters()) {
            for (AST::Fa_Expr const* param : method->get_parameters()) {
                auto const* param_name = dynamic_cast<AST::Fa_NameExpr const*>(param);
                if (param_name == nullptr) {
                    diagnostic::emit(CompilerError::INVALID_FUNCTION_PARAMETER);
                    continue;
                }

                u8 reg = alloc_register();
                declare_local(param_name->get_value(), reg);
            }
        }

        u8 instance_reg = alloc_register();
        declare_local(kClassInstanceName, instance_reg);
        emit_empty_dict(instance_reg, method_loc);

        if (auto const* body_block = dynamic_cast<AST::Fa_BlockStmt const*>(method->get_body())) {
            for (AST::Fa_Stmt const* child : body_block->get_statements())
                compile_stmt(child);
        } else {
            compile_stmt(method->get_body());
        }

        if (!fn_state.is_dead)
            emit(Fa_make_ABC(Fa_OpCode::RETURN, instance_reg, 1, 0), method_loc);

        end_scope(method_loc);
        fn_chunk->local_count = fn_state.max_reg;
        m_current = fn_state.enclosing;

        u8 dst = alloc_register();
        emit(Fa_make_ABx(Fa_OpCode::CLOSURE, dst, fn_idx), method_loc);
        return dst;
    };

    u8 class_reg = alloc_register();
    emit_empty_dict(class_reg, loc);

    // Deduplicate members: simple linear scan with string comparison (safe, no hash table lifetime issues)
    Fa_Array<Fa_ObjHeader*> member_names = Fa_Array<Fa_ObjHeader*>::with_capacity(s->get_members().size());

    for (AST::Fa_Expr const* member_expr : s->get_members()) {
        auto const* member_name = dynamic_cast<AST::Fa_NameExpr const*>(member_expr);
        if (member_name == nullptr)
            continue;

        Fa_StringRef name = member_name->get_value();
        bool seen = false;
        for (Fa_ObjHeader* existing : member_names) {
            if (static_cast<Fa_ObjString const*>(existing)->str == name) {
                seen = true;
                break;
            }
        }

        if (!seen)
            member_names.push(static_cast<Fa_ObjHeader*>(Fa_MAKE_OBJ_STRING(name)));
    }

    Fa_ObjClass* class_meta = get_allocator().allocate_object<Fa_ObjClass>(
        Fa_MAKE_OBJ_STRING(class_name->get_value()), member_names);

    {
        RegMark mark(m_current);
        u8 key_reg = alloc_register();
        emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, key_reg, intern_string(kClassMetadataKey)), loc);

        u8 meta_reg = alloc_register();
        emit_load_value(meta_reg, Fa_MAKE_OBJECT(class_meta), loc);
        emit(Fa_make_ABC(Fa_OpCode::LIST_SET, class_reg, key_reg, meta_reg), loc);
    }

    for (AST::Fa_Stmt const* method_stmt : s->get_methods()) {
        auto const* method = dynamic_cast<AST::Fa_FunctionDef const*>(method_stmt);
        if (method == nullptr) {
            diagnostic::emit(CompilerError::INVALID_STATEMENT_NODE, "class body entries must be methods");
            continue;
        }

        RegMark mark(m_current);
        u8 key_reg = alloc_register();
        emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, key_reg, intern_string(method->get_name()->get_value())), method->get_location());

        u8 method_reg = compile_method_closure(method);
        emit(Fa_make_ABC(Fa_OpCode::LIST_SET, class_reg, key_reg, method_reg), method->get_location());
    }

    u16 name_idx = intern_string(class_name->get_value());
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, class_reg, name_idx), loc);
    declare_local(class_name->get_value(), class_reg);
}

Fa_ExprResult Compiler::compile_expr_i(AST::Fa_Expr const* e)
{
    if (e == nullptr)
        return Fa_ExprResult::knil();

    switch (e->get_kind()) {
    case AST::Fa_Expr::Kind::LITERAL:
        return compile_literal_i(static_cast<AST::Fa_LiteralExpr const*>(e));
    case AST::Fa_Expr::Kind::NAME:
        return compile_name_i(static_cast<AST::Fa_NameExpr const*>(e));
    case AST::Fa_Expr::Kind::UNARY:
        return compile_unary_i(static_cast<AST::Fa_UnaryExpr const*>(e));
    case AST::Fa_Expr::Kind::BINARY:
        return compile_binary_i(static_cast<AST::Fa_BinaryExpr const*>(e));
    case AST::Fa_Expr::Kind::ASSIGNMENT:
        return compile_assign_i(static_cast<AST::Fa_AssignmentExpr const*>(e));
    case AST::Fa_Expr::Kind::CALL:
        return compile_call_impl(static_cast<AST::Fa_CallExpr const*>(e), nullptr, false);
    case AST::Fa_Expr::Kind::LIST:
        return compile_list_i(static_cast<AST::Fa_ListExpr const*>(e));
    case AST::Fa_Expr::Kind::DICT:
        return compile_dict_i(static_cast<AST::Fa_DictExpr const*>(e));
    case AST::Fa_Expr::Kind::INDEX:
        return compile_index_i(static_cast<AST::Fa_IndexExpr const*>(e));
    case AST::Fa_Expr::Kind::INVALID:
        diagnostic::emit(CompilerError::INVALID_EXPRESSION_NODE);
        return Fa_ExprResult::knil();
    }

    return Fa_ExprResult::knil();
}

Fa_ExprResult Compiler::compile_literal_i(AST::Fa_LiteralExpr const* e)
{
    if (e->is_string()) {
        u16 kidx = intern_string(e->get_str());
        u32 pc = emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, 0, kidx), e->get_location());
        return Fa_ExprResult::reloc(pc);
    }
    if (e->is_integer())
        return Fa_ExprResult::kint(e->get_int());
    if (e->is_float())
        return Fa_ExprResult::kfloat(e->get_float());
    if (e->is_bool())
        return Fa_ExprResult::kbool(e->get_bool());
    if (e->is_nil())
        return Fa_ExprResult::knil();

    diagnostic::emit(CompilerError::UNKNOWN_LITERAL_TYPE);
    return Fa_ExprResult::knil();
}

Fa_ExprResult Compiler::compile_name_i(AST::Fa_NameExpr const* e)
{
    Fa_SourceLocation loc = e->get_location();
    VarInfo vi = resolve_name(e->get_value());

    if (vi.kind == VarInfo::Kind::LOCAL)
        return Fa_ExprResult::reg(vi.index);

    u16 kidx = intern_string(e->get_value());
    u32 pc = emit(Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL, 0, kidx), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ExprResult Compiler::compile_unary_i(AST::Fa_UnaryExpr const* e)
{
    Fa_SourceLocation loc = e->get_location();

    if (auto folded = try_fold_unary(e)) {
        Fa_Value v = *folded;
        if (Fa_IS_INTEGER(v))
            return Fa_ExprResult::kint(Fa_AS_INTEGER(v));
        if (Fa_IS_DOUBLE(v))
            return Fa_ExprResult::kfloat(Fa_AS_DOUBLE(v));
        if (Fa_IS_BOOL(v))
            return Fa_ExprResult::kbool(Fa_AS_BOOL(v));
        if (Fa_IS_NIL(v))
            return Fa_ExprResult::knil();
    }

    if (auto reduced = try_strength_reduce_unary(e))
        return compile_expr_i(*reduced);

    Fa_OpCode op = Fa_OpCode::NOP;
    switch (e->get_operator()) {
    case AST::Fa_UnaryOp::OP_NEG:
        op = Fa_OpCode::OP_NEG;
        break;
    case AST::Fa_UnaryOp::OP_BITNOT:
        op = Fa_OpCode::OP_BITNOT;
        break;
    case AST::Fa_UnaryOp::OP_NOT:
        op = Fa_OpCode::OP_NOT;
        break;
    default:
        diagnostic::emit(CompilerError::UNKNOWN_UNARY_OPERATOR, diagnostic::Severity::FATAL);
        return Fa_ExprResult::knil();
    }

    RegMark mark(m_current);
    u8 src = any_reg(compile_expr_i(e->get_operand()), loc);
    u32 pc = emit(Fa_make_ABC(op, 0, src, 0), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ExprResult Compiler::compile_binary_i(AST::Fa_BinaryExpr const* e)
{
    Fa_SourceLocation loc = e->get_location();

    if (auto folded = try_fold_binary(e)) {
        Fa_Value v = *folded;
        if (Fa_IS_INTEGER(v))
            return Fa_ExprResult::kint(Fa_AS_INTEGER(v));
        if (Fa_IS_DOUBLE(v))
            return Fa_ExprResult::kfloat(Fa_AS_DOUBLE(v));
        if (Fa_IS_BOOL(v))
            return Fa_ExprResult::kbool(Fa_AS_BOOL(v));
        if (Fa_IS_NIL(v))
            return Fa_ExprResult::knil();
    }

    if (auto reduced = try_strength_reduce_binary(e))
        return compile_expr_i(*reduced);

    AST::Fa_BinaryOp op = e->get_operator();
    if (op == AST::Fa_BinaryOp::OP_AND) {
        u8 dst = alloc_register();

        {
            RegMark mark(m_current);
            discharge(compile_expr_i(e->get_left()), dst, loc);
        }

        u32 skip = emit_jump(Fa_OpCode::JUMP_IF_FALSE, dst, loc);

        {
            RegMark mark(m_current);
            discharge(compile_expr_i(e->get_right()), dst, loc);
        }

        patch_jump(skip);
        return Fa_ExprResult::reg(dst);
    }

    if (op == AST::Fa_BinaryOp::OP_OR) {
        u8 dst = alloc_register();

        {
            RegMark mark(m_current);
            discharge(compile_expr_i(e->get_left()), dst, loc);
        }

        u32 skip = emit_jump(Fa_OpCode::JUMP_IF_TRUE, dst, loc);

        {
            RegMark mark(m_current);
            discharge(compile_expr_i(e->get_right()), dst, loc);
        }

        patch_jump(skip);
        return Fa_ExprResult::reg(dst);
    }

    Fa_OpCode bc_op = Fa_OpCode::NOP;
    bool swapped = false;

    switch (op) {
    case AST::Fa_BinaryOp::OP_ADD:
        bc_op = Fa_OpCode::OP_ADD;
        break;
    case AST::Fa_BinaryOp::OP_SUB:
        bc_op = Fa_OpCode::OP_SUB;
        break;
    case AST::Fa_BinaryOp::OP_MUL:
        bc_op = Fa_OpCode::OP_MUL;
        break;
    case AST::Fa_BinaryOp::OP_DIV:
        bc_op = Fa_OpCode::OP_DIV;
        break;
    case AST::Fa_BinaryOp::OP_MOD:
        bc_op = Fa_OpCode::OP_MOD;
        break;
    case AST::Fa_BinaryOp::OP_POW:
        bc_op = Fa_OpCode::OP_POW;
        break;
    case AST::Fa_BinaryOp::OP_EQ:
        bc_op = Fa_OpCode::OP_EQ;
        break;
    case AST::Fa_BinaryOp::OP_NEQ:
        bc_op = Fa_OpCode::OP_NEQ;
        break;
    case AST::Fa_BinaryOp::OP_LT:
        bc_op = Fa_OpCode::OP_LT;
        break;
    case AST::Fa_BinaryOp::OP_LTE:
        bc_op = Fa_OpCode::OP_LTE;
        break;
    case AST::Fa_BinaryOp::OP_GT:
        bc_op = Fa_OpCode::OP_LT;
        swapped = true;
        break;
    case AST::Fa_BinaryOp::OP_GTE:
        bc_op = Fa_OpCode::OP_LTE;
        swapped = true;
        break;
    case AST::Fa_BinaryOp::OP_BITAND:
        bc_op = Fa_OpCode::OP_BITAND;
        break;
    case AST::Fa_BinaryOp::OP_BITOR:
        bc_op = Fa_OpCode::OP_BITOR;
        break;
    case AST::Fa_BinaryOp::OP_BITXOR:
        bc_op = Fa_OpCode::OP_BITXOR;
        break;
    case AST::Fa_BinaryOp::OP_LSHIFT:
        bc_op = Fa_OpCode::OP_LSHIFT;
        break;
    case AST::Fa_BinaryOp::OP_RSHIFT:
        bc_op = Fa_OpCode::OP_RSHIFT;
        break;
    default:
        diagnostic::emit(CompilerError::UNKNOWN_BINARY_OPERATOR, diagnostic::Severity::FATAL);
        return Fa_ExprResult::knil();
    }

    if (bc_op == Fa_OpCode::OP_LSHIFT || bc_op == Fa_OpCode::OP_RSHIFT) {
        auto amount_expr = dynamic_cast<AST::Fa_LiteralExpr const*>(e->get_right());
        if (amount_expr == nullptr || !amount_expr->is_integer()) {
            diagnostic::emit(CompilerError::SHIFT_AMOUNT_NOT_CONSTANT);
            return Fa_ExprResult::knil();
        }

        i64 amount = amount_expr->get_int();
        if (amount < 0 || amount > 63) {
            diagnostic::emit(CompilerError::SHIFT_AMOUNT_OUT_OF_RANGE, std::to_string(amount));
            return Fa_ExprResult::knil();
        }

        RegMark mark(m_current);
        u8 lhs = any_reg(compile_expr_i(e->get_left()), loc);
        u32 pc = emit(Fa_make_ABC(bc_op, 0, lhs, static_cast<u8>(amount)), loc);
        u8 ic = current_chunk()->alloc_ic_slot();
        emit(Fa_make_ABC(Fa_OpCode::NOP, ic, 0, 0), loc);
        return Fa_ExprResult::reloc(pc);
    }

    RegMark mark(m_current);
    u8 lhs = any_reg(compile_expr_i(e->get_left()), loc);
    u8 rhs = any_reg(compile_expr_i(e->get_right()), loc);

    if (swapped)
        std::swap(lhs, rhs);

    u32 pc = emit(Fa_make_ABC(bc_op, 0, lhs, rhs), loc);
    u8 ic = current_chunk()->alloc_ic_slot();
    emit(Fa_make_ABC(Fa_OpCode::NOP, ic, 0, 0), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ExprResult Compiler::compile_assign_i(AST::Fa_AssignmentExpr const* e)
{
    Fa_SourceLocation loc = e->get_location();
    AST::Fa_Expr const* target = e->get_target();

    if (target->get_kind() == AST::Fa_Expr::Kind::INDEX) {
        auto index_expr = static_cast<AST::Fa_IndexExpr const*>(target);
        RegMark mark(m_current);
        u8 list_reg = any_reg(compile_expr_i(index_expr->get_object()), loc);
        u8 index_reg = any_reg(compile_expr_i(index_expr->get_index()), loc);
        u8 value_reg = any_reg(compile_expr_i(e->get_value()), loc);
        emit(Fa_make_ABC(Fa_OpCode::LIST_SET, list_reg, index_reg, value_reg), loc);
        return Fa_ExprResult::reg(value_reg);
    }

    auto name = dynamic_cast<AST::Fa_NameExpr const*>(target);
    if (name == nullptr) {
        diagnostic::emit(CompilerError::INVALID_ASSIGNMENT_TARGET);
        return Fa_ExprResult::knil();
    }

    if (e->is_declaration()) {
        if (LocalVar const* local = lookup_local(name->get_value())) {
            u8 local_reg = local->reg;
            compile_expr(e->get_value(), &local_reg);
            return Fa_ExprResult::reg(local_reg);
        }

        if (m_current->is_top_level && m_current->scope_depth == 0) {
            RegMark mark(m_current);
            u8 src = any_reg(compile_expr_i(e->get_value()), loc);
            u16 kidx = intern_string(name->get_value());
            emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
            return Fa_ExprResult::reg(src);
        }

        u8 reg = alloc_register();
        discharge(compile_expr_i(e->get_value()), reg, loc);
        declare_local(name->get_value(), reg);
        return Fa_ExprResult::reg(reg);
    }

    VarInfo vi = resolve_name(name->get_value());
    if (vi.kind == VarInfo::Kind::LOCAL) {
        compile_expr(e->get_value(), &vi.index);
        return Fa_ExprResult::reg(vi.index);
    }

    if (!m_current->is_top_level) {
        u8 reg = alloc_register();
        discharge(compile_expr_i(e->get_value()), reg, loc);
        declare_local(name->get_value(), reg);
        return Fa_ExprResult::reg(reg);
    }

    RegMark mark(m_current);
    u8 src = any_reg(compile_expr_i(e->get_value()), loc);
    u16 kidx = intern_string(name->get_value());
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
    return Fa_ExprResult::reg(src);
}

Fa_ExprResult Compiler::compile_call_impl(AST::Fa_CallExpr const* e, u8* dst, bool tail)
{
    Fa_SourceLocation loc = e->get_location();
    u8 fn_reg = dst ? *dst : alloc_register();

    discharge(compile_expr_i(e->get_callee()), fn_reg, loc);

    for (AST::Fa_Expr const* arg : e->get_args()) {
        u8 arg_reg = alloc_register();
        discharge(compile_expr_i(arg), arg_reg, loc);
        m_current->free_regs_to(arg_reg + 1);
    }

    u8 argc = static_cast<u8>(e->get_args().size());
    if (tail && !m_current->is_top_level) {
        emit(Fa_make_ABC(Fa_OpCode::CALL_TAIL, fn_reg, argc, 0), loc);
        m_current->free_regs_to(fn_reg);
        return Fa_ExprResult::reg(fn_reg);
    }

    u8 ic = current_chunk()->alloc_ic_slot();
    emit(Fa_make_ABC(Fa_OpCode::IC_CALL, fn_reg, argc, ic), loc);
    m_current->free_regs_to(fn_reg + 1);
    return Fa_ExprResult::reg(fn_reg);
}

Fa_ExprResult Compiler::compile_list_i(AST::Fa_ListExpr const* e)
{
    Fa_SourceLocation loc = e->get_location();
    u8 dst = alloc_register();
    auto cap = static_cast<u8>(std::min<u32>(e->size(), 0xFF));
    emit(Fa_make_ABC(Fa_OpCode::LIST_NEW, dst, cap, 0), loc);

    RegMark mark(m_current);
    int i = 0;
    for (AST::Fa_Expr const* elem : e->get_elements()) {
        if (i == 0xFF)
            break;
        u8 reg = any_reg(compile_expr_i(elem), loc);
        emit(Fa_make_ABC(Fa_OpCode::LIST_APPEND, dst, reg, 0), loc);
        i += 1;
    }

    return Fa_ExprResult::reg(dst);
}

Fa_ExprResult Compiler::compile_index_i(AST::Fa_IndexExpr const* e)
{
    Fa_SourceLocation loc = e->get_location();
    RegMark mark(m_current);
    u8 object_reg = any_reg(compile_expr_i(e->get_object()), loc);
    u8 index_reg = any_reg(compile_expr_i(e->get_index()), loc);
    u32 pc = emit(Fa_make_ABC(e->is_unsafe() ? Fa_OpCode::UNSAFE_INDEX : Fa_OpCode::INDEX, 0, object_reg, index_reg), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ExprResult Compiler::compile_dict_i(AST::Fa_DictExpr const* e)
{
    Fa_SourceLocation loc = e->get_location();
    u8 fn_reg = alloc_register();
    u16 kidx = intern_string("جدول");
    emit(Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL, fn_reg, kidx), loc);

    RegMark mark(m_current);
    for (auto const& [key, value] : e->get_content()) {
        u8 key_reg = alloc_register();
        discharge(compile_expr_i(key), key_reg, key->get_location());

        u8 value_reg = alloc_register();
        discharge(compile_expr_i(value), value_reg, value->get_location());
    }

    u8 argc = static_cast<u8>(e->get_content().size() * 2);
    u8 ic = current_chunk()->alloc_ic_slot();
    emit(Fa_make_ABC(Fa_OpCode::IC_CALL, fn_reg, argc, ic), loc);
    m_current->free_regs_to(fn_reg + 1);
    return Fa_ExprResult::reg(fn_reg);
}

void Compiler::discharge(Fa_ExprResult const& r, u8 dst, Fa_SourceLocation loc)
{
    switch (r.kind) {
    case Fa_ExprResult::Kind::REG:
        if (r.reg_ != dst)
            emit(Fa_make_ABC(Fa_OpCode::MOVE, dst, r.reg_, 0), loc);
        break;
    case Fa_ExprResult::Kind::RELOC:
        patch_a(current_chunk(), r.reloc_pc, dst);
        break;
    case Fa_ExprResult::Kind::KINT:
        emit_load_value(dst, Fa_MAKE_INTEGER(r.ival), loc);
        break;
    case Fa_ExprResult::Kind::KFLOAT:
        emit_load_value(dst, Fa_MAKE_REAL(r.dval), loc);
        break;
    case Fa_ExprResult::Kind::KBOOL:
        emit_load_value(dst, Fa_MAKE_BOOL(r.bval), loc);
        break;
    case Fa_ExprResult::Kind::KNIL:
        emit_load_value(dst, NIL_VAL, loc);
        break;
    }
}

u8 Compiler::any_reg(Fa_ExprResult const& r, Fa_SourceLocation loc)
{
    if (r.kind == Fa_ExprResult::Kind::REG)
        return r.reg_;

    u8 dst = alloc_register();
    discharge(r, dst, loc);
    return dst;
}

u8 Compiler::compile_expr(AST::Fa_Expr const* e, u8* dst)
{
    if (e == nullptr)
        return error_reg();

    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r = compile_expr_i(e);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::compile_literal(AST::Fa_LiteralExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r = compile_literal_i(e);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::compile_name(AST::Fa_NameExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r = compile_name_i(e);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::compile_unary(AST::Fa_UnaryExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r = compile_unary_i(e);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::compile_binary(AST::Fa_BinaryExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r = compile_binary_i(e);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::compile_assignment_expr(AST::Fa_AssignmentExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r = compile_assign_i(e);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::compile_call(AST::Fa_CallExpr const* e, u8* dst, bool tail)
{
    Fa_ExprResult r = compile_call_impl(e, dst, tail);
    return r.reg_;
}

u8 Compiler::compile_list(AST::Fa_ListExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r = compile_list_i(e);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::compile_index(AST::Fa_IndexExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r = compile_index_i(e);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::compile_dict(AST::Fa_DictExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r = compile_dict_i(e);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::error_reg() const { return 0; }

u8 Compiler::ensure_reg(u8 const* m_reg) { return m_reg ? *m_reg : alloc_register(); }

u8 Compiler::alloc_register()
{
    u8 reg = m_current->alloc_register();
    if (reg >= MAX_REGS) {
        diagnostic::emit(CompilerError::TOO_MANY_REGISTERS, std::string(m_current->func_name.data(), m_current->func_name.len()));
        return 0;
    }

    return reg;
}

void Compiler::declare_local(Fa_StringRef const& m_name, u8 m_reg) { m_current->locals.push({ m_name, m_current->scope_depth, m_reg }); }

LocalVar const* Compiler::lookup_local(Fa_StringRef const& m_name) const
{
    auto const& locals = m_current->locals;
    for (auto i = static_cast<int>(locals.size()) - 1; i >= 0; i -= 1) {
        if (locals[i].name == m_name)
            return &locals[i];
    }

    return nullptr;
}

Compiler::VarInfo Compiler::resolve_name(Fa_StringRef const& name)
{
    if (LocalVar const* local = lookup_local(name))
        return { VarInfo::Kind::LOCAL, local->reg };

    return VarInfo {
        .kind = VarInfo::Kind::GLOBAL,
        .index = 0
    };
}

u32 Compiler::emit(u32 instr, Fa_SourceLocation loc) { return current_chunk()->emit(instr, loc); }

u32 Compiler::emit_jump(Fa_OpCode op, u8 cond, Fa_SourceLocation loc) { return emit(Fa_make_AsBx(op, cond, 0), loc); }

void Compiler::patch_jump(u32 idx)
{
    if (!current_chunk()->patch_jump(idx))
        diagnostic::emit(CompilerError::JUMP_OFFSET_OVERFLOW, diagnostic::Severity::FATAL);
}

void Compiler::push_loop(u32 loop_start) { m_current->loop_stack.push({ { }, { }, loop_start }); }

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
        diagnostic::emit(CompilerError::LOOP_JUMP_OFFSET_OVERFLOW, diagnostic::Severity::FATAL);

    u32 word = current_chunk()->code[instr_idx];
    current_chunk()->code[instr_idx] = Fa_make_AsBx(Fa_instr_op(word), Fa_instr_A(word), offset);
}

void Compiler::emit_load_value(u8 dst, Fa_Value v, Fa_SourceLocation loc)
{
    if (Fa_IS_NIL(v)) {
        emit(Fa_make_ABC(Fa_OpCode::LOAD_NIL, dst, dst, 1), loc);
        return;
    }

    if (Fa_IS_BOOL(v)) {
        emit(Fa_make_ABC(Fa_AS_BOOL(v) ? Fa_OpCode::LOAD_TRUE : Fa_OpCode::LOAD_FALSE, dst, 0, 0), loc);
        return;
    }

    if (Fa_IS_INTEGER(v)) {
        i64 iv = Fa_AS_INTEGER(v);
        if (iv >= -JUMP_OFFSET && iv <= JUMP_OFFSET) {
            emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, dst, static_cast<u16>(iv + JUMP_OFFSET)), loc);
            return;
        }
    }

    emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, dst, current_chunk()->add_constant(v)), loc);
}

Fa_Chunk* Compiler::current_chunk() const { return m_current->chunk; }

u32 Compiler::current_offset() const { return current_chunk()->code.size(); }

void Compiler::begin_scope() { m_current->scope_depth += 1; }

void Compiler::end_scope(Fa_SourceLocation loc)
{
    (void)loc;
    m_current->scope_depth -= 1;
    unsigned int depth = m_current->scope_depth;
    auto& locals = m_current->locals;
    size_t pop_from = locals.size();

    while (pop_from > 0 && locals[pop_from - 1].depth > depth)
        pop_from -= 1;

    if (pop_from < locals.size())
        m_current->next_reg = locals[pop_from].reg;

    locals.resize(static_cast<u32>(pop_from));
}

u32 Compiler::intern_string(Fa_StringRef const& str)
{
    Fa_Chunk* chunk = current_chunk();
    auto key = std::make_pair(str, chunk);
    if (u16* idx = m_string_cache.find_ptr(key))
        return *idx;

    Fa_ObjString* obj = Fa_MAKE_OBJ_STRING(str);
    u16 idx = chunk->add_constant(Fa_MAKE_OBJECT(obj));
    m_string_cache[key] = idx;
    return idx;
}

} // namespace fairuz::runtime
