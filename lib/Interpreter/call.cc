//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// CALL opcode execution.
//===----------------------------------------------------------------------===//

#include "Compiler/chunk.hpp"
#include "Interpreter/vm.hpp"

namespace fairuz::runtime {

ErrorOr<bool, RuntimeError> VM::run_instance_op(
    Value const& lhs, Value const& rhs, int op, u32 instr, int cur_frame_base, u32 ip)
{
    ObjClass* self_klass = nullptr;
    Value self_val, arg_val = Value::nil();
    int slot = -1;
    if (lhs.is_instance()) {
        self_klass = lhs.as_instance()->klass;
        slot = self_klass->method_slot(sp_method_name(op));
        if (slot >= 0) {
            self_val = lhs;
            arg_val = rhs;
        }
    }
    if (slot < 0 && rhs.is_instance()) {
        self_klass = rhs.as_instance()->klass;
        slot = self_klass->method_slot(sp_method_name(op));
        if (slot >= 0) {
            self_val = rhs;
            arg_val = lhs;
        }
    }

    if (UNLIKELY(slot < 0)) return raise_runtime_error(ErrorCode::TYPE_ERROR_ARITH);
    if (UNLIKELY(static_cast<u32>(slot) >= self_klass->vtable.size()))
        return raise_runtime_error(ErrorCode::UNDEFINED_METHOD);

    Chunk* target_chunk = self_klass->vtable[static_cast<u32>(slot)];
    int caller_stack_top = m_stack_top;
    int call_base = caller_stack_top;
    if (UNLIKELY(call_base + 2 >= STACK_SIZE)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);
    m_stack[call_base + 1] = arg_val;
    return invoke_method(target_chunk, self_val, cur_frame_base + instr_A(instr), call_base, 2, ip, caller_stack_top,
        target_chunk->globals);
}

// Calls target_chunk with self_val in callee register 0. total_argc includes
// that implicit self slot; explicit arguments must already follow call_base.
ErrorOr<bool, RuntimeError> VM::invoke_method(Chunk* target_chunk, Value self_val, int result_slot, int call_base,
    int total_argc, u32 ip, int caller_stack_top, GlobalEnvironment* globals)
{
    if (UNLIKELY(target_chunk == nullptr))
        return raise_runtime_error(ErrorCode::TYPE_ERROR_CALL, "method slot is empty");
    if (UNLIKELY(total_argc != target_chunk->arity)) return raise_runtime_error(ErrorCode::WRONG_ARG_COUNT);
    if (UNLIKELY(m_frames_top >= MAX_FRAMES)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);

    auto compiled = ensure_compiled(target_chunk);
    if (compiled.has_error()) return compiled.error();
    int local_count = target_chunk->local_count;
    int new_top = call_base + local_count + 1;
    if (UNLIKELY(new_top > STACK_SIZE)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);

    // The caller may stage method arguments immediately above its live top.
    // Preserve that initialized prefix before clearing the remaining locals.
    int initialized_top = call_base + total_argc;
    if (m_stack_top < initialized_top) m_stack_top = initialized_top;
    while (m_stack_top < new_top) {
        m_stack[m_stack_top] = Value::nil();
        m_stack_top++;
    }

    m_stack[call_base] = self_val; // self lands in register 0 of the callee
    // explicit args are assumed already placed at call_base+1 .. call_base+explicit_argc
    // by the caller, before this is invoked.

    for (int i = total_argc; i < local_count; i++) m_stack[call_base + i] = Value::nil();

    frame().ip = PCType { ip };
    m_frames[m_frames_top] = CallFrame(nullptr, target_chunk, PCType { 0 }, static_cast<u16>(call_base),
        static_cast<u16>(local_count), static_cast<u16>(result_slot), static_cast<u16>(caller_stack_top),
        globals == nullptr ? current_globals() : globals);
    m_frames_top++;

    return true;
}

ErrorOr<bool, RuntimeError> VM::call_value(Value callee, int argc, int call_base, bool tail)
{
    if (callee.is_function()) {
        ObjFunction* fn = callee.as_func();
        Chunk* fchk = fn->chunk;
        int arity = fchk->arity;

        if (argc != arity)
            return raise_runtime_error(ErrorCode::WRONG_ARG_COUNT,
                std::string(fchk->name.data(), fchk->name.len()) + "() expected " + std::to_string(arity)
                    + " arguments but got " + std::to_string(argc));

        auto compiled = ensure_compiled(fchk);
        if (compiled.has_error()) return compiled.error();
        int local_count = fchk->local_count;

        if (local_count < argc) return raise_runtime_error(ErrorCode::WRONG_ARG_COUNT);

        if (tail && m_frames_top > 0) {
            CallFrame old_frame = m_frames[m_frames_top - 1];
            int cur_base = old_frame.base;
            int new_top = cur_base + local_count + 1;
            if (UNLIKELY(new_top > STACK_SIZE)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);

            for (int i = 0; i < argc; i++) m_stack[cur_base + i] = m_stack[call_base + i];
            for (int i = argc; i < local_count; i++) m_stack[cur_base + i] = Value::nil();

            m_frames[m_frames_top - 1] = CallFrame(fn, fchk, PCType { 0 }, static_cast<u16>(cur_base),
                static_cast<u16>(local_count), old_frame.return_slot, old_frame.caller_stack_top, fn->globals);
            m_stack_top = new_top;
        } else {
            if (UNLIKELY(m_frames_top >= MAX_FRAMES)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);

            int caller_stack_top = m_stack_top;
            int new_top = call_base + local_count + 1;
            if (UNLIKELY(new_top > STACK_SIZE)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);

            while (m_stack_top < new_top) {
                m_stack[m_stack_top] = Value::nil();
                m_stack_top++;
            }

            for (int i = argc; i < local_count; i++) m_stack[call_base + i] = Value::nil();

            m_frames[m_frames_top]
                = CallFrame(fn, fchk, PCType { 0 }, static_cast<u16>(call_base), static_cast<u16>(local_count),
                    static_cast<u16>(call_base - 1), static_cast<u16>(caller_stack_top), fn->globals);
            m_frames_top++;
        }
        return true;
    }

    if (callee.is_native()) {
        ObjNative* nat = callee.as_native();
        if (nat->arity >= 0 && argc != nat->arity) {
            std::string name = (nat->name ? std::string(nat->name->str.data(), nat->name->str.len()) : "?");
            return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
                std::string("native '") + name + "' expected " + std::to_string(nat->arity) + " args, got "
                    + std::to_string(argc));
        }

        auto call_result = call_native(nat, argc, call_base);
        if (call_result.has_error()) return call_result.error();

        m_stack[call_base - 1] = call_result.value();
        return true;
    }

    if (callee.is_class()) {
        ObjClass* klass = callee.as_class();
        ObjInstance* inst = m_gc.make_obj_instance(klass);
        Value instance = Value::from_obj(reinterpret_cast<ObjHeader*>(inst));

        int ctor_slot = klass->method_slot(StringRef { "بداية" });
        if (ctor_slot < 0) ctor_slot = klass->method_slot(StringRef { "init" });

        if (ctor_slot < 0) {
            if (argc != 0) return raise_runtime_error(ErrorCode::WRONG_ARG_COUNT);
            if (tail && m_frames_top > 0) {
                CallFrame finished = frame();
                m_stack[finished.return_slot] = instance;
                m_frames_top -= 1;
                m_stack_top = finished.caller_stack_top;
                return true;
            }
            m_stack[call_base - 1] = instance;
            return true;
        }

        Chunk* ctor_chunk = klass->vtable[static_cast<u32>(ctor_slot)];
        if (UNLIKELY(argc + 1 != ctor_chunk->arity)) return raise_runtime_error(ErrorCode::WRONG_ARG_COUNT);

        auto compiled = ensure_compiled(ctor_chunk);
        if (compiled.has_error()) return compiled.error();
        int local_count = ctor_chunk->local_count;

        // NEW: honor tail — reuse the caller's own frame slot instead of
        // stacking a fresh one, exactly like the is_function tail path does.
        CallFrame old_frame;
        if (tail && m_frames_top > 0) old_frame = m_frames[m_frames_top - 1];
        int dest_base = (tail && m_frames_top > 0) ? old_frame.base : call_base;
        int caller_stack_top = m_stack_top;

        if (UNLIKELY(m_stack_top + 1 >= STACK_SIZE)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);

        if (tail && dest_base < call_base) {
            for (int i = 0; i < argc; ++i) m_stack[dest_base + i + 1] = m_stack[call_base + i];
        } else {
            for (int i = argc - 1; i >= 0; --i) m_stack[dest_base + i + 1] = m_stack[call_base + i];
        }
        m_stack[dest_base] = instance;

        int new_top = dest_base + local_count + 1;
        if (UNLIKELY(new_top > STACK_SIZE)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);
        while (m_stack_top < new_top) {
            m_stack[m_stack_top] = Value::nil();
            m_stack_top++;
        }
        for (int i = argc + 1; i < local_count; i++) m_stack[dest_base + i] = Value::nil();

        if (tail && m_frames_top > 0) {
            m_frames[m_frames_top - 1] = CallFrame(nullptr, ctor_chunk, PCType { 0 }, static_cast<u16>(dest_base),
                static_cast<u16>(local_count), old_frame.return_slot, old_frame.caller_stack_top, ctor_chunk->globals);
            m_stack_top = new_top;
        } else {
            if (UNLIKELY(m_frames_top >= MAX_FRAMES)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);
            m_frames[m_frames_top] = CallFrame(nullptr, ctor_chunk, PCType { 0 }, static_cast<u16>(dest_base),
                static_cast<u16>(local_count), static_cast<u16>(call_base - 1), static_cast<u16>(caller_stack_top),
                ctor_chunk->globals);
            m_frames_top++;
        }
        return true;
    }

    if (callee.is_instance()) {
        ObjInstance* inst = callee.as_instance();
        int slot = inst->klass->method_slot(sp_method_name(ObjClass::CALL));
        if (UNLIKELY(slot < 0 || static_cast<u32>(slot) >= inst->klass->vtable.size()
                || inst->klass->vtable[static_cast<u32>(slot)] == nullptr))
            return raise_runtime_error(ErrorCode::NOT_CALLABLE);

        Chunk* target = inst->klass->vtable[static_cast<u32>(slot)];
        if (UNLIKELY(argc + 1 != target->arity)) return raise_runtime_error(ErrorCode::WRONG_ARG_COUNT);
        auto compiled = ensure_compiled(target);
        if (compiled.has_error()) return compiled.error();

        if (tail && m_frames_top > 0) {
            CallFrame old_frame = m_frames[m_frames_top - 1];
            int dest_base = old_frame.base;
            int new_top = dest_base + static_cast<int>(target->local_count) + 1;
            if (UNLIKELY(new_top > STACK_SIZE)) return raise_runtime_error(ErrorCode::STACK_OVERFLOW);
            for (int i = 0; i < argc; ++i) m_stack[dest_base + i + 1] = m_stack[call_base + i];
            m_stack[dest_base] = callee;
            for (int i = argc + 1; i < static_cast<int>(target->local_count); ++i)
                m_stack[dest_base + i] = Value::nil();
            m_frames[m_frames_top - 1] = CallFrame(nullptr, target, PCType { 0 }, static_cast<u16>(dest_base),
                static_cast<u16>(target->local_count), old_frame.return_slot, old_frame.caller_stack_top,
                target->globals);
            m_stack_top = new_top;
            return true;
        }

        int caller_stack_top = m_stack_top;
        int method_base = caller_stack_top;
        int new_top = method_base + static_cast<int>(target->local_count) + 1;
        if (UNLIKELY(m_frames_top >= MAX_FRAMES || new_top > STACK_SIZE))
            return raise_runtime_error(ErrorCode::STACK_OVERFLOW);
        m_stack[method_base] = callee;

        for (int i = 0; i < argc; ++i) m_stack[method_base + i + 1] = m_stack[call_base + i];
        for (int i = argc + 1; i < static_cast<int>(target->local_count); ++i) m_stack[method_base + i] = Value::nil();

        m_stack_top = new_top;
        m_frames[m_frames_top++] = CallFrame(nullptr, target, PCType { 0 }, static_cast<u16>(method_base),
            static_cast<u16>(target->local_count), static_cast<u16>(call_base - 1), static_cast<u16>(caller_stack_top),
            target->globals);
        return true;
    }

    StringRef fn_name = "";
    if (callee.is_function()) fn_name = callee.as_func()->name();

    return raise_runtime_error(ErrorCode::NOT_CALLABLE, std::string(fn_name.data(), fn_name.len()));
}

VM::RuntimeResult VM::call_native(ObjNative* nat, int argc, int call_base)
{
    return (this->*nat->fn)(argc, &m_stack[call_base]);
}

VM::RuntimeResult VM::call_value_sync(Value callee, ObjList* arguments)
{
    if (arguments == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR, "dynamic call expects an argument list");
    int argc = static_cast<int>(arguments->elements.size());
    int stop_depth = m_frames_top;
    int saved_top = m_stack_top;
    auto ensure_result = ensure_stack_slots(saved_top + argc + 1);
    if (ensure_result.has_error()) return ensure_result.error();
    int callee_slot = saved_top;
    m_stack[callee_slot] = callee;
    for (int i = 0; i < argc; ++i) m_stack[callee_slot + 1 + i] = arguments->elements[static_cast<u32>(i)];
    m_stack_top = callee_slot + argc + 1;

    auto call_result = call_value(callee, argc, callee_slot + 1, false);
    if (call_result.has_error()) return call_result.error();

    // A native callee finishes synchronously without adding a VM frame.
    if (m_frames_top > stop_depth) {
        auto exec_result = execute(stop_depth);
        if (exec_result.has_error()) return exec_result.error();
    }
    Value result = m_stack[callee_slot];
    m_stack_top = saved_top;
    return result;
}

VM::RuntimeResult VM::call_special_sync(Value receiver, int special_slot)
{
    if (!receiver.is_instance() || m_frames_top == 0) return Value::nil();

    ObjInstance* instance = receiver.as_instance();
    StringRef name = sp_method_name(special_slot);
    int slot = instance->klass->method_slot(name);
    if (slot < 0 || static_cast<u32>(slot) >= instance->klass->vtable.size()) return Value::nil();
    Chunk* target = instance->klass->vtable[static_cast<u32>(slot)];
    if (target == nullptr || target->arity != 1) return Value::nil();

    int stop_depth = m_frames_top;
    int caller_top = m_stack_top;
    int result_slot = caller_top;
    int call_base = caller_top + 1;

    auto invoke_result
        = invoke_method(target, receiver, result_slot, call_base, 1, frame().ip.value, caller_top, target->globals);
    if (invoke_result.has_error()) return invoke_result.error();

    return execute(stop_depth);
}

}
