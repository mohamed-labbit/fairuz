//
// vm.cc
//

#include "vm.hpp"
#include "util.hpp"

#define Fa_DISPATCH()                                              \
    do {                                                           \
        instr = cur_chunk->code[ip];                               \
        ip += 1;                                                   \
        SAVE_IP();                                                 \
        goto* dispatch_table[static_cast<u8>(Fa_instr_op(instr))]; \
    } while (0)

#define Fa_BEGIN_DISPATCH() Fa_DISPATCH()
#define Fa_END_DISPATCH()
#define Fa_CASE(op) H_##op:

#define Fa_TABLE_INIT            \
    &&H_LOAD_NIL,                \
        &&H_LOAD_TRUE,           \
        &&H_LOAD_FALSE,          \
        &&H_LOAD_CONST,          \
        &&H_LOAD_INT,            \
        &&H_LOAD_GLOBAL,         \
        &&H_STORE_GLOBAL,        \
        &&H_LOAD_GLOBAL_CACHED,  \
        &&H_STORE_GLOBAL_CACHED, \
        &&H_MOVE,                \
        &&H_OP_ADD,              \
        &&H_OP_ADD_II,           \
        &&H_OP_ADD_FF,           \
        &&H_OP_ADD_RI,           \
        &&H_OP_SUB,              \
        &&H_OP_SUB_II,           \
        &&H_OP_SUB_FF,           \
        &&H_OP_SUB_RI,           \
        &&H_OP_MUL,              \
        &&H_OP_MUL_II,           \
        &&H_OP_MUL_FF,           \
        &&H_OP_MUL_RI,           \
        &&H_OP_DIV,              \
        &&H_OP_DIV_II,           \
        &&H_OP_DIV_FF,           \
        &&H_OP_MOD,              \
        &&H_OP_MOD_II,           \
        &&H_OP_MOD_FF,           \
        &&H_OP_POW,              \
        &&H_OP_NEG,              \
        &&H_OP_NEG_I,            \
        &&H_OP_NEG_F,            \
        &&H_OP_BITAND,           \
        &&H_OP_BITAND_I,         \
        &&H_OP_BITOR,            \
        &&H_OP_BITOR_I,          \
        &&H_OP_BITXOR,           \
        &&H_OP_BITXOR_I,         \
        &&H_OP_BITNOT,           \
        &&H_OP_LSHIFT,           \
        &&H_OP_RSHIFT,           \
        &&H_OP_EQ,               \
        &&H_OP_EQ_II,            \
        &&H_OP_EQ_FF,            \
        &&H_OP_EQ_SS,            \
        &&H_OP_EQ_RI,            \
        &&H_OP_NEQ,              \
        &&H_OP_NEQ_II,           \
        &&H_OP_NEQ_FF,           \
        &&H_OP_NEQ_SS,           \
        &&H_OP_LT,               \
        &&H_OP_LT_II,            \
        &&H_OP_LT_FF,            \
        &&H_OP_LT_SS,            \
        &&H_OP_LT_RI,            \
        &&H_OP_LTE,              \
        &&H_OP_LTE_II,           \
        &&H_OP_LTE_FF,           \
        &&H_OP_LTE_SS,           \
        &&H_OP_LTE_RI,           \
        &&H_OP_NOT,              \
        &&H_CONCAT,              \
        &&H_LIST_NEW,            \
        &&H_LIST_APPEND,         \
        &&H_LIST_GET,            \
        &&H_LIST_SET,            \
        &&H_LIST_LEN,            \
        &&H_JUMP,                \
        &&H_JUMP_IF_TRUE,        \
        &&H_JUMP_IF_FALSE,       \
        &&H_LOOP,                \
        &&H_FOR_PREP,            \
        &&H_FOR_STEP,            \
        &&H_CLOSURE,             \
        &&H_CALL,                \
        &&H_CALL_TAIL,           \
        &&H_RETURN,              \
        &&H_RETURN_NIL,          \
        &&H_RETURN1,             \
        &&H_IC_CALL,             \
        &&H_INDEX,               \
        &&H_NOP,                 \
        &&H_HALT

#define Fa_VMOPI(lhs, rhs, op) Fa_AS_INTEGER(lhs) op Fa_AS_INTEGER(rhs)
#define Fa_VMOPF(lhs, rhs, op) Fa_AS_DOUBLE(lhs) op Fa_AS_DOUBLE(rhs)

#define Fa_VM_ADDI(lhs, rhs) Fa_VMOPI(lhs, rhs, +)
#define Fa_VM_SUBI(lhs, rhs) Fa_VMOPI(lhs, rhs, -)
#define Fa_VM_MULI(lhs, rhs) Fa_VMOPI(lhs, rhs, *)
#define Fa_VM_DIVI(lhs, rhs) Fa_VMOPI(lhs, rhs, /)
#define Fa_VM_ADDF(lhs, rhs) Fa_VMOPF(lhs, rhs, +)
#define Fa_VM_SUBF(lhs, rhs) Fa_VMOPF(lhs, rhs, -)
#define Fa_VM_MULF(lhs, rhs) Fa_VMOPF(lhs, rhs, *)
#define Fa_VM_DIVF(lhs, rhs) Fa_VMOPF(lhs, rhs, /)

#define Fa_VM_ABC(op) cur_chunk->code[ip - 1] = Fa_make_ABC(op, a, b, c)

#define Fa_RA() cur_base[Fa_instr_A(instr)]
#define Fa_RB() cur_base[Fa_instr_B(instr)]
#define Fa_RC() cur_base[Fa_instr_C(instr)]

#define LOAD_FRAME()                 \
    do {                             \
        Fa_CallFrame& f = frame();   \
        cur_chunk = f.chunk;         \
        cur_frame_base = f.base;     \
        cur_base = &m_stack[f.base]; \
        ip = f.ip;                   \
    } while (0)

#define SAVE_IP()        \
    do {                 \
        frame().ip = ip; \
    } while (0)

#define Fa_RECORD_BINARY_IC(lhs, rhs, result)                      \
    do {                                                           \
        if (ip < cur_chunk->code.size()                            \
            && Fa_instr_op(cur_chunk->code[ip]) == Fa_OpCode::NOP) \
            update_ic_binary(cur_chunk, ip, lhs, rhs, result);     \
    } while (0)

#define REQUIRE_NUMBER(v)                               \
    do {                                                \
        if (!Fa_IS_NUMBER(v))                           \
            runtime_error(ErrorCode::TYPE_ERROR_ARITH); \
    } while (0)

namespace fairuz::runtime {

static bool values_equal(Fa_Value lhs, Fa_Value rhs)
{
    if (lhs == rhs)
        return true;

    if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs))
        return Fa_AS_INTEGER(lhs) == Fa_AS_INTEGER(rhs);
    if (Fa_IS_BOOL(lhs) && Fa_IS_BOOL(rhs))
        return Fa_AS_BOOL(lhs) == Fa_AS_BOOL(rhs);
    if (Fa_IS_NIL(lhs) && Fa_IS_NIL(rhs))
        return true;
    if (Fa_IS_NUMBER(lhs) && Fa_IS_NUMBER(rhs))
        return Fa_AS_DOUBLE_ANY(lhs) == Fa_AS_DOUBLE_ANY(rhs);
    if (Fa_IS_STRING(lhs) && Fa_IS_STRING(rhs))
        return Fa_AS_STRING(lhs)->str == Fa_AS_STRING(rhs)->str;

    return false;
}

static void check_stack_index(int index, int stack_size, char const* m_context)
{
    if (index < 0 || index >= stack_size)
        // This is a Fa_VM internal error, not a user error.
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR,
            std::string(" : ") + std::string("Fa_VM internal error: stack index ") + std::to_string(index)
                + " out of range [0," + std::to_string(stack_size) + ") in " + m_context,
            diagnostic::Severity::FATAL);
}

Fa_VM::Fa_VM()
{
    std::fill(m_stack, m_stack + STACK_SIZE, NIL_VAL);
    std::fill(m_frames, m_frames + MAX_FRAMES, Fa_CallFrame());

    open_stdlib();
}

Fa_VM::~Fa_VM()
{
    m_gc.sweep_all();
}

#define PUSH_VALUE(v)                                 \
    do {                                              \
        if (m_stack_top == STACK_SIZE)                \
            runtime_error(ErrorCode::STACK_OVERFLOW); \
        m_stack[m_stack_top] = v;                     \
        m_stack_top += 1;                             \
    } while (0);

// Ensure the value stack has at least `needed` slots allocated, filling
// any new slots with NIL.  Checks against the hard cap.
void Fa_VM::ensure_stack_slots(int needed)
{
    if (needed > STACK_SIZE)
        runtime_error(ErrorCode::STACK_OVERFLOW);

    while (m_stack_top < needed)
        PUSH_VALUE(NIL_VAL);
}

// Reference to the topmost call frame.
Fa_CallFrame& Fa_VM::top_frame()
{
    assert(m_frames_top > 0 && "topFrame called with empty frame stack");
    return m_frames[m_frames_top - 1];
}

Fa_CallFrame const& Fa_VM::top_frame() const
{
    assert(m_frames_top > 0 && "topFrame called with empty frame stack");
    return m_frames[m_frames_top - 1];
}

Fa_Value& Fa_VM::get_reg(Fa_CallFrame const& f, int reg)
{
    int abs = f.base + reg;
    check_stack_index(abs, STACK_SIZE, "getReg");
    return m_stack[abs];
}

Fa_Value Fa_VM::run(Fa_Chunk* chunk)
{
    if (chunk == nullptr)
        return NIL_VAL;

    m_stack_top = 0;
    m_frames_top = 0;

    Fa_ObjFunction* fn = Fa_MAKE_OBJ_FUNCTION();
    fn->chunk = chunk;
    Fa_ObjClosure* cl = Fa_MAKE_OBJ_CLOSURE(fn);

    m_stack[0] = Fa_MAKE_OBJECT(cl);
    m_stack_top = static_cast<int>(chunk->local_count) + 2;

    if (m_frames_top >= MAX_FRAMES || m_stack_top >= STACK_SIZE)
        runtime_error(ErrorCode::STACK_OVERFLOW);

    m_frames[m_frames_top] = Fa_CallFrame(cl, chunk, 0, 1, static_cast<int>(chunk->local_count));
    m_frames_top += 1;
    intern_chunk_constants(cl->function->chunk);

    if (m_gc.current_memory() >= GC_THRESHOLD)
        m_gc.collect(this);

    return execute();
}

Fa_Value Fa_VM::execute()
{
    static void const* dispatch_table[] = { Fa_TABLE_INIT };

    if (m_frames_top == 0)
        return NIL_VAL;

    u32 instr;
    Fa_Chunk* cur_chunk = frame().chunk;
    int cur_frame_base = frame().base;
    Fa_Value* cur_base = &m_stack[cur_frame_base];
    u32 ip = frame().ip;

    Fa_BEGIN_DISPATCH();

    Fa_CASE(LOAD_NIL)
    {
        u8 start = Fa_instr_B(instr);
        u8 count = Fa_instr_C(instr);

        for (u8 i = 0; i < count; i += 1)
            cur_base[start + i] = NIL_VAL;

        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_TRUE)
    {
        Fa_RA() = Fa_MAKE_BOOL(true);
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_FALSE)
    {
        Fa_RA() = Fa_MAKE_BOOL(false);
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_CONST)
    {
        Fa_RA() = cur_chunk->constants[Fa_instr_Bx(instr)];
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_INT)
    {
        Fa_RA() = Fa_MAKE_INTEGER(static_cast<i32>(static_cast<u16>(Fa_instr_Bx(instr))) - JUMP_OFFSET);
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_GLOBAL)
    {
        {
            Fa_Value name_v = cur_chunk->constants[Fa_instr_Bx(instr)];
            Fa_StringRef name = Fa_AS_STRING(name_v)->str;
            u32* slot = m_global_index.find_ptr(name);
            if (slot == nullptr)
                runtime_error(ErrorCode::UNDEFINED_GLOBAL, std::string(name.data(), name.len()));

            u32 slot_idx = *slot;
            Fa_RA() = m_global_slots[slot_idx];
        }
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_GLOBAL_CACHED)
    {
        u32 slot_idx = Fa_instr_Bx(instr);
        if (UNLIKELY(slot_idx >= m_global_slots.size()))
            runtime_error(ErrorCode::UNDEFINED_GLOBAL);

        Fa_RA() = m_global_slots[slot_idx];
        Fa_DISPATCH();
    }
    Fa_CASE(STORE_GLOBAL)
    {
        {
            Fa_Value name_v = cur_chunk->constants[Fa_instr_Bx(instr)];
            Fa_StringRef name = Fa_AS_STRING(name_v)->str;
            u32 slot_idx;

            if (u32* slot = m_global_index.find_ptr(name)) {
                slot_idx = *slot;
            } else {
                slot_idx = static_cast<u32>(m_global_slots.size());
                m_global_slots.push(NIL_VAL);
                m_global_index.insert_or_assign(name, slot_idx);
            }

            m_global_slots[slot_idx] = Fa_RA();
        }
        Fa_DISPATCH();
    }
    Fa_CASE(STORE_GLOBAL_CACHED)
    {
        u32 slot_idx = Fa_instr_Bx(instr);
        if (UNLIKELY(slot_idx >= m_global_slots.size()))
            runtime_error(ErrorCode::UNDEFINED_GLOBAL);

        m_global_slots[slot_idx] = Fa_RA();
        Fa_DISPATCH();
    }
    Fa_CASE(MOVE)
    {
        Fa_RA() = Fa_RB();
        Fa_DISPATCH();
    }
    Fa_CASE(OP_ADD)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];
        bool both_str = Fa_IS_STRING(lhs) && Fa_IS_STRING(rhs);

        if (both_str) {
            // String concatenation
            cur_base[a] = Fa_MAKE_STRING(Fa_AS_STRING(lhs)->str + Fa_AS_STRING(rhs)->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            // Fa_VM_ABC(Fa_OpCode::OP_ADD_SS);
        } else if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            // Integer addition
            cur_base[a] = Fa_MAKE_INTEGER(Fa_VM_ADDI(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_ADD_II);
        } else if (Fa_IS_NUMBER(lhs) && Fa_IS_NUMBER(rhs)) {
            // Float addition (includes mixed int/float)
            cur_base[a] = Fa_MAKE_REAL(Fa_VM_ADDF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_ADD_FF);
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_ADD_II)
    {
        Fa_RA() = Fa_MAKE_INTEGER(Fa_VM_ADDI(Fa_RB(), Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_ADD_FF)
    {
        Fa_RA() = Fa_MAKE_REAL(Fa_VM_ADDF(Fa_RB(), Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_ADD_RI)
    {
        Fa_Value lhs = Fa_RB();
        auto imm = static_cast<int8_t>(Fa_instr_C(instr) - 128);
        Fa_RA() = LIKELY(Fa_IS_INTEGER(lhs)) ? Fa_MAKE_INTEGER(Fa_AS_INTEGER(lhs) + imm) : Fa_MAKE_REAL(Fa_AS_DOUBLE(lhs) + imm);
        Fa_DISPATCH();
    }
    Fa_CASE(OP_SUB)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (UNLIKELY(!Fa_IS_NUMBER(lhs) || !Fa_IS_NUMBER(rhs)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            cur_base[a] = Fa_MAKE_INTEGER(Fa_VM_SUBI(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_SUB_II);
        } else {
            cur_base[a] = Fa_MAKE_REAL(Fa_VM_SUBF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_SUB_FF);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_SUB_II)
    {
        Fa_RA() = Fa_MAKE_INTEGER(Fa_VM_SUBI(Fa_RB(), Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_SUB_FF)
    {
        Fa_RA() = Fa_MAKE_REAL(Fa_VM_SUBF(Fa_RB(), Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_SUB_RI)
    {
        Fa_Value lhs = Fa_RB();
        auto imm = static_cast<int8_t>(Fa_instr_C(instr) - 128);
        Fa_RA() = LIKELY(Fa_IS_INTEGER(lhs)) ? Fa_MAKE_INTEGER(Fa_AS_INTEGER(lhs) - imm) : Fa_MAKE_REAL(Fa_AS_DOUBLE(lhs) - imm);
        Fa_DISPATCH();
    }
    Fa_CASE(OP_MUL)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (UNLIKELY(!Fa_IS_NUMBER(lhs) || !Fa_IS_NUMBER(rhs)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            cur_base[a] = Fa_MAKE_INTEGER(Fa_VM_MULI(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_MUL_II);
        } else {
            cur_base[a] = Fa_MAKE_REAL(Fa_VM_MULF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_MUL_FF);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_MUL_II)
    {
        Fa_RA() = Fa_MAKE_INTEGER(Fa_VM_MULI(Fa_RB(), Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_MUL_FF)
    {
        Fa_RA() = Fa_MAKE_REAL(Fa_VMOPF(Fa_RB(), Fa_RC(), *));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_MUL_RI)
    {
        Fa_Value lhs = Fa_RB();
        auto imm = static_cast<int8_t>(Fa_instr_C(instr) - 128);
        Fa_RA() = LIKELY(Fa_IS_INTEGER(lhs)) ? Fa_MAKE_INTEGER(Fa_AS_INTEGER(lhs) * imm) : Fa_MAKE_REAL(Fa_AS_DOUBLE(lhs) * imm);
        Fa_DISPATCH();
    }
    Fa_CASE(OP_DIV)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (UNLIKELY(!Fa_IS_NUMBER(lhs) || !Fa_IS_NUMBER(rhs)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        if (Fa_AS_DOUBLE_ANY(rhs) == 0.0)
            runtime_error(ErrorCode::DIVISION_BY_ZERO);

        if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            cur_base[a] = Fa_MAKE_INTEGER(Fa_VM_DIVI(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_DIV_II);
        } else {
            cur_base[a] = Fa_MAKE_REAL(Fa_VM_DIVF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_DIV_FF);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_DIV_II)
    {
        Fa_RA() = Fa_MAKE_INTEGER(Fa_VM_DIVI(Fa_RB(), Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_DIV_FF)
    {
        Fa_RA() = Fa_MAKE_REAL(Fa_VM_DIVF(Fa_RB(), Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_MOD)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (UNLIKELY(!Fa_IS_NUMBER(lhs) || !Fa_IS_NUMBER(rhs)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        if (Fa_AS_DOUBLE_ANY(rhs) == 0.0)
            runtime_error(ErrorCode::MODULO_BY_ZERO);

        if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            cur_base[a] = Fa_MAKE_REAL(static_cast<f64>(Fa_VMOPI(lhs, rhs, %)));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_MOD_II);
        } else {
            cur_base[a] = Fa_MAKE_REAL(std::fmod(Fa_AS_DOUBLE_ANY(lhs), Fa_AS_DOUBLE_ANY(rhs)));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_MOD_FF);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_MOD_II)
    {
        Fa_RA() = Fa_MAKE_REAL(static_cast<f64>(Fa_VMOPI(Fa_RB(), Fa_RC(), %)));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_MOD_FF)
    {
        Fa_RA() = Fa_MAKE_REAL(std::fmod(Fa_AS_DOUBLE_ANY(Fa_RB()), Fa_AS_DOUBLE_ANY(Fa_RC())));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_POW)
    {
        Fa_Value lhs = Fa_RB(), rhs = Fa_RC();
        REQUIRE_NUMBER(lhs);
        REQUIRE_NUMBER(rhs);
        Fa_RA() = Fa_MAKE_REAL(std::pow(Fa_AS_DOUBLE_ANY(lhs), Fa_AS_DOUBLE_ANY(rhs)));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_NEG)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value operand = cur_base[b];

        if (UNLIKELY(!Fa_IS_NUMBER(operand)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        if (Fa_IS_INTEGER(operand)) {
            cur_base[a] = Fa_MAKE_INTEGER(-Fa_AS_INTEGER(operand));
            Fa_VM_ABC(Fa_OpCode::OP_NEG_I);
        } else {
            cur_base[a] = Fa_MAKE_REAL(-Fa_AS_DOUBLE_ANY(operand));
            Fa_VM_ABC(Fa_OpCode::OP_NEG_F);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_NEG_I)
    {
        Fa_RA() = Fa_MAKE_INTEGER(-Fa_AS_INTEGER(Fa_RB()));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_NEG_F)
    {
        Fa_RA() = Fa_MAKE_REAL(-Fa_AS_DOUBLE_ANY(Fa_RB()));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITAND)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (UNLIKELY(!Fa_IS_INTEGER(lhs) || !Fa_IS_INTEGER(rhs)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        cur_base[a] = Fa_MAKE_INTEGER(Fa_VMOPI(lhs, rhs, &));
        Fa_VM_ABC(Fa_OpCode::OP_BITAND_I);
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITAND_I)
    {
        Fa_RA() = Fa_MAKE_INTEGER(Fa_VMOPI(Fa_RB(), Fa_RC(), &));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITOR)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (UNLIKELY(!Fa_IS_INTEGER(lhs) || !Fa_IS_INTEGER(rhs)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        cur_base[a] = Fa_MAKE_INTEGER(Fa_VMOPI(lhs, rhs, |));
        Fa_VM_ABC(Fa_OpCode::OP_BITOR_I);
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITOR_I)
    {
        Fa_RA() = Fa_MAKE_INTEGER(Fa_VMOPI(Fa_RB(), Fa_RC(), |));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITXOR)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (UNLIKELY(!Fa_IS_INTEGER(lhs) || !Fa_IS_INTEGER(rhs)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        cur_base[a] = Fa_MAKE_INTEGER(Fa_VMOPI(lhs, rhs, ^));
        Fa_VM_ABC(Fa_OpCode::OP_BITXOR_I);
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITXOR_I)
    {
        Fa_RA() = Fa_MAKE_INTEGER(Fa_VMOPI(Fa_RB(), Fa_RC(), ^));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITNOT)
    {
        Fa_Value operand = Fa_RB();
        if (UNLIKELY(!Fa_IS_INTEGER(operand)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        Fa_RA() = Fa_MAKE_INTEGER(~Fa_AS_INTEGER(operand));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LSHIFT)
    {
        Fa_Value lhs = Fa_RB();
        if (UNLIKELY(!Fa_IS_INTEGER(lhs)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        u8 imm = Fa_instr_C(instr);
        if (imm > 64)
            runtime_error(ErrorCode::INDEX_TYPE_ERROR);

        Fa_RA() = Fa_MAKE_INTEGER(static_cast<i64>(static_cast<u64>(Fa_AS_INTEGER(lhs)) << imm));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_RSHIFT)
    {
        Fa_Value lhs = Fa_RB();
        if (UNLIKELY(!Fa_IS_INTEGER(lhs)))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        u8 imm = Fa_instr_C(instr);
        if (imm > 64)
            runtime_error(ErrorCode::INDEX_TYPE_ERROR);

        auto shifted = static_cast<u64>(Fa_AS_INTEGER(lhs)) >> imm;

        if (Fa_AS_INTEGER(lhs) < 0)
            Fa_RA() = Fa_MAKE_REAL(static_cast<f64>(shifted));
        else
            Fa_RA() = Fa_MAKE_INTEGER(static_cast<i64>(shifted));

        Fa_DISPATCH();
    }
    Fa_CASE(OP_EQ)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            cur_base[a] = Fa_MAKE_BOOL(Fa_VMOPI(lhs, rhs, ==));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_EQ_II);
        } else if (Fa_IS_STRING(lhs) && Fa_IS_STRING(rhs)) {
            cur_base[a] = Fa_MAKE_BOOL(Fa_AS_STRING(lhs)->str == Fa_AS_STRING(rhs)->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_EQ_SS);
        } else {
            cur_base[a] = Fa_MAKE_BOOL(Fa_AS_DOUBLE_ANY(lhs) == Fa_AS_DOUBLE_ANY(rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_EQ_FF);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_EQ_II)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_VMOPI(Fa_RB(), Fa_RC(), ==));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_EQ_FF)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_AS_DOUBLE_ANY(Fa_RB()) == Fa_AS_DOUBLE_ANY(Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_EQ_SS)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_AS_STRING(Fa_RB())->str == Fa_AS_STRING(Fa_RC())->str);
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_EQ_RI)
    {
        Fa_Value lhs = Fa_RB();
        auto imm = static_cast<int8_t>(Fa_instr_C(instr) - 128);

        if (LIKELY(Fa_IS_INTEGER(lhs))) {
            Fa_RA() = Fa_MAKE_BOOL(Fa_AS_INTEGER(lhs) == static_cast<i64>(imm));
        } else {
            if (UNLIKELY(!Fa_IS_DOUBLE(lhs)))
                runtime_error(ErrorCode::TYPE_ERROR_ARITH);

            Fa_RA() = Fa_MAKE_BOOL(Fa_AS_DOUBLE(lhs) == static_cast<f64>(imm));
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_NEQ)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            cur_base[a] = Fa_MAKE_BOOL(Fa_VMOPI(lhs, rhs, !=));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_NEQ_II);
        } else if (Fa_IS_STRING(lhs) && Fa_IS_STRING(rhs)) {
            cur_base[a] = Fa_MAKE_BOOL(Fa_AS_STRING(lhs)->str != Fa_AS_STRING(rhs)->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_NEQ_SS);
        } else {
            cur_base[a] = Fa_MAKE_BOOL(Fa_AS_DOUBLE_ANY(lhs) != Fa_AS_DOUBLE_ANY(rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_NEQ_FF);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_NEQ_II)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_VMOPI(Fa_RB(), Fa_RC(), !=));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_NEQ_FF)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_AS_DOUBLE_ANY(Fa_RB()) != Fa_AS_DOUBLE_ANY(Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_NEQ_SS)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_AS_STRING(Fa_RB())->str != Fa_AS_STRING(Fa_RC())->str);
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LT)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            cur_base[a] = Fa_MAKE_BOOL(Fa_VMOPI(lhs, rhs, <));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_LT_II);
        } else if (Fa_IS_STRING(lhs) && Fa_IS_STRING(rhs)) {
            cur_base[a] = Fa_MAKE_BOOL(Fa_AS_STRING(lhs)->str < Fa_AS_STRING(rhs)->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_LT_SS);
        } else {
            cur_base[a] = Fa_MAKE_BOOL(Fa_AS_DOUBLE_ANY(lhs) < Fa_AS_DOUBLE_ANY(rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_LT_FF);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_LT_II)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_VMOPI(Fa_RB(), Fa_RC(), <));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LT_FF)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_AS_DOUBLE_ANY(Fa_RB()) < Fa_AS_DOUBLE_ANY(Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LT_SS)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_AS_STRING(Fa_RB())->str < Fa_AS_STRING(Fa_RC())->str);
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LT_RI)
    {
        Fa_Value lhs = Fa_RB();
        auto imm = static_cast<int8_t>(Fa_instr_C(instr) - 128);

        if (LIKELY(Fa_IS_INTEGER(lhs))) {
            Fa_RA() = Fa_MAKE_BOOL(Fa_AS_INTEGER(lhs) < static_cast<i64>(imm));
        } else {
            if (UNLIKELY(!Fa_IS_DOUBLE(lhs)))
                runtime_error(ErrorCode::TYPE_ERROR_ARITH);

            Fa_RA() = Fa_MAKE_BOOL(Fa_AS_DOUBLE(lhs) < static_cast<f64>(imm));
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_LTE)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            cur_base[a] = Fa_MAKE_BOOL(Fa_VMOPI(lhs, rhs, <=));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_LTE_II);
        } else if (Fa_IS_STRING(lhs) && Fa_IS_STRING(rhs)) {
            cur_base[a] = Fa_MAKE_BOOL(Fa_AS_STRING(lhs)->str <= Fa_AS_STRING(rhs)->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_LTE_SS);
        } else {
            cur_base[a] = Fa_MAKE_BOOL(Fa_AS_DOUBLE_ANY(lhs) <= Fa_AS_DOUBLE_ANY(rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_LTE_FF);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_LTE_II)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_VMOPI(Fa_RB(), Fa_RC(), <=));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LTE_FF)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_AS_DOUBLE_ANY(Fa_RB()) <= Fa_AS_DOUBLE_ANY(Fa_RC()));
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LTE_SS)
    {
        Fa_RA() = Fa_MAKE_BOOL(Fa_AS_STRING(Fa_RB())->str <= Fa_AS_STRING(Fa_RC())->str);
        Fa_RECORD_BINARY_IC(Fa_RB(), Fa_RC(), Fa_RA());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LTE_RI)
    {
        Fa_Value lhs = Fa_RB();
        auto imm = static_cast<int8_t>(Fa_instr_C(instr) - 128);

        if (LIKELY(Fa_IS_INTEGER(lhs))) {
            Fa_RA() = Fa_MAKE_BOOL(Fa_AS_INTEGER(lhs) <= static_cast<i64>(imm));
        } else {
            if (UNLIKELY(!Fa_IS_DOUBLE(lhs)))
                runtime_error(ErrorCode::TYPE_ERROR_ARITH);

            Fa_RA() = Fa_MAKE_BOOL(Fa_AS_DOUBLE(lhs) <= static_cast<f64>(imm));
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_NOT)
    {
        Fa_RA() = Fa_MAKE_BOOL(!Fa_IS_TRUTHY(Fa_RB()));
        Fa_DISPATCH();
    }
    Fa_CASE(CONCAT)
    {
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_NEW)
    {
        Fa_ObjList* list_obj = Fa_MAKE_OBJ_LIST();
        list_obj->reserve(Fa_instr_B(instr));
        Fa_RA() = Fa_MAKE_OBJECT(list_obj);
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_APPEND)
    {
        Fa_Value list_v = Fa_RA();
        if (!Fa_IS_LIST(list_v))
            runtime_error(ErrorCode::TYPE_ERROR_CALL);

        Fa_AS_LIST(list_v)->elements.push(Fa_RB());
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_GET)
    {
        Fa_Value list_v = Fa_RB(), index_v = Fa_RC();

        if (!Fa_IS_LIST(list_v))
            runtime_error(ErrorCode::TYPE_ERROR_CALL);
        if (!Fa_IS_INTEGER(index_v))
            runtime_error(ErrorCode::INDEX_TYPE_ERROR);

        auto& elems = Fa_AS_LIST(list_v)->elements;
        i64 idx = Fa_AS_INTEGER(index_v);
        if (idx < 0 || idx >= static_cast<i64>(elems.size()))
            runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        Fa_RA() = elems[static_cast<u32>(idx)];
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_SET)
    {
        Fa_Value object_v = Fa_RA(), index_v = Fa_RB(), new_val = Fa_RC();
        if (Fa_IS_LIST(object_v)) {
            if (!Fa_IS_INTEGER(index_v))
                runtime_error(ErrorCode::INDEX_TYPE_ERROR);

            auto& elems = Fa_AS_LIST(object_v)->elements;
            i64 idx = Fa_AS_INTEGER(index_v);

            if (idx < 0)
                idx += static_cast<i64>(elems.size());
            if (idx < 0 || idx >= static_cast<i64>(elems.size()))
                runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            elems[static_cast<u32>(idx)] = new_val;
        } else if (Fa_IS_DICT(object_v)) {
            Fa_ObjDict* as_dict = Fa_AS_DICT(object_v);
            as_dict->data[index_v] = new_val;
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_CALL);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(LIST_LEN)
    {
        Fa_Value list_v = Fa_RB();
        if (!Fa_IS_LIST(list_v))
            runtime_error(ErrorCode::TYPE_ERROR_CALL);

        Fa_RA() = Fa_MAKE_INTEGER(static_cast<i64>(Fa_AS_LIST(list_v)->elements.size()));
        Fa_DISPATCH();
    }
    Fa_CASE(JUMP)
    {
        ip += Fa_instr_sBx(instr);
        Fa_DISPATCH();
    }
    Fa_CASE(JUMP_IF_TRUE)
    {
        if (Fa_IS_TRUTHY(Fa_RA()))
            ip += Fa_instr_sBx(instr);

        Fa_DISPATCH();
    }
    Fa_CASE(JUMP_IF_FALSE)
    {
        if (!Fa_IS_TRUTHY(Fa_RA()))
            ip += Fa_instr_sBx(instr);

        Fa_DISPATCH();
    }
    Fa_CASE(LOOP)
    {
        ip += Fa_instr_sBx(instr);
        Fa_DISPATCH();
    }
    Fa_CASE(FOR_PREP)
    {
        u8 base_reg = Fa_instr_A(instr);
        Fa_Value init_v = cur_base[base_reg];
        Fa_Value limit_v = cur_base[base_reg + 1];
        Fa_Value step_v = cur_base[base_reg + 2];

        if (!Fa_IS_INTEGER(init_v) || !Fa_IS_INTEGER(limit_v) || !Fa_IS_INTEGER(step_v))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        i64 init = Fa_AS_INTEGER(init_v);
        i64 limit = Fa_AS_INTEGER(limit_v);
        i64 step = Fa_AS_INTEGER(step_v);
        if (step == 0)
            runtime_error(ErrorCode::DIVISION_BY_ZERO);

        cur_base[base_reg] = Fa_MAKE_INTEGER(limit);
        cur_base[base_reg + 1] = Fa_MAKE_INTEGER(step);
        cur_base[base_reg + 2] = Fa_MAKE_INTEGER(init);
        bool enters = (step > 0) ? (init <= limit) : (init >= limit);
        if (!enters)
            ip += Fa_instr_sBx(instr);

        Fa_DISPATCH();
    }
    Fa_CASE(FOR_STEP)
    {
        u8 base_reg = Fa_instr_A(instr);
        Fa_Value limit_v = cur_base[base_reg];
        Fa_Value step_v = cur_base[base_reg + 1];
        Fa_Value control_v = cur_base[base_reg + 2];

        if (Fa_IS_INTEGER(control_v)) {
            i64 control = Fa_AS_INTEGER(control_v) + Fa_AS_INTEGER(step_v);
            cur_base[base_reg + 2] = Fa_MAKE_INTEGER(control);
            bool continues = (Fa_AS_INTEGER(step_v) > 0) ? (control <= Fa_AS_INTEGER(limit_v)) : (control >= Fa_AS_INTEGER(limit_v));
            if (continues)
                ip += Fa_instr_sBx(instr);
        } else {
            f64 control = Fa_AS_DOUBLE_ANY(control_v) + Fa_AS_DOUBLE_ANY(step_v);
            cur_base[base_reg + 2] = Fa_MAKE_REAL(control);
            bool continues = Fa_AS_DOUBLE(step_v) > 0.0 ? control <= Fa_AS_DOUBLE(limit_v) : control >= Fa_AS_DOUBLE(limit_v);
            if (continues)
                ip += Fa_instr_sBx(instr);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(CLOSURE)
    {
        u16 fn_idx = Fa_instr_Bx(instr);
        Fa_Chunk* fn_chunk = cur_chunk->functions[fn_idx];
        Fa_ObjFunction* fn = Fa_MAKE_OBJ_FUNCTION(fn_chunk);
        fn->arity = fn_chunk->arity;
        Fa_RA() = Fa_MAKE_OBJECT(Fa_MAKE_OBJ_CLOSURE(fn));
        Fa_DISPATCH();
    }
    Fa_CASE(CALL)
    {
        u8 fn_reg = Fa_instr_A(instr);
        u8 argc = Fa_instr_B(instr);
        Fa_Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;

        if (Fa_IS_CLOSURE(callee)) {
            Fa_ObjClosure* cl = Fa_AS_CLOSURE(callee);
            Fa_Chunk* fchk = cl->function->chunk;

            if (UNLIKELY(argc != fchk->arity))
                runtime_error(ErrorCode::WRONG_ARG_COUNT);
            if (UNLIKELY(m_frames_top >= MAX_FRAMES))
                runtime_error(ErrorCode::STACK_OVERFLOW);

            int local_count = fchk->local_count;
            int new_top = base + local_count + 1;
            while (m_stack_top < new_top) {
                m_stack[m_stack_top] = NIL_VAL;
                m_stack_top += 1;
            }

            SAVE_IP();
            m_frames[m_frames_top] = Fa_CallFrame(cl, fchk, 0, base, local_count);
            m_frames_top += 1;
        } else {
            SAVE_IP();
            call_value(callee, argc, base, false);
        }

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(CALL_TAIL)
    {
        u8 fn_reg = Fa_instr_A(instr);
        u8 argc = Fa_instr_B(instr);
        Fa_Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;
        SAVE_IP();
        call_value(callee, argc, base, true);
        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(IC_CALL)
    {
        u8 fn_reg = Fa_instr_A(instr);
        u8 argc = Fa_instr_B(instr);
        u8 ic_idx = Fa_instr_C(instr);
        u32 call_ip = ip - 1;
        Fa_Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;
        bool has_slot = ic_idx < cur_chunk->ic_slots.size();

        if (has_slot) {
            auto& slot = cur_chunk->ic_slots[ic_idx];
            slot.seen_lhs |= static_cast<u8>(value_type_tag(callee));
            slot.hit_count += 1;
        }

        Fa_Chunk* caller_chunk = cur_chunk;
        int result_slot = base - 1;

        if (Fa_IS_CLOSURE(callee)) {
            Fa_ObjClosure* cl = Fa_AS_CLOSURE(callee);
            Fa_Chunk* fchk = cl->function->chunk;

            if (UNLIKELY(argc != fchk->arity))
                runtime_error(ErrorCode::WRONG_ARG_COUNT);
            if (UNLIKELY(m_frames_top >= MAX_FRAMES))
                runtime_error(ErrorCode::STACK_OVERFLOW);

            int local_count = fchk->local_count;
            int new_top = base + local_count + 1;
            while (m_stack_top < new_top) {
                m_stack[m_stack_top] = NIL_VAL;
                m_stack_top += 1;
            }

            SAVE_IP();
            m_frames[m_frames_top] = Fa_CallFrame(cl, fchk, 0, base, local_count);
            m_frames_top += 1;
        } else {
            SAVE_IP();
            call_value(callee, argc, base, false);
        }

        LOAD_FRAME();

        if (has_slot) {
            caller_chunk->ic_slots[ic_idx].seen_ret |= static_cast<u8>(value_type_tag(m_stack[result_slot]));
            caller_chunk->code[call_ip] = Fa_make_ABC(Fa_OpCode::CALL, fn_reg, argc, 0);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(RETURN)
    {
        u8 src = Fa_instr_A(instr);
        u8 n_ret = Fa_instr_B(instr);
        Fa_Value ret = n_ret > 0 ? cur_base[src] : NIL_VAL;
        m_stack[cur_frame_base - 1] = ret;
        m_frames_top -= 1;

        if (LIKELY(m_frames_top == 0))
            return ret;

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(RETURN_NIL)
    {
        m_stack[cur_frame_base - 1] = NIL_VAL;
        m_frames_top -= 1;

        if (LIKELY(m_frames_top == 0))
            return NIL_VAL;

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(RETURN1)
    {
        Fa_Value ret = cur_base[Fa_instr_A(instr)];
        m_stack[cur_frame_base - 1] = ret;
        m_frames_top -= 1;

        if (LIKELY(m_frames_top == 0))
            return ret;

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(INDEX)
    {
        Fa_Value obj = Fa_RB();
        Fa_Value idx = Fa_RC();

        if (Fa_IS_STRING(obj)) {
            if (UNLIKELY(!Fa_IS_INTEGER(idx)))
                runtime_error(ErrorCode::INDEX_TYPE_ERROR);

            i64 idx_int = Fa_AS_INTEGER(idx);
            if (UNLIKELY(idx_int < 0))
                runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            Fa_StringRef const& str = Fa_AS_STRING(obj)->str;

            size_t byte_pos = 0;
            i64 char_pos = 0;
            u64 char_bytes = 0;

            while (byte_pos < str.len() && char_pos < idx_int) {
                u64 step = 0;
                util::decode_utf8_at(str, byte_pos, &step);
                byte_pos += step;
                char_pos += 1;
            }

            if (UNLIKELY(byte_pos >= str.len()))
                runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            util::decode_utf8_at(str, byte_pos, &char_bytes);

            Fa_StringRef ch = str.slice(byte_pos, byte_pos + char_bytes);
            Fa_RA() = Fa_MAKE_STRING(ch);
        } else if (Fa_IS_LIST(obj)) {
            if (UNLIKELY(!Fa_IS_INTEGER(idx)))
                runtime_error(ErrorCode::INDEX_TYPE_ERROR);

            i64 idx_int = Fa_AS_INTEGER(idx);
            Fa_Array<Fa_Value> list = Fa_AS_LIST(obj)->elements;
            if (UNLIKELY(idx_int >= list.size() || idx_int < 0))
                runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            Fa_RA() = Fa_AS_LIST(obj)->elements[idx_int];
        } else if (Fa_IS_DICT(obj)) {
            Fa_ObjDict* dict_obj = Fa_AS_DICT(obj);
            if (dict_obj->data.find_ptr(idx) == nullptr)
                Fa_RA() = NIL_VAL;
            else
                Fa_RA() = dict_obj->data[idx];
        } else {
            runtime_error(ErrorCode::INDEX_TYPE_ERROR);
        }

        Fa_DISPATCH();
    }

    Fa_CASE(NOP)
    {
        Fa_DISPATCH();
    }
    Fa_CASE(HALT)
    {
        halt();
    }

    Fa_END_DISPATCH();

    return NIL_VAL; // UNREACHABLE
}

void Fa_VM::call_value(Fa_Value callee, int argc, int call_base, bool tail)
{
    if (Fa_IS_CLOSURE(callee)) {
        Fa_ObjClosure* cl = Fa_AS_CLOSURE(callee);
        Fa_Chunk* fchk = cl->function->chunk;
        int arity = fchk->arity;
        int local_count = fchk->local_count;

        if (argc != arity) {
            diagnostic::emit(ErrorCode::WRONG_ARG_COUNT, "expected " + std::to_string(arity) + " arguments but got " + std::to_string(argc));
            runtime_error(ErrorCode::WRONG_ARG_COUNT);
        }

        if (local_count < argc)
            runtime_error(ErrorCode::WRONG_ARG_COUNT);

        if (tail && m_frames_top > 0) {
            int cur_base = m_frames[m_frames_top - 1].base;

            for (int i = 0; i < argc; i += 1)
                m_stack[cur_base + i] = m_stack[call_base + i];
            for (int i = argc; i < local_count; i += 1)
                m_stack[cur_base + i] = NIL_VAL;

            m_frames[m_frames_top - 1] = Fa_CallFrame(cl, fchk, 0, cur_base, local_count);

            int new_top = cur_base + local_count + 1;
            if (m_stack_top < new_top)
                m_stack_top = new_top;
        } else {
            int new_top = call_base + local_count + 1;
            while (m_stack_top < new_top) {
                m_stack[m_stack_top] = NIL_VAL;
                m_stack_top += 1;
            }

            for (int i = argc; i < local_count; i += 1)
                m_stack[call_base + i] = NIL_VAL;

            m_frames[m_frames_top] = Fa_CallFrame(cl, fchk, 0, call_base, local_count);
            m_frames_top += 1;
        }
        return;
    }

    if (Fa_IS_NATIVE(callee)) {
        Fa_ObjNative* nat = Fa_AS_NATIVE(callee);
        if (nat->arity >= 0 && argc != nat->arity) {
            std::string name = (nat->name ? std::string(nat->name->str.data(), nat->name->str.len()) : "?");
            diagnostic::emit(ErrorCode::NATIVE_ARG_COUNT,
                std::string("native '") + name + "' expected " + std::to_string(nat->arity) + " args, got " + std::to_string(argc));
            runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        }

        m_stack[call_base - 1] = call_native(nat, argc, call_base);
        return;
    }

    Fa_StringRef fn_name;
    if (Fa_IS_FUNCTION(callee) && Fa_AS_FUNCTION(callee)->name)
        fn_name = Fa_AS_FUNCTION(callee)->name->str;

    diagnostic::emit(ErrorCode::NON_FUNCTION_CALL, std::string(fn_name.data(), fn_name.len()));
    runtime_error(ErrorCode::NON_FUNCTION_CALL);
}

Fa_Value Fa_VM::call_native(Fa_ObjNative* nat, int argc, int call_base)
{
    return (this->*nat->fn)(argc, &m_stack[call_base]);
}

Fa_ObjString* Fa_VM::intern(Fa_StringRef const& str)
{
    if (Fa_ObjString** existing = m_string_table.find_ptr(str))
        return *existing;

    Fa_ObjString* obj = Fa_MAKE_OBJ_STRING(str);
    m_string_table.insert_or_assign(str, obj);
    return obj;
}

void Fa_VM::intern_chunk_constants(Fa_Chunk* ch)
{
    if (ch == nullptr)
        return;

    for (u32 i = 0; i < ch->constants.size(); i += 1) {
        if (Fa_IS_STRING(ch->constants[i]))
            ch->constants[i] = Fa_MAKE_OBJECT(intern(Fa_AS_STRING(ch->constants[i])->str));
    }

    for (auto* fn : ch->functions)
        intern_chunk_constants(fn);
}

void Fa_VM::open_stdlib()
{
    register_native("حجم", &Fa_VM::Fa_len, -1);
    register_native("len", &Fa_VM::Fa_len, -1);
    register_native("اضف", &Fa_VM::Fa_append, -1);
    register_native("احذف", &Fa_VM::Fa_pop, -1);
    register_native("مقطع", &Fa_VM::Fa_slice, -1);
    register_native("اكتب", &Fa_VM::Fa_print, -1);
    register_native("input", &Fa_VM::Fa_input, -1);
    register_native("نوع", &Fa_VM::Fa_type, -1);
    register_native("طبيعي", &Fa_VM::Fa_int, -1);
    register_native("حقيقي", &Fa_VM::Fa_float, -1);
    register_native("str", &Fa_VM::Fa_str, -1);
    register_native("bool", &Fa_VM::Fa_bool, -1);
    register_native("قائمة", &Fa_VM::Fa_list, -1);
    register_native("جدول", &Fa_VM::Fa_dict, -1);
    register_native("dict", &Fa_VM::Fa_dict, -1);
    register_native("اقسم", &Fa_VM::Fa_split, -1);
    register_native("join", &Fa_VM::Fa_join, -1);
    register_native("substr", &Fa_VM::Fa_substr, -1);
    register_native("contains", &Fa_VM::Fa_contains, -1);
    register_native("trim", &Fa_VM::Fa_trim, -1);
    register_native("floor", &Fa_VM::Fa_floor, -1);
    register_native("ceil", &Fa_VM::Fa_ceil, -1);
    register_native("round", &Fa_VM::Fa_round, -1);
    register_native("abs", &Fa_VM::Fa_abs, -1);
    register_native("min", &Fa_VM::Fa_min, -1);
    register_native("max", &Fa_VM::Fa_max, -1);
    register_native("pow", &Fa_VM::Fa_pow, -1);
    register_native("sqrt", &Fa_VM::Fa_sqrt, -1);
    register_native("assert", &Fa_VM::Fa_assert, -1);
    register_native("clock", &Fa_VM::Fa_clock, -1);
    register_native("error", &Fa_VM::Fa_error, -1);
    register_native("time", &Fa_VM::Fa_time, -1);
}

void Fa_VM::register_native(Fa_StringRef const& name, NativeFn fn, int arity)
{
    Fa_ObjString* name_obj = Fa_MAKE_OBJ_STRING(name);
    Fa_Value val = Fa_MAKE_NATIVE(fn, name_obj, arity);

    if (u32* slot = m_global_index.find_ptr(name)) {
        m_global_slots[*slot] = val;
    } else {
        auto slot_idx = m_global_slots.size();
        m_global_slots.push(val);
        m_global_index.insert_or_assign(name, slot_idx);
    }
}

Fa_SourceLocation Fa_VM::current_location() const
{
    if (m_frames_top == 0)
        return { };

    Fa_CallFrame const& f = top_frame();
    size_t off = f.ip > 0 ? f.ip - 1 : 0;
    Fa_Chunk const& ch = *f.chunk;

    if (off < ch.locations.size())
        return ch.locations[off];

    return { };
}

void Fa_VM::runtime_error(ErrorCode code, std::string const& detail)
{
    Fa_SourceLocation loc = current_location();
    diagnostic::report(diagnostic::Severity::ERROR, loc, static_cast<u16>(code), detail);

    for (int i = m_frames_top - 1; i >= 0; i -= 1) {
        Fa_CallFrame* p = &m_frames[i];
        if (p->chunk == nullptr)
            continue;

        Fa_Chunk const& ch = *p->chunk;
        size_t off = p->ip > 0 ? p->ip - 1 : 0;
        if (off >= ch.locations.size())
            continue;

        Fa_SourceLocation frame_loc = ch.locations[off];
        std::string note = "at " + std::to_string(frame_loc.line) + ":" + std::to_string(frame_loc.column);
        if (p->closure && p->closure->function && p->closure->function->name) {
            Fa_StringRef fname = p->closure->function->name->str;
            note = "in '" + std::string(fname.data(), fname.len()) + "' " + note;
        }

        diagnostic::engine.add_note(frame_loc.line, note);
    }

    diagnostic::dump();
    halt();
}

void Fa_VM::halt()
{
    m_frames_top = 0;
    m_stack_top = 0;
    throw Fa_RuntimeHalt();
}

Fa_CallFrame& Fa_VM::frame() { return m_frames[m_frames_top - 1]; }
Fa_CallFrame const& Fa_VM::frame() const { return m_frames[m_frames_top - 1]; }

Fa_Chunk* Fa_VM::chunk() { return top_frame().chunk; }
Fa_Value& Fa_VM::m_reg(int r) { return m_stack[top_frame().base + r]; }

void Fa_VM::update_ic_binary(Fa_Chunk* ch, u32 nop_ip, Fa_Value lhs, Fa_Value rhs, Fa_Value result)
{
    if (ch == nullptr)
        return;

    u32 nop = ch->code[nop_ip];
    u8 ic_idx = Fa_instr_A(nop);

    if (ic_idx < ch->ic_slots.size()) {
        auto& slot = ch->ic_slots[ic_idx];
        slot.seen_lhs |= static_cast<u8>(value_type_tag(lhs));
        slot.seen_rhs |= static_cast<u8>(value_type_tag(rhs));
        slot.seen_ret |= static_cast<u8>(value_type_tag(result));
        slot.hit_count += 1;
    }
}

} // namespace fairuz::runtime
