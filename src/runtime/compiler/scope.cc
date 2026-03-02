#include "../../../include/runtime/compiler/compiler.hpp"

namespace mylang {
namespace runtime {

void Compiler::beginScope()
{
    ++Current_->scopeDepth;
}

void Compiler::endScope(uint32_t line)
{
    --Current_->scopeDepth;

    // Pop locals that belong to this scope.
    // If any were captured, close their upvalues.
    int depth = Current_->scopeDepth;
    auto& locals = Current_->locals;

    // Find the first local to remove
    int pop_from = static_cast<int>(locals.size());
    while (pop_from > 0 && locals[pop_from - 1].depth > depth)
        --pop_from;

    // Check if any are captured
    bool any_captured = false;
    for (int i = pop_from; i < static_cast<int>(locals.size()); ++i) {
        if (locals[i].isCaptured) {
            any_captured = true;
            break;
        }
    }

    if (any_captured && pop_from < static_cast<int>(locals.size())) {
        Reg first = locals[pop_from].reg;
        emit(make_ABC(static_cast<uint8_t>(OpCode::CLOSE_UPVALUE), first, 0, 0), line);
    }

    // Free registers back to pop_from (they're in the locals)
    if (pop_from < static_cast<int>(locals.size()))
        Current_->nextReg = locals[pop_from].reg;

    locals.resize(pop_from);
}

void Compiler::declareLocal(StringRef const& name, Reg reg, bool is_const)
{
    Current_->locals.push_back({ name, Current_->scopeDepth, reg, false, is_const });
}

Compiler::VarInfo Compiler::resolveName(StringRef const& name)
{
    // Search locals in current function (innermost scope first)
    auto& locals = Current_->locals;
    for (int i = static_cast<int>(locals.size()) - 1; i >= 0; --i) 
        if (locals[i].name == name)
            return { VarInfo::Kind::LOCAL, locals[i].reg };

    // Search enclosing functions for upvalues
    int uv_idx = addUpValue(Current_->enclosing, /*not found yet*/ false, 0);
    // Re-implement properly with recursive search:
    int uv = resolveUpValue(Current_, name);
    if (uv >= 0)
        return { VarInfo::Kind::UPVALUE, static_cast<uint8_t>(uv) };

    // Global
    return { VarInfo::Kind::GLOBAL, 0 };
}

// Recursive upvalue resolver — modeled after Lua 5.4's resolve_upvalue.
int Compiler::resolveUpValue(CompilerState* state, StringRef const& name)
{
    if (!state->enclosing)
        return -1;

    // Look for a local in the immediately enclosing function
    auto& enc_locals = state->enclosing->locals;
    for (int i = static_cast<int>(enc_locals.size()) - 1; i >= 0; --i) {
        if (enc_locals[i].name == name) {
            enc_locals[i].isCaptured = true;
            return addUpValue(state, true, enc_locals[i].reg);
        }
    }

    // Recurse upward
    int up = resolveUpValue(state->enclosing, name);
    if (up >= 0)
        return addUpValue(state, false, static_cast<uint8_t>(up));

    return -1;
}

int Compiler::addUpValue(CompilerState* state, bool isLocal, uint8_t index)
{
    // Deduplicate
    if (!state)
        return -1;

    for (int i = 0; i < static_cast<int>(state->upvalues.size()); ++i) {
        auto& uv = state->upvalues[i];
        if (uv.isLocal == isLocal && uv.index == index)
            return i;
    }

    state->upvalues.push_back({ isLocal, index });
    return static_cast<int>(state->upvalues.size() - 1);
}

}
}
