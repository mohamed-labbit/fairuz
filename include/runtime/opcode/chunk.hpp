#pragma once

#include "../compiler/value.hpp"
#include "opcode.hpp"
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
    uint32_t emit(Instruction instr, uint32_t line)
    {
        code.push_back(instr);
        addLine(line);
        return static_cast<uint32_t>(code.size() - 1);
    }

    // Patch a previously emitted jump with the real offset.
    // Returns false if offset overflows 16 bits.
    bool patchJump(uint32_t instr_idx)
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
    uint16_t addConstant(Value v)
    {
        // Deduplicate
        for (uint16_t i = 0; i < static_cast<uint16_t>(constants.size()); ++i) {
            if (constants[i] == v)
                return i;
        }

        constants.push_back(v);
        return static_cast<uint16_t>(constants.size() - 1);
    }

    // Allocate an IC slot, return its index.
    uint8_t allocIcSlot()
    {
        ic_slots.emplace_back();
        return static_cast<uint8_t>(ic_slots.size() - 1);
    }

    // Source line for instruction index (binary search)
    uint32_t getLine(uint32_t instr_idx) const
    {
        uint32_t line = 0;

        for (auto& e : lines) {
            if (e.start_instr > instr_idx)
                break;

            line = e.line;
        }

        return line;
    }

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
