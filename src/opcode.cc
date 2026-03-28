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
    addLine(loc.line);
    return static_cast<u32>(code.size() - 1);
}

bool Fa_Chunk::patchJump(u32 const instr_idx)
{
    i32 const offset = static_cast<i32>(code.size()) - static_cast<i32>(instr_idx) - 1;
    if (offset > JUMP_OFFSET || offset < -JUMP_OFFSET)
        return false;

    Fa_OpCode op = Fa_instr_op(code[instr_idx]);
    u8 A = Fa_instr_A(code[instr_idx]);
    code[instr_idx] = Fa_make_AsBx(op, A, offset);
    return true;
}

u16 Fa_Chunk::addConstant(Fa_Value const v)
{
    for (u16 i = 0, n = static_cast<u16>(constants.size()); i < n; ++i) {
        if (constants[i] == v)
            return i;
    }

    constants.push(v);
    return static_cast<u16>(constants.size() - 1);
}

u8 Fa_Chunk::allocIcSlot()
{
    icSlots.push(Fa_ICSlot());
    return static_cast<u8>(icSlots.size() - 1);
}

u32 Fa_Chunk::getLine(u32 const instr_idx) const
{
    u32 line = 0;

    for (auto& e : lines) {
        if (e.start > instr_idx)
            break;
        line = e.line;
    }

    return line;
}

void Fa_Chunk::disassemble() const
{
    ::printf("=== %s (arity=%d regs=%d upvals=%d) ===\n", name.data(), arity, localCount, upvalueCount);

    if (!constants.empty()) {
        ::printf("  constants:\n");

        for (size_t i = 0; i < constants.size(); ++i) {
            ::printf("    [%3zu] ", i);
            print_value(constants[i]);
            ::printf("\n");
        }
    }

    if (!icSlots.empty())
        ::printf("  ic_slots: %u\n", icSlots.size());

    ::printf("  code:\n");

    for (u32 i = 0; i < static_cast<u32>(code.size()); ++i) {
        u32 ins = code[i];
        auto op = static_cast<Fa_OpCode>(Fa_instr_op(ins));
        auto fmt = opcodeFormat(op);
        u32 line = getLine(i);

        ::printf("    %04u  [%3u]  %-16s ", i, line, Fa_opcode_name(op).data());

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
                    ::printf("  ; fn '%s'", functions[idx]->name.data());
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

void Fa_Chunk::addLine(u32 line)
{
    if (lines.empty() || lines.back().line != line)
        lines.push({ static_cast<u32>(code.size() - 1), line });
}

} // namespace fairuz::runtime
