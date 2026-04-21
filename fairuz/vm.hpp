#ifndef VM_HPP
#define VM_HPP

#include "gc.hpp"
#include "opcode.hpp"
#include "table.hpp"
#include "value.hpp"

#include <stdexcept>

namespace fairuz::runtime {

using ErrorCode = diagnostic::errc::runtime::Code;

struct Fa_RuntimeHalt final : public std::runtime_error {
    Fa_RuntimeHalt()
        : std::runtime_error("runtime error")
    {
    }
};

struct Fa_CallFrame {
    Fa_ObjClosure* closure { nullptr };
    Fa_Chunk* chunk { nullptr };
    u32 ip { 0 };
    u16 base { 0 };
    u16 local_count { 0 };

    Fa_CallFrame() = default;

    explicit Fa_CallFrame(Fa_ObjClosure* cl, Fa_Chunk* ch, u32 ip, u16 b, u16 lc)
        : closure(cl)
        , chunk(ch)
        , ip(ip)
        , base(b)
        , local_count(lc)
    {
    }
}; // struct Fa_CallFrame

class Fa_VM {
public:
    static constexpr int MAX_FRAMES = 256;
    static constexpr int STACK_SIZE = 1024 * 8;   // 8kb
    static constexpr int GC_THRESHOLD = 1024 * 4; // 4kb

    Fa_VM();
    ~Fa_VM();

    Fa_Value run(Fa_Chunk* chunk);

    /* STANDARD LIBRARY */

    /* IO */
    Fa_Value Fa_print(int argc, Fa_Value* argv);
    Fa_Value Fa_len(int argc, Fa_Value* argv);
    Fa_Value Fa_type(int argc, Fa_Value* argv);
    Fa_Value Fa_int(int argc, Fa_Value* argv);
    Fa_Value Fa_float(int argc, Fa_Value* argv);
    Fa_Value Fa_append(int argc, Fa_Value* argv);
    Fa_Value Fa_pop(int argc, Fa_Value* argv);
    Fa_Value Fa_slice(int argc, Fa_Value* argv);
    Fa_Value Fa_input(int argc, Fa_Value* argv);
    Fa_Value Fa_str(int argc, Fa_Value* argv);
    Fa_Value Fa_bool(int argc, Fa_Value* argv);
    Fa_Value Fa_list(int argc, Fa_Value* argv);
    Fa_Value Fa_dict(int argc, Fa_Value* argv);
    Fa_Value Fa_split(int argc, Fa_Value* argv);
    Fa_Value Fa_join(int argc, Fa_Value* argv);
    Fa_Value Fa_substr(int argc, Fa_Value* argv);
    Fa_Value Fa_contains(int argc, Fa_Value* argv);
    Fa_Value Fa_trim(int argc, Fa_Value* argv);
    Fa_Value Fa_floor(int argc, Fa_Value* argv);
    Fa_Value Fa_ceil(int argc, Fa_Value* argv);
    Fa_Value Fa_round(int argc, Fa_Value* argv);
    Fa_Value Fa_abs(int argc, Fa_Value* argv);
    Fa_Value Fa_min(int argc, Fa_Value* argv);
    Fa_Value Fa_max(int argc, Fa_Value* argv);
    Fa_Value Fa_pow(int argc, Fa_Value* argv);
    Fa_Value Fa_sqrt(int argc, Fa_Value* argv);
    Fa_Value Fa_assert(int argc, Fa_Value* argv);
    Fa_Value Fa_clock(int argc, Fa_Value* argv);
    Fa_Value Fa_error(int argc, Fa_Value* argv);
    Fa_Value Fa_time(int argc, Fa_Value* argv);

    // stdlib helpers
    void Fa_dict_put(Fa_Value* dict_ptr, Fa_Value k, Fa_Value v);
    Fa_Value Fa_dict_get(Fa_Value* dict_ptr, Fa_Value k);

    friend class Fa_GarbageCollector;

    Fa_GarbageCollector m_gc;
    Fa_Value m_stack[STACK_SIZE];
    Fa_CallFrame m_frames[MAX_FRAMES];
    int m_stack_top { 0 };
    int m_frames_top { 0 };
    Fa_HashTable<Fa_StringRef, u32, Fa_StringRefHash, Fa_StringRefEqual> m_global_index;
    Fa_HashTable<Fa_StringRef, Fa_ObjString*, Fa_StringRefHash, Fa_StringRefEqual> m_string_table;
    Fa_Array<Fa_Value> m_global_slots;
    bool m_is_dead { false };

    Fa_Value execute();

    Fa_CallFrame& frame();
    Fa_CallFrame const& frame() const;
    Fa_Chunk* chunk();
    Fa_Value& m_reg(int r);

    Fa_ObjString* intern(Fa_StringRef const& str);
    void update_ic_binary(Fa_Chunk* ch, u32 nop_ip, Fa_Value lhs, Fa_Value rhs, Fa_Value result);
    void call_value(Fa_Value m_callee, int argc, int base, bool tail);
    Fa_Value call_native(Fa_ObjNative* nat, int argc, int base);

    void open_stdlib();
    void register_native(Fa_StringRef const& m_name, NativeFn fn, int arity = -1);

    Fa_SourceLocation current_location() const;
    void runtime_error(ErrorCode code, std::string const& detail = "");
    [[noreturn]] void halt();
    void intern_chunk_constants(Fa_Chunk* ch);

    void ensure_stack_slots(int needed);
    Fa_CallFrame& top_frame();
    Fa_CallFrame const& top_frame() const;
    Fa_Value& get_reg(Fa_CallFrame const& f, int m_reg);
}; // class Fa_VM

} // namespace fairuz::runtime

#endif // VM_HPP
