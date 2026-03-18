#ifndef OPCODE_HPP
#define OPCODE_HPP

#include "../array.hpp"
#include "../string.hpp"
#include <cinttypes>
#include <cstdint>
#include <utility>

namespace mylang::runtime {

static constexpr uint16_t JUMP_OFFSET = 32767;
static constexpr uint8_t REG_NONE = 0xFF;
static constexpr uint16_t MAX_CONSTANTS = 0xFFFF;
static constexpr uint8_t MAX_REGS = 250; // room for sentinel + scratch

enum class OpCode : uint8_t {
    /// [Op]: [A] , [B], [C]
    /// or
    /// [Op]: [A] , [  Bx  ]
    /// or
    /// [Op]: [A] , [  sBx ]

    LOAD_NIL,     // dst, Start, Count - fill [B..B+C) with nil
    LOAD_TRUE,    // dst, - , -
    LOAD_FALSE,   // dst, - , -
    LOAD_CONST,   // dst, Const pool index
    LOAD_INT,     // dst, signed 16-bit int (with bias, for larger ints use uint64_t)
    LOAD_GLOBAL,  // dst, name const index
    STORE_GLOBAL, // src, name const index

    MOVE, // dst, src, -

    GET_UPVALUE,   // dst, index
    SET_UPVALUE,   // src, index
    CLOSE_UPVALUE, // first reg to close (close all the above), - , -

    // * is the value inside the given reg, if you have to ask ,then don't touch this code
    // *A = *left OP *right
    OP_ADD,
    OP_ADD_II,
    OP_ADD_FF,
    OP_SUB,
    OP_SUB_II,
    OP_SUB_FF,
    OP_MUL,
    OP_MUL_II,
    OP_MUL_FF,
    OP_DIV,
    OP_DIV_II,
    OP_DIV_FF,
    OP_MOD,
    OP_MOD_II,
    OP_MOD_FF,
    OP_POW,
    OP_NEG, // *A = -(*B)
    OP_NEG_I,
    OP_NEG_F,
    OP_BITAND,
    OP_BITAND_I,
    OP_BITOR,
    OP_BITXOR,
    OP_BITNOT, // *A = ~(*B)
    OP_LSHIFT,
    OP_RSHIFT,
    OP_EQ,
    OP_EQ_II,
    OP_EQ_FF,
    OP_EQ_SS,
    OP_NEQ,
    OP_NEQ_II,
    OP_NEQ_FF,
    OP_NEQ_SS,
    OP_LT,
    OP_LT_II,
    OP_LT_FF,
    OP_LT_SS,
    OP_LTE,
    OP_LTE_II,
    OP_LTE_FF,
    OP_LTE_SS,
    OP_NOT, // *A = !(*B)
    CONCAT, // A: dst, B: first reg, C: count  — concat C registers starting at B

    LIST_NEW,    // dst, init capacity hint, -
    LIST_APPEND, // list reg, val reg, -
    LIST_GET,    // dst, list reg, index reg
    LIST_SET,    // list reg, index reg, value reg
    LIST_LEN,    // dst, list reg

    JUMP,          // -, sBx : unconditional jump
    JUMP_IF_TRUE,  // cond reg, offset if truthy (DOESN'T pop)
    JUMP_IF_FALSE, // cond reg, offset if falsy
    LOOP,          // -, sBx: backward offset (neg = back)

    FOR_PREP, // base, (lim=*A + 1, step = *A + 2, idx = *A + 3), jump past block if done
    FOR_STEP, // base, jump back to top of block if not done

    CLOSURE,    // dst, function const index
                // followed by upvalue descriptors, one MOVE per upvalue
    CALL,       // func reg, argc, expected ret (0xFF=discard)
    CALL_TAIL,  // func reg, argc | tail call optimized
    RETURN,     // first result reg, result count (0=RETURN NIL)
    RETURN_NIL, // no operands, fast path insead of load and return

    IC_CALL, // func reg, argc, slot index

    // misc
    NOP,
    HALT,

    _COUNT
};

// inline uint8_t instr_op(uint32_t const i) { return (i >> 24) & 0xFF; }

inline OpCode instr_op(uint32_t const i) { return static_cast<OpCode>((i >> 24) & 0xFF); }

inline uint8_t instr_A(uint32_t const i) { return (i >> 16) & 0xFF; }

inline uint8_t instr_B(uint32_t const i) { return (i >> 8) & 0xFF; }

inline uint8_t instr_C(uint32_t const i) { return i & 0xFF; }

inline uint16_t instr_Bx(uint32_t const i) { return i & 0xFFFF; }

inline int16_t instr_sBx(uint32_t const i) { return static_cast<int16_t>(i & 0xFFFF) - 32767; }

inline uint32_t make_ABC(OpCode op, uint8_t A, uint8_t B, uint8_t C)
{
    return (static_cast<uint32_t>(op) << 24) | (static_cast<uint32_t>(A) << 16)
        | (static_cast<uint32_t>(B) << 8) | static_cast<uint32_t>(C);
}

inline uint32_t make_ABx(OpCode op, uint8_t A, uint16_t Bx)
{
    return (static_cast<uint32_t>(op) << 24) | (static_cast<uint32_t>(A) << 16) | (static_cast<uint32_t>(Bx));
}

inline uint32_t make_AsBx(OpCode op, uint8_t A, int64_t sBx)
{
    uint16_t _sBx = static_cast<uint16_t>(sBx + JUMP_OFFSET);
    return (static_cast<uint32_t>(op) << 24) | (static_cast<uint32_t>(A) << 16) | (static_cast<uint32_t>(_sBx));
}

enum class InstrFormat : uint8_t {
    ABC,
    ABx,
    AsBx,
    A,
    NONE
};

struct ICSlot {
    uint8_t seenLhs { 0 };
    uint8_t seenRhs { 0 };
    uint8_t seenRet { 0 };
    uint32_t hitCount { 0 };
    void* jitStub { nullptr };
};

struct LineEntry {
    uint32_t start;
    uint32_t line;
};

static StringRef opcode_name(OpCode op)
{
    switch (op) {
    case OpCode::LOAD_NIL:
        return "LOAD_NIL";
    case OpCode::LOAD_TRUE:
        return "LOAD_TRUE";
    case OpCode::LOAD_FALSE:
        return "LOAD_FALSE";
    case OpCode::LOAD_CONST:
        return "LOAD_CONST";
    case OpCode::LOAD_INT:
        return "LOAD_INT";
    case OpCode::LOAD_GLOBAL:
        return "LOAD_GLOBAL";
    case OpCode::STORE_GLOBAL:
        return "STORE_GLOBAL";
    case OpCode::MOVE:
        return "MOVE";
    case OpCode::GET_UPVALUE:
        return "GET_UPVALUE";
    case OpCode::SET_UPVALUE:
        return "SET_UPVALUE";
    case OpCode::CLOSE_UPVALUE:
        return "CLOSE_UPVALUE";
    case OpCode::OP_ADD:
        return "OP_ADD";
    case OpCode::OP_SUB:
        return "OP_SUB";
    case OpCode::OP_MUL:
        return "OP_MUL";
    case OpCode::OP_DIV:
        return "OP_DIV";
    case OpCode::OP_MOD:
        return "OP_MOD";
    case OpCode::OP_POW:
        return "OP_POW";
    case OpCode::OP_NEG:
        return "OP_NEG";
    case OpCode::OP_BITAND:
        return "OP_BITAND";
    case OpCode::OP_BITOR:
        return "OP_BITOR";
    case OpCode::OP_BITXOR:
        return "BITXOR";
    case OpCode::OP_BITNOT:
        return "BITNOT";
    case OpCode::OP_LSHIFT:
        return "OP_LSHIFT";
    case OpCode::OP_RSHIFT:
        return "OP_RSHIFT";
    case OpCode::OP_EQ:
        return "OP_EQ";
    case OpCode::OP_NEQ:
        return "OP_NEQ";
    case OpCode::OP_LT:
        return "OP_LT";
    case OpCode::OP_LTE:
        return "OP_LE";
    case OpCode::OP_NOT:
        return "OP_NOT";
    case OpCode::CONCAT:
        return "CONCAT";
    case OpCode::LIST_NEW:
        return "LIST_NEW";
    case OpCode::LIST_APPEND:
        return "LIST_APPEND";
    case OpCode::LIST_GET:
        return "LIST_GET";
    case OpCode::LIST_SET:
        return "LIST_SET";
    case OpCode::LIST_LEN:
        return "LIST_LEN";
    case OpCode::JUMP:
        return "JUMP";
    case OpCode::JUMP_IF_TRUE:
        return "JUMP_IF_TRUE";
    case OpCode::JUMP_IF_FALSE:
        return "JUMP_IF_FALSE";
    case OpCode::LOOP:
        return "LOOP";
    case OpCode::FOR_PREP:
        return "FOR_PREP";
    case OpCode::FOR_STEP:
        return "FOR_STEP";
    case OpCode::CLOSURE:
        return "CLOSURE";
    case OpCode::CALL:
        return "CALL";
    case OpCode::CALL_TAIL:
        return "CALL_TAIL";
    case OpCode::RETURN:
        return "RETURN";
    case OpCode::RETURN_NIL:
        return "RETURN_NIL";
    case OpCode::IC_CALL:
        return "IC_CALL";
    case OpCode::NOP:
        return "NOP";
    case OpCode::HALT:
        return "HALT";
    default:
        return "???";
    }
}

static InstrFormat opcodeFormat(OpCode op)
{
    switch (op) {
    case OpCode::LOAD_CONST:
    case OpCode::LOAD_INT:
    case OpCode::LOAD_GLOBAL:
    case OpCode::STORE_GLOBAL:
    case OpCode::CLOSURE:
        return InstrFormat::ABx;
    case OpCode::JUMP:
    case OpCode::JUMP_IF_TRUE:
    case OpCode::JUMP_IF_FALSE:
    case OpCode::LOOP:
    case OpCode::FOR_PREP:
    case OpCode::FOR_STEP:
        return InstrFormat::AsBx;
    case OpCode::RETURN_NIL:
    case OpCode::HALT:
    case OpCode::NOP:
        return InstrFormat::NONE;
    default:
        return InstrFormat::ABC;
    }
}

static void print_value(uint64_t v);

struct Chunk {
    StringRef name { "" };
    int arity { 0 };
    unsigned int localCount { 0 };
    unsigned int upvalueCount { 0 };

    Array<uint32_t> code;
    Array<SourceLocation> locations;
    Array<uint64_t> constants;
    Array<LineEntry> lines;
    Array<Chunk*> functions;
    Array<ICSlot> icSlots;

    Chunk() = default;
    ~Chunk() = default;

    Chunk(Chunk const&) = delete;
    Chunk(Chunk&&) = default;

    Chunk& operator=(Chunk const&) = delete;
    Chunk& operator=(Chunk&&) = default;

    uint32_t emit(uint32_t instr, SourceLocation loc);

    bool patchJump(uint32_t const instr_idx);

    uint16_t addConstant(uint64_t const v);

    uint8_t allocIcSlot();

    uint32_t getLine(uint32_t const instr_idx) const;

    void disassemble() const;

private:
    void addLine(uint32_t line);
};

template<typename... Args>
static Chunk* makeChunk(Args&&... args)
{
    return getRuntimeAllocator().allocateObject<Chunk>(std::forward<Args>(args)...);
}

}

#endif // OPCODE_HPP
