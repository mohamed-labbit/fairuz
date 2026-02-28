#pragma once

#include "../compiler/value.hpp"
#include "opcode.hpp"
#include <cinttypes>
#include <cstdint>
#include <string>
#include <vector>

namespace mylang {
namespace runtime {

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
