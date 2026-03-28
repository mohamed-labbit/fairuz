#ifndef OPCODE_HPP
#define OPCODE_HPP

#include "array.hpp"
#include "string.hpp"

namespace fairuz::runtime {

static constexpr u16 JUMP_OFFSET = 32767;
static constexpr u8 REG_NONE = 0xFF;
static constexpr u16 MAX_CONSTANTS = 0xFFFF;
static constexpr u8 MAX_REGS = 250;

enum class Fa_OpCode : u8 {
    /// [Op]: [A] , [B], [C]
    /// or
    /// [Op]: [A] , [  Bx  ]
    /// or
    /// [Op]: [A] , [  sBx ]

    LOAD_NIL,           // dst, Start, Count - fill [B..B+C) with nil
    LOAD_TRUE,          // dst, - , -
    LOAD_FALSE,         // dst, - , -
    LOAD_CONST,         // dst, Const pool index
    LOAD_INT,           // dst, signed 16-bit int (with bias, for larger ints use u64)
    LOAD_GLOBAL,        // dst, name const index
    STORE_GLOBAL,       // src, name const index
    LOAD_GLOBAL_CACHED, // A = dst, Bx = index into GlobalSlots_
    STORE_GLOBAL_CACHED,

    MOVE, // dst, src, -

    GET_UPVALUE,   // dst, index
    SET_UPVALUE,   // src, index
    CLOSE_UPVALUE, // first reg to close (close all the above), - , -

    // * is the value inside the given reg, if you have to ask ,then don't touch
    // this code
    // *A = *left OP *right
    OP_ADD,
    OP_ADD_II,
    OP_ADD_FF,
    OP_ADD_RI,
    OP_SUB,
    OP_SUB_II,
    OP_SUB_FF,
    OP_SUB_RI,
    OP_MUL,
    OP_MUL_II,
    OP_MUL_FF,
    OP_MUL_RI,
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
    OP_BITOR_I,
    OP_BITXOR,
    OP_BITXOR_I,
    OP_BITNOT, // *A = ~(*B)
    OP_LSHIFT,
    OP_RSHIFT,
    OP_EQ,
    OP_EQ_II,
    OP_EQ_FF,
    OP_EQ_SS,
    OP_EQ_RI,
    OP_NEQ,
    OP_NEQ_II,
    OP_NEQ_FF,
    OP_NEQ_SS,
    OP_LT,
    OP_LT_II,
    OP_LT_FF,
    OP_LT_SS,
    OP_LT_RI,
    OP_LTE,
    OP_LTE_II,
    OP_LTE_FF,
    OP_LTE_SS,
    OP_LTE_RI,
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

    FOR_PREP, // base, (lim=*A + 1, step = *A + 2, idx = *A + 3), jump past block
              // if done
    FOR_STEP, // base, jump back to top of block if not done

    CLOSURE,    // dst, function const index
                // followed by upvalue descriptors, one MOVE per upvalue
    CALL,       // func reg, argc, expected ret (0xFF=discard)
    CALL_TAIL,  // func reg, argc | tail call optimized
    RETURN,     // first result reg, result count (0=RETURN NIL)
    RETURN_NIL, // no operands, fast path insead of load and return
    RETURN1,

    IC_CALL, // func reg, argc, slot index

    // misc
    NOP,
    HALT,

    _COUNT
}; // enum Fa_OpCode

inline Fa_OpCode Fa_instr_op(u32 const i) { return static_cast<Fa_OpCode>((i >> 24) & 0xFF); }

inline u8 Fa_instr_A(u32 const i) { return (i >> 16) & 0xFF; }

inline u8 Fa_instr_B(u32 const i) { return (i >> 8) & 0xFF; }

inline u8 Fa_instr_C(u32 const i) { return i & 0xFF; }

inline u16 Fa_instr_Bx(u32 const i) { return i & 0xFFFF; }

inline i16 Fa_instr_sBx(u32 const i) { return static_cast<i16>(i & 0xFFFF) - 32767; }

inline u32 Fa_make_ABC(Fa_OpCode op, u8 A, u8 B, u8 C)
{
    return (static_cast<u32>(op) << 24) | (static_cast<u32>(A) << 16) | (static_cast<u32>(B) << 8) | static_cast<u32>(C);
}

inline u32 Fa_make_ABx(Fa_OpCode op, u8 A, u16 Bx)
{
    return (static_cast<u32>(op) << 24) | (static_cast<u32>(A) << 16) | (static_cast<u32>(Bx));
}

inline u32 Fa_make_AsBx(Fa_OpCode op, u8 A, i64 sBx)
{
    u16 _sBx = static_cast<u16>(sBx + JUMP_OFFSET);
    return (static_cast<u32>(op) << 24) | (static_cast<u32>(A) << 16) | (static_cast<u32>(_sBx));
}

inline u32 Fa_make_ABSC(Fa_OpCode op, u8 A, u8 B, int8_t sC)
{
    return (static_cast<u32>(op) << 24) | (static_cast<u32>(A) << 16)
        | (static_cast<u32>(B) << 8) | static_cast<u32>(static_cast<u8>(sC + 128));
}

inline int8_t Fa_instr_sC(u32 i) { return static_cast<int8_t>(i & 0xFF) - 128; }

enum class Fa_InstrFormat : u8 {
    ABC,
    ABx,
    AsBx,
    A,
    NONE
}; // enum Fa_InstrFormat

struct Fa_ICSlot {
    u8 seenLhs { 0 };
    u8 seenRhs { 0 };
    u8 seenRet { 0 };
    u32 hitCount { 0 };
    void* jitStub { nullptr };

    // global ic
    u64* globalPtr { nullptr };
    u64 version { 0 };
}; // struct Fa_ICSlot

struct Fa_LineEntry {
    u32 start;
    u32 line;
}; // struct LineEntry

static Fa_StringRef Fa_opcode_name(Fa_OpCode op)
{
    switch (op) {
    case Fa_OpCode::LOAD_NIL:
        return "LOAD_NIL";
    case Fa_OpCode::LOAD_TRUE:
        return "LOAD_TRUE";
    case Fa_OpCode::LOAD_FALSE:
        return "LOAD_FALSE";
    case Fa_OpCode::LOAD_CONST:
        return "LOAD_CONST";
    case Fa_OpCode::LOAD_INT:
        return "LOAD_INT";
    case Fa_OpCode::LOAD_GLOBAL:
        return "LOAD_GLOBAL";
    case Fa_OpCode::STORE_GLOBAL:
        return "STORE_GLOBAL";
    case Fa_OpCode::MOVE:
        return "MOVE";
    case Fa_OpCode::LOAD_GLOBAL_CACHED:
        return "LOAD_GLOBAL_CACHED";
    case Fa_OpCode::STORE_GLOBAL_CACHED:
        return "STORE_GLOBAL_CACHED";
    case Fa_OpCode::GET_UPVALUE:
        return "GET_UPVALUE";
    case Fa_OpCode::SET_UPVALUE:
        return "SET_UPVALUE";
    case Fa_OpCode::CLOSE_UPVALUE:
        return "CLOSE_UPVALUE";
    case Fa_OpCode::OP_ADD:
        return "OP_ADD";
    case Fa_OpCode::OP_ADD_II:
        return "OP_ADD_II";
    case Fa_OpCode::OP_ADD_FF:
        return "OP_ADD_FF";
    case Fa_OpCode::OP_ADD_RI:
        return "OP_ADD_RI";
    case Fa_OpCode::OP_SUB:
        return "OP_SUB";
    case Fa_OpCode::OP_SUB_II:
        return "OP_SUB_II";
    case Fa_OpCode::OP_SUB_FF:
        return "OP_SUB_FF";
    case Fa_OpCode::OP_SUB_RI:
        return "OP_SUB_RI";
    case Fa_OpCode::OP_MUL:
        return "OP_MUL";
    case Fa_OpCode::OP_MUL_II:
        return "OP_MUL_II";
    case Fa_OpCode::OP_MUL_FF:
        return "OP_MUL_FF";
    case Fa_OpCode::OP_MUL_RI:
        return "OP_MUL_RI";
    case Fa_OpCode::OP_DIV:
        return "OP_DIV";
    case Fa_OpCode::OP_DIV_II:
        return "OP_DIV_II";
    case Fa_OpCode::OP_DIV_FF:
        return "OP_DIV_FF";
    case Fa_OpCode::OP_MOD:
        return "OP_MOD";
    case Fa_OpCode::OP_MOD_II:
        return "OP_MOD_II";
    case Fa_OpCode::OP_MOD_FF:
        return "OP_MOD_FF";
    case Fa_OpCode::OP_POW:
        return "OP_POW";
    case Fa_OpCode::OP_NEG:
        return "OP_NEG";
    case Fa_OpCode::OP_NEG_I:
        return "OP_NEG_I";
    case Fa_OpCode::OP_NEG_F:
        return "OP_NEG_F";
    case Fa_OpCode::OP_BITAND:
        return "OP_BITAND";
    case Fa_OpCode::OP_BITAND_I:
        return "OP_BITAND_I";
    case Fa_OpCode::OP_BITOR:
        return "OP_BITOR";
    case Fa_OpCode::OP_BITOR_I:
        return "OP_BITOR_I";
    case Fa_OpCode::OP_BITXOR:
        return "OP_BITXOR";
    case Fa_OpCode::OP_BITXOR_I:
        return "OP_BITXOR_I";
    case Fa_OpCode::OP_BITNOT:
        return "OP_BITNOT";
    case Fa_OpCode::OP_LSHIFT:
        return "OP_LSHIFT";
    case Fa_OpCode::OP_RSHIFT:
        return "OP_RSHIFT";
    case Fa_OpCode::OP_EQ:
        return "OP_EQ";
    case Fa_OpCode::OP_EQ_II:
        return "OP_EQ_II";
    case Fa_OpCode::OP_EQ_FF:
        return "OP_EQ_FF";
    case Fa_OpCode::OP_EQ_SS:
        return "OP_EQ_SS";
    case Fa_OpCode::OP_EQ_RI:
        return "OP_EQ_RI";
    case Fa_OpCode::OP_NEQ:
        return "OP_NEQ";
    case Fa_OpCode::OP_NEQ_II:
        return "OP_NEQ_II";
    case Fa_OpCode::OP_NEQ_FF:
        return "OP_NEQ_FF";
    case Fa_OpCode::OP_NEQ_SS:
        return "OP_NEQ_SS";
    case Fa_OpCode::OP_LT:
        return "OP_LT";
    case Fa_OpCode::OP_LT_II:
        return "OP_LT_II";
    case Fa_OpCode::OP_LT_FF:
        return "OP_LT_FF";
    case Fa_OpCode::OP_LT_SS:
        return "OP_LT_SS";
    case Fa_OpCode::OP_LT_RI:
        return "OP_LT_RI";
    case Fa_OpCode::OP_LTE:
        return "OP_LTE";
    case Fa_OpCode::OP_LTE_II:
        return "OP_LTE_II";
    case Fa_OpCode::OP_LTE_FF:
        return "OP_LTE_FF";
    case Fa_OpCode::OP_LTE_SS:
        return "OP_LTE_SS";
    case Fa_OpCode::OP_LTE_RI:
        return "OP_LTE_RI";
    case Fa_OpCode::OP_NOT:
        return "OP_NOT";
    case Fa_OpCode::CONCAT:
        return "CONCAT";
    case Fa_OpCode::LIST_NEW:
        return "LIST_NEW";
    case Fa_OpCode::LIST_APPEND:
        return "LIST_APPEND";
    case Fa_OpCode::LIST_GET:
        return "LIST_GET";
    case Fa_OpCode::LIST_SET:
        return "LIST_SET";
    case Fa_OpCode::LIST_LEN:
        return "LIST_LEN";
    case Fa_OpCode::JUMP:
        return "JUMP";
    case Fa_OpCode::JUMP_IF_TRUE:
        return "JUMP_IF_TRUE";
    case Fa_OpCode::JUMP_IF_FALSE:
        return "JUMP_IF_FALSE";
    case Fa_OpCode::LOOP:
        return "LOOP";
    case Fa_OpCode::FOR_PREP:
        return "FOR_PREP";
    case Fa_OpCode::FOR_STEP:
        return "FOR_STEP";
    case Fa_OpCode::CLOSURE:
        return "CLOSURE";
    case Fa_OpCode::CALL:
        return "CALL";
    case Fa_OpCode::CALL_TAIL:
        return "CALL_TAIL";
    case Fa_OpCode::RETURN:
        return "RETURN";
    case Fa_OpCode::RETURN_NIL:
        return "RETURN_NIL";
    case Fa_OpCode::RETURN1:
        return "RETURN1";
    case Fa_OpCode::IC_CALL:
        return "IC_CALL";
    case Fa_OpCode::NOP:
        return "NOP";
    case Fa_OpCode::HALT:
        return "HALT";
    default:
        return "???";
    }
}

static Fa_InstrFormat opcodeFormat(Fa_OpCode op)
{
    switch (op) {
    case Fa_OpCode::LOAD_CONST:
    case Fa_OpCode::LOAD_INT:
    case Fa_OpCode::LOAD_GLOBAL:
    case Fa_OpCode::STORE_GLOBAL:
    case Fa_OpCode::CLOSURE:
        return Fa_InstrFormat::ABx;
    case Fa_OpCode::JUMP:
    case Fa_OpCode::JUMP_IF_TRUE:
    case Fa_OpCode::JUMP_IF_FALSE:
    case Fa_OpCode::LOOP:
    case Fa_OpCode::FOR_PREP:
    case Fa_OpCode::FOR_STEP:
        return Fa_InstrFormat::AsBx;
    case Fa_OpCode::RETURN_NIL:
    case Fa_OpCode::HALT:
    case Fa_OpCode::NOP:
        return Fa_InstrFormat::NONE;
    default:
        return Fa_InstrFormat::ABC;
    }
}

static void print_value(u64 v);

struct Fa_Chunk {
    Fa_StringRef name { "" };
    int arity { 0 };
    unsigned int localCount { 0 };
    unsigned int upvalueCount { 0 };

    Fa_Array<u32> code;
    Fa_Array<Fa_SourceLocation> locations;
    Fa_Array<u64> constants; // constants are boxed values!!!
    Fa_Array<Fa_LineEntry> lines;
    Fa_Array<Fa_Chunk*> functions;
    Fa_Array<Fa_ICSlot> icSlots;
    Fa_Array<u64*> globalCache;

    Fa_Chunk() = default;
    ~Fa_Chunk() = default;

    Fa_Chunk(Fa_Chunk const&) = delete;
    Fa_Chunk(Fa_Chunk&&) = default;

    Fa_Chunk& operator=(Fa_Chunk const&) = delete;
    Fa_Chunk& operator=(Fa_Chunk&&) = default;

    u32 emit(u32 instr, Fa_SourceLocation loc);
    bool patchJump(u32 const instr_idx);
    u16 addConstant(u64 const v);
    u8 allocIcSlot();
    u32 getLine(u32 const instr_idx) const;
    void disassemble() const;

private:
    void addLine(u32 line);
}; // struct Fa_Chunk

template<typename... Args>
static Fa_Chunk* makeChunk(Args&&... args) { return getAllocator().allocateObject<Fa_Chunk>(std::forward<Args>(args)...); }

} // namespace fairuz::runtime

#endif // OPCODE_HPP
