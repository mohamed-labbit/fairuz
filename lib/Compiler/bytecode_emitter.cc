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
#include "Interpreter/value.hpp"
#include "Macros.hpp"

namespace fairuz::runtime {

PCType ByteCodeEmitter::binary(OpCode op, RegType dst, RegType lhs, RegType rhs, SourceLocation loc)
{
    PCType pc = emit_ABC(op, dst.value, lhs.value, rhs.value, loc);
    CacheSlot ic = m_compiler_ptr->current_chunk()->alloc_ic_slot();
    emit_cache_payload(ic, loc);
    return pc;
}

PCType ByteCodeEmitter::binary_immediate(OpCode op, RegType dst, RegType lhs, u8 rhs, SourceLocation loc)
{
    assert(op == OpCode::OP_LSHIFT || op == OpCode::OP_RSHIFT);
    PCType pc = emit_ABC(op, dst.value, lhs.value, rhs, loc);
    emit_cache_payload(m_compiler_ptr->current_chunk()->alloc_ic_slot(), loc);
    return pc;
}

PCType ByteCodeEmitter::get_named_field(RegType dst, RegType obj, ConstantIndex name_idx, SourceLocation loc)
{
    PCType pc = emit_ABC(OpCode::GET_FIELD, dst.value, obj.value, 0xFF, loc);
    emit_name_payload(name_idx, loc);
    return pc;
}

PCType ByteCodeEmitter::set_named_field(RegType obj, RegType src, ConstantIndex name_idx, SourceLocation loc)
{
    PCType pc = emit_ABC(OpCode::SET_FIELD, obj.value, 0xFF, src.value, loc);
    emit_name_payload(name_idx, loc);
    return pc;
}

PCType ByteCodeEmitter::invoke(RegType obj, MethodSlot slot, u32 argc, SourceLocation loc)
{
    PCType pc = emit_ABC(OpCode::INVOKE, obj.value, slot.value, argc, loc);
    emit_cache_payload(m_compiler_ptr->current_chunk()->alloc_ic_slot(), loc);
    return pc;
}

PCType ByteCodeEmitter::invoke_named(RegType obj, u32 argc, ConstantIndex idx, SourceLocation loc)
{
    // B is reserved in the named form; it is not a method/cache-slot index.
    PCType pc = emit_ABC(OpCode::INVOKE_NAMED, obj.value, 0, argc, loc);
    emit_name_payload(idx, loc);
    return pc;
}

PCType ByteCodeEmitter::ic_call(RegType obj, u32 argc, SourceLocation loc)
{
    // Reject an unencodable count before allocating an inline-cache slot.
    if (argc > 0xFF) diagnostic::panic(ErrorCode::TOO_MANY_REGISTERS);
    CacheSlot slot = m_compiler_ptr->current_chunk()->alloc_ic_slot();
    return emit_ABC(OpCode::IC_CALL, obj.value, argc, slot.value, loc);
}

PCType ByteCodeEmitter::emit_cache_payload(CacheSlot idx, SourceLocation loc)
{
    return emit_ABC(OpCode::NOP, idx.value, 0, 0, loc);
}

PCType ByteCodeEmitter::emit_name_payload(ConstantIndex idx, SourceLocation loc)
{
    return emit_ABx(OpCode::NOP, 0, idx.value, loc);
}

PCType ByteCodeEmitter::load_constant(RegType dst, ConstantIndex idx, SourceLocation loc)
{
    return emit_ABx(OpCode::LOAD_CONST, dst.value, idx.value, loc);
}

PCType ByteCodeEmitter::load_big_int(RegType dst, BigIntIndex idx, SourceLocation loc)
{
    return emit_ABx(OpCode::LOAD_BIG_INT, dst.value, idx.value, loc);
}

PCType ByteCodeEmitter::load_global(RegType dst, ConstantIndex name, SourceLocation loc)
{
    return emit_ABx(OpCode::LOAD_GLOBAL, dst.value, name.value, loc);
}

PCType ByteCodeEmitter::store_global(RegType src, ConstantIndex name, SourceLocation loc)
{
    return emit_ABx(OpCode::STORE_GLOBAL, src.value, name.value, loc);
}

PCType ByteCodeEmitter::import_module(RegType dst, ConstantIndex name, SourceLocation loc)
{
    return emit_ABx(OpCode::IMPORT_MODULE, dst.value, name.value, loc);
}

PCType ByteCodeEmitter::closure(RegType dst, FunctionIndex idx, SourceLocation loc)
{
    return emit_ABx(OpCode::CLOSURE, dst.value, idx.value, loc);
}

PCType ByteCodeEmitter::new_class(RegType dst, ClassIndex idx, SourceLocation loc)
{
    return emit_ABx(OpCode::NEW_CLASS, dst.value, idx.value, loc);
}

PCType ByteCodeEmitter::jump_if_false(RegType cond_reg, SourceLocation loc)
{
    return emit_AsBx(OpCode::JUMP_IF_FALSE, cond_reg.value, 0, loc);
}

PCType ByteCodeEmitter::jump(OpCode op, RegType cond, SourceLocation loc)
{
    assert(op == OpCode::JUMP_IF_FALSE || op == OpCode::JUMP_IF_TRUE);
    return emit_AsBx(op, cond.value, 0, loc);
}

PCType ByteCodeEmitter::jump(SourceLocation loc) { return emit_AsBx(OpCode::JUMP, 0, 0, loc); }

PCType ByteCodeEmitter::return_nil(SourceLocation loc) { return emit_ABC(OpCode::RETURN_NIL, 0, 0, 0, loc); }

void ByteCodeEmitter::patch_jump(PCType at)
{
    if (!m_compiler_ptr->current_chunk()->patch_jump(at)) diagnostic::panic(ErrorCode::JUMP_OFFSET_OVERFLOW);
}

void ByteCodeEmitter::patch_jump_to(PCType instr_idx, PCType target)
{
    i64 offset = static_cast<i64>(target.value) - instr_idx.value - 1;
    if (offset > JUMP_OFFSET || offset < -JUMP_OFFSET) diagnostic::panic(ErrorCode::LOOP_JUMP_OFFSET_OVERFLOW);
    InstrType word = m_compiler_ptr->current_chunk()->code[instr_idx.value];
    m_compiler_ptr->current_chunk()->code[instr_idx.value] = make_AsBx(instr_op(word), instr_A(word), offset);
}

PCType ByteCodeEmitter::emit_load_value(RegType dst, Value v, SourceLocation loc)
{
    if (v.is_nil()) return emit_ABC(OpCode::LOAD_NIL, dst.value, dst.value, 1, loc);
    if (v.is_bool()) return emit_ABC(v.as_bool() ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE, dst.value, 0, 0, loc);
    if (v.is_int()) {
        i64 iv = v.as_int();
        if (iv >= -JUMP_OFFSET && iv <= JUMP_OFFSET)
            return emit_ABx(OpCode::LOAD_INT, dst.value, static_cast<u32>(iv + JUMP_OFFSET), loc);
    }

    return load_constant(dst, m_compiler_ptr->current_chunk()->add_constant(v), loc);
}

void ByteCodeEmitter::discharge(ExprResult const& r, RegType dst, SourceLocation loc)
{
    switch (r.kind) {
    case ExprResult::Kind::REG:
        if (r.reg_ != dst) emit_ABC(OpCode::MOVE, dst.value, r.reg_.value, 0, loc);
        return;
    case ExprResult::Kind::RELOC: patch_a(m_compiler_ptr->current_chunk(), r.reloc_pc, dst); return;
    case ExprResult::Kind::KINT: {
#if FA_USE_NANBOX
        /// r.ival only holds 64 bit int, so this is only applicable on NAN-BOXed Value
        if (r.ival > Value::int_max() || r.ival < Value::int_min()) {
            load_big_int(dst, m_compiler_ptr->current_chunk()->add_big_int(r.ival), loc);
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

PCType ByteCodeEmitter::emit_ABC(OpCode op, u32 a, u32 b, u32 c, SourceLocation loc)
{
    if (a > 0xFF || b > 0xFF || c > 0xFF) diagnostic::panic(ErrorCode::TOO_MANY_REGISTERS);
    return emit(make_ABC(op, static_cast<u8>(a), static_cast<u8>(b), static_cast<u8>(c)), loc);
}

PCType ByteCodeEmitter::emit_ABx(OpCode op, u32 a, u32 bx, SourceLocation loc)
{
    if (a > 0xFF) diagnostic::panic(ErrorCode::TOO_MANY_REGISTERS);
    if (bx > 0xFFFF) diagnostic::panic(ErrorCode::TOO_MANY_CONSTANTS);
    return emit(make_ABx(op, static_cast<u8>(a), static_cast<u16>(bx)), loc);
}

PCType ByteCodeEmitter::emit_AsBx(OpCode op, u32 a, i64 s_bx, SourceLocation loc)
{
    if (a > 0xFF) diagnostic::panic(ErrorCode::TOO_MANY_REGISTERS);
    if (s_bx < -JUMP_OFFSET || s_bx > JUMP_OFFSET) diagnostic::panic(ErrorCode::JUMP_OFFSET_OVERFLOW);
    return emit(make_AsBx(op, static_cast<u8>(a), s_bx), loc);
}

PCType ByteCodeEmitter::emit(InstrType instr, SourceLocation loc)
{
    return m_compiler_ptr->current_chunk()->emit(instr, loc);
}

void ByteCodeEmitter::patch_a(Chunk* ch, PCType pc, RegType a)
{
    InstrType instr = ch->code[pc.value];
    ch->code[pc.value] = InstrType((instr & 0xFF00FFFFu) | (static_cast<u32>(a.value) << 16));
}

} // namespace fairuz::runtime
