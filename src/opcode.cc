/// opcode.cc

#include "../include/value.hpp"

namespace fairuz::runtime {

void print_value(u64 v)
{
    if (Fa_IS_NIL(v))
        ::printf("nil");
    else if (Fa_IS_BOOL(v))
        ::printf("%s", Fa_AS_BOOL(v) ? "true" : "false");
    else if (Fa_IS_INTEGER(v))
        ::printf("%lli", Fa_AS_INTEGER(v));
    else if (Fa_IS_DOUBLE(v))
        ::printf("%g", Fa_AS_DOUBLE(v));
    else if (Fa_IS_STRING(v))
        ::printf("\"%s\"", Fa_AS_STRING(v)->str.data());
    else if (Fa_IS_OBJECT(v))
        ::printf("<obj %p>", (void*)(Fa_AS_OBJECT(v)));
    ::printf("?");
}

u32 Fa_Chunk::emit(u32 instr, Fa_SourceLocation loc)
{
    locations.push(loc);
    code.push(instr);
    add_line(loc.m_line);
    return static_cast<u32>(code.size() - 1);
}

bool Fa_Chunk::patch_jump(u32 const instr_idx)
{
    i32 const m_offset = static_cast<i32>(code.size()) - static_cast<i32>(instr_idx) - 1;
    if (m_offset > JUMP_OFFSET || m_offset < -JUMP_OFFSET)
        return false;

    Fa_OpCode op = Fa_instr_op(code[instr_idx]);
    u8 A = Fa_instr_A(code[instr_idx]);
    code[instr_idx] = Fa_make_AsBx(op, A, m_offset);
    return true;
}

u16 Fa_Chunk::add_constant(Fa_Value const v)
{
    for (u16 i = 0, n = static_cast<u16>(constants.size()); i < n; ++i) {
        if (constants[i] == v)
            return i;
    }

    constants.push(v);
    return static_cast<u16>(constants.size() - 1);
}

u8 Fa_Chunk::alloc_ic_slot()
{
    ic_slots.push(Fa_ICSlot());
    return static_cast<u8>(ic_slots.size() - 1);
}

u32 Fa_Chunk::get_line(u32 const instr_idx) const
{
    u32 m_line = 0;

    for (auto& e : lines) {
        if (e.start > instr_idx)
            break;
        m_line = e.m_line;
    }

    return m_line;
}

void Fa_Chunk::disassemble() const
{
    ::printf("=== %s (arity=%d regs=%d upvals=%d) ===\n", m_name.data(), arity, local_count, upvalue_count);

    if (!constants.empty()) {
        ::printf("  constants:\n");

        for (size_t i = 0; i < constants.size(); ++i) {
            ::printf("    [%3zu] ", i);
            print_value(constants[i]);
            ::printf("\n");
        }
    }

    if (!ic_slots.empty())
        ::printf("  ic_slots: %u\n", ic_slots.size());

    ::printf("  code:\n");

    for (u32 i = 0; i < static_cast<u32>(code.size()); ++i) {
        u32 ins = code[i];
        auto op = static_cast<Fa_OpCode>(Fa_instr_op(ins));
        auto fmt = opcode_format(op);
        u32 m_line = get_line(i);

        ::printf("    %04u  [%3u]  %-16s ", i, m_line, Fa_opcode_name(op).data());

        switch (fmt) {
        case Fa_InstrFormat::ABC:
            ::printf("A=%-3u  B=%-3u  C=%-3u", Fa_instr_A(ins), Fa_instr_B(ins), Fa_instr_C(ins));
            break;

        case Fa_InstrFormat::ABx:
            ::printf("A=%-3u  Bx=%-5u", Fa_instr_A(ins), Fa_instr_Bx(ins));
            // Annotate with constant value
            if (op == Fa_OpCode::LOAD_CONST || op == Fa_OpCode::LOAD_GLOBAL || op == Fa_OpCode::STORE_GLOBAL) {
                u16 idx = Fa_instr_Bx(ins);

                if (idx < constants.size()) {
                    ::printf("  ; ");
                    print_value(constants[idx]);
                }
            } else if (op == Fa_OpCode::LOAD_INT) {
                ::printf("  ; %d", static_cast<int>(Fa_instr_Bx(ins)) - 32767);
            } else if (op == Fa_OpCode::CLOSURE) {
                u16 idx = Fa_instr_Bx(ins);
                if (idx < functions.size())
                    ::printf("  ; fn '%s'", functions[idx]->m_name.data());
            }
            break;

        case Fa_InstrFormat::AsBx:
            ::printf("A=%-3u  sBx=%-5d  -> %d", Fa_instr_A(ins), Fa_instr_sBx(ins), static_cast<int>(i) + 1 + Fa_instr_sBx(ins));
            break;

        case Fa_InstrFormat::NONE:
            break;

        case Fa_InstrFormat::A:
            ::printf("A=%-3u", Fa_instr_A(ins));
            break;
        }

        ::printf("\n");
    }

    for (Fa_Chunk const* fn : functions) {
        ::printf("\n");
        fn->disassemble();
    }
}

void Fa_Chunk::add_line(u32 m_line)
{
    if (lines.empty() || lines.back().m_line != m_line)
        lines.push({ static_cast<u32>(code.size() - 1), m_line });
}

} // namespace fairuz::runtime
