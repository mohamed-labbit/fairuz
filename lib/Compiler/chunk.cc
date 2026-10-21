//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Bytecode Chunk method implementations.
//===----------------------------------------------------------------------===//

#include "Compiler/chunk.hpp"
#include "Macros.hpp"

#include <iostream>

namespace fairuz::runtime {

static inline void print_value(Value v)
{
    if (v.is_nil()) std::cout << "nil";
    else if (v.is_bool()) std::cout << (v.as_bool() ? "صحيح" : "خطا");
    else if (v.is_int()) std::cout << integer::to_string(v);
    else if (v.is_double()) std::cout << std::to_string(v.as_double());
    else if (v.is_string()) std::cout << "\"" << v.as_string() << "\"";
    else if (v.is_obj()) std::cout << "<obj " << std::to_string(reinterpret_cast<uintptr_t>(v.as_obj())) << ">";

    ::printf("?");
}

PCType Chunk::emit(InstrType instr, SourceLocation loc)
{
    locations.push(loc);
    code.push(instr);
    add_line(loc.line);
    return PCType { code.size() - 1 };
}

bool Chunk::patch_jump(PCType const instr_idx)
{
    assert(instr_idx.value < code.size());
    i64 const offset = static_cast<i64>(code.size()) - instr_idx.value - 1;
    if (offset > JUMP_OFFSET || offset < -JUMP_OFFSET) return false;

    OpCode op = instr_op(code[instr_idx.value]);
    u8 A = instr_A(code[instr_idx.value]);
    code[instr_idx.value] = make_AsBx(op, A, offset);
    return true;
}

BigIntIndex Chunk::add_big_int(integer::Data const& v)
{
    for (u32 i = 0, n = big_ints.size(); i < n; i++) {
        if (big_ints[i] == v) return BigIntIndex { static_cast<u16>(i) };
    }

    if (big_ints.size() > MAX_CONSTANTS) diagnostic::panic(ErrorCode::TOO_MANY_CONSTANTS);

    big_ints.push(v);
    return BigIntIndex { static_cast<u16>(big_ints.size() - 1) };
}

ConstantIndex Chunk::add_constant(Value const v)
{
    for (u32 i = 0, n = constants.size(); i < n; i++) {
        if (ValueEqual { }(constants[i], v)) return ConstantIndex { static_cast<u16>(i) };
    }

    if (constants.size() > MAX_CONSTANTS) diagnostic::panic(ErrorCode::TOO_MANY_CONSTANTS);

    constants.push(v);
    return ConstantIndex { static_cast<u16>(constants.size() - 1) };
}

FunctionIndex Chunk::add_function(Chunk* function)
{
    if (functions.size() > MAX_CONSTANTS) diagnostic::panic(ErrorCode::TOO_MANY_FUNCTIONS);
    FunctionIndex index { static_cast<u16>(functions.size()) };
    functions.push(function);
    return index;
}

ClassIndex Chunk::add_class_descriptor(ClassDescriptor&& descriptor)
{
    if (class_descriptors.size() > MAX_CONSTANTS) diagnostic::panic(ErrorCode::TOO_MANY_CONSTANTS);
    ClassIndex index { static_cast<u16>(class_descriptors.size()) };
    class_descriptors.push(std::move(descriptor));
    return index;
}

CacheSlot Chunk::alloc_ic_slot()
{
    if (ic_slots.size() > MAX_IC_SLOTS) diagnostic::panic(ErrorCode::TOO_MANY_INLINE_CACHES);

    ic_slots.push(ICSlot());
    return CacheSlot { static_cast<u8>(ic_slots.size() - 1) };
}

u32 Chunk::get_line(u32 const instr_idx) const
{
    u32 line = 0;

    for (auto& e : lines) {
        if (e.start > instr_idx) break;

        line = e.line;
    }

    return line;
}

/// NOTE: this disassembler is entirely generated using Anthropic's Claude (with mods for style)
void Chunk::disassemble() const
{
    ::printf("=== %s (arity=%d regs=%d) ===\n", name.data(), arity, local_count);

    if (!constants.empty()) {
        ::printf("  constants:\n");

        for (size_t i = 0; i < constants.size(); i++) {
            ::printf("    [%3zu] ", i);
            print_value(constants[i]);
            ::printf("\n");
        }
    }

    if (!ic_slots.empty()) ::printf("  ic_slots: %u\n", ic_slots.size());

    ::printf("  code:\n");

    for (u32 i = 0; i < static_cast<u32>(code.size()); i++) {
        InstrType ins = code[i];
        auto op = static_cast<OpCode>(instr_op(ins));
        auto fmt = opcode_format(op);
        u32 line = get_line(i);

        ::printf("    %04u  [%3u]  %-16s ", i, line, opcode_name(op).data());

        switch (fmt) {
        case InstrFormat::ABC: ::printf("A=%-3u  B=%-3u  C=%-3u", instr_A(ins), instr_B(ins), instr_C(ins)); break;

        case InstrFormat::ABx:
            ::printf("A=%-3u  Bx=%-5u", instr_A(ins), instr_Bx(ins));
            // Annotate with constant value
            if (op == OpCode::LOAD_CONST || op == OpCode::LOAD_GLOBAL || op == OpCode::STORE_GLOBAL
                || op == OpCode::IMPORT_MODULE) {
                u16 idx = instr_Bx(ins);
                if (idx < constants.size()) {
                    ::printf("  ; ");
                    print_value(constants[idx]);
                }
            } else if (op == OpCode::LOAD_INT) {
                ::printf("  ; %d", instr_sBx(ins));
            } else if (op == OpCode::CLOSURE) {
                u16 idx = instr_Bx(ins);
                if (idx < functions.size()) ::printf("  ; fn '%s'", functions[idx]->name.data());
            }
            break;

        case InstrFormat::AsBx:
            ::printf("A=%-3u  sBx=%-5d  -> %d", instr_A(ins), instr_sBx(ins), static_cast<int>(i) + 1 + instr_sBx(ins));
            break;

        case InstrFormat::NONE: break;

        case InstrFormat::A: ::printf("A=%-3u", instr_A(ins)); break;
        }

        ::printf("\n");
    }

    for (Chunk const* fn : functions) {
        ::printf("\n");
        fn->disassemble();
    }
}

void Chunk::add_line(u32 line)
{
    if (lines.empty() || lines.back().line != line) lines.push({ static_cast<u32>(code.size() - 1), line });
}

}
