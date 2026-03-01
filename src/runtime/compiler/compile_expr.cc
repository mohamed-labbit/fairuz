#include "../../../include/ast/ast.hpp"
#include "../../../include/runtime/compiler/compiler.hpp"
#include "../../../include/util.hpp"

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

}
}
