//
// opcode.cc
//

#include "fopcode.hpp"
#include "fobject.hpp"
#include "fvalue.hpp"

#include <cstdint>
#include <iostream>

namespace fairuz::runtime {

static inline void print_value(Value v)
{
    if (v.is_nil())
        std::cout << "nil";
    else if (v.is_bool())
        std::cout << (v.as_bool() ? "صحيح" : "خطا");
    else if (v.is_int())
        std::cout << std::to_string(v.as_int());
    else if (v.is_double())
        std::cout << std::to_string(v.as_double());
    else if (v.is_string())
        std::cout << "\"" << v.as_string() << "\"";
    else if (v.is_obj())
        std::cout << "<obj " << std::to_string(reinterpret_cast<uintptr_t>(v.as_obj())) << ">";

    ::printf("?");
}

u32 Chunk::emit(u32 instr, SourceLocation loc)
{
    locations.push(loc);
    code.push(instr);
    add_line(loc.line);
    return static_cast<u32>(code.size() - 1);
}

bool Chunk::patch_jump(u32 const instr_idx)
{
    i32 const m_offset = static_cast<i32>(code.size()) - static_cast<i32>(instr_idx) - 1;
    if (m_offset > JUMP_OFFSET || m_offset < -JUMP_OFFSET)
        return false;

    OpCode op = instr_op(code[instr_idx]);
    u8 A = instr_A(code[instr_idx]);
    code[instr_idx] = make_AsBx(op, A, m_offset);
    return true;
}

u16 Chunk::add_constant(Value const v)
{
    for (u32 i = 0, n = constants.size(); i < n; i++) {
        if (constants[i] == v)
            return static_cast<u16>(i);
    }

    if (constants.size() > MAX_CONSTANTS)
        diagnostic::panic(ErrorCode::TOO_MANY_CONSTANTS);

    constants.push(v);
    return static_cast<u16>(constants.size() - 1);
}

u8 Chunk::alloc_ic_slot()
{
    if (ic_slots.size() > MAX_IC_SLOTS)
        diagnostic::panic(ErrorCode::TOO_MANY_INLINE_CACHES);

    ic_slots.push(ICSlot());
    return static_cast<u8>(ic_slots.size() - 1);
}

u32 Chunk::get_line(u32 const instr_idx) const
{
    u32 line = 0;

    for (auto& e : lines) {
        if (e.start > instr_idx)
            break;

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

    if (!ic_slots.empty())
        ::printf("  ic_slots: %u\n", ic_slots.size());

    ::printf("  code:\n");

    for (u32 i = 0; i < static_cast<u32>(code.size()); i++) {
        u32 ins = code[i];
        auto op = static_cast<OpCode>(instr_op(ins));
        auto fmt = opcode_format(op);
        u32 line = get_line(i);

        ::printf("    %04u  [%3u]  %-16s ", i, line, opcode_name(op).data());

        switch (fmt) {
        case InstrFormat::ABC:
            ::printf("A=%-3u  B=%-3u  C=%-3u", instr_A(ins), instr_B(ins), instr_C(ins));
            break;

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
                ::printf("  ; %d", static_cast<int>(instr_Bx(ins)) - 32767);
            } else if (op == OpCode::CLOSURE) {
                u16 idx = instr_Bx(ins);
                if (idx < functions.size())
                    ::printf("  ; fn '%s'", functions[idx]->name.data());
            }
            break;

        case InstrFormat::AsBx:
            ::printf("A=%-3u  sBx=%-5d  -> %d", instr_A(ins), instr_sBx(ins), static_cast<int>(i) + 1 + instr_sBx(ins));
            break;

        case InstrFormat::NONE:
            break;

        case InstrFormat::A:
            ::printf("A=%-3u", instr_A(ins));
            break;
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
    if (lines.empty() || lines.back().line != line)
        lines.push({ static_cast<u32>(code.size() - 1), line });
}

} // namespace fairuz::runtime
