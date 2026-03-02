#include "../../../include/runtime/vm/vm.hpp"

namespace mylang {
namespace runtime {

// String interning

ObjString* VM::intern(StringRef const& s)
{
    auto it = string_table_.find(s);
    if (it != string_table_.end())
        return it->second;

    ObjString* obj = alloc_obj<ObjString>(s);
    string_table_[s] = obj;
    return obj;
}

// Constructor / Destructor

VM::VM()
{
    stack_.resize(STACK_SIZE);
    frames_.reserve(MAX_FRAMES);
}

VM::~VM()
{
    // Free all GC objects
    ObjHeader* obj = gc_head_;
    while (obj) {
        ObjHeader* next = obj->next;
        // Dispatch by type to call the right destructor
        switch (obj->type) {
        case ObjType::STRING: delete static_cast<ObjString*>(obj); break;
        case ObjType::LIST: delete static_cast<ObjList*>(obj); break;
        case ObjType::FUNCTION: delete static_cast<ObjFunction*>(obj); break;
        case ObjType::CLOSURE: delete static_cast<ObjClosure*>(obj); break;
        case ObjType::NATIVE: delete static_cast<ObjNative*>(obj); break;
        case ObjType::UPVALUE: delete static_cast<ObjUpvalue*>(obj); break;
        }
        obj = next;
    }
}

// Globals

void VM::set_global(StringRef const& name, Value v)
{
    globals_[name] = v;
}

Value VM::get_global(StringRef const& name) const
{
    auto it = globals_.find(name);
    return it != globals_.end() ? it->second : Value::nil();
}

bool VM::has_global(StringRef const& name) const
{
    return globals_.count(name) > 0;
}

// Native registration

void VM::register_native(StringRef const& name, NativeFn fn, int arity)
{
    ObjString* n = intern(name);
    ObjNative* nat = alloc_obj<ObjNative>(fn, n, arity);
    globals_[name] = Value::object(nat);
}

// run() — entry point

Value VM::run(Chunk* chunk)
{
    // Wrap the top-level chunk in a synthetic ObjFunction + ObjClosure
    ObjFunction* fn = alloc_obj<ObjFunction>();
    fn->arity = 0;
    fn->upvalue_count = 0;
    fn->chunk = chunk;
    fn->name = nullptr;

    ObjClosure* closure = alloc_obj<ObjClosure>(fn);

    // Push the closure into register 0 of the base frame
    ensure_stack(chunk->local_count + 8);
    stack_[0] = Value::object(closure);
    stack_top_ = chunk->local_count + 1;

    frames_.push_back({ closure, 0, 0 });

    Value result = execute();

    frames_.pop_back();
    return result;
}

// ensure_stack

void VM::ensure_stack(int needed)
{
    while (static_cast<int>(stack_.size()) < stack_top_ + needed)
        stack_.resize(stack_.size() * 2);
}

// Main dispatch loop

// Convenience macros — only alive inside execute()

#define READ_INSTR() (chunk()->code[frame().ip++])
#define PEEK_INSTR() (chunk()->code[frame().ip])
#define RA(i) reg(instr_A(i))
#define RB(i) reg(instr_B(i))
#define RC(i) reg(instr_C(i))

Value VM::execute()
{
    for (;;) {
        Instruction ins = READ_INSTR();
        uint8_t op = instr_op(ins);

        switch (static_cast<OpCode>(op)) {

            // Loads

        case OpCode::LOAD_NIL: {
            // Fill registers [B .. B+C)
            uint8_t start = instr_B(ins);
            uint8_t count = instr_C(ins);

            for (int i = 0; i < count; ++i)
                reg(start + i) = Value::nil();

            break;
        }

        case OpCode::LOAD_TRUE: RA(ins) = Value::boolean(true); break;
        case OpCode::LOAD_FALSE: RA(ins) = Value::boolean(false); break;

        case OpCode::LOAD_CONST: {
            uint16_t idx = instr_Bx(ins);
            Value v = chunk()->constants[idx];

            // Compile-time ObjString pointers need to be re-interned so
            // the GC owns them.  They're stored as raw Value::object().
            if (v.isString())
                v = Value::object(intern(v.asString()->chars));

            RA(ins) = v;
            break;
        }

        case OpCode::LOAD_INT: {
            int16_t raw = static_cast<int16_t>(instr_Bx(ins)) - 32767;
            RA(ins) = Value::integer(raw);
            break;
        }

        case OpCode::LOAD_GLOBAL: {
            uint16_t idx = instr_Bx(ins);
            Value name_val = chunk()->constants[idx];
            StringRef key = name_val.isString() ? name_val.asString()->chars : StringRef();
            auto it = globals_.find(key);
            RA(ins) = (it != globals_.end()) ? it->second : Value::nil();
            break;
        }

        case OpCode::STORE_GLOBAL: {
            uint16_t idx = instr_Bx(ins);
            Value name_val = chunk()->constants[idx];
            StringRef key = name_val.isString() ? name_val.asString()->chars : StringRef();
            globals_[key] = RA(ins);
            break;
        }

            // Register moves

        case OpCode::MOVE:
            RA(ins) = RB(ins);
            break;

            // Upvalues

        case OpCode::GET_UPVALUE: {
            uint8_t idx = instr_B(ins);
            RA(ins) = *frame().closure->upvalues[idx]->location;
            break;
        }

        case OpCode::SET_UPVALUE: {
            uint8_t idx = instr_B(ins);
            *frame().closure->upvalues[idx]->location = RA(ins);
            break;
        }

        case OpCode::CLOSE_UPVALUE: {
            int first_stack = frame().base + instr_A(ins);
            close_upvalues(first_stack);
            break;
        }

            // Arithmetic — integer-specialised fast paths

        case OpCode::ADD: {
            Value a = RB(ins), b = RC(ins);
            Value result;

            if (a.isInt() && b.isInt())
                result = Value::integer(a.asInt() + b.asInt());
            else
                result = do_add(a, b);

            RA(ins) = result;
            // IC profile update: the NOP immediately following carries the slot index
            if (frame().ip < (uint32_t)chunk()->code.size()) {
                Instruction nop = PEEK_INSTR();
                if (instr_op(nop) == static_cast<uint8_t>(OpCode::NOP))
                    update_ic_binary(chunk(), frame().ip, a, b, result);
            }

            break;
        }

        case OpCode::SUB: {
            Value a = RB(ins), b = RC(ins);
            Value result;

            if (a.isInt() && b.isInt())
                result = Value::integer(a.asInt() - b.asInt());
            else
                result = do_sub(a, b);

            RA(ins) = result;
            if (frame().ip < (uint32_t)chunk()->code.size()) {
                Instruction nop = PEEK_INSTR();
                if (instr_op(nop) == static_cast<uint8_t>(OpCode::NOP))
                    update_ic_binary(chunk(), frame().ip, a, b, result);
            }

            break;
        }

        case OpCode::MUL: {
            Value a = RB(ins), b = RC(ins);
            Value result;
            if (a.isInt() && b.isInt())
                result = Value::integer(a.asInt() * b.asInt());
            else
                result = do_mul(a, b);
            RA(ins) = result;
            if (frame().ip < (uint32_t)chunk()->code.size()) {
                Instruction nop = PEEK_INSTR();
                if (instr_op(nop) == static_cast<uint8_t>(OpCode::NOP))
                    update_ic_binary(chunk(), frame().ip, a, b, result);
            }
            break;
        }

        case OpCode::DIV: {
            Value a = RB(ins), b = RC(ins);
            Value result = do_div(a, b);
            RA(ins) = result;
            if (frame().ip < (uint32_t)chunk()->code.size()) {
                Instruction nop = PEEK_INSTR();
                if (instr_op(nop) == static_cast<uint8_t>(OpCode::NOP))
                    update_ic_binary(chunk(), frame().ip, a, b, result);
            }
            break;
        }

        case OpCode::IDIV: {
            Value a = RB(ins), b = RC(ins);
            Value result = do_idiv(a, b);
            RA(ins) = result;
            break;
        }

        case OpCode::MOD: {
            Value a = RB(ins), b = RC(ins);
            Value result = do_mod(a, b);
            RA(ins) = result;
            break;
        }

        case OpCode::POW: {
            Value a = RB(ins), b = RC(ins);
            RA(ins) = do_pow(a, b);
            break;
        }

        case OpCode::NEG: {
            Value a = RB(ins);
            if (a.isInt()) {
                RA(ins) = Value::integer(-a.asInt());
                break;
            }
            if (a.isDouble()) {
                RA(ins) = Value::real(-a.asDouble());
                break;
            }
            runtime_error("unary '-' applied to non-numeric value");
        }

            // ---------------------------------------------------------------
            // Bitwise
            // ---------------------------------------------------------------

        case OpCode::BAND: RA(ins) = do_band(RB(ins), RC(ins)); break;
        case OpCode::BOR: RA(ins) = do_bor(RB(ins), RC(ins)); break;
        case OpCode::BXOR: RA(ins) = do_bxor(RB(ins), RC(ins)); break;
        case OpCode::BNOT: RA(ins) = do_bnot(RB(ins)); break;
        case OpCode::SHL: RA(ins) = do_shl(RB(ins), RC(ins)); break;
        case OpCode::SHR:
            RA(ins) = do_shr(RB(ins), RC(ins));
            break;

            // Comparison — result is boolean in A

        case OpCode::EQ: RA(ins) = Value::boolean(do_eq(RB(ins), RC(ins))); break;
        case OpCode::NEQ: RA(ins) = Value::boolean(!do_eq(RB(ins), RC(ins))); break;
        case OpCode::LT: RA(ins) = Value::boolean(do_lt(RB(ins), RC(ins))); break;
        case OpCode::LE:
            RA(ins) = Value::boolean(do_le(RB(ins), RC(ins)));
            break;

            // ---------------------------------------------------------------
            // Logical
            // ---------------------------------------------------------------

        case OpCode::NOT:
            RA(ins) = Value::boolean(!RB(ins).isTruthy());
            break;

            // String concatenation

        case OpCode::CONCAT: {
            uint8_t first = instr_B(ins);
            uint8_t count = instr_C(ins);
            StringRef result;
            result.reserve(64);

            for (int i = 0; i < count; ++i) {
                Value v = reg(first + i);
                if (v.isString())
                    result += v.asString()->chars;
                else if (v.isInt())
                    result += std::to_string(v.asInt()).data();
                else if (v.isDouble())
                    result += std::to_string(v.asDouble()).data();
                else if (v.isBool())
                    result += v.asBool() ? "true" : "false";
                else if (v.isNil())
                    result += "nil";
                else
                    runtime_error("cannot concatenate value of this type");
            }

            RA(ins) = Value::object(intern(result));
            break;
        }

            // List operations

        case OpCode::NEW_LIST: {
            uint8_t cap = instr_B(ins);
            ObjList* lst = alloc_obj<ObjList>();
            lst->elements.reserve(cap);
            RA(ins) = Value::object(lst);
            break;
        }

        case OpCode::LIST_APPEND: {
            Value list_val = RA(ins);

            if (!list_val.isList())
                runtime_error("LIST_APPEND on non-list");

            list_val.asList()->elements.push_back(RB(ins));
            break;
        }

        case OpCode::LIST_GET: {
            Value list_val = RB(ins);
            Value index_val = RC(ins);

            if (!list_val.isList())
                runtime_error("index access on non-list");
            if (!index_val.isInt())
                runtime_error("list index must be an integer");

            auto& elems = list_val.asList()->elements;
            int64_t idx = index_val.asInt();

            // Support negative indexing
            if (idx < 0)
                idx += static_cast<int64_t>(elems.size());
            if (idx < 0 || idx >= static_cast<int64_t>(elems.size()))
                runtime_error("list index out of range");

            RA(ins) = elems[static_cast<size_t>(idx)];
            break;
        }

        case OpCode::LIST_SET: {
            Value list_val = RA(ins);
            Value index_val = RB(ins);
            Value new_val = RC(ins);

            if (!list_val.isList())
                runtime_error("index assignment on non-list");
            if (!index_val.isInt())
                runtime_error("list index must be an integer");

            auto& elems = list_val.asList()->elements;
            int64_t idx = index_val.asInt();

            if (idx < 0)
                idx += static_cast<int64_t>(elems.size());
            if (idx < 0 || idx >= static_cast<int64_t>(elems.size()))
                runtime_error("list index out of range");

            elems[static_cast<size_t>(idx)] = new_val;
            break;
        }

        case OpCode::LIST_LEN: {
            Value list_val = RB(ins);
            if (!list_val.isList())
                runtime_error("len() on non-list");

            RA(ins) = Value::integer(static_cast<int64_t>(list_val.asList()->elements.size()));
            break;
        }

            // Jumps

        case OpCode::JUMP:
            frame().ip += instr_sBx(ins);
            break;

        case OpCode::JUMP_IF_TRUE:
            if (RA(ins).isTruthy())
                frame().ip += instr_sBx(ins);
            break;

        case OpCode::JUMP_IF_FALSE:
            if (!RA(ins).isTruthy())
                frame().ip += instr_sBx(ins);
            break;

        case OpCode::LOOP:
            frame().ip += instr_sBx(ins); // sBx is negative → backward
            break;

            // Numeric for-loop
            //
            // Registers at base A:
            //   A+0  internal counter (starts at start - step, gets step added)
            //   A+1  limit
            //   A+2  step
            //   A+3  user loop variable (updated from A+0 each iteration)

        case OpCode::FOR_PREP: {
            uint8_t base = instr_A(ins);
            Value start = reg(base);
            Value step = reg(base + 2);

            // Subtract step so the first FOR_STEP will add it back
            if (start.isInt() && step.isInt())
                reg(base) = Value::integer(start.asInt() - step.asInt());
            else {
                double s = start.asNumberDouble();
                double st = step.asNumberDouble();
                reg(base) = Value::real(s - st);
            }

            // Fall through to FOR_STEP to do the initial check
            // (FOR_STEP immediately follows FOR_PREP in the code stream)
            // We actually jump to the FOR_STEP that follows the body by
            // falling into FOR_STEP via the next instruction.
            // The sBx here is the offset to past the body — if we're
            // already done, skip entirely.
            // We re-run the FOR_STEP logic inline:
            {
                Value& counter = reg(base);
                Value limit = reg(base + 1);
                Value& step_v = reg(base + 2);
                Value& idx = reg(base + 3);

                if (counter.isInt() && limit.isInt() && step_v.isInt()) {
                    int64_t c = counter.asInt() + step_v.asInt();
                    int64_t lim = limit.asInt();
                    int64_t st = step_v.asInt();
                    counter = Value::integer(c);
                    idx = Value::integer(c);
                    bool done = (st > 0) ? (c > lim) : (c < lim);
                    if (done)
                        frame().ip += instr_sBx(ins);
                } else {
                    double c = counter.asNumberDouble() + step_v.asNumberDouble();
                    double lim = limit.asNumberDouble();
                    double st = step_v.asNumberDouble();
                    counter = Value::real(c);
                    idx = Value::real(c);
                    bool done = (st > 0.0) ? (c > lim) : (c < lim);
                    if (done)
                        frame().ip += instr_sBx(ins);
                }
            }
            break;
        }

        case OpCode::FOR_STEP: {
            uint8_t base = instr_A(ins);
            Value& counter = reg(base);
            Value limit = reg(base + 1);
            Value& step_v = reg(base + 2);
            Value& idx = reg(base + 3);

            if (counter.isInt() && limit.isInt() && step_v.isInt()) {
                int64_t c = counter.asInt() + step_v.asInt();
                int64_t lim = limit.asInt();
                int64_t st = step_v.asInt();
                counter = Value::integer(c);
                idx = Value::integer(c);
                bool keep = (st > 0) ? (c <= lim) : (c >= lim);
                if (keep)
                    frame().ip += instr_sBx(ins); // jump back
            } else {
                double c = counter.asNumberDouble() + step_v.asNumberDouble();
                double lim = limit.asNumberDouble();
                double st = step_v.asNumberDouble();
                counter = Value::real(c);
                idx = Value::real(c);
                bool keep = (st > 0.0) ? (c <= lim) : (c >= lim);
                if (keep)
                    frame().ip += instr_sBx(ins);
            }

            break;
        }

            // Closures
            //
            // CLOSURE A Bx   — A = new closure for chunk->functions[Bx]
            // Immediately following: one MOVE pseudo-instruction per upvalue:
            //   MOVE A=is_local B=index  (A=1 → capture local, A=0 → re-capture upvalue)

        case OpCode::CLOSURE: {
            uint16_t fn_idx = instr_Bx(ins);
            Chunk* fn_chunk = chunk()->functions[fn_idx];

            // Build a synthetic ObjFunction wrapping this sub-chunk
            ObjFunction* fn = alloc_obj<ObjFunction>();
            fn->arity = fn_chunk->arity;
            fn->upvalue_count = fn_chunk->upvalue_count;
            fn->chunk = fn_chunk;
            fn->name = nullptr;

            ObjClosure* cl = alloc_obj<ObjClosure>(fn);

            // Read upvalue descriptors
            for (int i = 0; i < fn_chunk->upvalue_count; ++i) {
                Instruction desc = READ_INSTR(); // consume the MOVE pseudo-instr
                bool is_local = instr_A(desc) == 1;
                uint8_t index = instr_B(desc);

                if (is_local)
                    cl->upvalues[i] = capture_upvalue(frame().base + index);
                else
                    cl->upvalues[i] = frame().closure->upvalues[index];
            }

            RA(ins) = Value::object(cl);
            break;
        }

            // Function calls

        case OpCode::CALL: {
            uint8_t func_reg = instr_A(ins);
            uint8_t argc = instr_B(ins);
            Value callee = reg(func_reg);
            int base = frame().base + func_reg;
            call_value(callee, argc, base, /*tail=*/false);
            break;
        }

        case OpCode::IC_CALL: {
            uint8_t func_reg = instr_A(ins);
            uint8_t argc = instr_B(ins);
            uint8_t ic_idx = instr_C(ins);
            Value callee = reg(func_reg);
            int base = frame().base + func_reg;

            // Update IC slot before calling so JIT can see the callee type
            if (ic_idx < chunk()->ic_slots.size()) {
                auto& slot = chunk()->ic_slots[ic_idx];
                slot.seen_lhs |= value_type_tag(callee);
                slot.hit_count++;
            }

            call_value(callee, argc, base, /*tail=*/false);
            // Update return type in IC slot after call returns
            if (ic_idx < chunk()->ic_slots.size())
                chunk()->ic_slots[ic_idx].seen_ret |= value_type_tag(reg(func_reg)); // result lands in func_reg

            break;
        }

        case OpCode::CALL_TAIL: {
            uint8_t func_reg = instr_A(ins);
            uint8_t argc = instr_B(ins);
            Value callee = reg(func_reg);
            int base = frame().base + func_reg;
            call_value(callee, argc, base, /*tail=*/true);
            break;
        }

            // Return

        case OpCode::RETURN: {
            uint8_t src = instr_A(ins);
            uint8_t nresult = instr_B(ins);
            Value result = (nresult > 0) ? reg(src) : Value::nil(); // capture BEFORE pop
            return_from_call(src, nresult);
            if (frames_.empty())
                return result; // ← must exit execute() here
            break;
        }

        case OpCode::RETURN_NIL: {
            return_from_call(0, 0);
            if (frames_.empty())
                return Value::nil();
            break;
        }

            // Misc

        case OpCode::NOP:
            // IC descriptor NOP — consumed by the opcode that precedes it;
            // if we reach it standalone (e.g. after a binary op that already
            // peeked it), just advance.
            break;

        case OpCode::HALT:
            return stack_top_ > 0 ? stack_[0] : Value::nil();

        default:
            runtime_error("unknown opcode " + std::to_string(static_cast<int>(op)));
        }
    }
}

#undef READ_INSTR
#undef PEEK_INSTR
#undef RA
#undef RB
#undef RC

// call_value — dispatch to closure or native

void VM::call_value(Value callee, int argc, int base, bool tail)
{
    if (callee.isClosure()) {
        ObjClosure* cl = callee.asClosure();
        int arity = cl->function->arity;

        if (argc != arity)
            runtime_error("expected " + std::to_string(arity) + " arguments but got " + std::to_string(argc));

        if (static_cast<int>(frames_.size()) >= MAX_FRAMES)
            runtime_error("stack overflow");

        if (tail && !frames_.empty()) {
            // Tail-call: reuse the current frame.
            // Copy the callee and args down into the current frame's base.
            int cur_base = frames_.back().base;

            // Move callee + args to cur_base
            for (int i = 0; i <= argc; ++i)
                stack_[cur_base + i] = stack_[base + i];

            // Replace the current frame
            frames_.back() = { cl, 0, cur_base };
            // Clear registers above argc in the new frame
            ensure_stack(cl->function->chunk->local_count + 8);
            int new_top = cur_base + cl->function->chunk->local_count + 1;

            while (stack_top_ < new_top)
                stack_[stack_top_++] = Value::nil();
        } else {
            ensure_stack(cl->function->chunk->local_count + 8);
            int new_top = base + cl->function->chunk->local_count + 1;

            while (stack_top_ < new_top)
                stack_[stack_top_++] = Value::nil();

            frames_.push_back({ cl, 0, base });
        }
    } else if (callee.isNative()) {
        ObjNative* nat = callee.asNative();
        if (nat->arity >= 0 && argc != nat->arity)
            runtime_error("native '" + std::string(nat->name ? nat->name->chars.data() : "?")
                + "' expected " + std::to_string(nat->arity) + " args, got " + std::to_string(argc));

        Value result = call_native(nat, argc, base);
        // Place result in the function register (base)
        stack_[base] = result;
        stack_top_ = base + 1;
    } else
        runtime_error("attempt to call a non-function value");
}

Value VM::call_native(ObjNative* nat, int argc, int base)
{
    // Arguments are at stack_[base+1 .. base+argc]
    Value* args = &stack_[base + 1];
    return nat->fn(argc, args);
}

// return_from_call

void VM::return_from_call(int result_reg, int nresults)
{
    if (frames_.empty())
        return;

    CallFrame returning = frames_.back();
    frames_.pop_back();
    // Close any upvalues in the returning frame
    close_upvalues(returning.base);
    // The result goes into the slot where the function was (base - 1... or base)
    // Convention: the function sits at stack_[base], result replaces it.
    Value result = (nresults > 0) ? stack_[returning.base + result_reg] : Value::nil();
    if (!frames_.empty()) {
        // Find where the callee was in the caller's register file.
        // The call instruction placed the function at frame.base + func_reg.
        // After return, result lives in that same slot.
        // We stored base = frame.base + func_reg, so stack_[base] gets result.
        stack_[returning.base] = result;
        // Trim stack to just past the caller's register window
        stack_top_ = returning.base + frames_.back().closure->function->chunk->local_count + 1;
    } else {
        // Returning from the top-level script
        stack_[0] = result;
        stack_top_ = 1;
    }
}

// ---------------------------------------------------------------------------
// Upvalue management
// ---------------------------------------------------------------------------

ObjUpvalue* VM::capture_upvalue(int stack_pos)
{
    // Reuse an existing open upvalue pointing at the same stack slot
    for (ObjUpvalue* uv : open_upvalues_)
        if (uv->location == &stack_[stack_pos])
            return uv;

    ObjUpvalue* uv = alloc_obj<ObjUpvalue>(&stack_[stack_pos]);
    open_upvalues_.push_back(uv);
    return uv;
}

void VM::close_upvalues(int from_stack_pos)
{
    // Close all open upvalues whose location >= from_stack_pos
    for (auto it = open_upvalues_.begin(); it != open_upvalues_.end();) {
        ObjUpvalue* uv = *it;
        if (uv->location >= &stack_[from_stack_pos]) {
            uv->closed = *uv->location;
            uv->location = &uv->closed;
            it = open_upvalues_.erase(it);
        } else
            ++it;
    }
}

// Arithmetic helpers

static inline bool both_numeric(Value a, Value b)
{
    return a.isNumber() && b.isNumber();
}

Value VM::do_add(Value a, Value b)
{
    if (a.isString() && b.isString())
        return Value::object(intern(a.asString()->chars + b.asString()->chars));
    if (!both_numeric(a, b))
        runtime_error("'+' operands must be numbers or strings");

    return Value::real(a.asNumberDouble() + b.asNumberDouble());
}

Value VM::do_sub(Value a, Value b)
{
    if (!both_numeric(a, b))
        runtime_error("'-' operands must be numbers");

    return Value::real(a.asNumberDouble() - b.asNumberDouble());
}

Value VM::do_mul(Value a, Value b)
{
    if (!both_numeric(a, b))
        runtime_error("'*' operands must be numbers");

    return Value::real(a.asNumberDouble() * b.asNumberDouble());
}

Value VM::do_div(Value a, Value b)
{
    if (!both_numeric(a, b))
        runtime_error("'/' operands must be numbers");

    double divisor = b.asNumberDouble();
    if (divisor == 0.0)
        runtime_error("division by zero");

    return Value::real(a.asNumberDouble() / divisor);
}

Value VM::do_idiv(Value a, Value b)
{
    if (!both_numeric(a, b))
        runtime_error("'//' operands must be numbers");

    if (b.isInt()) {
        if (b.asInt() == 0)
            runtime_error("integer division by zero");

        int64_t la = a.isInt() ? a.asInt() : static_cast<int64_t>(a.asDouble());
        int64_t lb = b.asInt();
        int64_t q = la / lb;

        // Floor division: adjust if signs differ and there's a remainder
        if ((la ^ lb) < 0 && q * lb != la)
            --q;

        return Value::integer(q);
    }

    double db = b.asNumberDouble();
    if (db == 0.0)
        runtime_error("float floor-division by zero");

    return Value::real(std::floor(a.asNumberDouble() / db));
}

Value VM::do_mod(Value a, Value b)
{
    if (!both_numeric(a, b))
        runtime_error("'%' operands must be numbers");

    if (b.isInt() && a.isInt()) {
        int64_t lb = b.asInt();
        if (lb == 0)
            runtime_error("modulo by zero");

        int64_t r = a.asInt() % lb;
        if (r != 0 && (r ^ lb) < 0)
            r += lb; // Python-style mod

        return Value::integer(r);
    }

    double db = b.asNumberDouble();
    if (db == 0.0)
        runtime_error("modulo by zero");

    return Value::real(std::fmod(a.asNumberDouble(), db));
}

Value VM::do_pow(Value a, Value b)
{
    if (!both_numeric(a, b))
        runtime_error("'**' operands must be numbers");

    return Value::real(std::pow(a.asNumberDouble(), b.asNumberDouble()));
}

Value VM::do_neg(Value a)
{
    if (a.isInt())
        return Value::integer(-a.asInt());
    if (a.isDouble())
        return Value::real(-a.asDouble());

    runtime_error("unary '-' on non-number");
}

Value VM::do_concat(Value a, Value b)
{
    StringRef sa = a.isString() ? a.asString()->chars : "";
    StringRef sb = b.isString() ? b.asString()->chars : "";
    return Value::object(intern(sa + sb));
}

// Bitwise — require integer operands
static inline int64_t require_int(Value v, char const* op, VM*)
{
    if (v.isInt())
        return v.asInt();

    throw VMError(std::string("bitwise '") + op + "' requires integer operands");
}

Value VM::do_band(Value a, Value b)
{
    return Value::integer(require_int(a, "&", this) & require_int(b, "&", this));
}

Value VM::do_bor(Value a, Value b)
{
    return Value::integer(require_int(a, "|", this) | require_int(b, "|", this));
}

Value VM::do_bxor(Value a, Value b)
{
    return Value::integer(require_int(a, "^", this) ^ require_int(b, "^", this));
}

Value VM::do_bnot(Value a)
{
    if (!a.isInt())
        runtime_error("bitwise '~' requires integer operand");

    return Value::integer(~a.asInt());
}

Value VM::do_shl(Value a, Value b)
{
    int64_t la = require_int(a, "<<", this);
    int64_t lb = require_int(b, "<<", this);

    if (lb < 0 || lb >= 64)
        runtime_error("shift amount out of range");

    return Value::integer(la << lb);
}

Value VM::do_shr(Value a, Value b)
{
    int64_t la = require_int(a, ">>", this);
    int64_t lb = require_int(b, ">>", this);

    if (lb < 0 || lb >= 64)
        runtime_error("shift amount out of range");

    return Value::integer(static_cast<uint64_t>(la) >> lb); // logical shift
}

// Comparison
bool VM::do_eq(Value a, Value b)
{
    return a == b; // raw bit equality handles all same-type cases
}

bool VM::do_lt(Value a, Value b)
{
    if (a.isInt() && b.isInt())
        return a.asInt() < b.asInt();
    if (a.isNumber() && b.isNumber())
        return a.asNumberDouble() < b.asNumberDouble();
    if (a.isString() && b.isString())
        return a.asString()->chars < b.asString()->chars;

    runtime_error("'<' operands must be numbers or strings");
}

bool VM::do_le(Value a, Value b)
{
    if (a.isInt() && b.isInt())
        return a.asInt() <= b.asInt();
    if (a.isNumber() && b.isNumber())
        return a.asNumberDouble() <= b.asNumberDouble();
    if (a.isString() && b.isString())
        return a.asString()->chars <= b.asString()->chars;

    runtime_error("'<=' operands must be numbers or strings");
}

// IC profile update

void VM::update_ic_binary(Chunk* ch, uint32_t nop_ip, Value lhs, Value rhs, Value result)
{
    Instruction nop = ch->code[nop_ip];
    uint8_t ic_idx = instr_A(nop);

    if (ic_idx < ch->ic_slots.size()) {
        auto& slot = ch->ic_slots[ic_idx];
        slot.seen_lhs |= value_type_tag(lhs);
        slot.seen_rhs |= value_type_tag(rhs);
        slot.seen_ret |= value_type_tag(result);
        slot.hit_count++;
    }
}

void VM::update_ic_call(Chunk* ch, uint32_t ic_ip, Value callee, Value result)
{
    Instruction ins = ch->code[ic_ip];
    uint8_t ic_idx = instr_C(ins);

    if (ic_idx < ch->ic_slots.size()) {
        auto& slot = ch->ic_slots[ic_idx];
        slot.seen_lhs |= value_type_tag(callee);
        slot.seen_ret |= value_type_tag(result);
        slot.hit_count++;
    }
}

// Error helpers

[[noreturn]] void VM::runtime_error(std::string const& msg)
{
    std::string full = msg + "\n" + build_traceback();
    throw VMError(full);
}

std::string VM::build_traceback() const
{
    std::ostringstream ss;
    ss << "stack traceback:\n";

    for (int i = static_cast<int>(frames_.size()) - 1; i >= 0; --i) {
        CallFrame const& f = frames_[i];
        Chunk const* ch = f.closure->function->chunk;
        uint32_t line = ch->getLine(f.ip > 0 ? f.ip - 1 : 0);
        ss << "  [line " << line << "] in " << (ch->name.empty() ? "?" : ch->name) << "\n";
    }

    return ss.str();
}

}
}
