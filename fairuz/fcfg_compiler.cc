//
// fcompiler.cc
//

#include "fcfg_compiler.hpp"
#include "fAST.hpp"
#include "fcfg.hpp"
#include "fdiagnostic.hpp"
#include "ferror.hpp"
#include "fmacros.hpp"
#include "fopcode.hpp"
#include "foptim.hpp"
#include "fstring.hpp"
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

static u8 const ERROR_REG = 0;

namespace fairuz::runtime {

using CompilerError = diagnostic::errc::compiler::Code;
using cmp_ret = Fa_ErrorOr<CFG_Fa_ExprResult>;

static constexpr char kClassInstanceName[] = "__class$instance";
static constexpr char kClassMetadataKey[] = "__class__";

static bool is_terminal_top_level_call(AST::Fa_Stmt const* s)
{
    auto const* expr_stmt = dynamic_cast<AST::Fa_ExprStmt const*>(s);
    if (expr_stmt == nullptr)
        return false;

    return dynamic_cast<AST::Fa_CallExpr const*>(expr_stmt->get_expr()) != nullptr;
}

bool CFG_Compiler::is_declaration(AST::Fa_AssignmentExpr const* e) const
{
    if (!AST::is_name(e->get_target()))
        // complex expression cannot be used for decl
        return false;

    auto name = AS_CONST_NAME(e->get_target());
    if (lookup_local(name->get_value()))
        return false;
    return true;
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

Fa_Chunk* CFG_Compiler::compile(Fa_Program* program)
{
    if (!program)
        return nullptr;

    set_program(program);

    Fa_CFG* main = program->get_main();
    Fa_Chunk* chunk = make_chunk();
    chunk->name = "<main>";
    CFG_CompilerState state;
    state.chunk = chunk;
    state.func_name = chunk->name;
    state.is_top_level = true;
    state.enclosing = nullptr;
    m_current = &state;

    auto ret = compile_cfg_body(main, true);
    if (ret.has_error()) {
        if (diagnostic::has_errors())
            diagnostic::dump();
        return nullptr;
    }

    return chunk;
}

Fa_ErrorOr<bool> CFG_Compiler::compile_stmt(AST::Fa_Stmt* s)
{
    if (s == nullptr || m_current->is_dead)
        return true;

    switch (s->get_kind()) {
    case AST::Fa_Stmt::Kind::BLOCK: return compile_block(AS_BLOCK(s));
    case AST::Fa_Stmt::Kind::EXPR: return compile_expr_stmt(AS_EXPR_STMT(s));
    case AST::Fa_Stmt::Kind::ASSIGNMENT: return compile_assignment_stmt(AS_ASSIGNMENT_STMT(s));
    case AST::Fa_Stmt::Kind::FUNC: return compile_function_def(AS_FUNCTION_DEF(s));
    case AST::Fa_Stmt::Kind::RETURN: return compile_return(AS_RETURN(s));
    case AST::Fa_Stmt::Kind::CLASS_DEF: return compile_class_def(AS_CLASS_DEF(s));
    case AST::Fa_Stmt::Kind::INVALID:
    default:
        return report_error(CompilerError::INVALID_STATEMENT_NODE, s->get_location());
    }
}

Fa_ErrorOr<bool> CFG_Compiler::compile_block(AST::Fa_BlockStmt* s)
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

Fa_ErrorOr<bool> CFG_Compiler::compile_expr_stmt(AST::Fa_ExprStmt* s)
{
    CFG_RegMark mark(m_current);
    CFG_Fa_ExprResult r;
    COMPILE_EXPR_IMPL(s->get_expr(), &r);
    u8 tmp;
    ANY_REG(r, s->get_location(), &tmp);
    return true;
}

Fa_ErrorOr<bool> CFG_Compiler::compile_assignment_stmt(AST::Fa_AssignmentStmt* s)
{
    Fa_SourceLocation loc = s->get_location();

    if (auto* index_expr = dynamic_cast<AST::Fa_IndexExpr*>(s->get_target())) {
        CFG_RegMark mark(m_current);
        CFG_Fa_ExprResult object_expr_result, index_expr_result, value_expr_result;
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
                    CFG_RegMark mark(m_current);
                    CFG_Fa_ExprResult object_expr_result, value_expr_result;
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
                    CFG_RegMark mark(m_current);
                    CFG_LocalVar const* self = lookup_local(kClassInstanceName);
                    if (self != nullptr) {
                        CFG_Fa_ExprResult value_expr_result;
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
        if (CFG_LocalVar const* local = lookup_local(name->get_value())) {
            u8 local_reg = local->reg;
            return compile_expr(s->get_value(), &local_reg).error_or(true);
        }

        u8 reg;
        ALLOC_REG(&reg);
        CFG_Fa_ExprResult value_expr_result;
        COMPILE_EXPR_IMPL(s->get_value(), &value_expr_result);
        discharge(value_expr_result, reg, loc);
        declare_local(name->get_value(), reg, infer_constructed_class(s->get_value()));
        return true;
    }

    if (int field_idx = current_method_field_index(name->get_value()); field_idx >= 0) {
        CFG_RegMark mark(m_current);
        CFG_LocalVar const* self = lookup_local(kClassInstanceName);
        if (self == nullptr)
            return report_error(CompilerError::INVALID_ASSIGNMENT_TARGET, name->get_location());

        CFG_Fa_ExprResult value_expr_result;
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
        CFG_Fa_ExprResult expr_result;
        COMPILE_EXPR_IMPL(s->get_value(), &expr_result);
        discharge(expr_result, reg, loc);
        declare_local(name->get_value(), reg);
        return true;
    }

    CFG_RegMark mark(m_current);
    CFG_Fa_ExprResult expr_result;
    COMPILE_EXPR_IMPL(s->get_value(), &expr_result);
    u8 src;
    ANY_REG(expr_result, loc, &src);
    u16 kidx = intern_string(name->get_value());
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
    return true;
}

Fa_ErrorOr<bool> CFG_Compiler::compile_function_def(Fa_CFG_Function* f)
{
    AST::Fa_FunctionDef* fn_node = f->def;
    Fa_SourceLocation loc = fn_node->get_location();
    if (!m_current->is_top_level || m_current->scope_depth != 0)
        return report_error(CompilerError::NESTED_FUNCTION_UNSUPPORTED, loc);

    Fa_StringRef fn_name = fn_node->get_name()->get_value();
    Fa_Chunk* fn_chunk = make_chunk();
    fn_chunk->name = fn_name;
    fn_chunk->arity = fn_node->has_parameters() ? static_cast<int>(fn_node->get_parameters().size()) : 0;
    auto fn_idx = static_cast<u16>(current_chunk()->functions.size());
    current_chunk()->functions.push(fn_chunk);
    CFG_CompilerState fn_state;
    fn_state.chunk = fn_chunk;
    fn_state.func_name = fn_name;
    fn_state.enclosing = m_current;
    m_current = &fn_state;
    begin_scope();

    if (fn_node->has_parameters()) {
        for (AST::Fa_Expr* param : fn_node->get_parameters()) {
            u8 reg;
            ALLOC_REG(&reg);
            declare_local(AS_CONST_NAME(param)->get_value(), reg);
        }
    }

    // we call it as top level functions simply
    // because nested functions are not supported
    auto ret_body = compile_cfg_body(f->cfg, true);
    Fa_VERIFY_RESULT(ret_body);

    end_scope(loc);
    fn_chunk->local_count = fn_state.max_reg;
    m_current = fn_state.enclosing;
    u8 dst;
    ALLOC_REG(&dst);
    emit(Fa_make_ABx(Fa_OpCode::CLOSURE, dst, fn_idx), loc);

    if (m_current != nullptr && m_current->is_top_level) {
        u16 name_idx = intern_string(fn_name);
        emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, dst, name_idx), loc);
    }

    declare_local(fn_name, dst);
    return true;
}

Fa_ErrorOr<bool> CFG_Compiler::compile_function_def(AST::Fa_FunctionDef* f)
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

    CFG_CompilerState fn_state;
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

Fa_ErrorOr<bool> CFG_Compiler::compile_return(AST::Fa_ReturnStmt* s)
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
        CFG_RegMark mark(m_current);
        auto call_ret = compile_call_impl(AS_CALL(value), nullptr, true);
        Fa_VERIFY_RESULT(call_ret);
        (void)call_ret;
        m_current->is_dead = true;
        return true;
    }

    CFG_RegMark mark(m_current);
    CFG_Fa_ExprResult expr_result;
    u8 src;
    COMPILE_EXPR_IMPL(value, &expr_result);
    ANY_REG(expr_result, loc, &src);
    emit(Fa_make_ABC(Fa_OpCode::RETURN, src, 1, 0), loc);
    m_current->is_dead = true;
    return true;
}

Fa_ErrorOr<bool> CFG_Compiler::compile_class_def(AST::Fa_ClassDef* s)
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

    auto compile_method_closure = [&](Fa_CFG_Function* method) -> Fa_ErrorOr<std::tuple<u8, Fa_Chunk*>> {
        assert(method != nullptr);
        AST::Fa_FunctionDef* method_node = method->def;
        Fa_SourceLocation method_loc = method_node->get_location();
        Fa_StringRef method_name = class_name + "." + method_node->get_name()->get_value();
        Fa_Chunk* ch = make_chunk();
        ch->name = method_name;
        int ex_param_count = method_node->has_parameters() ? static_cast<int>(method_node->get_parameters().size()) : 0;
        ch->arity = ex_param_count + 1; // always adding implicit 'this' parameter
        auto fn_idx = static_cast<u16>(current_chunk()->functions.size());
        current_chunk()->functions.push(ch);
        CFG_CompilerState state;
        state.chunk = ch;
        state.func_name = method_name;
        state.enclosing = m_current;
        state.is_class_method = true;
        state.class_field_names = field_names;
        m_current = &state;

        begin_scope();
        u8 inst_reg;
        ALLOC_REG(&inst_reg);
        declare_local(kClassInstanceName, inst_reg, class_name);

        if (method_node->has_parameters()) {
            for (AST::Fa_Expr* p : method_node->get_parameters()) {
                u8 reg;
                ALLOC_REG(&reg);
                declare_local(AS_CONST_NAME(p)->get_value(), reg);
            }
        }

        auto ret_body = compile_cfg_body(method->cfg, false);
        Fa_VERIFY_RESULT(ret_body);

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
        assert(m_program && "==> DEBUG: m_program is null here");
        auto method_cfg = m_program->cfg_of_method(s, method_name);
        if (method_cfg == nullptr)
            return report_error(CompilerError::MISSING_CFG, loc,
                "missing cfg for this method: " + std::string(method->get_name()->get_value().data()) + "\n");
        auto result = compile_method_closure(method_cfg);
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

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_expr_impl(AST::Fa_Expr* e)
{
    if (e == nullptr)
        return CFG_Fa_ExprResult::knil();

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

    return CFG_Fa_ExprResult::knil();
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_literal_impl(AST::Fa_LiteralExpr* e)
{
    if (e->is_string()) {
        u16 kidx = intern_string(e->get_str());
        u32 pc = emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, 0, kidx), e->get_location());
        return CFG_Fa_ExprResult::reloc(pc);
    }
    if (e->is_integer())
        return CFG_Fa_ExprResult::kint(e->get_int());
    if (e->is_float())
        return CFG_Fa_ExprResult::kfloat(e->get_float());
    if (e->is_bool())
        return CFG_Fa_ExprResult::kbool(e->get_bool());
    if (e->is_nil())
        return CFG_Fa_ExprResult::knil();

    return report_error(CompilerError::UNKNOWN_LITERAL_TYPE, e->get_location());
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_name_impl(AST::Fa_NameExpr* e)
{
    Fa_SourceLocation loc = e->get_location();
    VarInfo vi = resolve_name(e->get_value());

    if (vi.kind == VarInfo::Kind::LOCAL)
        return CFG_Fa_ExprResult::reg(vi.index);

    if (int field_idx = current_method_field_index(e->get_value()); field_idx >= 0) {
        CFG_LocalVar const* self = lookup_local(kClassInstanceName);
        if (self == nullptr)
            return report_error(CompilerError::INVALID_EXPRESSION_NODE, e->get_location());

        u32 pc = emit(Fa_make_ABC(Fa_OpCode::GET_FIELD, 0, self->reg, static_cast<u8>(field_idx)), loc);
        return CFG_Fa_ExprResult::reloc(pc);
    }

    u16 kidx = intern_string(e->get_value());
    u32 pc = emit(Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL, 0, kidx), loc);
    return CFG_Fa_ExprResult::reloc(pc);
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_unary_impl(AST::Fa_UnaryExpr* e)
{
    Fa_SourceLocation loc = e->get_location();

    if (auto folded = try_fold_unary(e)) {
        Fa_Value v = *folded;
        if (Fa_IS_INTEGER(v))
            return CFG_Fa_ExprResult::kint(Fa_AS_INTEGER(v));
        if (Fa_IS_DOUBLE(v))
            return CFG_Fa_ExprResult::kfloat(Fa_AS_DOUBLE(v));
        if (Fa_IS_BOOL(v))
            return CFG_Fa_ExprResult::kbool(Fa_AS_BOOL(v));
        if (Fa_IS_NIL(v))
            return CFG_Fa_ExprResult::knil();
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

    CFG_RegMark mark(m_current);
    CFG_Fa_ExprResult expr_result;
    COMPILE_EXPR_IMPL(e->get_operand(), &expr_result);
    u8 src;
    ANY_REG(expr_result, loc, &src);
    u32 pc = emit(Fa_make_ABC(op, 0, src, 0), loc);
    return CFG_Fa_ExprResult::reloc(pc);
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_binary_impl(AST::Fa_BinaryExpr* e)
{
    Fa_SourceLocation loc = e->get_location();

    if (auto folded = try_fold_binary(e)) {
        Fa_Value v = *folded;
        if (Fa_IS_INTEGER(v))
            return CFG_Fa_ExprResult::kint(Fa_AS_INTEGER(v));
        if (Fa_IS_DOUBLE(v))
            return CFG_Fa_ExprResult::kfloat(Fa_AS_DOUBLE(v));
        if (Fa_IS_BOOL(v))
            return CFG_Fa_ExprResult::kbool(Fa_AS_BOOL(v));
        if (Fa_IS_NIL(v))
            return CFG_Fa_ExprResult::knil();
    }

    if (auto reduced = try_strength_reduce_binary(e))
        return compile_expr_impl(*reduced);

    AST::Fa_BinaryOp op = e->get_operator();
    /// TODO: short circuit AND / OR 

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

        CFG_RegMark mark(m_current);
        CFG_Fa_ExprResult expr_result;
        u8 lhs;

        COMPILE_EXPR_IMPL(e->get_left(), &expr_result);
        ANY_REG(expr_result, loc, &lhs);
        u32 pc = emit(Fa_make_ABC(bc_op, 0, lhs, static_cast<u8>(amount)), loc);
        u8 ic = current_chunk()->alloc_ic_slot();
        emit(Fa_make_ABC(Fa_OpCode::NOP, ic, 0, 0), loc);
        return CFG_Fa_ExprResult::reloc(pc);
    }

    CFG_RegMark mark(m_current);
    CFG_Fa_ExprResult lhs_ret, rhs_ret;
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
    return CFG_Fa_ExprResult::reloc(pc);
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_assign_impl(AST::Fa_AssignmentExpr* e)
{
    Fa_SourceLocation loc = e->get_location();
    AST::Fa_Expr* target = e->get_target();

    if (AST::is_index(target)) {
        auto index_expr = AS_INDEX(target);
        CFG_RegMark mark(m_current);
        CFG_Fa_ExprResult list_expr_result, index_expr_result, value_expr_result;
        u8 list_reg, index_reg, value_reg;

        COMPILE_EXPR_IMPL(index_expr->get_object(), &list_expr_result);
        COMPILE_EXPR_IMPL(index_expr->get_index(), &index_expr_result);
        COMPILE_EXPR_IMPL(e->get_value(), &value_expr_result);
        ANY_REG(list_expr_result, loc, &list_reg);
        ANY_REG(index_expr_result, loc, &index_reg);
        ANY_REG(value_expr_result, loc, &value_reg);
        emit(Fa_make_ABC(Fa_OpCode::LIST_SET, list_reg, index_reg, value_reg), loc);
        return CFG_Fa_ExprResult::reg(value_reg);
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
                    CFG_RegMark mark(m_current);
                    CFG_Fa_ExprResult object_expr_result, value_expr_result;
                    u8 object_reg, value_reg;

                    COMPILE_EXPR_IMPL(get_expr->get_object(), &object_expr_result);
                    COMPILE_EXPR_IMPL(e->get_value(), &value_expr_result);
                    ANY_REG(object_expr_result, loc, &object_reg);
                    ANY_REG(value_expr_result, loc, &value_reg);
                    emit(Fa_make_ABC(Fa_OpCode::SET_FIELD, object_reg,
                             static_cast<u8>(field_idx), value_reg),
                        loc);
                    return CFG_Fa_ExprResult::reg(value_reg);
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
                    CFG_RegMark mark(m_current);
                    CFG_LocalVar const* self = lookup_local(kClassInstanceName);
                    if (self != nullptr) {
                        CFG_Fa_ExprResult expr_result;
                        u8 value_reg;

                        COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
                        ANY_REG(expr_result, loc, &value_reg);
                        emit(Fa_make_ABC(Fa_OpCode::SET_FIELD, self->reg,
                                 static_cast<u8>(field_idx), value_reg),
                            loc);
                        return CFG_Fa_ExprResult::reg(value_reg);
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

            CFG_RegMark mark(m_current);
            CFG_Fa_ExprResult expr_result;
            u8 src;

            COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
            ANY_REG(expr_result, loc, &src);
            u16 kidx = intern_string(name->get_value());
            emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
            return CFG_Fa_ExprResult::reg(src);
        }
    instance_decl:
        u8 reg;
        CFG_Fa_ExprResult expr_result;

        ALLOC_REG(&reg);
        COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
        discharge(expr_result, reg, loc);
        declare_local(name->get_value(), reg, infer_constructed_class(e->get_value()));
        return CFG_Fa_ExprResult::reg(reg);
    }

    if (int field_idx = current_method_field_index(name->get_value()); field_idx >= 0) {
        CFG_RegMark mark(m_current);
        CFG_LocalVar const* self = lookup_local(kClassInstanceName);
        if (self == nullptr)
            return report_error(CompilerError::INVALID_ASSIGNMENT_TARGET, name->get_location());

        CFG_Fa_ExprResult expr_result;
        u8 value_reg;

        COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
        ANY_REG(expr_result, loc, &value_reg);
        emit(Fa_make_ABC(Fa_OpCode::SET_FIELD, self->reg, static_cast<u8>(field_idx), value_reg), loc);
        return CFG_Fa_ExprResult::reg(value_reg);
    }

    VarInfo vi = resolve_name(name->get_value());
    if (vi.kind == VarInfo::Kind::LOCAL) {
        auto ret = compile_expr(e->get_value(), &vi.index);
        return CFG_Fa_ExprResult::reg(vi.index);
    }

    if (!m_current->is_top_level) {
        u8 reg;
        CFG_Fa_ExprResult expr_result;

        ALLOC_REG(&reg);
        COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
        discharge(expr_result, reg, loc);
        declare_local(name->get_value(), reg);
        return CFG_Fa_ExprResult::reg(reg);
    }

    CFG_RegMark mark(m_current);
    CFG_Fa_ExprResult expr_result;
    u8 src;

    COMPILE_EXPR_IMPL(e->get_value(), &expr_result);
    ANY_REG(expr_result, loc, &src);
    u16 kidx = intern_string(name->get_value());
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
    return CFG_Fa_ExprResult::reg(src);
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_call_impl(AST::Fa_CallExpr* e, u8* dst, bool tail)
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
                    CFG_Fa_ExprResult expr_result;

                    ALLOC_REG(&receiver_reg);
                    COMPILE_EXPR_IMPL(get->get_object(), &expr_result);
                    discharge(expr_result, receiver_reg, get->get_object()->get_location());
                    ALLOC_REG(&reserved_reg); // reserve callee frame slot 0 for implicit self

                    for (AST::Fa_Expr* arg : e->get_args()) {
                        u8 arg_reg;
                        CFG_Fa_ExprResult expr_result;
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
                    return CFG_Fa_ExprResult::reg(receiver_reg);
                }
                // Name resolved to the class but not to a known method
                // (e.g. dynamically-added attribute) — fall through.
            }
        }

        // SLOW PATH — unchanged dict-style dispatch for unknown receivers.
        u8 receiver_reg;
        ALLOC_REG(&receiver_reg);
        CFG_Fa_ExprResult expr_result;
        COMPILE_EXPR_IMPL(get->get_object(), &expr_result);
        discharge(expr_result, receiver_reg, get->get_object()->get_location());

        u8 member_reg;
        ALLOC_REG(&member_reg);
        if (AST::Fa_NameExpr* member_name = as_simple_member_name(get->get_member())) {
            emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, member_reg,
                     intern_string(member_name->get_value())),
                get->get_member()->get_location());
        } else {
            CFG_Fa_ExprResult expr_result;
            COMPILE_EXPR_IMPL(get->get_member(), &expr_result);
            discharge(expr_result, member_reg, get->get_member()->get_location());
        }

        emit(Fa_make_ABC(Fa_OpCode::INDEX, fn_reg, receiver_reg, member_reg), loc);
        m_current->free_regs_to(receiver_reg + 1);

        for (AST::Fa_Expr* arg : e->get_args()) {
            u8 arg_reg;
            ALLOC_REG(&arg_reg);
            CFG_Fa_ExprResult expr_result;
            COMPILE_EXPR_IMPL(arg, &expr_result);
            discharge(expr_result, arg_reg, loc);
            m_current->free_regs_to(arg_reg + 1);
        }

        u8 argc = static_cast<u8>(e->get_args().size() + 1);
        if (tail && !m_current->is_top_level) {
            emit(Fa_make_ABC(Fa_OpCode::CALL_TAIL, fn_reg, argc, 0), loc);
            m_current->free_regs_to(fn_reg);
            return CFG_Fa_ExprResult::reg(fn_reg);
        }

        u8 ic = current_chunk()->alloc_ic_slot();
        emit(Fa_make_ABC(Fa_OpCode::IC_CALL, fn_reg, argc, ic), loc);
        m_current->free_regs_to(fn_reg + 1);
        return CFG_Fa_ExprResult::reg(fn_reg);
    }

    // Plain function call (no GetExpr callee) — unchanged.
    CFG_Fa_ExprResult expr_result;
    COMPILE_EXPR_IMPL(e->get_callee(), &expr_result);
    discharge(expr_result, fn_reg, loc);

    for (AST::Fa_Expr* arg : e->get_args()) {
        u8 arg_reg;
        ALLOC_REG(&arg_reg);
        CFG_Fa_ExprResult expr_result;
        COMPILE_EXPR_IMPL(arg, &expr_result);
        discharge(expr_result, arg_reg, loc);
        m_current->free_regs_to(arg_reg + 1);
    }

    u8 argc = static_cast<u8>(e->get_args().size());
    if (tail && !m_current->is_top_level) {
        emit(Fa_make_ABC(Fa_OpCode::CALL_TAIL, fn_reg, argc, 0), loc);
        m_current->free_regs_to(fn_reg);
        return CFG_Fa_ExprResult::reg(fn_reg);
    }

    u8 ic = current_chunk()->alloc_ic_slot();
    emit(Fa_make_ABC(Fa_OpCode::IC_CALL, fn_reg, argc, ic), loc);
    m_current->free_regs_to(fn_reg + 1);
    return CFG_Fa_ExprResult::reg(fn_reg);
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_list_impl(AST::Fa_ListExpr* e)
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
        CFG_Fa_ExprResult expr_result;
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
    return CFG_Fa_ExprResult::reg(dst);
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_index_impl(AST::Fa_IndexExpr* e)
{
    Fa_SourceLocation loc = e->get_location();
    CFG_RegMark mark(m_current);
    CFG_Fa_ExprResult object_expr_result, index_expr_result;
    COMPILE_EXPR_IMPL(e->get_object(), &object_expr_result);
    COMPILE_EXPR_IMPL(e->get_index(), &index_expr_result);
    u8 object_reg, index_reg;
    ANY_REG(object_expr_result, loc, &object_reg);
    ANY_REG(index_expr_result, loc, &index_reg);
    u32 pc = emit(Fa_make_ABC(Fa_OpCode::INDEX, 0, object_reg, index_reg), loc);
    return CFG_Fa_ExprResult::reloc(pc);
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_dict_impl(AST::Fa_DictExpr* e)
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
        CFG_Fa_ExprResult expr_result;
        ALLOC_REG(&key_reg);
        COMPILE_EXPR_IMPL(key, &expr_result);
        discharge(expr_result, key_reg, key->get_location());

        u8 value_reg;
        CFG_Fa_ExprResult val_expr_result;
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
    return CFG_Fa_ExprResult::reg(dst);
}

Fa_ErrorOr<CFG_Fa_ExprResult> CFG_Compiler::compile_get_impl(AST::Fa_GetExpr* e)
{
    Fa_SourceLocation loc = e->get_location();

    if (AST::Fa_NameExpr* member_name = as_simple_member_name(e->get_member())) {
        if (ClassDesc const* desc = resolve_receiver_class(e->get_object())) {
            int idx = desc->field_index(member_name->get_value());
            if (idx >= 0) {
                CFG_RegMark mark(m_current);
                CFG_Fa_ExprResult expr_result;
                u8 obj_reg;

                COMPILE_EXPR_IMPL(e->get_object(), &expr_result);
                ANY_REG(expr_result, loc, &obj_reg);

                u32 pc = emit(Fa_make_ABC(Fa_OpCode::GET_FIELD, 0, obj_reg, static_cast<u8>(idx)), loc);
                return CFG_Fa_ExprResult::reloc(pc);
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
                CFG_LocalVar const* self = lookup_local(kClassInstanceName);
                if (self != nullptr) {
                    u32 pc = emit(Fa_make_ABC(Fa_OpCode::GET_FIELD, 0, self->reg, static_cast<u8>(idx)), loc);
                    return CFG_Fa_ExprResult::reloc(pc);
                }
            }
        }
    }

    CFG_RegMark mark(m_current);
    CFG_Fa_ExprResult object_expr_result;
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
        CFG_Fa_ExprResult expr_result;
        COMPILE_EXPR_IMPL(e->get_member(), &expr_result);
        discharge(expr_result, member_reg, e->get_member()->get_location());
    }

    u32 pc = emit(Fa_make_ABC(Fa_OpCode::INDEX, 0, object_reg, member_reg), loc);
    return CFG_Fa_ExprResult::reloc(pc);
}

void CFG_Compiler::discharge(CFG_Fa_ExprResult const& r, u8 dst, Fa_SourceLocation loc)
{
    switch (r.kind) {
    case CFG_Fa_ExprResult::Kind::REG:
        if (r.reg_ != dst)
            emit(Fa_make_ABC(Fa_OpCode::MOVE, dst, r.reg_, 0), loc);
        break;
    case CFG_Fa_ExprResult::Kind::RELOC: patch_a(current_chunk(), r.reloc_pc, dst); break;
    case CFG_Fa_ExprResult::Kind::KINT: emit_load_value(dst, Fa_MAKE_INTEGER(r.ival), loc); break;
    case CFG_Fa_ExprResult::Kind::KFLOAT: emit_load_value(dst, Fa_MAKE_REAL(r.dval), loc); break;
    case CFG_Fa_ExprResult::Kind::KBOOL: emit_load_value(dst, Fa_MAKE_BOOL(r.bval), loc); break;
    case CFG_Fa_ExprResult::Kind::KNIL: emit_load_value(dst, Fa_MAKE_NIL(), loc); break;
    }
}

Fa_ErrorOr<u8> CFG_Compiler::any_reg(CFG_Fa_ExprResult const& r, Fa_SourceLocation loc)
{
    if (r.kind == CFG_Fa_ExprResult::Kind::REG)
        return r.reg_;

    u8 dst;
    ALLOC_REG(&dst);
    discharge(r, dst, loc);
    return dst;
}

Fa_ErrorOr<u8> CFG_Compiler::compile_expr(AST::Fa_Expr* e, u8* dst)
{
    if (e == nullptr)
        return ERROR_REG;

    if (dst != nullptr)
        reserve_register(*dst);

    Fa_SourceLocation loc = e->get_location();
    CFG_Fa_ExprResult r;
    COMPILE_EXPR_IMPL(e, &r);
    if (dst != nullptr) {
        discharge(r, *dst, loc);
        return *dst;
    }

    return any_reg(r, loc);
}

Fa_ErrorOr<u8> CFG_Compiler::alloc_register()
{
    u8 reg = m_current->alloc_register();
    if (reg >= MAX_REGS)
        return report_error(CompilerError::TOO_MANY_REGISTERS, { });
    return reg;
}

void CFG_Compiler::declare_local(Fa_StringRef const& name, u8 m_reg)
{
    declare_local(name, m_reg, "");
}

void CFG_Compiler::declare_local(Fa_StringRef const& name, u8 m_reg, Fa_StringRef const& known_class)
{
    m_current->locals.push({ name, m_current->scope_depth, m_reg, known_class });
}

CFG_LocalVar const* CFG_Compiler::lookup_local(Fa_StringRef const& name) const
{
    auto const& locals = m_current->locals;
    for (auto i = static_cast<int>(locals.size()) - 1; i >= 0; i -= 1) {
        if (locals[i].name == name)
            return &locals[i];
    }

    return nullptr;
}

CFG_Compiler::VarInfo CFG_Compiler::resolve_name(Fa_StringRef const& name)
{
    if (CFG_LocalVar const* local = lookup_local(name))
        return { VarInfo::Kind::LOCAL, local->reg };

    return VarInfo {
        .kind = VarInfo::Kind::GLOBAL,
        .index = 0
    };
}

u32 CFG_Compiler::emit(u32 instr, Fa_SourceLocation loc)
{
    return current_chunk()->emit(instr, loc);
}

u32 CFG_Compiler::emit_jump(Fa_OpCode op, u8 cond, Fa_SourceLocation loc)
{
    return emit(Fa_make_AsBx(op, cond, 0), loc);
}

void CFG_Compiler::emit_load_value(u8 dst, Fa_Value v, Fa_SourceLocation loc)
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

Fa_Chunk* CFG_Compiler::current_chunk() const { return m_current->chunk; }

u32 CFG_Compiler::current_offset() const { return current_chunk()->code.size(); }

void CFG_Compiler::begin_scope() { m_current->scope_depth += 1; }

// wherever CFG_Compiler::end_scope lives
void CFG_Compiler::end_scope_no_reclaim(Fa_SourceLocation loc)
{
    (void)loc;
    m_current->scope_depth -= 1;
    unsigned int depth = m_current->scope_depth;
    auto& locals = m_current->locals;
    size_t pop_from = locals.size();
    while (pop_from > 0 && locals[pop_from - 1].depth > depth)
        pop_from -= 1;
    // No next_reg rollback: monotonic allocation stays monotonic.
    locals.resize(static_cast<u32>(pop_from));
}

void CFG_Compiler::end_scope(Fa_SourceLocation loc)
{
    (void)loc;
    if (m_current->scope_depth > 0)
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

u32 CFG_Compiler::intern_string(Fa_StringRef const& str)
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

CFG_Compiler::ClassDesc const* CFG_Compiler::resolve_receiver_class(AST::Fa_Expr const* e) const
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
    if (CFG_LocalVar const* local = lookup_local(name)) {
        if (!local->known_class.empty()) {
            if (auto* d = m_class_registry.find_ptr(local->known_class))
                return d;
        }
    }

    return nullptr;
}

Fa_StringRef CFG_Compiler::infer_constructed_class(AST::Fa_Expr const* e) const
{
    if (e == nullptr || e->get_kind() != AST::Fa_Expr::Kind::CALL)
        return { };

    auto const* call = AS_CONST_CALL(e);
    AST::Fa_Expr const* callee = call->get_callee();
    if (callee == nullptr || callee->get_kind() != AST::Fa_Expr::Kind::NAME)
        return { };

    Fa_StringRef name = AS_CONST_NAME(callee)->get_value();
    return m_class_registry.find_ptr(name) != nullptr ? name : Fa_StringRef { "" };
}

int CFG_Compiler::current_method_field_index(Fa_StringRef const& name) const
{
    if (m_current == nullptr || !m_current->is_class_method)
        return -1;

    for (u32 i = 0, n = m_current->class_field_names.size(); i < n; i += 1) {
        if (m_current->class_field_names[i] == name)
            return static_cast<int>(i);
    }

    return -1;
}

Fa_ErrorOr<bool> CFG_Compiler::compile_cfg_body(Fa_CFG* cfg, bool is_top_level_script)
{
    if (cfg == nullptr)
        return report_error(CompilerError::MISSING_CFG, { });

    // reverse-postorder layout — identical algorithm to compile_if_cfg's,
    // generalized from succ_true/succ_false/succ_jump to the real
    // get_succs()[0]/[1] (BRANCH: [0]=true,[1]=false; JUMP: [0]=target)
    Fa_Array<bool> visited(cfg->blocks.size(), false);
    Fa_Array<Fa_BasicBlock*> post, layout;

    struct Frame {
        Fa_BasicBlock* b;
        int next;
    };

    Fa_Array<Frame> stack;
    stack.push({ cfg->entry, 0 });
    visited[cfg->entry->id] = true;

    while (!stack.empty()) {
        Frame& top = stack[stack.size() - 1];
        Fa_Array<Fa_BasicBlock*> const& s = top.b->succs;
        Fa_Array<Fa_BasicBlock*> order;
        if (top.b->terminator == TerminatorTag::BRANCH) {
            order.push(s[1]);
            order.push(s[0]);
        } else if (top.b->terminator == TerminatorTag::JUMP) {
            order.push(s[0]);
        }
        if (top.next < (int)order.size()) {
            Fa_BasicBlock* nxt = order[top.next++];
            if (nxt && !visited[nxt->id]) {
                visited[nxt->id] = true;
                stack.push({ nxt, 0 });
            }
        } else {
            post.push(top.b);
            stack.erase(stack.size() - 1);
        }
    }

    for (size_t i = 0; i < post.size(); ++i)
        layout.push(post[post.size() - 1 - i]);

    Fa_Array<u32> block_start(cfg->blocks.size(), 0);
    Fa_Array<int> false_idx(cfg->blocks.size(), -1), false_tgt(cfg->blocks.size(), -1);
    Fa_Array<int> jump_idx(cfg->blocks.size(), -1), jump_tgt(cfg->blocks.size(), -1);

    for (size_t i = 0; i < layout.size(); ++i) {
        Fa_BasicBlock* b = layout[i];
        m_current->is_dead = false; // CFG reachability already guarantees this block matters
        while (m_current->scope_depth < b->scope_depth)
            begin_scope();
        while (m_current->scope_depth > b->scope_depth)
            end_scope_no_reclaim({ 1, 1, 0 });
        block_start[b->id] = current_offset();

        for (AST::Fa_Stmt* stmt : b->stmts)
            // FUNC/CLASS_DEF inside a statement list dispatch into their
            // own pre-lowered Fa_CFG (Phase 4), NOT the old compile_stmt
            // path — everything else still goes through compile_stmt
            // exactly as today.
            COMPILE_STMT_DISCARD(stmt);

        Fa_BasicBlock* next = (i + 1 < layout.size()) ? layout[i + 1] : nullptr;
        Fa_SourceLocation loc = b->stmts.empty() ? Fa_SourceLocation { 1, 1, 0 } : b->stmts.back()->get_location();

        switch (b->terminator) {
        case TerminatorTag::BRANCH: {
            u8 cond;
            CFG_Fa_ExprResult r;
            COMPILE_EXPR_IMPL(b->cond, &r);
            ANY_REG(r, loc, &cond);
            u32 jf = emit_jump(Fa_OpCode::JUMP_IF_FALSE, cond, loc);
            false_idx[b->id] = (int)jf;
            false_tgt[b->id] = (int)b->succs[1]->id;
            if (b->succs[0] != next) {
                u32 j = emit_jump(Fa_OpCode::JUMP, 0, loc);
                jump_idx[b->id] = (int)j;
                jump_tgt[b->id] = (int)b->succs[0]->id;
            }
            break;
        }
        case TerminatorTag::JUMP:
            if (b->succs[0] != next) {
                u32 j = emit_jump(Fa_OpCode::JUMP, 0, loc);
                jump_idx[b->id] = (int)j;
                jump_tgt[b->id] = (int)b->succs[0]->id;
            }
            break;
        case TerminatorTag::RETURN:
            break; // compile_stmt already emitted RETURN/RETURN_NIL
        case TerminatorTag::NORETURN:
            if (is_top_level_script && !b->stmts.empty() && is_terminal_top_level_call(b->stmts.back())) {
                auto const* expr_stmt = AS_CONST_EXPR_STMT(b->stmts.back());
                Fa_SourceLocation loc = expr_stmt->get_location();
                CFG_RegMark mark(m_current);
                auto expr_result = compile_expr_impl(expr_stmt->get_expr());
                if (expr_result.has_error())
                    break;
                auto src = any_reg(expr_result.value(), loc);
                if (src.has_error())
                    break;
                emit(Fa_make_ABC(Fa_OpCode::RETURN, src.value(), 1, 0), loc);
                break;
            } else {
                emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);
            }
            m_current->is_dead = true;
            break;
        case TerminatorTag::NONE:
            assert(false && "unterminated block reached codegen");
        }
    }

    return true;
}

} // namespace fairuz::runtime
