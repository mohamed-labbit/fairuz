#include "../../../include/runtime/opcode/chunk.hpp"

namespace mylang {
namespace runtime {

uint32_t Chunk::emit(Instruction instr, uint32_t line)
{
    code.push_back(instr);
    addLine(line);
    return static_cast<uint32_t>(code.size() - 1);
}

// Patch a previously emitted jump with the real offset.
// Returns false if offset overflows 16 bits.
bool Chunk::patchJump(uint32_t instr_idx)
{
    int32_t offset = static_cast<int32_t>(code.size()) - static_cast<int32_t>(instr_idx) - 1;
    if (offset > 32767 || offset < -32768)
        return false;

    auto op = instr_op(code[instr_idx]);
    auto A = instr_A(code[instr_idx]);
    code[instr_idx] = make_AsBx(op, A, offset);

    return true;
}

// Add a constant; deduplicates integers and doubles and strings.
uint16_t Chunk::addConstant(Value v)
{
    // Deduplicate
    for (uint16_t i = 0; i < static_cast<uint16_t>(constants.size()); ++i)
        if (constants[i] == v)
            return i;

    constants.push_back(v);
    return static_cast<uint16_t>(constants.size() - 1);
}

// Allocate an IC slot, return its index.
uint8_t Chunk::allocIcSlot()
{
    ic_slots.emplace_back();
    return static_cast<uint8_t>(ic_slots.size() - 1);
}

// Source line for instruction index (binary search)
uint32_t Chunk::getLine(uint32_t instr_idx) const
{
    uint32_t line = 0;

    for (auto& e : lines) {
        if (e.start_instr > instr_idx)
            break;

        line = e.line;
    }

    return line;
}

void Chunk::disassemble() const
{
    ::printf("=== %s (arity=%d regs=%d upvals=%d) ===\n", name.data(), arity, local_count, upvalue_count);

    // Print constants
    if (!constants.empty()) {
        ::printf("  constants:\n");

        for (size_t i = 0; i < constants.size(); ++i) {
            ::printf("    [%3zu] ", i);
            print_value(constants[i]);
            ::printf("\n");
        }
    }

    // Print IC slots
    if (!ic_slots.empty())
        ::printf("  ic_slots: %zu\n", ic_slots.size());

    // Print instructions
    ::printf("  code:\n");

    for (uint32_t i = 0; i < static_cast<uint32_t>(code.size()); ++i) {
        Instruction ins = code[i];
        auto op = static_cast<OpCode>(instr_op(ins));
        auto fmt = opcode_format(op);
        uint32_t line = getLine(i);

        ::printf("    %04u  [%3u]  %-16s ", i, line, opcode_name(op).data());

        switch (fmt) {
        case InstrFmt::ABC:
            ::printf("A=%-3u  B=%-3u  C=%-3u", instr_A(ins), instr_B(ins), instr_C(ins));
            break;

        case InstrFmt::ABx:
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

        case InstrFmt::AsBx:
            ::printf("A=%-3u  sBx=%-5d  -> %d", instr_A(ins), instr_sBx(ins), static_cast<int>(i) + 1 + instr_sBx(ins));
            break;

        case InstrFmt::NONE:
            break;

        case InstrFmt::A:
            ::printf("A=%-3u", instr_A(ins));
            break;
        }

        ::printf("\n");
    }

    // Recurse into nested functions
    for (Chunk const* fn : functions) {
        ::printf("\n");
        fn->disassemble();
    }
}

}
}
