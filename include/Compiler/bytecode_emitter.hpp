//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Defines compiler bytecode emitter.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_COMPILER_BYTECODE_EMITTER_HPP
#define FAIRUZ_COMPILER_BYTECODE_EMITTER_HPP

#include "Macros.hpp"
#include "opcode.hpp"

namespace fairuz::runtime {

class Compiler;
class Value;
struct ExprResult;
struct Chunk;

class ByteCodeEmitter {
private:
    Compiler* m_compiler_ptr;

public:
    ByteCodeEmitter(Compiler* c)
        : m_compiler_ptr(c)
    {
    }

    PCType binary(OpCode op, RegType dst, RegType lhs, RegType rhs, SourceLocation loc);
    PCType binary_immediate(OpCode op, RegType dst, RegType lhs, u8 rhs, SourceLocation loc);
    PCType get_named_field(RegType dst, RegType obj, ConstantIndex name_idx, SourceLocation loc);
    PCType set_named_field(RegType obj, RegType src, ConstantIndex name_idx, SourceLocation loc);
    PCType invoke(RegType obj, MethodSlot slot, u32 argc, SourceLocation loc);
    PCType invoke_named(RegType obj, u32 argc, ConstantIndex name_idx, SourceLocation loc);
    PCType ic_call(RegType obj, u32 argc, SourceLocation loc);
    PCType emit_cache_payload(CacheSlot idx, SourceLocation loc);
    PCType emit_name_payload(ConstantIndex idx, SourceLocation loc);

    PCType load_constant(RegType dst, ConstantIndex idx, SourceLocation loc);
    PCType load_big_int(RegType dst, BigIntIndex idx, SourceLocation loc);
    PCType load_global(RegType dst, ConstantIndex name, SourceLocation loc);
    PCType store_global(RegType src, ConstantIndex name, SourceLocation loc);
    PCType import_module(RegType dst, ConstantIndex name, SourceLocation loc);
    PCType closure(RegType dst, FunctionIndex idx, SourceLocation loc);
    PCType new_class(RegType dst, ClassIndex idx, SourceLocation loc);
    PCType jump_if_false(RegType cond_reg, SourceLocation loc);
    PCType jump(OpCode op, RegType cond, SourceLocation loc);
    PCType jump(SourceLocation loc);
    PCType return_nil(SourceLocation loc);
    void patch_jump(PCType at);
    void patch_jump_to(PCType instr_idx, PCType target);
    PCType emit_load_value(RegType dst, Value v, SourceLocation loc);
    void discharge(ExprResult const& r, RegType dst, SourceLocation loc);

    // Raw encoding boundary. Accept wide values so overflow is diagnosed
    // before narrowing; these fields are not uniformly register identities.
    PCType emit_ABC(OpCode op, u32 a, u32 b, u32 c, SourceLocation loc);
    PCType emit_ABx(OpCode op, u32 a, u32 bx, SourceLocation loc);
    PCType emit_AsBx(OpCode op, u32 a, i64 s_bx, SourceLocation loc);

private:
    PCType emit(InstrType instr, SourceLocation loc);

    static void patch_a(Chunk* ch, PCType pc, RegType a);
};

} // namespace fairuz::runtime

#endif // FAIRUZ_COMPILER_BYTECODE_EMITTER_HPP
