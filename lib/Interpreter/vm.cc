//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Executes Fairuz bytecode.
//===----------------------------------------------------------------------===//

#include "Interpreter/vm.hpp"
#include "Compiler/compiler.hpp"
#include "Compiler/opcode.hpp"
#include "Diag/diagnostic.hpp"
#include "Diag/runtime_error.hpp"
#include "Interpreter/obj_header.hpp"
#include "Interpreter/object.hpp"
#include "Interpreter/value.hpp"
#include "Macros.hpp"
#include "Support/error.hpp"
#include "Support/string.hpp"
#include "Util/utf8.hpp"

#include <cstdio>
#include <cstdlib>

namespace fairuz::runtime {

namespace {

std::string similar_name(GlobalEnvironment* environment, std::string const& missing)
{
    auto points = [](StringRef const& name) {
        std::vector<u32> result;
        for (size_t i = 0; i < name.len() && result.size() < 65;) {
            u64 bytes = 0;
            result.push_back(util::decode_utf8_at(name, i, &bytes));
            i += bytes;
        }
        return result;
    };
    auto wanted = points(StringRef(missing.c_str()));
    if (wanted.size() < 3 || wanted.size() > 64)
        return { };
    size_t best = wanted.size() < 6 ? 2 : 3;
    std::string match;
    size_t examined = 0;
    auto consider = [&](StringRef const& name) {
        if (++examined > 1000)
            return;
        auto candidate = points(name);
        if (candidate.size() > 64 || candidate.size() + best < wanted.size() || wanted.size() + best < candidate.size())
            return;
        std::vector<size_t> previous(candidate.size() + 1), current(candidate.size() + 1);
        for (size_t j = 0; j <= candidate.size(); ++j)
            previous[j] = j;
        for (size_t i = 1; i <= wanted.size(); ++i) {
            current[0] = i;
            for (size_t j = 1; j <= candidate.size(); ++j)
                current[j] = std::min({ previous[j] + 1, current[j - 1] + 1, previous[j - 1] + (wanted[i - 1] != candidate[j - 1]) });
            previous.swap(current);
        }
        std::string text(name.data(), name.len());
        if (previous.back() < best || (previous.back() == best && !match.empty() && text < match)) {
            best = previous.back();
            match = std::move(text);
        }
    };
    for (auto* env = environment; env && examined < 1000; env = env->fallback) {
        for (auto const& [name, slot] : env->index) {
            consider(name);
            if (examined >= 1000)
                break;
        }
        if (env->builtins != nullptr) {
            for (auto const& builtin : BuiltinsList::definitions())
                consider(StringRef(builtin.name.data()));
        }
    }
    return match;
}

} // namespace

static void check_stack_index(int index, int stack_size, char const* m_context)
{
    if (index < 0 || index >= stack_size)
        // This is a VM internal error, not a user error.
        diagnostic::panic(ErrorCode::INTERNAL_ERROR,
            std::string(" : ") + std::string("VM internal error: stack index ") + std::to_string(index)
                + " out of range [0," + std::to_string(stack_size) + ") in " + m_context);
}

VM::VM()
    : m_builtin_functions(*this)
{
    diagnostic::reset();

    std::fill(m_stack, m_stack + STACK_SIZE, Value::nil());
    std::fill(m_frames, m_frames + MAX_FRAMES, CallFrame());

    m_root_environment.fallback = &m_builtin_environment;

    m_builtin_environment.builtins = &m_builtin_functions;
}

VM::~VM()
{
    m_gc.sweep_all();
}

#define PUSH_VALUE(v)                                              \
    do {                                                           \
        if (m_stack_top == STACK_SIZE)                             \
            return raise_runtime_error(ErrorCode::STACK_OVERFLOW); \
        m_stack[m_stack_top] = v;                                  \
        m_stack_top++;                                             \
    } while (0);

// Ensure the value stack has at least `needed` slots allocated, filling
// any new slots with NIL.  Checks against the hard cap.
ErrorOr<bool, RuntimeError> VM::ensure_stack_slots(int needed)
{
    if (needed > STACK_SIZE)
        return raise_runtime_error(ErrorCode::STACK_OVERFLOW);

    while (m_stack_top < needed)
        PUSH_VALUE(Value::nil());

    return true;
}

// Reference to the topmost call frame.

Value& VM::get_reg(CallFrame const& f, int reg)
{
    int abs = f.base + reg;
    check_stack_index(abs, STACK_SIZE, "getReg");
    return m_stack[abs];
}

GlobalEnvironment* VM::current_globals()
{
    if (m_frames_top == 0 || frame().globals == nullptr)
        return &m_root_environment;
    return frame().globals;
}

Value const* VM::find_global(GlobalEnvironment* env, StringRef const& name) const
{
    if (env == nullptr)
        env = const_cast<GlobalEnvironment*>(&m_root_environment);
    return env->find(name);
}

void VM::store_global(GlobalEnvironment* env, StringRef const& name, Value value)
{
    if (env == nullptr)
        env = &m_root_environment;
    if (u32* slot = env->index.find_ptr(name)) {
        env->slots[*slot] = value;
        return;
    }
    u32 slot = env->slots.size();
    env->slots.push(value);
    env->index.insert_or_assign(name, slot);
}

VM::RuntimeResult VM::run(Chunk* chunk, Compiler*)
{
    assert(chunk != nullptr);

    diagnostic::reset();
    diagnostic::SourceScope source_scope(chunk->source);
    m_stack_top = 0;
    m_frames_top = 0;

    ObjFunction* fn = m_gc.make_obj_function(chunk, &m_root_environment);

    m_stack[0] = Value::from_obj(reinterpret_cast<ObjHeader*>(fn));
    m_stack_top = static_cast<int>(chunk->local_count) + 2;

    m_frames[m_frames_top] = CallFrame(fn, chunk, PCType { 0 }, 1,
        static_cast<u16>(chunk->local_count), 0, 0, &m_root_environment);
    m_frames_top++;
    intern_chunk_constants(fn->chunk);

    if (m_gc.current_memory() >= GC_THRESHOLD)
        m_gc.collect(this);

    auto result = execute();
    if (result.has_error())
        unwind();
    return result;
}

ObjString* VM::intern(StringRef const& str)
{
    if (ObjString** existing = m_string_table.find_ptr(str))
        return *existing;

    ObjString* obj = m_gc.make_obj_string(str);
    m_string_table.insert_or_assign(str, obj);
    return obj;
}

ErrorOr<bool, RuntimeError> VM::ensure_compiled(Chunk* ch)
{
    if (!ch->deferred_body)
        return true;
    auto comp_result = Compiler::compile_function_deferred(ch);
    if (comp_result.has_error())
        return comp_result.error();
    intern_chunk_constants(ch);
    return true;
}

void VM::intern_chunk_constants(Chunk* ch)
{
    if (ch == nullptr)
        return;

    for (u32 i = 0; i < ch->constants.size(); i++) {
        if (ch->constants[i].is_string())
            ch->constants[i] = Value::from_obj(reinterpret_cast<ObjHeader*>(intern(ch->constants[i].as_string()->str)));
    }

    for (auto* fn : ch->functions)
        intern_chunk_constants(fn);
}

SourceLocation VM::current_location() const
{
    if (m_frames_top == 0)
        return { };

    CallFrame const& f = top_frame();
    size_t off = f.ip.value > 0 ? f.ip.value - 1 : 0;
    Chunk const& ch = *f.chunk;

    if (off < ch.locations.size())
        return ch.locations[off];

    return { };
}

RuntimeError VM::raise_runtime_error(ErrorCode errc, std::string const& detail)
{
    if (m_frames_top == 0)
        return RuntimeError { errc, nullptr, { }, detail };
    FrameTrace top { frame() };
    RuntimeError error { errc, top.source, top.loc, detail };
    for (int i = m_frames_top - 1; i >= 0; --i)
        error.add_trace(m_frames[i]);
    return error;
}

void VM::update_ic_binary(Chunk* ch, u32 nop_ip, Value lhs, Value rhs, Value result)
{
    if (ch == nullptr)
        return;

    u32 nop = ch->code[nop_ip];
    u8 ic_idx = instr_A(nop);

    if (ic_idx < ch->ic_slots.size()) {
        ICSlot& slot = ch->ic_slots[ic_idx];
        slot.seen_lhs |= static_cast<u16>(value_type_tag(lhs));
        slot.seen_rhs |= static_cast<u16>(value_type_tag(rhs));
        slot.seen_ret |= static_cast<u16>(value_type_tag(result));
        slot.hit_count++;
    }
}

} // namespace fairuz::runtime
