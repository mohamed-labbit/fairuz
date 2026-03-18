#ifndef VM_HPP
#define VM_HPP

#include "../array.hpp"
#include "value_.hpp"
#include <unordered_map>

namespace mylang::runtime {

using ErrorCode = diagnostic::errc::runtime::Code;

class VM {
public:
    static constexpr int MAX_FRAMES = 256;
    static constexpr int STACK_SIZE = 8192;

    VM()
    {
        std::fill(Stack_, Stack_ + STACK_SIZE, NIL_VAL);
        std::fill(Frames_, Frames_ + MAX_FRAMES, CallFrame { });
        openStdlib();
    }

    Value run(Chunk* chunk);

private:
    struct CallFrame {
        ObjClosure* closure { nullptr };
        uint32_t ip { 0 };
        int32_t base { 0 };
        int localCount { 0 };
    };

    Value Stack_[STACK_SIZE];
    CallFrame Frames_[MAX_FRAMES];
    unsigned int StackTop_ { 0 };
    unsigned int FramesTop_ { 0 };

    std::unordered_map<StringRef, Value> Globals_;
    std::unordered_map<StringRef, ObjString*> StringTable_;
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

}; // class VM

} // namespace mylang::runtime

#endif // VM_HPP
