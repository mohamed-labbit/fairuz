#ifndef FA_OPCODE_HPP
#define FA_OPCODE_HPP

#include "farray.hpp"
#include "fobject.hpp"
#include "fstring.hpp"

namespace fairuz::runtime {

static constexpr u16 JUMP_OFFSET = 32767;
static constexpr u8 REG_NONE = 0xFF;
static constexpr u16 MAX_CONSTANTS = 0xFFFF;
static constexpr u8 MAX_IC_SLOTS = 0xFF;
static constexpr u8 MAX_REGS = 250;

/*
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
        IMPORT_MODULE,     // dst, module-name constant index

        MOVE, // dst, src, -

        // * is the value inside the given reg, if you have to ask ,then don't touch
        // this code
        // *A = *left OP *right
        OP_ADD,
        OP_SUB,
        OP_MUL,
        OP_DIV,
        OP_MOD,
        OP_POW,
        OP_NEG, // *A = -(*B)
        OP_BITAND,
        OP_BITOR,
        OP_BITXOR,
        OP_BITNOT, // *A = ~(*B)
        OP_LSHIFT,
        OP_RSHIFT,
        OP_EQ,
        OP_NEQ,
        OP_LT,
        OP_LTE,
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
        CALL,       // func reg, argc, expected ret (0xFF=discard)
        CALL_TAIL,  // func reg, argc | tail call optimized
        RETURN,     // first result reg, result count (0=RETURN NIL)
        RETURN_NIL, // no operands, fast path insead of load and return
        RETURN1,

        IC_CALL, // func reg, argc, slot index

        INDEX_READ,
        INDEX_WRITE,

        NEW_CLASS,

        // instances
        NEW_INSTANCE, // A: new ObjInstance Bx: constant idx of ObjClass*
        INVOKE,       // A: instance reg B: vtable idx reg C: args
        GET_FIELD,    // A: result reg, B: instance reg, C: field reg
        SET_FIELD,    // A: instance reg, B: field reg, C: src reg

        // misc
        NOP,
        HALT,

        _COUNT
*/

#if FA_USE_NANBOX
#    define FA_OPCODE_BIG_INT() X(LOAD_BIG_INT)
#else
#    define FA_OPCODE_BIG_INT()
#endif

#define FA_OPCODE_LIST(X)  \
    X(LOAD_NIL)            \
    X(LOAD_TRUE)           \
    X(LOAD_FALSE)          \
    X(LOAD_CONST)          \
    X(LOAD_INT)            \
    FA_OPCODE_BIG_INT()    \
    X(LOAD_GLOBAL)         \
    X(STORE_GLOBAL)        \
    X(LOAD_GLOBAL_CACHED)  \
    X(STORE_GLOBAL_CACHED) \
    X(IMPORT_MODULE)       \
    X(MOVE)                \
    X(OP_ADD)              \
    X(OP_SUB)              \
    X(OP_MUL)              \
    X(OP_DIV)              \
    X(OP_MOD)              \
    X(OP_POW)              \
    X(OP_NEG)              \
    X(OP_BITAND)           \
    X(OP_BITOR)            \
    X(OP_BITXOR)           \
    X(OP_BITNOT)           \
    X(OP_LSHIFT)           \
    X(OP_RSHIFT)           \
    X(OP_LSHIFT_REG)       \
    X(OP_RSHIFT_REG)       \
    X(OP_EQ)               \
    X(OP_NEQ)              \
    X(OP_LT)               \
    X(OP_LTE)              \
    X(OP_GT)               \
    X(OP_GTE)              \
    X(OP_NOT)              \
    X(CONCAT)              \
    X(LIST_NEW)            \
    X(LIST_APPEND)         \
    X(LIST_GET)            \
    X(LIST_SET)            \
    X(LIST_LEN)            \
    X(JUMP)                \
    X(JUMP_IF_TRUE)        \
    X(JUMP_IF_FALSE)       \
    X(LOOP)                \
    X(FOR_PREP)            \
    X(FOR_STEP)            \
    X(CLOSURE)             \
    X(CALL)                \
    X(CALL_TAIL)           \
    X(RETURN)              \
    X(RETURN_NIL)          \
    X(RETURN1)             \
    X(IC_CALL)             \
    X(INDEX_READ)          \
    X(INDEX_WRITE)         \
    X(NEW_CLASS)           \
    X(NEW_INSTANCE)        \
    X(INVOKE)              \
    X(INVOKE_NAMED)        \
    X(GET_FIELD)           \
    X(SET_FIELD)           \
    X(NOP)                 \
    X(HALT)

enum class OpCode : u8 {
#define X(name) name,
    FA_OPCODE_LIST(X)
#undef X
        _COUNT
};

static inline StringRef opcode_name(OpCode op)
{
    switch (op) {
#define X(name) \
case OpCode::name: return #name;
        FA_OPCODE_LIST(X)
#undef X
    default: return "???";
    }
}

inline OpCode instr_op(u32 const i) { return static_cast<OpCode>((i >> 24) & 0xFF); }

inline u8 instr_A(u32 const i) { return (i >> 16) & 0xFF; }

inline u8 instr_B(u32 const i) { return (i >> 8) & 0xFF; }

inline u8 instr_C(u32 const i) { return i & 0xFF; }

inline u16 instr_Bx(u32 const i) { return i & 0xFFFF; }

inline i16 instr_sBx(u32 const i) { return static_cast<i16>(i & 0xFFFF) - 32767; }

inline u32 make_ABC(OpCode op, u8 A, u8 B, u8 C)
{
    return (static_cast<u32>(op) << 24) | (static_cast<u32>(A) << 16) | (static_cast<u32>(B) << 8) | static_cast<u32>(C);
}

inline u32 make_ABx(OpCode op, u8 A, u16 Bx)
{
    return (static_cast<u32>(op) << 24) | (static_cast<u32>(A) << 16) | (static_cast<u32>(Bx));
}

inline u32 make_AsBx(OpCode op, u8 A, i64 s_bx)
{
    u16 _s_bx = static_cast<u16>(s_bx + JUMP_OFFSET);
    return (static_cast<u32>(op) << 24) | (static_cast<u32>(A) << 16) | (static_cast<u32>(_s_bx));
}

inline u32 make_ABSC(OpCode op, u8 A, u8 B, int8_t s_c)
{
    return (static_cast<u32>(op) << 24) | (static_cast<u32>(A) << 16)
        | (static_cast<u32>(B) << 8) | static_cast<u32>(static_cast<u8>(s_c + 128));
}

inline int8_t instr_sC(u32 i) { return static_cast<int8_t>(i & 0xFF) - 128; }

enum class InstrFormat : u8 {
    ABC,
    ABx,
    AsBx,
    A,
    NONE
}; // enum InstrFormat

struct ICSlot {
    u8 seen_lhs { 0 };
    u8 seen_rhs { 0 };
    u8 seen_ret { 0 };
    u32 hit_count { 0 };
    void* jit_stub { nullptr };

    // global ic
    u64* global_ptr { nullptr };
    u64 version { 0 };
}; // struct ICSlot

struct LineEntry {
    u32 start { 0 };
    u32 line { 0 };
}; // struct LineEntry

static inline InstrFormat opcode_format(OpCode op)
{
    switch (op) {
    case OpCode::LOAD_CONST:
    case OpCode::LOAD_INT:
    case OpCode::LOAD_GLOBAL:
    case OpCode::STORE_GLOBAL:
    case OpCode::IMPORT_MODULE:
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

struct ClassDescriptor {
    StringRef name;
    StringRef parent_name;
    u32 field_count { 0 };
    Array<StringRef> field_names; // for runtime slot-map / debug info
    u32 vtable_size { 0 };
    Array<StringRef> method_names; // parallel to vtable_indices
    Array<u32> vtable_indices;     // indices into Chunk::functions[]
                                   // of the enclosing (top-level) chunk
    static constexpr u32 NULL_SLOT = UINT32_MAX;
};

class Value;

struct Chunk {
    StringRef name { "" };
    std::string source_path;
    diagnostic::SourcePtr source;
    GlobalEnvironment* globals { nullptr };
    int arity { 0 };
    u32 local_count { 0 };

    Array<u32> code;
    Array<SourceLocation> locations;
    Array<Value> constants;
#if FA_USE_NANBOX
    // Large literals are parsed once into normalized, non-GC limb constants.
    Array<integer::Data> big_ints;
#endif
    Array<LineEntry> lines;
    Array<Chunk*> functions;
    Array<ICSlot> ic_slots;
    Array<u64*> global_cache;
    Array<ClassDescriptor> class_descriptors; // new

    u16 add_class_descriptor(ClassDescriptor&& d)
    {
        class_descriptors.push(std::move(d));
        return static_cast<u16>(class_descriptors.size() - 1);
    }

    Chunk() = default;
    ~Chunk() = default;

    Chunk(Chunk const&) = delete;
    Chunk(Chunk&&) = default;

    Chunk& operator=(Chunk const&) = delete;
    Chunk& operator=(Chunk&&) = default;

    u32 emit(u32 instr, SourceLocation loc);
    bool patch_jump(u32 const instr_idx);
    u16 add_constant(Value const v);
#if FA_USE_NANBOX
    u16 add_big_int(integer::Data const& v);
    u16 add_big_int(i64 v) { return add_big_int(integer::from_i64(v)); }
#endif
    u8 alloc_ic_slot();
    u32 get_line(u32 const instr_idx) const;
    void disassemble() const;
    void add_line(u32 line);
}; // struct Chunk

template<typename... Args>
static inline Chunk* make_chunk(Args&&... args) { return get_allocator().allocate_object<Chunk>(std::forward<Args>(args)...); }

} // namespace fairuz::runtime

#endif // FA_OPCODE_HPP
