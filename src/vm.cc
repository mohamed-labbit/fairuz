//
// vm.cc
//

#include "../include/vm.hpp"

#define Fa_DISPATCH()                                              \
    do {                                                           \
        instr = cur_chunk->code[ip++];                             \
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
        &&H_GET_UPVALUE,         \
        &&H_SET_UPVALUE,         \
        &&H_CLOSE_UPVALUE,       \
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

#define LOAD_FRAME()                \
    do {                            \
        Fa_CallFrame& f = frame();  \
        cur_closure = f.closure;    \
        cur_chunk = f.chunk;        \
        cur_frame_base = f.base;    \
        cur_base = &Stack_[f.base]; \
        ip = f.ip;                  \
    } while (0)

#define SAVE_IP()        \
    do {                 \
        frame().ip = ip; \
    } while (0)

#define CLOSE_UPVALUES_IF_NEEDED()                                         \
    do {                                                                   \
        if (UNLIKELY(has_open_upvalues != 0)) {                            \
            Fa_Value* threshold = &Stack_[cur_frame_base];                 \
            auto i = static_cast<u32>(OpenUpvalues_.size());               \
            while (i > 0 && OpenUpvalues_[i - 1]->location >= threshold) { \
                ObjUpvalue* uv = OpenUpvalues_[--i];                       \
                uv->closed = *uv->location;                                \
                uv->location = &uv->closed;                                \
            }                                                              \
            has_open_upvalues = static_cast<int>(i);                       \
            OpenUpvalueCount_ = has_open_upvalues;                         \
            OpenUpvalues_.resize(i);                                       \
        }                                                                  \
    } while (0)

#define Fa_RECORD_BINARY_IC(lhs, rhs, result)                      \
    do {                                                           \
        if (ip < cur_chunk->code.size()                            \
            && Fa_instr_op(cur_chunk->code[ip]) == Fa_OpCode::NOP) \
            updateIcBinary(cur_chunk, ip, lhs, rhs, result);       \
    } while (0)

#define REQUIRE_NUMBER(v)                              \
    do {                                               \
        if (!Fa_IS_NUMBER(v))                          \
            runtimeError(ErrorCode::TYPE_ERROR_ARITH); \
    } while (0)

namespace fairuz::runtime {

static void checkStackIndex(int index, int stack_size, char const* context)
{
    if (index < 0 || index >= stack_size) {
        // This is a Fa_VM internal error, not a user error.
        throw std::logic_error(
            std::string("Fa_VM internal error: stack index ") + std::to_string(index)
            + " out of range [0," + std::to_string(stack_size) + ") in " + context);
    }
}

Fa_VM::Fa_VM()
{
    std::fill(Stack_, Stack_ + STACK_SIZE, NIL_VAL);
    std::fill(Frames_, Frames_ + MAX_FRAMES, Fa_CallFrame());

    openStdlib();
}

Fa_VM::~Fa_VM()
{
    GC_.sweepAll();
}

#define PUSH_VALUE(v)                                \
    do {                                             \
        if (StackTop_ == STACK_SIZE)                 \
            runtimeError(ErrorCode::STACK_OVERFLOW); \
        Stack_[StackTop_++] = v;                     \
    } while (0);

// Ensure the value stack has at least `needed` slots allocated, filling
// any new slots with NIL.  Checks against the hard cap.
void Fa_VM::ensureStackSlots(int needed)
{
    if (needed > STACK_SIZE)
        runtimeError(ErrorCode::STACK_OVERFLOW);

    while (StackTop_ < needed)
        PUSH_VALUE(NIL_VAL);
}

// Reference to the topmost call frame.
Fa_VM::Fa_CallFrame& Fa_VM::topFrame()
{
    assert(FramesTop_ > 0 && "topFrame called with empty frame stack");
    return Frames_[FramesTop_ - 1];
}

Fa_VM::Fa_CallFrame const& Fa_VM::topFrame() const
{
    assert(FramesTop_ > 0 && "topFrame called with empty frame stack");
    return Frames_[FramesTop_ - 1];
}

Fa_Value& Fa_VM::getReg(Fa_CallFrame const& f, int reg)
{
    int abs = f.base + reg;
    checkStackIndex(abs, STACK_SIZE, "getReg");
    return Stack_[abs];
}

Fa_Value Fa_VM::run(Fa_Chunk* chunk)
{
    if (!chunk)
        return NIL_VAL;

    StackTop_ = 0;
    FramesTop_ = 0;
    OpenUpvalueCount_ = 0;
    OpenUpvalues_.clear();

    Fa_ObjFunction* fn = Fa_MAKE_OBJ_FUNCTION();
    fn->chunk = chunk;
    Fa_ObjClosure* cl = Fa_MAKE_OBJ_CLOSURE(fn);

    Stack_[0] = Fa_MAKE_OBJECT(cl);
    StackTop_ = static_cast<int>(chunk->localCount) + 2;

    if (FramesTop_ >= MAX_FRAMES || StackTop_ >= STACK_SIZE)
        runtimeError(ErrorCode::STACK_OVERFLOW);

    Frames_[FramesTop_++] = Fa_CallFrame(cl, chunk, 0, 1, static_cast<int>(chunk->localCount));
    internChunkConstants(cl->function->chunk);

    if (GC_.currentMemory() >= GC_THRESHOLD)
        GC_.collect(this);

    return execute();
}

Fa_Value Fa_VM::execute()
{
    static void const* dispatch_table[] = { Fa_TABLE_INIT };

    if (FramesTop_ == 0)
        return NIL_VAL;

    u32 instr;
    int has_open_upvalues = OpenUpvalueCount_;
    Fa_Chunk* cur_chunk = frame().chunk;
    Fa_ObjClosure* cur_closure = frame().closure;
    int cur_frame_base = frame().base;
    Fa_Value* cur_base = &Stack_[cur_frame_base];
    u32 ip = frame().ip;

    Fa_BEGIN_DISPATCH();

    Fa_CASE(LOAD_NIL)
    {
        u8 start = Fa_instr_B(instr);
        u8 count = Fa_instr_C(instr);
        for (u8 i = 0; i < count; ++i)
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
            u32 slot_idx;
            if (u32* slot = GlobalIndex_.findPtr(name)) {
                slot_idx = *slot;
            } else {
                slot_idx = static_cast<u32>(GlobalSlots_.size());
                GlobalSlots_.push(NIL_VAL);
                GlobalIndex_.insertOrAssign(name, slot_idx);
            }
            Fa_RA() = GlobalSlots_[slot_idx];
            cur_chunk->code[ip - 1] = Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL_CACHED, Fa_instr_A(instr), slot_idx);
        }
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_GLOBAL_CACHED)
    {
        Fa_RA() = GlobalSlots_[Fa_instr_Bx(instr)];
        Fa_DISPATCH();
    }
    Fa_CASE(STORE_GLOBAL)
    {
        {
            Fa_Value name_v = cur_chunk->constants[Fa_instr_Bx(instr)];
            Fa_StringRef name = Fa_AS_STRING(name_v)->str;
            u32 slot_idx;
            if (u32* slot = GlobalIndex_.findPtr(name)) {
                slot_idx = *slot;
            } else {
                slot_idx = static_cast<u32>(GlobalSlots_.size());
                GlobalSlots_.push(NIL_VAL);
                GlobalIndex_.insertOrAssign(name, slot_idx);
            }
            GlobalSlots_[slot_idx] = Fa_RA();
            cur_chunk->code[ip - 1] = Fa_make_ABx(Fa_OpCode::STORE_GLOBAL_CACHED, Fa_instr_A(instr), slot_idx);
        }
        Fa_DISPATCH();
    }
    Fa_CASE(STORE_GLOBAL_CACHED)
    {
        GlobalSlots_[Fa_instr_Bx(instr)] = Fa_RA();
        Fa_DISPATCH();
    }
    Fa_CASE(MOVE)
    {
        Fa_RA() = Fa_RB();
        Fa_DISPATCH();
    }
    Fa_CASE(GET_UPVALUE)
    {
        Fa_RA() = *cur_closure->upValues[Fa_instr_B(instr)]->location;
        Fa_DISPATCH();
    }
    Fa_CASE(SET_UPVALUE)
    {
        *cur_closure->upValues[Fa_instr_B(instr)]->location = Fa_RA();
        Fa_DISPATCH();
    }
    Fa_CASE(CLOSE_UPVALUE)
    {
        closeUpvalues(cur_frame_base + Fa_instr_A(instr));
        has_open_upvalues = OpenUpvalueCount_;
        Fa_DISPATCH();
    }
    Fa_CASE(OP_ADD)
    {
        u8 a = Fa_instr_A(instr), b = Fa_instr_B(instr), c = Fa_instr_C(instr);
        Fa_Value lhs = cur_base[b], rhs = cur_base[c];

        if (UNLIKELY(!Fa_IS_NUMBER(lhs) || !Fa_IS_NUMBER(rhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

        if (Fa_IS_INTEGER(lhs) && Fa_IS_INTEGER(rhs)) {
            cur_base[a] = Fa_MAKE_INTEGER(Fa_VM_ADDI(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_ADD_II);
        } else {
            cur_base[a] = Fa_MAKE_REAL(Fa_VM_ADDF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            Fa_VM_ABC(Fa_OpCode::OP_ADD_FF);
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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

        if (Fa_AS_DOUBLE_ANY(rhs) == 0.0)
            runtimeError(ErrorCode::DIVISION_BY_ZERO);

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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

        if (Fa_AS_DOUBLE_ANY(rhs) == 0.0)
            runtimeError(ErrorCode::MODULO_BY_ZERO);

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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

        Fa_RA() = Fa_MAKE_INTEGER(~Fa_AS_INTEGER(operand));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LSHIFT)
    {
        Fa_Value lhs = Fa_RB();
        if (UNLIKELY(!Fa_IS_INTEGER(lhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

        u8 imm = Fa_instr_C(instr);
        if (imm > 64)
            runtimeError(ErrorCode::INDEX_TYPE_ERROR);

        Fa_RA() = Fa_MAKE_INTEGER(static_cast<i64>(static_cast<u64>(Fa_AS_INTEGER(lhs)) << imm));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_RSHIFT)
    {
        Fa_Value lhs = Fa_RB();
        if (UNLIKELY(!Fa_IS_INTEGER(lhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

        u8 imm = Fa_instr_C(instr);
        if (imm > 64)
            runtimeError(ErrorCode::INDEX_TYPE_ERROR);

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
                runtimeError(ErrorCode::TYPE_ERROR_ARITH);

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
                runtimeError(ErrorCode::TYPE_ERROR_ARITH);

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
                runtimeError(ErrorCode::TYPE_ERROR_ARITH);
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
            runtimeError(ErrorCode::TYPE_ERROR_CALL);

        Fa_AS_LIST(list_v)->elements.push(Fa_RB());
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_GET)
    {
        Fa_Value list_v = Fa_RB(), index_v = Fa_RC();
        if (!Fa_IS_LIST(list_v))
            runtimeError(ErrorCode::TYPE_ERROR_CALL);

        if (!Fa_IS_INTEGER(index_v))
            runtimeError(ErrorCode::INDEX_TYPE_ERROR);

        auto& elems = Fa_AS_LIST(list_v)->elements;
        i64 idx = Fa_AS_INTEGER(index_v);
        if (idx < 0 || idx >= static_cast<i64>(elems.size()))
            runtimeError(ErrorCode::INDEX_OUT_OF_BOUNDS);

        Fa_RA() = elems[static_cast<u32>(idx)];
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_SET)
    {
        Fa_Value list_v = Fa_RA(), index_v = Fa_RB(), new_val = Fa_RC();
        if (!Fa_IS_LIST(list_v))
            runtimeError(ErrorCode::TYPE_ERROR_CALL);

        if (!Fa_IS_INTEGER(index_v))
            runtimeError(ErrorCode::INDEX_TYPE_ERROR);

        auto& elems = Fa_AS_LIST(list_v)->elements;
        i64 idx = Fa_AS_INTEGER(index_v);
        if (idx < 0)
            idx += static_cast<i64>(elems.size());

        if (idx < 0 || idx >= static_cast<i64>(elems.size()))
            runtimeError(ErrorCode::INDEX_OUT_OF_BOUNDS);

        elems[static_cast<u32>(idx)] = new_val;
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_LEN)
    {
        Fa_Value list_v = Fa_RB();
        if (!Fa_IS_LIST(list_v))
            runtimeError(ErrorCode::TYPE_ERROR_CALL);

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
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);

        i64 init = Fa_AS_INTEGER(init_v);
        i64 limit = Fa_AS_INTEGER(limit_v);
        i64 step = Fa_AS_INTEGER(step_v);
        if (step == 0)
            runtimeError(ErrorCode::DIVISION_BY_ZERO);

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
        fn->upvalueCount = fn_chunk->upvalueCount;
        Fa_ObjClosure* cl = Fa_MAKE_OBJ_CLOSURE(fn);

        for (unsigned int i = 0; i < fn_chunk->upvalueCount; ++i) {
            u32 desc = cur_chunk->code[ip++];
            bool is_local = Fa_instr_A(desc) == 1;
            u8 index = Fa_instr_B(desc);

            if (is_local) {
                cl->upValues[i] = captureUpvalue(cur_frame_base + index);
                has_open_upvalues = OpenUpvalueCount_;
            } else {
                cl->upValues[i] = cur_closure->upValues[index];
            }
        }

        Fa_RA() = Fa_MAKE_OBJECT(cl);
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
                runtimeError(ErrorCode::WRONG_ARG_COUNT);

            if (UNLIKELY(FramesTop_ >= MAX_FRAMES))
                runtimeError(ErrorCode::STACK_OVERFLOW);

            int local_count = fchk->localCount;
            int new_top = base + local_count + 1;
            while (StackTop_ < new_top)
                Stack_[StackTop_++] = NIL_VAL;

            SAVE_IP();
            Frames_[FramesTop_++] = Fa_CallFrame(cl, fchk, 0, base, local_count);
        } else {
            SAVE_IP();
            callValue(callee, argc, base, false);
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
        callValue(callee, argc, base, true);
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
        bool has_slot = ic_idx < cur_chunk->icSlots.size();

        if (has_slot) {
            auto& slot = cur_chunk->icSlots[ic_idx];
            slot.seenLhs |= static_cast<u8>(valueTypeTag(callee));
            slot.hitCount++;
        }

        Fa_Chunk* caller_chunk = cur_chunk;
        int result_slot = base - 1;

        if (Fa_IS_CLOSURE(callee)) {
            Fa_ObjClosure* cl = Fa_AS_CLOSURE(callee);
            Fa_Chunk* fchk = cl->function->chunk;

            if (UNLIKELY(argc != fchk->arity))
                runtimeError(ErrorCode::WRONG_ARG_COUNT);

            if (UNLIKELY(FramesTop_ >= MAX_FRAMES))
                runtimeError(ErrorCode::STACK_OVERFLOW);

            int local_count = fchk->localCount;
            int new_top = base + local_count + 1;
            while (StackTop_ < new_top)
                Stack_[StackTop_++] = NIL_VAL;

            SAVE_IP();
            Frames_[FramesTop_++] = Fa_CallFrame(cl, fchk, 0, base, local_count);
        } else {
            SAVE_IP();
            callValue(callee, argc, base, false);
        }

        LOAD_FRAME();

        if (has_slot) {
            caller_chunk->icSlots[ic_idx].seenRet |= static_cast<u8>(valueTypeTag(Stack_[result_slot]));
            caller_chunk->code[call_ip] = Fa_make_ABC(Fa_OpCode::CALL, fn_reg, argc, 0);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(RETURN)
    {
        u8 src = Fa_instr_A(instr);
        u8 n_ret = Fa_instr_B(instr);
        Fa_Value ret = n_ret > 0 ? cur_base[src] : NIL_VAL;
        CLOSE_UPVALUES_IF_NEEDED();
        Stack_[cur_frame_base - 1] = ret;
        --FramesTop_;

        if (LIKELY(FramesTop_ == 0))
            return ret;

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(RETURN_NIL)
    {
        CLOSE_UPVALUES_IF_NEEDED();
        Stack_[cur_frame_base - 1] = NIL_VAL;
        --FramesTop_;

        if (LIKELY(FramesTop_ == 0))
            return NIL_VAL;

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(RETURN1)
    {
        Fa_Value ret = cur_base[Fa_instr_A(instr)];
        CLOSE_UPVALUES_IF_NEEDED();
        Stack_[cur_frame_base - 1] = ret;
        --FramesTop_;

        if (LIKELY(FramesTop_ == 0))
            return ret;

        LOAD_FRAME();
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

void Fa_VM::callValue(Fa_Value callee, int argc, int call_base, bool tail)
{
    if (Fa_IS_CLOSURE(callee)) {
        Fa_ObjClosure* cl = Fa_AS_CLOSURE(callee);
        Fa_Chunk* fchk = cl->function->chunk;
        int arity = fchk->arity;
        int local_count = fchk->localCount;

        if (argc != arity) {
            diagnostic::emit(ErrorCode::WRONG_ARG_COUNT,
                "expected " + std::to_string(arity) + " arguments but got " + std::to_string(argc));
            runtimeError(ErrorCode::WRONG_ARG_COUNT);
        }

        if (local_count < argc)
            runtimeError(ErrorCode::WRONG_ARG_COUNT);

        if (tail && FramesTop_ > 0) {
            int cur_base = Frames_[FramesTop_ - 1].base;

            for (int i = 0; i < argc; ++i)
                Stack_[cur_base + i] = Stack_[call_base + i];

            for (int i = argc; i < local_count; ++i)
                Stack_[cur_base + i] = NIL_VAL;

            Frames_[FramesTop_ - 1] = Fa_CallFrame(cl, fchk, 0, cur_base, local_count);

            int new_top = cur_base + local_count + 1;
            if (StackTop_ < new_top)
                StackTop_ = new_top;
        } else {
            int new_top = call_base + local_count + 1;
            while (StackTop_ < new_top)
                Stack_[StackTop_++] = NIL_VAL;

            for (int i = argc; i < local_count; ++i)
                Stack_[call_base + i] = NIL_VAL;

            Frames_[FramesTop_++] = Fa_CallFrame(cl, fchk, 0, call_base, local_count);
        }
        return;
    }

    if (Fa_IS_NATIVE(callee)) {
        Fa_ObjNative* nat = Fa_AS_NATIVE(callee);
        if (nat->arity >= 0 && argc != nat->arity) {
            std::string name = (nat->name ? std::string(nat->name->str.data(), nat->name->str.len()) : "?");
            diagnostic::emit(ErrorCode::NATIVE_ARG_COUNT,
                "native '" + name + "' expected " + std::to_string(nat->arity) + " args, got " + std::to_string(argc));
            runtimeError(ErrorCode::NATIVE_ARG_COUNT);
        }

        Stack_[call_base - 1] = callNative(nat, argc, call_base);
        return;
    }

    Fa_StringRef fn_name;
    if (Fa_IS_FUNCTION(callee) && Fa_AS_FUNCTION(callee)->name)
        fn_name = Fa_AS_FUNCTION(callee)->name->str;

    diagnostic::emit(ErrorCode::NON_FUNCTION_CALL, std::string(fn_name.data(), fn_name.len()));
    runtimeError(ErrorCode::NON_FUNCTION_CALL);
}

Fa_Value Fa_VM::callNative(Fa_ObjNative* nat, int argc, int call_base)
{
    return (this->*nat->fn)(argc, &Stack_[call_base]);
}

ObjUpvalue* Fa_VM::captureUpvalue(unsigned int stack_pos)
{
    // Re-use an existing open upvalue pointing to the same slot.
    for (auto i = static_cast<int>(OpenUpvalues_.size()) - 1; i >= 0; --i) {
        ObjUpvalue* uv = OpenUpvalues_[i];
        if (uv->location == &Stack_[stack_pos])
            return uv;
    }

    ObjUpvalue* uv = Fa_MAKE_OBJ_UPVALUE(&Stack_[stack_pos]);
    OpenUpvalues_.push(uv);
    OpenUpvalueCount_ = static_cast<int>(OpenUpvalues_.size());
    return uv;
}

void Fa_VM::closeUpvalues(unsigned int from_stack_pos)
{
    if (OpenUpvalues_.empty())
        return;

    Fa_Value* threshold = &Stack_[from_stack_pos];
    auto i = static_cast<int>(OpenUpvalues_.size());

    while (i > 0 && OpenUpvalues_[i - 1]->location >= threshold) {
        ObjUpvalue* uv = OpenUpvalues_[--i];
        uv->closed = *uv->location;
        uv->location = &uv->closed;
    }

    OpenUpvalues_.resize(static_cast<u32>(i));
    OpenUpvalueCount_ = i;
}

// Close all upvalues captured within a specific call frame.
void Fa_VM::closeUpvaluesForFrame(Fa_CallFrame const& f)
{
    closeUpvalues(static_cast<unsigned int>(f.base));
}

Fa_ObjString* Fa_VM::intern(Fa_StringRef const& str)
{
    if (Fa_ObjString** existing = StringTable_.findPtr(str))
        return *existing;

    Fa_ObjString* obj = Fa_MAKE_OBJ_STRING(str);
    StringTable_.insertOrAssign(str, obj);
    return obj;
}

void Fa_VM::internChunkConstants(Fa_Chunk* ch)
{
    if (!ch)
        return;

    for (u32 i = 0; i < ch->constants.size(); ++i) {
        if (Fa_IS_STRING(ch->constants[i]))
            ch->constants[i] = Fa_MAKE_OBJECT(intern(Fa_AS_STRING(ch->constants[i])->str));
    }

    for (auto* fn : ch->functions)
        internChunkConstants(fn);
}

void Fa_VM::openStdlib()
{
    registerNative("حجم", &Fa_VM::Fa_len, -1);
    registerNative("اضف", &Fa_VM::Fa_append, -1);
    registerNative("احذف", &Fa_VM::Fa_pop, -1);
    registerNative("مقطع", &Fa_VM::Fa_slice, -1);
    registerNative("اكتب", &Fa_VM::Fa_print, -1);
    registerNative("input", &Fa_VM::Fa_input, -1);
    registerNative("نوع", &Fa_VM::Fa_type, -1);
    registerNative("طبيعي", &Fa_VM::Fa_int, -1);
    registerNative("حقيقي", &Fa_VM::Fa_float, -1);
    registerNative("str", &Fa_VM::Fa_str, -1);
    registerNative("bool", &Fa_VM::Fa_bool, -1);
    registerNative("قائمة", &Fa_VM::Fa_list, -1);
    registerNative("اقسم", &Fa_VM::Fa_split, -1);
    registerNative("join", &Fa_VM::Fa_join, -1);
    registerNative("substr", &Fa_VM::Fa_substr, -1);
    registerNative("contains", &Fa_VM::Fa_contains, -1);
    registerNative("trim", &Fa_VM::Fa_trim, -1);
    registerNative("floor", &Fa_VM::Fa_floor, -1);
    registerNative("ceil", &Fa_VM::Fa_ceil, -1);
    registerNative("round", &Fa_VM::Fa_round, -1);
    registerNative("abs", &Fa_VM::Fa_abs, -1);
    registerNative("min", &Fa_VM::Fa_min, -1);
    registerNative("max", &Fa_VM::Fa_max, -1);
    registerNative("pow", &Fa_VM::Fa_pow, -1);
    registerNative("sqrt", &Fa_VM::Fa_sqrt, -1);
    registerNative("assert", &Fa_VM::Fa_assert, -1);
    registerNative("clock", &Fa_VM::Fa_clock, -1);
    registerNative("error", &Fa_VM::Fa_error, -1);
    registerNative("time", &Fa_VM::Fa_time, -1);
}

void Fa_VM::registerNative(Fa_StringRef const& name, NativeFn fn, int arity)
{
    Fa_ObjString* name_obj = Fa_MAKE_OBJ_STRING(name);
    Fa_Value val = Fa_MAKE_NATIVE(fn, name_obj, arity);
    if (u32* slot = GlobalIndex_.findPtr(name)) {
        GlobalSlots_[*slot] = val;
    } else {
        auto slot_idx = GlobalSlots_.size();
        GlobalSlots_.push(val);
        GlobalIndex_.insertOrAssign(name, slot_idx);
    }
}

Fa_SourceLocation Fa_VM::currentLocation() const
{
    if (FramesTop_ == 0)
        return { 0, 0, 0 };

    Fa_CallFrame const& f = topFrame();
    size_t off = f.ip > 0 ? f.ip - 1 : 0;
    Fa_Chunk const& ch = *f.chunk;

    if (off < ch.locations.size())
        return ch.locations[off];

    return { 0, 0, 0 };
}

void Fa_VM::runtimeError(ErrorCode code)
{
    Fa_SourceLocation loc = currentLocation();
    diagnostic::report(
        diagnostic::Severity::ERROR,
        loc.line, loc.column,
        static_cast<u16>(code));

    Fa_StringRef fname = "";
    Fa_SourceLocation floc = { 0, 0, 0 };
    for (int i = FramesTop_ - 1; i >= 0; --i) {
        Fa_CallFrame* p = &Frames_[i];
        if (!p->chunk)
            continue;
        Fa_Chunk const& ch = *p->chunk;
        size_t off = p->ip > 0 ? p->ip - 1 : 0;
        if (off >= ch.locations.size())
            continue;

        floc = ch.locations[off];
        fname = (p->closure && p->closure->function && p->closure->function->name)
            ? p->closure->function->name->str
            : "";
    }
    diagnostic::engine.addNote(
        floc.line,
        (fname.empty() ? "" : ("in '" + std::string(fname.data(), fname.len()))) + "' at " + std::to_string(floc.line) + ":" + std::to_string(floc.column));
    diagnostic::dump();
    halt();
}

void Fa_VM::halt()
{
    closeUpvalues(0);
    FramesTop_ = 0;
    StackTop_ = 0;
    throw std::runtime_error("");
}

Fa_VM::Fa_CallFrame& Fa_VM::frame() { return Frames_[FramesTop_ - 1]; }
Fa_VM::Fa_CallFrame const& Fa_VM::frame() const { return Frames_[FramesTop_ - 1]; }

Fa_Chunk* Fa_VM::chunk() { return topFrame().chunk; }
Fa_Value& Fa_VM::reg(int r) { return Stack_[topFrame().base + r]; }

void Fa_VM::updateIcBinary(Fa_Chunk* ch, u32 nop_ip, Fa_Value lhs, Fa_Value rhs, Fa_Value result)
{
    if (!ch)
        return;
    u32 nop = ch->code[nop_ip];
    u8 ic_idx = Fa_instr_A(nop);
    if (ic_idx < ch->icSlots.size()) {
        auto& slot = ch->icSlots[ic_idx];
        slot.seenLhs |= static_cast<u8>(valueTypeTag(lhs));
        slot.seenRhs |= static_cast<u8>(valueTypeTag(rhs));
        slot.seenRet |= static_cast<u8>(valueTypeTag(result));
        slot.hitCount++;
    }
}

} // namespace fairuz::runtime
