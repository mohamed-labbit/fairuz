//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Implements compiler and portability macros.
//===----------------------------------------------------------------------===//

#include "Compiler/bytecode_emitter.hpp"
#include "Compiler/chunk.hpp"
#include "Compiler/compiler.hpp"
#include "Compiler/opcode.hpp"
#include "Compiler/register_allocator.hpp"
#include "Interpreter/value.hpp"
#include "Macros.hpp"

namespace fairuz::runtime {

PCType ByteCodeEmitter::binary(OpCode op, RegType dst, RegType lhs, RegType rhs, SourceLocation loc)
{
    PCType pc = emit_ABC(op, dst, lhs, rhs, loc);
    u16 ic = m_compiler_ptr->current_chunk()->alloc_ic_slot();
    emit_cache_payload(ic, loc);
    return pc;
}

PCType ByteCodeEmitter::get_named_field(RegType dst, RegType obj, IndexType name_idx, SourceLocation loc)
{
    PCType pc = emit_ABC(OpCode::GET_FIELD, dst, obj, 0xFF, loc);
    emit_name_payload(name_idx, loc);
    return pc;
}

PCType ByteCodeEmitter::set_named_field(RegType obj, RegType src, IndexType name_idx, SourceLocation loc)
{
    PCType pc = emit_ABC(OpCode::SET_FIELD, obj, 0xFF, src, loc);
    emit_name_payload(name_idx, loc);
    return pc;
}

PCType ByteCodeEmitter::invoke(RegType obj, RegType slot, RegType argc, SourceLocation loc)
{
    PCType pc = emit_ABC(OpCode::INVOKE, obj, slot, argc, loc);
    emit_cache_payload(m_compiler_ptr->current_chunk()->alloc_ic_slot(), loc);
    return pc;
}

PCType ByteCodeEmitter::invoke_named(RegType obj, RegType slot, RegType argc, IndexType idx, SourceLocation loc)
{
    PCType pc = emit_ABC(OpCode::INVOKE_NAMED, obj, slot, argc, loc);
    emit_name_payload(idx, loc);
    return pc;
}

PCType ByteCodeEmitter::ic_call(RegType obj, RegType argc, SourceLocation loc)
{
    RegType slot = m_compiler_ptr->current_chunk()->alloc_ic_slot();
    return emit_ABC(OpCode::IC_CALL, obj, argc, slot, loc);
}

PCType ByteCodeEmitter::emit_cache_payload(IndexType idx, SourceLocation loc)
{
    return emit_ABC(OpCode::NOP, static_cast<RegType>(idx), 0, 0, loc);
}

PCType ByteCodeEmitter::emit_name_payload(IndexType idx, SourceLocation loc)
{
    return emit_ABx(OpCode::NOP, 0, idx, loc);
}

PCType ByteCodeEmitter::jump_if_false(RegType cond_reg, SourceLocation loc)
{
    return emit_AsBx(OpCode::JUMP_IF_FALSE, cond_reg, 0, loc);
}

PCType ByteCodeEmitter::jump(OpCode op, RegType cond, SourceLocation loc)
{
    return emit_AsBx(op, cond, 0, loc);
}

PCType ByteCodeEmitter::return_nil(SourceLocation loc)
{
    return emit_ABC(OpCode::RETURN_NIL, 0, 0, 0, loc);
}

void ByteCodeEmitter::patch_jump(PCType at)
{
    if (!m_compiler_ptr->current_chunk()->patch_jump(at))
        diagnostic::panic(ErrorCode::JUMP_OFFSET_OVERFLOW);
}

void ByteCodeEmitter::patch_jump_to(u32 instr_idx, u32 target)
{
    int offset = static_cast<int>(target) - static_cast<int>(instr_idx) - 1;
    if (offset > JUMP_OFFSET || offset < -JUMP_OFFSET)
        diagnostic::panic(ErrorCode::LOOP_JUMP_OFFSET_OVERFLOW);
    u32 word = m_compiler_ptr->current_chunk()->code[instr_idx];
    m_compiler_ptr->current_chunk()->code[instr_idx] = make_AsBx(instr_op(word), instr_A(word), offset);
}

PCType ByteCodeEmitter::emit_load_value(RegType dst, Value v, SourceLocation loc)
{
    if (v.is_nil())
        return emit_ABC(OpCode::LOAD_NIL, dst, dst, 1, loc);

    if (v.is_bool())
        return emit_ABC(v.as_bool() ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE, dst, 0, 0, loc);

    if (v.is_int()) {
        i64 iv = v.as_int();
        if (iv >= -JUMP_OFFSET && iv <= JUMP_OFFSET)
            return emit_ABx(OpCode::LOAD_INT, dst, iv + JUMP_OFFSET, loc);
    }

    return emit_ABx(OpCode::LOAD_CONST, dst, m_compiler_ptr->current_chunk()->add_constant(v), loc);
}

void ByteCodeEmitter::discharge(ExprResult const& r, RegType dst, SourceLocation loc)
{
    switch (r.kind) {
    case ExprResult::Kind::REG:
        if (r.reg_ != dst)
            emit_ABC(OpCode::MOVE, dst, r.reg_, 0, loc);
        return;
    case ExprResult::Kind::RELOC: patch_a(m_compiler_ptr->current_chunk(), r.reloc_pc, dst); return;
    case ExprResult::Kind::KINT: {
#if FA_USE_NANBOX
        /// r.ival only holds 64 bit int, so this is only applicable on NAN-BOXed Value
        if (r.ival > Value::int_max() || r.ival < Value::int_min()) {
            emit_ABx(OpCode::LOAD_BIG_INT, dst, m_compiler_ptr->current_chunk()->add_big_int(r.ival), loc);
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

PCType ByteCodeEmitter::emit_ABC(OpCode op, RegType a, RegType b, RegType c, SourceLocation loc)
{
    return emit(make_ABC(op, a, b, c), loc);
}

PCType ByteCodeEmitter::emit_ABx(OpCode op, RegType a, IndexType idx, SourceLocation loc)
{
    return emit(make_ABx(op, a, idx), loc);
}

PCType ByteCodeEmitter::emit_AsBx(OpCode op, RegType a, i64 s_bx, SourceLocation loc)
{
    return emit(make_AsBx(op, a, s_bx), loc);
}

PCType ByteCodeEmitter::emit(InstrType instr, SourceLocation loc)
{
    return m_compiler_ptr->current_chunk()->emit(instr, loc);
}

void ByteCodeEmitter::patch_a(Chunk* ch, PCType pc, RegType a)
{
    InstrType instr = ch->code[pc];
    ch->code[pc] = (instr & 0xFF00FFFFu) | (static_cast<u32>(a) << 16);
}

} // namespace fairuz::runtime
