#ifndef FA_VM_HPP
#define FA_VM_HPP

#include "fbuiltins.hpp"
#include "fgc.hpp"
#include "fopcode.hpp"
#include "fstring.hpp"
#include "ftable.hpp"

#include <filesystem>
#include <memory>
#include <regex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace fairuz::lex {

class FileManager;

}

namespace fairuz::runtime {

struct RuntimeHalt final : public std::runtime_error {
    RuntimeHalt()
        : std::runtime_error("runtime error")
    {
    }
};

struct CallFrame {
    ObjFunction* func { nullptr };
    Chunk* chunk { nullptr };
    u32 ip { 0 };
    u16 base { 0 };
    u16 local_count { 0 };
    u16 return_slot { 0 };
    u16 caller_stack_top { 0 };
    GlobalEnvironment* globals { nullptr };
    ObjModule* module { nullptr };

    CallFrame() = default;

    explicit CallFrame(ObjFunction* cl, Chunk* ch, u32 ip, u16 b, u16 lc,
        u16 ret_slot, u16 saved_stack_top, GlobalEnvironment* env = nullptr,
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
    static constexpr int MAX_FRAMES = 256;
    static constexpr int STACK_SIZE = 1024 * 8;   // 8kb
    static constexpr int GC_THRESHOLD = 1024 * 4; // 4kb

    VM();
    ~VM();

    Value run(Chunk* chunk);

    /// Builtin functions that require access to memory or internal interpreter
    /// state, or maybe just have to be really efficient.

    Value print(int argc, Value* argv);
    Value open(int argc, Value* argv);
    Value len(int argc, Value* argv);
    Value type(int argc, Value* argv);
    Value Int(int argc, Value* argv);
    Value Float(int argc, Value* argv);
    Value append(int argc, Value* argv);
    Value pop(int argc, Value* argv);
    Value slice(int argc, Value* argv);
    Value input(int argc, Value* argv);
    Value str(int argc, Value* argv);
    Value Bool(int argc, Value* argv);
    Value list(int argc, Value* argv);
    Value dict(int argc, Value* argv);
    Value dict_keys(int argc, Value* argv);
    Value dict_contains(int argc, Value* argv);
    Value dict_delete(int argc, Value* argv);
    Value split(int argc, Value* argv);
    Value join(int argc, Value* argv);
    Value substr(int argc, Value* argv);
    Value contains(int argc, Value* argv);
    Value trim(int argc, Value* argv);
    Value char_from_codepoint(int argc, Value* argv);
    Value number_from_text(int argc, Value* argv);
    Value number_finite(int argc, Value* argv);
    Value number_is_nan(int argc, Value* argv);
    Value json_escape(int argc, Value* argv);
    Value json_read_string(int argc, Value* argv);
    Value dynamic_call(int argc, Value* argv);
    Value executor_new(int argc, Value* argv);
    Value executor_close(int argc, Value* argv);
    Value task_start(int argc, Value* argv);
    Value task_done(int argc, Value* argv);
    Value task_result(int argc, Value* argv);
    Value task_cancel(int argc, Value* argv);
    Value task_wait_all(int argc, Value* argv);
    Value file_open(int argc, Value* argv);
    Value file_read(int argc, Value* argv);
    Value file_read_all(int argc, Value* argv);
    Value file_read_line(int argc, Value* argv);
    Value file_write(int argc, Value* argv);
    Value file_flush(int argc, Value* argv);
    Value path_delete(int argc, Value* argv);
    Value path_glob(int argc, Value* argv);
    Value temp_file(int argc, Value* argv);
    Value temp_directory(int argc, Value* argv);
    Value remove_tree(int argc, Value* argv);
    Value datetime_now(int argc, Value* argv);
    Value datetime_from_fields(int argc, Value* argv);
    Value datetime_to_fields(int argc, Value* argv);
    Value datetime_parse(int argc, Value* argv);
    Value datetime_format(int argc, Value* argv);
    Value base64_encode(int argc, Value* argv);
    Value base64_decode(int argc, Value* argv);
    Value hex_encode(int argc, Value* argv);
    Value hex_decode(int argc, Value* argv);
    Value hash_new(int argc, Value* argv);
    Value hash_update(int argc, Value* argv);
    Value hash_digest(int argc, Value* argv);
    Value hmac(int argc, Value* argv);
    Value compress(int argc, Value* argv);
    Value decompress(int argc, Value* argv);
    Value floor(int argc, Value* argv);
    Value ceil(int argc, Value* argv);
    Value round(int argc, Value* argv);
    Value abs(int argc, Value* argv);
    Value min(int argc, Value* argv);
    Value max(int argc, Value* argv);
    Value pow(int argc, Value* argv);
    Value sqrt(int argc, Value* argv);
    Value math_unary(int argc, Value* argv);
    Value math_binary(int argc, Value* argv);
    Value url_encode(int argc, Value* argv);
    Value url_decode(int argc, Value* argv);
    Value url_parse(int argc, Value* argv);
    Value url_build(int argc, Value* argv);
    Value regex_compile(int argc, Value* argv);
    Value regex_search(int argc, Value* argv);
    Value regex_match(int argc, Value* argv);
    Value regex_fullmatch(int argc, Value* argv);
    Value regex_findall(int argc, Value* argv);
    Value regex_split(int argc, Value* argv);
    Value regex_replace(int argc, Value* argv);
    Value make_regex_result(std::string const& input, std::smatch const& match, size_t base_offset);
    Value Assert(int argc, Value* argv);
    Value clock(int argc, Value* argv);
    Value error(int argc, Value* argv);
    Value time(int argc, Value* argv);
    Value append_file(int argc, Value* argv);
    Value close(int argc, Value* argv);

    // stdlib helpers
    void dict_put(Value* dict_ptr, Value k, Value v);
    Value dict_get(Value* dict_ptr, Value k);

    friend class GarbageCollector;

    GarbageCollector m_gc;
    Value m_stack[STACK_SIZE];
    CallFrame m_frames[MAX_FRAMES];
    int m_stack_top { 0 };
    int m_frames_top { 0 };

    friend struct BuiltinsList;

    HashTable<StringRef, ObjString*, StringRefHash, StringRefEqual> m_string_table;
    GlobalEnvironment m_builtin_environment;
    GlobalEnvironment m_root_environment;
    std::vector<std::unique_ptr<GlobalEnvironment>> m_module_environments;
    std::vector<std::unique_ptr<lex::FileManager>> m_module_sources;
    std::unordered_map<std::string, ObjModule*> m_module_cache;
    BuiltinsList m_builtin_functions;
    bool m_is_dead { false };

    Value execute(int stop_frame_depth = 0);
    void unwind_failed_run();
    Value call_special_sync(Value receiver, int special_slot);
    Value call_value_sync(Value callee, ObjList* arguments);

    CallFrame& frame();
    CallFrame const& frame() const;
    Chunk* chunk();
    Value& reg(int r);

    ObjString* intern(StringRef const& str);
    void update_ic_binary(Chunk* ch, u32 nop_ip, Value lhs, Value rhs, Value result);
    void call_value(Value callee, int argc, int base, bool tail);
    Value call_native(ObjNative* nat, int argc, int base);

    SourceLocation current_location() const;
    void raise_error(ErrorCode errc, std::string const& detail = "");
    void _raise_error(ErrorCode errc, std::string const& detail = "");

    [[noreturn]] void halt();
    void intern_chunk_constants(Chunk* ch);

    void ensure_stack_slots(int needed);
    CallFrame& top_frame();
    CallFrame const& top_frame() const;
    Value& get_reg(CallFrame const& f, int reg);
    void invoke_method(Chunk* target_chunk, Value self_val, int result_slot,
        int call_base, int total_argc, u32 return_ip, int caller_stack_top,
        GlobalEnvironment* globals = nullptr);
    GlobalEnvironment* current_globals();
    Value const* find_global(GlobalEnvironment* env, StringRef const& name) const;
    void store_global(GlobalEnvironment* env, StringRef const& name, Value value);
    std::filesystem::path resolve_module_path(std::string const& name) const;
    ObjModule* load_module(std::string const& name);
}; // class VM

} // namespace fairuz::runtime

#endif // FA_VM_HPP
