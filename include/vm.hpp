#ifndef VM_HPP
#define VM_HPP

#include "gc.hpp"
#include "opcode.hpp"
#include "table.hpp"
#include "value.hpp"

namespace fairuz::runtime {

using ErrorCode = diagnostic::errc::runtime::Code;

class Fa_VM {
public:
    static constexpr int MAX_FRAMES = 256;
    static constexpr int STACK_SIZE = 8192;
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

    friend class Fa_GarbageCollector;

    struct Fa_CallFrame {
        Fa_ObjClosure* closure { nullptr };
        Fa_Chunk* chunk { nullptr };
        u32 ip { 0 };
        u16 base { 0 };
        u16 localCount { 0 };

        Fa_CallFrame() = default;

        explicit Fa_CallFrame(Fa_ObjClosure* cl, Fa_Chunk* ch, u32 ip, u16 b, u16 lc)
            : closure(cl)
            , chunk(ch)
            , ip(ip)
            , base(b)
            , localCount(lc)
        {
        }
    }; // struct Fa_CallFrame

    Fa_GarbageCollector GC_;

    Fa_Value Stack_[STACK_SIZE];
    Fa_CallFrame Frames_[MAX_FRAMES];

    int StackTop_ { 0 };
    int FramesTop_ { 0 };

    int OpenUpvalueCount_ { 0 };

    NarrowHashTable<Fa_StringRef, u32, Fa_StringRefHash, Fa_StringRefEqual> GlobalIndex_;
    NarrowHashTable<Fa_StringRef, Fa_ObjString*, Fa_StringRefHash, Fa_StringRefEqual> StringTable_;
    Fa_Array<Fa_Value> GlobalSlots_;
    Fa_Array<ObjUpvalue*> OpenUpvalues_;
    bool isDead_ { false };

    Fa_Value execute();

    Fa_CallFrame& frame();
    Fa_CallFrame const& frame() const;
    Fa_Chunk* chunk();
    Fa_Value& reg(int r);

    Fa_ObjString* intern(Fa_StringRef const& str);
    void ensureStack(int needed);
    void closeUpvalues(unsigned int from_stack_pos);
    void updateIcBinary(Fa_Chunk* ch, u32 nop_ip, Fa_Value lhs, Fa_Value rhs, Fa_Value result);
    ObjUpvalue* captureUpvalue(unsigned int stack_pos);
    void callValue(Fa_Value callee, int argc, int base, bool tail);
    Fa_Value callNative(Fa_ObjNative* nat, int argc, int base);
    void returnFromCall(int ret_reg, int n_ret);

    void openStdlib();
    void registerNative(Fa_StringRef const& name, NativeFn fn, int arity = -1);

    Fa_SourceLocation currentLocation() const;
    void runtimeError(ErrorCode code);
    [[noreturn]] void halt();
    void internChunkConstants(Fa_Chunk* ch);

    int stackSize() const;
    int frameDepth() const;
    void pushValue(Fa_Value v);
    Fa_Value popValue();
    Fa_Value& stackAt(int index);
    void ensureStackSlots(int needed);
    void pushFrame(Fa_CallFrame const& f);
    void popFrame();
    Fa_CallFrame& topFrame();
    Fa_CallFrame const& topFrame() const;
    Fa_Value& getReg(Fa_CallFrame const& f, int reg);
    Fa_Value& regA(Fa_CallFrame const& f, u32 instr);
    Fa_Value& regB(Fa_CallFrame const& f, u32 instr);
    Fa_Value& regC(Fa_CallFrame const& f, u32 instr);
    void closeUpvaluesForFrame(Fa_CallFrame const& f);
}; // class Fa_VM

} // namespace fairuz::runtime

#endif // VM_HPP
