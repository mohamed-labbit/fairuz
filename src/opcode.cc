/// opcode.cc

#include "../include/value.hpp"

namespace mylang::runtime {

void print_value(uint64_t v)
{
    if (IS_NIL(v))
        ::printf("nil");
    else if (IS_BOOL(v))
        ::printf("%s", AS_BOOL(v) ? "true" : "false");
    else if (IS_INTEGER(v))
        ::printf("%lli", AS_INTEGER(v));
    else if (IS_DOUBLE(v))
        ::printf("%g", AS_DOUBLE(v));
    else if (IS_STRING(v))
        ::printf("\"%s\"", AS_STRING(v)->str.data());
    else if (IS_OBJECT(v))
        ::printf("<obj %p>", (void*)(AS_OBJECT(v)));
    ::printf("?");
}

uint32_t Chunk::emit(uint32_t instr, SourceLocation loc)
{
    locations.push(loc);
    code.push(instr);
    addLine(loc.line);
    return static_cast<uint32_t>(code.size() - 1);
}

bool Chunk::patchJump(uint32_t const instr_idx)
{
    int32_t const offset = static_cast<int32_t>(code.size()) - static_cast<int32_t>(instr_idx) - 1;
    if (offset > JUMP_OFFSET || offset < -JUMP_OFFSET)
        return false;

    OpCode op = instr_op(code[instr_idx]);
    uint8_t A = instr_A(code[instr_idx]);
    code[instr_idx] = make_AsBx(op, A, offset);
    return true;
}

uint16_t Chunk::addConstant(Value const v)
{
    for (uint16_t i = 0, n = static_cast<uint16_t>(constants.size()); i < n; ++i) {
        if (constants[i] == v)
            return i;
    }

    constants.push(v);
    return static_cast<uint16_t>(constants.size() - 1);
}

uint8_t Chunk::allocIcSlot()
{
    icSlots.push(ICSlot());
    return static_cast<uint8_t>(icSlots.size() - 1);
}

uint32_t Chunk::getLine(uint32_t const instr_idx) const
{
    uint32_t line = 0;

    for (auto& e : lines) {
        if (e.start > instr_idx)
            break;
        line = e.line;
    }

    return line;
}

void Chunk::disassemble() const
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

    for (uint32_t i = 0; i < static_cast<uint32_t>(code.size()); ++i) {
        uint32_t ins = code[i];
        auto op = static_cast<OpCode>(instr_op(ins));
        auto fmt = opcodeFormat(op);
        uint32_t line = getLine(i);

        ::printf("    %04u  [%3u]  %-16s ", i, line, opcode_name(op).data());

        switch (fmt) {
        case InstrFormat::ABC:
            ::printf("A=%-3u  B=%-3u  C=%-3u", instr_A(ins), instr_B(ins), instr_C(ins));
            break;

        case InstrFormat::ABx:
            ::printf("A=%-3u  Bx=%-5u", instr_A(ins), instr_Bx(ins));
            // Annotate with constant value
            if (op == OpCode::LOAD_CONST || op == OpCode::LOAD_GLOBAL || op == OpCode::STORE_GLOBAL) {
                uint16_t idx = instr_Bx(ins);

                if (idx < constants.size()) {
                    ::printf("  ; ");
                    print_value(constants[idx]);
                }
            } else if (op == OpCode::LOAD_INT) {
                ::printf("  ; %d", static_cast<int>(instr_Bx(ins)) - 32767);
            } else if (op == OpCode::CLOSURE) {
                uint16_t idx = instr_Bx(ins);
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

void Chunk::addLine(uint32_t line)
{
    if (lines.empty() || lines.back().line != line)
        lines.push({ static_cast<uint32_t>(code.size() - 1), line });
}

} // namespace mylang::runtime
