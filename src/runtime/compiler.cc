#include "../../include/runtime/compiler.hpp"
#include "../../include/ast.hpp"
#include "../../include/runtime/runtime_allocator.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace mylang {
namespace runtime {

using namespace ast;

Reg Compiler::compileExpr(Expr const* e, Reg* dst)
{
    if (!e)
        return errorReg();

    if (had_error())
        return 0;

    switch (e->getKind()) {
    case Expr::Kind::LITERAL: return compileLiteral(static_cast<LiteralExpr const*>(e), dst);
    case Expr::Kind::NAME: return compileName(static_cast<NameExpr const*>(e), dst);
    case Expr::Kind::BINARY: return compileBinary(static_cast<BinaryExpr const*>(e), dst);
    case Expr::Kind::UNARY: return compileUnary(static_cast<UnaryExpr const*>(e), dst);
    case Expr::Kind::ASSIGNMENT: return compileAssignmentExpr(static_cast<AssignmentExpr const*>(e), dst);
    case Expr::Kind::CALL: return compileCall(static_cast<CallExpr const*>(e), dst, /*tail=*/false);
    case Expr::Kind::LIST: return compileList(static_cast<ListExpr const*>(e), dst);
    case Expr::Kind::INVALID:
        error("invalid expression node", e->getLine());
        return errorReg();
    }

    return errorReg();
}

Reg Compiler::compileLiteral(LiteralExpr const* e, Reg* dst)
{
    uint32_t line = e->getLine();
    Reg d = ensureDst(dst);

    if (e->isBoolean()) {
        OpCode op = e->getBool() ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE;
        emit(make_ABC(static_cast<uint8_t>(op), d, 0, 0), line);
        return d;
    }

    if (e->isNil()) {
        emit(make_ABC(static_cast<uint8_t>(OpCode::LOAD_NIL), d, d, 1), line);
        return d;
    }

    if (e->isInteger()) {
        int64_t v = e->getInt();
        emitLoadValue(d, Value::integer(v), line);
        return d;
    }

    if (e->isDecimal()) {
        double v = e->getFloat();
        // Constant fold: if the double is exactly an integer, use integer repr
        int64_t iv;

        if (util::isIntegerValue(v, iv))
            emitLoadValue(d, Value::integer(iv), line);
        else
            emitLoadValue(d, Value::real(v), line);

        return d;
    }

    if (e->isString()) {
        uint16_t kidx = internString(e->getStr(), line);
        emit(make_ABx(static_cast<uint8_t>(OpCode::LOAD_CONST), d, kidx), line);
        return d;
    }

    error("unknown literal type", line);
    return errorReg();
}

Reg Compiler::compileName(NameExpr const* e, Reg* dst)
{
    uint32_t line = e->getLine();
    VarInfo vi = resolveName(e->getValue());

    switch (vi.kind) {
    case VarInfo::Kind::LOCAL:
        // Already in a register.  If dst is provided and differs, emit MOVE.
        if (dst && *dst != vi.index) {
            emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), *dst, vi.index, 0), line);
            return *dst;
        }
        return vi.index;

    case VarInfo::Kind::UPVALUE: {
        Reg d = ensureDst(dst);
        emit(make_ABC(static_cast<uint8_t>(OpCode::GET_UPVALUE), d, vi.index, 0), line);
        return d;
    }

    case VarInfo::Kind::GLOBAL: {
        Reg d = ensureDst(dst);
        uint16_t kidx = internString(e->getValue(), line);
        emit(make_ABx(static_cast<uint8_t>(OpCode::LOAD_GLOBAL), d, kidx), line);
        return d;
    }
    }
    return errorReg();
}

Reg Compiler::compileBinary(BinaryExpr const* e, Reg* dst)
{
    uint32_t line = e->getLine();

    // --- Constant folding ---
    if (auto cv = tryFoldBinary(e)) {
        Reg d = ensureDst(dst);
        emitLoadValue(d, *cv, line);
        return d;
    }

    BinaryOp const op = e->getOperator();

    // --- Short-circuit && / || ---
    if (op == BinaryOp::OP_AND) {
        Reg d = ensureDst(dst);
        Reg mark = Current_->nextReg;
        Reg left = compileExpr(e->getLeft(), &d);

        if (left != d)
            emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), d, left, 0), line);

        uint32_t skip = emitJump(OpCode::JUMP_IF_FALSE, d, line);
        freeRegsTo(mark);
        Reg R = compileExpr(e->getRight(), &d);

        if (R != d)
            emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), d, R, 0), line);

        patchJump(skip);
        return d;
    }

    if (op == BinaryOp::OP_OR) {
        Reg d = ensureDst(dst);
        Reg mark = Current_->nextReg;
        Reg left = compileExpr(e->getLeft(), &d);

        if (left != d)
            emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), d, left, 0), line);

        uint32_t skip = emitJump(OpCode::JUMP_IF_TRUE, d, line);
        freeRegsTo(mark);
        Reg R = compileExpr(e->getRight(), &d);

        if (R != d)
            emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), d, R, 0), line);

        patchJump(skip);
        return d;
    }

    // --- General binary op ---
    Reg mark = Current_->nextReg;
    Reg L = compileExpr(e->getLeft());
    Reg R = compileExpr(e->getRight());
    Reg d = ensureDst(dst);

    // Allocate IC slot for the JIT to profile types on this binary op
    uint8_t ic = currentChunk()->allocIcSlot();

    OpCode bc_op;
    switch (op) {
    case BinaryOp::OP_ADD: bc_op = OpCode::ADD; break;
    case BinaryOp::OP_SUB: bc_op = OpCode::SUB; break;
    case BinaryOp::OP_MUL: bc_op = OpCode::MUL; break;
    case BinaryOp::OP_DIV: bc_op = OpCode::DIV; break;
    // case BinaryOp::Op::IDIV: bc_op = OpCode::IDIV; break;
    case BinaryOp::OP_MOD: bc_op = OpCode::MOD; break;
    // case BinaryOp::OP_POW: bc_op = OpCode::POW; break;
    case BinaryOp::OP_EQ: bc_op = OpCode::EQ; break;
    case BinaryOp::OP_NEQ: bc_op = OpCode::NEQ; break;
    case BinaryOp::OP_LT: bc_op = OpCode::LT; break;
    case BinaryOp::OP_LTE: bc_op = OpCode::LE; break;
    case BinaryOp::OP_GT: // normalize: a > b  ===  b < a
        std::swap(L, R);
        bc_op = OpCode::LT;
        break;
    case BinaryOp::OP_GTE: // normalize: a >= b ===  b <= a
        std::swap(L, R);
        bc_op = OpCode::LE;
        break;
    case BinaryOp::OP_BITAND: bc_op = OpCode::BAND; break;
    case BinaryOp::OP_BITOR: bc_op = OpCode::BOR; break;
    case BinaryOp::OP_BITXOR: bc_op = OpCode::BXOR; break;
    case BinaryOp::OP_LSHIFT: bc_op = OpCode::SHL; break;
    case BinaryOp::OP_RSHIFT: bc_op = OpCode::SHR; break;
    default:
        error("unknown binary operator", line);
        return errorReg();
    }

    emit(make_ABC(static_cast<uint8_t>(bc_op), d, L, R), line);
    // Emit IC slot as a NOP carrying the slot index so the JIT can find it.
    emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), ic, 0, 0), line);

    freeRegsTo(mark);
    return d;
}

Reg Compiler::compileUnary(UnaryExpr const* e, Reg* dst)
{
    uint32_t line = e->getLine();

    if (auto cv = tryFoldUnary(e)) {
        Reg d = ensureDst(dst);
        emitLoadValue(d, *cv, line);
        return d;
    }

    Reg mark = Current_->nextReg;
    Reg src = compileExpr(e->getOperand());
    Reg d = ensureDst(dst);

    OpCode op;
    switch (e->getOperator()) {
    // case UnaryExpr::OP_NEG: op = OpCode::NEG; break;
    case UnaryOp::OP_NOT: op = OpCode::NOT; break;
    case UnaryOp::OP_BITNOT: op = OpCode::BNOT; break;
    case UnaryOp::OP_NEG: op = OpCode::NEG; break;
    default:
        error("unknown unary operator", line);
        return errorReg();
    }

    emit(make_ABC(static_cast<uint8_t>(op), d, src, 0), line);
    freeRegsTo(mark);
    return d;
}

Reg Compiler::compileAssignmentExpr(AssignmentExpr const* e, Reg* dst)
{
    uint32_t line = e->getLine();
    StringRef name = e->getTarget()->getValue();
    VarInfo vi = resolveName(name);

    Reg result;

    switch (vi.kind) {
    case VarInfo::Kind::LOCAL: {
        result = compileExpr(e->getValue(), &vi.index);
        if (dst && *dst != vi.index) {
            emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), *dst, vi.index, 0), line);
            return *dst;
        }
        return vi.index;
    }

    case VarInfo::Kind::UPVALUE: {
        Reg mark = Current_->nextReg;
        result = compileExpr(e->getValue());
        emit(make_ABC(static_cast<uint8_t>(OpCode::SET_UPVALUE), result, vi.index, 0), line);
        Reg d = ensureDst(dst);
        if (d != result)
            emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), d, result, 0), line);
        freeRegsTo(mark);
        return d;
    }

    case VarInfo::Kind::GLOBAL: {
        Reg mark = Current_->nextReg;
        result = compileExpr(e->getValue());
        uint16_t kidx = internString(name, line);
        emit(make_ABx(static_cast<uint8_t>(OpCode::STORE_GLOBAL), result, kidx), line);
        Reg d = ensureDst(dst);
        if (d != result)
            emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), d, result, 0), line);
        freeRegsTo(mark);
        return d;
    }
    }

    return errorReg();
}

Reg Compiler::compileCall(CallExpr const* e, Reg* dst, bool tail_pos)
{
    uint32_t line = e->getLine();

    // The function and all arguments must be in consecutive registers.
    // We push the function first, then each argument in the next registers.
    Reg func_reg = allocReg(); // function always at fixed position
    compileExpr(e->getCallee(), &func_reg);

    auto const& args = e->getArgs();
    for (Expr const* arg : args) {
        Reg arg_reg = allocReg();
        compileExpr(arg, &arg_reg);
    }

    uint8_t argc = static_cast<uint8_t>(args.size());

    if (tail_pos && !Current_->isTopLevel) {
        emit(make_ABC(static_cast<uint8_t>(OpCode::CALL_TAIL), func_reg, argc, 0), line);
        // CALL_TAIL never returns here; free regs is moot but keep bookkeeping clean
        freeRegsTo(func_reg);
        return func_reg;
    }

    // Allocate IC slot — the JIT will cache the call target here
    uint8_t ic = currentChunk()->allocIcSlot();

    // Emit IC_CALL instead of CALL for polymorphic call sites so the JIT
    // knows to profile this site.  Plain CALL is left for known-monomorphic
    // or builtin scenarios.
    emit(make_ABC(static_cast<uint8_t>(OpCode::IC_CALL), func_reg, argc, ic), line);

    Reg d = dst ? *dst : func_reg; // reuse func_reg for result if no dst
    if (d != func_reg)
        emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), d, func_reg, 0), line);

    // Free the arg registers (result stays in func_reg/d)
    freeRegsTo(func_reg + 1);
    return d;
}

Reg Compiler::compileList(ListExpr const* e, Reg* dst)
{
    uint32_t line = e->getLine();
    Reg d = ensureDst(dst);

    auto const& elems = e->getElements();
    // Capacity hint (capped at 255 for the B field)
    uint8_t cap = static_cast<uint8_t>(std::min<size_t>(elems.size(), 255));
    emit(make_ABC(static_cast<uint8_t>(OpCode::NEW_LIST), d, cap, 0), line);

    Reg mark = Current_->nextReg;
    for (Expr const* elem : elems) {
        Reg val = compileExpr(elem);
        emit(make_ABC(static_cast<uint8_t>(OpCode::LIST_APPEND), d, val, 0), line);
        freeRegsTo(mark);
    }
    return d;
}

void Compiler::compileStmt(Stmt const* s)
{
    if (!s || had_error())
        return;

    if (isDead_)
        return; // dead code elimination

    switch (s->getKind()) {
    case Stmt::Kind::BLOCK: compileBlock(dynamic_cast<BlockStmt const*>(s)); break;
    case Stmt::Kind::EXPR: compileExprStmt(dynamic_cast<ExprStmt const*>(s)); break;
    case Stmt::Kind::ASSIGNMENT: compileAssignmentStmt(dynamic_cast<AssignmentStmt const*>(s)); break;
    case Stmt::Kind::IF: compileIf(dynamic_cast<IfStmt const*>(s)); break;
    case Stmt::Kind::WHILE: compileWhile(dynamic_cast<WhileStmt const*>(s)); break;
    case Stmt::Kind::FUNC: compileFunctionDef(dynamic_cast<FunctionDef const*>(s)); break;
    case Stmt::Kind::RETURN: compileReturn(dynamic_cast<ReturnStmt const*>(s)); break;
    case Stmt::Kind::FOR:
        std::cerr << "Not implemented yet." << std::endl;
        break;
    case Stmt::Kind::INVALID:
        error("invalid statement node", s->getLine());
        break;
    }
}

void Compiler::compileBlock(BlockStmt const* s)
{
    beginScope();

    for (Stmt const* child : s->getStatements())
        compileStmt(child);

    endScope(s->getLine());
}

void Compiler::compileExprStmt(ExprStmt const* s)
{
    Reg mark = Current_->nextReg;
    compileExpr(s->getExpr());
    freeRegsTo(mark); // discard temporary result
}

void Compiler::compileAssignmentStmt(AssignmentStmt const* s)
{
    uint32_t line = s->getLine();
    /// NOTE: no support for expressions such as : a[i] = 0
    ast::NameExpr* name_expr = dynamic_cast<ast::NameExpr*>(s->getTarget());
    if (!name_expr)
        /// TODO: report internal error
        return;

    StringRef name = name_expr->getValue();

    if (s->isDeclaration()) {
        // New local variable — allocate a register for it
        Reg reg = allocReg();
        compileExpr(s->getValue(), &reg);
        declareLocal(name, reg, false /*no support for constants*/);
    } else {
        // Assignment to existing variable
        VarInfo vi = resolveName(name);
        switch (vi.kind) {
        case VarInfo::Kind::LOCAL: {
            // Check const
            // Walk locals to find it
            for (auto& loc : Current_->locals) {
                if (loc.name == name && loc.isConst) {
                    error("assignment to const variable '" + name + "'", line);
                    return;
                }
            }
            compileExpr(s->getValue(), &vi.index);
            break;
        }
        case VarInfo::Kind::UPVALUE: {
            Reg mark = Current_->nextReg;
            Reg src = compileExpr(s->getValue());
            emit(make_ABC(static_cast<uint8_t>(OpCode::SET_UPVALUE), src, vi.index, 0), line);
            freeRegsTo(mark);
            break;
        }
        case VarInfo::Kind::GLOBAL: {
            Reg mark = Current_->nextReg;
            Reg src = compileExpr(s->getValue());
            uint16_t kidx = internString(name, line);
            emit(make_ABx(static_cast<uint8_t>(OpCode::STORE_GLOBAL), src, kidx), line);
            freeRegsTo(mark);
            break;
        }
        }
    }
}

void Compiler::compileIf(IfStmt const* s)
{
    if (!s)
        return;

    uint32_t line = s->getLine();

    // --- attempt constant folding on the condition ---
    if (auto cv = constValue(s->getCondition())) {
        // Statically known — compile only the reachable branch (DCE)
        if (cv.has_value()) {
            if (cv->isTruthy())
                compileStmt(s->getThenBlock());
            else if (s->getElseBlock())
                compileStmt(s->getElseBlock());
        }

        return;
    }

    Reg mark = Current_->nextReg;
    Reg cond = compileExpr(s->getCondition());

    uint32_t jump_false = emitJump(OpCode::JUMP_IF_FALSE, cond, line);
    freeRegsTo(mark);

    compileStmt(s->getThenBlock());
    bool then_falls_through = !isDead_;
    clearDead();

    if (s->getElseBlock()) {
        uint32_t jump_over_else = 0;
        if (then_falls_through) {
            jump_over_else = emitJump(OpCode::JUMP, REG_NONE, line);
        }

        patchJump(jump_false);

        compileStmt(s->getElseBlock());
        bool else_falls_through = !isDead_;
        clearDead();

        if (then_falls_through)
            patchJump(jump_over_else);

        // If BOTH branches returned, the join point is still dead
        if (!then_falls_through && !else_falls_through)
            markDead();
    } else {
        patchJump(jump_false);
        clearDead(); // false branch always falls through
    }
}

void Compiler::compileWhile(WhileStmt const* s)
{
    uint32_t line = s->getLine();

    // Constant condition optimizations
    if (auto cv = constValue(s->getCondition())) {
        if (!cv->isTruthy()) {
            // while(false) { } — DCE, emit nothing
            return;
        }

        // while(true) { } — emit as unconditional loop
        uint32_t loop_start = currentOffset();
        pushLoop(loop_start);
        compileStmt(s->getBlock());
        clearDead();

        // LOOP back to loop_start
        int offset = static_cast<int>(loop_start) - static_cast<int>(currentOffset()) - 1;
        emit(make_AsBx(static_cast<uint8_t>(OpCode::LOOP), 0, offset), line);
        // pop loop: break targets jump here
        popLoop(currentOffset(), loop_start, line);
        return;
    }

    uint32_t loop_start = currentOffset();
    pushLoop(loop_start);

    Reg mark = Current_->nextReg;
    Reg cond = compileExpr(s->getCondition());
    uint32_t exit_jump = emitJump(OpCode::JUMP_IF_FALSE, cond, line);
    freeRegsTo(mark);

    compileStmt(s->getBlock());
    clearDead();

    int offset = static_cast<int>(loop_start) - static_cast<int>(currentOffset()) - 1;
    emit(make_AsBx(static_cast<uint8_t>(OpCode::LOOP), 0, offset), line);

    patchJump(exit_jump);
    popLoop(currentOffset(), loop_start, line);
    clearDead();
}

/*
void Compiler::compile_for(ForStmt const* s)
{
    // ASSUMPTION: ForStmt::getVarName() -> std::string
    //             ForStmt::getStart()   -> Expr const*
    //             ForStmt::getLimit()   -> Expr const*
    //             ForStmt::getStep()    -> Expr const* (may be null → step=1)
    //             ForStmt::getBody()    -> Stmt const*
    uint32_t line = s->getLine();

    begin_scope();

    // Allocate four consecutive registers: base, limit, step, index (loop var)
    Reg base = allocReg(); // internal start/index counter
    Reg limit = allocReg();
    Reg step = allocReg();
    Reg idx = allocReg(); // this is the user-visible loop variable

    compileExpr(s->getStart(), base);
    compileExpr(s->getLimit(), limit);
    if (s->getStep())
    compileExpr(s->getStep(), step);
    else
    emit(make_ABx(static_cast<uint8_t>(OpCode::LOAD_INT), step,
    static_cast<uint16_t>(1 + 32767)),
    line);

    // FOR_PREP checks types and initialises; jumps past body if already done.
    uint32_t prep_instr = emit(make_AsBx(static_cast<uint8_t>(OpCode::FOR_PREP), base, 0), line);

    // Declare the loop variable into register idx
    declareLocal(s->getVarName(), idx, **is_const**=true);

    uint32_t loop_start = current_offset();
    pushLoop(loop_start);
    compileStmt(s->getBody());
    clearDead();

    uint32_t step_instr = current_offset();
    int step_offset = static_cast<int>(loop_start) - static_cast<int>(step_instr) - 1;
    emit(make_AsBx(static_cast<uint8_t>(OpCode::FOR_STEP), base,
    static_cast<uint16_t>(step_offset + 32767)),
    line);

    // Patch FOR_PREP to jump past FOR_STEP
    int prep_offset = static_cast<int>(current_offset()) - static_cast<int>(prep_instr) - 1;
    auto prep_op = instr_op(currentChunk()->code[prep_instr]);
    auto prep_A = instr_A(currentChunk()->code[prep_instr]);
    currentChunk()->code[prep_instr] = make_AsBx(prep_op, prep_A, prep_offset);

    popLoop(current_offset(), step_instr, line);
    clearDead();

    endScope(line);
}
*/

void Compiler::compileFunctionDef(FunctionDef const* func)
{
    uint32_t line = func->getLine();
    ast::NameExpr* name_expr = func->getName();
    if (!name_expr) {
        /// TODO: report internal error
        return;
    }

    StringRef name = name_expr->getValue();

    // extract parameters
    std::vector<StringRef> params;
    if (func->hasParameters()) {
        for (Expr const* p : func->getParameters())
            params.push_back(dynamic_cast<NameExpr const*>(p)->getValue());
    }

    Chunk* fn_chunk = compileFunction(name, params, func->getBody(), line);
    if (!fn_chunk)
        return;

    // Find fn_chunk's index in parent's functions list
    auto& fns = currentChunk()->functions;
    auto it = std::find(fns.begin(), fns.end(), fn_chunk);
    assert(it != fns.end());
    uint16_t fn_idx = static_cast<uint16_t>(std::distance(fns.begin(), it));

    // Allocate a register for the closure and declare as local
    Reg dst = allocReg();
    emit(make_ABx(static_cast<uint8_t>(OpCode::CLOSURE), dst, fn_idx), line);

    // Emit upvalue descriptor pseudo-instructions
    // Each upvalue: MOVE A=isLocal B=index C=0
    for (auto& uv : fn_chunk->functions)
        (void)uv; // already encoded by compileFunction into the sub-state

    // Actually emit the upvalue descriptors that compileFunction stored
    // We stored them in fn_chunk->upvalue_descs (see compileFunction)
    // ASSUMPTION: we flush them here via a small side channel (see compileFunction impl)

    declareLocal(name, dst);
}

void Compiler::compileReturn(ReturnStmt const* s)
{
    uint32_t line = s->getLine();

    // this is a stub, because compileReturn should never be called with a nullptr
    // a node ptr should be valid to contain node metadata
    if (!s->hasValue())
        return;

    if (s->getValue()->getKind() == Expr::Kind::LITERAL && dynamic_cast<LiteralExpr const*>(s->getValue())->isNil()) {
        // generate one return nil op code instead of load nil and return
        emit(make_ABC(static_cast<uint8_t>(OpCode::RETURN_NIL), 0, 0, 0), line);
        markDead();
        return;
    }

    // Check for tail-call opportunity
    Expr const* val = s->getValue();

    if (val->getKind() == Expr::Kind::CALL) {
        Reg mark = Current_->nextReg;
        compileCall(static_cast<CallExpr const*>(val), nullptr, /*tail_pos=*/true);
        freeRegsTo(mark);
        markDead();
        return;
    }

    Reg mark = Current_->nextReg;
    Reg src = compileExpr(val);

    emit(make_ABC(static_cast<uint8_t>(OpCode::RETURN), src, 1, 0), line);
    freeRegsTo(mark);
    markDead();
}

Chunk* Compiler::compileFunction(StringRef const& name, std::vector<StringRef> const& params, Stmt const* body, uint32_t line)
{
    // Allocate a new chunk for this function, owned by the parent chunk.
    Chunk* fn_chunk = runtime_allocator.allocateObject<Chunk>();
    fn_chunk->name = name;
    fn_chunk->arity = static_cast<int>(params.size());
    currentChunk()->functions.push_back(fn_chunk);

    // Push a new compiler state
    CompilerState state;
    state.chunk = fn_chunk;
    state.funcName = name;
    state.enclosing = Current_;
    Current_ = &state;
    isDead_ = false;

    beginScope();

    // Parameters occupy the first registers
    for (StringRef const& p : params) {
        Reg reg = allocReg();
        declareLocal(p, reg);
    }

    // Compile body
    compileStmt(body);

    if (!isDead_)
        emit(make_ABC(static_cast<uint8_t>(OpCode::RETURN_NIL), 0, 0, 0), line);

    endScope(line);

    fn_chunk->local_count = state.maxReg;
    fn_chunk->upvalue_count = static_cast<int>(state.upvalues.size());

    // Emit upvalue descriptors into the parent chunk as MOVE pseudo-instructions
    // immediately after the CLOSURE instruction (emitted by compileFunctionDef).
    // The VM reads these to know how to build the closure's upvalue array.
    CompilerState* parent = state.enclosing;
    Current_ = parent;
    isDead_ = false;

    for (auto& uv : state.upvalues)
        // MOVE: A=isLocal, B=index, C=0 — purely a descriptor, not executed
        emit(make_ABC(static_cast<uint8_t>(OpCode::MOVE), static_cast<uint8_t>(uv.isLocal ? 1 : 0), uv.index, 0), line);

    return fn_chunk;
}

std::optional<Value> Compiler::constValue(Expr const* e)
{
    if (!e)
        return std::nullopt;
    if (e->getKind() != Expr::Kind::LITERAL)
        return std::nullopt;

    LiteralExpr const* lit = static_cast<LiteralExpr const*>(e);
    if (lit->isBoolean())
        return Value::boolean(lit->getBool());
    /*
    if (lit->isNil())
        return Value::nil();
    */
    if (lit->isInteger())
        return Value::integer(lit->getInt());
    if (lit->isDecimal())
        return Value::real(lit->getFloat());

    // Strings are not folded here (they are heap objects)
    return std::nullopt;
}

std::optional<Value> Compiler::tryFoldUnary(UnaryExpr const* e)
{
    std::optional<Value> cv = constValue(e->getOperand());
    if (!cv)
        return std::nullopt;

    switch (e->getOperator()) {
    case UnaryOp::OP_NEG:
        if (cv->isInt())
            return Value::integer(-cv->asInt());
        if (cv->isDouble())
            return Value::real(-cv->asDouble());

        return std::nullopt;
    case UnaryOp::OP_NOT:
        return Value::boolean(!cv->isTruthy());
    case UnaryOp::OP_BITNOT:
        if (cv->isInt())
            return Value::integer(~cv->asInt());

        return std::nullopt;
    default:
        return std::nullopt;
    }
}

std::optional<Value> Compiler::tryFoldBinary(BinaryExpr const* e)
{
    if (!e)
        return std::nullopt;

    Expr* LE = e->getLeft();
    Expr* RE = e->getRight();

    if (!LE || !RE)
        return std::nullopt;

    if (LE->getKind() == Expr::Kind::LITERAL && RE->getKind() == Expr::Kind::LITERAL)
        // simple non nested expression
        return _tryFoldBinary(e);

    std::optional<Value> L, R;

    // fold left
    if (LE->getKind() == Expr::Kind::BINARY)
        L = tryFoldBinary(static_cast<BinaryExpr const*>(LE));
    else if (LE->getKind() == Expr::Kind::UNARY)
        L = tryFoldUnary(static_cast<UnaryExpr const*>(LE));

    // fold right
    if (RE->getKind() == Expr::Kind::BINARY)
        R = tryFoldBinary(static_cast<BinaryExpr const*>(RE));
    else if (RE->getKind() == Expr::Kind::UNARY)
        R = tryFoldUnary(static_cast<UnaryExpr const*>(RE));

    if (!R && !L)
        return std::nullopt;

    BinaryExpr* ce = e->clone();

    auto makeLiteralFromVal = [](Value const v) {
        if (v.isInt())
            return makeLiteralInt(v.asInt());
        else if (v.isDouble())
            return makeLiteralFloat(v.asDouble());
        else if (v.isBool())
            return makeLiteralBool(v.asBool());

        return makeLiteralNil();
    };

    if (L)
        ce->setLeft(makeLiteralFromVal(*L));
    if (R)
        ce->setRight(makeLiteralFromVal(*R));

    return _tryFoldBinary(ce);
}

std::optional<Value> Compiler::_tryFoldBinary(BinaryExpr const* e)
{
    std::optional<Value> L = constValue(e->getLeft());
    std::optional<Value> R = constValue(e->getRight());

    if (!L || !R)
        return std::nullopt;

    BinaryOp op = e->getOperator();

    // Equality works on all types
    if (op == BinaryOp::OP_EQ)
        return Value::boolean(*L == *R);
    if (op == BinaryOp::OP_NEQ)
        return Value::boolean(*L != *R);
    // Arithmetic requires numeric operands
    if (!L->isNumber() || !R->isNumber())
        return std::nullopt;

    bool both_int = L->isInt() && R->isInt();

    int64_t li = L->isInt() ? L->asInt() : static_cast<int64_t>(L->asDouble());
    int64_t ri = R->isInt() ? R->asInt() : static_cast<int64_t>(R->asDouble());

    double ld = L->asNumberDouble();
    double rd = R->asNumberDouble();

    switch (op) {
    case BinaryOp::OP_ADD: return both_int ? Value::integer(li + ri) : Value::real(ld + rd);
    case BinaryOp::OP_SUB: return both_int ? Value::integer(li - ri) : Value::real(ld - rd);
    case BinaryOp::OP_MUL: return both_int ? Value::integer(li * ri) : Value::real(ld * rd);
    case BinaryOp::OP_DIV:
        if (rd == 0.0)
            return std::nullopt; // don't fold division by zero

        return Value::real(ld / rd);
    /*
    case BinaryOp::OP_IDIV:
        if (ri == 0)
            return std::nullopt;
        return Value::integer(std::floor(ld / rd) >= 0
            ? li / ri
            : (li - (ri - 1)) / ri); // floor division
    */
    case BinaryOp::OP_MOD:
        if (ri == 0)
            return std::nullopt;

        return both_int ? Value::integer(((li % ri) + ri) % ri) : Value::real(std::fmod(ld, rd));
    case BinaryOp::OP_POW: return Value::real(std::pow(ld, rd));
    case BinaryOp::OP_LT: return Value::boolean(ld < rd);
    case BinaryOp::OP_GT: return Value::boolean(ld > rd);
    case BinaryOp::OP_LTE: return Value::boolean(ld <= rd);
    case BinaryOp::OP_GTE: return Value::boolean(ld >= rd);
    case BinaryOp::OP_BITAND: return both_int ? Value::integer(li & ri) : std::optional<Value> { };
    case BinaryOp::OP_BITOR: return both_int ? Value::integer(li | ri) : std::optional<Value> { };
    case BinaryOp::OP_BITXOR: return both_int ? Value::integer(li ^ ri) : std::optional<Value> { };
    case BinaryOp::OP_LSHIFT:
        if (!both_int || ri < 0 || ri >= 64)
            return std::nullopt;

        return Value::integer(li << ri);
    case BinaryOp::OP_RSHIFT:
        if (!both_int || ri < 0 || ri >= 64)
            return std::nullopt;

        return Value::integer(li >> ri);
    default:
        return std::nullopt;
    }
}

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
    Current_->loopStack.push_back({ { }, { }, loop_start });
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

Reg Compiler::allocReg()
{
    Reg r = Current_->allocReg();
    if (r >= MAX_REGISTERS) {
        error("too many registers in function '" + Current_->funcName + "'", 0);
        return 0;
    }

    return r;
}

void Compiler::freeReg() { Current_->freeReg(); }
void Compiler::freeRegsTo(Reg mark) { Current_->freeRegsTo(mark); }
Reg Compiler::ensureDst(Reg* dst) { return dst ? *dst : allocReg(); }
Reg Compiler::errorReg() { return 0; }

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

StringRef CompileError::format() const
{
    std::ostringstream ss;
    ss << "[line " << line << "] in '" << context << "': " << message;
    return ss.str().data();
}

Chunk* Compiler::compile(std::vector<ast::Stmt*> const& stmts)
{
    if (stmts.empty() || !stmts[0]) {
        error("null AST root", 0);
        return nullptr;
    }

    // Build the top-level chunk
    Chunk* top_chunk = runtime_allocator.allocateObject<Chunk>();
    top_chunk->name = "<main>";

    CompilerState state;
    state.chunk = top_chunk;
    state.funcName = "<main>";
    state.isTopLevel = true;
    state.enclosing = nullptr;
    Current_ = &state;

    for (ast::Stmt const* s : stmts)
        compileStmt(s);

    // Emit implicit RETURN_NIL if the function didn't already return
    if (!isDead_)
        emit(make_ABC(static_cast<uint8_t>(OpCode::RETURN_NIL), 0, 0, 0), stmts.back()->getLine());

    top_chunk->local_count = state.maxReg;
    top_chunk->upvalue_count = static_cast<int>(state.upvalues.size());

    Current_ = nullptr;

    if (had_error())
        return nullptr;

    return top_chunk;
}

// Utilities

void Compiler::error(StringRef const& msg, uint32_t line)
{
    StringRef ctx = Current_ ? Current_->funcName : "<unknown>";
    Errors_.push_back({ msg, line, ctx });
}

} // namespace runtime
} // namespace mylang
