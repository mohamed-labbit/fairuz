#ifndef VM_HPP
#define VM_HPP

#include "gc.hpp"
#include "opcode.hpp"
#include "table.hpp"
#include "value.hpp"

namespace mylang::runtime {

using ErrorCode = diagnostic::errc::runtime::Code;

class VM {
public:
    static constexpr int MAX_FRAMES = 256;
    static constexpr int STACK_SIZE = 8192;
    static constexpr int GC_THRESHOLD = 1024 * 4; // 4kb

    VM();
    ~VM();

    Value run(Chunk* chunk);

    Value nativeLen(int argc, Value* argv);
    Value nativePrint(int argc, Value* argv);
    Value nativeType(int argc, Value* argv);
    Value nativeInt(int argc, Value* argv);
    Value nativeFloat(int argc, Value* argv);
    Value nativeAppend(int argc, Value* argv);
    Value nativePop(int argc, Value* argv);
    Value nativeSlice(int argc, Value* argv);
    Value nativeInput(int argc, Value* argv);
    Value nativeStr(int argc, Value* argv);
    Value nativeBool(int argc, Value* argv);
    Value nativeList(int argc, Value* argv);
    Value nativeSplit(int argc, Value* argv);
    Value nativeJoin(int argc, Value* argv);
    Value nativeSubstr(int argc, Value* argv);
    Value nativeContains(int argc, Value* argv);
    Value nativeTrim(int argc, Value* argv);
    Value nativeFloor(int argc, Value* argv);
    Value nativeCeil(int argc, Value* argv);
    Value nativeRound(int argc, Value* argv);
    Value nativeAbs(int argc, Value* argv);
    Value nativeMin(int argc, Value* argv);
    Value nativeMax(int argc, Value* argv);
    Value nativePow(int argc, Value* argv);
    Value nativeSqrt(int argc, Value* argv);
    Value nativeAssert(int argc, Value* argv);
    Value nativeClock(int argc, Value* argv);
    Value nativeError(int argc, Value* argv);
    Value nativeTime(int argc, Value* argv);

    friend class GarbageCollector;

    struct CallFrame {
        ObjClosure* closure { nullptr };
        Chunk* chunk { nullptr };
        uint32_t ip { 0 };
        uint16_t base { 0 };
        uint16_t localCount { 0 };

        CallFrame() = default;

        explicit CallFrame(ObjClosure* cl, Chunk* ch, uint32_t ip, uint16_t b, uint16_t lc)
            : closure(cl)
            , chunk(ch)
            , ip(ip)
            , base(b)
            , localCount(lc)
        {
        }
    }; // struct CallFrame

    GarbageCollector GC_;

    Value Stack_[STACK_SIZE];
    CallFrame Frames_[MAX_FRAMES];

    int StackTop_ { 0 };
    int FramesTop_ { 0 };

    int OpenUpvalueCount_ { 0 };

    NarrowHashTable<StringRef, uint32_t, StringRefHash, StringRefEqual> GlobalIndex_;
    NarrowHashTable<StringRef, ObjString*, StringRefHash, StringRefEqual> StringTable_;
    Array<Value> GlobalSlots_;
    Array<ObjUpvalue*> OpenUpvalues_;
    bool isDead_ { false };

    Value execute();

    CallFrame& frame();
    CallFrame const& frame() const;
    Chunk* chunk();
    Value& reg(int r);

    ObjString* intern(StringRef const& str);
    void ensureStack(int needed);
    void closeUpvalues(unsigned int from_stack_pos);
    void updateIcBinary(Chunk* ch, uint32_t nop_ip, Value lhs, Value rhs, Value result);
    ObjUpvalue* captureUpvalue(unsigned int stack_pos);
    void callValue(Value callee, int argc, int base, bool tail);
    Value callNative(ObjNative* nat, int argc, int base);
    void returnFromCall(int ret_reg, int n_ret);

    void openStdlib();
    void registerNative(StringRef const& name, NativeFn fn, int arity = -1);

    SourceLocation currentLocation() const;
    void runtimeError(ErrorCode code);
    [[noreturn]] void halt();
    void internChunkConstants(Chunk* ch);

    int stackSize() const;
    int frameDepth() const;
    void pushValue(Value v);
    Value popValue();
    Value& stackAt(int index);
    void ensureStackSlots(int needed);
    void pushFrame(CallFrame const& f);
    void popFrame();
    CallFrame& topFrame();
    CallFrame const& topFrame() const;
    Value& getReg(CallFrame const& f, int reg);
    Value& regA(CallFrame const& f, uint32_t instr);
    Value& regB(CallFrame const& f, uint32_t instr);
    Value& regC(CallFrame const& f, uint32_t instr);
    void closeUpvaluesForFrame(CallFrame const& f);
}; // class VM

} // namespace mylang::runtime

#endif // VM_HPP
