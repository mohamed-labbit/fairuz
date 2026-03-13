#include "../../include/runtime/vm.hpp"
#include "../../include/allocator.hpp"
#include "../../include/runtime/opcode.hpp"
#include "../../include/runtime/stdlib.hpp"
#include "../../include/runtime/value.hpp"

namespace mylang::runtime {

Value VM::run(Chunk* chunk)
{
    ObjFunction* fn = makeObjectFunction();
    fn->chunk = chunk;
    ObjClosure* cl = makeObjectClosure(fn);

    Stack_[0] = Value::object(cl);     // slot 0 = top-level closure (frame overhead)
    StackTop_ = chunk->localCount + 2; // +1 for self slot, +1 past locals

    Frames_[FramesTop_++] = { cl, 0, 1 }; // base = 1, reg(0) = Stack_[1]
    Value res = execute();
    --FramesTop_;

    return res;
}

Value VM::execute()
{
    auto READ_INSTR = [this]() { return chunk()->code[frame().ip++]; };
    auto PEEK_INSTR = [this]() { return chunk()->code[frame().ip]; };

    auto RA = [this](uint32_t const i) -> Value& { return reg(instr_A(i)); };
    auto RB = [this](uint32_t const i) -> Value& { return reg(instr_B(i)); };
    auto RC = [this](uint32_t const i) -> Value& { return reg(instr_C(i)); };

    for (;;) {
        uint32_t instr = READ_INSTR();
        uint8_t op = instr_op(instr);

        switch (static_cast<OpCode>(op)) {
        case OpCode::LOAD_NIL: {
            uint8_t start = instr_B(instr);
            uint8_t count = instr_C(instr);

            for (uint8_t i = 0; i < count; ++i)
                reg(start + i) = Value::nil();
        } break;
        case OpCode::LOAD_TRUE:
            RA(instr) = Value::boolean(true);
            break;
        case OpCode::LOAD_FALSE:
            RA(instr) = Value::boolean(false);
            break;
        case OpCode::LOAD_CONST: {
            uint16_t idx = instr_Bx(instr);
            Value val = chunk()->constants[idx];

            if (val.isString())
                val = Value::object(intern(val.asString()->str));

            RA(instr) = val;
        } break;
        case OpCode::LOAD_INT: {
            int32_t raw = static_cast<uint16_t>(instr_Bx(instr)) - JUMP_OFFSET;
            RA(instr) = Value::integer(raw);
        } break;
        case OpCode::LOAD_GLOBAL: {
            uint16_t idx = instr_Bx(instr);
            Value name_v = chunk()->constants[idx];
            StringRef key = name_v.isString() ? name_v.asString()->str : "";
            auto it = Globals_.find(key);
            RA(instr) = (it != Globals_.end()) ? it->second : Value::nil();
        } break;
        case OpCode::STORE_GLOBAL: {
            uint16_t idx = instr_Bx(instr);
            Value name_v = chunk()->constants[idx];
            StringRef key = name_v.isString() ? name_v.asString()->str : "";
            Globals_[key] = RA(instr);
        } break;
        case OpCode::MOVE:
            RA(instr) = RB(instr);
            break;
        case OpCode::GET_UPVALUE:
            RA(instr) = *frame().closure->upValues[instr_B(instr)]->location;
            break;
        case OpCode::SET_UPVALUE:
            *frame().closure->upValues[instr_B(instr)]->location = RA(instr);
            break;
        case OpCode::CLOSE_UPVALUE:
            closeUpvalues(frame().base + instr_A(instr));
            break;
        case OpCode::OP_ADD: {
            Value lhs = RB(instr);
            Value rhs = RC(instr);
            Value res = _add(lhs, rhs);
            RA(instr) = res;

            if (frame().ip < static_cast<uint32_t>(chunk()->code.size())) {
                uint32_t nop = PEEK_INSTR();
                if (instr_op(nop) == static_cast<uint8_t>(OpCode::NOP))
                    updateIcBinary(chunk(), frame().ip, lhs, rhs, res);
            }
        } break;
        case OpCode::OP_SUB: {
            Value lhs = RB(instr);
            Value rhs = RC(instr);
            Value res = _sub(lhs, rhs);
            RA(instr) = res;

            if (frame().ip < static_cast<uint32_t>(chunk()->code.size())) {
                uint32_t nop = PEEK_INSTR();
                if (instr_op(nop) == static_cast<uint8_t>(OpCode::NOP))
                    updateIcBinary(chunk(), frame().ip, lhs, rhs, res);
            }
        } break;
        case OpCode::OP_MUL: {
            Value lhs = RB(instr);
            Value rhs = RC(instr);
            Value res = _mul(lhs, rhs);
            RA(instr) = res;

            if (frame().ip < static_cast<uint32_t>(chunk()->code.size())) {
                uint32_t nop = PEEK_INSTR();
                if (instr_op(nop) == static_cast<uint8_t>(OpCode::NOP))
                    updateIcBinary(chunk(), frame().ip, lhs, rhs, res);
            }
        } break;
        case OpCode::OP_DIV: {
            Value lhs = RB(instr);
            Value rhs = RC(instr);
            Value res = _div(lhs, rhs);
            RA(instr) = res;

            if (frame().ip < static_cast<uint32_t>(chunk()->code.size())) {
                uint32_t nop = PEEK_INSTR();
                if (instr_op(nop) == static_cast<uint8_t>(OpCode::NOP))
                    updateIcBinary(chunk(), frame().ip, lhs, rhs, res);
            }
        } break;
        case OpCode::OP_MOD:
            RA(instr) = _mod(RB(instr), RC(instr));
            break;
        case OpCode::OP_POW:
            RA(instr) = _pow(RB(instr), RC(instr));
            break;
        case OpCode::OP_NEG:
            RA(instr) = _neg(RB(instr));
            break;
        case OpCode::OP_BITAND:
            RA(instr) = _bitand(RB(instr), RC(instr));
            break;
        case OpCode::OP_BITOR:
            RA(instr) = _bitor(RB(instr), RC(instr));
            break;
        case OpCode::OP_BITXOR:
            RA(instr) = _bitxor(RB(instr), RC(instr));
            break;
        case OpCode::OP_BITNOT:
            RA(instr) = _bitnot(RB(instr));
            break;
        case OpCode::OP_LSHIFT:
            RA(instr) = _shl(RB(instr), RC(instr));
            break;
        case OpCode::OP_RSHIFT:
            RA(instr) = _shr(RB(instr), RC(instr));
            break;
        case OpCode::OP_EQ:
            RA(instr) = Value::boolean(_eq(RB(instr), RC(instr)));
            break;
        case OpCode::OP_NEQ:
            RA(instr) = Value::boolean(!_eq(RB(instr), RC(instr)));
            break;
        case OpCode::OP_LT:
            RA(instr) = Value::boolean(_lt(RB(instr), RC(instr)));
            break;
        case OpCode::OP_LTE:
            RA(instr) = Value::boolean(_le(RB(instr), RC(instr)));
            break;
        case OpCode::OP_NOT:
            RA(instr) = Value::boolean(!RB(instr).isTruthy());
            break;
        case OpCode::CONCAT:
            RA(instr) = _cnc(RB(instr), RC(instr));
            break; /// needs checking
        case OpCode::LIST_NEW: {
            uint8_t cap = instr_B(instr);
            ObjList* list_obj = makeObjectList();
            list_obj->reserve(cap);
            RA(instr) = Value::object(list_obj);
        } break;
        case OpCode::LIST_APPEND: {
            Value list_v = RA(instr);

            if (!list_v.isList())
                diagnostic::emit("LIST_APPEND on non-list", diagnostic::Severity::FATAL);

            list_v.asList()->elements.push(RB(instr));
        } break;
        case OpCode::LIST_GET: {
            Value list_v = RB(instr), index_v = RC(instr);

            if (!list_v.isList())
                diagnostic::emit("index access on non-list", diagnostic::Severity::FATAL);

            if (!index_v.isInteger())
                diagnostic::emit("list index must be an integer", diagnostic::Severity::FATAL);

            auto& elems = list_v.asList()->elements;
            int64_t idx = index_v.asInteger();

            if (idx < 0 || idx >= static_cast<int64_t>(elems.size()))
                diagnostic::emit("list index out of range", diagnostic::Severity::FATAL);

            RA(instr) = elems[static_cast<size_t>(idx)];
        } break;
        case OpCode::LIST_SET: {
            Value list_v = RA(instr), index_v = RB(instr), new_val = RC(instr);

            if (!list_v.isList())
                diagnostic::emit("index assignment on non-list", diagnostic::Severity::FATAL);

            if (!index_v.isInteger())
                diagnostic::emit("list index must be an integer", diagnostic::Severity::FATAL);

            auto& elems = list_v.asList()->elements;
            int64_t idx = index_v.asInteger();

            if (idx < 0)
                idx += static_cast<int64_t>(elems.size());

            if (idx < 0 || idx >= static_cast<int64_t>(elems.size()))
                diagnostic::emit("list index out of range", diagnostic::Severity::FATAL);

            elems[static_cast<size_t>(idx)] = new_val;
        } break;
        case OpCode::LIST_LEN: {
            Value list_v = RB(instr);

            if (!list_v.isList())
                diagnostic::emit("len() on non-list", diagnostic::Severity::FATAL);

            RA(instr) = Value::integer(static_cast<int64_t>(list_v.asList()->elements.size()));
        } break;
        case OpCode::JUMP:
            frame().ip += instr_sBx(instr);
            break;
        case OpCode::JUMP_IF_TRUE:
            if (RA(instr).isTruthy())
                frame().ip += instr_sBx(instr);

            break;
        case OpCode::JUMP_IF_FALSE:
            if (!RA(instr).isTruthy())
                frame().ip += instr_sBx(instr);

            break;
        case OpCode::LOOP:
            frame().ip += instr_sBx(instr);
            break;
        case OpCode::FOR_PREP:
            break;
        case OpCode::FOR_STEP:
            break;
        case OpCode::CLOSURE: {
            uint16_t fn_idx = instr_Bx(instr);
            Chunk* fn_chunk = chunk()->functions[fn_idx];
            ObjFunction* fn = makeObjectFunction(fn_chunk);
            fn->arity = fn_chunk->arity;
            fn->upvalueCount = fn_chunk->upvalueCount;
            ObjClosure* cl = makeObjectClosure(fn);

            for (unsigned int i = 0; i < fn_chunk->upvalueCount; ++i) {
                uint32_t desc = READ_INSTR();
                bool is_local = instr_A(desc) == 1;
                uint8_t index = instr_B(desc);

                if (is_local)
                    cl->upValues[i] = captureUpvalue(frame().base + index);
                else
                    cl->upValues[i] = frame().closure->upValues[index];
            }

            RA(instr) = Value::object(cl);
        } break;
        case OpCode::CALL: {
            uint8_t fn_reg = instr_A(instr);
            uint8_t argc = instr_B(instr);
            Value callee = reg(fn_reg);
            int base = frame().base + fn_reg + 1; // +1: skip closure slot

            callValue(callee, argc, base, false);
        } break;
        case OpCode::IC_CALL: {
            uint8_t fn_reg = instr_A(instr);
            uint8_t argc = instr_B(instr);
            uint8_t ic_idx = instr_C(instr);
            Value callee = reg(fn_reg);
            int base = frame().base + fn_reg + 1; // +1 here too

            if (ic_idx < chunk()->icSlots.size()) {
                auto& slot = chunk()->icSlots[ic_idx];
                slot.seenLhs |= valueTypeTag(callee);
                slot.hitCount++;
            }

            callValue(callee, argc, base, /*tail=*/false);

            if (ic_idx < chunk()->icSlots.size())
                chunk()->icSlots[ic_idx].seenRet |= valueTypeTag(reg(fn_reg));
        } break;
        case OpCode::CALL_TAIL: {
            uint8_t fn_reg = instr_A(instr);
            uint8_t argc = instr_B(instr);
            Value callee = reg(fn_reg);
            int base = frame().base + fn_reg + 1;

            callValue(callee, argc, base, true);
        } break;
        case OpCode::RETURN: {
            uint8_t src = instr_A(instr);
            uint8_t n_ret = instr_B(instr);
            Value ret = (n_ret > 0) ? reg(src) : Value::nil();

            returnFromCall(src, n_ret);

            if (FramesTop_ == 0)
                return ret;
        } break;
        case OpCode::RETURN_NIL:
            returnFromCall(0, 0);

            if (FramesTop_ == 0)
                return Value::nil();

            break;
        case OpCode::NOP:
            break;
        case OpCode::HALT:
            return StackTop_ > 0 ? Stack_[0] : Value::nil();
        default:
            diagnostic::emit("Unknown opcode : " + std::to_string(static_cast<int>(op)), diagnostic::Severity::FATAL);
        }
    }
}

namespace {

// dispatch helpers that take the op as template/lambda params
template<typename IntOp, typename RealOp>
Value _binaryArith(Value const lhs, Value const rhs, IntOp intOp, RealOp realOp)
{
    if (!lhs.isNumber() || !rhs.isNumber())
        diagnostic::emit("binary arithmetic on non numeric operand is not allowed", diagnostic::Severity::FATAL);

    if (lhs.isInteger() && rhs.isInteger())
        return Value::integer(intOp(lhs.asInteger(), rhs.asInteger()));

    return Value::real(realOp(lhs.asDoubleAny(), rhs.asDoubleAny()));
}

template<typename IntOp>
Value _binaryBitwise(Value const lhs, Value const rhs, IntOp intOp)
{
    if (!lhs.isInteger() || !rhs.isInteger())
        diagnostic::emit("bitwise op on non integer operand is not allowed", diagnostic::Severity::FATAL);

    return Value::integer(intOp(lhs.asInteger(), rhs.asInteger()));
}

template<typename IntOp>
Value _binaryShift(Value const lhs, Value const rhs, IntOp intOp)
{
    if (!lhs.isInteger() || !rhs.isInteger())
        diagnostic::emit("bit shift on non integer operand is not allowed", diagnostic::Severity::FATAL);

    auto shift = rhs.asInteger();
    if (shift < 0 || shift >= std::numeric_limits<decltype(lhs.asInteger())>::digits)
        diagnostic::emit("shift amount out of range", diagnostic::Severity::FATAL);

    return Value::integer(intOp(lhs.asInteger(), shift));
}

template<typename IntOp, typename RealOp>
bool _binaryCmp(Value const lhs, Value const rhs, IntOp intOp, RealOp realOp)
{
    if (lhs.isInteger() && rhs.isInteger())
        return intOp(lhs.asInteger(), rhs.asInteger());

    if (lhs.isString() && rhs.isString())
        return realOp(lhs.asString(), rhs.asString());

    return realOp(lhs.asDoubleAny(), rhs.asDoubleAny());
}

template<typename T>
T intPow(T base, T exp)
{
    assert(exp >= 0);
    T result = 1;
    while (exp > 0) {
        if (exp & 1)
            result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
}

} // namespace anonymous

Value VM::_add(Value const lhs, Value const rhs)
{
    return _binaryArith(lhs, rhs, std::plus<> { }, std::plus<> { });
}

Value VM::_sub(Value const lhs, Value const rhs)
{
    return _binaryArith(lhs, rhs, std::minus<> { }, std::minus<> { });
}

Value VM::_mul(Value const lhs, Value const rhs)
{
    return _binaryArith(lhs, rhs, std::multiplies<> { }, std::multiplies<> { });
}

Value VM::_cnc(Value const lhs, Value const rhs)
{
    if (!lhs.isString() || !rhs.isString())
        diagnostic::emit("String concatenation can only apply to string operands", diagnostic::Severity::FATAL);

    return Value::object(intern(lhs.asString()->str + rhs.asString()->str));
}

Value VM::_div(Value const lhs, Value const rhs)
{
    if (rhs.asDoubleAny() == 0.0)
        diagnostic::emit("division by zero is undefined", diagnostic::Severity::FATAL);

    return _binaryArith(lhs, rhs, std::divides<> { }, std::divides<> { });
}

Value VM::_mod(Value const lhs, Value const rhs)
{
    if (rhs.asDoubleAny() == 0.0)
        diagnostic::emit("modulo by zero is undefined", diagnostic::Severity::FATAL);

    return _binaryArith(lhs, rhs, std::modulus<> { }, [](double a, double b) { return std::fmod(a, b); });
}

Value VM::_pow(Value const lhs, Value const rhs)
{
    assert(lhs.isNumber() && rhs.isNumber());

    if (lhs.isInteger() && rhs.isInteger()) {
        auto exp = rhs.asInteger();
        if (exp < 0)
            diagnostic::emit("negative integer exponent is undefined", diagnostic::Severity::FATAL);
        return Value::integer(intPow(lhs.asInteger(), exp));
    }

    return Value::real(std::pow(lhs.asDoubleAny(), rhs.asDoubleAny()));
}

Value VM::_bitand(Value const lhs, Value const rhs)
{
    return _binaryBitwise(lhs, rhs, std::bit_and<> { });
}

Value VM::_bitor(Value const lhs, Value const rhs)
{
    return _binaryBitwise(lhs, rhs, std::bit_or<> { });
}

Value VM::_bitxor(Value const lhs, Value const rhs)
{
    return _binaryBitwise(lhs, rhs, std::bit_xor<> { });
}

Value VM::_shl(Value const lhs, Value const rhs)
{
    return _binaryShift(lhs, rhs, [](auto a, auto b) { return a << b; });
}

Value VM::_shr(Value const lhs, Value const rhs)
{
    return _binaryShift(lhs, rhs, [](int64_t a, int64_t b) {
        return static_cast<int64_t>(static_cast<uint64_t>(a) >> static_cast<uint64_t>(b));
    });
}

Value VM::_neg(Value const a)
{
    if (!a.isNumber())
        diagnostic::emit("'-' on a non numeric value is not allowed", diagnostic::Severity::FATAL);

    return a.isInteger() ? Value::integer(-a.asInteger()) : Value::real(-a.asDouble());
}

Value VM::_bitnot(Value const a)
{
    if (!a.isInteger())
        diagnostic::emit("'~' on non integer is not allowed", diagnostic::Severity::FATAL);

    return Value::integer(~a.asInteger());
}

bool VM::_eq(Value const lhs, Value const rhs)
{
    return _binaryCmp(lhs, rhs, std::equal_to<> { }, std::equal_to<> { });
}

bool VM::_lt(Value const lhs, Value const rhs)
{
    return _binaryCmp(lhs, rhs, std::less<> { }, std::less<> { });
}

bool VM::_le(Value const lhs, Value const rhs)
{
    return _binaryCmp(lhs, rhs, std::less_equal<> { }, std::less_equal<> { });
}

ObjString* VM::intern(StringRef const& str)
{
    auto it = StringTable_.find(str);
    if (it != StringTable_.end())
        return it->second;

    ObjString* obj = makeObjectString(str);
    StringTable_[str] = obj;
    return obj;
}

void VM::ensureStack(int needed)
{
    if (static_cast<size_t>(StackTop_) + static_cast<size_t>(needed) > STACK_SIZE)
        diagnostic::emit("stack overflow", diagnostic::Severity::FATAL);
}

void VM::closeUpvalues(unsigned int from_stack_pos)
{
    // Close all open upvalues whose location >= from_stack_pos
    for (auto it = OpenUpvalues_.begin(); it != OpenUpvalues_.end();) {
        ObjUpvalue* uv = *it;
        if (uv->location >= &Stack_[from_stack_pos]) {
            uv->closed = *uv->location;
            uv->location = &uv->closed;
            it = OpenUpvalues_.erase(it);
        } else
            ++it;
    }
}

void VM::updateIcBinary(Chunk* ch, uint32_t nop_ip, Value lhs, Value rhs, Value result)
{
    uint32_t nop = ch->code[nop_ip];
    uint8_t ic_idx = instr_A(nop);

    if (ic_idx < ch->icSlots.size()) {
        auto& slot = ch->icSlots[ic_idx];
        slot.seenLhs |= valueTypeTag(lhs);
        slot.seenRhs |= valueTypeTag(rhs);
        slot.seenRet |= valueTypeTag(result);
        slot.hitCount++;
    }
}

ObjUpvalue* VM::captureUpvalue(unsigned int stack_pos)
{
    // reuse an existing open upvalue pointing at the same stack slot
    for (ObjUpvalue* uv : OpenUpvalues_)
        if (uv->location == &Stack_[stack_pos])
            return uv;

    ObjUpvalue* uv = makeObjectUpvalue(&Stack_[stack_pos]);
    OpenUpvalues_.push(uv);
    return uv;
}

void VM::callValue(Value callee, int argc, int base, bool tail)
{
    if (callee.isClosure()) {
        ObjClosure* cl = callee.asClosure();
        int arity = cl->function->arity;

        if (argc != arity)
            diagnostic::emit("expected " + std::to_string(arity) + " arguments but got " + std::to_string(argc),
                diagnostic::Severity::FATAL);

        if (FramesTop_ >= MAX_FRAMES)
            diagnostic::emit("Stack overflow", diagnostic::Severity::FATAL);

        if (tail && FramesTop_ > 0) {
            int cur_base = Frames_[FramesTop_ - 1].base; // ← was Frames_.back()

            for (int i = 0; i < argc; ++i)
                Stack_[cur_base + i] = Stack_[base + i];

            for (int i = argc; i < cl->function->chunk->localCount; ++i)
                Stack_[cur_base + i] = Value::nil();

            Frames_[FramesTop_ - 1] = { cl, 0, cur_base }; // ← was Frames_.back()

            int new_top = cur_base + cl->function->chunk->localCount + 1;
            while (StackTop_ < new_top)
                Stack_[StackTop_++] = Value::nil();
            StackTop_ = new_top;
        } else {
            // ensureStack(cl->function->chunk->localCount + 8);
            int new_top = base + cl->function->chunk->localCount + 1;

            while (StackTop_ < new_top)
                Stack_[StackTop_++] = Value::nil();

            Frames_[FramesTop_++] = { cl, 0, base };
        }

        return;
    } else if (callee.isNative()) {
        ObjNative* nat = callee.asNative();
        if (nat->arity >= 0 && argc != nat->arity)
            diagnostic::emit("native '" + std::string(nat->name ? nat->name->str.data() : "?")
                + "' expected " + std::to_string(nat->arity) + " args, got " + std::to_string(argc));

        Value result = callNative(nat, argc, base);
        // Write result into fn_reg slot (base - 1), mirroring returnFromCall for closures.
        // base = caller_frame.base + fn_reg + 1, so base-1 is exactly the fn_reg slot.
        Stack_[base - 1] = result;
        // StackTop_ is unchanged — no frame was pushed, so nothing to clean up.
        return;
    }

    diagnostic::emit("Attempting call on non-function value", diagnostic::Severity::FATAL);
}

Value VM::callNative(ObjNative* nat, int argc, int base)
{
    // base already points at arg 0 (= frame().base + fn_reg + 1)
    Value* args = &Stack_[base]; // ← was base + 1
    return nat->fn(argc, args);
}

void VM::returnFromCall(int ret_reg, int n_ret)
{
    if (FramesTop_ == 0)
        return;

    CallFrame returning = Frames_[FramesTop_ - 1];
    --FramesTop_;
    closeUpvalues(returning.base);
    Value ret = (n_ret > 0) ? Stack_[returning.base + ret_reg] : Value::nil();
    Stack_[returning.base - 1] = ret; // write return value into closure slot (fn_reg in caller)

    if (FramesTop_ > 0) {
        Stack_[returning.base] = ret;
        // Use the CALLER's localCount (Frames_[FramesTop_ - 1]), not returning's
        StackTop_ = (returning.base - 1) + Frames_[FramesTop_ - 1].closure->function->chunk->localCount + 1;
    } else {
        Stack_[0] = ret;
        StackTop_ = 1;
    }
}

typename VM::CallFrame& VM::frame()
{
    return Frames_[FramesTop_ - 1];
}

typename VM::CallFrame const& VM::frame() const
{
    return Frames_[FramesTop_ - 1];
}

Chunk* VM::chunk()
{
    return frame().closure->function->chunk;
}

Value& VM::reg(int r)
{
    return Stack_[frame().base + r];
}

void VM::openStdlib()
{
    registerNative("len", [](int argc, Value* argv) -> Value { return nativeLen(argc, argv); }, -1);
    registerNative("append", [](int argc, Value* argv) -> Value { return nativeAppend(argc, argv); }, -1);
    registerNative("pop", [](int argc, Value* argv) -> Value { return nativePop(argc, argv); }, -1);
    registerNative("slice", [](int argc, Value* argv) -> Value { return nativeSlice(argc, argv); }, -1);
    registerNative("print", [](int argc, Value* argv) -> Value { return nativePrint(argc, argv); }, -1);
    registerNative("input", [](int argc, Value* argv) -> Value { return nativeInput(argc, argv); }, -1);
    registerNative("type", [](int argc, Value* argv) -> Value { return nativeType(argc, argv); }, -1);
    registerNative("int", [](int argc, Value* argv) -> Value { return nativeInt(argc, argv); }, -1);
    registerNative("float", [](int argc, Value* argv) -> Value { return nativeFloat(argc, argv); }, -1);
    registerNative("str", [](int argc, Value* argv) -> Value { return nativeStr(argc, argv); }, -1);
    registerNative("bool", [](int argc, Value* argv) -> Value { return nativeBool(argc, argv); }, -1);
    registerNative("list", [](int argc, Value* argv) -> Value { return nativeList(argc, argv); }, -1);
    registerNative("split", [](int argc, Value* argv) -> Value { return nativeSplit(argc, argv); }, -1);
    registerNative("join", [](int argc, Value* argv) -> Value { return nativeJoin(argc, argv); }, -1);
    registerNative("substr", [](int argc, Value* argv) -> Value { return nativeSubstr(argc, argv); }, -1);
    registerNative("contains", [](int argc, Value* argv) -> Value { return nativeContains(argc, argv); }, -1);
    registerNative("trim", [](int argc, Value* argv) -> Value { return nativeTrim(argc, argv); }, -1);
    registerNative("floor", [](int argc, Value* argv) -> Value { return nativeFloor(argc, argv); }, -1);
    registerNative("ceil", [](int argc, Value* argv) -> Value { return nativeCeil(argc, argv); }, -1);
    registerNative("round", [](int argc, Value* argv) -> Value { return nativeRound(argc, argv); }, -1);
    registerNative("abs", [](int argc, Value* argv) -> Value { return nativeAbs(argc, argv); }, -1);
    registerNative("min", [](int argc, Value* argv) -> Value { return nativeMin(argc, argv); }, -1);
    registerNative("max", [](int argc, Value* argv) -> Value { return nativeMax(argc, argv); }, -1);
    registerNative("pow", [](int argc, Value* argv) -> Value { return nativePow(argc, argv); }, -1);
    registerNative("sqrt", [](int argc, Value* argv) -> Value { return nativeSqrt(argc, argv); }, -1);
    registerNative("assert", [](int argc, Value* argv) -> Value { return nativeAssert(argc, argv); }, -1);
    registerNative("clock", [](int argc, Value* argv) -> Value { return nativeClock(argc, argv); }, -1);
    registerNative("error", [](int argc, Value* argv) -> Value { return nativeError(argc, argv); }, -1);
    registerNative("time", [](int argc, Value* argv) -> Value { return nativeTime(argc, argv); }, -1);

    /// TODO: ...
}

} // namespace mylang::runtime
