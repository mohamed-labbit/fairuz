//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Executes Fairuz bytecode and native calls.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_INTERPRETER_VM_HPP
#define FAIRUZ_INTERPRETER_VM_HPP

#include "Diag/runtime_error.hpp"
#include "Interpreter/builtins.hpp"
#include "Interpreter/gc.hpp"
#include "Interpreter/object.hpp"
#include "Macros.hpp"
#include "Support/error.hpp"
#include "Support/string.hpp"
#include "Support/table.hpp"

#include <filesystem>
#include <memory>
#include <regex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace fairuz::runtime {

class Compiler;

struct RuntimeHalt final : public std::runtime_error {
    RuntimeHalt()
        : std::runtime_error("runtime error")
    {
    }
};

struct CallFrame {
    ObjFunction* func { nullptr };
    Chunk* chunk { nullptr };
    PCType ip { 0 };
    u16 base { 0 }; // Absolute VM-stack offset, not a constant-pool index.
    u16 local_count { 0 };
    u16 return_slot { 0 };
    u16 caller_stack_top { 0 };
    GlobalEnvironment* globals { nullptr };
    ObjModule* module { nullptr };

    CallFrame() = default;

    explicit CallFrame(ObjFunction* cl, Chunk* ch, PCType ip, u16 b, u16 lc, u16 ret_slot, u16 saved_stack_top, GlobalEnvironment* env = nullptr,
        ObjModule* module_obj = nullptr)
        : func(cl)
        , chunk(ch)
        , ip(ip)
        , base(b)
        , local_count(lc)
        , return_slot(ret_slot)
        , caller_stack_top(saved_stack_top)
        , globals(env)
        , module(module_obj)
    {
    }
}; // struct CallFrame

class VM {
public:
    using RuntimeResult = ErrorOr<Value, RuntimeError>;

    static constexpr int MAX_FRAMES = 256;
    static constexpr int STACK_SIZE = 1024 * 8;   // 8kb
    static constexpr int GC_THRESHOLD = 1024 * 4; // 4kb

    VM();
    ~VM();

    // The optional compiler argument is retained for existing callers; chunks own their context.
    RuntimeResult run(Chunk* chunk, Compiler* cmp = nullptr);

    /// These are builtin functions that require access to memory or internal interpreter
    /// state, or maybe just have to be really efficient.
    /// NOTE: Some builtin functions and helpers may not actually raise an error
    /// nevertheless they still must use RuntimeResult because all builtins
    /// must have the same function signature for registry
    FAIRUZ_NODISCARD RuntimeResult print(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult open(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult len(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult same_object(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult type(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult Int(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult Float(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult append(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult pop(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult slice(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult input(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult str(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult Bool(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult list(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult dict(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult dict_keys(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult dict_contains(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult dict_delete(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult split(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult join(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult substr(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult contains(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult trim(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult char_from_codepoint(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult number_from_text(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult number_finite(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult number_is_nan(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult json_escape(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult json_read_string(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult dynamic_call(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult executor_new(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult executor_close(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult task_start(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult task_done(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult task_result(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult task_cancel(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult task_wait_all(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult file_open(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult file_read(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult file_read_all(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult file_read_line(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult file_write(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult file_flush(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult path_delete(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult path_glob(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult temp_file(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult temp_directory(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult remove_tree(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult datetime_now(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult datetime_from_fields(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult datetime_to_fields(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult datetime_parse(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult datetime_format(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult base64_encode(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult base64_decode(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult hex_encode(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult hex_decode(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult hash_new(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult hash_update(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult hash_digest(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult hmac(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult compress(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult decompress(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult floor(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult ceil(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult round(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult abs(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult min(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult max(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult pow(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult sqrt(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult math_unary(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult math_binary(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult url_encode(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult url_decode(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult url_parse(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult url_build(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult regex_compile(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult regex_search(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult regex_match(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult regex_fullmatch(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult regex_findall(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult regex_split(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult regex_replace(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult make_regex_result(std::string const& input, std::smatch const& match, size_t base_offset);
    FAIRUZ_NODISCARD RuntimeResult Assert(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult clock(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult error(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult time(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult append_file(int argc, Value* argv);
    FAIRUZ_NODISCARD RuntimeResult close(int argc, Value* argv);

    // builtins helpers
    void dict_put(Value* dict_ptr, Value k, Value v);
    Value dict_get(Value* dict_ptr, Value k);

    GarbageCollector& gc() { return m_gc; }
    GlobalEnvironment& builtin_env() { return m_builtin_environment; }
    BuiltinsList const& builtins() const { return m_builtin_functions; }
    Value const* find_global(GlobalEnvironment* env, StringRef const& name) const;
    void store_global(GlobalEnvironment* env, StringRef const& name, Value value);

private:
    // runtime error handler
    [[noreturn]] void halt();
    void unwind();
    void print(RuntimeError const& err);

private:
    friend class GarbageCollector;
    friend struct BuiltinsList;

    // global state
    GarbageCollector m_gc;
    Value m_stack[STACK_SIZE];
    CallFrame m_frames[MAX_FRAMES];
    int m_stack_top { 0 };
    int m_frames_top { 0 };
    HashTable<StringRef, ObjString*, StringRefHash, StringRefEqual> m_string_table;
    GlobalEnvironment m_builtin_environment;
    GlobalEnvironment m_root_environment;
    std::vector<std::unique_ptr<GlobalEnvironment>> m_module_environments;
    std::vector<std::unique_ptr<FileManager>> m_module_sources;
    std::unordered_map<std::string, ObjModule*> m_module_cache;
    BuiltinsList m_builtin_functions;
    bool m_is_dead { false };

private:
    RuntimeResult execute(int stop_frame_depth = 0);

    /// Call helpers
    FAIRUZ_NODISCARD RuntimeResult call_special_sync(Value receiver, int special_slot);
    FAIRUZ_NODISCARD RuntimeResult call_value_sync(Value callee, ObjList* arguments);
    FAIRUZ_NODISCARD RuntimeResult call_native(ObjNative* nat, int argc, int base);
    FAIRUZ_NODISCARD ErrorOr<bool, RuntimeError> call_value(Value callee, int argc, int base, bool tail);
    FAIRUZ_NODISCARD ErrorOr<bool, RuntimeError> invoke_method(Chunk* target_chunk, Value self_val, int result_slot, int call_base, int total_argc,
        u32 return_ip, int caller_stack_top, GlobalEnvironment* globals = nullptr);
    FAIRUZ_NODISCARD ErrorOr<bool, RuntimeError> run_instance_op(Value const& lhs, Value const& rhs, int op, u32 instr, int cur_frame_base, u32 ip);

    /// Generic helpers
    ObjString* intern(StringRef const& str);
    void update_ic_binary(Chunk* ch, u32 nop_ip, Value lhs, Value rhs, Value result);
    SourceLocation current_location() const;
    ErrorOr<bool, RuntimeError> ensure_compiled(Chunk* ch);
    void intern_chunk_constants(Chunk* ch);
    FAIRUZ_NODISCARD ErrorOr<bool, RuntimeError> ensure_stack_slots(int needed);

    /// Module loading
    Value& get_reg(CallFrame const& f, int reg);
    GlobalEnvironment* current_globals();
    std::filesystem::path resolve_module_path(std::string const& name) const;
    FAIRUZ_NODISCARD ErrorOr<ObjModule*, RuntimeError> load_module(StringRef const& name);

    /// Error handling
    FAIRUZ_NODISCARD RuntimeError raise_runtime_error(ErrorCode errc, std::string const& detail = "");

private:
    /// usable access helpers

    CallFrame& frame() { return top_frame(); }
    CallFrame const& frame() const { return top_frame(); }
    Chunk* chunk() { return top_frame().chunk; }
    Value& reg(RegType r) { return m_stack[top_frame().base + r.value]; }

    CallFrame& top_frame()
    {
        assert(m_frames_top > 0 && "topFrame called with empty frame stack");
        return m_frames[m_frames_top - 1];
    }

    CallFrame const& top_frame() const
    {
        assert(m_frames_top > 0 && "topFrame called with empty frame stack");
        return m_frames[m_frames_top - 1];
    }

private:
    static StringRef sp_method_name(int m)
    {
        switch (m) {
        case ObjClass::INIT: return "بداية";
        case ObjClass::CALL: return "نداء";
        case ObjClass::ADD: return "عملية+";
        case ObjClass::SUB: return "عملية-";
        case ObjClass::MUL: return "عملية*";
        case ObjClass::DIV: return "عملية/";
        case ObjClass::MOD: return "عملية%";
        case ObjClass::NEG: return "سالب";
        case ObjClass::EQ: return "يساوي";
        case ObjClass::NEQ: return "لا_يساوي";
        case ObjClass::LT: return "اصغر_من";
        case ObjClass::LTE: return "اصغر_او_يساوي";
        case ObjClass::GT: return "اكبر_من";
        case ObjClass::GTE: return "اكبر_او_يساوي";
        case ObjClass::REPR: return "كتابة";
        default: return { };
        }
    }
}; // class VM

} // namespace fairuz::runtime

#endif // FAIRUZ_INTERPRETER_VM_HPP
