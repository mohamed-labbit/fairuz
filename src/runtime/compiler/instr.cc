#include "../../../include/runtime/compiler/compiler.hpp"

namespace mylang {
namespace runtime {

uint32_t Compiler::emit(Instruction i, uint32_t line)
{
    if (isDead_)
        return static_cast<uint32_t>(currentChunk()->code.size()); // discard

    return currentChunk()->emit(i, line);
}

uint32_t Compiler::emitJump(OpCode op, Reg cond, uint32_t line)
{
    // Emit with placeholder offset 0; caller patches later.
    return emit(make_AsBx(static_cast<uint8_t>(op), cond, 0), line);
}

void Compiler::patchJump(uint32_t idx)
{
    if (!currentChunk()->patchJump(idx))
        error("jump offset overflow", 0);
}

uint32_t Compiler::currentOffset() const
{
    return static_cast<uint32_t>(currentChunk()->code.size());
}

void Compiler::emitLoadValue(Reg dst, Value v, uint32_t line)
{
    if (v.isNil()) {
        emit(make_ABC(static_cast<uint8_t>(OpCode::LOAD_NIL), dst, dst, 1), line);
    } else if (v.isBool()) {
        OpCode op = v.asBool() ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE;
        emit(make_ABC(static_cast<uint8_t>(op), dst, 0, 0), line);
    } else if (v.isInt()) {
        int64_t iv = v.asInt();
        // Use LOAD_INT for values that fit in the signed 16-bit sBx field
        if (iv >= -32767 && iv <= 32767) {
            emit(make_ABx(static_cast<uint8_t>(OpCode::LOAD_INT), dst,
                     static_cast<uint16_t>(iv + 32767)),
                line);
        } else {
            uint16_t kidx = currentChunk()->addConstant(v);
            emit(make_ABx(static_cast<uint8_t>(OpCode::LOAD_CONST), dst, kidx), line);
        }
    } else {
        uint16_t kidx = currentChunk()->addConstant(v);
        emit(make_ABx(static_cast<uint8_t>(OpCode::LOAD_CONST), dst, kidx), line);
    }
}

void Compiler::pushLoop(uint32_t loop_start)
{
    Current_->loopStack.push_back({ {}, {}, loop_start });
}

void Compiler::popLoop(uint32_t loop_exit, uint32_t continue_target, uint32_t line)
{
    assert(!Current_->loopStack.empty());
    auto& ctx = Current_->loopStack.back();

    // Patch break → loop exit
    for (uint32_t idx : ctx.breakPatches)
        patchJumpTo(idx, loop_exit);

    // Patch continue → continue_target (for-step or while-cond)
    for (uint32_t idx : ctx.continuePatches)
        patchJumpTo(idx, continue_target);

    Current_->loopStack.pop_back();
}

void Compiler::patchJumpTo(uint32_t instr_idx, uint32_t target)
{
    int32_t offset = static_cast<int32_t>(target) - static_cast<int32_t>(instr_idx) - 1;
    if (offset > 32767 || offset < -32768) {
        error("jump offset overflow in loop", 0);
        return;
    }

    auto op = instr_op(currentChunk()->code[instr_idx]);
    auto A = instr_A(currentChunk()->code[instr_idx]);
    currentChunk()->code[instr_idx] = make_AsBx(op, A, offset);
}

void Compiler::emitBreak(uint32_t line)
{
    if (Current_->loopStack.empty()) {
        error("'break' outside loop", line);
        return;
    }

    uint32_t idx = emitJump(OpCode::JUMP, REG_NONE, line);
    Current_->loopStack.back().breakPatches.push_back(idx);
    markDead();
}

void Compiler::emitContinue(uint32_t line)
{
    if (Current_->loopStack.empty()) {
        error("'continue' outside loop", line);
        return;
    }

    uint32_t idx = emitJump(OpCode::JUMP, REG_NONE, line);
    Current_->loopStack.back().continuePatches.push_back(idx);
    markDead();
}

uint32_t Compiler::internString(StringRef const& s, uint32_t line)
{
    auto it = StringCache_.find(s);
    if (it != StringCache_.end())
        return it->second;

    // We store the string as a Value wrapping an ObjString.
    // At compile time we can't allocate GC objects, so we store the string
    // as a special "compile-time string" sentinel: we embed the raw bytes
    // into the constant pool as a tagged pointer.
    //
    // ASSUMPTION: your VM's Chunk loader will re-intern all string constants
    // before execution.  For now we use a placeholder Value with a pointer
    // to a heap-allocated ObjString that lives as long as the Chunk.
    //
    // Simpler alternative: store a std::string* cast to uintptr_t until VM load.
    // We mark these with a special tag that the VM recognises and replaces.
    //
    // For compilation purposes we just use the index.
    auto* obj = new ObjString(s); // VM GC will own this after loading
    uint16_t idx = currentChunk()->addConstant(Value::object(obj));
    StringCache_[s] = idx;
    if (idx == MAX_CONSTANTS)
        error("too many string constants", line);

    return idx;
}

}
}
