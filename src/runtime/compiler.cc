#include "../../include/runtime/compiler.hpp"
#include <unordered_map>

namespace mylang::runtime {

static SourceLocation srcLoc(Stmt const* s) { return SourceLocation { s->getLine(), s->getColumn(), 0 }; }
static SourceLocation srcLoc(Expr const* e) { return SourceLocation { e->getLine(), e->getColumn(), 0 }; }

Chunk* Compiler::compile(Array<Stmt*> const& stmts)
{
    if (stmts.empty() || !stmts[0]) {
        diagnostic::emit("null AST root", diagnostic::Severity::FATAL);
        return nullptr;
    }

    // Build the top-level chunk
    auto top_chunk = makeChunk();
    top_chunk->name = "<main>";

    CompilerState state;
    state.chunk = top_chunk;
    state.funcName = "<main>";
    state.isTopLevel = true;
    state.enclosing = nullptr;
    Current_ = &state;

    for (auto const& s : stmts)
        compileStmt(s);

    SourceLocation cur_loc = { stmts.back()->getLine(), stmts.back()->getColumn(), 0 };

    // Emit implicit RETURN_NIL only when control flow is still live.
    if (!state.isDead_) {
        uint32_t line = stmts[0] ? stmts[0]->getLine() : 0;
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), cur_loc);
    }

    top_chunk->localCount = state.maxReg;
    top_chunk->upvalueCount = static_cast<int>(state.upvalues.size());
    Current_ = nullptr;

    return top_chunk;
}

void Compiler::compileStmt(Stmt const* s)
{
    if (!s || Current_->isDead_)
        return;

    switch (s->getKind()) {
    case Stmt::Kind::BLOCK:
        compileBlock(static_cast<BlockStmt const*>(s));
        break;
    case Stmt::Kind::EXPR:
        compileExprStmt(static_cast<ExprStmt const*>(s));
        break;
    case Stmt::Kind::ASSIGNMENT:
        compileAssignmentStmt(static_cast<AssignmentStmt const*>(s));
        break;
    case Stmt::Kind::IF:
        compileIf(static_cast<IfStmt const*>(s));
        break;
    case Stmt::Kind::WHILE:
        compileWhile(static_cast<WhileStmt const*>(s));
        break;
    case Stmt::Kind::FUNC:
        compileFunctionDef(static_cast<FunctionDef const*>(s));
        break;
    case Stmt::Kind::RETURN:
        compileReturn(static_cast<ReturnStmt const*>(s));
        break;
    case Stmt::Kind::FOR:
        diagnostic::emit("Not implemented yet.");
        break;
    case Stmt::Kind::INVALID:
        diagnostic::emit("Invalid statement node.");
        break;
    }
}

void Compiler::compileBlock(BlockStmt const* s)
{
    beginScope();

    for (Stmt const* child : s->getStatements())
        compileStmt(child);

    SourceLocation loc = { 1, 1, 0 };
    if (!s->getStatements().empty()) {
        Stmt* n = s->getStatements().back();
        loc = { n->getLine(), n->getColumn(), 0 };
    }
    endScope(loc);
}

void Compiler::compileExprStmt(ExprStmt const* s)
{
    RegMark mark(Current_);
    compileExpr(s->getExpr());
}

void Compiler::compileAssignmentStmt(AssignmentStmt const* s)
{
    SourceLocation cur_loc = { s->getLine(), s->getColumn(), 0 };
    NameExpr const* name = dynamic_cast<NameExpr const*>(s->getTarget());
    assert(name);

    if (s->isDeclaration()) {
        uint8_t reg = allocRegister();
        compileExpr(s->getValue(), &reg);
        declareLocal(name->getValue(), reg);
    } else {
        VarInfo vi = resolveName(name->getValue());

        switch (vi.kind) {
        case VarInfo::Kind::LOCAL:
            compileExpr(s->getValue(), &vi.index);
            break;
        case VarInfo::Kind::UPVALUE: {
            RegMark mark(Current_);
            uint8_t src = compileExpr(s->getValue());
            emit(make_ABC(OpCode::SET_UPVALUE, src, vi.index, 0), cur_loc);
        } break;
        case VarInfo::Kind::GLOBAL: {
            RegMark mark(Current_);
            uint8_t src = compileExpr(s->getValue());
            uint16_t kidx = internString(name->getValue());
            emit(make_ABx((OpCode::STORE_GLOBAL), src, kidx), cur_loc);
        } break;
        }
    }
}

void Compiler::compileIf(IfStmt const* s)
{
    if (!s)
        return;

    SourceLocation cur_loc = { s->getLine(), s->getColumn(), 0 };
    bool const incoming_dead = Current_->isDead_;

    auto folded_cond = tryFoldExpr(s->getCondition());

    if (folded_cond) {
        if (isTruthy(*folded_cond)) {
            compileStmt(s->getThenBlock());
            return;
        }

        if (BlockStmt const* else_block = s->getElseBlock()) {
            compileStmt(else_block);
            return;
        }

        Current_->isDead_ = incoming_dead;
        return;
    }

    RegMark mark(Current_);
    uint8_t cond = compileExpr(s->getCondition());
    uint32_t jump_false = emitJump(OpCode::JUMP_IF_FALSE, cond, cur_loc);
    compileStmt(s->getThenBlock());

    if (BlockStmt const* else_block = s->getElseBlock()) {
        uint32_t jump_end = emitJump(OpCode::JUMP, 0, cur_loc);
        patchJump(jump_false);
        compileStmt(else_block);
        patchJump(jump_end);
    } else
        patchJump(jump_false);

    Current_->isDead_ = incoming_dead;
}

void Compiler::compileWhile(WhileStmt const* s)
{
    if (!s)
        return;

    SourceLocation const cur_loc = { s->getLine(), s->getColumn(), 0 };
    bool const incoming_dead = Current_->isDead_;

    auto folded_cond = tryFoldExpr(s->getCondition());

    if (folded_cond) {
        if (!isTruthy(*folded_cond)) {
            Current_->isDead_ = incoming_dead;
            return;
        }

        uint32_t loop_start_folded = currentOffset();
        pushLoop(loop_start_folded);
        compileStmt(s->getBlock());
        uint32_t offset_folded = loop_start_folded - currentOffset() - 1;
        emit(make_AsBx((OpCode::LOOP), 0, offset_folded), cur_loc);
        popLoop(currentOffset(), loop_start_folded, cur_loc.line);
        Current_->isDead_ = incoming_dead;
        return;
    }

    uint32_t loop_start = currentOffset();
    pushLoop(loop_start);
    RegMark mark(Current_);
    uint8_t cond = compileExpr(s->getCondition());
    uint32_t exit_jump = emitJump(OpCode::JUMP_IF_FALSE, cond, cur_loc);
    compileStmt(s->getBlock());
    uint32_t offset = loop_start - currentOffset() - 1;
    emit(make_AsBx((OpCode::LOOP), 0, offset), cur_loc);
    patchJump(exit_jump);
    popLoop(currentOffset(), loop_start, cur_loc.line);
    Current_->isDead_ = incoming_dead;
}

void Compiler::compileFunctionDef(FunctionDef const* f)
{
    SourceLocation cur_loc = { f->getLine(), f->getColumn(), 0 };

    NameExpr* name = f->getName();
    if (!name) {
        diagnostic::emit("Function name is null", diagnostic::Severity::FATAL);
        return;
    }

    std::vector<StringRef> params;
    if (f->hasParameters())
        for (Expr const* e : f->getParameters())
            params.push_back(dynamic_cast<NameExpr const*>(e)->getValue());

    Chunk* fn_chunk = makeChunk();
    fn_chunk->name = name->getValue();
    fn_chunk->arity = static_cast<int>(params.size());
    currentChunk()->functions.push(fn_chunk);

    // find the index in the parent chunk before switching state
    auto& funcs = currentChunk()->functions;
    auto it = std::find(funcs.begin(), funcs.end(), fn_chunk);
    assert(it != funcs.end());
    uint16_t fn_idx = static_cast<uint16_t>(std::distance(funcs.begin(), it));

    CompilerState fn_state;
    fn_state.chunk = fn_chunk;
    fn_state.funcName = name->getValue();
    fn_state.enclosing = Current_;
    Current_ = &fn_state;

    beginScope();

    for (StringRef const& p : params) {
        uint8_t reg = allocRegister();
        declareLocal(p, reg);
    }

    compileStmt(f->getBody());
    if (!fn_state.isDead_)
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), cur_loc);

    endScope(cur_loc);

    fn_chunk->localCount = fn_state.maxReg;
    fn_chunk->upvalueCount = static_cast<unsigned int>(fn_state.upvalues.size());

    // restore parent before emitting anything into parent chunk
    Current_ = fn_state.enclosing;

    uint8_t dst = allocRegister();
    emit(make_ABx((OpCode::CLOSURE), dst, fn_idx), cur_loc);

    for (auto& uv : fn_state.upvalues)
        emit(make_ABC(OpCode::MOVE, (uv.isLocal ? 1 : 0), uv.index, 0), cur_loc);
        
    if (Current_->isTopLevel) {
        uint16_t kidx = internString(name->getValue());
        emit(make_ABx((OpCode::STORE_GLOBAL), dst, kidx), cur_loc);
    } else {
        declareLocal(name->getValue(), dst);
    }
}

void Compiler::compileReturn(ReturnStmt const* s)
{
    SourceLocation cur_loc = { s->getLine(), s->getColumn(), 0 };

    if (!s->hasValue() || (s->getValue()->getKind() == Expr::Kind::LITERAL && static_cast<LiteralExpr const*>(s->getValue())->isNil())) {
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), cur_loc);
        Current_->isDead_ = true;
        return;
    }

    Expr const* value = s->getValue();

    if (value->getKind() == Expr::Kind::CALL) {
        RegMark mark(Current_);
        compileCall(static_cast<CallExpr const*>(value), nullptr, true);
        Current_->isDead_ = true;
        return;
    }

    RegMark mark(Current_);
    uint8_t src = compileExpr(value);
    emit(make_ABC(OpCode::RETURN, src, 1, 0), cur_loc);
    Current_->isDead_ = true;
}

uint8_t Compiler::compileExpr(Expr const* e, uint8_t* dst)
{
    if (!e)
        return errorReg();

    switch (e->getKind()) {
    case Expr::Kind::LITERAL:
        return compileLiteral(static_cast<LiteralExpr const*>(e), dst);
    case Expr::Kind::NAME:
        return compileName(static_cast<NameExpr const*>(e), dst);
    case Expr::Kind::UNARY:
        return compileUnary(static_cast<UnaryExpr const*>(e), dst);
    case Expr::Kind::BINARY:
        return compileBinary(static_cast<BinaryExpr const*>(e), dst);
    case Expr::Kind::ASSIGNMENT:
        return compileAssignmentExpr(static_cast<AssignmentExpr const*>(e), dst);
    case Expr::Kind::CALL:
        return compileCall(static_cast<CallExpr const*>(e), dst);
    case Expr::Kind::LIST:
        return compileList(static_cast<ListExpr const*>(e), dst);
    case Expr::Kind::INVALID:
        diagnostic::emit("Invalid expression node");
        break;
    }

    return errorReg();
}

uint8_t Compiler::compileLiteral(LiteralExpr const* e, uint8_t* dst)
{
    SourceLocation cur_loc = { e->getLine(), e->getColumn(), 0 };
    uint8_t dst_reg = ensureReg(dst);

    Value v;

    if (e->isInteger())
        v = makeInteger(e->getInt());
    else if (e->isDecimal())
        v = makeReal(e->getFloat());
    else if (e->isBoolean())
        v = makeBool(e->getBool());
    else if (e->isNil())
        v = NIL_VAL;
    else if (e->isString()) {
        uint16_t kidx = internString(e->getStr());
        emit(make_ABx((OpCode::LOAD_CONST), dst_reg, kidx), cur_loc);
        return dst_reg;
    } else {
        diagnostic::emit("Unknown literal type");
        return errorReg();
    }

    emitLoadValue(dst_reg, v, cur_loc);
    return dst_reg;
}

uint8_t Compiler::compileName(NameExpr const* e, uint8_t* dst)
{
    SourceLocation cur_loc = { e->getLine(), e->getColumn(), 0 };
    VarInfo vi = resolveName(e->getValue());

    switch (vi.kind) {
    case VarInfo::Kind::LOCAL:
        if (dst && *dst != vi.index)
            emit(make_ABC((OpCode::MOVE), *dst, vi.index, 0), cur_loc);

        return dst ? *dst : vi.index;
    case VarInfo::Kind::UPVALUE: {
        uint8_t dst_reg = ensureReg(dst);
        emit(make_ABC((OpCode::GET_UPVALUE), dst_reg, vi.index, 0), cur_loc);
        return dst_reg;
    }
    case VarInfo::Kind::GLOBAL: {
        uint8_t dst_reg = ensureReg(dst);
        uint16_t kidx = internString(e->getValue());
        emit(make_ABx((OpCode::LOAD_GLOBAL), dst_reg, kidx), cur_loc);
        return dst_reg;
    }
    }

    return errorReg();
}

uint8_t Compiler::compileUnary(UnaryExpr const* e, uint8_t* dst)
{
    SourceLocation cur_loc = { e->getLine(), e->getColumn(), 0 };

    if (std::optional<Value> folded = tryFoldUnary(e)) {
        uint8_t dst_reg = ensureReg(dst);
        emitLoadValue(dst_reg, *folded, cur_loc);
        return dst_reg;
    }

    RegMark mark(Current_);
    uint8_t src = compileExpr(e->getOperand());
    uint8_t dst_reg = ensureReg(dst);

    OpCode op;

    switch (e->getOperator()) {
    case UnaryOp::OP_NOT:
        op = OpCode::OP_NOT;
        break;
    case UnaryOp::OP_BITNOT:
        op = OpCode::OP_BITNOT;
        break;
    case UnaryOp::OP_NEG:
        op = OpCode::OP_NEG;
        break;
    // case UnaryOp::OP_PLUS: op = OpCode::OP_PLUS; break;
    default:
        diagnostic::emit("Unknown unary operator", diagnostic::Severity::FATAL);
    }

    emit(make_ABC((op), dst_reg, src, 0), cur_loc);
    return dst_reg;
}

uint8_t Compiler::compileBinary(BinaryExpr const* e, uint8_t* dst)
{
    SourceLocation cur_loc = { e->getLine(), e->getColumn(), 0 };

    if (std::optional<Value> folded = tryFoldBinary(e)) {
        uint8_t dst_reg = ensureReg(dst);
        emitLoadValue(dst_reg, *folded, cur_loc);
        return dst_reg;
    }

    BinaryOp const op = e->getOperator();

    if (op == BinaryOp::OP_AND) {
        uint8_t dst_reg = ensureReg(dst);
        RegMark mark(Current_);
        uint8_t left = compileExpr(e->getLeft(), &dst_reg);

        if (left != dst_reg)
            emit(make_ABC((OpCode::MOVE), dst_reg, left, 0), cur_loc);

        uint32_t skip = emitJump(OpCode::JUMP_IF_FALSE, dst_reg, cur_loc);
        uint8_t R = compileExpr(e->getRight(), &dst_reg);

        if (R != dst_reg)
            emit(make_ABC((OpCode::MOVE), dst_reg, R, 0), cur_loc);

        patchJump(skip);
        return dst_reg;
    }

    if (op == BinaryOp::OP_OR) {
        uint8_t dst_reg = ensureReg(dst);
        RegMark mark(Current_);
        uint8_t left = compileExpr(e->getLeft(), &dst_reg);

        if (left != dst_reg)
            emit(make_ABC((OpCode::MOVE), dst_reg, left, 0), cur_loc);

        uint32_t skip = emitJump(OpCode::JUMP_IF_TRUE, dst_reg, cur_loc);
        uint8_t R = compileExpr(e->getRight(), &dst_reg);

        if (R != dst_reg)
            emit(make_ABC((OpCode::MOVE), dst_reg, R, 0), cur_loc);

        patchJump(skip);
        return dst_reg;
    }

    RegMark mark(Current_);
    uint8_t L = compileExpr(e->getLeft());
    uint8_t R = compileExpr(e->getRight());
    uint8_t dst_reg = ensureReg(dst);
    uint8_t ic = currentChunk()->allocIcSlot();

    OpCode bc_op;

    switch (op) {
    case BinaryOp::OP_ADD: //
        bc_op = OpCode::OP_ADD;
        break;
    case BinaryOp::OP_SUB: //
        bc_op = OpCode::OP_SUB;
        break;
    case BinaryOp::OP_MUL: //
        bc_op = OpCode::OP_MUL;
        break;
    case BinaryOp::OP_DIV:
        bc_op = OpCode::OP_DIV;
        break;
    case BinaryOp::OP_MOD:
        bc_op = OpCode::OP_MOD;
        break;
    case BinaryOp::OP_EQ:
        bc_op = OpCode::OP_EQ;
        break;
    case BinaryOp::OP_NEQ:
        bc_op = OpCode::OP_NEQ;
        break;
    case BinaryOp::OP_LT:
        bc_op = OpCode::OP_LT;
        break;
    case BinaryOp::OP_LTE:
        bc_op = OpCode::OP_LTE;
        break;
    case BinaryOp::OP_BITAND:
        bc_op = OpCode::OP_BITAND;
        break;
    case BinaryOp::OP_BITOR:
        bc_op = OpCode::OP_BITOR;
        break;
    case BinaryOp::OP_BITXOR:
        bc_op = OpCode::OP_BITXOR;
        break;
    case BinaryOp::OP_LSHIFT:
        bc_op = OpCode::OP_LSHIFT;
        break;
    case BinaryOp::OP_RSHIFT:
        bc_op = OpCode::OP_RSHIFT;
        break;
    case BinaryOp::OP_GT:
        std::swap(L, R);
        bc_op = OpCode::OP_LT;
        break;
    case BinaryOp::OP_GTE:
        std::swap(L, R);
        bc_op = OpCode::OP_LTE;
        break;
    default:
        diagnostic::emit("Unknown binary operator", diagnostic::Severity::FATAL);
    }

    emit(make_ABC((bc_op), dst_reg, L, R), cur_loc);
    emit(make_ABC((OpCode::NOP), ic, 0, 0), cur_loc);

    return dst_reg;
}

uint8_t Compiler::compileAssignmentExpr(AssignmentExpr const* e, uint8_t* dst)
{
    SourceLocation cur_loc = { e->getLine(), e->getColumn(), 0 };
    StringRef name = e->getTarget()->getValue();
    VarInfo vi = resolveName(name);
    uint8_t result;

    switch (vi.kind) {
    case VarInfo::Kind::LOCAL: {
        result = compileExpr(e->getValue(), &vi.index);
        if (dst && *dst != vi.index) {
            emit(make_ABC((OpCode::MOVE), *dst, vi.index, 0), cur_loc);
            return *dst;
        }
        return vi.index;
    }

    case VarInfo::Kind::UPVALUE: {
        RegMark mark(Current_);
        result = compileExpr(e->getValue());
        emit(make_ABC((OpCode::SET_UPVALUE), result, vi.index, 0), cur_loc);
        uint8_t dst_reg = ensureReg(dst);
        if (dst_reg != result)
            emit(make_ABC((OpCode::MOVE), dst_reg, result, 0), cur_loc);
        return dst_reg;
    }

    case VarInfo::Kind::GLOBAL: {
        RegMark mark(Current_);
        result = compileExpr(e->getValue());
        uint16_t kidx = internString(name);
        emit(make_ABx((OpCode::STORE_GLOBAL), result, kidx), cur_loc);
        uint8_t dst_reg = ensureReg(dst);
        if (dst_reg != result)
            emit(make_ABC((OpCode::MOVE), dst_reg, result, 0), cur_loc);
        return dst_reg;
    }
    }

    return errorReg();
}

uint8_t Compiler::compileCall(CallExpr const* e, uint8_t* dst, bool tail)
{
    SourceLocation cur_loc = { e->getLine(), e->getColumn(), 0 };
    uint8_t fn_reg = allocRegister();
    compileExpr(e->getCallee(), &fn_reg);

    auto const& args = e->getArgs();
    for (Expr const* arg : args) {
        uint8_t arg_reg = allocRegister();
        compileExpr(arg, &arg_reg);
    }

    // this essentialy caps argc at 0xFF, if u got more args you should see a doctor
    uint8_t argc = (args.size());

    if (tail && !Current_->isTopLevel) {
        emit(make_ABC((OpCode::CALL_TAIL), fn_reg, argc, 0), cur_loc);
        Current_->freeRegsTo(fn_reg);
        return fn_reg;
    }

    uint8_t ic = currentChunk()->allocIcSlot();
    emit(make_ABC((OpCode::IC_CALL), fn_reg, argc, ic), cur_loc);
    uint8_t dst_reg = dst ? *dst : fn_reg;
    if (dst_reg != fn_reg)
        emit(make_ABC((OpCode::MOVE), dst_reg, fn_reg, 0), cur_loc);

    Current_->freeRegsTo(fn_reg + 1);
    return dst_reg;
}

uint8_t Compiler::compileList(ListExpr const* e, uint8_t* dst)
{
    SourceLocation cur_loc = { e->getLine(), e->getColumn(), 0 };
    uint8_t dst_reg = ensureReg(dst);
    auto const& elems = e->getElements();
    uint8_t cap = (std::min<size_t>(elems.size(), 0xFF));
    emit(make_ABC((OpCode::LIST_NEW), dst_reg, cap, 0), cur_loc);

    RegMark mark(Current_);
    for (Expr const* elem : elems) {
        uint8_t val = compileExpr(elem);
        emit(make_ABC((OpCode::LIST_APPEND), dst_reg, val, 0), cur_loc);
    }

    return dst_reg;
}

uint8_t Compiler::errorReg() const { return 0; }

uint8_t Compiler::ensureReg(uint8_t const* reg) { return reg ? *reg : allocRegister(); }

uint8_t Compiler::allocRegister()
{
    uint8_t reg = Current_->allocRegister();
    if (reg >= MAX_REGS) {
        diagnostic::emit("Too many registers in function '" + std::string(Current_->funcName.data()) + "'");
        return 0;
    }
    return reg;
}

void Compiler::declareLocal(StringRef const& name, uint8_t const reg)
{
    Current_->locals.push({ name, Current_->scopeDepth, reg, false });
}

// searches from inner scope all the way to globals
typename Compiler::VarInfo Compiler::resolveName(StringRef const& name)
{
    auto const& locals = Current_->locals;
    for (auto i = static_cast<int>(locals.size()) - 1; i >= 0; --i)
        if (locals[i].name == name)
            return { VarInfo::Kind::LOCAL, locals[i].reg };

    int uv = resolveUpvalue(Current_, name);
    if (uv >= 0)
        return { VarInfo::Kind::UPVALUE, static_cast<uint8_t>(uv) };

    return { VarInfo::Kind::GLOBAL, 0 };
}

int Compiler::resolveUpvalue(CompilerState* state, StringRef const& name)
{
    if (!state->enclosing)
        return -1;

    // Look for a local in the immediately enclosing function
    auto& enc_locals = state->enclosing->locals;
    for (auto i = static_cast<int>(enc_locals.size()) - 1; i >= 0; --i) {
        if (enc_locals[i].name == name) {
            enc_locals[i].isCaptured = true;
            return addUpvalue(state, true, enc_locals[i].reg);
        }
    }

    int up = resolveUpvalue(state->enclosing, name);
    if (up >= 0)
        return addUpvalue(state, false, (up));

    return -1;
}

int Compiler::addUpvalue(CompilerState* state, bool const is_local, uint8_t const index)
{
    if (!state)
        return -1;

    for (int i = 0; i < static_cast<int>(state->upvalues.size()); ++i) {
        auto const& uv = state->upvalues[i];
        if (uv.isLocal == is_local && uv.index == index)
            return i;
    }

    state->upvalues.push({ is_local, index });
    return static_cast<int>(state->upvalues.size() - 1);
}

uint32_t Compiler::emit(uint32_t const instr, SourceLocation loc)
{
    return currentChunk()->emit(instr, loc);
}

uint32_t Compiler::emitJump(OpCode const op, uint8_t const cond, SourceLocation loc)
{
    return emit(make_AsBx((op), cond, 0), loc);
}

void Compiler::patchJump(uint32_t const idx)
{
    if (!currentChunk()->patchJump(idx))
        diagnostic::emit("Jump offset overflow", diagnostic::Severity::FATAL);
}

void Compiler::pushLoop(uint32_t const loop_start) { Current_->loopStack.push({ { }, { }, loop_start }); }

void Compiler::popLoop(uint32_t const loop_exit, uint32_t const continue_target, uint32_t const line)
{
    assert(!Current_->loopStack.empty());
    auto& ctx = Current_->loopStack.back();

    for (auto idx : ctx.breakPatches)
        patchJumpTo(idx, loop_exit);

    for (auto idx : ctx.continuePatches)
        patchJumpTo(idx, continue_target);

    Current_->loopStack.pop();
}

void Compiler::patchJumpTo(uint32_t const instr_idx, uint32_t const target)
{
    int32_t offset = static_cast<int32_t>(target) - static_cast<int32_t>(instr_idx) - 1;

    if (offset > JUMP_OFFSET || offset < -JUMP_OFFSET)
        diagnostic::emit("Jump offset overflow in loop", diagnostic::Severity::FATAL);

    OpCode op = instr_op(currentChunk()->code[instr_idx]);
    uint8_t A = instr_A(currentChunk()->code[instr_idx]);
    currentChunk()->code[instr_idx] = make_AsBx(op, A, offset);
}

void Compiler::emitLoadValue(uint8_t const dst, Value const v, SourceLocation loc)
{
    if (v == NIL_VAL)
        emit(make_ABC((OpCode::LOAD_NIL), dst, dst, 1), loc);
    else if (IS_BOOL(v)) {
        OpCode op = asBool(v) ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE;
        emit(make_ABC((op), dst, 0, 0), loc);
    } else if (IS_INTEGER(v)) {
        int64_t int_v = asInteger(v);
        if (int_v >= -JUMP_OFFSET && int_v <= JUMP_OFFSET)
            emit(make_ABx((OpCode::LOAD_INT), dst, static_cast<uint16_t>(int_v + JUMP_OFFSET)), loc);
        else {
            uint16_t kidx = currentChunk()->addConstant(v);
            emit(make_ABx(OpCode::LOAD_CONST, dst, kidx), loc);
        }
    } else if (isDouble(v)) {
        uint16_t kidx = currentChunk()->addConstant(v);
        emit(make_ABx((OpCode::LOAD_CONST), dst, kidx), loc);
    }
}

Chunk* Compiler::currentChunk() const { return Current_->chunk; }

uint32_t Compiler::currentOffset() const { return currentChunk()->code.size(); }

void Compiler::beginScope() { ++Current_->scopeDepth; }

void Compiler::endScope(SourceLocation loc)
{
    unsigned int const depth = --Current_->scopeDepth;
    auto& locals = Current_->locals;
    size_t pop_from = locals.size();

    while (pop_from > 0 && locals[pop_from - 1].depth > depth)
        --pop_from;

    bool any = false; // for upvalues
    for (size_t i = pop_from; i < locals.size(); ++i) {
        if (locals[i].isCaptured) {
            any = true;
            break;
        }
    }

    if (any && pop_from < locals.size()) {
        uint8_t first = locals[pop_from].reg;
        emit(make_ABC((OpCode::CLOSE_UPVALUE), first, 0, 0), loc);
    }

    if (pop_from < locals.size())
        Current_->nextReg = locals[pop_from].reg;

    locals.resize(pop_from);
}

uint32_t Compiler::internString(StringRef const& str)
{
    // Key must be per-chunk, not global
    Chunk* chunk = currentChunk();
    auto key = std::make_pair(str, chunk);
    auto it = StringCache_.find(key);
    if (it != StringCache_.end())
        return it->second;

    ObjString* obj = makeObjectString(str);
    uint16_t idx = chunk->addConstant(makeObject(obj));
    StringCache_[key] = idx;
    return idx;
}

} // namespace mylang::runtime
