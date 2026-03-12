#ifndef VM_HPP
#define VM_HPP

#include "../array.hpp"
#include "value.hpp"
#include <unordered_map>

namespace mylang::runtime {

class VM {
public:
    static constexpr int MAX_FRAMES = 256;
    static constexpr int STACK_SIZE = 1024 * 4;

    VM()
    {
        openStdlib();
    }

    Value run(Chunk* chunk);

private:
    struct CallFrame {
        ObjClosure* closure { nullptr }; // currently executing closure (never null)
        uint32_t ip { 0 };               // instruction pointer into closure->function->chunk
        int32_t base { 0 };              // index into VM::stack_ where register 0 lives
    };

    std::unordered_map<StringRef, Value> Globals_;
    std::unordered_map<StringRef, ObjString*> StringTable_;
    Array<Value> Stack_ { STACK_SIZE, Value::nil() };
    Array<CallFrame> Frames_ { MAX_FRAMES, CallFrame() };
    unsigned int StackTop_ { 0 };
    unsigned int FramesTop_ { 0 };
    Array<ObjUpvalue*> OpenUpvalues_;
    bool isDead_ { false };

    Value execute();

    // help
    CallFrame& frame();
    CallFrame const& frame() const;
    Chunk* chunk();
    Value& reg(int r);

    // binary ops
    Value _add(Value const lhs, Value const rhs);    // lhs + rhs
    Value _sub(Value const lhs, Value const rhs);    // lhs - rhs
    Value _mul(Value const lhs, Value const rhs);    // lhs * rhs
    Value _div(Value const lhs, Value const rhs);    // lhs / rhs
    Value _mod(Value const lhs, Value const rhs);    // lhs % rhs
    Value _pow(Value const lhs, Value const rhs);    // lhs ** rhs
    Value _cnc(Value const lhs, Value const rhs);    // str concat
    Value _bitand(Value const lhs, Value const rhs); // lhs & rhs
    Value _bitor(Value const lhs, Value const rhs);  // lhs | rhs
    Value _bitxor(Value const lhs, Value const rhs); // lhs ^ rhs
    Value _shl(Value const lhs, Value const rhs);    // lhs << rhs
    Value _shr(Value const lhs, Value const rhs);    // lhs >> rhs

    // unary ops
    Value _neg(Value const a);    // -a
    Value _bitnot(Value const a); // ~a

    // cmp
    bool _eq(Value const lhs, Value const rhs); // lhs = rhs
    bool _lt(Value const lhs, Value const rhs); // lhs < rhs
    bool _le(Value const lhs, Value const rhs); // lhs <= rhs

    ObjString* intern(StringRef const& str);
    void ensureStack(int needed);
    void closeUpvalues(unsigned int from_stack_pos);
    void updateIcBinary(Chunk* ch, uint32_t nop_ip, Value lhs, Value rhs, Value result);
    ObjUpvalue* captureUpvalue(unsigned int stack_pos);
    void callValue(Value callee, int argc, int base, bool tail);
    Value callNative(ObjNative* nat, int argc, int base);
    void returnFromCall(int ret_reg, int n_ret);

    void openStdlib();
    void registerNative(StringRef const& name, NativeFn fn, int arity = -1)
    {
        ObjString* name_obj = makeObjectString(name);
        Globals_[name] = Value::object(makeObjectNative(fn, name_obj, arity));
    }
}; // class VM

} // namespace mylang::runtime

#endif // VM_HPP
