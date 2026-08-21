//
// fcompiler.cc
//

#include "fcompiler.hpp"
#include "fAST.hpp"
#include "fdiagnostic.hpp"
#include "ferror.hpp"
#include "fmacros.hpp"
#include "fopcode.hpp"
#include "foptim.hpp"
#include "fvalue.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <utility>

#define Fa_VERIFY_RESULT(r)            \
    do {                               \
        if (UNLIKELY((r).has_error())) \
            return (r).error();        \
    } while (0)

#define COMPILE_EXPR_IMPL(e, r) ({ auto _r = compile_expr_impl(e); Fa_VERIFY_RESULT(_r); *r = _r.value(); })
#define COMPILE_STMT_DISCARD(s) ({ auto _r = compile_stmt(s); Fa_VERIFY_RESULT(_r); (void)_r; })
#define ANY_REG(v, l, r) ({ auto _r = any_reg(v, l); Fa_VERIFY_RESULT(_r); *r = _r.value(); })
#define ALLOC_REG(r) ({ auto _r = alloc_register(); Fa_VERIFY_RESULT(_r); *r = _r.value(); })

namespace fairuz::runtime {

using CompilerError = diagnostic::errc::compiler::Code;
using cmp_ret = Fa_ErrorOr<Fa_ExprResult>;

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

static AST::Fa_NameExpr* as_simple_member_name(AST::Fa_Expr* e)
{
    return e != nullptr && e->get_kind() == AST::Fa_Expr::Kind::NAME ? AS_NAME(e) : nullptr;
}

// Mirrors fairuz::parser::same_name (fparser.cc), which is file-local to
// that translation unit and not visible here. Used to detect `this.field`
// GET targets (object side is the synthetic kClassInstanceName NAME node)
// so we can fall back to current_method_field_index() instead of
// resolve_receiver_class(), which depends on m_class_registry — and the
// class currently being compiled is NOT YET in m_class_registry while its
// own methods are still being compiled (see compile_class_def: the
// registry insert happens only after the full method-compilation loop).
static bool is_this_reference(AST::Fa_Expr const* e)
{
    return e != nullptr
        && e->get_kind() == AST::Fa_Expr::Kind::NAME
        && AS_CONST_NAME(e)->get_value() == Fa_StringRef(kClassInstanceName);
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
        AST::Fa_Stmt* stmt = stmts[i];
        if (i + 1 == stmts.size() && stmt && !state.is_dead && is_terminal_top_level_call(stmt)) {
            auto const* expr_stmt = AS_CONST_EXPR_STMT(stmt);
            Fa_SourceLocation loc = expr_stmt->get_location();
            RegMark mark(m_current);
            auto expr_result = compile_expr_impl(expr_stmt->get_expr());
            if (expr_result.has_error())
                break;
            auto src = any_reg(expr_result.value(), loc);
            if (src.has_error())
                break;
            emit(Fa_make_ABC(Fa_OpCode::RETURN, src.value(), 1, 0), loc);
            state.is_dead = true;
            break;
        }
        auto stmt_result = compile_stmt(stmt);
        if (stmt_result.has_error())
            break;
    }

    Fa_SourceLocation loc = { 1, 1, 0 };
    if (!stmts.empty() && stmts.back())
        loc = stmts.back()->get_location();

    if (!state.is_dead)
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);

    chunk->local_count = state.max_reg;
    m_current = nullptr;

    if (diagnostic::has_errors())
        diagnostic::dump();
    return chunk;
}

Fa_ErrorOr<bool> Compiler::compile_stmt(AST::Fa_Stmt* s)
{
    if (s == nullptr || m_current->is_dead)
        return true;

    switch (s->get_kind()) {
    case AST::Fa_Stmt::Kind::BLOCK: return compile_block(AS_BLOCK(s));
    case AST::Fa_Stmt::Kind::EXPR: return compile_expr_stmt(AS_EXPR_STMT(s));
    case AST::Fa_Stmt::Kind::ASSIGNMENT: return compile_assignment_stmt(AS_ASSIGNMENT_STMT(s));
    case AST::Fa_Stmt::Kind::IF: return compile_if(AS_IF(s));
    case AST::Fa_Stmt::Kind::WHILE: return compile_while(AS_WHILE(s));
    case AST::Fa_Stmt::Kind::FUNC: return compile_function_def(AS_FUNCTION_DEF(s));
    case AST::Fa_Stmt::Kind::RETURN: return compile_return(AS_RETURN(s));
    case AST::Fa_Stmt::Kind::FOR: return compile_for(AS_FOR(s));
    case AST::Fa_Stmt::Kind::BREAK: return compile_break(AS_BREAK(s));
    case AST::Fa_Stmt::Kind::CONTINUE: return compile_continue(AS_CONTINUE(s));
    case AST::Fa_Stmt::Kind::CLASS_DEF: return compile_class_def(AS_CLASS_DEF(s));
    case AST::Fa_Stmt::Kind::INVALID:
        return report_error(CompilerError::INVALID_STATEMENT_NODE, s->get_location());
    }
}

Fa_ErrorOr<bool> Compiler::compile_block(AST::Fa_BlockStmt* s)
{
    begin_scope();

    for (AST::Fa_Stmt* child : s->get_statements()) {
        auto r = compile_stmt(child);
        Fa_VERIFY_RESULT(r);
    }

    Fa_SourceLocation loc = { 1, 1, 0 };
    if (!s->get_statements().empty() && s->get_statements().back())
        loc = s->get_statements().back()->get_location();

    end_scope(loc);
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_expr_stmt(AST::Fa_ExprStmt* s)
{
    RegMark mark(m_current);
    Fa_ExprResult r;
    COMPILE_EXPR_IMPL(s->get_expr(), &r);
    u8 tmp;
    ANY_REG(r, s->get_location(), &tmp);
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_assignment_stmt(AST::Fa_AssignmentStmt* s)
{
    Fa_SourceLocation loc = s->get_location();

    if (auto* index_expr = dynamic_cast<AST::Fa_IndexExpr*>(s->get_target())) {
        RegMark mark(m_current);
        Fa_ExprResult object_expr_result, index_expr_result, value_expr_result;
        COMPILE_EXPR_IMPL(index_expr->get_object(), &object_expr_result);
        COMPILE_EXPR_IMPL(index_expr->get_index(), &index_expr_result);
        COMPILE_EXPR_IMPL(s->get_value(), &value_expr_result);
        u8 object_reg, index_reg, value_reg;
        ANY_REG(object_expr_result, loc, &object_reg);
        ANY_REG(index_expr_result, loc, &index_reg);
        ANY_REG(value_expr_result, loc, &value_reg);
        emit(Fa_make_ABC(Fa_OpCode::LIST_SET, object_reg, index_reg, value_reg), loc);
        return true;
    }

    if (auto* get_expr = dynamic_cast<AST::Fa_GetExpr*>(s->get_target())) {
        if (AST::Fa_NameExpr* member_name = as_simple_member_name(get_expr->get_member())) {
            // Fast path: receiver's class is already registered in
            // m_class_registry (e.g. `obj.field := x` where obj's class
            // finished compiling earlier).
            if (ClassDesc const* desc = resolve_receiver_class(get_expr->get_object())) {
                int field_idx = desc->field_index(member_name->get_value());
                if (field_idx >= 0) {
                    RegMark mark(m_current);
                    Fa_ExprResult object_expr_result, value_expr_result;
                    COMPILE_EXPR_IMPL(get_expr->get_object(), &object_expr_result);
                    COMPILE_EXPR_IMPL(s->get_value(), &value_expr_result);
                    u8 object_reg, value_reg;
                    ANY_REG(object_expr_result, loc, &object_reg);
                    ANY_REG(value_expr_result, loc, &value_reg);
                    emit(Fa_make_ABC(Fa_OpCode::SET_FIELD, object_reg, static_cast<u8>(field_idx), value_reg), loc);
                    return true;
                }
            }

            // `this.field := x` inside the class's own method body, while
            // that class is still being compiled. m_class_registry doesn't
            // have this class yet (compile_class_def registers it only
            // after all methods finish compiling), but
            // state.class_field_names was pre-populated from the parser's
            // this.field-assignment scan before any method body compiled,
            // so current_method_field_index() already knows about this
            // field even though resolve_receiver_class() can't see it yet.
            if (is_this_reference(get_expr->get_object())) {
                int field_idx = current_method_field_index(member_name->get_value());
                if (field_idx >= 0) {
                    RegMark mark(m_current);
                    LocalVar const* self = lookup_local(kClassInstanceName);
                    if (self != nullptr) {
                        Fa_ExprResult value_expr_result;
                        COMPILE_EXPR_IMPL(s->get_value(), &value_expr_result);
                        u8 value_reg;
                        ANY_REG(value_expr_result, loc, &value_reg);
                        emit(Fa_make_ABC(Fa_OpCode::SET_FIELD, self->reg, static_cast<u8>(field_idx), value_reg), loc);
                        return true;
                    }
                }
            }
        }
    }

    auto* name = dynamic_cast<AST::Fa_NameExpr*>(s->get_target());
    if (name == nullptr)
        /// TODO: go to report_error def and add an option to put addition text errors
        return report_error(CompilerError::INVALID_ASSIGNMENT_TARGET, s->get_location());

    if (s->is_declaration()) {
        if (LocalVar const* local = lookup_local(name->get_value())) {
            u8 local_reg = local->reg;
            return compile_expr(s->get_value(), &local_reg).error_or(true);
        }

        u8 reg;
        ALLOC_REG(&reg);
        Fa_ExprResult value_expr_result;
        COMPILE_EXPR_IMPL(s->get_value(), &value_expr_result);
        discharge(value_expr_result, reg, loc);
        declare_local(name->get_value(), reg, infer_constructed_class(s->get_value()));
        return true;
    }

    if (int field_idx = current_method_field_index(name->get_value()); field_idx >= 0) {
        RegMark mark(m_current);
        LocalVar const* self = lookup_local(kClassInstanceName);
        if (self == nullptr)
            return report_error(CompilerError::INVALID_ASSIGNMENT_TARGET, name->get_location());

        Fa_ExprResult value_expr_result;
        COMPILE_EXPR_IMPL(s->get_value(), &value_expr_result);
        u8 value_reg;
        ANY_REG(value_expr_result, loc, &value_reg);
        emit(Fa_make_ABC(Fa_OpCode::SET_FIELD, self->reg, static_cast<u8>(field_idx), value_reg), loc);
        return true;
    }

    VarInfo vi = resolve_name(name->get_value());
    if (vi.kind == VarInfo::Kind::LOCAL)
        return compile_expr(s->get_value(), &vi.index).error_or(true);

    if (!m_current->is_top_level) {
        u8 reg;
        ALLOC_REG(&reg);
        Fa_ExprResult expr_result;
        COMPILE_EXPR_IMPL(s->get_value(), &expr_result);
        discharge(expr_result, reg, loc);
        declare_local(name->get_value(), reg);
        return true;
    }

    RegMark mark(m_current);
    Fa_ExprResult expr_result;
    COMPILE_EXPR_IMPL(s->get_value(), &expr_result);
    u8 src;
    ANY_REG(expr_result, loc, &src);
    u16 kidx = intern_string(name->get_value());
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_if(AST::Fa_IfStmt* s)
{
    if (s == nullptr)
        return true;

    Fa_SourceLocation loc = s->get_location();
    begin_scope();
    bool incoming_dead = m_current->is_dead;

    if (auto folded = try_fold_expr(s->get_condition())) {
        if (Fa_IS_TRUTHY(*folded)) {
            auto ret = compile_stmt(s->get_then());
            m_current->is_dead = incoming_dead;
            return ret;
        } else if (AST::Fa_Stmt* m_else_stmt = s->get_else()) {
            auto ret = compile_stmt(m_else_stmt);
            m_current->is_dead = incoming_dead;
            return ret;
        }
        return true;
    }

    RegMark mark(m_current);
    Fa_ExprResult expr_result;
    u8 cond;
    COMPILE_EXPR_IMPL(s->get_condition(), &expr_result);
    ANY_REG(expr_result, loc, &cond);
    u32 jump_false = emit_jump(Fa_OpCode::JUMP_IF_FALSE, cond, loc);
    COMPILE_STMT_DISCARD(s->get_then());

    if (AST::Fa_Stmt* else_stmt = s->get_else()) {
        u32 jump_end = emit_jump(Fa_OpCode::JUMP, 0, loc);
        patch_jump(jump_false);
        auto else_ret = compile_stmt(else_stmt);
        Fa_VERIFY_RESULT(else_ret);
        (void)else_ret;
        patch_jump(jump_end);
    } else {
        patch_jump(jump_false);
    }

    m_current->is_dead = incoming_dead;
    end_scope(loc);
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_while(AST::Fa_WhileStmt* s)
{
    if (s == nullptr)
        return true;

    Fa_SourceLocation loc = s->get_location();

    begin_scope();

    bool incoming_dead = m_current->is_dead;
    if (auto folded = try_fold_expr(s->get_condition())) {
        if (Fa_IS_TRUTHY(*folded)) {
            u32 loop_start = current_offset();
            push_loop(loop_start);
            COMPILE_STMT_DISCARD(s->get_body());
            u32 continue_target = current_offset();
            emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(current_offset()) - 1), loc);
            pop_loop(current_offset(), continue_target, loc.line);
        }

        m_current->is_dead = incoming_dead;
        return true;
    }

    u32 loop_start = current_offset();
    push_loop(loop_start);

    {
        RegMark mark(m_current);
        Fa_ExprResult expr_result;
        u8 cond;
        COMPILE_EXPR_IMPL(s->get_condition(), &expr_result);
        ANY_REG(expr_result, loc, &cond);
        u32 exit_jump = emit_jump(Fa_OpCode::JUMP_IF_FALSE, cond, loc);
        COMPILE_STMT_DISCARD(s->get_body());
        u32 continue_target = current_offset();
        emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(current_offset()) - 1), loc);
        patch_jump(exit_jump);
        pop_loop(current_offset(), continue_target, loc.line);
    }
    m_current->is_dead = incoming_dead;

    end_scope(loc);
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_function_def(AST::Fa_FunctionDef* f)
{
    Fa_SourceLocation loc = f->get_location();
    if (!m_current->is_top_level || m_current->scope_depth != 0)
        return report_error(CompilerError::NESTED_FUNCTION_UNSUPPORTED, f->get_location());

    AST::Fa_NameExpr* name = f->get_name();
    if (name == nullptr)
        return report_error(CompilerError::NULL_FUNCTION_NAME, f->get_location());

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
        for (AST::Fa_Expr* param : f->get_parameters()) {
            auto param_name = dynamic_cast<AST::Fa_NameExpr*>(param);
            if (param_name == nullptr)
                return report_error(CompilerError::INVALID_FUNCTION_PARAMETER, param->get_location());

            u8 reg;
            ALLOC_REG(&reg);
            declare_local(param_name->get_value(), reg);
        }
    }

    COMPILE_STMT_DISCARD(f->get_body());

    if (!fn_state.is_dead)
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);

    end_scope(loc);

    fn_chunk->local_count = fn_state.max_reg;
    m_current = fn_state.enclosing;

    u8 dst;
    ALLOC_REG(&dst);
    emit(Fa_make_ABx(Fa_OpCode::CLOSURE, dst, fn_idx), loc);

    if (m_current != nullptr && m_current->is_top_level) {
        u16 name_idx = intern_string(name->get_value());
        emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, dst, name_idx), loc);
    }

    declare_local(name->get_value(), dst);
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_return(AST::Fa_ReturnStmt* s)
{
    Fa_SourceLocation loc = s->get_location();

    if (!s->has_value()) {
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);
        m_current->is_dead = true;
        return true;
    }

    AST::Fa_Expr* value = s->get_value();
    if (value->get_kind() == AST::Fa_Expr::Kind::LITERAL && AS_LITERAL(value)->is_nil()) {
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);
        m_current->is_dead = true;
        return true;
    }

    if (value->get_kind() == AST::Fa_Expr::Kind::CALL && !m_current->is_top_level) {
        RegMark mark(m_current);
        auto call_ret = compile_call_impl(AS_CALL(value), nullptr, true);
        Fa_VERIFY_RESULT(call_ret);
        (void)call_ret;
        m_current->is_dead = true;
        return true;
    }

    RegMark mark(m_current);
    Fa_ExprResult expr_result;
    u8 src;
    COMPILE_EXPR_IMPL(value, &expr_result);
    ANY_REG(expr_result, loc, &src);
    emit(Fa_make_ABC(Fa_OpCode::RETURN, src, 1, 0), loc);
    m_current->is_dead = true;
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_for(AST::Fa_ForStmt* s)
{
    Fa_SourceLocation loc = s->get_location();
    if (!AST::is_name(s->get_target()))
        return report_error(CompilerError::INVALID_ASSIGNMENT_TARGET, s->get_location());

    auto target = AS_NAME(s->get_target());
    bool incoming_dead = m_current->is_dead;

    begin_scope();

    u8 iter_reg;
    ALLOC_REG(&iter_reg);
    {
        declare_local("__for_iter", iter_reg);
        RegMark mark(m_current);
        Fa_ExprResult expr_result;
        COMPILE_EXPR_IMPL(s->get_iter(), &expr_result);
        discharge(expr_result, iter_reg, loc);
    }

    u8 len_reg, index_reg, target_reg, cond_reg, step_reg;

    ALLOC_REG(&len_reg);
    ALLOC_REG(&index_reg);
    ALLOC_REG(&target_reg);
    ALLOC_REG(&cond_reg);
    ALLOC_REG(&step_reg);

    declare_local("__for_len", len_reg);
    declare_local("__for_index", index_reg);
    declare_local(target->get_value(), target_reg);
    declare_local("__for_cond", cond_reg);
    declare_local("__for_step", step_reg);

    emit(Fa_make_ABC(Fa_OpCode::LIST_LEN, len_reg, iter_reg, 0), loc);
    emit_load_value(index_reg, Fa_MAKE_INTEGER(0), loc);
    emit_load_value(step_reg, Fa_MAKE_INTEGER(1), loc);

    u32 loop_start = current_offset();
    push_loop(loop_start);
    emit(Fa_make_ABC(Fa_OpCode::OP_LT, cond_reg, index_reg, len_reg), loc);
    emit(Fa_make_ABC(Fa_OpCode::NOP, current_chunk()->alloc_ic_slot(), 0, 0), loc);

    u32 exit_jump = emit_jump(Fa_OpCode::JUMP_IF_FALSE, cond_reg, loc);
    emit(Fa_make_ABC(Fa_OpCode::LIST_GET, target_reg, iter_reg, index_reg), loc);
    COMPILE_STMT_DISCARD(s->get_body());

    u32 continue_target = current_offset();
    emit(Fa_make_ABC(Fa_OpCode::OP_ADD, index_reg, index_reg, step_reg), loc);
    emit(Fa_make_ABC(Fa_OpCode::NOP, current_chunk()->alloc_ic_slot(), 0, 0), loc);
    emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(current_offset()) - 1), loc);

    patch_jump(exit_jump);
    pop_loop(current_offset(), continue_target, loc.line);

    end_scope(loc);

    m_current->is_dead = incoming_dead;
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_break(AST::Fa_BreakStmt* s)
{
    if (m_current->loop_stack.empty())
        return report_error(CompilerError::BREAK_OUTSIDE_LOOP, s->get_location());

    Fa_SourceLocation loc = s->get_location();
    m_current->loop_stack.back().break_patches.push(emit_jump(Fa_OpCode::JUMP, 0, loc));
    m_current->is_dead = true;
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_continue(AST::Fa_ContinueStmt* s)
{
    if (m_current->loop_stack.empty())
        return report_error(CompilerError::CONTINUE_OUTSIDE_LOOP, s->get_location());

    Fa_SourceLocation loc = s->get_location();
    m_current->loop_stack.back().continue_patches.push(emit_jump(Fa_OpCode::JUMP, 0, loc));
    m_current->is_dead = true;
    return true;
}

Fa_ErrorOr<bool> Compiler::compile_class_def(AST::Fa_ClassDef* s)
{
    if (s == nullptr)
        return true;

    Fa_SourceLocation loc = s->get_location();
    if (!m_current->is_top_level || m_current->scope_depth != 0)
        return report_error(CompilerError::NESTED_CLASS_UNSUPPORTED, loc);

    Fa_Array<AST::Fa_Expr*> fields = s->get_members();
    Fa_Array<AST::Fa_Stmt*> methods = s->get_methods();
    Fa_StringRef class_name = AS_NAME(s->get_name())->get_value();
    Fa_Array<Fa_StringRef> field_names;

    for (AST::Fa_Expr* field : fields) {
        if (field->get_kind() != AST::Fa_Expr::Kind::NAME)
            return report_error(CompilerError::INVALID_ASSIGNMENT_TARGET, field->get_location());

        auto* name = AS_NAME(field);
        Fa_StringRef fname = name->get_value();

        bool seen = false;
        for (auto& existing : field_names) {
            if (existing == fname) {
                seen = true;
                break;
            }
        }
        if (!seen)
            field_names.push(fname);
    }

    auto compile_method_closure = [&](AST::Fa_FunctionDef* method) -> Fa_ErrorOr<std::tuple<u8, Fa_Chunk*>> {
        Fa_SourceLocation method_loc = method->get_location();
        AST::Fa_NameExpr* method_name = method->get_name();
        if (method_name == nullptr)
            return report_error(CompilerError::NULL_FUNCTION_NAME, method_name->get_location());

        Fa_Chunk* ch = make_chunk();
        ch->name = class_name + "." + method_name->get_value();
        int ex_param_count = method->has_parameters() ? static_cast<int>(method->get_parameters().size()) : 0;
        ch->arity = ex_param_count + 1;

        auto fn_idx = static_cast<u16>(current_chunk()->functions.size());
        current_chunk()->functions.push(ch);

        CompilerState state;
        state.chunk = ch;
        state.func_name = method_name->get_value();
        state.enclosing = m_current;
        state.is_class_method = true;
        state.class_field_names = field_names;
        m_current = &state;

        begin_scope();
        u8 inst_reg;
        ALLOC_REG(&inst_reg);
        declare_local(kClassInstanceName, inst_reg, class_name);

        if (method->has_parameters()) {
            for (AST::Fa_Expr* p : method->get_parameters()) {
                auto* p_name = dynamic_cast<AST::Fa_NameExpr*>(p);
                if (p_name == nullptr)
                    return report_error(CompilerError::INVALID_FUNCTION_PARAMETER, p->get_location());

                u8 reg;
                ALLOC_REG(&reg);
                declare_local(p_name->get_value(), reg);
            }
        }

        COMPILE_STMT_DISCARD(method->get_body());
        if (!state.is_dead)
            emit(Fa_make_ABC(Fa_OpCode::RETURN, inst_reg, 1, 0), method_loc);

        end_scope(method_loc);
        ch->local_count = state.max_reg;
        m_current = state.enclosing;

        u8 dst;
        ALLOC_REG(&dst);
        emit(Fa_make_ABx(Fa_OpCode::CLOSURE, dst, fn_idx), method_loc);
        return std::tuple<u8, Fa_Chunk*> { dst, ch };
    };

    // Map a method name to its reserved special slot, or -1 if it's an
    // ordinary user method. This is the single source of truth for the
    // fixed-slot layout — both the vtable build and method_names/
    // method_slot_map below must agree with it.
    auto special_slot_for = [](Fa_StringRef const& name) -> int {
        if (name == "بداية")
            return Fa_ObjClass::INIT;
        if (name == "نداء")
            return Fa_ObjClass::CALL;
        if (name == "عملية+")
            return Fa_ObjClass::ADD;
        if (name == "عملية-")
            return Fa_ObjClass::SUB;
        if (name == "عملية*")
            return Fa_ObjClass::MUL;
        if (name == "عملية/")
            return Fa_ObjClass::DIV;
        if (name == "عملية%" || name == "عملية٪")
            return Fa_ObjClass::MOD;
        if (name == "سالب")
            return Fa_ObjClass::NEG;
        if (name == "يساوي")
            return Fa_ObjClass::EQ;
        if (name == "لا_يساوي")
            return Fa_ObjClass::NEQ;
        if (name == "اصغر_من")
            return Fa_ObjClass::LT;
        if (name == "اصغر_او_يساوي")
            return Fa_ObjClass::LTE;
        if (name == "اكبر_من")
            return Fa_ObjClass::GT;
        if (name == "اكبر_او_يساوي")
            return Fa_ObjClass::GTE;
        if (name == "كتابة")
            return Fa_ObjClass::REPR;
        return -1;
    };

    // Pre-size the reserved region; ordinary methods are appended after it.
    Fa_Array<Fa_Chunk*> vtable(static_cast<u32>(Fa_ObjClass::_COUNT), /* fill_v= */ nullptr);
    Fa_Array<Fa_StringRef> method_names(static_cast<u32>(Fa_ObjClass::_COUNT), Fa_StringRef { });
    Fa_Array<Fa_StringRef> seen_names; // dedup guard across BOTH special and ordinary methods

    for (AST::Fa_Stmt* m : methods) {
        if (m->get_kind() != AST::Fa_Stmt::Kind::FUNC)
            return report_error(CompilerError::INVALID_STATEMENT_NODE, m->get_location());

        auto* method = AS_FUNCTION_DEF(m);
        Fa_StringRef method_name = method->get_name()->get_value();

        bool seen = false;
        for (auto& existing : seen_names) {
            if (existing == method_name) {
                seen = true;
                break;
            }
        }

        if (seen)
            return report_error(CompilerError::INVALID_STATEMENT_NODE, method->get_location());

        seen_names.push(method_name);
        auto result = compile_method_closure(method);
        Fa_VERIFY_RESULT(result);
        auto [reg, chunk] = result.value();

        if (chunk == nullptr)
            continue;

        int special = special_slot_for(method_name);
        if (special >= 0) {
            vtable[static_cast<u32>(special)] = chunk;
            method_names[static_cast<u32>(special)] = method_name;
        } else {
            vtable.push(chunk);
            method_names.push(method_name);
        }
    }

    // Build the descriptor from the same arrays already computed above.
    // vtable_indices[i] is the index into current_chunk()->functions[] of the
    // chunk that compile_method_closure() pushed there.  The parallel between
    // vtable[] (Fa_Chunk*) and current_chunk()->functions[] is exact because
    // compile_method_closure() does:
    //   auto fn_idx = static_cast<u16>(current_chunk()->functions.size());
    //   current_chunk()->functions.push(ch);
    // so we reconstruct those indices here by scanning for each chunk pointer.
    Fa_Array<u32> vtable_indices;
    for (u32 i = 0; i < vtable.size(); ++i) {
        if (vtable[i] == nullptr) {
            vtable_indices.push(Fa_ClassDescriptor::NULL_SLOT);
            continue;
        }
        // Find the index that compile_method_closure pushed this chunk at
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

    Fa_ClassDescriptor desc_data;
    desc_data.name = Fa_StringRef(class_name);
    desc_data.field_count = static_cast<u32>(field_names.size());
    desc_data.field_names = field_names;
    desc_data.vtable_size = static_cast<u32>(vtable.size());
    desc_data.method_names = method_names;
    desc_data.vtable_indices = std::move(vtable_indices);

    u16 desc_idx = current_chunk()->add_class_descriptor(std::move(desc_data));
    u8 class_reg;

    ALLOC_REG(&class_reg);
    emit(Fa_make_ABx(Fa_OpCode::NEW_CLASS, class_reg, desc_idx), loc);
    u16 name_idx = intern_string(class_name);
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, class_reg, name_idx), loc);
    declare_local(class_name, class_reg);

    // ClassDesc registration — unchanged
    ClassDesc cdesc;
    cdesc.name = class_name;
    cdesc.field_names = field_names;
    cdesc.method_names = method_names;

    for (size_t i = 0; i < field_names.size(); i++)
        cdesc.field_map[field_names[i]] = static_cast<int>(i);
    for (size_t i = 0; i < method_names.size(); i++) {
        if (!method_names[i].empty())
            cdesc.method_map[method_names[i]] = static_cast<int>(i);
    }

    m_class_registry[class_name] = std::move(cdesc);
    return true;
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_expr_impl(AST::Fa_Expr* e)
{
    if (e == nullptr)
        return Fa_ExprResult::knil();

    switch (e->get_kind()) {
    case AST::Fa_Expr::Kind::LITERAL: return compile_literal_impl(AS_LITERAL(e));
    case AST::Fa_Expr::Kind::NAME: return compile_name_impl(AS_NAME(e));
    case AST::Fa_Expr::Kind::UNARY: return compile_unary_impl(AS_UNARY(e));
    case AST::Fa_Expr::Kind::BINARY: return compile_binary_impl(AS_BINARY(e));
    case AST::Fa_Expr::Kind::ASSIGNMENT: return compile_assign_impl(AS_ASSIGNMENT_EXPR(e));
    case AST::Fa_Expr::Kind::CALL: return compile_call_impl(AS_CALL(e), nullptr, false);
    case AST::Fa_Expr::Kind::LIST: return compile_list_impl(AS_LIST(e));
    case AST::Fa_Expr::Kind::DICT: return compile_dict_impl(AS_DICT(e));
    case AST::Fa_Expr::Kind::INDEX: return compile_index_impl(AS_INDEX(e));
    case AST::Fa_Expr::Kind::GET: return compile_get_impl(AS_GET_EXPR(e));
    case AST::Fa_Expr::Kind::INVALID:
        return report_error(CompilerError::INVALID_EXPRESSION_NODE, e->get_location());
    }

    return Fa_ExprResult::knil();
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_literal_impl(AST::Fa_LiteralExpr* e)
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

    return report_error(CompilerError::UNKNOWN_LITERAL_TYPE, e->get_location());
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_name_impl(AST::Fa_NameExpr* e)
{
    Fa_SourceLocation loc = e->get_location();
    VarInfo vi = resolve_name(e->get_value());

    if (vi.kind == VarInfo::Kind::LOCAL)
        return Fa_ExprResult::reg(vi.index);

    if (int field_idx = current_method_field_index(e->get_value()); field_idx >= 0) {
        LocalVar const* self = lookup_local(kClassInstanceName);
        if (self == nullptr)
            return report_error(CompilerError::INVALID_EXPRESSION_NODE, e->get_location());

        u32 pc = emit(Fa_make_ABC(Fa_OpCode::GET_FIELD, 0, self->reg, static_cast<u8>(field_idx)), loc);
        return Fa_ExprResult::reloc(pc);
    }

    u16 kidx = intern_string(e->get_value());
    u32 pc = emit(Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL, 0, kidx), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_unary_impl(AST::Fa_UnaryExpr* e)
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
        return compile_expr_impl(*reduced);

    Fa_OpCode op = Fa_OpCode::NOP;
    switch (e->get_operator()) {
    case AST::Fa_UnaryOp::OP_NEG: op = Fa_OpCode::OP_NEG; break;
    case AST::Fa_UnaryOp::OP_BITNOT: op = Fa_OpCode::OP_BITNOT; break;
    case AST::Fa_UnaryOp::OP_NOT: op = Fa_OpCode::OP_NOT; break;
    default:
        return report_error(CompilerError::UNKNOWN_UNARY_OPERATOR, e->get_location());
    }

    RegMark mark(m_current);
    Fa_ExprResult expr_result;
    COMPILE_EXPR_IMPL(e->get_operand(), &expr_result);
    u8 src;
    ANY_REG(expr_result, loc, &src);
    u32 pc = emit(Fa_make_ABC(op, 0, src, 0), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_binary_impl(AST::Fa_BinaryExpr* e)
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
        return compile_expr_impl(*reduced);

    AST::Fa_BinaryOp op = e->get_operator();
    if (op == AST::Fa_BinaryOp::OP_AND) {
        u8 dst;
        ALLOC_REG(&dst);

        {
            RegMark mark(m_current);
            Fa_ExprResult expr_result;
            COMPILE_EXPR_IMPL(e->get_left(), &expr_result);
            discharge(expr_result, dst, loc);
        }

        u32 skip = emit_jump(Fa_OpCode::JUMP_IF_FALSE, dst, loc);

        {
            RegMark mark(m_current);
            Fa_ExprResult expr_result;
            COMPILE_EXPR_IMPL(e->get_right(), &expr_result);
            discharge(expr_result, dst, loc);
        }

        patch_jump(skip);
        return Fa_ExprResult::reg(dst);
    }

    if (op == AST::Fa_BinaryOp::OP_OR) {
        u8 dst;
        ALLOC_REG(&dst);

        {
            RegMark mark(m_current);
            Fa_ExprResult expr_result;
            COMPILE_EXPR_IMPL(e->get_left(), &expr_result);
            discharge(expr_result, dst, loc);
        }

        u32 skip = emit_jump(Fa_OpCode::JUMP_IF_TRUE, dst, loc);

        {
            RegMark mark(m_current);
            Fa_ExprResult expr_result;
            COMPILE_EXPR_IMPL(e->get_right(), &expr_result);
            discharge(expr_result, dst, loc);
        }

        patch_jump(skip);
        return Fa_ExprResult::reg(dst);
    }

    Fa_OpCode bc_op = Fa_OpCode::NOP;
    bool swapped = false;

    switch (op) {
    case AST::Fa_BinaryOp::OP_ADD: bc_op = Fa_OpCode::OP_ADD; break;
    case AST::Fa_BinaryOp::OP_SUB: bc_op = Fa_OpCode::OP_SUB; break;
    case AST::Fa_BinaryOp::OP_MUL: bc_op = Fa_OpCode::OP_MUL; break;
    case AST::Fa_BinaryOp::OP_DIV: bc_op = Fa_OpCode::OP_DIV; break;
    case AST::Fa_BinaryOp::OP_MOD: bc_op = Fa_OpCode::OP_MOD; break;
    case AST::Fa_BinaryOp::OP_POW: bc_op = Fa_OpCode::OP_POW; break;
    case AST::Fa_BinaryOp::OP_EQ: bc_op = Fa_OpCode::OP_EQ; break;
    case AST::Fa_BinaryOp::OP_NEQ: bc_op = Fa_OpCode::OP_NEQ; break;
    case AST::Fa_BinaryOp::OP_LT: bc_op = Fa_OpCode::OP_LT; break;
    case AST::Fa_BinaryOp::OP_LTE: bc_op = Fa_OpCode::OP_LTE; break;
    case AST::Fa_BinaryOp::OP_GT: bc_op = Fa_OpCode::OP_LT, swapped = true; break;
    case AST::Fa_BinaryOp::OP_GTE: bc_op = Fa_OpCode::OP_LTE, swapped = true; break;
    case AST::Fa_BinaryOp::OP_BITAND: bc_op = Fa_OpCode::OP_BITAND; break;
    case AST::Fa_BinaryOp::OP_BITOR: bc_op = Fa_OpCode::OP_BITOR; break;
    case AST::Fa_BinaryOp::OP_BITXOR: bc_op = Fa_OpCode::OP_BITXOR; break;
    case AST::Fa_BinaryOp::OP_LSHIFT: bc_op = Fa_OpCode::OP_LSHIFT; break;
    case AST::Fa_BinaryOp::OP_RSHIFT: bc_op = Fa_OpCode::OP_RSHIFT; break;
    default:
        return report_error(CompilerError::UNKNOWN_BINARY_OPERATOR, e->get_location());
    }

    if (bc_op == Fa_OpCode::OP_LSHIFT || bc_op == Fa_OpCode::OP_RSHIFT) {
        auto amount_expr = dynamic_cast<AST::Fa_LiteralExpr*>(e->get_right());
        if (amount_expr == nullptr || !amount_expr->is_integer())
            return report_error(CompilerError::SHIFT_AMOUNT_NOT_CONSTANT, amount_expr->get_location());

        i64 amount = amount_expr->get_int();
        if (amount < 0 || amount > 63)
            return report_error(CompilerError::SHIFT_AMOUNT_OUT_OF_RANGE, amount_expr->get_location());

        RegMark mark(m_current);
        Fa_ExprResult expr_result;
        u8 lhs;

        COMPILE_EXPR_IMPL(e->get_left(), &expr_result);
        ANY_REG(expr_result, loc, &lhs);
        u32 pc = emit(Fa_make_ABC(bc_op, 0, lhs, static_cast<u8>(amount)), loc);
        u8 ic = current_chunk()->alloc_ic_slot();
        emit(Fa_make_ABC(Fa_OpCode::NOP, ic, 0, 0), loc);
        return Fa_ExprResult::reloc(pc);
    }

    RegMark mark(m_current);
    Fa_ExprResult lhs_ret, rhs_ret;
    u8 lhs, rhs;

    COMPILE_EXPR_IMPL(e->get_left(), &lhs_ret);
    COMPILE_EXPR_IMPL(e->get_right(), &rhs_ret);
    ANY_REG(lhs_ret, loc, &lhs);
    ANY_REG(rhs_ret, loc, &rhs);

    if (swapped)
        std::swap(lhs, rhs);

    u32 pc = emit(Fa_make_ABC(bc_op, 0, lhs, rhs), loc);
    u8 ic = current_chunk()->alloc_ic_slot();
    emit(Fa_make_ABC(Fa_OpCode::NOP, ic, 0, 0), loc);
    return Fa_ExprResult::reloc(pc);
}

bool Compiler::is_declaration(AST::Fa_AssignmentExpr const* e) const
{
    if (!AST::is_name(e->get_target()))
        // complex expression cannot be used for decl
        return false;

    auto name = AS_CONST_NAME(e->get_target());
    if (lookup_local(name->get_value()))
        return false;
    return true;
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_assign_impl(AST::Fa_AssignmentExpr* e)
{
    Fa_SourceLocation loc = e->get_location();
    AST::Fa_Expr* target = e->get_target();

    if (AST::is_index(target)) {
        auto index_expr = AS_INDEX(target);
        RegMark mark(m_current);
        Fa_ExprResult list_expr_result, index_expr_result, value_expr_result;
        u8 list_reg, index_reg, value_reg;

        COMPILE_EXPR_IMPL(index_expr->get_object(), &list_expr_result);
        COMPILE_EXPR_IMPL(index_expr->get_index(), &index_expr_result);
        COMPILE_EXPR_IMPL(e->get_value(), &value_expr_result);
        ANY_REG(list_expr_result, loc, &list_reg);
        ANY_REG(index_expr_result, loc, &index_reg);
        ANY_REG(value_expr_result, loc, &value_reg);
        emit(Fa_make_ABC(Fa_OpCode::LIST_SET, list_reg, index_reg, value_reg), loc);
        return Fa_ExprResult::reg(value_reg);
    }

    if (target->get_kind() == AST::Fa_Expr::Kind::GET) {
        auto get_expr = AS_GET_EXPR(target);
        if (AST::Fa_NameExpr* member_name = as_simple_member_name(get_expr->get_member())) {
            // Fast path: receiver's class is already registered in
            // m_class_registry (e.g. `obj.field := x` where obj's class
            // finished compiling earlier).
            if (ClassDesc const* desc = resolve_receiver_class(get_expr->get_object())) {
                int field_idx = desc->field_index(member_name->get_value());
                if (field_idx >= 0) {
                    RegMark mark(m_current);
                    Fa_ExprResult object_expr_result, value_expr_result;
                    u8 object_reg, value_reg;

                    COMPILE_EXPR_IMPL(get_expr->get_object(), &object_expr_result);
                    COMPILE_EXPR_IMPL(e->get_value(), &value_expr_result);
                    ANY_REG(object_expr_result, loc, &object_reg);
                    ANY_REG(value_expr_result, loc, &value_reg);
                    emit(Fa_make_ABC(Fa_OpCode::SET_FIELD, object_reg,
                             static_cast<u8>(field_idx), value_reg),
                        loc);
                    return Fa_ExprResult::reg(value_reg);
                }
            }

            // `this.field := x` inside the class's own method body, while
            // that class is still being compiled. m_class_registry doesn't
            // have this class yet (compile_class_def registers it only
            // after all methods finish compiling), but
            // state.class_field_names was pre-populated from the parser's
            // this.field-assignment scan before any method body compiled,
            // so current_method_field_index() already knows about this
            // field even though resolve_receiver_class() can't see it yet.
            if (is_this_reference(get_expr->get_object())) {
                int field_idx = current_method_field_index(member_name->get_value());
                if (field_idx >= 0) {
                    RegMark mark(m_current);
                    LocalVar const* self = lookup_local(kClassInstanceName);
                    if (self != nullptr) {
                        Fa_ExprResult expr_result;
                        u8 value_reg;

                        COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
                        ANY_REG(expr_result, loc, &value_reg);
                        emit(Fa_make_ABC(Fa_OpCode::SET_FIELD, self->reg,
                                 static_cast<u8>(field_idx), value_reg),
                            loc);
                        return Fa_ExprResult::reg(value_reg);
                    }
                }
            }
        }
    }

    auto name = dynamic_cast<AST::Fa_NameExpr*>(target);
    if (name == nullptr)
        return report_error(CompilerError::INVALID_ASSIGNMENT_TARGET, target->get_location());

    if (is_declaration(e)) {
        if (m_current->is_top_level && m_current->scope_depth == 0) {
            if (!infer_constructed_class(e->get_value()).empty())
                goto instance_decl;

            RegMark mark(m_current);
            Fa_ExprResult expr_result;
            u8 src;

            COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
            ANY_REG(expr_result, loc, &src);
            u16 kidx = intern_string(name->get_value());
            emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
            return Fa_ExprResult::reg(src);
        }
    instance_decl:
        u8 reg;
        Fa_ExprResult expr_result;

        ALLOC_REG(&reg);
        COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
        discharge(expr_result, reg, loc);
        declare_local(name->get_value(), reg, infer_constructed_class(e->get_value()));
        return Fa_ExprResult::reg(reg);
    }

    if (int field_idx = current_method_field_index(name->get_value()); field_idx >= 0) {
        RegMark mark(m_current);
        LocalVar const* self = lookup_local(kClassInstanceName);
        if (self == nullptr)
            return report_error(CompilerError::INVALID_ASSIGNMENT_TARGET, name->get_location());

        Fa_ExprResult expr_result;
        u8 value_reg;

        COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
        ANY_REG(expr_result, loc, &value_reg);
        emit(Fa_make_ABC(Fa_OpCode::SET_FIELD, self->reg, static_cast<u8>(field_idx), value_reg), loc);
        return Fa_ExprResult::reg(value_reg);
    }

    VarInfo vi = resolve_name(name->get_value());
    if (vi.kind == VarInfo::Kind::LOCAL) {
        auto ret = compile_expr(e->get_value(), &vi.index);
        return Fa_ExprResult::reg(vi.index);
    }

    if (!m_current->is_top_level) {
        u8 reg;
        Fa_ExprResult expr_result;

        ALLOC_REG(&reg);
        COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
        discharge(expr_result, reg, loc);
        declare_local(name->get_value(), reg);
        return Fa_ExprResult::reg(reg);
    }

    RegMark mark(m_current);
    Fa_ExprResult expr_result;
    u8 src;

    COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
    ANY_REG(expr_result, loc, &src);
    u16 kidx = intern_string(name->get_value());
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
    return Fa_ExprResult::reg(src);
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_call_impl(AST::Fa_CallExpr* e, u8* dst, bool tail)
{
    Fa_SourceLocation loc = e->get_location();
    auto fn_reg_ret = dst ? *dst : alloc_register();
    Fa_VERIFY_RESULT(fn_reg_ret);
    u8 fn_reg = fn_reg_ret.value();

    if (auto* get = dynamic_cast<AST::Fa_GetExpr*>(e->get_callee())) {
        if (AST::Fa_NameExpr* member_name = as_simple_member_name(get->get_member())) {
            if (ClassDesc const* desc = resolve_receiver_class(get->get_object())) {
                int slot = desc->method_slot(member_name->get_value());
                if (slot >= 0) {
                    // FAST PATH: statically known instance + known method slot.
                    u8 receiver_reg, reserved_reg;
                    Fa_ExprResult expr_result;

                    ALLOC_REG(&receiver_reg);
                    COMPILE_EXPR_IMPL(get->get_object(), &expr_result);
                    discharge(expr_result, receiver_reg, get->get_object()->get_location());
                    ALLOC_REG(&reserved_reg); // reserve callee frame slot 0 for implicit self

                    for (AST::Fa_Expr* arg : e->get_args()) {
                        u8 arg_reg;
                        Fa_ExprResult expr_result;
                        ALLOC_REG(&arg_reg);
                        COMPILE_EXPR_IMPL(arg, &expr_result);
                        discharge(expr_result, arg_reg, loc);
                    }

                    u8 argc = static_cast<u8>(e->get_args().size() + 1); // +1 for self
                    emit(Fa_make_ABC(Fa_OpCode::INVOKE, receiver_reg, static_cast<u8>(slot), argc), loc);
                    emit(Fa_make_ABC(Fa_OpCode::NOP, current_chunk()->alloc_ic_slot(), 0, 0), loc);

                    if (tail && !m_current->is_top_level)
                        emit(Fa_make_ABC(Fa_OpCode::RETURN, receiver_reg, 1, 0), loc);

                    m_current->free_regs_to(receiver_reg + 1);
                    return Fa_ExprResult::reg(receiver_reg);
                }
                // Name resolved to the class but not to a known method
                // (e.g. dynamically-added attribute) — fall through.
            }
        }

        // SLOW PATH — unchanged dict-style dispatch for unknown receivers.
        u8 receiver_reg;
        ALLOC_REG(&receiver_reg);
        Fa_ExprResult expr_result;
        COMPILE_EXPR_IMPL(get->get_object(), &expr_result);
        discharge(expr_result, receiver_reg, get->get_object()->get_location());

        u8 member_reg;
        ALLOC_REG(&member_reg);
        if (AST::Fa_NameExpr* member_name = as_simple_member_name(get->get_member())) {
            emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, member_reg,
                     intern_string(member_name->get_value())),
                get->get_member()->get_location());
        } else {
            Fa_ExprResult expr_result;
            COMPILE_EXPR_IMPL(get->get_member(), &expr_result);
            discharge(expr_result, member_reg, get->get_member()->get_location());
        }

        emit(Fa_make_ABC(Fa_OpCode::INDEX, fn_reg, receiver_reg, member_reg), loc);
        m_current->free_regs_to(receiver_reg + 1);

        for (AST::Fa_Expr* arg : e->get_args()) {
            u8 arg_reg;
            ALLOC_REG(&arg_reg);
            Fa_ExprResult expr_result;
            COMPILE_EXPR_IMPL(arg, &expr_result);
            discharge(expr_result, arg_reg, loc);
            m_current->free_regs_to(arg_reg + 1);
        }

        u8 argc = static_cast<u8>(e->get_args().size() + 1);
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

    // Plain function call (no GetExpr callee) — unchanged.
    Fa_ExprResult expr_result;
    COMPILE_EXPR_IMPL(e->get_callee(), &expr_result);
    discharge(expr_result, fn_reg, loc);

    for (AST::Fa_Expr* arg : e->get_args()) {
        u8 arg_reg;
        ALLOC_REG(&arg_reg);
        Fa_ExprResult expr_result;
        COMPILE_EXPR_IMPL(arg, &expr_result);
        discharge(expr_result, arg_reg, loc);
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

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_list_impl(AST::Fa_ListExpr* e)
{
    Fa_SourceLocation loc = e->get_location();

    if (e->size() > 0xFF)
        return report_error(CompilerError::TOO_MANY_LIST_ELEMENTS, loc);

    u8 dst;
    ALLOC_REG(&dst);

    u8 list_reg;
    ALLOC_REG(&list_reg);
    auto cap = static_cast<u8>(e->size());
    emit(Fa_make_ABC(Fa_OpCode::LIST_NEW, list_reg, cap, 0), loc);

    for (AST::Fa_Expr* elem : e->get_elements()) {
        Fa_ExprResult expr_result;
        u8 reg;
        ALLOC_REG(&reg);
        COMPILE_EXPR_IMPL(elem, &expr_result);
        discharge(expr_result, reg, loc);
        emit(Fa_make_ABC(Fa_OpCode::LIST_APPEND, list_reg, reg, 0), loc);
        m_current->free_regs_to(reg);
    }

    if (list_reg != dst)
        emit(Fa_make_ABC(Fa_OpCode::MOVE, dst, list_reg, 0), loc);

    m_current->free_regs_to(dst + 1);
    return Fa_ExprResult::reg(dst);
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_index_impl(AST::Fa_IndexExpr* e)
{
    Fa_SourceLocation loc = e->get_location();
    RegMark mark(m_current);
    Fa_ExprResult object_expr_result, index_expr_result;
    COMPILE_EXPR_IMPL(e->get_object(), &object_expr_result);
    COMPILE_EXPR_IMPL(e->get_index(), &index_expr_result);
    u8 object_reg, index_reg;
    ANY_REG(object_expr_result, loc, &object_reg);
    ANY_REG(index_expr_result, loc, &index_reg);
    u32 pc = emit(Fa_make_ABC(Fa_OpCode::INDEX, 0, object_reg, index_reg), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_dict_impl(AST::Fa_DictExpr* e)
{
    Fa_SourceLocation loc = e->get_location();

    u8 dst;
    ALLOC_REG(&dst); // reserve the expression's result register FIRST

    u8 fn_reg;
    ALLOC_REG(&fn_reg);
    u16 kidx = intern_string("قاموس");
    emit(Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL, fn_reg, kidx), loc);

    for (auto const& [key, value] : e->get_content()) {
        u8 key_reg;
        Fa_ExprResult expr_result;
        ALLOC_REG(&key_reg);
        COMPILE_EXPR_IMPL(key, &expr_result);
        discharge(expr_result, key_reg, key->get_location());

        u8 value_reg;
        Fa_ExprResult val_expr_result;
        ALLOC_REG(&value_reg);
        COMPILE_EXPR_IMPL(value, &val_expr_result);
        discharge(val_expr_result, value_reg, value->get_location());
    }

    u8 argc = static_cast<u8>(e->get_content().size() * 2);
    u8 ic = current_chunk()->alloc_ic_slot();
    emit(Fa_make_ABC(Fa_OpCode::IC_CALL, fn_reg, argc, ic), loc);

    if (fn_reg != dst)
        emit(Fa_make_ABC(Fa_OpCode::MOVE, dst, fn_reg, 0), loc);

    m_current->free_regs_to(dst + 1);
    return Fa_ExprResult::reg(dst);
}

Fa_ErrorOr<Fa_ExprResult> Compiler::compile_get_impl(AST::Fa_GetExpr* e)
{
    Fa_SourceLocation loc = e->get_location();

    if (AST::Fa_NameExpr* member_name = as_simple_member_name(e->get_member())) {
        if (ClassDesc const* desc = resolve_receiver_class(e->get_object())) {
            int idx = desc->field_index(member_name->get_value());
            if (idx >= 0) {
                RegMark mark(m_current);
                Fa_ExprResult expr_result;
                u8 obj_reg;

                COMPILE_EXPR_IMPL(e->get_object(), &expr_result);
                ANY_REG(expr_result, loc, &obj_reg);

                u32 pc = emit(Fa_make_ABC(Fa_OpCode::GET_FIELD, 0, obj_reg, static_cast<u8>(idx)), loc);
                return Fa_ExprResult::reloc(pc);
            }
            // Name matches the class but isn't a field — could be a bound
            // method reference; fall through to the slow path below.
        }

        // `this.field` read inside the class's own method body, while that
        // class is still being compiled — same m_class_registry-not-yet-
        // populated situation as compile_assign_impl/compile_assignment_stmt.
        // Use current_method_field_index() instead, which was pre-populated
        // before any method body started compiling.
        if (is_this_reference(e->get_object())) {
            int idx = current_method_field_index(member_name->get_value());
            if (idx >= 0) {
                LocalVar const* self = lookup_local(kClassInstanceName);
                if (self != nullptr) {
                    u32 pc = emit(Fa_make_ABC(Fa_OpCode::GET_FIELD, 0, self->reg, static_cast<u8>(idx)), loc);
                    return Fa_ExprResult::reloc(pc);
                }
            }
        }
    }

    RegMark mark(m_current);
    Fa_ExprResult object_expr_result;
    u8 object_reg;

    COMPILE_EXPR_IMPL(e->get_object(), &object_expr_result);
    ANY_REG(object_expr_result, loc, &object_reg);

    u8 member_reg;
    ALLOC_REG(&member_reg);

    if (AST::Fa_NameExpr* member_name = as_simple_member_name(e->get_member())) {
        emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, member_reg,
                 intern_string(member_name->get_value())),
            e->get_member()->get_location());
    } else {
        Fa_ExprResult expr_result;
        COMPILE_EXPR_IMPL(e->get_member(), &expr_result);
        discharge(expr_result, member_reg, e->get_member()->get_location());
    }

    u32 pc = emit(Fa_make_ABC(Fa_OpCode::INDEX, 0, object_reg, member_reg), loc);
    return Fa_ExprResult::reloc(pc);
}

void Compiler::discharge(Fa_ExprResult const& r, u8 dst, Fa_SourceLocation loc)
{
    switch (r.kind) {
    case Fa_ExprResult::Kind::REG:
        if (r.reg_ != dst)
            emit(Fa_make_ABC(Fa_OpCode::MOVE, dst, r.reg_, 0), loc);
        break;
    case Fa_ExprResult::Kind::RELOC: patch_a(current_chunk(), r.reloc_pc, dst); break;
    case Fa_ExprResult::Kind::KINT: emit_load_value(dst, Fa_MAKE_INTEGER(r.ival), loc); break;
    case Fa_ExprResult::Kind::KFLOAT: emit_load_value(dst, Fa_MAKE_REAL(r.dval), loc); break;
    case Fa_ExprResult::Kind::KBOOL: emit_load_value(dst, Fa_MAKE_BOOL(r.bval), loc); break;
    case Fa_ExprResult::Kind::KNIL: emit_load_value(dst, Fa_MAKE_NIL(), loc); break;
    }
}

Fa_ErrorOr<u8> Compiler::any_reg(Fa_ExprResult const& r, Fa_SourceLocation loc)
{
    if (r.kind == Fa_ExprResult::Kind::REG)
        return r.reg_;

    u8 dst;
    ALLOC_REG(&dst);
    discharge(r, dst, loc);
    return dst;
}

Fa_ErrorOr<u8> Compiler::compile_expr(AST::Fa_Expr* e, u8* dst)
{
    if (e == nullptr)
        return error_reg();

    if (dst != nullptr)
        reserve_register(*dst);

    Fa_SourceLocation loc = e->get_location();
    Fa_ExprResult r;
    COMPILE_EXPR_IMPL(e, &r);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

u8 Compiler::error_reg() const { return 0; }

Fa_ErrorOr<u8> Compiler::alloc_register()
{
    u8 reg = m_current->alloc_register();
    if (reg >= MAX_REGS)
        return report_error(CompilerError::TOO_MANY_REGISTERS, { });
    return reg;
}

void Compiler::declare_local(Fa_StringRef const& name, u8 m_reg)
{
    declare_local(name, m_reg, "");
}

void Compiler::declare_local(Fa_StringRef const& name, u8 m_reg, Fa_StringRef const& known_class)
{
    m_current->locals.push({ name, m_current->scope_depth, m_reg, known_class });
}

LocalVar const* Compiler::lookup_local(Fa_StringRef const& name) const
{
    auto const& locals = m_current->locals;
    for (auto i = static_cast<int>(locals.size()) - 1; i >= 0; i -= 1) {
        if (locals[i].name == name)
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

u32 Compiler::emit_jump(Fa_OpCode op, u8 cond, Fa_SourceLocation loc)
{
    return emit(Fa_make_AsBx(op, cond, 0), loc);
}

void Compiler::patch_jump(u32 idx)
{
    if (!current_chunk()->patch_jump(idx))
        diagnostic::panic(CompilerError::JUMP_OFFSET_OVERFLOW);
}

void Compiler::push_loop(u32 loop_start)
{
    m_current->loop_stack.push({ { }, { }, loop_start });
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
        diagnostic::panic(CompilerError::LOOP_JUMP_OFFSET_OVERFLOW);

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

    Fa_ObjString* obj = get_allocator().allocate_object<Fa_ObjString>();
    obj->str = str;
    u16 idx = chunk->add_constant(Fa_MAKE_OBJECT(obj));
    m_string_cache[key] = idx;
    return idx;
}

Compiler::ClassDesc const* Compiler::resolve_receiver_class(AST::Fa_Expr const* e) const
{
    using EK = AST::Fa_Expr::Kind;
    EK e_kind = e->get_kind();
    if (e_kind != EK::NAME)
        return nullptr;

    Fa_StringRef const name = AS_CONST_NAME(e)->get_value();

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

Fa_StringRef Compiler::infer_constructed_class(AST::Fa_Expr const* e) const
{
    if (e == nullptr || e->get_kind() != AST::Fa_Expr::Kind::CALL)
        return "";

    auto const* call = AS_CONST_CALL(e);
    AST::Fa_Expr const* callee = call->get_callee();
    if (callee == nullptr || callee->get_kind() != AST::Fa_Expr::Kind::NAME)
        return "";

    Fa_StringRef name = AS_CONST_NAME(callee)->get_value();
    return m_class_registry.find_ptr(name) != nullptr ? name : Fa_StringRef { "" };
}

int Compiler::current_method_field_index(Fa_StringRef const& name) const
{
    if (m_current == nullptr || !m_current->is_class_method)
        return -1;

    for (u32 i = 0, n = m_current->class_field_names.size(); i < n; i += 1) {
        if (m_current->class_field_names[i] == name)
            return static_cast<int>(i);
    }

    return -1;
}

} // namespace fairuz::runtime
