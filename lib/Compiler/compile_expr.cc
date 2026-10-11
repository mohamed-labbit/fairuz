//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Compiles Fairuz Expression AST nodes into bytecode.
//===----------------------------------------------------------------------===//

#include "Compiler/compiler.hpp"
#include "Compiler/opcode.hpp"
#include "Interpreter/object.hpp"
#include "Macros.hpp"
#include "Parser/ast.hpp"
#include "Support/array.hpp"
#include "Support/error.hpp"
#include "Support/string.hpp"
#include "compile_macros.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

namespace fairuz::runtime {

Compiler::compile_expr_result Compiler::compile_expr_impl(AST::ExprPtr e)
{
    if (AST::is_unary(e))
        return compile_unary_impl(AST::as_unary(e));

    if (AST::is_binary(e))
        return compile_binary_impl(AST::as_binary(e));

    switch (e->get_kind()) {
    case AST::ExprKind::INT_LITERAL: return compile_literal_int_impl(AST::as_literal_int(e));
    case AST::ExprKind::FLOAT_LITERAL: return compile_literal_float_impl(AST::as_literal_float(e));
    case AST::ExprKind::BOOL_LITERAL: return compile_literal_bool_impl(AST::as_literal_bool(e));
    case AST::ExprKind::STRING_LITERAL: return compile_literal_string_impl(AST::as_literal_string(e));
    case AST::ExprKind::NIL: return compile_nil_impl(AST::as_nil(e));
    case AST::ExprKind::IDENTIFIER: return compile_identifier_impl(AST::as_identifier(e));
    case AST::ExprKind::ASSIGNMENT: return compile_assign_impl(AST::as_assignment_expr(e));
    case AST::ExprKind::CALL: return compile_call_impl(AST::as_call(e), nullptr, false);
    case AST::ExprKind::LIST: return compile_list_impl(AST::as_list(e));
    case AST::ExprKind::DICT: return compile_dict_impl(AST::as_dict(e));
    case AST::ExprKind::INDEX_READ: return compile_index_impl(as_index(e));
    case AST::ExprKind::GET: return compile_get_impl(as_get(e));
    default:
        return report_error(ErrorCode::INVALID_EXPRESSION_NODE, e->get_location());
    }

    return ExprResult::knil();
}

Compiler::compile_expr_result Compiler::compile_literal_int_impl(AST::IntLiteralExpr* e)
{
    if (!e->large_literal.empty()) {
        auto data = integer::parse(e->large_literal, e->literal_base);
        u32 pc = emit(make_ABx(OpCode::LOAD_BIG_INT, 0, current_chunk()->add_big_int(data)), e->get_location());
        return ExprResult::reloc(pc);
    }
    return ExprResult::kint(e->value);
}

Compiler::compile_expr_result Compiler::compile_literal_float_impl(AST::FloatLiteralExpr* e)
{
    return ExprResult::kfloat(e->value);
}

Compiler::compile_expr_result Compiler::compile_literal_bool_impl(AST::BoolLiteralExpr* e)
{
    return ExprResult::kbool(e->value);
}

Compiler::compile_expr_result Compiler::compile_literal_string_impl(AST::StringLiteralExpr* e)
{
    u16 kidx = intern_string(e->str);
    u32 pc = emit(make_ABx(OpCode::LOAD_CONST, 0, kidx), e->get_location());
    return ExprResult::reloc(pc);
}

Compiler::compile_expr_result Compiler::compile_nil_impl(AST::NilExpr* e)
{
    (void)e;
    return ExprResult::knil();
}

Compiler::compile_expr_result Compiler::compile_identifier_impl(AST::IdentifierExpr* e)
{
    SourceLocation loc = e->get_location();
    VarInfo vi = resolve_name(e->spelling);

    if (vi.kind == VarInfo::Kind::LOCAL)
        return ExprResult::reg(vi.index);

    if (int field_idx = current_method_field_index(e->spelling); field_idx >= 0) {
        LocalVar const* self = lookup_local(kClassInstanceName);
        if (self == nullptr)
            return report_error(ErrorCode::INVALID_EXPRESSION_NODE, e->get_location());

        u32 pc = emit(make_ABC(OpCode::GET_FIELD, 0, self->reg, static_cast<reg_type>(field_idx)), loc);
        return ExprResult::reloc(pc);
    }

    u16 kidx = intern_string(e->spelling);
    u32 pc = emit(make_ABx(OpCode::LOAD_GLOBAL, 0, kidx), loc);
    return ExprResult::reloc(pc);
}

Compiler::compile_expr_result Compiler::compile_unary_impl(AST::UnaryExpr* e)
{
    SourceLocation loc = e->get_location();

    OpCode op = OpCode::NOP;
    switch (e->get_kind()) {
    case AST::ExprKind::OP_NEG: op = OpCode::OP_NEG; break;
    case AST::ExprKind::OP_BITNOT: op = OpCode::OP_BITNOT; break;
    case AST::ExprKind::OP_NOT: op = OpCode::OP_NOT; break;
    default:
        return report_error(ErrorCode::UNKNOWN_UNARY_OPERATOR, e->get_location());
    }

    RegMark mark(m_current);
    ExprResult expr_result;
    COMPILE_EXPR_IMPL(e->operand, &expr_result);
    reg_type src;
    ANY_REG(expr_result, loc, &src);
    u32 pc = emit(make_ABC(op, 0, src, 0), loc);
    return ExprResult::reloc(pc);
}

Compiler::compile_expr_result Compiler::compile_binary_impl(AST::BinaryExpr* e)
{
    SourceLocation loc = e->get_location();

    AST::ExprKind op = e->get_kind();
    if (op == AST::ExprKind::OP_AND) {
        reg_type dst;
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

    if (op == AST::ExprKind::OP_OR) {
        reg_type dst;
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
    case AST::ExprKind::OP_ADD: bc_op = OpCode::OP_ADD; break;
    case AST::ExprKind::OP_SUB: bc_op = OpCode::OP_SUB; break;
    case AST::ExprKind::OP_MUL: bc_op = OpCode::OP_MUL; break;
    case AST::ExprKind::OP_DIV: bc_op = OpCode::OP_DIV; break;
    case AST::ExprKind::OP_MOD: bc_op = OpCode::OP_MOD; break;
    case AST::ExprKind::OP_POW: bc_op = OpCode::OP_POW; break;
    case AST::ExprKind::OP_EQ: bc_op = OpCode::OP_EQ; break;
    case AST::ExprKind::OP_NEQ: bc_op = OpCode::OP_NEQ; break;
    case AST::ExprKind::OP_LT: bc_op = OpCode::OP_LT; break;
    case AST::ExprKind::OP_LTE: bc_op = OpCode::OP_LTE; break;
    case AST::ExprKind::OP_GT: bc_op = OpCode::OP_GT; break;
    case AST::ExprKind::OP_GTE: bc_op = OpCode::OP_GTE; break;
    case AST::ExprKind::OP_BITAND: bc_op = OpCode::OP_BITAND; break;
    case AST::ExprKind::OP_BITOR: bc_op = OpCode::OP_BITOR; break;
    case AST::ExprKind::OP_BITXOR: bc_op = OpCode::OP_BITXOR; break;
    case AST::ExprKind::OP_LSHIFT: bc_op = OpCode::OP_LSHIFT; break;
    case AST::ExprKind::OP_RSHIFT: bc_op = OpCode::OP_RSHIFT; break;
    default:
        return report_error(ErrorCode::UNKNOWN_BINARY_OPERATOR, e->get_location());
    }

    if ((bc_op == OpCode::OP_LSHIFT || bc_op == OpCode::OP_RSHIFT) && AST::is_literal_int(e->rhs)
        && AST::as_literal_int(e->rhs)->large_literal.empty()
        && AST::as_literal_int(e->rhs)->value >= 0 && AST::as_literal_int(e->rhs)->value <= 255) {
        auto* amount_expr = AST::as_literal_int(e->rhs);
        i64 amount = amount_expr->value;
        if (amount < 0 || amount > 255)
            return report_error(ErrorCode::SHIFT_AMOUNT_OUT_OF_RANGE, amount_expr->get_location());

        RegMark mark(m_current);
        ExprResult expr_result;
        reg_type lhs;

        COMPILE_EXPR_IMPL(e->lhs, &expr_result);
        ANY_REG(expr_result, loc, &lhs);
        u32 pc = emit(make_ABC(bc_op, 0, lhs, static_cast<reg_type>(amount)), loc);
        reg_type ic = current_chunk()->alloc_ic_slot();
        emit(make_ABC(OpCode::NOP, ic, 0, 0), loc);
        return ExprResult::reloc(pc);
    }

    // Nonliteral shift counts live in registers, unlike the immediate forms.
    if (bc_op == OpCode::OP_LSHIFT)
        bc_op = OpCode::OP_LSHIFT_REG;
    else if (bc_op == OpCode::OP_RSHIFT)
        bc_op = OpCode::OP_RSHIFT_REG;

    RegMark mark(m_current);
    ExprResult lhs_ret, rhs_ret;
    reg_type lhs, rhs;

    COMPILE_EXPR_IMPL(e->lhs, &lhs_ret);
    ANY_REG(lhs_ret, loc, &lhs);
    COMPILE_EXPR_IMPL(e->rhs, &rhs_ret);
    ANY_REG(rhs_ret, loc, &rhs);

    if (swapped)
        std::swap(lhs, rhs);

    u32 pc = emit(make_ABC(bc_op, 0, lhs, rhs), loc);
    reg_type ic = current_chunk()->alloc_ic_slot();
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

Compiler::compile_expr_result Compiler::compile_assign_impl(AST::AssignExpr* e)
{
    SourceLocation loc = e->get_location();

    if (e->augmented && (AST::is_index(e->target) || AST::is_get(e->target))) {
        // Capture the target once, read its old value before the RHS, and
        // write the result back through the same object and index/field.
        reg_type result;
        ALLOC_REG(&result);
        RegMark mark(m_current);
        auto capture = [&](AST::ExprPtr expression) -> ErrorOr<reg_type> {
            reg_type reg;
            ALLOC_REG(&reg);
            ExprResult value;
            COMPILE_EXPR_IMPL(expression, &value);
            discharge(value, reg, expression->get_location());
            m_current->free_regs_to(reg + 1);
            return reg;
        };
        auto* operation = AST::as_binary(e->value);
        bool indexed = AST::is_index(e->target);
        reg_type object, index = 0, previous, rhs;
        u16 member = 0;
        if (indexed) {
            auto* target = AST::as_index(e->target);
            TRY_ASSIGN(&object, capture(target->object));
            TRY_ASSIGN(&index, capture(target->index));
        } else {
            auto* target = AST::as_get(e->target);
            TRY_ASSIGN(&object, capture(target->object));
            member = static_cast<u16>(intern_string(target->member->spelling));
        }
        ALLOC_REG(&previous);
        if (indexed)
            emit(make_ABC(OpCode::INDEX_READ, previous, object, index), loc);
        else {
            emit(make_ABC(OpCode::GET_FIELD, previous, object, 0xFF), loc);
            emit(make_ABx(OpCode::NOP, 0, member), loc);
        }
        OpCode opcode;
        switch (operation->get_kind()) {
        case AST::ExprKind::OP_ADD: opcode = OpCode::OP_ADD; break;
        case AST::ExprKind::OP_SUB: opcode = OpCode::OP_SUB; break;
        case AST::ExprKind::OP_MUL: opcode = OpCode::OP_MUL; break;
        case AST::ExprKind::OP_DIV: opcode = OpCode::OP_DIV; break;
        case AST::ExprKind::OP_MOD: opcode = OpCode::OP_MOD; break;
        case AST::ExprKind::OP_BITAND: opcode = OpCode::OP_BITAND; break;
        case AST::ExprKind::OP_BITOR: opcode = OpCode::OP_BITOR; break;
        case AST::ExprKind::OP_BITXOR: opcode = OpCode::OP_BITXOR; break;
        case AST::ExprKind::OP_LSHIFT: opcode = OpCode::OP_LSHIFT_REG; break;
        case AST::ExprKind::OP_RSHIFT: opcode = OpCode::OP_RSHIFT_REG; break;
        default: return report_error(ErrorCode::UNKNOWN_BINARY_OPERATOR, loc);
        }
        TRY_ASSIGN(&rhs, capture(operation->rhs));
        emit(make_ABC(opcode, result, previous, rhs), loc);
        emit(make_ABC(OpCode::NOP, current_chunk()->alloc_ic_slot(), 0, 0), loc);
        if (indexed)
            emit(make_ABC(OpCode::INDEX_WRITE, object, index, result), loc);
        else {
            emit(make_ABC(OpCode::SET_FIELD, object, 0xFF, result), loc);
            emit(make_ABx(OpCode::NOP, 0, member), loc);
        }
        return ExprResult::reg(result);
    }

    // Indexed assignment: evaluate the object and index before the value,
    // then write the resulting value into the computed location.
    if (AST::is_index(e->target)) {
        auto index_expr = as_index(e->target);
        RegMark mark(m_current);
        ExprResult object_expr_result, index_expr_result, value_expr_result;
        reg_type target_object_reg, index_reg, value_reg;

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
        int field_idx = -1;
        if (ClassDesc const* desc = resolve_receiver_class(get_expr->object))
            field_idx = desc->field_index(member_name->spelling);
        if (field_idx < 0 && is_this_reference(get_expr->object))
            field_idx = current_method_field_index(member_name->spelling);

        RegMark mark(m_current);
        ExprResult object_expr_result, value_expr_result;
        reg_type object_reg, value_reg;

        COMPILE_EXPR_IMPL(get_expr->object, &object_expr_result);
        ANY_REG(object_expr_result, loc, &object_reg);
        COMPILE_EXPR_IMPL(e->value, &value_expr_result);
        ANY_REG(value_expr_result, loc, &value_reg);

        if (field_idx >= 0) {
            emit(make_ABC(OpCode::SET_FIELD, object_reg,
                     static_cast<reg_type>(field_idx), value_reg),
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
            reg_type src;

            COMPILE_EXPR_IMPL(e->value, &value_result);
            ANY_REG(value_result, loc, &src);
            emit(make_ABx(OpCode::STORE_GLOBAL, src,
                     static_cast<u16>(intern_string(name_value))),
                loc);
            m_globals[name_value] = true;
            return ExprResult::reg(src);
        }

        reg_type dst;
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
        reg_type value_reg;
        COMPILE_EXPR_IMPL(e->value, &value_result);
        ANY_REG(value_result, loc, &value_reg);
        emit(make_ABC(OpCode::SET_FIELD, self->reg,
                 static_cast<reg_type>(field_idx), value_reg),
            loc);
        return ExprResult::reg(value_reg);
    }

    if (LocalVar const* local = lookup_local(name_value)) {
        reg_type dst = local->reg;
        for (auto& variable : m_current->locals) {
            if (variable.reg == dst)
                variable.known_class = { };
        }
        auto result = compile_expr(e->value, &dst);
        VERIFY_RESULT(result);
        return ExprResult::reg(dst);
    }

    // Assignment inside a function introduces a local even when a global of
    // the same name already exists. Globals are only written at top level.
    if (!m_current->is_top_level) {
        reg_type dst;
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
    reg_type src;
    ANY_REG(value_result, loc, &src);

    emit(make_ABx(OpCode::STORE_GLOBAL, src,
             static_cast<u16>(intern_string(name_value))),
        loc);
    m_globals[name_value] = true;
    return ExprResult::reg(src);
}

Compiler::compile_expr_result Compiler::compile_call_impl(AST::CallExpr* e, reg_type* dst, bool tail)
{
    SourceLocation loc = e->get_location();
    auto fn_reg_ret = dst == nullptr ? alloc_register() : *dst;
    VERIFY_RESULT(fn_reg_ret);
    reg_type fn_reg = fn_reg_ret.value();

    auto compile_args = [&]() -> Compiler::compile_stmt_result {
        for (AST::ExprPtr arg : e->args) {
            reg_type arg_reg;
            ExprResult arg_cmp_ret;
            ALLOC_REG(&arg_reg);
            COMPILE_EXPR_IMPL(arg, &arg_cmp_ret);
            discharge(arg_cmp_ret, arg_reg, arg->get_location());
            m_current->free_regs_to(arg_reg + 1);
        }
        return true;
    };

    // Resolve external members from the actual receiver at runtime, including
    // modules carried through aliases and locals shadowing an import name.
    if (AST::is_get(e->callee)) {
        AST::GetExpr const* get_expr = AST::as_get(e->callee);
        /// NOTE: we don't have to verify if this is in fact a method call
        /// and not a semantic error of calling a non-callable plain field
        /// because the parser already will enforce this for us
        AST::ExprPtr object = get_expr->object;
        AST::ExprPtr member = get_expr->member;

        if (AST::is_identifier(object) && AST::as_identifier(object)->spelling == kClassInstanceName) {
            /// internal method call 'this.method(implicit this, ...)'
            auto method_name = AST::as_identifier(member)->spelling;
            int slot = current_method_slot(method_name);
            if (slot >= 0) {
                reg_type object_reg;
                reg_type reserved_reg;
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

                auto args_cmp_ret = compile_args();
                VERIFY_RESULT(args_cmp_ret);

                u8 argc = static_cast<u8>(e->args.size() + 1);
                emit(make_ABC(OpCode::INVOKE, object_reg, static_cast<reg_type>(slot), argc), loc);
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
            reg_type object_reg;
            reg_type member_reg;

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

            u8 argc = static_cast<u8>(e->args.size() + 1); // +1 for implicit 'this'
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

    u8 argc = static_cast<u8>(e->args.size());
    if (tail && !m_current->is_top_level) {
        emit(make_ABC(OpCode::CALL_TAIL, fn_reg, argc, 0), loc);
        m_current->free_regs_to(fn_reg);
        return ExprResult::reg(fn_reg);
    }

    reg_type ic = current_chunk()->alloc_ic_slot();
    emit(make_ABC(OpCode::IC_CALL, fn_reg, argc, ic), loc);
    m_current->free_regs_to(fn_reg + 1);
    return ExprResult::reg(fn_reg);
}

Compiler::compile_expr_result Compiler::compile_list_impl(AST::ListExpr* e)
{
    SourceLocation loc = e->get_location();

    if (e->size() > 0xFF)
        return report_error(ErrorCode::TOO_MANY_LIST_ELEMENTS, loc);

    reg_type dst;
    ALLOC_REG(&dst);

    reg_type list_reg;
    ALLOC_REG(&list_reg);
    auto cap = static_cast<reg_type>(e->size());
    emit(make_ABC(OpCode::LIST_NEW, list_reg, cap, 0), loc);

    for (AST::ExprPtr elem : e->elements) {
        ExprResult expr_result;
        reg_type reg;
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

Compiler::compile_expr_result Compiler::compile_index_impl(AST::IndexExpr* e)
{
    SourceLocation loc = e->get_location();
    RegMark mark(m_current);
    ExprResult object_expr_result, index_expr_result;
    reg_type object_reg, index_reg;
    COMPILE_EXPR_IMPL(e->object, &object_expr_result);
    ANY_REG(object_expr_result, loc, &object_reg);
    COMPILE_EXPR_IMPL(e->index, &index_expr_result);
    ANY_REG(index_expr_result, loc, &index_reg);
    u32 pc = emit(make_ABC(OpCode::INDEX_READ, 0, object_reg, index_reg), loc);
    return ExprResult::reloc(pc);
}

Compiler::compile_expr_result Compiler::compile_dict_impl(AST::DictExpr* e)
{
    SourceLocation loc = e->get_location();

    reg_type dst;
    ALLOC_REG(&dst); // reserve the expression's result register FIRST

    reg_type fn_reg;
    ALLOC_REG(&fn_reg);
    u16 kidx = intern_string("قاموس");
    emit(make_ABx(OpCode::LOAD_GLOBAL, fn_reg, kidx), loc);

    for (auto const& [key, value] : e->content) {
        reg_type key_reg;
        ExprResult expr_result;
        ALLOC_REG(&key_reg);
        COMPILE_EXPR_IMPL(key, &expr_result);
        discharge(expr_result, key_reg, key->get_location());
        m_current->free_regs_to(key_reg + 1);

        reg_type value_reg;
        ExprResult val_expr_result;
        ALLOC_REG(&value_reg);
        COMPILE_EXPR_IMPL(value, &val_expr_result);
        discharge(val_expr_result, value_reg, value->get_location());
        m_current->free_regs_to(value_reg + 1);
    }

    reg_type argc = static_cast<reg_type>(e->content.size() * 2);
    reg_type ic = current_chunk()->alloc_ic_slot();
    emit(make_ABC(OpCode::IC_CALL, fn_reg, argc, ic), loc);

    if (fn_reg != dst)
        emit(make_ABC(OpCode::MOVE, dst, fn_reg, 0), loc);

    m_current->free_regs_to(dst + 1);
    return ExprResult::reg(dst);
}

Compiler::compile_expr_result Compiler::compile_get_impl(AST::GetExpr* e)
{
    SourceLocation loc = e->get_location();
    u32 name_idx = intern_string(e->member->spelling);
    reg_type object_reg;
    ExprResult object_cmp_ret;
    ALLOC_REG(&object_reg);
    COMPILE_EXPR_IMPL(e->object, &object_cmp_ret);
    discharge(object_cmp_ret, object_reg, e->object->get_location());
    u32 pc = emit(make_ABC(OpCode::GET_FIELD, 0, object_reg, 0xFF), loc);
    emit(make_ABx(OpCode::NOP, 0, static_cast<u16>(name_idx)), loc);
    return ExprResult::reloc(pc);
}

ErrorOr<Compiler::reg_type> Compiler::compile_expr(AST::ExprPtr e, reg_type* dst)
{
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

} // namespace fairuz::runtime
