//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Fairuz bytecode execution dispatch loop.
//===----------------------------------------------------------------------===//

#include "Compiler/chunk.hpp"
#include "Interpreter/gc.hpp"
#include "Interpreter/object.hpp"
#include "Interpreter/vm.hpp"
#include "Macros.hpp"
#include "Util/utf8.hpp"

#include <cmath>
#include <set>

/// Only routines related to case dispatch can use macros for maximum efficiency
/// all other routines should use normal helper functions, unless you know better
#define DISPATCH()                                                                                      \
    do {                                                                                                \
        if (UNLIKELY(m_gc.should_collect())) m_gc.collect(this);                                        \
        if (UNLIKELY(ip >= cur_chunk->code.size()))                                                     \
            return raise_runtime_error(ErrorCode::INVALID_OPCODE, "instruction pointer out of bounds"); \
        instr = cur_chunk->code[ip];                                                                    \
        ip++;                                                                                           \
        SAVE_IP();                                                                                      \
        u8 opcode = static_cast<u8>(instr_op(instr));                                                   \
        if (UNLIKELY(opcode >= static_cast<u8>(OpCode::_COUNT)))                                        \
            return raise_runtime_error(ErrorCode::INVALID_OPCODE);                                      \
        goto* dispatch_table[opcode];                                                                   \
    } while (0)

#define BEGIN_DISPATCH() DISPATCH()
#define END_DISPATCH()
#define CASE(op) H_##op:

#define RA() cur_base[instr_A(instr)]
#define RB() cur_base[instr_B(instr)]
#define RC() cur_base[instr_C(instr)]

#define LOAD_FRAME()                 \
    do {                             \
        CallFrame& f = frame();      \
        cur_chunk = f.chunk;         \
        cur_frame_base = f.base;     \
        cur_base = &m_stack[f.base]; \
        ip = f.ip.value;             \
    } while (0)

#define SAVE_IP()                   \
    do {                            \
        frame().ip = PCType { ip }; \
    } while (0)

#define REQUIRE_NUMBER(v)                                                            \
    do {                                                                             \
        if (!v.is_number()) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH); \
    } while (0)

namespace fairuz::runtime {

namespace {

using ComparedObjects = std::set<std::pair<ObjHeader const*, ObjHeader const*>>;

// checks whether or not two values are in fact equal, since two values
// might be distinct object but wrap logically equal data, therefor
// this checks logical equality rather than pointer equality
bool values_equal_impl(Value lhs, Value rhs, ComparedObjects& seen)
{
    if (lhs.is_int() && rhs.is_int()) return integer::compare(lhs, rhs) == 0;
    if (lhs.is_number() && rhs.is_number()) return integer::compare_numbers(lhs, rhs) == 0;
    if ((lhs.is_obj() && rhs.is_obj()) && (lhs.as_obj() == rhs.as_obj())) return true;
    if (lhs.is_nil() || rhs.is_nil()) return lhs.is_nil() && rhs.is_nil();
    if (lhs.is_bool() || rhs.is_bool()) return lhs.is_bool() && rhs.is_bool() && lhs.as_bool() == rhs.as_bool();
    if (lhs.is_string() && rhs.is_string()) return lhs.as_string()->str == rhs.as_string()->str;
    if (!lhs.is_obj() || !rhs.is_obj() || lhs.as_obj()->type != rhs.as_obj()->type) return false;

    auto pair
        = std::make_pair(static_cast<ObjHeader const*>(lhs.as_obj()), static_cast<ObjHeader const*>(rhs.as_obj()));
    if (!seen.insert(pair).second) return true;

    if (lhs.is_list()) {
        ObjList* left = lhs.as_list();
        ObjList* right = rhs.as_list();
        if (left->elements.size() != right->elements.size()) return false;
        for (u32 i = 0; i < left->elements.size(); ++i) {
            if (!values_equal_impl(left->elements[i], right->elements[i], seen)) return false;
        }
        return true;
    }
    if (lhs.is_dict()) {
        ObjDict* left = lhs.as_dict();
        ObjDict* right = rhs.as_dict();
        if (left->data.size() != right->data.size()) return false;
        for (auto const& [key, value] : left->data) {
            Value const* other = right->data.find_ptr(key);
            if (other == nullptr || !values_equal_impl(value, *other, seen)) return false;
        }
        return true;
    }

    // Distinct functions, classes, instances, modules and resources are unequal.
    // Numeric comparisons above deliberately precede identity so NaN != NaN.
    return false;
}

bool values_equal(Value lhs, Value rhs)
{
    ComparedObjects seen;
    return values_equal_impl(lhs, rhs, seen);
}

} // anonymous namespace

#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpedantic"
#endif

VM::RuntimeResult VM::execute(int stop_frame_depth)
{
    static void* dispatch_table[] = {
#define X(name) &&H_##name,
        FA_OPCODE_LIST(X)
#undef X
    };

    if (m_frames_top == 0) return Value::nil();

    u32 instr;
    Chunk* cur_chunk = frame().chunk;
    int cur_frame_base = frame().base;
    Value* cur_base = &m_stack[cur_frame_base];
    u32 ip = frame().ip.value; // Raw cursor for dispatch arithmetic; frames store a typed PC.

    BEGIN_DISPATCH();

    CASE(LOAD_NIL)
    {
        u8 start = instr_B(instr);
        u8 count = instr_C(instr);

        for (u8 i = 0; i < count; i++) cur_base[start + i] = Value::nil();

        DISPATCH();
    }
    CASE(LOAD_TRUE)
    {
        RA() = Value::from_bool(true);
        DISPATCH();
    }
    CASE(LOAD_FALSE)
    {
        RA() = Value::from_bool(false);
        DISPATCH();
    }
    CASE(LOAD_CONST)
    {
        u16 index = instr_Bx(instr);
        if (UNLIKELY(index >= cur_chunk->constants.size()))
            return raise_runtime_error(ErrorCode::INVALID_OPCODE, "constant index out of bounds");
        RA() = cur_chunk->constants[index];
        DISPATCH();
    }
    CASE(LOAD_INT)
    {
        RA() = Value::from_int(static_cast<i32>(static_cast<u16>(instr_Bx(instr))) - JUMP_OFFSET);
        DISPATCH();
    }
    CASE(LOAD_BIG_INT)
    {
        RA() = integer::finish(cur_chunk->big_ints[instr_Bx(instr)], m_gc);
        DISPATCH();
    }
    CASE(LOAD_GLOBAL)
    {
        {
            u16 index = instr_Bx(instr);
            if (UNLIKELY(index >= cur_chunk->constants.size() || !cur_chunk->constants[index].is_string()))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid global-name constant");
            Value name_v = cur_chunk->constants[index];
            StringRef name = name_v.as_string()->str;
            Value const* value = find_global(current_globals(), name);
            if (value == nullptr)
                return raise_runtime_error(ErrorCode::UNDEFINED_GLOBAL, std::string(name.data(), name.len()));
            RA() = *value;
        }
        DISPATCH();
    }
    CASE(LOAD_GLOBAL_CACHED)
    {
        u16 slot_idx = instr_Bx(instr);
        GlobalEnvironment* env = current_globals();
        if (UNLIKELY(env == nullptr || slot_idx >= env->slots.size()))
            return raise_runtime_error(ErrorCode::UNDEFINED_GLOBAL);
        RA() = env->slots[slot_idx];
        DISPATCH();
    }
    CASE(STORE_GLOBAL)
    {
        {
            u16 index = instr_Bx(instr);
            if (UNLIKELY(index >= cur_chunk->constants.size() || !cur_chunk->constants[index].is_string()))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid global-name constant");
            Value name_v = cur_chunk->constants[index];
            StringRef name = name_v.as_string()->str;
            store_global(current_globals(), name, RA());
        }
        DISPATCH();
    }
    CASE(STORE_GLOBAL_CACHED)
    {
        u16 slot_idx = instr_Bx(instr);
        GlobalEnvironment* env = current_globals();
        if (UNLIKELY(env == nullptr || slot_idx >= env->slots.size()))
            return raise_runtime_error(ErrorCode::UNDEFINED_GLOBAL);
        env->slots[slot_idx] = RA();
        DISPATCH();
    }
    CASE(IMPORT_MODULE)
    {
        ObjModule* module = nullptr;
        {
            u16 index = instr_Bx(instr);
            if (UNLIKELY(index >= cur_chunk->constants.size() || !cur_chunk->constants[index].is_string()))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid module-name constant");
            auto load_result = load_module(cur_chunk->constants[index].as_string()->str);
            if (load_result.has_error()) return load_result.error();

            module = load_result.value();
        }
        RA() = Value::from_module(module);

        if (module->initialized || module->executing) {
            DISPATCH();
        }

        module->executing = true;
        if (UNLIKELY(module->chunk == nullptr || m_frames_top >= MAX_FRAMES))
            return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid imported module");
        int result_slot = cur_frame_base + instr_A(instr);
        int module_base = m_stack_top;
        int new_top = module_base + static_cast<int>(module->chunk->local_count) + 1;
        if (UNLIKELY(new_top > STACK_SIZE)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);
        while (m_stack_top < new_top) m_stack[m_stack_top++] = Value::nil();
        ObjFunction* module_fn = m_gc.make_obj_function(module->chunk, module->globals);
        SAVE_IP();
        m_frames[m_frames_top++] = CallFrame(module_fn, module->chunk, PCType { 0 }, static_cast<u16>(module_base),
            static_cast<u16>(module->chunk->local_count), static_cast<u16>(result_slot), static_cast<u16>(module_base),
            module->globals, module);
        intern_chunk_constants(module->chunk);
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(MOVE)
    {
        RA() = RB();
        DISPATCH();
    }
    CASE(OP_ADD)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        bool both_str = lhs.is_string() && rhs.is_string();

        if (both_str) {
            // String concatenation
            res = m_gc.make_string(lhs.as_string()->str + rhs.as_string()->str);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_int() && rhs.is_int()) {
            // Integer addition
            res = integer::add(lhs, rhs, m_gc);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            // Float addition (includes mixed int/float)
            res = Value::from_real(lhs.as_double_any() + rhs.as_double_any());
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::ADD, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_SUB)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = integer::sub(lhs, rhs, m_gc);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_real(lhs.as_double_any() - rhs.as_double_any());
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::SUB, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_MUL)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = integer::mul(lhs, rhs, m_gc);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_real(lhs.as_double_any() * rhs.as_double_any());
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::MUL, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_DIV)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            if (!rhs.is_truthy()) return raise_runtime_error(ErrorCode::DIVISION_BY_ZERO);
            res = integer::div(lhs, rhs, m_gc);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            if (rhs.as_double_any() == 0.0) return raise_runtime_error(ErrorCode::DIVISION_BY_ZERO);
            res = Value::from_real(lhs.as_double_any() / rhs.as_double_any());
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::DIV, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_MOD)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            if (!rhs.is_truthy()) return raise_runtime_error(ErrorCode::MODULO_BY_ZERO);
            res = integer::div(lhs, rhs, m_gc, true);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            if (rhs.as_double_any() == 0.0) return raise_runtime_error(ErrorCode::MODULO_BY_ZERO);
            res = Value::from_real(std::fmod(lhs.as_double_any(), rhs.as_double_any()));
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::MOD, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_POW)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        REQUIRE_NUMBER(lhs);
        REQUIRE_NUMBER(rhs);

        res = lhs.is_int() && rhs.is_int() ? integer::pow(lhs, rhs, m_gc)
                                           : Value::from_real(std::pow(lhs.as_double_any(), rhs.as_double_any()));
        DISPATCH();
    }
    CASE(OP_NEG)
    {
        Value& res = RA();
        Value operand = RB();

        if (operand.is_int()) {
            res = integer::neg(operand, m_gc);
        } else if (operand.is_double()) {
            res = Value::from_real(-operand.as_double_any());
        } else if (operand.is_instance()) {
            ObjInstance* instance = operand.as_instance();
            ObjClass* self_klass = instance->klass;

            int slot = self_klass->method_slot(sp_method_name(ObjClass::NEG));

            if (UNLIKELY(slot < 0)) return raise_runtime_error(ErrorCode::UNDEFINED_METHOD);
            if (UNLIKELY(static_cast<u32>(slot) >= self_klass->vtable.size()))
                return raise_runtime_error(ErrorCode::UNDEFINED_METHOD);

            Chunk* target_chunk = self_klass->vtable[static_cast<u32>(slot)];
            int caller_stack_top = m_stack_top;
            int call_base = caller_stack_top;

            if (UNLIKELY(call_base + 1 >= STACK_SIZE)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);
            {
                auto invoke_result = invoke_method(target_chunk, operand, cur_frame_base + instr_A(instr), call_base, 1,
                    ip, caller_stack_top, target_chunk->globals);
                if (invoke_result.has_error()) return invoke_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_BITAND)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (UNLIKELY(!lhs.is_int() || !rhs.is_int())) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        res = integer::bitwise(lhs, rhs, '&', m_gc);
        DISPATCH();
    }
    CASE(OP_BITOR)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (UNLIKELY(!lhs.is_int() || !rhs.is_int())) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        res = integer::bitwise(lhs, rhs, '|', m_gc);
        DISPATCH();
    }
    CASE(OP_BITXOR)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (UNLIKELY(!lhs.is_int() || !rhs.is_int())) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        res = integer::bitwise(lhs, rhs, '^', m_gc);
        DISPATCH();
    }
    CASE(OP_BITNOT)
    {
        Value& res = RA();
        Value operand = RB();
        if (UNLIKELY(!operand.is_int())) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        res = integer::bitwise(operand, Value::from_int(-1), '^', m_gc);
        DISPATCH();
    }
    CASE(OP_LSHIFT)
    CASE(OP_LSHIFT_REG)
    {
        Value& res = RA();
        Value lhs = RB();
        if (UNLIKELY(!lhs.is_int())) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        Value count = Value::from_int(instr_C(instr));
        if (instr_op(instr) == OpCode::OP_LSHIFT_REG) {
            if (!RC().is_int()) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
            count = RC();
        }
        if (integer::compare(count, Value::from_int(0)) < 0)
            return raise_runtime_error(ErrorCode::SHIFT_AMOUNT_OUT_OF_RANGE);
        res = integer::shift(lhs, count, true, m_gc);
        DISPATCH();
    }
    CASE(OP_RSHIFT)
    CASE(OP_RSHIFT_REG)
    {
        Value& res = RA();
        Value lhs = RB();
        if (UNLIKELY(!lhs.is_int())) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);

        Value count = Value::from_int(instr_C(instr));
        if (instr_op(instr) == OpCode::OP_RSHIFT_REG) {
            if (!RC().is_int()) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
            count = RC();
        }
        if (integer::compare(count, Value::from_int(0)) < 0)
            return raise_runtime_error(ErrorCode::SHIFT_AMOUNT_OUT_OF_RANGE);
        res = integer::shift(lhs, count, false, m_gc);
        DISPATCH();
    }
    CASE(OP_EQ)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        bool has_overload = false;
        if (lhs.is_instance()) has_overload = lhs.as_instance()->klass->method_slot(sp_method_name(ObjClass::EQ)) >= 0;
        if (!has_overload && rhs.is_instance())
            has_overload = rhs.as_instance()->klass->method_slot(sp_method_name(ObjClass::EQ)) >= 0;
        if (has_overload) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::EQ, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        }
        res = Value::from_bool(values_equal(lhs, rhs));
        update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);

        DISPATCH();
    }
    CASE(OP_NEQ)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        bool has_overload = false;
        if (lhs.is_instance()) has_overload = lhs.as_instance()->klass->method_slot(sp_method_name(ObjClass::NEQ)) >= 0;
        if (!has_overload && rhs.is_instance())
            has_overload = rhs.as_instance()->klass->method_slot(sp_method_name(ObjClass::NEQ)) >= 0;
        if (has_overload) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::NEQ, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        }
        res = Value::from_bool(!values_equal(lhs, rhs));
        update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);

        DISPATCH();
    }
    CASE(OP_LT)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = Value::from_bool(integer::compare(lhs, rhs) < 0);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Value::from_bool(lhs.as_string()->str < rhs.as_string()->str);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_bool(integer::compare_numbers(lhs, rhs) == -1);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::LT, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        DISPATCH();
    }
    CASE(OP_LTE)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = Value::from_bool(integer::compare(lhs, rhs) <= 0);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Value::from_bool(lhs.as_string()->str <= rhs.as_string()->str);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_bool(
                (integer::compare_numbers(lhs, rhs) == -1 || integer::compare_numbers(lhs, rhs) == 0));
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::LTE, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        DISPATCH();
    }
    CASE(OP_GT)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = Value::from_bool(integer::compare(lhs, rhs) > 0);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Value::from_bool(lhs.as_string()->str > rhs.as_string()->str);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_bool(integer::compare_numbers(lhs, rhs) == 1);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::GT, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        DISPATCH();
    }
    CASE(OP_GTE)
    {
        u32 payload_ip = ip;

        if (ip < cur_chunk->code.size() && instr_op(cur_chunk->code[ip]) == OpCode::NOP) ++ip;

        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = Value::from_bool(integer::compare(lhs, rhs) >= 0);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Value::from_bool(lhs.as_string()->str >= rhs.as_string()->str);
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_bool(
                (integer::compare_numbers(lhs, rhs) == 1 || integer::compare_numbers(lhs, rhs) == 0));
            update_ic_binary(cur_chunk, payload_ip, lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            {
                auto op_result = run_instance_op(lhs, rhs, ObjClass::GTE, instr, cur_frame_base, ip);
                if (op_result.has_error()) return op_result.error();
            }
            LOAD_FRAME();
            DISPATCH();
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        DISPATCH();
    }
    CASE(OP_NOT)
    {
        RA() = Value::from_bool(!RB().is_truthy());
        DISPATCH();
    }
    CASE(CONCAT) { DISPATCH(); }
    CASE(LIST_NEW)
    {
        ObjList* list_obj = m_gc.make_obj_list();
        list_obj->reserve(instr_B(instr));
        RA() = Value::from_obj(reinterpret_cast<ObjHeader*>(list_obj));
        DISPATCH();
    }
    CASE(LIST_APPEND)
    {
        Value& list_v = RA();
        if (!list_v.is_list())
            return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL, "attempting to append on a non list");

        list_v.as_list()->elements.push(RB());
        DISPATCH();
    }
    CASE(LIST_GET)
    {
        Value& res = RA();
        Value list_v = RB();
        Value index_v = RC();

        if (!list_v.is_list()) return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL, "attempting get on a non list");
        if (!index_v.is_int())
            return raise_runtime_error(ErrorCode::INDEX_TYPE_ERROR, "attempting get with a non integer index");

        auto& elems = list_v.as_list()->elements;
        i64 idx = index_v.as_int();
        if (idx < 0 || idx >= static_cast<i64>(elems.size()))
            return raise_runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        res = elems[static_cast<u32>(idx)];
        DISPATCH();
    }
    CASE(LIST_SET)
    {
        Value& object_v = RA();
        Value index_v = RB();
        Value new_val = RC();

        if (object_v.is_list()) {
            if (!index_v.is_int()) return raise_runtime_error(ErrorCode::INDEX_TYPE_ERROR);

            auto& elems = object_v.as_list()->elements;
            i64 idx = index_v.as_int();

            if (idx < 0) idx += static_cast<i64>(elems.size());
            if (idx < 0 || idx >= static_cast<i64>(elems.size()))
                return raise_runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS,
                    "index " + std::to_string(index_v.as_int()) + " for list of length "
                        + std::to_string(elems.size()));

            elems[static_cast<u32>(idx)] = new_val;
        } else {
            return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL, "attempting set on a non list value");
        }

        DISPATCH();
    }
    CASE(LIST_LEN)
    {
        Value list_v = RB();
        if (!list_v.is_list())
            return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL, "attempting len on a non list value");

        RA() = Value::from_int(static_cast<i64>(list_v.as_list()->elements.size()));
        DISPATCH();
    }
    CASE(JUMP)
    {
        ip += instr_sBx(instr);
        DISPATCH();
    }
    CASE(JUMP_IF_TRUE)
    {
        if (RA().is_truthy()) ip += instr_sBx(instr);

        DISPATCH();
    }
    CASE(JUMP_IF_FALSE)
    {
        if (!RA().is_truthy()) ip += instr_sBx(instr);

        DISPATCH();
    }
    CASE(LOOP)
    {
        ip += instr_sBx(instr);
        DISPATCH();
    }
    CASE(FOR_PREP)
    {
        u8 base_reg = instr_A(instr);
        Value init_v = cur_base[base_reg];
        Value limit_v = cur_base[base_reg + 1];
        Value step_v = cur_base[base_reg + 2];

        if (!init_v.is_int() || !limit_v.is_int() || !step_v.is_int())
            return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
        if (!step_v.is_truthy()) return raise_runtime_error(ErrorCode::DIVISION_BY_ZERO);

        cur_base[base_reg] = limit_v;
        cur_base[base_reg + 1] = step_v;
        cur_base[base_reg + 2] = init_v;
        bool enters = integer::compare(step_v, Value::from_int(0)) > 0 ? integer::compare(init_v, limit_v) <= 0
                                                                       : integer::compare(init_v, limit_v) >= 0;
        if (!enters) ip += instr_sBx(instr);

        DISPATCH();
    }
    CASE(FOR_STEP)
    {
        u8 base_reg = instr_A(instr);
        Value limit_v = cur_base[base_reg];
        Value step_v = cur_base[base_reg + 1];
        Value control_v = cur_base[base_reg + 2];

        if (control_v.is_int()) {
            Value control_value = integer::add(control_v, step_v, m_gc);
            cur_base[base_reg + 2] = control_value;
            bool continues = integer::compare(step_v, Value::from_int(0)) > 0
                ? integer::compare(control_value, limit_v) <= 0
                : integer::compare(control_value, limit_v) >= 0;
            if (continues) ip += instr_sBx(instr);
        } else {
            f64 control = control_v.as_double_any() + step_v.as_double_any();
            cur_base[base_reg + 2] = Value::from_real(control);
            bool continues = step_v.as_double() > 0.0 ? control <= limit_v.as_double() : control >= limit_v.as_double();
            if (continues) ip += instr_sBx(instr);
        }

        DISPATCH();
    }
    CASE(CLOSURE)
    {
        u16 fn_idx = instr_Bx(instr);
        if (UNLIKELY(fn_idx >= cur_chunk->functions.size() || cur_chunk->functions[fn_idx] == nullptr))
            return raise_runtime_error(ErrorCode::INVALID_OPCODE, "function index out of bounds");
        Chunk* fn_chunk = cur_chunk->functions[fn_idx];
        RA() = m_gc.make_function(fn_chunk, current_globals());
        DISPATCH();
    }
    CASE(CALL)
    {
        u8 fn_reg = instr_A(instr);
        u8 argc = instr_B(instr);
        Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;
        SAVE_IP();
        {
            auto call_result = call_value(callee, argc, base, false);
            if (call_result.has_error()) return call_result.error();
        }
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(CALL_TAIL)
    {
        u8 fn_reg = instr_A(instr);
        u8 argc = instr_B(instr);
        Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;

        if (callee.is_native()) {
            ObjBuiltin* nat = callee.as_native();
            if (UNLIKELY(nat->arity >= 0 && argc != nat->arity)) {
                std::string name = (nat->name ? std::string(nat->name->str.data(), nat->name->str.len()) : "?");
                return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
                    std::string("native '") + name + "' expected " + std::to_string(nat->arity) + " args, got "
                        + std::to_string(argc));
            }

            // Builtins run synchronously with no pushed frame, so a tail call to
            // one must perform *this* frame's own return afterward — the
            // compiler omits a trailing RETURN for CALL_TAIL sites, trusting
            // the callee to terminate the frame itself.
            Value ret = Value::nil();
            {
                auto call_result = call_native(nat, argc, base);
                if (call_result.has_error()) return call_result.error();
                ret = call_result.value();
            }

            CallFrame finished = frame();
            m_stack[finished.return_slot] = ret;
            m_frames_top -= 1;
            m_stack_top = finished.caller_stack_top;

            if (LIKELY(m_frames_top == 0)) return ret;

            LOAD_FRAME();
            DISPATCH();
        }

        SAVE_IP();
        {
            auto call_result = call_value(callee, argc, base, true);
            if (call_result.has_error()) return call_result.error();
        }
        // A tail call to a class without a constructor completes synchronously
        // inside call_value().  In the outermost frame there is then no frame
        // for LOAD_FRAME() to read.
        if (UNLIKELY(m_frames_top == 0)) return m_stack[0];
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(IC_CALL)
    {
        u8 fn_reg = instr_A(instr);
        u8 argc = instr_B(instr);
        u8 ic_idx = instr_C(instr);
        u32 call_ip = ip - 1;
        Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;
        bool has_slot = ic_idx < cur_chunk->ic_slots.size();

        if (has_slot) {
            auto& slot = cur_chunk->ic_slots[ic_idx];
            slot.seen_lhs |= static_cast<u16>(value_type_tag(callee));
            slot.hit_count++;
        }

        Chunk* caller_chunk = cur_chunk;
        int result_slot = base - 1;

        SAVE_IP();

        {
            auto call_result = call_value(callee, argc, base, false);
            if (call_result.has_error()) return call_result.error();
        }

        LOAD_FRAME();

        if (has_slot) {
            caller_chunk->ic_slots[ic_idx].seen_ret |= static_cast<u16>(value_type_tag(m_stack[result_slot]));
            caller_chunk->code[call_ip] = make_ABC(OpCode::CALL, fn_reg, argc, 0);
        }

        DISPATCH();
    }
    CASE(RETURN)
    {
        u8 src = instr_A(instr);
        u8 n_ret = instr_B(instr);
        Value ret = n_ret > 0 ? cur_base[src] : Value::nil();
        CallFrame finished = frame();
        if (finished.module != nullptr) {
            finished.module->executing = false;
            finished.module->initialized = true;
            ret = Value::from_module(finished.module);
        }
        m_stack[finished.return_slot] = ret;
        m_frames_top -= 1;
        m_stack_top = finished.caller_stack_top;

        if (m_frames_top == stop_frame_depth) return ret;
        if (LIKELY(m_frames_top == 0)) return ret;

        LOAD_FRAME();
        DISPATCH();
    }
    CASE(RETURN_NIL)
    {
        CallFrame finished = frame();
        Value ret = Value::nil();
        if (finished.module != nullptr) {
            finished.module->executing = false;
            finished.module->initialized = true;
            ret = Value::from_module(finished.module);
        }
        m_stack[finished.return_slot] = ret;
        m_frames_top -= 1;
        m_stack_top = finished.caller_stack_top;

        if (m_frames_top == stop_frame_depth) return ret;
        if (LIKELY(m_frames_top == 0)) return ret;

        LOAD_FRAME();
        DISPATCH();
    }
    CASE(RETURN1)
    {
        Value ret = cur_base[instr_A(instr)];
        CallFrame finished = frame();
        if (finished.module != nullptr) {
            finished.module->executing = false;
            finished.module->initialized = true;
            ret = Value::from_module(finished.module);
        }
        m_stack[finished.return_slot] = ret;
        m_frames_top -= 1;
        m_stack_top = finished.caller_stack_top;

        if (m_frames_top == stop_frame_depth) return ret;
        if (LIKELY(m_frames_top == 0)) return ret;

        LOAD_FRAME();
        DISPATCH();
    }
    CASE(INDEX_READ)
    {
        Value& res = RA();
        Value obj = RB();
        Value idx = RC();

        if (obj.is_string()) {
            if (UNLIKELY(!idx.is_int())) return raise_runtime_error(ErrorCode::INDEX_TYPE_ERROR);

            i64 idx_int = idx.as_int();
            if (UNLIKELY(idx_int < 0)) return raise_runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            StringRef const& str = obj.as_string()->str;

            size_t byte_pos = 0;
            i64 char_pos = 0;
            u64 char_bytes = 0;

            while (byte_pos < str.len() && char_pos < idx_int) {
                u64 step = 0;
                util::decode_utf8_at(str, byte_pos, &step);
                byte_pos += step;
                char_pos++;
            }

            if (UNLIKELY(byte_pos >= str.len())) return raise_runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            util::decode_utf8_at(str, byte_pos, &char_bytes);

            StringRef ch = str.slice(byte_pos, byte_pos + char_bytes);
            res = m_gc.make_string(ch);
        } else if (obj.is_list()) {
            if (UNLIKELY(!idx.is_int())) return raise_runtime_error(ErrorCode::INDEX_TYPE_ERROR);

            i64 idx_int = idx.as_int();
            ListType list = obj.as_list()->elements;
            if (UNLIKELY(idx_int >= list.size() || idx_int < 0))
                return raise_runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS,
                    "index " + std::to_string(idx_int) + " for list of length " + std::to_string(list.size()));

            res = obj.as_list()->elements[idx_int];
        } else if (obj.is_dict()) {
            ObjDict* dict_obj = obj.as_dict();
            if (dict_obj->data.find_ptr(idx) == nullptr) res = Value::nil();
            else res = dict_obj->data[idx];
        } else if (obj.is_instance()) {
            if (!idx.is_string()) return raise_runtime_error(ErrorCode::INDEX_TYPE_ERROR);
            ObjInstance* inst = obj.as_instance();
            int field_idx = inst->klass->field_index(idx.as_string()->str);
            if (field_idx < 0)
                return raise_runtime_error(ErrorCode::UNDEFINED_METHOD); // or a dedicated "no such field" code
            res = inst->fields[field_idx];
        } else {
            return raise_runtime_error(ErrorCode::INDEX_TYPE_ERROR);
        }

        DISPATCH();
    }
    CASE(INDEX_WRITE)
    {
        Value& obj = RA();
        Value idx = RB();
        Value val = RC();

        /// NOTE: 'str' is immutable by design, so INDEX_WRITE falls back to return raise_runtime_error

        if (obj.is_list()) {
            if (!idx.is_int()) return raise_runtime_error(ErrorCode::INDEX_TYPE_ERROR);
            i64 idx_int = idx.as_int();
            ObjList* list = obj.as_list();
            if (idx_int >= list->size() || idx_int < 0) return raise_runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);
            list->elements[idx.as_int()] = val;
        } else if (obj.is_dict()) {
            obj.as_dict()->set(idx, val);
        } else if (obj.is_instance()) {
            /// TODO: go get the '[]' operator
            return raise_runtime_error(ErrorCode::INDEX_OBJECT_TYPE_ERROR);
        } else {
            return raise_runtime_error(ErrorCode::INDEX_OBJECT_TYPE_ERROR);
        }

        DISPATCH();
    }
    CASE(NEW_CLASS)
    {
        {
            Value& res = RA();
            u16 desc_idx = instr_Bx(instr);
            if (UNLIKELY(desc_idx >= cur_chunk->class_descriptors.size()))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "class descriptor index out of bounds");
            ClassDescriptor klass_desc = cur_chunk->class_descriptors[desc_idx];
            ObjClass* parent = nullptr;
            if (!klass_desc.parent_name.empty()) {
                Value const* parent_value = find_global(current_globals(), klass_desc.parent_name);
                if (parent_value == nullptr || !parent_value->is_class())
                    return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL,
                        "parent is not a class: "
                            + std::string(klass_desc.parent_name.data(), klass_desc.parent_name.len()));
                parent = parent_value->as_class();
            }

            Array<StringRef, GarbageCollector> fields { &m_gc };
            Array<StringRef, GarbageCollector> methods { &m_gc };
            Array<Chunk*, GarbageCollector> vtable { &m_gc };

            if (parent != nullptr) {
                for (StringRef const& field : parent->field_names) fields.push(field);
                for (StringRef const& method : parent->method_names) methods.push(method);
                for (Chunk* fn : parent->vtable) vtable.push(fn);
            } else {
                methods.resize(ObjClass::_COUNT);
                vtable.resize(ObjClass::_COUNT);
            }

            for (u32 i = 0; i < klass_desc.field_count; ++i) {
                bool duplicate = false;
                for (StringRef const& inherited : fields) {
                    if (inherited == klass_desc.field_names[i]) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) fields.push(klass_desc.field_names[i]);
            }

            for (u32 i = 0; i < klass_desc.vtable_size; ++i) {
                u32 fn_idx = klass_desc.vtable_indices[i];
                if (UNLIKELY(fn_idx != ClassDescriptor::NULL_SLOT && fn_idx >= cur_chunk->functions.size()))
                    return raise_runtime_error(ErrorCode::INVALID_OPCODE, "class method index out of bounds");
                if (fn_idx == ClassDescriptor::NULL_SLOT || klass_desc.method_names[i].empty()) continue;
                Chunk* method_chunk = cur_chunk->functions[fn_idx];
                method_chunk->globals = current_globals();

                int target = -1;
                if (i < ObjClass::_COUNT) {
                    target = static_cast<int>(i);
                } else {
                    for (u32 j = 0; j < methods.size(); ++j) {
                        if (methods[j] == klass_desc.method_names[i]) {
                            target = static_cast<int>(j);
                            break;
                        }
                    }
                }
                if (target < 0) {
                    target = static_cast<int>(methods.size());
                    methods.push(klass_desc.method_names[i]);
                    vtable.push(nullptr);
                }
                if (static_cast<size_t>(target) >= methods.size()) {
                    methods.resize(static_cast<size_t>(target) + 1);
                    vtable.resize(static_cast<size_t>(target) + 1);
                }
                methods[static_cast<size_t>(target)] = klass_desc.method_names[i];
                vtable[static_cast<size_t>(target)] = method_chunk;
            }

            res = m_gc.make_class(klass_desc.name, fields, methods, vtable);
            res.as_class()->parent = parent;
            res.as_class()->globals = current_globals();
        }

        DISPATCH();
    }
    CASE(NEW_INSTANCE)
    {
        u16 index = instr_Bx(instr);
        if (UNLIKELY(index >= cur_chunk->constants.size() || !cur_chunk->constants[index].is_class()))
            return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid class constant");
        ObjClass* klass = cur_chunk->constants[index].as_class();
        RA() = m_gc.make_instance(klass);
        DISPATCH();
    }
    CASE(INVOKE)
    {
        u8 self_reg = instr_A(instr);
        u8 slot = instr_B(instr);
        u8 argc = instr_C(instr);

        if (UNLIKELY(ip >= cur_chunk->code.size() || instr_op(cur_chunk->code[ip]) != OpCode::NOP))
            return raise_runtime_error(ErrorCode::INVALID_OPCODE, "missing INVOKE cache payload");
        ++ip; // consume the trailing NOP carrying the IC slot index

        Value self_val = cur_base[self_reg];

        if (UNLIKELY(!self_val.is_instance()))
            return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL, "(method call on non-instance)");

        ObjInstance* inst = self_val.as_instance();

        if (UNLIKELY(slot >= inst->klass->vtable.size() || inst->klass->vtable[slot] == nullptr))
            return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL, "method slot is empty");

        {
            auto invoke_result = invoke_method(inst->klass->vtable[slot], self_val, cur_frame_base + self_reg,
                cur_frame_base + self_reg + 1, argc, ip, m_stack_top, inst->klass->vtable[slot]->globals);
            if (invoke_result.has_error()) return invoke_result.error();
        }

        LOAD_FRAME();
        DISPATCH();
    }
    CASE(INVOKE_NAMED)
    {
        {
            Value inst = RA();
            u8 argc = instr_C(instr);

            if (UNLIKELY(ip >= cur_chunk->code.size()))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "missing INVOKE_NAMED payload");
            u32 payload = cur_chunk->code[ip++];
            if (UNLIKELY(instr_op(payload) != OpCode::NOP))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid INVOKE_NAMED payload");
            u32 name_idx = instr_Bx(payload);

            /// there many unchecked operations that may potentially fail here
            /// Compiler must guarantee that the name index in the constant table is valid
            /// otherwise it should throw an early error, also that index should always contain
            /// a string object and of course the index muse be an int, all of these must be
            /// guaranteed by the compiler before emitting this instruction
            if (UNLIKELY(name_idx >= cur_chunk->constants.size() || !cur_chunk->constants[name_idx].is_string()))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid method-name constant");
            StringRef method_name = cur_chunk->constants[name_idx].as_string()->str;
            Value callee = Value::nil();
            bool callable_member = false;
            if (inst.is_module()) {
                ObjModule* module = inst.as_module();
                Value const* value = module->globals == nullptr ? nullptr : module->globals->find(method_name);
                if (value == nullptr || module->globals->index.find_ptr(method_name) == nullptr)
                    return raise_runtime_error(ErrorCode::UNDEFINED_GLOBAL,
                        "module has no export '" + std::string(method_name.data(), method_name.len()) + "'");
                callee = *value;
                callable_member = true;
            } else if (inst.is_instance()) {
                ObjInstance* object = inst.as_instance();
                int field = object->klass->field_index(method_name);
                if (field >= 0 && object->klass->method_slot(method_name) < 0) {
                    callee = object->fields[static_cast<u32>(field)];
                    callable_member = true;
                }
            } else {
                return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL, "(method call on non-instance)");
            }
            if (callable_member) {
                if (argc == 0)
                    return raise_runtime_error(ErrorCode::INVALID_OPCODE, "missing member-call receiver slot");
                int result_slot = cur_frame_base + instr_A(instr);
                // Named calls reserve an implicit-self slot. Plain callable
                // attributes receive only their explicit arguments.
                for (u32 i = 0; i + 1 < argc; ++i) m_stack[result_slot + 1 + i] = m_stack[result_slot + 2 + i];
                m_stack[result_slot] = callee;
                SAVE_IP();
                auto call_result = call_value(callee, static_cast<int>(argc - 1), result_slot + 1, false);
                if (call_result.has_error()) return call_result.error();
            } else {
                ObjInstance* inst_obj = inst.as_instance();
                int slot = inst_obj->klass->method_slot(method_name);
                if (slot == -1)
                    return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL,
                        "instance class does not define this method: " + std::string(method_name.data()));

                auto invoke_result = invoke_method(inst_obj->klass->vtable[slot], inst, cur_frame_base + instr_A(instr),
                    cur_frame_base + instr_A(instr) + 1, static_cast<int>(argc), ip, m_stack_top,
                    inst_obj->klass->vtable[slot]->globals);
                if (invoke_result.has_error()) return invoke_result.error();
            }
        }
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(GET_FIELD)
    {
        Value& res = RA();
        Value obj_v = RB();

        if (obj_v.is_module()) {
            if (UNLIKELY(instr_C(instr) != 0xFF || ip >= cur_chunk->code.size()
                    || instr_op(cur_chunk->code[ip]) != OpCode::NOP))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "module access requires a name payload");
            u16 name_idx = instr_Bx(cur_chunk->code[ip++]);
            if (UNLIKELY(name_idx >= cur_chunk->constants.size() || !cur_chunk->constants[name_idx].is_string()))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid module attribute name");
            ObjModule* module = obj_v.as_module();
            StringRef const& name = cur_chunk->constants[name_idx].as_string()->str;
            Value const* value = module->globals == nullptr ? nullptr : module->globals->find(name);
            if (value == nullptr || (module->globals->index.find_ptr(name) == nullptr))
                return raise_runtime_error(ErrorCode::UNDEFINED_GLOBAL,
                    "module '" + std::string(module->name.data()) + "' has no export '"
                        + std::string(name.data(), name.len()) + "'");
            res = *value;
            DISPATCH();
        }

        if (!UNLIKELY(obj_v.is_instance())) {
            auto type_result = type(1, &obj_v);
            if (type_result.has_error()) return type_result.error();
            return raise_runtime_error(ErrorCode::UNDEFINED_FIELD,
                type_result.value().as_string()->str.data() + std::string(" is not a class"));
        }

        ObjInstance* inst = obj_v.as_instance();
        u8 field_idx = instr_C(instr);

        if (field_idx == 0xFF) {
            if (UNLIKELY(ip >= cur_chunk->code.size() || instr_op(cur_chunk->code[ip]) != OpCode::NOP))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "missing GET_FIELD payload");
            u32 payload = cur_chunk->code[ip]; // the NOP immediately after GET_FIELD
            u16 name_idx = instr_Bx(payload);
            ip++; // consume the payload word so DISPATCH doesn't re-decode it as a real op

            if (UNLIKELY(name_idx >= cur_chunk->constants.size() || !cur_chunk->constants[name_idx].is_string()))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid field-name constant");
            ObjString* name_obj = cur_chunk->constants[name_idx].as_string();
            int slot = inst->klass->field_index(name_obj->str); // needs a runtime-side field_index lookup

            if (UNLIKELY(slot < 0)) {
                RuntimeResult type_result = type(1, &obj_v);
                if (type_result.has_error()) return type_result.error();
                return raise_runtime_error(ErrorCode::UNDEFINED_FIELD,
                    type_result.value().as_string()->str.data() + std::string(" does not define ")
                        + name_obj->str.data());
            }

            res = inst->fields[static_cast<u32>(slot)];
            DISPATCH();
        }

        if (UNLIKELY(field_idx >= inst->fields.size())) return raise_runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        res = inst->fields[field_idx];
        DISPATCH();
    }
    CASE(SET_FIELD)
    {
        Value obj_v = RA();
        if (UNLIKELY(!obj_v.is_instance()))
            return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL, "SET_FIELD on non-instance");

        ObjInstance* inst = obj_v.as_instance();
        u8 field_idx = instr_B(instr);

        if (field_idx == 0xFF) {
            if (UNLIKELY(ip >= cur_chunk->code.size() || instr_op(cur_chunk->code[ip]) != OpCode::NOP))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "missing SET_FIELD payload");
            u32 payload = cur_chunk->code[ip++];
            u16 name_idx = instr_Bx(payload);

            if (UNLIKELY(name_idx >= cur_chunk->constants.size() || !cur_chunk->constants[name_idx].is_string()))
                return raise_runtime_error(ErrorCode::INVALID_OPCODE, "invalid field-name constant");

            ObjString* name_obj = cur_chunk->constants[name_idx].as_string();
            int slot = inst->klass->field_index(name_obj->str);
            if (UNLIKELY(slot < 0))
                return raise_runtime_error(
                    ErrorCode::UNDEFINED_FIELD, std::string(name_obj->str.data(), name_obj->str.len()));

            inst->fields[static_cast<u32>(slot)] = RC();
            DISPATCH();
        }

        if (UNLIKELY(field_idx >= inst->fields.size())) return raise_runtime_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        inst->fields[field_idx] = RC();
        DISPATCH();
    }

    CASE(NOP) { DISPATCH(); }

    END_DISPATCH();

    return Value::nil(); // UNREACHABLE
}

#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic pop
#endif

} // namespace fairuz::runtime
