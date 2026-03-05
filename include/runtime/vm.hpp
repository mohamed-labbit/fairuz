#ifndef _VM_HPP
#define _VM_HPP

// =============================================================================
// vm.hpp — Register-based bytecode interpreter
//
// Design
// ------
// • Matched 1-to-1 with the compiler's emit decisions: every opcode the
//   compiler emits has a precise handler here.
// • Each call frame owns a contiguous window into a shared value stack.
//   Registers are stack[frame.base + reg].  No heap allocation per call.
// • Upvalues follow Lua 5.4's open/close model: while the enclosing local
//   is live, ObjUpvalue::location points into the stack.  When end_scope
//   emits CLOSE_UPVALUE the VM copies the value into ObjUpvalue::closed
//   and redirects the pointer — so closures keep working after the frame
//   is gone.
// • A stop-the-world mark-and-sweep GC runs when heap bytes exceed a
//   threshold.  All GC roots are reachable through the CallStack and the
//   global table.
// • Type profiling: every IC_CALL and the NOP following every binary op
//   update the compiler-allocated ICSlot.  The JIT can read these later.
// • Built-in native functions are registered at VM construction time.
//
// Error handling
// --------------
// Runtime errors throw VMError (a std::exception subclass) with a full
// stack-trace string attached.  Callers catch at the top level.
// =============================================================================

#include "opcode.hpp"
#include "value.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mylang {
namespace runtime {

// =============================================================================
// VMError
// =============================================================================

struct VMError : std::runtime_error {
    explicit VMError(std::string const& msg)
        : std::runtime_error(msg)
    {
    }
};

// =============================================================================
// CallFrame
// =============================================================================

struct CallFrame {
    ObjClosure* closure; // currently executing closure (never null)
    uint32_t ip;         // instruction pointer into closure->function->chunk
    int base;            // index into VM::stack_ where register 0 lives
};

// =============================================================================
// VM
// =============================================================================

class VM {
public:
    // Maximum call depth before stack-overflow error
    static constexpr int MAX_FRAMES = 256;
    // Initial value stack capacity (grows automatically)
    static constexpr int STACK_SIZE = 1024 * 4;
    // GC threshold — collect when allocated bytes exceed this
    static constexpr size_t GC_INITIAL = 1024 * 1024; // 1 MB
    static constexpr double GC_GROWTH = 2.0;

    VM();
    ~VM();

    // Non-copyable
    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

    // ---- execution ----

    // Run a top-level Chunk to completion.  Returns the final value
    // (whatever the last RETURN instruction left in register 0, or nil).
    Value run(Chunk* chunk);

    // ---- globals ----
    void set_global(StringRef const& name, Value v);
    Value get_global(StringRef const& name) const;
    bool has_global(StringRef const& name) const;

    // ---- native registration ----
    void register_native(StringRef const& name, NativeFn fn, int arity = -1);

    // ---- standard library ----
    void open_stdlib();

    // ---- GC ----
    void collect_garbage();

private:
    // ---- value stack ----
    std::vector<Value> stack_;
    int stack_top_ = 0; // index of first unused slot

    // ---- call frames ----
    std::vector<CallFrame> frames_;

    // ---- globals ----
    std::unordered_map<StringRef, Value> globals_;

    // ---- GC ----
    ObjHeader* gc_head_ = nullptr; // intrusive list of all heap objects
    size_t gc_allocated_ = 0;
    size_t gc_threshold_ = GC_INITIAL;
    std::vector<ObjUpvalue*> open_upvalues_; // sorted by stack position, newest first

    // ---- helpers ----
    CallFrame& frame() { return frames_.back(); }

    CallFrame const& frame() const { return frames_.back(); }

    Chunk* chunk() { return frame().closure->function->chunk; }

    Value& reg(int r) { return stack_[frame().base + r]; }

    // ---- instruction dispatch ----
    Value execute();

    // ---- runtime operations ----
    Value do_add(Value a, Value b);
    Value do_sub(Value a, Value b);
    Value do_mul(Value a, Value b);
    Value do_div(Value a, Value b);
    Value do_idiv(Value a, Value b);
    Value do_mod(Value a, Value b);
    Value do_pow(Value a, Value b);
    Value do_neg(Value a);
    Value do_concat(Value a, Value b);
    Value do_band(Value a, Value b);
    Value do_bor(Value a, Value b);
    Value do_bxor(Value a, Value b);
    Value do_bnot(Value a);
    Value do_shl(Value a, Value b);
    Value do_shr(Value a, Value b);

    bool do_eq(Value a, Value b);
    bool do_lt(Value a, Value b);
    bool do_le(Value a, Value b);

    // ---- call helpers ----
    void call_value(Value callee, int argc, int base, bool tail);
    Value call_native(ObjNative* nat, int argc, int base);
    void return_from_call(int result_reg, int nresults);

    // ---- upvalue management ----
    ObjUpvalue* capture_upvalue(int stack_pos);
    void close_upvalues(int from_stack_pos);

    // ---- stack management ----
    void ensure_stack(int needed);

    // ---- GC internals ----
    template<typename T, typename... Args>
    T* alloc_obj(Args&&... args);

    void mark_value(Value v);
    void mark_object(ObjHeader* obj);
    void mark_roots();
    void trace_references();
    void sweep();

    // ---- error helpers ----
    [[noreturn]] void runtime_error(std::string const& msg);
    std::string build_traceback() const;

    // ---- string interning ----
    std::unordered_map<StringRef, ObjString*> string_table_;
    ObjString* intern(StringRef const& s);

    // ---- IC profile update ----
    void update_ic_binary(Chunk* ch, uint32_t nop_ip, Value lhs, Value rhs, Value result);
    void update_ic_call(Chunk* ch, uint32_t ic_ip, Value callee, Value result);
};

} // namespace runtime
} // namespace mylang

#endif // _VM_HPP
