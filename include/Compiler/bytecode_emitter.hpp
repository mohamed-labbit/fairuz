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
    PCType get_named_field(RegType dst, RegType obj, RegType name, IndexType ic_idx, SourceLocation loc);
    PCType set_named_field(RegType obj, RegType field, RegType src, IndexType ic_idx, SourceLocation loc);
    PCType invoke(RegType obj, RegType slot, RegType argc, SourceLocation loc);
    PCType invoke_named(RegType obj, RegType slot, RegType argc, IndexType idx, SourceLocation loc);
    PCType ic_call(RegType obj, RegType argc, SourceLocation loc);
    PCType nop(IndexType idx, SourceLocation loc);
    PCType jump_if_false(RegType cond_reg, SourceLocation loc);
    PCType return_nil(SourceLocation loc);
    void patch_jump(PCType at);
    PCType emit_load_value(PCType dst, Value v, SourceLocation loc);
    void discharge(ExprResult const& r, RegType dst, SourceLocation loc);

    PCType emit_ABC(OpCode op, RegType a, RegType b, RegType c, SourceLocation loc);
    PCType emit_ABx(OpCode op, RegType a, IndexType idx, SourceLocation loc);
    PCType emit_AsBx(OpCode op, RegType a, i64 s_bx, SourceLocation loc);

private:
    PCType emit(InstrType instr, SourceLocation loc);

    static void patch_a(Chunk* ch, PCType pc, RegType a);
};

} // namespace fairuz::runtime

#endif // FAIRUZ_COMPILER_BYTECODE_EMITTER_HPP
