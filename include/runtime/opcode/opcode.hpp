#pragma once

#include <cstddef>
#include <cstdint>

namespace mylang {
namespace runtime {

// ---------------------------------------------------------------------------
// Instruction encoding — fixed 32-bit words.
//
//  Format ABC  (most instructions):
//   31..24  opcode  (8 bits)
//   23..16  A       (8 bits)  — destination register
//   15.. 8  B       (8 bits)  — source register / small immediate
//    7.. 0  C       (8 bits)  — source register / small immediate
//
//  Format ABx  (LOAD_CONST, CLOSURE, …):
//   31..24  opcode  (8 bits)
//   23..16  A       (8 bits)
//   15.. 0  Bx      (16 bits) — unsigned index into constant table
//
//  Format AsBx (JUMP, LOOP, …):
//   31..24  opcode  (8 bits)
//   23..16  A       (8 bits)  — condition register (or unused=0)
//   15.. 0  sBx     (16 bits) — signed offset, bias = 32767
//
// Register 0..254 are general-purpose per-function registers.
// Register 255 is reserved as a "no register" sentinel.
// ---------------------------------------------------------------------------

using Instruction = uint32_t;

// ---- field extraction/packing ----

inline uint8_t instr_op(Instruction i)
{
    return (i >> 24) & 0xFF;
}

inline uint8_t instr_A(Instruction i)
{
    return (i >> 16) & 0xFF;
}

inline uint8_t instr_B(Instruction i)
{
    return (i >> 8) & 0xFF;
}

inline uint8_t instr_C(Instruction i)
{
    return i & 0xFF;
}

inline uint16_t instr_Bx(Instruction i)
{
    return i & 0xFFFF;
}

inline int16_t instr_sBx(Instruction i)
{
    return static_cast<int16_t>(i & 0xFFFF) - 32767;
}

inline Instruction make_ABC(uint8_t op, uint8_t A, uint8_t B, uint8_t C)
{
    return (static_cast<uint32_t>(op) << 24)
        | (static_cast<uint32_t>(A) << 16)
        | (static_cast<uint32_t>(B) << 8)
        | static_cast<uint32_t>(C);
}

inline Instruction make_ABx(uint8_t op, uint8_t A, uint16_t Bx)
{
    return (static_cast<uint32_t>(op) << 24)
        | (static_cast<uint32_t>(A) << 16)
        | static_cast<uint32_t>(Bx);
}

inline Instruction make_AsBx(uint8_t op, uint8_t A, int offset)
{
    // offset is relative to the instruction AFTER the jump
    uint16_t sBx = static_cast<uint16_t>(offset + 32767);
    return (static_cast<uint32_t>(op) << 24)
        | (static_cast<uint32_t>(A) << 16)
        | static_cast<uint32_t>(sBx);
}

static constexpr uint8_t REG_NONE = 255;
static constexpr uint16_t MAX_CONSTANTS = 65535;
static constexpr uint8_t MAX_REGISTERS = 250; // leave room for sentinel + scratch

// ---------------------------------------------------------------------------
// OpCode table
// ---------------------------------------------------------------------------
enum class OpCode : uint8_t {
    // ---- loads ----
    LOAD_NIL,     // A: dst, B: start, C: count  — fill [B..B+C) with nil
    LOAD_TRUE,    // A: dst
    LOAD_FALSE,   // A: dst
    LOAD_CONST,   // A: dst, Bx: const index
    LOAD_INT,     // A: dst, Bx: signed 16-bit integer (bias 32767)
    LOAD_GLOBAL,  // A: dst, Bx: name const index
    STORE_GLOBAL, // A: src, Bx: name const index

    // ---- register moves ----
    MOVE, // A: dst, B: src

    // ---- upvalue ops ----
    GET_UPVALUE,   // A: dst, B: upvalue index
    SET_UPVALUE,   // A: src, B: upvalue index
    CLOSE_UPVALUE, // A: first register to close (close all above A)

    // ---- arithmetic (integer-specialised paths in VM) ----
    ADD,  // A=B+C
    SUB,  // A=B-C
    MUL,  // A=B*C
    DIV,  // A=B/C  (float division)
    IDIV, // A=B//C (integer floor division)
    MOD,  // A=B%C
    POW,  // A=B**C
    NEG,  // A=-B

    // ---- bitwise ----
    BAND, // A=B&C
    BOR,  // A=B|C
    BXOR, // A=B^C
    BNOT, // A=~B
    SHL,  // A=B<<C
    SHR,  // A=B>>C

    // ---- comparison (result in A: 0 or 1) ----
    EQ,  // A = (B == C)
    NEQ, // A = (B != C)
    LT,  // A = (B <  C)
    LE,  // A = (B <= C)

    // ---- logical ----
    NOT, // A = !B (boolean)

    // ---- string ----
    CONCAT, // A: dst, B: first reg, C: count  — concat C registers starting at B

    // ---- list ----
    NEW_LIST,    // A: dst, B: initial capacity hint
    LIST_APPEND, // A: list reg, B: value reg
    LIST_GET,    // A: dst, B: list reg, C: index reg
    LIST_SET,    // A: list reg, B: index reg, C: value reg
    LIST_LEN,    // A: dst, B: list reg

    // ---- jumps ----
    JUMP,          // sBx: unconditional offset
    JUMP_IF_TRUE,  // A: cond reg, sBx: offset if truthy  (does NOT pop)
    JUMP_IF_FALSE, // A: cond reg, sBx: offset if falsy
    LOOP,          // sBx: backward offset (negative = back)

    // ---- numeric for loop (fast path) ----
    FOR_PREP, // A: base (limit=A+1, step=A+2, idx=A+3), sBx: jump past body if done
    FOR_STEP, // A: base, sBx: jump back to body top if not done

    // ---- functions ----
    CLOSURE,    // A: dst, Bx: function const index
                // followed by upvalue descriptors: one MOVE per upvalue
    CALL,       // A: func reg, B: argc, C: expected results (255=discard all)
    CALL_TAIL,  // A: func reg, B: argc  — tail-call optimised
    RETURN,     // A: first result reg, B: result count (0 = return nil)
    RETURN_NIL, // no operands — fast path

    // ---- inline-cache hint (emitted on polymorphic call sites) ----
    IC_CALL, // A: func reg, B: argc, C: IC slot index

    // ---- misc ----
    NOP,
    HALT,

    _COUNT
};

// Instruction format, for disassembly
enum class InstrFmt {
    ABC,
    ABx,
    AsBx,
    A,
    NONE
};

} // namespace runtime
} // namespace mylang
