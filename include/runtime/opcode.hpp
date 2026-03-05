#ifndef _OPCODE_HPP
#define _OPCODE_HPP

#include "value.hpp"

#include <cinttypes>
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

// ---------------------------------------------------------------------------
// TypeProfile — collected at runtime to guide the JIT.
// Each call-site or binary-op slot records the types it has seen.
// Represented as a bitmask so the JIT can quickly check "always int?".
// ---------------------------------------------------------------------------
enum class TypeTag : uint8_t {
    NONE = 0,
    NIL = 1 << 0,
    BOOL = 1 << 1,
    INT = 1 << 2,
    DOUBLE = 1 << 3,
    STRING = 1 << 4,
    LIST = 1 << 5,
    CLOSURE = 1 << 6,
    NATIVE = 1 << 7,
};

inline TypeTag operator|(TypeTag a, TypeTag b)
{
    return static_cast<TypeTag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline TypeTag& operator|=(TypeTag& a, TypeTag b)
{
    a = a | b;
    return a;
}

inline bool has_tag(TypeTag mask, TypeTag t)
{
    return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(t)) != 0;
}

inline TypeTag value_type_tag(Value v)
{
    if (v.isNil())
        return TypeTag::NIL;
    if (v.isBool())
        return TypeTag::BOOL;
    if (v.isInt())
        return TypeTag::INT;
    if (v.isDouble())
        return TypeTag::DOUBLE;
    if (v.isString())
        return TypeTag::STRING;
    if (v.isList())
        return TypeTag::LIST;
    if (v.isClosure())
        return TypeTag::CLOSURE;
    if (v.isNative())
        return TypeTag::NATIVE;

    return TypeTag::NONE;
}

struct ICSlot {
    TypeTag seen_lhs = TypeTag::NONE; // for binary ops
    TypeTag seen_rhs = TypeTag::NONE;
    TypeTag seen_ret = TypeTag::NONE; // for calls
    uint32_t hit_count = 0;           // execution count
    void* jit_stub = nullptr;         // patched by JIT, null = not compiled
};

// ---------------------------------------------------------------------------
// DebugInfo — maps instruction index → source line for error reporting.
// Run-length encoded to save memory.
// ---------------------------------------------------------------------------
struct LineEntry {
    uint32_t start_instr; // first instruction index with this line
    uint32_t line;
};

static StringRef opcode_name(OpCode op)
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
    default:
        return "???";
    }
}

static InstrFmt opcode_format(OpCode op)
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

// ---------------------------------------------------------------------------
// Chunk — the bytecode + constant pool for one function or script.
// ---------------------------------------------------------------------------
struct Chunk {
    // --- identity ---
    StringRef name; // "<main>" or function name
    int arity = 0;
    int local_count = 0; // number of registers needed (set by compiler)
    int upvalue_count = 0;

    // --- code ---
    std::vector<Instruction> code;
    std::vector<Value> constants; // constant pool
    std::vector<LineEntry> lines; // debug info

    // --- nested functions (closures defined in this chunk) ---
    std::vector<Chunk*> functions; // owned

    // --- type profile slots (one per IC_CALL / binary op that requested one) ---
    std::vector<ICSlot> ic_slots;

    Chunk() = default;
    ~Chunk() = default;

    // non-copyable — chunks own their sub-chunks
    Chunk(Chunk const&) = delete;
    Chunk(Chunk&&) = default;

    Chunk& operator=(Chunk const&) = delete;
    Chunk& operator=(Chunk&&) = default;

    // ---- emit helpers ----
    uint32_t emit(Instruction instr, uint32_t line);

    // Patch a previously emitted jump with the real offset.
    // Returns false if offset overflows 16 bits.
    bool patchJump(uint32_t instr_idx);

    // Add a constant; deduplicates integers and doubles and strings.
    uint16_t addConstant(Value v);

    // Allocate an IC slot, return its index.
    uint8_t allocIcSlot();

    // Source line for instruction index (binary search)
    uint32_t getLine(uint32_t instr_idx) const;

    // Disassemble to stdout (defined in disassembler.cpp)
    void disassemble() const;

private:
    void addLine(uint32_t line)
    {
        if (lines.empty() || lines.back().line != line)
            lines.push_back({ static_cast<uint32_t>(code.size() - 1), line });
    }
};

} // namespace runtime
} // namespace mylang

#endif // _OPCODE_HPP
