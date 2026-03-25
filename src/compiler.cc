/// compiler.cc

#include "../include/compiler.hpp"
#include "../include/optim.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

namespace mylang::runtime {

using CompilerError = diagnostic::errc::compiler::Code;

static SourceLocation srcLoc(Stmt const* s) { return { s->getLine(), s->getColumn(), 0 }; }
static SourceLocation srcLoc(Expr const* e) { return { e->getLine(), e->getColumn(), 0 }; }

static bool isTerminalTopLevelCall(Stmt const* s)
{
    auto const* expr_stmt = dynamic_cast<ExprStmt const*>(s);
    if (!expr_stmt)
        return false;
    return dynamic_cast<CallExpr const*>(expr_stmt->getExpr()) != nullptr;
}

static void patchA(Chunk* chunk, uint32_t pc, uint8_t a)
{
    uint32_t instr = chunk->code[pc];
    chunk->code[pc] = (instr & 0xFF00FFFFu) | (static_cast<uint32_t>(a) << 16);
}

Chunk* Compiler::compile(Array<Stmt*> const& stmts)
{
    Chunk* chunk = makeChunk();
    chunk->name = "<main>";

    CompilerState state;
    state.chunk = chunk;
    state.funcName = "<main>";
    state.isTopLevel = true;
    state.enclosing = nullptr;
    Current_ = &state;

    for (size_t i = 0; i < stmts.size(); ++i) {
        Stmt const* stmt = stmts[i];
        if (i + 1 == stmts.size() && stmt && !state.isDead_ && isTerminalTopLevelCall(stmt)) {
            auto const* expr_stmt = static_cast<ExprStmt const*>(stmt);
            SourceLocation loc = srcLoc(expr_stmt);
            RegMark mark(Current_);
            uint8_t src = anyReg(compileExprI(expr_stmt->getExpr()), loc);
            emit(make_ABC(OpCode::RETURN, src, 1, 0), loc);
            state.isDead_ = true;
            break;
        }
        compileStmt(stmt);
    }

    SourceLocation loc = { 1, 1, 0 };
    if (!stmts.empty() && stmts.back())
        loc = srcLoc(stmts.back());

    if (!state.isDead_)
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);

    chunk->localCount = state.maxReg;
    chunk->upvalueCount = 0;
    Current_ = nullptr;
    return chunk;
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
        compileFor(static_cast<ForStmt const*>(s));
        break;
    case Stmt::Kind::INVALID:
        diagnostic::emit(CompilerError::INVALID_STATEMENT_NODE);
        break;
    }
}

void Compiler::compileBlock(BlockStmt const* s)
{
    beginScope();
    for (Stmt const* child : s->getStatements())
        compileStmt(child);

    SourceLocation loc = { 1, 1, 0 };
    if (!s->getStatements().empty() && s->getStatements().back())
        loc = srcLoc(s->getStatements().back());
    endScope(loc);
}

void Compiler::compileExprStmt(ExprStmt const* s)
{
    RegMark mark(Current_);
    uint8_t tmp = allocRegister();
    ExprResult r = compileExprI(s->getExpr());
    discharge(r, tmp, srcLoc(s));
}

void Compiler::compileAssignmentStmt(AssignmentStmt const* s)
{
    SourceLocation loc = srcLoc(s);
    auto const* name = dynamic_cast<NameExpr const*>(s->getTarget());
    if (!name) {
        diagnostic::emit(CompilerError::INVALID_ASSIGNMENT_TARGET, "only simple name assignments are supported");
        return;
    }

    if (s->isDeclaration()) {
        uint8_t reg = allocRegister();
        ExprResult value = compileExprI(s->getValue());
        discharge(value, reg, loc);
        declareLocal(name->getValue(), reg);
        return;
    }

    VarInfo vi = resolveName(name->getValue());
    if (vi.kind == VarInfo::Kind::LOCAL) {
        compileExpr(s->getValue(), &vi.index);
        return;
    }

    RegMark mark(Current_);
    uint8_t src = anyReg(compileExprI(s->getValue()), loc);
    uint16_t kidx = internString(name->getValue());
    emit(make_ABx(OpCode::STORE_GLOBAL, src, kidx), loc);
}

void Compiler::compileIf(IfStmt const* s)
{
    if (!s)
        return;

    SourceLocation loc = srcLoc(s);
    bool incoming_dead = Current_->isDead_;

    if (auto folded = tryFoldExpr(s->getCondition())) {
        if (IS_TRUTHY(*folded))
            compileStmt(s->getThen());
        else if (Stmt* else_stmt = s->getElse())
            compileStmt(else_stmt);
        Current_->isDead_ = incoming_dead;
        return;
    }

    RegMark mark(Current_);
    uint8_t cond = anyReg(compileExprI(s->getCondition()), loc);
    uint32_t jump_false = emitJump(OpCode::JUMP_IF_FALSE, cond, loc);
    compileStmt(s->getThen());

    if (Stmt* else_stmt = s->getElse()) {
        uint32_t jump_end = emitJump(OpCode::JUMP, 0, loc);
        patchJump(jump_false);
        compileStmt(else_stmt);
        patchJump(jump_end);
    } else {
        patchJump(jump_false);
    }

    Current_->isDead_ = incoming_dead;
}

void Compiler::compileWhile(WhileStmt const* s)
{
    if (!s)
        return;

    SourceLocation loc = srcLoc(s);
    bool incoming_dead = Current_->isDead_;

    if (auto folded = tryFoldExpr(s->getCondition())) {
        if (IS_TRUTHY(*folded)) {
            uint32_t loop_start = currentOffset();
            pushLoop(loop_start);
            compileStmt(s->getBody());
            emit(make_AsBx(OpCode::LOOP, 0, static_cast<int32_t>(loop_start) - static_cast<int32_t>(currentOffset()) - 1), loc);
            popLoop(currentOffset(), loop_start, loc.line);
        }
        Current_->isDead_ = incoming_dead;
        return;
    }

    uint32_t loop_start = currentOffset();
    pushLoop(loop_start);
    {
        RegMark mark(Current_);
        uint8_t cond = anyReg(compileExprI(s->getCondition()), loc);
        uint32_t exit_jump = emitJump(OpCode::JUMP_IF_FALSE, cond, loc);
        compileStmt(s->getBody());
        emit(make_AsBx(OpCode::LOOP, 0, static_cast<int32_t>(loop_start) - static_cast<int32_t>(currentOffset()) - 1), loc);
        patchJump(exit_jump);
    }
    popLoop(currentOffset(), loop_start, loc.line);
    Current_->isDead_ = incoming_dead;
}

void Compiler::compileFunctionDef(FunctionDef const* f)
{
    SourceLocation loc = srcLoc(f);
    NameExpr* name = f->getName();
    if (!name) {
        diagnostic::emit(CompilerError::NULL_FUNCTION_NAME, diagnostic::Severity::FATAL);
        return;
    }

    Chunk* fn_chunk = makeChunk();
    fn_chunk->name = name->getValue();
    fn_chunk->arity = f->hasParameters() ? static_cast<int>(f->getParameters().size()) : 0;
    fn_chunk->upvalueCount = 0;

    auto fn_idx = static_cast<uint16_t>(currentChunk()->functions.size());
    currentChunk()->functions.push(fn_chunk);

    CompilerState fn_state;
    fn_state.chunk = fn_chunk;
    fn_state.funcName = name->getValue();
    fn_state.enclosing = Current_;
    Current_ = &fn_state;

    beginScope();
    if (f->hasParameters()) {
        for (Expr const* param : f->getParameters()) {
            auto const* param_name = dynamic_cast<NameExpr const*>(param);
            if (!param_name) {
                diagnostic::emit(CompilerError::INVALID_FUNCTION_PARAMETER);
                continue;
            }
            uint8_t reg = allocRegister();
            declareLocal(param_name->getValue(), reg);
        }
    }

    compileStmt(f->getBody());
    if (!fn_state.isDead_)
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);

    endScope(loc);
    fn_chunk->localCount = fn_state.maxReg;
    Current_ = fn_state.enclosing;

    uint8_t dst = allocRegister();
    emit(make_ABx(OpCode::CLOSURE, dst, fn_idx), loc);
    if (Current_ && Current_->isTopLevel) {
        uint16_t name_idx = internString(name->getValue());
        emit(make_ABx(OpCode::STORE_GLOBAL, dst, name_idx), loc);
    }
    declareLocal(name->getValue(), dst);
}

void Compiler::compileReturn(ReturnStmt const* s)
{
    SourceLocation loc = srcLoc(s);

    if (!s->hasValue()) {
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);
        Current_->isDead_ = true;
        return;
    }

    Expr const* value = s->getValue();
    if (value->getKind() == Expr::Kind::LITERAL
        && static_cast<LiteralExpr const*>(value)->isNil()) {
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);
        Current_->isDead_ = true;
        return;
    }

    if (value->getKind() == Expr::Kind::CALL && !Current_->isTopLevel) {
        RegMark mark(Current_);
        compileCallImpl(static_cast<CallExpr const*>(value), nullptr, true);
        Current_->isDead_ = true;
        return;
    }

    RegMark mark(Current_);
    uint8_t src = anyReg(compileExprI(value), loc);
    emit(make_ABC(OpCode::RETURN, src, 1, 0), loc);
    Current_->isDead_ = true;
}

void Compiler::compileFor(ForStmt const* s)
{
    (void)s;
    diagnostic::emit(CompilerError::FOR_NOT_IMPLEMENTED);
}

ExprResult Compiler::compileExprI(Expr const* e)
{
    if (!e)
        return ExprResult::knil();

    switch (e->getKind()) {
    case Expr::Kind::LITERAL:
        return compileLiteralI(static_cast<LiteralExpr const*>(e));
    case Expr::Kind::NAME:
        return compileNameI(static_cast<NameExpr const*>(e));
    case Expr::Kind::UNARY:
        return compileUnaryI(static_cast<UnaryExpr const*>(e));
    case Expr::Kind::BINARY:
        return compileBinaryI(static_cast<BinaryExpr const*>(e));
    case Expr::Kind::ASSIGNMENT:
        return compileAssignI(static_cast<AssignmentExpr const*>(e));
    case Expr::Kind::CALL:
        return compileCallImpl(static_cast<CallExpr const*>(e), nullptr, false);
    case Expr::Kind::LIST:
        return compileListI(static_cast<ListExpr const*>(e));
    case Expr::Kind::INDEX:
        return compileIndexI(static_cast<IndexExpr const*>(e));
    case Expr::Kind::INVALID:
        diagnostic::emit(CompilerError::INVALID_EXPRESSION_NODE);
        return ExprResult::knil();
    }

    return ExprResult::knil();
}

ExprResult Compiler::compileLiteralI(LiteralExpr const* e)
{
    if (e->isString()) {
        uint16_t kidx = internString(e->getStr());
        uint32_t pc = emit(make_ABx(OpCode::LOAD_CONST, 0, kidx), srcLoc(e));
        return ExprResult::reloc(pc);
    }
    if (e->isInteger())
        return ExprResult::kint(e->getInt());
    if (e->isDecimal())
        return ExprResult::kfloat(e->getFloat());
    if (e->isBoolean())
        return ExprResult::kbool(e->getBool());
    if (e->isNil())
        return ExprResult::knil();

    diagnostic::emit(CompilerError::UNKNOWN_LITERAL_TYPE);
    return ExprResult::knil();
}

ExprResult Compiler::compileNameI(NameExpr const* e)
{
    SourceLocation loc = srcLoc(e);
    VarInfo vi = resolveName(e->getValue());

    if (vi.kind == VarInfo::Kind::LOCAL)
        return ExprResult::reg(vi.index);

    uint16_t kidx = internString(e->getValue());
    uint32_t pc = emit(make_ABx(OpCode::LOAD_GLOBAL, 0, kidx), loc);
    return ExprResult::reloc(pc);
}

ExprResult Compiler::compileUnaryI(UnaryExpr const* e)
{
    SourceLocation loc = srcLoc(e);

    if (auto folded = tryFoldUnary(e)) {
        Value v = *folded;
        if (IS_INTEGER(v))
            return ExprResult::kint(AS_INTEGER(v));
        if (IS_DOUBLE(v))
            return ExprResult::kfloat(AS_DOUBLE(v));
        if (IS_BOOL(v))
            return ExprResult::kbool(AS_BOOL(v));
        if (IS_NIL(v))
            return ExprResult::knil();
    }

    OpCode op = OpCode::NOP;
    switch (e->getOperator()) {
    case UnaryOp::OP_NEG:
        op = OpCode::OP_NEG;
        break;
    case UnaryOp::OP_BITNOT:
        op = OpCode::OP_BITNOT;
        break;
    case UnaryOp::OP_NOT:
        op = OpCode::OP_NOT;
        break;
    default:
        diagnostic::emit(CompilerError::UNKNOWN_UNARY_OPERATOR, diagnostic::Severity::FATAL);
        return ExprResult::knil();
    }

    RegMark mark(Current_);
    uint8_t src = anyReg(compileExprI(e->getOperand()), loc);
    uint32_t pc = emit(make_ABC(op, 0, src, 0), loc);
    return ExprResult::reloc(pc);
}

ExprResult Compiler::compileBinaryI(BinaryExpr const* e)
{
    SourceLocation loc = srcLoc(e);

    if (auto folded = tryFoldBinary(e)) {
        Value v = *folded;
        if (IS_INTEGER(v))
            return ExprResult::kint(AS_INTEGER(v));
        if (IS_DOUBLE(v))
            return ExprResult::kfloat(AS_DOUBLE(v));
        if (IS_BOOL(v))
            return ExprResult::kbool(AS_BOOL(v));
        if (IS_NIL(v))
            return ExprResult::knil();
    }

    BinaryOp op = e->getOperator();
    if (op == BinaryOp::OP_AND) {
        uint8_t dst = allocRegister();
        {
            RegMark mark(Current_);
            discharge(compileExprI(e->getLeft()), dst, loc);
        }
        uint32_t skip = emitJump(OpCode::JUMP_IF_FALSE, dst, loc);
        {
            RegMark mark(Current_);
            discharge(compileExprI(e->getRight()), dst, loc);
        }
        patchJump(skip);
        return ExprResult::reg(dst);
    }

    if (op == BinaryOp::OP_OR) {
        uint8_t dst = allocRegister();
        {
            RegMark mark(Current_);
            discharge(compileExprI(e->getLeft()), dst, loc);
        }
        uint32_t skip = emitJump(OpCode::JUMP_IF_TRUE, dst, loc);
        {
            RegMark mark(Current_);
            discharge(compileExprI(e->getRight()), dst, loc);
        }
        patchJump(skip);
        return ExprResult::reg(dst);
    }

    OpCode bc_op = OpCode::NOP;
    bool swapped = false;
    switch (op) {
    case BinaryOp::OP_ADD:
        bc_op = OpCode::OP_ADD;
        break;
    case BinaryOp::OP_SUB:
        bc_op = OpCode::OP_SUB;
        break;
    case BinaryOp::OP_MUL:
        bc_op = OpCode::OP_MUL;
        break;
    case BinaryOp::OP_DIV:
        bc_op = OpCode::OP_DIV;
        break;
    case BinaryOp::OP_MOD:
        bc_op = OpCode::OP_MOD;
        break;
    case BinaryOp::OP_POW:
        bc_op = OpCode::OP_POW;
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
    case BinaryOp::OP_GT:
        bc_op = OpCode::OP_LT;
        swapped = true;
        break;
    case BinaryOp::OP_GTE:
        bc_op = OpCode::OP_LTE;
        swapped = true;
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
    default:
        diagnostic::emit(CompilerError::UNKNOWN_BINARY_OPERATOR, diagnostic::Severity::FATAL);
        return ExprResult::knil();
    }

    if (bc_op == OpCode::OP_LSHIFT || bc_op == OpCode::OP_RSHIFT) {
        auto const* amount_expr = dynamic_cast<LiteralExpr const*>(e->getRight());
        if (!amount_expr || !amount_expr->isInteger()) {
            diagnostic::emit(CompilerError::SHIFT_AMOUNT_NOT_CONSTANT);
            return ExprResult::knil();
        }
        int64_t amount = amount_expr->getInt();
        if (amount < 0 || amount > 63) {
            diagnostic::emit(CompilerError::SHIFT_AMOUNT_OUT_OF_RANGE, std::to_string(amount));
            return ExprResult::knil();
        }

        RegMark mark(Current_);
        uint8_t lhs = anyReg(compileExprI(e->getLeft()), loc);
        uint32_t pc = emit(make_ABC(bc_op, 0, lhs, static_cast<uint8_t>(amount)), loc);
        uint8_t ic = currentChunk()->allocIcSlot();
        emit(make_ABC(OpCode::NOP, ic, 0, 0), loc);
        return ExprResult::reloc(pc);
    }

    RegMark mark(Current_);
    uint8_t lhs = anyReg(compileExprI(e->getLeft()), loc);
    uint8_t rhs = anyReg(compileExprI(e->getRight()), loc);
    if (swapped)
        std::swap(lhs, rhs);

    uint32_t pc = emit(make_ABC(bc_op, 0, lhs, rhs), loc);
    uint8_t ic = currentChunk()->allocIcSlot();
    emit(make_ABC(OpCode::NOP, ic, 0, 0), loc);
    return ExprResult::reloc(pc);
}

ExprResult Compiler::compileAssignI(AssignmentExpr const* e)
{
    SourceLocation loc = srcLoc(e);
    Expr const* target = e->getTarget();

    if (target->getKind() == Expr::Kind::INDEX) {
        auto index_expr = static_cast<IndexExpr const*>(target);
        RegMark mark(Current_);
        uint8_t list_reg = anyReg(compileExprI(index_expr->getObject()), loc);
        uint8_t index_reg = anyReg(compileExprI(index_expr->getIndex()), loc);
        uint8_t value_reg = anyReg(compileExprI(e->getValue()), loc);
        emit(make_ABC(OpCode::LIST_SET, list_reg, index_reg, value_reg), loc);
        return ExprResult::reg(value_reg);
    }

    auto const* name = dynamic_cast<NameExpr const*>(target);
    if (!name) {
        diagnostic::emit(CompilerError::INVALID_ASSIGNMENT_TARGET);
        return ExprResult::knil();
    }

    VarInfo vi = resolveName(name->getValue());
    if (vi.kind == VarInfo::Kind::LOCAL) {
        compileExpr(e->getValue(), &vi.index);
        return ExprResult::reg(vi.index);
    }

    RegMark mark(Current_);
    uint8_t src = anyReg(compileExprI(e->getValue()), loc);
    uint16_t kidx = internString(name->getValue());
    emit(make_ABx(OpCode::STORE_GLOBAL, src, kidx), loc);
    return ExprResult::reg(src);
}

ExprResult Compiler::compileCallImpl(CallExpr const* e, uint8_t* dst, bool tail)
{
    SourceLocation loc = srcLoc(e);
    uint8_t fn_reg = dst ? *dst : allocRegister();

    discharge(compileExprI(e->getCallee()), fn_reg, loc);

    for (Expr const* arg : e->getArgs()) {
        uint8_t arg_reg = allocRegister();
        discharge(compileExprI(arg), arg_reg, loc);
    }

    uint8_t argc = static_cast<uint8_t>(e->getArgs().size());
    if (tail && !Current_->isTopLevel) {
        emit(make_ABC(OpCode::CALL_TAIL, fn_reg, argc, 0), loc);
        Current_->freeRegsTo(fn_reg);
        return ExprResult::reg(fn_reg);
    }

    uint8_t ic = currentChunk()->allocIcSlot();
    emit(make_ABC(OpCode::IC_CALL, fn_reg, argc, ic), loc);
    Current_->freeRegsTo(fn_reg + 1);
    return ExprResult::reg(fn_reg);
}

ExprResult Compiler::compileListI(ListExpr const* e)
{
    SourceLocation loc = srcLoc(e);
    uint8_t dst = allocRegister();
    auto cap = static_cast<uint8_t>(std::min<uint32_t>(e->size(), 0xFF));
    emit(make_ABC(OpCode::LIST_NEW, dst, cap, 0), loc);

    RegMark mark(Current_);
    for (Expr const* elem : e->getElements()) {
        uint8_t reg = anyReg(compileExprI(elem), loc);
        emit(make_ABC(OpCode::LIST_APPEND, dst, reg, 0), loc);
    }

    return ExprResult::reg(dst);
}

ExprResult Compiler::compileIndexI(IndexExpr const* e)
{
    SourceLocation loc = srcLoc(e);
    RegMark mark(Current_);
    uint8_t list_reg = anyReg(compileExprI(e->getObject()), loc);
    uint8_t index_reg = anyReg(compileExprI(e->getIndex()), loc);
    uint32_t pc = emit(make_ABC(OpCode::LIST_GET, 0, list_reg, index_reg), loc);
    return ExprResult::reloc(pc);
}

void Compiler::discharge(ExprResult const& r, uint8_t dst, SourceLocation loc)
{
    switch (r.kind) {
    case ExprResult::Kind::REG:
        if (r.reg_ != dst)
            emit(make_ABC(OpCode::MOVE, dst, r.reg_, 0), loc);
        break;
    case ExprResult::Kind::RELOC:
        patchA(currentChunk(), r.reloc_pc, dst);
        break;
    case ExprResult::Kind::KINT:
        emitLoadValue(dst, MAKE_INTEGER(r.ival), loc);
        break;
    case ExprResult::Kind::KFLOAT:
        emitLoadValue(dst, MAKE_REAL(r.dval), loc);
        break;
    case ExprResult::Kind::KBOOL:
        emitLoadValue(dst, MAKE_BOOL(r.bval), loc);
        break;
    case ExprResult::Kind::KNIL:
        emitLoadValue(dst, NIL_VAL, loc);
        break;
    }
}

uint8_t Compiler::anyReg(ExprResult const& r, SourceLocation loc)
{
    if (r.kind == ExprResult::Kind::REG)
        return r.reg_;

    uint8_t dst = allocRegister();
    discharge(r, dst, loc);
    return dst;
}

uint8_t Compiler::compileExpr(Expr const* e, uint8_t* dst)
{
    if (!e)
        return errorReg();

    SourceLocation loc = srcLoc(e);
    ExprResult r = compileExprI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

uint8_t Compiler::compileLiteral(LiteralExpr const* e, uint8_t* dst)
{
    SourceLocation loc = srcLoc(e);
    ExprResult r = compileLiteralI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

uint8_t Compiler::compileName(NameExpr const* e, uint8_t* dst)
{
    SourceLocation loc = srcLoc(e);
    ExprResult r = compileNameI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

uint8_t Compiler::compileUnary(UnaryExpr const* e, uint8_t* dst)
{
    SourceLocation loc = srcLoc(e);
    ExprResult r = compileUnaryI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

uint8_t Compiler::compileBinary(BinaryExpr const* e, uint8_t* dst)
{
    SourceLocation loc = srcLoc(e);
    ExprResult r = compileBinaryI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

uint8_t Compiler::compileAssignmentExpr(AssignmentExpr const* e, uint8_t* dst)
{
    SourceLocation loc = srcLoc(e);
    ExprResult r = compileAssignI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

uint8_t Compiler::compileCall(CallExpr const* e, uint8_t* dst, bool tail)
{
    ExprResult r = compileCallImpl(e, dst, tail);
    return r.reg_;
}

uint8_t Compiler::compileList(ListExpr const* e, uint8_t* dst)
{
    SourceLocation loc = srcLoc(e);
    ExprResult r = compileListI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

uint8_t Compiler::compileIndex(IndexExpr const* e, uint8_t* dst)
{
    SourceLocation loc = srcLoc(e);
    ExprResult r = compileIndexI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

uint8_t Compiler::errorReg() const
{
    return 0;
}

uint8_t Compiler::ensureReg(uint8_t const* reg)
{
    return reg ? *reg : allocRegister();
}

uint8_t Compiler::allocRegister()
{
    uint8_t reg = Current_->allocRegister();
    if (reg >= MAX_REGS) {
        diagnostic::emit(CompilerError::TOO_MANY_REGISTERS, std::string(Current_->funcName.data(), Current_->funcName.len()));
        return 0;
    }
    return reg;
}

void Compiler::declareLocal(StringRef const& name, uint8_t reg)
{
    Current_->locals.push({ name, Current_->scopeDepth, reg });
}

Compiler::VarInfo Compiler::resolveName(StringRef const& name)
{
    auto const& locals = Current_->locals;
    for (auto i = static_cast<int>(locals.size()) - 1; i >= 0; --i) {
        if (locals[i].name == name)
            return { VarInfo::Kind::LOCAL, locals[i].reg };
    }

    return { VarInfo::Kind::GLOBAL, 0 };
}

int Compiler::resolveUpvalue(CompilerState* state, StringRef const& name)
{
    (void)state;
    (void)name;
    return -1;
}

int Compiler::addUpvalue(CompilerState* state, bool is_local, uint8_t index)
{
    (void)state;
    (void)is_local;
    (void)index;
    return -1;
}

uint32_t Compiler::emit(uint32_t instr, SourceLocation loc)
{
    return currentChunk()->emit(instr, loc);
}

uint32_t Compiler::emitJump(OpCode op, uint8_t cond, SourceLocation loc)
{
    return emit(make_AsBx(op, cond, 0), loc);
}

void Compiler::patchJump(uint32_t idx)
{
    if (!currentChunk()->patchJump(idx))
        diagnostic::emit(CompilerError::JUMP_OFFSET_OVERFLOW, diagnostic::Severity::FATAL);
}

void Compiler::pushLoop(uint32_t loop_start)
{
    Current_->loopStack.push({ { }, { }, loop_start });
}

void Compiler::popLoop(uint32_t loop_exit, uint32_t continue_target, uint32_t line)
{
    (void)line;
    assert(!Current_->loopStack.empty());
    auto& ctx = Current_->loopStack.back();

    for (uint32_t idx : ctx.breakPatches)
        patchJumpTo(idx, loop_exit);
    for (uint32_t idx : ctx.continuePatches)
        patchJumpTo(idx, continue_target);

    Current_->loopStack.pop();
}

void Compiler::patchJumpTo(uint32_t instr_idx, uint32_t target)
{
    auto offset = static_cast<int32_t>(target) - static_cast<int32_t>(instr_idx) - 1;
    if (offset > JUMP_OFFSET || offset < -JUMP_OFFSET)
        diagnostic::emit(CompilerError::LOOP_JUMP_OFFSET_OVERFLOW, diagnostic::Severity::FATAL);

    uint32_t word = currentChunk()->code[instr_idx];
    currentChunk()->code[instr_idx] = make_AsBx(instr_op(word), instr_A(word), offset);
}

void Compiler::emitLoadValue(uint8_t dst, Value v, SourceLocation loc)
{
    if (IS_NIL(v)) {
        emit(make_ABC(OpCode::LOAD_NIL, dst, dst, 1), loc);
        return;
    }

    if (IS_BOOL(v)) {
        emit(make_ABC(AS_BOOL(v) ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE, dst, 0, 0), loc);
        return;
    }

    if (IS_INTEGER(v)) {
        int64_t iv = AS_INTEGER(v);
        if (iv >= -JUMP_OFFSET && iv <= JUMP_OFFSET) {
            emit(make_ABx(OpCode::LOAD_INT, dst, static_cast<uint16_t>(iv + JUMP_OFFSET)), loc);
            return;
        }
    }

    emit(make_ABx(OpCode::LOAD_CONST, dst, currentChunk()->addConstant(v)), loc);
}

Chunk* Compiler::currentChunk() const
{
    return Current_->chunk;
}

uint32_t Compiler::currentOffset() const
{
    return currentChunk()->code.size();
}

void Compiler::beginScope()
{
    ++Current_->scopeDepth;
}

void Compiler::endScope(SourceLocation loc)
{
    (void)loc;
    unsigned int depth = --Current_->scopeDepth;
    auto& locals = Current_->locals;
    size_t pop_from = locals.size();

    while (pop_from > 0 && locals[pop_from - 1].depth > depth)
        --pop_from;

    if (pop_from < locals.size())
        Current_->nextReg = locals[pop_from].reg;

    locals.resize(static_cast<uint32_t>(pop_from));
}

uint32_t Compiler::internString(StringRef const& str)
{
    Chunk* chunk = currentChunk();
    auto key = std::make_pair(str, chunk);
    auto it = StringCache_.find(key);
    if (it != StringCache_.end())
        return it->second;

    ObjString* obj = MAKE_OBJ_STRING(str);
    uint16_t idx = chunk->addConstant(MAKE_OBJECT(obj));
    StringCache_[key] = idx;
    return idx;
}

} // namespace mylang::runtime
