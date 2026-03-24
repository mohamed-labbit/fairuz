//
// vm.cc
// 

#include "../include/vm.hpp"
#include "value.hpp"

#define DISPATCH()                                                   \
    do {                                                             \
        instr = cur_chunk->code[ip++];                               \
        goto* dispatch_table[static_cast<uint8_t>(instr_op(instr))]; \
    } while (0)

#define BEGIN_DISPATCH() DISPATCH()
#define END_DISPATCH()
#define CASE(op) H_##op:

#define TABLE_INIT               \
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

#define VMOP_II(lhs, rhs, op) AS_INTEGER(lhs) op AS_INTEGER(rhs)
#define VMOP_FF(lhs, rhs, op) AS_DOUBLE(lhs) op AS_DOUBLE(rhs)

#define VM_ADDI(lhs, rhs) VMOP_II(lhs, rhs, +)
#define VM_SUBI(lhs, rhs) VMOP_II(lhs, rhs, -)
#define VM_MULI(lhs, rhs) VMOP_II(lhs, rhs, *)
#define VM_DIVI(lhs, rhs) VMOP_II(lhs, rhs, /)
#define VM_ADDF(lhs, rhs) VMOP_FF(lhs, rhs, +)
#define VM_SUBF(lhs, rhs) VMOP_FF(lhs, rhs, -)
#define VM_MULF(lhs, rhs) VMOP_FF(lhs, rhs, *)
#define VM_DIVF(lhs, rhs) VMOP_FF(lhs, rhs, /)

#define VM_ABC(op) cur_chunk->code[ip - 1] = make_ABC(op, a, b, c)

#define RA() cur_base[instr_A(instr)]
#define RB() cur_base[instr_B(instr)]
#define RC() cur_base[instr_C(instr)]

#define LOAD_FRAME()                \
    do {                            \
        CallFrame& f = frame();     \
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
            Value* threshold = &Stack_[cur_frame_base];                    \
            uint32_t i = static_cast<uint32_t>(OpenUpvalues_.size());      \
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

#define RECORD_BINARY_IC(lhs, rhs, result)                                 \
    do {                                                                   \
        if (ip < cur_chunk->code.size()                                    \
            && instr_op(cur_chunk->code[ip]) == OpCode::NOP)               \
            updateIcBinary(cur_chunk, ip, lhs, rhs, result);               \
    } while (0)

namespace mylang::runtime {

static void checkStackIndex(int index, int stack_size, char const* context)
{
    if (index < 0 || index >= stack_size) {
        // This is a VM internal error, not a user error.
        throw std::logic_error(
            std::string("VM internal error: stack index ") + std::to_string(index) + " out of range [0," + std::to_string(stack_size) + ") in " + context);
    }
}

VM::VM()
{
    std::fill(Stack_, Stack_ + STACK_SIZE, NIL_VAL);
    std::fill(Frames_, Frames_ + MAX_FRAMES, CallFrame());
    
    openStdlib();
}

VM::~VM()
{
    GC_.sweepAll();
}

// How many value slots are currently in use.
int VM::stackSize() const { return StackTop_; }

// How many call frames are active.
int VM::frameDepth() const { return FramesTop_; }

// Push a value, checking for overflow.
void VM::pushValue(Value v)
{
    if (StackTop_ == STACK_SIZE)
        runtimeError(ErrorCode::STACK_OVERFLOW);
    
    Stack_[StackTop_] = v;
    StackTop_++;
}

// Pop a value, checking for underflow.
Value VM::popValue()
{
    if (StackTop_ == 0)
        runtimeError(ErrorCode::STACK_UNDERFLOW);
    
    --StackTop_;
    return Stack_[StackTop_];
}

// Read a value at an absolute stack index.
Value& VM::stackAt(int index)
{
    checkStackIndex(index, STACK_SIZE, "stackAt");
    return Stack_[index];
}

// Ensure the value stack has at least `needed` slots allocated, filling
// any new slots with NIL.  Checks against the hard cap.
void VM::ensureStackSlots(int needed)
{
    if (needed > STACK_SIZE)
        runtimeError(ErrorCode::STACK_OVERFLOW);
    
    while (StackTop_ < needed)
        pushValue(NIL_VAL);
}

// Push a call frame, checking for overflow.
void VM::pushFrame(CallFrame const& f)
{
    if (FramesTop_ == MAX_FRAMES)
        runtimeError(ErrorCode::STACK_OVERFLOW);
    
    Frames_[FramesTop_] = f;
    FramesTop_++;
}

// Pop a call frame, checking for underflow.
void VM::popFrame()
{
    if (FramesTop_ == 0)
        runtimeError(ErrorCode::STACK_UNDERFLOW);
    
    --FramesTop_;
}

// Reference to the topmost call frame.
VM::CallFrame& VM::topFrame()
{
    assert(FramesTop_ > 0 && "topFrame called with empty frame stack");
    return Frames_[FramesTop_ - 1];
}

VM::CallFrame const& VM::topFrame() const
{
    assert(FramesTop_ > 0 && "topFrame called with empty frame stack");
    return Frames_[FramesTop_ - 1];
}

Value& VM::getReg(CallFrame const& f, int reg)
{
    int abs = f.base + reg;
    checkStackIndex(abs, STACK_SIZE, "getReg");
    return Stack_[abs];
}

static void requireNumber(Value v, VM* vm)
{
    if (!IS_NUMBER(v))
        vm->runtimeError(ErrorCode::TYPE_ERROR_ARITH);
}

static void requireInteger(Value v, VM* vm)
{
    if (!IS_INTEGER(v))
        vm->runtimeError(ErrorCode::TYPE_ERROR_ARITH);
}

Value VM::run(Chunk* chunk)
{
    if (!chunk)
        return NIL_VAL;

    StackTop_ = 0;
    FramesTop_ = 0;
    OpenUpvalueCount_ = 0;
    OpenUpvalues_.clear();

    ObjFunction* fn = MAKE_OBJ_FUNCTION();
    fn->chunk = chunk;
    ObjClosure* cl = MAKE_OBJ_CLOSURE(fn);

    Stack_[0] = MAKE_OBJECT(cl);
    StackTop_ = static_cast<int>(chunk->localCount) + 2;

    if (FramesTop_ >= MAX_FRAMES || StackTop_ >= STACK_SIZE)
        runtimeError(ErrorCode::STACK_OVERFLOW);

    Frames_[FramesTop_++] = CallFrame(cl, chunk, 0, 1, static_cast<int>(chunk->localCount));
    internChunkConstants(cl->function->chunk);

    if (GC_.currentMemory() >= GC_THRESHOLD)
        GC_.collect(this);

    return execute();
}

Value VM::execute()
{
    static void const* dispatch_table[] = { TABLE_INIT };

    if (frameDepth() == 0)
        return NIL_VAL;

    uint32_t instr;
    int has_open_upvalues = OpenUpvalueCount_;
    Chunk* cur_chunk = frame().chunk;
    ObjClosure* cur_closure = frame().closure;
    int cur_frame_base = frame().base;
    Value* cur_base = &Stack_[cur_frame_base];
    uint32_t ip = frame().ip;

    BEGIN_DISPATCH();

    CASE(LOAD_NIL)
    {
        uint8_t start = instr_B(instr);
        uint8_t count = instr_C(instr);
        for (uint8_t i = 0; i < count; ++i)
            cur_base[start + i] = NIL_VAL;
        DISPATCH();
    }
    CASE(LOAD_TRUE)
    {
        RA() = MAKE_BOOL(true);
        DISPATCH();
    }
    CASE(LOAD_FALSE)
    {
        RA() = MAKE_BOOL(false);
        DISPATCH();
    }
    CASE(LOAD_CONST)
    {
        RA() = cur_chunk->constants[instr_Bx(instr)];
        DISPATCH();
    }
    CASE(LOAD_INT)
    {
        RA() = MAKE_INTEGER(static_cast<int32_t>(static_cast<uint16_t>(instr_Bx(instr))) - JUMP_OFFSET);
        DISPATCH();
    }
    CASE(LOAD_GLOBAL)
    {
        {
            Value name_v = cur_chunk->constants[instr_Bx(instr)];
            StringRef name = AS_STRING(name_v)->str;
            uint32_t slot_idx;
            if (uint32_t* slot = GlobalIndex_.findPtr(name)) {
                slot_idx = *slot;
            } else {
                slot_idx = static_cast<uint32_t>(GlobalSlots_.size());
                GlobalSlots_.push(NIL_VAL);
                GlobalIndex_.insertOrAssign(name, slot_idx);
            }
            RA() = GlobalSlots_[slot_idx];
            cur_chunk->code[ip - 1] = make_ABx(OpCode::LOAD_GLOBAL_CACHED, instr_A(instr), slot_idx);
        }
        DISPATCH();
    }
    CASE(LOAD_GLOBAL_CACHED)
    {
        RA() = GlobalSlots_[instr_Bx(instr)];
        DISPATCH();
    }
    CASE(STORE_GLOBAL)
    {
        {
            Value name_v = cur_chunk->constants[instr_Bx(instr)];
            StringRef name = AS_STRING(name_v)->str;
            uint32_t slot_idx;
            if (uint32_t* slot = GlobalIndex_.findPtr(name)) {
                slot_idx = *slot;
            } else {
                slot_idx = static_cast<uint32_t>(GlobalSlots_.size());
                GlobalSlots_.push(NIL_VAL);
                GlobalIndex_.insertOrAssign(name, slot_idx);
            }
            GlobalSlots_[slot_idx] = RA();
            cur_chunk->code[ip - 1] = make_ABx(OpCode::STORE_GLOBAL_CACHED, instr_A(instr), slot_idx);
        }
        DISPATCH();
    }
    CASE(STORE_GLOBAL_CACHED)
    {
        GlobalSlots_[instr_Bx(instr)] = RA();
        DISPATCH();
    }
    CASE(MOVE)
    {
        RA() = RB();
        DISPATCH();
    }
    CASE(GET_UPVALUE)
    {
        RA() = *cur_closure->upValues[instr_B(instr)]->location;
        DISPATCH();
    }
    CASE(SET_UPVALUE)
    {
        *cur_closure->upValues[instr_B(instr)]->location = RA();
        DISPATCH();
    }
    CASE(CLOSE_UPVALUE)
    {
        closeUpvalues(cur_frame_base + instr_A(instr));
        has_open_upvalues = OpenUpvalueCount_;
        DISPATCH();
    }
    CASE(OP_ADD)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (UNLIKELY(!IS_NUMBER(lhs) || !IS_NUMBER(rhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        if (IS_INTEGER(lhs) && IS_INTEGER(rhs)) {
            cur_base[a] = MAKE_INTEGER(VM_ADDI(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_ADD_II);
        } else {
            cur_base[a] = MAKE_REAL(VM_ADDF(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_ADD_FF);
        }
        DISPATCH();
    }
    CASE(OP_ADD_II)
    {
        RA() = MAKE_INTEGER(VM_ADDI(RB(), RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_ADD_FF)
    {
        RA() = MAKE_REAL(VM_ADDF(RB(), RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_ADD_RI)
    {
        Value lhs = RB();
        int8_t imm = static_cast<int8_t>(instr_C(instr) - 128);
        RA() = LIKELY(IS_INTEGER(lhs)) ? MAKE_INTEGER(AS_INTEGER(lhs) + imm) : MAKE_REAL(AS_DOUBLE(lhs) + imm);
        DISPATCH();
    }
    CASE(OP_SUB)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (UNLIKELY(!IS_NUMBER(lhs) || !IS_NUMBER(rhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        if (IS_INTEGER(lhs) && IS_INTEGER(rhs)) {
            cur_base[a] = MAKE_INTEGER(VM_SUBI(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_SUB_II);
        } else {
            cur_base[a] = MAKE_REAL(VM_SUBF(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_SUB_FF);
        }
        DISPATCH();
    }
    CASE(OP_SUB_II)
    {
        RA() = MAKE_INTEGER(VM_SUBI(RB(), RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_SUB_FF)
    {
        RA() = MAKE_REAL(VM_SUBF(RB(), RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_SUB_RI)
    {
        Value lhs = RB();
        int8_t imm = static_cast<int8_t>(instr_C(instr) - 128);
        RA() = LIKELY(IS_INTEGER(lhs)) ? MAKE_INTEGER(AS_INTEGER(lhs) - imm) : MAKE_REAL(AS_DOUBLE(lhs) - imm);
        DISPATCH();
    }
    CASE(OP_MUL)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (UNLIKELY(!IS_NUMBER(lhs) || !IS_NUMBER(rhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        if (IS_INTEGER(lhs) && IS_INTEGER(rhs)) {
            cur_base[a] = MAKE_INTEGER(VM_MULI(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_MUL_II);
        } else {
            cur_base[a] = MAKE_REAL(VM_MULF(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_MUL_FF);
        }
        DISPATCH();
    }
    CASE(OP_MUL_II)
    {
        RA() = MAKE_INTEGER(VM_MULI(RB(), RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_MUL_FF)
    {
        RA() = MAKE_REAL(VMOP_FF(RB(), RC(), *));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_MUL_RI)
    {
        Value lhs = RB();
        int8_t imm = static_cast<int8_t>(instr_C(instr) - 128);
        RA() = LIKELY(IS_INTEGER(lhs)) ? MAKE_INTEGER(AS_INTEGER(lhs) * imm) : MAKE_REAL(AS_DOUBLE(lhs) * imm);
        DISPATCH();
    }
    CASE(OP_DIV)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (UNLIKELY(!IS_NUMBER(lhs) || !IS_NUMBER(rhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        if (AS_DOUBLE_ANY(rhs) == 0.0)
            runtimeError(ErrorCode::DIVISION_BY_ZERO);
        if (IS_INTEGER(lhs) && IS_INTEGER(rhs)) {
            cur_base[a] = MAKE_INTEGER(VM_DIVI(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_DIV_II);
        } else {
            cur_base[a] = MAKE_REAL(VM_DIVF(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_DIV_FF);
        }
        DISPATCH();
    }
    CASE(OP_DIV_II)
    {
        RA() = MAKE_INTEGER(VM_DIVI(RB(), RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_DIV_FF)
    {
        RA() = MAKE_REAL(VM_DIVF(RB(), RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_MOD)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (UNLIKELY(!IS_NUMBER(lhs) || !IS_NUMBER(rhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        if (AS_DOUBLE_ANY(rhs) == 0.0)
            runtimeError(ErrorCode::MODULO_BY_ZERO);
        if (IS_INTEGER(lhs) && IS_INTEGER(rhs)) {
            cur_base[a] = MAKE_REAL(static_cast<double>(VMOP_II(lhs, rhs, %)));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_MOD_II);
        } else {
            cur_base[a] = MAKE_REAL(std::fmod(AS_DOUBLE_ANY(lhs), AS_DOUBLE_ANY(rhs)));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_MOD_FF);
        }
        DISPATCH();
    }
    CASE(OP_MOD_II)
    {
        RA() = MAKE_REAL(static_cast<double>(VMOP_II(RB(), RC(), %)));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_MOD_FF)
    {
        RA() = MAKE_REAL(std::fmod(AS_DOUBLE_ANY(RB()), AS_DOUBLE_ANY(RC())));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_POW)
    {
        Value lhs = RB(), rhs = RC();
        requireNumber(lhs, this);
        requireNumber(rhs, this);
        RA() = MAKE_REAL(std::pow(AS_DOUBLE_ANY(lhs), AS_DOUBLE_ANY(rhs)));
        DISPATCH();
    }
    CASE(OP_NEG)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value operand = cur_base[b];
        if (UNLIKELY(!IS_NUMBER(operand)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        if (IS_INTEGER(operand)) {
            cur_base[a] = MAKE_INTEGER(-AS_INTEGER(operand));
            VM_ABC(OpCode::OP_NEG_I);
        } else {
            cur_base[a] = MAKE_REAL(-AS_DOUBLE_ANY(operand));
            VM_ABC(OpCode::OP_NEG_F);
        }
        DISPATCH();
    }
    CASE(OP_NEG_I)
    {
        RA() = MAKE_INTEGER(-AS_INTEGER(RB()));
        DISPATCH();
    }
    CASE(OP_NEG_F)
    {
        RA() = MAKE_REAL(-AS_DOUBLE_ANY(RB()));
        DISPATCH();
    }
    CASE(OP_BITAND)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (UNLIKELY(!IS_INTEGER(lhs) || !IS_INTEGER(rhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        cur_base[a] = MAKE_INTEGER(VMOP_II(lhs, rhs, &));
        VM_ABC(OpCode::OP_BITAND_I);
        DISPATCH();
    }
    CASE(OP_BITAND_I)
    {
        RA() = MAKE_INTEGER(VMOP_II(RB(), RC(), &));
        DISPATCH();
    }
    CASE(OP_BITOR)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (UNLIKELY(!IS_INTEGER(lhs) || !IS_INTEGER(rhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        cur_base[a] = MAKE_INTEGER(VMOP_II(lhs, rhs, |));
        VM_ABC(OpCode::OP_BITOR_I);
        DISPATCH();
    }
    CASE(OP_BITOR_I)
    {
        RA() = MAKE_INTEGER(VMOP_II(RB(), RC(), |));
        DISPATCH();
    }
    CASE(OP_BITXOR)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (UNLIKELY(!IS_INTEGER(lhs) || !IS_INTEGER(rhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        cur_base[a] = MAKE_INTEGER(VMOP_II(lhs, rhs, ^));
        VM_ABC(OpCode::OP_BITXOR_I);
        DISPATCH();
    }
    CASE(OP_BITXOR_I)
    {
        RA() = MAKE_INTEGER(VMOP_II(RB(), RC(), ^));
        DISPATCH();
    }
    CASE(OP_BITNOT)
    {
        Value operand = RB();
        if (UNLIKELY(!IS_INTEGER(operand)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        RA() = MAKE_INTEGER(~AS_INTEGER(operand));
        DISPATCH();
    }
    CASE(OP_LSHIFT)
    {
        Value lhs = RB();
        if (UNLIKELY(!IS_INTEGER(lhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        uint8_t imm = instr_C(instr);
        if (imm > 64)
            runtimeError(ErrorCode::INDEX_TYPE_ERROR);
        RA() = MAKE_INTEGER(static_cast<int64_t>(static_cast<uint64_t>(AS_INTEGER(lhs)) << imm));
        DISPATCH();
    }
    CASE(OP_RSHIFT)
    {
        Value lhs = RB();
        if (UNLIKELY(!IS_INTEGER(lhs)))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        uint8_t imm = instr_C(instr);
        if (imm > 64)
            runtimeError(ErrorCode::INDEX_TYPE_ERROR);
        uint64_t shifted = static_cast<uint64_t>(AS_INTEGER(lhs)) >> imm;
        if (AS_INTEGER(lhs) < 0)
            RA() = MAKE_REAL(static_cast<double>(shifted));
        else
            RA() = MAKE_INTEGER(static_cast<int64_t>(shifted));
        DISPATCH();
    }
    CASE(OP_EQ)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (IS_INTEGER(lhs) && IS_INTEGER(rhs)) {
            cur_base[a] = MAKE_BOOL(VMOP_II(lhs, rhs, ==));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_EQ_II);
        } else if (IS_STRING(lhs) && IS_STRING(rhs)) {
            cur_base[a] = MAKE_BOOL(AS_STRING(lhs)->str == AS_STRING(rhs)->str);
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_EQ_SS);
        } else {
            cur_base[a] = MAKE_BOOL(AS_DOUBLE_ANY(lhs) == AS_DOUBLE_ANY(rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_EQ_FF);
        }
        DISPATCH();
    }
    CASE(OP_EQ_II)
    {
        RA() = MAKE_BOOL(VMOP_II(RB(), RC(), ==));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_EQ_FF)
    {
        RA() = MAKE_BOOL(AS_DOUBLE_ANY(RB()) == AS_DOUBLE_ANY(RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_EQ_SS)
    {
        RA() = MAKE_BOOL(AS_STRING(RB())->str == AS_STRING(RC())->str);
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_EQ_RI)
    {
        Value lhs = RB();
        int8_t imm = static_cast<int8_t>(instr_C(instr) - 128);
        if (LIKELY(IS_INTEGER(lhs)))
            RA() = MAKE_BOOL(AS_INTEGER(lhs) == static_cast<int64_t>(imm));
        else {
            if (UNLIKELY(!IS_DOUBLE(lhs)))
                runtimeError(ErrorCode::TYPE_ERROR_ARITH);
            RA() = MAKE_BOOL(AS_DOUBLE(lhs) == static_cast<double>(imm));
        }
        DISPATCH();
    }
    CASE(OP_NEQ)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (IS_INTEGER(lhs) && IS_INTEGER(rhs)) {
            cur_base[a] = MAKE_BOOL(VMOP_II(lhs, rhs, !=));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_NEQ_II);
        } else if (IS_STRING(lhs) && IS_STRING(rhs)) {
            cur_base[a] = MAKE_BOOL(AS_STRING(lhs)->str != AS_STRING(rhs)->str);
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_NEQ_SS);
        } else {
            cur_base[a] = MAKE_BOOL(AS_DOUBLE_ANY(lhs) != AS_DOUBLE_ANY(rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_NEQ_FF);
        }
        DISPATCH();
    }
    CASE(OP_NEQ_II)
    {
        RA() = MAKE_BOOL(VMOP_II(RB(), RC(), !=));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_NEQ_FF)
    {
        RA() = MAKE_BOOL(AS_DOUBLE_ANY(RB()) != AS_DOUBLE_ANY(RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_NEQ_SS)
    {
        RA() = MAKE_BOOL(AS_STRING(RB())->str != AS_STRING(RC())->str);
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_LT)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (IS_INTEGER(lhs) && IS_INTEGER(rhs)) {
            cur_base[a] = MAKE_BOOL(VMOP_II(lhs, rhs, <));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_LT_II);
        } else if (IS_STRING(lhs) && IS_STRING(rhs)) {
            cur_base[a] = MAKE_BOOL(AS_STRING(lhs)->str < AS_STRING(rhs)->str);
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_LT_SS);
        } else {
            cur_base[a] = MAKE_BOOL(AS_DOUBLE_ANY(lhs) < AS_DOUBLE_ANY(rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_LT_FF);
        }
        DISPATCH();
    }
    CASE(OP_LT_II)
    {
        RA() = MAKE_BOOL(VMOP_II(RB(), RC(), <));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_LT_FF)
    {
        RA() = MAKE_BOOL(AS_DOUBLE_ANY(RB()) < AS_DOUBLE_ANY(RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_LT_SS)
    {
        RA() = MAKE_BOOL(AS_STRING(RB())->str < AS_STRING(RC())->str);
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_LT_RI)
    {
        Value lhs = RB();
        int8_t imm = static_cast<int8_t>(instr_C(instr) - 128);
        if (LIKELY(IS_INTEGER(lhs)))
            RA() = MAKE_BOOL(AS_INTEGER(lhs) < static_cast<int64_t>(imm));
        else {
            if (UNLIKELY(!IS_DOUBLE(lhs)))
                runtimeError(ErrorCode::TYPE_ERROR_ARITH);
            RA() = MAKE_BOOL(AS_DOUBLE(lhs) < static_cast<double>(imm));
        }
        DISPATCH();
    }
    CASE(OP_LTE)
    {
        uint8_t a = instr_A(instr), b = instr_B(instr), c = instr_C(instr);
        Value lhs = cur_base[b], rhs = cur_base[c];
        if (IS_INTEGER(lhs) && IS_INTEGER(rhs)) {
            cur_base[a] = MAKE_BOOL(VMOP_II(lhs, rhs, <=));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_LTE_II);
        } else if (IS_STRING(lhs) && IS_STRING(rhs)) {
            cur_base[a] = MAKE_BOOL(AS_STRING(lhs)->str <= AS_STRING(rhs)->str);
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_LTE_SS);
        } else {
            cur_base[a] = MAKE_BOOL(AS_DOUBLE_ANY(lhs) <= AS_DOUBLE_ANY(rhs));
            RECORD_BINARY_IC(lhs, rhs, cur_base[a]);
            VM_ABC(OpCode::OP_LTE_FF);
        }
        DISPATCH();
    }
    CASE(OP_LTE_II)
    {
        RA() = MAKE_BOOL(VMOP_II(RB(), RC(), <=));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_LTE_FF)
    {
        RA() = MAKE_BOOL(AS_DOUBLE_ANY(RB()) <= AS_DOUBLE_ANY(RC()));
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_LTE_SS)
    {
        RA() = MAKE_BOOL(AS_STRING(RB())->str <= AS_STRING(RC())->str);
        RECORD_BINARY_IC(RB(), RC(), RA());
        DISPATCH();
    }
    CASE(OP_LTE_RI)
    {
        Value lhs = RB();
        int8_t imm = static_cast<int8_t>(instr_C(instr) - 128);
        if (LIKELY(IS_INTEGER(lhs)))
            RA() = MAKE_BOOL(AS_INTEGER(lhs) <= static_cast<int64_t>(imm));
        else {
            if (UNLIKELY(!IS_DOUBLE(lhs)))
                runtimeError(ErrorCode::TYPE_ERROR_ARITH);
            RA() = MAKE_BOOL(AS_DOUBLE(lhs) <= static_cast<double>(imm));
        }
        DISPATCH();
    }
    CASE(OP_NOT)
    {
        RA() = MAKE_BOOL(!IS_TRUTHY(RB()));
        DISPATCH();
    }
    CASE(CONCAT) { DISPATCH(); }
    CASE(LIST_NEW)
    {
        ObjList* list_obj = MAKE_OBJ_LIST();
        list_obj->reserve(instr_B(instr));
        RA() = MAKE_OBJECT(list_obj);
        DISPATCH();
    }
    CASE(LIST_APPEND)
    {
        Value list_v = RA();
        if (!IS_LIST(list_v))
            runtimeError(ErrorCode::TYPE_ERROR_CALL);
        AS_LIST(list_v)->elements.push(RB());
        DISPATCH();
    }
    CASE(LIST_GET)
    {
        Value list_v = RB(), index_v = RC();
        if (!IS_LIST(list_v))
            runtimeError(ErrorCode::TYPE_ERROR_CALL);
        if (!IS_INTEGER(index_v))
            runtimeError(ErrorCode::INDEX_TYPE_ERROR);
        auto& elems = AS_LIST(list_v)->elements;
        int64_t idx = AS_INTEGER(index_v);
        if (idx < 0 || idx >= static_cast<int64_t>(elems.size()))
            runtimeError(ErrorCode::INDEX_OUT_OF_BOUNDS);
        RA() = elems[static_cast<uint32_t>(idx)];
        DISPATCH();
    }
    CASE(LIST_SET)
    {
        Value list_v = RA(), index_v = RB(), new_val = RC();
        if (!IS_LIST(list_v))
            runtimeError(ErrorCode::TYPE_ERROR_CALL);
        if (!IS_INTEGER(index_v))
            runtimeError(ErrorCode::INDEX_TYPE_ERROR);
        auto& elems = AS_LIST(list_v)->elements;
        int64_t idx = AS_INTEGER(index_v);
        if (idx < 0)
            idx += static_cast<int64_t>(elems.size());
        if (idx < 0 || idx >= static_cast<int64_t>(elems.size()))
            runtimeError(ErrorCode::INDEX_OUT_OF_BOUNDS);
        elems[static_cast<uint32_t>(idx)] = new_val;
        DISPATCH();
    }
    CASE(LIST_LEN)
    {
        Value list_v = RB();
        if (!IS_LIST(list_v))
            runtimeError(ErrorCode::TYPE_ERROR_CALL);
        RA() = MAKE_INTEGER(static_cast<int64_t>(AS_LIST(list_v)->elements.size()));
        DISPATCH();
    }
    CASE(JUMP)
    {
        ip += instr_sBx(instr);
        DISPATCH();
    }
    CASE(JUMP_IF_TRUE)
    {
        if (IS_TRUTHY(RA()))
            ip += instr_sBx(instr);
        DISPATCH();
    }
    CASE(JUMP_IF_FALSE)
    {
        if (!IS_TRUTHY(RA()))
            ip += instr_sBx(instr);
        DISPATCH();
    }
    CASE(LOOP)
    {
        ip += instr_sBx(instr);
        DISPATCH();
    }
    CASE(FOR_PREP)
    {
        uint8_t base_reg = instr_A(instr);
        Value init_v = cur_base[base_reg];
        Value limit_v = cur_base[base_reg + 1];
        Value step_v = cur_base[base_reg + 2];
        if (!IS_INTEGER(init_v) || !IS_INTEGER(limit_v) || !IS_INTEGER(step_v))
            runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        int64_t init = AS_INTEGER(init_v);
        int64_t limit = AS_INTEGER(limit_v);
        int64_t step = AS_INTEGER(step_v);
        if (step == 0)
            runtimeError(ErrorCode::DIVISION_BY_ZERO);
        cur_base[base_reg] = MAKE_INTEGER(limit);
        cur_base[base_reg + 1] = MAKE_INTEGER(step);
        cur_base[base_reg + 2] = MAKE_INTEGER(init);
        bool enters = (step > 0) ? (init <= limit) : (init >= limit);
        if (!enters)
            ip += instr_sBx(instr);
        DISPATCH();
    }
    CASE(FOR_STEP)
    {
        uint8_t base_reg = instr_A(instr);
        Value limit_v = cur_base[base_reg];
        Value step_v = cur_base[base_reg + 1];
        Value control_v = cur_base[base_reg + 2];
        if (IS_INTEGER(control_v)) {
            int64_t control = AS_INTEGER(control_v) + AS_INTEGER(step_v);
            cur_base[base_reg + 2] = MAKE_INTEGER(control);
            bool continues = (AS_INTEGER(step_v) > 0)
                ? (control <= AS_INTEGER(limit_v))
                : (control >= AS_INTEGER(limit_v));
            if (continues)
                ip += instr_sBx(instr);
        } else {
            double control = AS_DOUBLE_ANY(control_v) + AS_DOUBLE_ANY(step_v);
            cur_base[base_reg + 2] = MAKE_REAL(control);
            bool continues = (AS_DOUBLE(step_v) > 0.0)
                ? (control <= AS_DOUBLE(limit_v))
                : (control >= AS_DOUBLE(limit_v));
            if (continues)
                ip += instr_sBx(instr);
        }
        DISPATCH();
    }
    CASE(CLOSURE)
    {
        uint16_t fn_idx = instr_Bx(instr);
        Chunk* fn_chunk = cur_chunk->functions[fn_idx];
        ObjFunction* fn = MAKE_OBJ_FUNCTION(fn_chunk);
        fn->arity = fn_chunk->arity;
        fn->upvalueCount = fn_chunk->upvalueCount;
        ObjClosure* cl = MAKE_OBJ_CLOSURE(fn);
        for (unsigned int i = 0; i < fn_chunk->upvalueCount; ++i) {
            uint32_t desc = cur_chunk->code[ip++];
            bool is_local = instr_A(desc) == 1;
            uint8_t index = instr_B(desc);
            if (is_local) {
                cl->upValues[i] = captureUpvalue(cur_frame_base + index);
                has_open_upvalues = OpenUpvalueCount_;
            } else {
                cl->upValues[i] = cur_closure->upValues[index];
            }
        }
        RA() = MAKE_OBJECT(cl);
        DISPATCH();
    }
    CASE(CALL)
    {
        uint8_t fn_reg = instr_A(instr);
        uint8_t argc = instr_B(instr);
        Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;
        if (IS_CLOSURE(callee)) {
            ObjClosure* cl = AS_CLOSURE(callee);
            Chunk* fchk = cl->function->chunk;
            if (UNLIKELY(argc != fchk->arity))
                runtimeError(ErrorCode::WRONG_ARG_COUNT);
            if (UNLIKELY(FramesTop_ >= MAX_FRAMES))
                runtimeError(ErrorCode::STACK_OVERFLOW);
            int local_count = fchk->localCount;
            int new_top = base + local_count + 1;
            while (StackTop_ < new_top)
                Stack_[StackTop_++] = NIL_VAL;
            SAVE_IP();
            Frames_[FramesTop_++] = CallFrame(cl, fchk, 0, base, local_count);
        } else {
            SAVE_IP();
            callValue(callee, argc, base, false);
        }
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(CALL_TAIL)
    {
        uint8_t fn_reg = instr_A(instr);
        uint8_t argc = instr_B(instr);
        Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;
        SAVE_IP();
        callValue(callee, argc, base, true);
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(IC_CALL)
    {
        uint8_t fn_reg = instr_A(instr);
        uint8_t argc = instr_B(instr);
        uint8_t ic_idx = instr_C(instr);
        uint32_t call_ip = ip - 1;
        Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;
        bool has_slot = ic_idx < cur_chunk->icSlots.size();
        if (has_slot) {
            auto& slot = cur_chunk->icSlots[ic_idx];
            slot.seenLhs |= static_cast<uint8_t>(valueTypeTag(callee));
            slot.hitCount++;
        }
        Chunk* caller_chunk = cur_chunk;
        int result_slot = base - 1;
        if (IS_CLOSURE(callee)) {
            ObjClosure* cl = AS_CLOSURE(callee);
            Chunk* fchk = cl->function->chunk;
            if (UNLIKELY(argc != fchk->arity))
                runtimeError(ErrorCode::WRONG_ARG_COUNT);
            if (UNLIKELY(FramesTop_ >= MAX_FRAMES))
                runtimeError(ErrorCode::STACK_OVERFLOW);
            int local_count = fchk->localCount;
            int new_top = base + local_count + 1;
            while (StackTop_ < new_top)
                Stack_[StackTop_++] = NIL_VAL;
            SAVE_IP();
            Frames_[FramesTop_++] = CallFrame(cl, fchk, 0, base, local_count);
        } else {
            SAVE_IP();
            callValue(callee, argc, base, false);
        }
        LOAD_FRAME();
        if (has_slot) {
            caller_chunk->icSlots[ic_idx].seenRet |= static_cast<uint8_t>(valueTypeTag(Stack_[result_slot]));
            caller_chunk->code[call_ip] = make_ABC(OpCode::CALL, fn_reg, argc, 0);
        }
        DISPATCH();
    }
    CASE(RETURN)
    {
        uint8_t src = instr_A(instr);
        uint8_t n_ret = instr_B(instr);
        Value ret = (n_ret > 0) ? cur_base[src] : NIL_VAL;
        CLOSE_UPVALUES_IF_NEEDED();
        Stack_[cur_frame_base - 1] = ret;
        --FramesTop_;
        if (LIKELY(FramesTop_ == 0))
            return ret;
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(RETURN_NIL)
    {
        CLOSE_UPVALUES_IF_NEEDED();
        Stack_[cur_frame_base - 1] = NIL_VAL;
        --FramesTop_;
        if (LIKELY(FramesTop_ == 0))
            return NIL_VAL;
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(RETURN1)
    {
        Value ret = cur_base[instr_A(instr)];
        CLOSE_UPVALUES_IF_NEEDED();
        Stack_[cur_frame_base - 1] = ret;
        --FramesTop_;
        if (LIKELY(FramesTop_ == 0))
            return ret;
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(NOP) { DISPATCH(); }
    CASE(HALT) { halt(); }
    END_DISPATCH();

    return NIL_VAL;
}

void VM::callValue(Value callee, int argc, int call_base, bool tail)
{
    if (IS_CLOSURE(callee)) {
        ObjClosure* cl = AS_CLOSURE(callee);
        Chunk* fchk = cl->function->chunk;
        int arity = fchk->arity;
        int local_count = fchk->localCount;

        if (argc != arity) {
            diagnostic::emit(
                "expected " + std::to_string(arity) + " arguments but got " + std::to_string(argc));
            runtimeError(ErrorCode::WRONG_ARG_COUNT);
        }

        if (local_count < argc)
            runtimeError(ErrorCode::WRONG_ARG_COUNT);

        if (tail && frameDepth() > 0) {
            int cur_base = Frames_[FramesTop_ - 1].base;
            for (int i = 0; i < argc; ++i)
                Stack_[cur_base + i] = Stack_[call_base + i];
            for (int i = argc; i < local_count; ++i)
                Stack_[cur_base + i] = NIL_VAL;
            Frames_[FramesTop_ - 1] = CallFrame(cl, fchk, 0, cur_base, local_count);
            int new_top = cur_base + local_count + 1;
            if (StackTop_ < new_top)
                StackTop_ = new_top;
        } else {
            int new_top = call_base + local_count + 1;
            while (StackTop_ < new_top)
                Stack_[StackTop_++] = NIL_VAL;
            for (int i = argc; i < local_count; ++i)
                Stack_[call_base + i] = NIL_VAL;
            Frames_[FramesTop_++] = CallFrame(cl, fchk, 0, call_base, local_count);
        }
        return;
    }

    if (IS_NATIVE(callee)) {
        ObjNative* nat = AS_NATIVE(callee);
        if (nat->arity >= 0 && argc != nat->arity) {
            diagnostic::emit(
                "native '" + std::string(nat->name ? nat->name->str.data() : "?") + "' expected " + std::to_string(nat->arity) + " args, got " + std::to_string(argc));
            runtimeError(ErrorCode::WRONG_ARG_COUNT);
        }
        Stack_[call_base - 1] = callNative(nat, argc, call_base);
        return;
    }

    StringRef fn_name;
    if (IS_FUNCTION(callee) && AS_FUNCTION(callee)->name)
        fn_name = AS_FUNCTION(callee)->name->str;
    diagnostic::emit("Attempting call on non-function value: " + std::string(fn_name.data()));
    runtimeError(ErrorCode::TYPE_ERROR_CALL);
}

Value VM::callNative(ObjNative* nat, int argc, int call_base)
{
    return (this->*nat->fn)(argc, &Stack_[call_base]);
}

ObjUpvalue* VM::captureUpvalue(unsigned int stack_pos)
{
    // Re-use an existing open upvalue pointing to the same slot.
    for (int i = static_cast<int>(OpenUpvalues_.size()) - 1; i >= 0; --i) {
        ObjUpvalue* uv = OpenUpvalues_[i];
        if (uv->location == &Stack_[stack_pos])
            return uv;
    }

    ObjUpvalue* uv = MAKE_OBJ_UPVALUE(&Stack_[stack_pos]);
    OpenUpvalues_.push(uv);
    OpenUpvalueCount_ = static_cast<int>(OpenUpvalues_.size());
    return uv;
}

void VM::closeUpvalues(unsigned int from_stack_pos)
{
    if (OpenUpvalues_.empty())
        return;

    Value* threshold = &Stack_[from_stack_pos];
    int i = static_cast<int>(OpenUpvalues_.size());

    while (i > 0 && OpenUpvalues_[i - 1]->location >= threshold) {
        ObjUpvalue* uv = OpenUpvalues_[--i];
        uv->closed = *uv->location;
        uv->location = &uv->closed;
    }

    OpenUpvalues_.resize(static_cast<uint32_t>(i));
    OpenUpvalueCount_ = i;
}

// Close all upvalues captured within a specific call frame.
void VM::closeUpvaluesForFrame(CallFrame const& f)
{
    closeUpvalues(static_cast<unsigned int>(f.base));
}

ObjString* VM::intern(StringRef const& str)
{
    if (ObjString** existing = StringTable_.findPtr(str))
        return *existing;
    ObjString* obj = MAKE_OBJ_STRING(str);
    StringTable_.insertOrAssign(str, obj);
    return obj;
}

void VM::internChunkConstants(Chunk* ch)
{
    if (!ch)
        return;
    for (uint32_t i = 0; i < ch->constants.size(); ++i) {
        if (IS_STRING(ch->constants[i]))
            ch->constants[i] = MAKE_OBJECT(intern(AS_STRING(ch->constants[i])->str));
    }
    for (auto* fn : ch->functions)
        internChunkConstants(fn);
}

// ---------------------------------------------------------------------------
// Standard library registration
// ---------------------------------------------------------------------------

void VM::openStdlib()
{
    registerNative("len", &VM::nativeLen, -1);
    registerNative("اضف", &VM::nativeAppend, -1);
    registerNative("pop", &VM::nativePop, -1);
    registerNative("slice", &VM::nativeSlice, -1);
    registerNative("اكتب", &VM::nativePrint, -1);
    registerNative("input", &VM::nativeInput, -1);
    registerNative("type", &VM::nativeType, -1);
    registerNative("int", &VM::nativeInt, -1);
    registerNative("float", &VM::nativeFloat, -1);
    registerNative("str", &VM::nativeStr, -1);
    registerNative("bool", &VM::nativeBool, -1);
    registerNative("قائمة", &VM::nativeList, -1);
    registerNative("split", &VM::nativeSplit, -1);
    registerNative("join", &VM::nativeJoin, -1);
    registerNative("substr", &VM::nativeSubstr, -1);
    registerNative("contains", &VM::nativeContains, -1);
    registerNative("trim", &VM::nativeTrim, -1);
    registerNative("floor", &VM::nativeFloor, -1);
    registerNative("ceil", &VM::nativeCeil, -1);
    registerNative("round", &VM::nativeRound, -1);
    registerNative("abs", &VM::nativeAbs, -1);
    registerNative("min", &VM::nativeMin, -1);
    registerNative("max", &VM::nativeMax, -1);
    registerNative("pow", &VM::nativePow, -1);
    registerNative("sqrt", &VM::nativeSqrt, -1);
    registerNative("assert", &VM::nativeAssert, -1);
    registerNative("clock", &VM::nativeClock, -1);
    registerNative("error", &VM::nativeError, -1);
    registerNative("time", &VM::nativeTime, -1);
}

void VM::registerNative(StringRef const& name, NativeFn fn, int arity)
{
    ObjString* name_obj = MAKE_OBJ_STRING(name);
    Value val = MAKE_NATIVE(fn, name_obj, arity);
    if (uint32_t* slot = GlobalIndex_.findPtr(name)) {
        GlobalSlots_[*slot] = val;
    } else {
        uint32_t slot_idx = static_cast<uint32_t>(GlobalSlots_.size());
        GlobalSlots_.push(val);
        GlobalIndex_.insertOrAssign(name, slot_idx);
    }
}

SourceLocation VM::currentLocation() const
{
    if (frameDepth() == 0)
        return { 0, 0, 0 };

    CallFrame const& f = topFrame();
    size_t off = f.ip > 0 ? f.ip - 1 : 0;
    Chunk const& ch = *f.chunk;

    if (off < ch.locations.size())
        return ch.locations[off];

    return { 0, 0, 0 };
}

void VM::runtimeError(ErrorCode code)
{
    SourceLocation loc = currentLocation();
    diagnostic::report(
        diagnostic::Severity::ERROR,
        loc.line, loc.column,
        static_cast<uint16_t>(code));

    for (int i = FramesTop_ - 1; i >= 0; --i) {
        CallFrame* p = &Frames_[i];
        if (!p->chunk)
            continue;
        Chunk const& ch = *p->chunk;
        size_t off = p->ip > 0 ? p->ip - 1 : 0;
        if (off >= ch.locations.size())
            continue;

        SourceLocation floc = ch.locations[off];
        StringRef fname = (p->closure && p->closure->function && p->closure->function->name)
            ? p->closure->function->name->str
            : "<toplevel>";
        diagnostic::engine.addNote(
            floc.line,
            "in '" + std::string(fname.data()) + "' at " + std::to_string(floc.line) + ":" + std::to_string(floc.column));
    }

    diagnostic::dump();
    halt();
}

void VM::halt()
{
    closeUpvalues(0);
    FramesTop_ = 0;
    StackTop_ = 0;
    throw std::runtime_error("");
}

VM::CallFrame& VM::frame() { return Frames_[FramesTop_ - 1]; }
VM::CallFrame const& VM::frame() const { return Frames_[FramesTop_ - 1]; }

Chunk* VM::chunk() { return topFrame().chunk; }
Value& VM::reg(int r) { return Stack_[topFrame().base + r]; }

void VM::updateIcBinary(Chunk* ch, uint32_t nop_ip, Value lhs, Value rhs, Value result)
{
    if (!ch)
        return;
    uint32_t nop = ch->code[nop_ip];
    uint8_t ic_idx = instr_A(nop);
    if (ic_idx < ch->icSlots.size()) {
        auto& slot = ch->icSlots[ic_idx];
        slot.seenLhs |= static_cast<uint8_t>(valueTypeTag(lhs));
        slot.seenRhs |= static_cast<uint8_t>(valueTypeTag(rhs));
        slot.seenRet |= static_cast<uint8_t>(valueTypeTag(result));
        slot.hitCount++;
    }
}

} // namespace mylang::runtime
