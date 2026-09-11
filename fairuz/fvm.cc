//
// vm.cc
//

#include "fvm.hpp"
#include "fdiagnostic.hpp"
#include "fmacros.hpp"
#include "fobj_header.hpp"
#include "fobject.hpp"
#include "fopcode.hpp"
#include "fstring.hpp"
#include "futil.hpp"
#include "fvalue.hpp"
#include <cstdint>
#include <cstdio>

namespace fairuz::runtime {

#define Fa_DISPATCH()                                              \
    do {                                                           \
        if (UNLIKELY(m_gc.should_collect()))                       \
            m_gc.collect(this);                                    \
        if (UNLIKELY(ip >= cur_chunk->code.size()))                \
            runtime_error(ErrorCode::INVALID_OPCODE, "instruction pointer out of bounds"); \
        instr = cur_chunk->code[ip];                               \
        ip++;                                                      \
        SAVE_IP();                                                 \
        u8 opcode = static_cast<u8>(Fa_instr_op(instr));           \
        if (UNLIKELY(opcode >= static_cast<u8>(Fa_OpCode::_COUNT))) \
            runtime_error(ErrorCode::INVALID_OPCODE);              \
        goto* dispatch_table[opcode];                              \
    } while (0)

#define Fa_BEGIN_DISPATCH() Fa_DISPATCH()
#define Fa_END_DISPATCH()
#define Fa_CASE(op) Fa_##op:

#define Fa_VMOPI(lhs, rhs, op) lhs.as_int() op rhs.as_int()
#define Fa_VMOPF(lhs, rhs, op) lhs.as_double_any() op rhs.as_double_any()

#define Fa_VM_ADDI(lhs, rhs) Fa_VMOPI(lhs, rhs, +)
#define Fa_VM_SUBI(lhs, rhs) Fa_VMOPI(lhs, rhs, -)
#define Fa_VM_MULI(lhs, rhs) Fa_VMOPI(lhs, rhs, *)
#define Fa_VM_DIVI(lhs, rhs) Fa_VMOPI(lhs, rhs, /)
#define Fa_VM_ADDF(lhs, rhs) Fa_VMOPF(lhs, rhs, +)
#define Fa_VM_SUBF(lhs, rhs) Fa_VMOPF(lhs, rhs, -)
#define Fa_VM_MULF(lhs, rhs) Fa_VMOPF(lhs, rhs, *)
#define Fa_VM_DIVF(lhs, rhs) Fa_VMOPF(lhs, rhs, /)
#define Fa_VM_INSTANCE_OP(op_name)                                                    \
    do {                                                                              \
        Fa_ObjClass* self_klass = nullptr;                                            \
        Fa_Value self_val, arg_val = Fa_Value::nil();                                 \
        int slot = -1;                                                                \
        if (lhs.is_instance()) {                                                      \
            self_klass = lhs.as_instance()->klass;                                    \
            slot = self_klass->method_slot(sp_method_name(Fa_ObjClass::op_name));     \
            if (slot >= 0) {                                                          \
                self_val = lhs;                                                       \
                arg_val = rhs;                                                        \
            }                                                                         \
        }                                                                             \
        if (slot < 0 && rhs.is_instance()) {                                          \
            self_klass = rhs.as_instance()->klass;                                    \
            slot = self_klass->method_slot(sp_method_name(Fa_ObjClass::op_name));     \
            if (slot >= 0) {                                                          \
                self_val = rhs;                                                       \
                arg_val = lhs;                                                        \
            }                                                                         \
        }                                                                             \
        if (UNLIKELY(slot < 0))                                                       \
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);                               \
        if (UNLIKELY(static_cast<u32>(slot) >= self_klass->vtable.size()))            \
            runtime_error(ErrorCode::UNDEFINED_METHOD);                              \
        Fa_Chunk* target_chunk = self_klass->vtable[static_cast<u32>(slot)];          \
        int caller_stack_top = m_stack_top;                                           \
        int call_base = caller_stack_top;                                             \
        if (UNLIKELY(call_base + 2 >= STACK_SIZE))                                    \
            runtime_error(ErrorCode::STACK_OVERFLOW);                                 \
        m_stack[call_base + 1] = arg_val;                                             \
        invoke_method(target_chunk, self_val, cur_frame_base + Fa_instr_A(instr),    \
            call_base, 2, ip, caller_stack_top);                                      \
    } while (0)

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
        if (!v.is_number())                             \
            runtime_error(ErrorCode::TYPE_ERROR_ARITH); \
    } while (0)

Fa_StringRef sp_method_name(int m)
{
    switch (m) {
    case Fa_ObjClass::INIT: return "بداية";
    case Fa_ObjClass::CALL: return "نداء";
    case Fa_ObjClass::ADD: return "عملية+";
    case Fa_ObjClass::SUB: return "عملية-";
    case Fa_ObjClass::MUL: return "عملية*";
    case Fa_ObjClass::DIV: return "عملية/";
    case Fa_ObjClass::MOD: return "عملية%";
    case Fa_ObjClass::NEG: return "سالب";
    case Fa_ObjClass::EQ: return "يساوي";
    case Fa_ObjClass::NEQ: return "لا_يساوي";
    case Fa_ObjClass::LT: return "اصغر_من";
    case Fa_ObjClass::LTE: return "اصغر_او_يساوي";
    case Fa_ObjClass::GT: return "اكبر_من";
    case Fa_ObjClass::GTE: return "اكبر_او_يساوي";
    case Fa_ObjClass::REPR: return "كتابة";
    default:
        return { };
    }
}

static void check_stack_index(int index, int stack_size, char const* m_context)
{
    if (index < 0 || index >= stack_size)
        // This is a Fa_VM internal error, not a user error.
        diagnostic::panic(diagnostic::errc::general::Code::INTERNAL_ERROR,
            std::string(" : ") + std::string("Fa_VM internal error: stack index ") + std::to_string(index)
                + " out of range [0," + std::to_string(stack_size) + ") in " + m_context);
}

Fa_VM::Fa_VM()
{
    diagnostic::reset();

    std::fill(m_stack, m_stack + STACK_SIZE, Fa_Value::nil());
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
        m_stack_top++;                             \
    } while (0);

// Ensure the value stack has at least `needed` slots allocated, filling
// any new slots with NIL.  Checks against the hard cap.
void Fa_VM::ensure_stack_slots(int needed)
{
    if (needed > STACK_SIZE)
        runtime_error(ErrorCode::STACK_OVERFLOW);

    while (m_stack_top < needed)
        PUSH_VALUE(Fa_Value::nil());
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
        return Fa_Value::nil();

    m_stack_top = 0;
    m_frames_top = 0;

    Fa_ObjFunction* fn = m_gc.make_obj_function(chunk);

    m_stack[0] = Fa_Value::from_obj(reinterpret_cast<Fa_ObjHeader*>(fn));
    m_stack_top = static_cast<int>(chunk->local_count) + 2;

    if (m_frames_top >= MAX_FRAMES || m_stack_top >= STACK_SIZE)
        runtime_error(ErrorCode::STACK_OVERFLOW);

    m_frames[m_frames_top] = Fa_CallFrame(fn, chunk, 0, 1,
        static_cast<u16>(chunk->local_count), 0, 0);
    m_frames_top++;
    intern_chunk_constants(fn->chunk);

    if (m_gc.current_memory() >= GC_THRESHOLD)
        m_gc.collect(this);

    return execute();
}

#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpedantic"
#endif

Fa_Value Fa_VM::execute(int stop_frame_depth)
{
    static void* dispatch_table[] = {
#define X(name) &&Fa_##name,
        FA_OPCODE_LIST(X)
#undef X
    };

    if (m_frames_top == 0)
        return Fa_Value::nil();

    u32 instr;
    Fa_Chunk* cur_chunk = frame().chunk;
    int cur_frame_base = frame().base;
    Fa_Value* cur_base = &m_stack[cur_frame_base];
    u32 ip = frame().ip;

    using reg_t = u8;
    auto checked_int = [this](__int128 value) -> Fa_Value {
        if (value < static_cast<__int128>(Fa_Value::int_min())
            || value > static_cast<__int128>(Fa_Value::int_max()))
            runtime_error(ErrorCode::NUMERIC_OUT_OF_RANGE);
        return Fa_Value::from_int(static_cast<i64>(value));
    };

    Fa_BEGIN_DISPATCH();

    Fa_CASE(LOAD_NIL)
    {
        reg_t start = Fa_instr_B(instr);
        reg_t count = Fa_instr_C(instr);

        for (u8 i = 0; i < count; i++)
            cur_base[start + i] = Fa_Value::nil();

        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_TRUE)
    {
        Fa_RA() = Fa_Value::from_bool(true);
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_FALSE)
    {
        Fa_RA() = Fa_Value::from_bool(false);
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_CONST)
    {
        u16 index = Fa_instr_Bx(instr);
        if (UNLIKELY(index >= cur_chunk->constants.size()))
            runtime_error(ErrorCode::INVALID_OPCODE, "constant index out of bounds");
        Fa_RA() = cur_chunk->constants[index];
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_INT)
    {
        Fa_Value& ret = Fa_RA();
        /// TODO: create a Fa_ObjInt if using nanboxing and v is larger than 48 bits
        ret = Fa_Value::from_int(static_cast<i32>(static_cast<u16>(Fa_instr_Bx(instr))) - JUMP_OFFSET);
        Fa_DISPATCH();
    }
    Fa_CASE(LOAD_GLOBAL)
    {
        {
            u16 index = Fa_instr_Bx(instr);
            if (UNLIKELY(index >= cur_chunk->constants.size()
                    || !cur_chunk->constants[index].is_string()))
                runtime_error(ErrorCode::INVALID_OPCODE, "invalid global-name constant");
            Fa_Value name_v = cur_chunk->constants[index];
            Fa_StringRef name = name_v.as_string()->str;
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
            u16 index = Fa_instr_Bx(instr);
            if (UNLIKELY(index >= cur_chunk->constants.size()
                    || !cur_chunk->constants[index].is_string()))
                runtime_error(ErrorCode::INVALID_OPCODE, "invalid global-name constant");
            Fa_Value name_v = cur_chunk->constants[index];
            Fa_StringRef name = name_v.as_string()->str;
            u32 slot_idx;

            if (u32* slot = m_global_index.find_ptr(name)) {
                slot_idx = *slot;
            } else {
                slot_idx = static_cast<u32>(m_global_slots.size());
                m_global_slots.push(Fa_Value::nil());
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
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();
        bool both_str = lhs.is_string() && rhs.is_string();

        if (both_str) {
            // String concatenation
            res = m_gc.make_string(lhs.as_string()->str + rhs.as_string()->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
            // Fa_VM_ABC(Fa_OpCode::OP_ADD_SS);
        } else if (lhs.is_int() && rhs.is_int()) {
            // Integer addition
            res = checked_int(static_cast<__int128>(lhs.as_int()) + rhs.as_int());
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            // Float addition (includes mixed int/float)
            res = Fa_Value::from_real(Fa_VM_ADDF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            Fa_VM_INSTANCE_OP(ADD);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_SUB)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = checked_int(static_cast<__int128>(lhs.as_int()) - rhs.as_int());
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Fa_Value::from_real(Fa_VM_SUBF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            Fa_VM_INSTANCE_OP(SUB);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_MUL)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = checked_int(static_cast<__int128>(lhs.as_int()) * rhs.as_int());
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Fa_Value::from_real(Fa_VM_MULF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            Fa_VM_INSTANCE_OP(MUL);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_DIV)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (lhs.is_int() && rhs.is_int()) {
            if (rhs.as_int() == 0)
                runtime_error(ErrorCode::DIVISION_BY_ZERO);
            if (lhs.as_int() == Fa_Value::int_min() && rhs.as_int() == -1)
                runtime_error(ErrorCode::NUMERIC_OUT_OF_RANGE);
            if (lhs.as_int() % rhs.as_int() == INT64_C(0))
                res = Fa_Value::from_int(Fa_VM_DIVI(lhs, rhs));
            else
                res = Fa_Value::from_real(Fa_VM_DIVF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            if (rhs.as_double_any() == 0.0)
                runtime_error(ErrorCode::DIVISION_BY_ZERO);
            res = Fa_Value::from_real(Fa_VM_DIVF(lhs, rhs));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            Fa_VM_INSTANCE_OP(DIV);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_MOD)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (lhs.is_int() && rhs.is_int()) {
            if (rhs.as_int() == 0)
                runtime_error(ErrorCode::MODULO_BY_ZERO);
            res = Fa_Value::from_real(static_cast<f64>(Fa_VMOPI(lhs, rhs, %)));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            if (rhs.as_double_any() == 0.0)
                runtime_error(ErrorCode::MODULO_BY_ZERO);
            res = Fa_Value::from_real(std::fmod(lhs.as_double_any(), rhs.as_double_any()));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            Fa_VM_INSTANCE_OP(MOD);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_POW)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        REQUIRE_NUMBER(lhs);
        REQUIRE_NUMBER(rhs);

        res = Fa_Value::from_real(std::pow(lhs.as_double_any(), rhs.as_double_any()));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_NEG)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value operand = Fa_RB();

        if (operand.is_int()) {
            res = checked_int(-static_cast<__int128>(operand.as_int()));
        } else if (operand.is_double()) {
            res = Fa_Value::from_real(-operand.as_double_any());
        } else if (operand.is_instance()) {
            Fa_ObjInstance* instance = operand.as_instance();
            Fa_ObjClass* self_klass = instance->klass;

            int slot = self_klass->method_slot(sp_method_name(Fa_ObjClass::NEG));
            if (UNLIKELY(slot < 0))
                runtime_error(ErrorCode::UNDEFINED_METHOD);

            if (UNLIKELY(static_cast<u32>(slot) >= self_klass->vtable.size()))
                runtime_error(ErrorCode::UNDEFINED_METHOD);
            Fa_Chunk* target_chunk = self_klass->vtable[static_cast<u32>(slot)];
            int caller_stack_top = m_stack_top;
            int call_base = caller_stack_top;

            if (UNLIKELY(call_base + 1 >= STACK_SIZE))
                runtime_error(ErrorCode::STACK_OVERFLOW);
            invoke_method(target_chunk, operand, cur_frame_base + Fa_instr_A(instr),
                call_base, 1, ip, caller_stack_top);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITAND)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (UNLIKELY(!lhs.is_int() || !rhs.is_int()))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        res = Fa_Value::from_int(Fa_VMOPI(lhs, rhs, &));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITOR)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (UNLIKELY(!lhs.is_int() || !rhs.is_int()))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        res = Fa_Value::from_int(Fa_VMOPI(lhs, rhs, |));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITXOR)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (UNLIKELY(!lhs.is_int() || !rhs.is_int()))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        res = Fa_Value::from_int(Fa_VMOPI(lhs, rhs, ^));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_BITNOT)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value operand = Fa_RB();
        if (UNLIKELY(!operand.is_int()))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        res = Fa_Value::from_int(~operand.as_int());
        Fa_DISPATCH();
    }
    Fa_CASE(OP_LSHIFT)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        if (UNLIKELY(!lhs.is_int()))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        reg_t imm = Fa_instr_C(instr);
        if (imm >= 64)
            runtime_error(ErrorCode::INDEX_TYPE_ERROR);

        res = checked_int(static_cast<__int128>(lhs.as_int())
            * (static_cast<__int128>(1) << imm));
        Fa_DISPATCH();
    }
    Fa_CASE(OP_RSHIFT)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        if (UNLIKELY(!lhs.is_int()))
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        reg_t imm = Fa_instr_C(instr);
        if (imm >= 64)
            runtime_error(ErrorCode::INDEX_TYPE_ERROR);

        u64 shifted = static_cast<u64>(lhs.as_int()) >> imm;
        if (lhs.as_int() < 0)
            res = Fa_Value::from_real(static_cast<f64>(shifted));
        else
            res = Fa_Value::from_int(static_cast<i64>(shifted));

        Fa_DISPATCH();
    }
    Fa_CASE(OP_EQ)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (lhs.is_nil() || rhs.is_nil()) {
            res = Fa_Value::from_bool(lhs.is_nil() && rhs.is_nil());
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_int() && rhs.is_int()) {
            res = Fa_Value::from_bool(Fa_VMOPI(lhs, rhs, ==));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Fa_Value::from_bool(lhs.as_string()->str == rhs.as_string()->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Fa_Value::from_bool(lhs.as_double_any() == rhs.as_double_any());
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            Fa_VM_INSTANCE_OP(EQ);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_NEQ)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (lhs.is_nil() || rhs.is_nil()) {
            res = Fa_Value::from_bool(!(lhs.is_nil() && rhs.is_nil()));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_int() && rhs.is_int()) {
            res = Fa_Value::from_bool(Fa_VMOPI(lhs, rhs, !=));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Fa_Value::from_bool(lhs.as_string()->str != rhs.as_string()->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Fa_Value::from_bool(lhs.as_double_any() != rhs.as_double_any());
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            Fa_VM_INSTANCE_OP(NEQ);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_LT)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = Fa_Value::from_bool(Fa_VMOPI(lhs, rhs, <));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Fa_Value::from_bool(lhs.as_string()->str < rhs.as_string()->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Fa_Value::from_bool(lhs.as_double_any() < rhs.as_double_any());
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            Fa_VM_INSTANCE_OP(LT);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_LTE)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value lhs = Fa_RB();
        Fa_Value rhs = Fa_RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = Fa_Value::from_bool(Fa_VMOPI(lhs, rhs, <=));
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Fa_Value::from_bool(lhs.as_string()->str <= rhs.as_string()->str);
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Fa_Value::from_bool(lhs.as_double_any() <= rhs.as_double_any());
            Fa_RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            Fa_VM_INSTANCE_OP(LTE);
            LOAD_FRAME();
            Fa_DISPATCH();
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(OP_NOT)
    {
        Fa_RA() = Fa_Value::from_bool(!Fa_RB().is_truthy());
        Fa_DISPATCH();
    }
    Fa_CASE(CONCAT)
    {
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_NEW)
    {
        Fa_ObjList* list_obj = m_gc.make_obj_list();
        list_obj->reserve(Fa_instr_B(instr));
        Fa_RA() = Fa_Value::from_obj(reinterpret_cast<Fa_ObjHeader*>(list_obj));
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_APPEND)
    {
        Fa_Value& list_v = Fa_RA();
        if (!list_v.is_list())
            runtime_error(ErrorCode::TYPE_ERROR_CALL, "attempting to append on a non list");

        list_v.as_list()->elements.push(Fa_RB());
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_GET)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value list_v = Fa_RB();
        Fa_Value index_v = Fa_RC();

        if (!list_v.is_list())
            runtime_error(ErrorCode::TYPE_ERROR_CALL, "attempting get on a non list");
        if (!index_v.is_int())
            runtime_error(ErrorCode::INDEX_TYPE_ERROR, "attempting get with a non integer index");

        auto& elems = list_v.as_list()->elements;
        i64 idx = index_v.as_int();
        if (idx < 0 || idx >= static_cast<i64>(elems.size()))
            runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        res = elems[static_cast<u32>(idx)];
        Fa_DISPATCH();
    }
    Fa_CASE(LIST_SET)
    {
        Fa_Value& object_v = Fa_RA();
        Fa_Value index_v = Fa_RB();
        Fa_Value new_val = Fa_RC();

        if (object_v.is_list()) {
            if (!index_v.is_int())
                runtime_error(ErrorCode::INDEX_TYPE_ERROR);

            auto& elems = object_v.as_list()->elements;
            i64 idx = index_v.as_int();

            if (idx < 0)
                idx += static_cast<i64>(elems.size());
            if (idx < 0 || idx >= static_cast<i64>(elems.size()))
                runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            elems[static_cast<u32>(idx)] = new_val;
        } else {
            runtime_error(ErrorCode::TYPE_ERROR_CALL, "attempting set on a non list value");
        }

        Fa_DISPATCH();
    }
    Fa_CASE(LIST_LEN)
    {
        Fa_Value list_v = Fa_RB();
        if (!list_v.is_list())
            runtime_error(ErrorCode::TYPE_ERROR_CALL, "attempting len on a non list value");

        Fa_RA() = Fa_Value::from_int(static_cast<i64>(list_v.as_list()->elements.size()));
        Fa_DISPATCH();
    }
    Fa_CASE(JUMP)
    {
        ip += Fa_instr_sBx(instr);
        Fa_DISPATCH();
    }
    Fa_CASE(JUMP_IF_TRUE)
    {
        if (Fa_RA().is_truthy())
            ip += Fa_instr_sBx(instr);

        Fa_DISPATCH();
    }
    Fa_CASE(JUMP_IF_FALSE)
    {
        if (!Fa_RA().is_truthy())
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
        reg_t base_reg = Fa_instr_A(instr);
        Fa_Value init_v = cur_base[base_reg];
        Fa_Value limit_v = cur_base[base_reg + 1];
        Fa_Value step_v = cur_base[base_reg + 2];

        if (!init_v.is_int() || !limit_v.is_int() || !step_v.is_int())
            runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        i64 init = init_v.as_int();
        i64 limit = limit_v.as_int();
        i64 step = step_v.as_int();
        if (step == 0)
            runtime_error(ErrorCode::DIVISION_BY_ZERO);

        cur_base[base_reg] = Fa_Value::from_int(limit);
        cur_base[base_reg + 1] = Fa_Value::from_int(step);
        cur_base[base_reg + 2] = Fa_Value::from_int(init);
        bool enters = (step > 0) ? (init <= limit) : (init >= limit);
        if (!enters)
            ip += Fa_instr_sBx(instr);

        Fa_DISPATCH();
    }
    Fa_CASE(FOR_STEP)
    {
        reg_t base_reg = Fa_instr_A(instr);
        Fa_Value limit_v = cur_base[base_reg];
        Fa_Value step_v = cur_base[base_reg + 1];
        Fa_Value control_v = cur_base[base_reg + 2];

        if (control_v.is_int()) {
            Fa_Value control_value = checked_int(static_cast<__int128>(control_v.as_int()) + step_v.as_int());
            i64 control = control_value.as_int();
            cur_base[base_reg + 2] = control_value;
            bool continues = (step_v.as_int() > 0) ? (control <= limit_v.as_int())
                                                   : (control >= limit_v.as_int());
            if (continues)
                ip += Fa_instr_sBx(instr);
        } else {
            f64 control = control_v.as_double_any() + step_v.as_double_any();
            cur_base[base_reg + 2] = Fa_Value::from_real(control);
            bool continues = step_v.as_double() > 0.0 ? control <= limit_v.as_double()
                                                      : control >= limit_v.as_double();
            if (continues)
                ip += Fa_instr_sBx(instr);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(CLOSURE)
    {
        u16 fn_idx = Fa_instr_Bx(instr);
        if (UNLIKELY(fn_idx >= cur_chunk->functions.size()
                || cur_chunk->functions[fn_idx] == nullptr))
            runtime_error(ErrorCode::INVALID_OPCODE, "function index out of bounds");
        Fa_Chunk* fn_chunk = cur_chunk->functions[fn_idx];
        Fa_RA() = m_gc.make_function(fn_chunk);
        Fa_DISPATCH();
    }
    Fa_CASE(CALL)
    {
        reg_t fn_reg = Fa_instr_A(instr);
        reg_t argc = Fa_instr_B(instr);
        Fa_Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;

        SAVE_IP();
        call_value(callee, argc, base, false);

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(CALL_TAIL)
    {
        reg_t fn_reg = Fa_instr_A(instr);
        reg_t argc = Fa_instr_B(instr);
        Fa_Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;

        if (callee.is_native()) {
            Fa_ObjNative* nat = callee.as_native();
            if (UNLIKELY(nat->arity >= 0 && argc != nat->arity)) {
                std::string name = (nat->name ? std::string(nat->name->str.data(), nat->name->str.len()) : "?");
                runtime_error(ErrorCode::NATIVE_ARG_COUNT,
                    std::string("native '") + name + "' expected " + std::to_string(nat->arity) + " args, got " + std::to_string(argc));
            }

            // Natives run synchronously with no pushed frame, so a tail call to
            // one must perform *this* frame's own return afterward — the
            // compiler omits a trailing RETURN for CALL_TAIL sites, trusting
            // the callee to terminate the frame itself.
            Fa_Value ret = call_native(nat, argc, base);
            Fa_CallFrame finished = frame();
            m_stack[finished.return_slot] = ret;
            m_frames_top -= 1;
            m_stack_top = finished.caller_stack_top;

            if (LIKELY(m_frames_top == 0))
                return ret;

            LOAD_FRAME();
            Fa_DISPATCH();
        }

        SAVE_IP();
        call_value(callee, argc, base, true);
        // A tail call to a class without a constructor completes synchronously
        // inside call_value().  In the outermost frame there is then no frame
        // for LOAD_FRAME() to read.
        if (UNLIKELY(m_frames_top == 0))
            return m_stack[0];
        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(IC_CALL)
    {
        reg_t fn_reg = Fa_instr_A(instr);
        reg_t argc = Fa_instr_B(instr);
        reg_t ic_idx = Fa_instr_C(instr);
        u32 call_ip = ip - 1;
        Fa_Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;
        bool has_slot = ic_idx < cur_chunk->ic_slots.size();

        if (has_slot) {
            auto& slot = cur_chunk->ic_slots[ic_idx];
            slot.seen_lhs |= static_cast<u8>(value_type_tag(callee));
            slot.hit_count++;
        }

        Fa_Chunk* caller_chunk = cur_chunk;
        int result_slot = base - 1;

        SAVE_IP();
        call_value(callee, argc, base, false);

        LOAD_FRAME();

        if (has_slot) {
            caller_chunk->ic_slots[ic_idx].seen_ret |= static_cast<u8>(value_type_tag(m_stack[result_slot]));
            caller_chunk->code[call_ip] = Fa_make_ABC(Fa_OpCode::CALL, fn_reg, argc, 0);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(RETURN)
    {
        reg_t src = Fa_instr_A(instr);
        reg_t n_ret = Fa_instr_B(instr);
        Fa_Value ret = n_ret > 0 ? cur_base[src] : Fa_Value::nil();
        Fa_CallFrame finished = frame();
        m_stack[finished.return_slot] = ret;
        m_frames_top -= 1;
        m_stack_top = finished.caller_stack_top;

        if (m_frames_top == stop_frame_depth)
            return ret;
        if (LIKELY(m_frames_top == 0))
            return ret;

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(RETURN_NIL)
    {
        Fa_CallFrame finished = frame();
        m_stack[finished.return_slot] = Fa_Value::nil();
        m_frames_top -= 1;
        m_stack_top = finished.caller_stack_top;

        if (m_frames_top == stop_frame_depth)
            return Fa_Value::nil();
        if (LIKELY(m_frames_top == 0))
            return Fa_Value::nil();

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(RETURN1)
    {
        Fa_Value ret = cur_base[Fa_instr_A(instr)];
        Fa_CallFrame finished = frame();
        m_stack[finished.return_slot] = ret;
        m_frames_top -= 1;
        m_stack_top = finished.caller_stack_top;

        if (m_frames_top == stop_frame_depth)
            return ret;
        if (LIKELY(m_frames_top == 0))
            return ret;

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(INDEX_READ)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value obj = Fa_RB();
        Fa_Value idx = Fa_RC();

        if (obj.is_string()) {
            if (UNLIKELY(!idx.is_int()))
                runtime_error(ErrorCode::INDEX_TYPE_ERROR);

            i64 idx_int = idx.as_int();
            if (UNLIKELY(idx_int < 0))
                runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            Fa_StringRef const& str = obj.as_string()->str;

            size_t byte_pos = 0;
            i64 char_pos = 0;
            u64 char_bytes = 0;

            while (byte_pos < str.len() && char_pos < idx_int) {
                u64 step = 0;
                util::decode_utf8_at(str, byte_pos, &step);
                byte_pos += step;
                char_pos++;
            }

            if (UNLIKELY(byte_pos >= str.len()))
                runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            util::decode_utf8_at(str, byte_pos, &char_bytes);

            Fa_StringRef ch = str.slice(byte_pos, byte_pos + char_bytes);
            res = m_gc.make_string(ch);
        } else if (obj.is_list()) {
            if (UNLIKELY(!idx.is_int()))
                runtime_error(ErrorCode::INDEX_TYPE_ERROR);

            i64 idx_int = idx.as_int();
            Fa_ListType list = obj.as_list()->elements;
            if (UNLIKELY(idx_int >= list.size() || idx_int < 0))
                runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            res = obj.as_list()->elements[idx_int];
        } else if (obj.is_dict()) {
            Fa_ObjDict* dict_obj = obj.as_dict();
            if (dict_obj->data.find_ptr(idx) == nullptr)
                res = Fa_Value::nil();
            else
                res = dict_obj->data[idx];
        } else if (obj.is_instance()) {
            if (!idx.is_string())
                runtime_error(ErrorCode::INDEX_TYPE_ERROR);
            Fa_ObjInstance* inst = obj.as_instance();
            int field_idx = inst->klass->field_index(idx.as_string()->str);
            if (field_idx < 0)
                runtime_error(ErrorCode::UNDEFINED_METHOD); // or a dedicated "no such field" code
            res = inst->fields[field_idx];
        } else {
            runtime_error(ErrorCode::INDEX_TYPE_ERROR);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(INDEX_WRITE)
    {
        Fa_Value& obj = Fa_RA();
        Fa_Value idx = Fa_RB();
        Fa_Value val = Fa_RC();

        /// NOTE: 'str' is immutable by design, so INDEX_WRITE falls back to runtime_error

        if (obj.is_list()) {
            if (!idx.is_int())
                runtime_error(ErrorCode::INDEX_TYPE_ERROR);
            i64 idx_int = idx.as_int();
            Fa_ObjList* list = obj.as_list();
            if (idx_int >= list->size() || idx_int < 0)
                runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);
            list->elements[idx.as_int()] = val;
        } else if (obj.is_dict()) {
            obj.as_dict()->data[idx] = val;
        } else if (obj.is_instance()) {
            /// TODO: go get the '[]' operator
            runtime_error(ErrorCode::INDEX_OBJECT_TYPE_ERROR);
        } else {
            runtime_error(ErrorCode::INDEX_OBJECT_TYPE_ERROR);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(NEW_CLASS)
    {
        {
            Fa_Value& res = Fa_RA();
            u32 desc_idx = Fa_instr_Bx(instr);
            if (UNLIKELY(desc_idx >= cur_chunk->class_descriptors.size()))
                runtime_error(ErrorCode::INVALID_OPCODE, "class descriptor index out of bounds");
            Fa_ClassDescriptor klass_desc = cur_chunk->class_descriptors[desc_idx];
            u32 field_count = klass_desc.field_count;
            u32 method_count = klass_desc.method_names.size();
            u32 vtable_size = klass_desc.vtable_size;

            Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> kfield_names { field_count, { }, &m_gc };
            Fa_Array<Fa_StringRef, /*_Alloc=*/Fa_GarbageCollector> kmethod_names { method_count, { }, &m_gc };
            Fa_Array<Fa_Chunk*, /*_Alloc=*/Fa_GarbageCollector> kvtable { vtable_size, { }, &m_gc };

            for (u32 i = 0; i < field_count;++i)
                kfield_names[i] = klass_desc.field_names[i];

            for (u32 i = 0; i < method_count;++i)
                kmethod_names[i] = klass_desc.method_names[i];

            for (u32 i = 0; i < vtable_size;++i) {
                u32 fn_idx = klass_desc.vtable_indices[i];
                if (UNLIKELY(fn_idx != Fa_ClassDescriptor::NULL_SLOT
                        && fn_idx >= cur_chunk->functions.size()))
                    runtime_error(ErrorCode::INVALID_OPCODE, "class method index out of bounds");
                kvtable[i] = fn_idx == Fa_ClassDescriptor::NULL_SLOT ? nullptr : cur_chunk->functions[fn_idx];
            }

            res = m_gc.make_class(klass_desc.name, kfield_names, kmethod_names, kvtable);
        }

        Fa_DISPATCH();
    }
    Fa_CASE(NEW_INSTANCE)
    {
        u16 index = Fa_instr_Bx(instr);
        if (UNLIKELY(index >= cur_chunk->constants.size()
                || !cur_chunk->constants[index].is_class()))
            runtime_error(ErrorCode::INVALID_OPCODE, "invalid class constant");
        Fa_ObjClass* klass = cur_chunk->constants[index].as_class();
        Fa_RA() = m_gc.make_instance(klass);
        Fa_DISPATCH();
    }
    Fa_CASE(INVOKE)
    {
        reg_t self_reg = Fa_instr_A(instr);
        reg_t slot = Fa_instr_B(instr);
        reg_t argc = Fa_instr_C(instr);

        if (UNLIKELY(ip >= cur_chunk->code.size()
                || Fa_instr_op(cur_chunk->code[ip]) != Fa_OpCode::NOP))
            runtime_error(ErrorCode::INVALID_OPCODE, "missing INVOKE cache payload");
        ++ip; // consume the trailing NOP carrying the IC slot index

        Fa_Value self_val = cur_base[self_reg];

        if (UNLIKELY(!self_val.is_instance()))
            runtime_error(ErrorCode::TYPE_ERROR_CALL, "(method call on non-instance)");

        Fa_ObjInstance* inst = self_val.as_instance();

        if (UNLIKELY(slot >= inst->klass->vtable.size() || inst->klass->vtable[slot] == nullptr))
            runtime_error(ErrorCode::TYPE_ERROR_CALL, "method slot is empty");

        invoke_method(inst->klass->vtable[slot], self_val,
            cur_frame_base + self_reg, cur_frame_base + self_reg + 1,
            argc, ip, m_stack_top);

        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(INVOKE_NAMED)
    {
        {
            Fa_Value inst = Fa_RA();
            u32 argc = Fa_instr_C(instr);

            if (UNLIKELY(ip >= cur_chunk->code.size()))
                runtime_error(ErrorCode::INVALID_OPCODE, "missing INVOKE_NAMED payload");
            u32 payload = cur_chunk->code[ip++];
            if (UNLIKELY(Fa_instr_op(payload) != Fa_OpCode::NOP))
                runtime_error(ErrorCode::INVALID_OPCODE, "invalid INVOKE_NAMED payload");
            u32 name_idx = Fa_instr_Bx(payload);

            if (UNLIKELY(!inst.is_instance()))
                runtime_error(ErrorCode::TYPE_ERROR_CALL, "(method call on non-instance)");

            Fa_ObjInstance* inst_obj = inst.as_instance();
            /// there many unchecked operations that may potentially fail here
            /// Fa_Compiler must guarantee that the name index in the constant table is valid
            /// otherwise it should throw an early error, also that index should always contain
            /// a string object and of course the index muse be an int, all of these must be
            /// guaranteed by the compiler before emitting this instruction
            if (UNLIKELY(name_idx >= cur_chunk->constants.size()
                    || !cur_chunk->constants[name_idx].is_string()))
                runtime_error(ErrorCode::INVALID_OPCODE, "invalid method-name constant");
            Fa_StringRef method_name = cur_chunk->constants[name_idx].as_string()->str;
            int slot = inst_obj->klass->method_slot(method_name);
            if (slot == -1)
                runtime_error(ErrorCode::TYPE_ERROR_CALL,
                    "instance class does not define this method: " + std::string(method_name.data()));

            invoke_method(inst_obj->klass->vtable[slot], inst,
                cur_frame_base + Fa_instr_A(instr),
                cur_frame_base + Fa_instr_A(instr) + 1,
                static_cast<int>(argc), ip, m_stack_top);
        }
        LOAD_FRAME();
        Fa_DISPATCH();
    }
    Fa_CASE(GET_FIELD)
    {
        Fa_Value& res = Fa_RA();
        Fa_Value obj_v = Fa_RB();

        if (!UNLIKELY(obj_v.is_instance()))
            runtime_error(ErrorCode::UNDEFINED_FIELD,
                Fa_type(1, &obj_v).as_string()->str.data() + std::string(" is not a class"));

        Fa_ObjInstance* inst = obj_v.as_instance();
        u32 field_idx = Fa_instr_C(instr);

        if (field_idx == 0xFF) {
            if (UNLIKELY(ip >= cur_chunk->code.size()
                    || Fa_instr_op(cur_chunk->code[ip]) != Fa_OpCode::NOP))
                runtime_error(ErrorCode::INVALID_OPCODE, "missing GET_FIELD payload");
            u32 payload = cur_chunk->code[ip]; // the NOP immediately after GET_FIELD
            u16 name_idx = Fa_instr_Bx(payload);
            ip++; // consume the payload word so DISPATCH doesn't re-decode it as a real op

            if (UNLIKELY(name_idx >= cur_chunk->constants.size()
                    || !cur_chunk->constants[name_idx].is_string()))
                runtime_error(ErrorCode::INVALID_OPCODE, "invalid field-name constant");
            Fa_ObjString* name_obj = cur_chunk->constants[name_idx].as_string();
            int slot = inst->klass->field_index(name_obj->str); // needs a runtime-side field_index lookup

            if (UNLIKELY(slot < 0))
                runtime_error(ErrorCode::UNDEFINED_FIELD,
                    Fa_type(1, &obj_v).as_string()->str.data() + std::string(" does not define ") + name_obj->str.data());

            res = inst->fields[static_cast<u32>(slot)];
            Fa_DISPATCH();
        }

        if (UNLIKELY(field_idx >= inst->fields.size()))
            runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        res = inst->fields[field_idx];
        Fa_DISPATCH();
    }
    Fa_CASE(SET_FIELD)
    {
        Fa_Value obj_v = Fa_RA();
        if (UNLIKELY(!obj_v.is_instance()))
            runtime_error(ErrorCode::TYPE_ERROR_CALL, "SET_FIELD on non-instance");

        Fa_ObjInstance* inst = obj_v.as_instance();
        reg_t field_idx = Fa_instr_B(instr);

        if (UNLIKELY(field_idx >= inst->fields.size()))
            runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        inst->fields[field_idx] = Fa_RC();
        Fa_DISPATCH();
    }

    Fa_CASE(NOP) { Fa_DISPATCH(); }
    Fa_CASE(HALT) { halt(); }

    Fa_END_DISPATCH();

    return Fa_Value::nil(); // UNREACHABLE
}

#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic pop
#endif

// Calls target_chunk with self_val in callee register 0. total_argc includes
// that implicit self slot; explicit arguments must already follow call_base.
void Fa_VM::invoke_method(Fa_Chunk* target_chunk, Fa_Value self_val,
    int result_slot, int call_base, int total_argc, u32 ip, int caller_stack_top)
{
    if (UNLIKELY(target_chunk == nullptr))
        runtime_error(ErrorCode::TYPE_ERROR_CALL, "method slot is empty");
    if (UNLIKELY(total_argc != target_chunk->arity))
        runtime_error(ErrorCode::WRONG_ARG_COUNT);
    if (UNLIKELY(m_frames_top >= MAX_FRAMES))
        runtime_error(ErrorCode::STACK_OVERFLOW);

    int local_count = target_chunk->local_count;
    int new_top = call_base + local_count + 1;
    if (UNLIKELY(new_top > STACK_SIZE))
        runtime_error(ErrorCode::STACK_OVERFLOW);

    // The caller may stage method arguments immediately above its live top.
    // Preserve that initialized prefix before clearing the remaining locals.
    int initialized_top = call_base + total_argc;
    if (m_stack_top < initialized_top)
        m_stack_top = initialized_top;
    while (m_stack_top < new_top) {
        m_stack[m_stack_top] = Fa_Value::nil();
        m_stack_top++;
    }

    m_stack[call_base] = self_val; // self lands in register 0 of the callee
    // explicit args are assumed already placed at call_base+1 .. call_base+explicit_argc
    // by the caller, before this is invoked.

    for (int i = total_argc; i < local_count; i++)
        m_stack[call_base + i] = Fa_Value::nil();

    SAVE_IP();
    m_frames[m_frames_top] = Fa_CallFrame(nullptr, target_chunk, 0,
        static_cast<u16>(call_base), static_cast<u16>(local_count),
        static_cast<u16>(result_slot), static_cast<u16>(caller_stack_top));
    m_frames_top++;
}

void Fa_VM::call_value(Fa_Value callee, int argc, int call_base, bool tail)
{
    if (callee.is_function()) {
        Fa_ObjFunction* fn = callee.as_func();
        Fa_Chunk* fchk = fn->chunk;
        int arity = fchk->arity;
        int local_count = fchk->local_count;

        if (argc != arity)
            runtime_error(ErrorCode::WRONG_ARG_COUNT, "expected " + std::to_string(arity) + " arguments but got " + std::to_string(argc));

        if (local_count < argc)
            runtime_error(ErrorCode::WRONG_ARG_COUNT);

        if (tail && m_frames_top > 0) {
            Fa_CallFrame old_frame = m_frames[m_frames_top - 1];
            int cur_base = old_frame.base;
            int new_top = cur_base + local_count + 1;
            if (UNLIKELY(new_top > STACK_SIZE))
                runtime_error(ErrorCode::STACK_OVERFLOW);

            for (int i = 0; i < argc; i++)
                m_stack[cur_base + i] = m_stack[call_base + i];
            for (int i = argc; i < local_count; i++)
                m_stack[cur_base + i] = Fa_Value::nil();

            m_frames[m_frames_top - 1] = Fa_CallFrame(fn, fchk, 0,
                static_cast<u16>(cur_base), static_cast<u16>(local_count),
                old_frame.return_slot, old_frame.caller_stack_top);
            m_stack_top = new_top;
        } else {
            if (UNLIKELY(m_frames_top >= MAX_FRAMES))
                runtime_error(ErrorCode::STACK_OVERFLOW);

            int caller_stack_top = m_stack_top;
            int new_top = call_base + local_count + 1;
            if (UNLIKELY(new_top > STACK_SIZE))
                runtime_error(ErrorCode::STACK_OVERFLOW);

            while (m_stack_top < new_top) {
                m_stack[m_stack_top] = Fa_Value::nil();
                m_stack_top++;
            }

            for (int i = argc; i < local_count; i++)
                m_stack[call_base + i] = Fa_Value::nil();

            m_frames[m_frames_top] = Fa_CallFrame(fn, fchk, 0,
                static_cast<u16>(call_base), static_cast<u16>(local_count),
                static_cast<u16>(call_base - 1), static_cast<u16>(caller_stack_top));
            m_frames_top++;
        }
        return;
    }

    if (callee.is_native()) {
        Fa_ObjNative* nat = callee.as_native();
        if (nat->arity >= 0 && argc != nat->arity) {
            std::string name = (nat->name ? std::string(nat->name->str.data(), nat->name->str.len()) : "?");
            runtime_error(ErrorCode::NATIVE_ARG_COUNT,
                std::string("native '") + name + "' expected " + std::to_string(nat->arity) + " args, got " + std::to_string(argc));
        }

        m_stack[call_base - 1] = call_native(nat, argc, call_base);
        return;
    }

    if (callee.is_class()) {
        Fa_ObjClass* klass = callee.as_class();
        Fa_ObjInstance* inst = m_gc.make_obj_instance(klass);
        Fa_Value instance = Fa_Value::from_obj(reinterpret_cast<Fa_ObjHeader*>(inst));

        int ctor_slot = klass->method_slot(Fa_StringRef { "بداية" });
        if (ctor_slot < 0)
            ctor_slot = klass->method_slot(Fa_StringRef { "init" });

        if (ctor_slot < 0) {
            if (argc != 0)
                runtime_error(ErrorCode::WRONG_ARG_COUNT);
            if (tail && m_frames_top > 0) {
                Fa_CallFrame finished = frame();
                m_stack[finished.return_slot] = instance;
                m_frames_top -= 1;
                m_stack_top = finished.caller_stack_top;
                return;
            }
            m_stack[call_base - 1] = instance;
            return;
        }

        Fa_Chunk* ctor_chunk = klass->vtable[static_cast<u32>(ctor_slot)];
        if (UNLIKELY(argc + 1 != ctor_chunk->arity))
            runtime_error(ErrorCode::WRONG_ARG_COUNT);

        int local_count = ctor_chunk->local_count;

        // NEW: honor tail — reuse the caller's own frame slot instead of
        // stacking a fresh one, exactly like the Fa_is_function tail path does.
        Fa_CallFrame old_frame;
        if (tail && m_frames_top > 0)
            old_frame = m_frames[m_frames_top - 1];
        int dest_base = (tail && m_frames_top > 0) ? old_frame.base : call_base;
        int caller_stack_top = m_stack_top;

        if (UNLIKELY(m_stack_top + 1 >= STACK_SIZE))
            runtime_error(ErrorCode::STACK_OVERFLOW);

        if (tail && dest_base < call_base) {
            for (int i = 0; i < argc; ++i)
                m_stack[dest_base + i + 1] = m_stack[call_base + i];
        } else {
            for (int i = argc - 1; i >= 0; --i)
                m_stack[dest_base + i + 1] = m_stack[call_base + i];
        }
        m_stack[dest_base] = instance;

        int new_top = dest_base + local_count + 1;
        if (UNLIKELY(new_top > STACK_SIZE))
            runtime_error(ErrorCode::STACK_OVERFLOW);
        while (m_stack_top < new_top) {
            m_stack[m_stack_top] = Fa_Value::nil();
            m_stack_top++;
        }
        for (int i = argc + 1; i < local_count; i++)
            m_stack[dest_base + i] = Fa_Value::nil();

        if (tail && m_frames_top > 0) {
            m_frames[m_frames_top - 1] = Fa_CallFrame(nullptr, ctor_chunk, 0,
                static_cast<u16>(dest_base), static_cast<u16>(local_count),
                old_frame.return_slot, old_frame.caller_stack_top);
            m_stack_top = new_top;
        } else {
            if (UNLIKELY(m_frames_top >= MAX_FRAMES))
                runtime_error(ErrorCode::STACK_OVERFLOW);
            m_frames[m_frames_top] = Fa_CallFrame(nullptr, ctor_chunk, 0,
                static_cast<u16>(dest_base), static_cast<u16>(local_count),
                static_cast<u16>(call_base - 1), static_cast<u16>(caller_stack_top));
            m_frames_top++;
        }
        return;
    }

    if (callee.is_instance()) {
        Fa_ObjInstance* inst = callee.as_instance();
        int slot = inst->klass->method_slot(sp_method_name(Fa_ObjClass::CALL));
        if (UNLIKELY(slot < 0 || static_cast<u32>(slot) >= inst->klass->vtable.size()
                || inst->klass->vtable[static_cast<u32>(slot)] == nullptr))
            runtime_error(ErrorCode::NON_FUNCTION_CALL);

        Fa_Chunk* target = inst->klass->vtable[static_cast<u32>(slot)];
        if (UNLIKELY(argc + 1 != target->arity))
            runtime_error(ErrorCode::WRONG_ARG_COUNT);

        if (tail && m_frames_top > 0) {
            Fa_CallFrame old_frame = m_frames[m_frames_top - 1];
            int dest_base = old_frame.base;
            int new_top = dest_base + static_cast<int>(target->local_count) + 1;
            if (UNLIKELY(new_top > STACK_SIZE))
                runtime_error(ErrorCode::STACK_OVERFLOW);
            for (int i = 0; i < argc; ++i)
                m_stack[dest_base + i + 1] = m_stack[call_base + i];
            m_stack[dest_base] = callee;
            for (int i = argc + 1; i < static_cast<int>(target->local_count); ++i)
                m_stack[dest_base + i] = Fa_Value::nil();
            m_frames[m_frames_top - 1] = Fa_CallFrame(nullptr, target, 0,
                static_cast<u16>(dest_base), static_cast<u16>(target->local_count),
                old_frame.return_slot, old_frame.caller_stack_top);
            m_stack_top = new_top;
            return;
        }

        int caller_stack_top = m_stack_top;
        int method_base = caller_stack_top;
        int new_top = method_base + static_cast<int>(target->local_count) + 1;
        if (UNLIKELY(m_frames_top >= MAX_FRAMES || new_top > STACK_SIZE))
            runtime_error(ErrorCode::STACK_OVERFLOW);
        m_stack[method_base] = callee;
        for (int i = 0; i < argc; ++i)
            m_stack[method_base + i + 1] = m_stack[call_base + i];
        for (int i = argc + 1; i < static_cast<int>(target->local_count); ++i)
            m_stack[method_base + i] = Fa_Value::nil();
        m_stack_top = new_top;
        m_frames[m_frames_top++] = Fa_CallFrame(nullptr, target, 0,
            static_cast<u16>(method_base), static_cast<u16>(target->local_count),
            static_cast<u16>(call_base - 1), static_cast<u16>(caller_stack_top));
        return;
    }

    Fa_StringRef fn_name = "";
    if (callee.is_function())
        fn_name = callee.as_func()->name();

    runtime_error(ErrorCode::NON_FUNCTION_CALL, std::string(fn_name.data(), fn_name.len()));
}

Fa_Value Fa_VM::call_native(Fa_ObjNative* nat, int argc, int call_base)
{
    return (this->*nat->fn)(argc, &m_stack[call_base]);
}

Fa_Value Fa_VM::call_special_sync(Fa_Value receiver, int special_slot)
{
    if (!receiver.is_instance() || m_frames_top == 0)
        return Fa_Value::nil();

    Fa_ObjInstance* instance = receiver.as_instance();
    Fa_StringRef name = sp_method_name(special_slot);
    int slot = instance->klass->method_slot(name);
    if (slot < 0 || static_cast<u32>(slot) >= instance->klass->vtable.size())
        return Fa_Value::nil();
    Fa_Chunk* target = instance->klass->vtable[static_cast<u32>(slot)];
    if (target == nullptr || target->arity != 1)
        return Fa_Value::nil();

    int stop_depth = m_frames_top;
    int caller_top = m_stack_top;
    int result_slot = caller_top;
    int call_base = caller_top + 1;
    invoke_method(target, receiver, result_slot, call_base, 1,
        frame().ip, caller_top);
    return execute(stop_depth);
}

Fa_ObjString* Fa_VM::intern(Fa_StringRef const& str)
{
    if (Fa_ObjString** existing = m_string_table.find_ptr(str))
        return *existing;

    Fa_ObjString* obj = m_gc.make_obj_string(str);
    m_string_table.insert_or_assign(str, obj);
    return obj;
}

void Fa_VM::intern_chunk_constants(Fa_Chunk* ch)
{
    if (ch == nullptr)
        return;

    for (u32 i = 0; i < ch->constants.size(); i++) {
        if (ch->constants[i].is_string())
            ch->constants[i] = Fa_Value::from_obj(reinterpret_cast<Fa_ObjHeader*>(intern(ch->constants[i].as_string()->str)));
    }

    for (auto* fn : ch->functions)
        intern_chunk_constants(fn);
}

void Fa_VM::open_stdlib()
{
    // Collections
    assert(register_native("طول", &Fa_VM::Fa_len, 1) && "Failed to register native 'len'");
    assert(register_native("اضف", &Fa_VM::Fa_append, -1) && "Failed to register native 'append'");
    assert(register_native("احذف", &Fa_VM::Fa_pop, 1) && "Failed to register native 'pop'");
    assert(register_native("مقطع", &Fa_VM::Fa_slice, -1) && "Failed to register native 'slice'");
    assert(register_native("قائمة", &Fa_VM::Fa_list, -1) && "Failed to register native 'list'");
    assert(register_native("قاموس", &Fa_VM::Fa_dict, -1) && "Failed to register native 'dict'");
    // I/O
    assert(register_native("اكتب", &Fa_VM::Fa_print, -1) && "Failed to register native 'print'");
    assert(register_native("ادخل", &Fa_VM::Fa_input, 0) && "Failed to register native 'input'");
    assert(register_native("افتح", &Fa_VM::Fa_open, 2) && "Failed to register native 'open'");
    assert(register_native("اضف_ملف", &Fa_VM::Fa_append_file, 2) && "Failed to register native 'append_file'");
    assert(register_native("اغلق", &Fa_VM::Fa_close, 1) && "Failed to register native 'close'");
    // Type system / conversion
    assert(register_native("صنف", &Fa_VM::Fa_type, 1) && "Failed to register native 'type'");
    assert(register_native("طبيعي", &Fa_VM::Fa_int, 1) && "Failed to register native 'int'");
    assert(register_native("حقيقي", &Fa_VM::Fa_float, 1) && "Failed to register native 'float'");
    assert(register_native("سلسلة", &Fa_VM::Fa_str, -1) && "Failed to register native 'str'");
    assert(register_native("منطقي", &Fa_VM::Fa_bool, 1) && "Failed to register native 'bool'");
    // String ops
    assert(register_native("اقسم", &Fa_VM::Fa_split, 2) && "Failed to register native 'split'");
    assert(register_native("اجمع", &Fa_VM::Fa_join, 2) && "Failed to register native 'join'");
    assert(register_native("جزء", &Fa_VM::Fa_substr, 3) && "Failed to register native 'substr'");
    assert(register_native("يحتوي", &Fa_VM::Fa_contains, 2) && "Failed to register native 'contains'");
    assert(register_native("قص", &Fa_VM::Fa_trim, 1) && "Failed to register native 'trim'");
    // Math
    assert(register_native("ادنى", &Fa_VM::Fa_floor, 1) && "Failed to register native 'floor'");
    assert(register_native("اعلى", &Fa_VM::Fa_ceil, 1) && "Failed to register native 'ceil'");
    assert(register_native("تقريب", &Fa_VM::Fa_round, 1) && "Failed to register native 'round'");
    assert(register_native("مطلق", &Fa_VM::Fa_abs, 1) && "Failed to register native 'abs'");
    assert(register_native("اصغر", &Fa_VM::Fa_min, -1) && "Failed to register native 'min'");
    assert(register_native("اكبر", &Fa_VM::Fa_max, -1) && "Failed to register native 'max'");
    assert(register_native("قوة", &Fa_VM::Fa_pow, 2) && "Failed to register native 'pow'");
    assert(register_native("جذر", &Fa_VM::Fa_sqrt, 1) && "Failed to register native 'sqrt'");
    // Runtime / diagnostics
    assert(register_native("تاكد", &Fa_VM::Fa_assert, -1) && "Failed to register native 'assert'");
    assert(register_native("ساعة", &Fa_VM::Fa_clock, 0) && "Failed to register native 'clock'");
    assert(register_native("عطل", &Fa_VM::Fa_error, -1) && "Failed to register native 'error'");
    assert(register_native("وقت", &Fa_VM::Fa_time, 0) && "Failed to register native 'time'");
}

bool Fa_VM::register_native(Fa_StringRef const& name, NativeFn fn, int arity)
{
    Fa_ObjString* name_obj = m_gc.make_obj_string(name);
    Fa_Value val = m_gc.make_native(fn, name_obj, arity);

    if (u32* slot = m_global_index.find_ptr(name)) {
        m_global_slots[*slot] = val;
    } else {
        auto slot_idx = m_global_slots.size();
        m_global_slots.push(val);
        m_global_index.insert_or_assign(name, slot_idx);
    }

    return true;
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

void Fa_VM::_runtime_error(u16 errc, std::string const& detail)
{
    Fa_SourceLocation loc = current_location();
    auto id = diagnostic::report_deferred(diagnostic::Severity::ERROR, loc, static_cast<u16>(errc), detail);

    int frame_no = 0;
    for (int i = m_frames_top - 1; i >= 0; i -= 1) {
        Fa_CallFrame* p = &m_frames[i];
        if (p->chunk == nullptr)
            continue;

        Fa_Chunk const& ch = *p->chunk;
        size_t off = p->ip > 0 ? p->ip - 1 : 0;
        if (off >= ch.locations.size())
            continue;

        Fa_SourceLocation frame_loc = ch.locations[off];
        std::string note = "stack trace #" + std::to_string(frame_no++) + ": at "
            + std::to_string(frame_loc.line) + ":" + std::to_string(frame_loc.column);
        if (p->func) {
            Fa_StringRef fname = p->func->name();
            note = "in '" + std::string(fname.data(), fname.len()) + "' " + note;
        } else if (!p->chunk->name.empty()) {
            note = "in '" + std::string(p->chunk->name.data(), p->chunk->name.len()) + "' " + note;
        }

        diagnostic::engine.add_note(id, frame_loc.line, note);
    }

    diagnostic::dump();
    halt();
}

void Fa_VM::runtime_error(ErrorCode errc, std::string const& detail)
{
    _runtime_error(static_cast<u16>(errc), detail);
}

void Fa_VM::stdlib_error(diagnostic::errc::stdlib::Code errc, std::string const& detail)
{
    _runtime_error(static_cast<u16>(errc), detail);
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
Fa_Value& Fa_VM::reg(int r) { return m_stack[top_frame().base + r]; }

void Fa_VM::update_ic_binary(Fa_Chunk* ch, u32 nop_ip, Fa_Value lhs, Fa_Value rhs, Fa_Value result)
{
    if (ch == nullptr)
        return;

    u32 nop = ch->code[nop_ip];
    u8 ic_idx = Fa_instr_A(nop);

    if (ic_idx < ch->ic_slots.size()) {
        Fa_ICSlot& slot = ch->ic_slots[ic_idx];
        slot.seen_lhs |= static_cast<u8>(value_type_tag(lhs));
        slot.seen_rhs |= static_cast<u8>(value_type_tag(rhs));
        slot.seen_ret |= static_cast<u8>(value_type_tag(result));
        slot.hit_count++;
    }
}

} // namespace fairuz::runtime
