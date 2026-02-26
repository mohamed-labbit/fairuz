#pragma once

#include "../../types/string.hpp"
#include "../opcode/chunk.hpp"
#include "../opcode/opcode.hpp"

#include <cinttypes>
#include <cstdio>

namespace mylang {
namespace runtime {

// ---------------------------------------------------------------------------
// opcode_name / opcode_format  (defined here so chunk.hpp stays header-only)
// ---------------------------------------------------------------------------

StringRef opcode_name(OpCode op)
{
    switch (op) {
    case OpCode::LOAD_NIL: return "LOAD_NIL";
    case OpCode::LOAD_TRUE: return "LOAD_TRUE";
    case OpCode::LOAD_FALSE: return "LOAD_FALSE";
    case OpCode::LOAD_CONST: return "LOAD_CONST";
    case OpCode::LOAD_INT: return "LOAD_INT";
    case OpCode::LOAD_GLOBAL: return "LOAD_GLOBAL";
    case OpCode::STORE_GLOBAL: return "STORE_GLOBAL";
    case OpCode::MOVE: return "MOVE";
    case OpCode::GET_UPVALUE: return "GET_UPVALUE";
    case OpCode::SET_UPVALUE: return "SET_UPVALUE";
    case OpCode::CLOSE_UPVALUE: return "CLOSE_UPVALUE";
    case OpCode::ADD: return "ADD";
    case OpCode::SUB: return "SUB";
    case OpCode::MUL: return "MUL";
    case OpCode::DIV: return "DIV";
    case OpCode::IDIV: return "IDIV";
    case OpCode::MOD: return "MOD";
    case OpCode::POW: return "POW";
    case OpCode::NEG: return "NEG";
    case OpCode::BAND: return "BAND";
    case OpCode::BOR: return "BOR";
    case OpCode::BXOR: return "BXOR";
    case OpCode::BNOT: return "BNOT";
    case OpCode::SHL: return "SHL";
    case OpCode::SHR: return "SHR";
    case OpCode::EQ: return "EQ";
    case OpCode::NEQ: return "NEQ";
    case OpCode::LT: return "LT";
    case OpCode::LE: return "LE";
    case OpCode::NOT: return "NOT";
    case OpCode::CONCAT: return "CONCAT";
    case OpCode::NEW_LIST: return "NEW_LIST";
    case OpCode::LIST_APPEND: return "LIST_APPEND";
    case OpCode::LIST_GET: return "LIST_GET";
    case OpCode::LIST_SET: return "LIST_SET";
    case OpCode::LIST_LEN: return "LIST_LEN";
    case OpCode::JUMP: return "JUMP";
    case OpCode::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
    case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
    case OpCode::LOOP: return "LOOP";
    case OpCode::FOR_PREP: return "FOR_PREP";
    case OpCode::FOR_STEP: return "FOR_STEP";
    case OpCode::CLOSURE: return "CLOSURE";
    case OpCode::CALL: return "CALL";
    case OpCode::CALL_TAIL: return "CALL_TAIL";
    case OpCode::RETURN: return "RETURN";
    case OpCode::RETURN_NIL: return "RETURN_NIL";
    case OpCode::IC_CALL: return "IC_CALL";
    case OpCode::NOP: return "NOP";
    case OpCode::HALT: return "HALT";
    default: return "???";
    }
}

InstrFmt opcode_format(OpCode op)
{
    switch (op) {
    case OpCode::LOAD_CONST:
    case OpCode::LOAD_INT:
    case OpCode::LOAD_GLOBAL:
    case OpCode::STORE_GLOBAL:
    case OpCode::CLOSURE:
        return InstrFmt::ABx;

    case OpCode::JUMP:
    case OpCode::JUMP_IF_TRUE:
    case OpCode::JUMP_IF_FALSE:
    case OpCode::LOOP:
    case OpCode::FOR_PREP:
    case OpCode::FOR_STEP:
        return InstrFmt::AsBx;

    case OpCode::RETURN_NIL:
    case OpCode::HALT:
    case OpCode::NOP:
        return InstrFmt::NONE;

    default:
        return InstrFmt::ABC;
    }
}

// ---------------------------------------------------------------------------
// Chunk::disassemble
// ---------------------------------------------------------------------------

static void print_value(Value v)
{
    if (v.isNil())
        ::printf("nil");
    else if (v.isBool())
        ::printf("%s", v.asBool() ? "true" : "false");
    else if (v.isInt())
        ::printf("%" PRId64, v.asInt());
    else if (v.isDouble())
        ::printf("%g", v.asDouble());
    else if (v.isString())
        ::printf("\"%s\"", v.asString()->chars.data());
    else if (v.isObj())
        ::printf("<obj %p>", (void*)v.asObj());

    ::printf("?");
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

} // namespace runtime
} // namespace mylang
