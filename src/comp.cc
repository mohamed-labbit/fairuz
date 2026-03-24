/// compiler.cc

#include "../include/compiler.hpp"
#include "../include/optim.hpp"

namespace mylang::runtime {

static SourceLocation srcLoc(Stmt const* s) { return { s->getLine(), s->getColumn(), 0 }; }
static SourceLocation srcLoc(Expr const* e) { return { e->getLine(), e->getColumn(), 0 }; }

static void patchA(Chunk* chunk, uint32_t pc, uint8_t a)
{
    uint32_t instr = chunk->code[pc];
    chunk->code[pc] = (instr & 0xFF00FFFFu) | (static_cast<uint32_t>(a) << 16);
}

Chunk* Compiler::compile(Array<Stmt*> const& stmts)
{
    if (stmts.empty() || !stmts[0]) {
        diagnostic::emit("null AST root", diagnostic::Severity::FATAL);
        return nullptr;
    }

    auto* top_chunk = makeChunk();
    top_chunk->name = "<main>";

    CompilerState state;
    state.chunk = top_chunk;
    state.funcName = "<main>";
    state.isTopLevel = true;
    state.enclosing = nullptr;
    Current_ = &state;

    for (auto const& s : stmts)
        compileStmt(s);

    if (!state.isDead_) {
        SourceLocation loc = { stmts.back()->getLine(), stmts.back()->getColumn(), 0 };
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), loc);
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
        compileFor(static_cast<ForStmt const*>(s));
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
    ExprResult r = compileExprI(s->getExpr());
    if (r.isReloc())
        patchA(currentChunk(), r.reloc_pc, 0);
}

void Compiler::compileAssignmentStmt(AssignmentStmt const* s)
{
    SourceLocation cur_loc = srcLoc(s);
    auto name = dynamic_cast<NameExpr const*>(s->getTarget());
    assert(name);

    if (s->isDeclaration()) {
        uint8_t reg = allocRegister();
        ExprResult val = compileExprI(s->getValue());
        discharge(val, reg, cur_loc);
        declareLocal(name->getValue(), reg);
        return;
    }

    VarInfo vi = resolveName(name->getValue());
    switch (vi.kind) {
    case VarInfo::Kind::LOCAL: {
        ExprResult val = compileExprI(s->getValue());
        discharge(val, vi.index, cur_loc);
        break;
    }
    case VarInfo::Kind::UPVALUE: {
        RegMark mark(Current_);
        uint8_t src = anyReg(compileExprI(s->getValue()), cur_loc);
        emit(make_ABC(OpCode::SET_UPVALUE, src, vi.index, 0), cur_loc);
        break;
    }
    case VarInfo::Kind::GLOBAL: {
        RegMark mark(Current_);
        uint8_t src = anyReg(compileExprI(s->getValue()), cur_loc);
        uint16_t kidx = internString(name->getValue());
        emit(make_ABx(OpCode::STORE_GLOBAL, src, kidx), cur_loc);
        break;
    }
    }
}

void Compiler::compileIf(IfStmt const* s)
{
    if (!s)
        return;

    SourceLocation cur_loc = srcLoc(s);
    bool const incoming_dead = Current_->isDead_;

    if (auto folded = tryFoldExpr(s->getCondition())) {
        if (IS_TRUTHY(*folded))
            compileStmt(s->getThen());
        else if (auto* eb = s->getElse())
            compileStmt(eb);
        Current_->isDead_ = incoming_dead;
        return;
    }

    RegMark mark(Current_);
    uint8_t cond = anyReg(compileExprI(s->getCondition()), cur_loc);
    uint32_t jump_false = emitJump(OpCode::JUMP_IF_FALSE, cond, cur_loc);
    compileStmt(s->getThen());

    if (auto* else_block = s->getElse()) {
        uint32_t jump_end = emitJump(OpCode::JUMP, 0, cur_loc);
        patchJump(jump_false);
        compileStmt(else_block);
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

    SourceLocation const cur_loc = srcLoc(s);
    bool const incoming_dead = Current_->isDead_;

    if (auto folded = tryFoldExpr(s->getCondition())) {
        if (IS_TRUTHY(*folded)) {
            uint32_t loop_start = currentOffset();
            pushLoop(loop_start);
            compileStmt(s->getBody());
            emit(make_AsBx(OpCode::LOOP, 0, loop_start - currentOffset() - 1), cur_loc);
            popLoop(currentOffset(), loop_start, cur_loc.line);
        }
        Current_->isDead_ = incoming_dead;
        return;
    }

    uint32_t loop_start = currentOffset();
    pushLoop(loop_start);
    {
        RegMark mark(Current_);
        uint8_t cond = anyReg(compileExprI(s->getCondition()), cur_loc);
        uint32_t exit_jump = emitJump(OpCode::JUMP_IF_FALSE, cond, cur_loc);
        compileStmt(s->getBody());
        emit(make_AsBx(OpCode::LOOP, 0, loop_start - currentOffset() - 1), cur_loc);
        patchJump(exit_jump);
    }
    popLoop(currentOffset(), loop_start, cur_loc.line);
    Current_->isDead_ = incoming_dead;
}

void Compiler::compileFunctionDef(FunctionDef const* f)
{
    SourceLocation cur_loc = srcLoc(f);
    NameExpr* name = f->getName();
    if (!name) {
        diagnostic::emit("Function name is null", diagnostic::Severity::FATAL);
        return;
    }

    Chunk* fn_chunk = makeChunk();
    fn_chunk->name = name->getValue();
    fn_chunk->arity = f->hasParameters() ? static_cast<int>(f->getParameters().size()) : 0;

    uint16_t fn_idx = static_cast<uint16_t>(currentChunk()->functions.size());
    currentChunk()->functions.push(fn_chunk);

    CompilerState fn_state;
    fn_state.chunk = fn_chunk;
    fn_state.funcName = name->getValue();
    fn_state.enclosing = Current_;
    Current_ = &fn_state;

    beginScope();
    if (f->hasParameters()) {
        for (Expr const* e : f->getParameters()) {
            uint8_t reg = allocRegister();
            declareLocal(dynamic_cast<NameExpr const*>(e)->getValue(), reg);
        }
    }

    compileStmt(f->getBody());
    if (!fn_state.isDead_)
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), cur_loc);

    endScope(cur_loc);

    fn_chunk->localCount = fn_state.maxReg;
    fn_chunk->upvalueCount = static_cast<unsigned int>(fn_state.upvalues.size());

    Current_ = fn_state.enclosing;

    uint8_t dst = allocRegister();
    emit(make_ABx(OpCode::CLOSURE, dst, fn_idx), cur_loc);

    for (auto& uv : fn_state.upvalues)
        emit(make_ABC(OpCode::MOVE, uv.isLocal ? 1 : 0, uv.index, 0), cur_loc);

    if (Current_->isTopLevel) {
        uint16_t kidx = internString(name->getValue());
        emit(make_ABx(OpCode::STORE_GLOBAL, dst, kidx), cur_loc);
        Current_->freeRegsTo(dst);
    } else {
        declareLocal(name->getValue(), dst);
    }
}

void Compiler::compileReturn(ReturnStmt const* s)
{
    SourceLocation cur_loc = srcLoc(s);

    if (!s->hasValue() || (s->getValue()->getKind() == Expr::Kind::LITERAL && static_cast<LiteralExpr const*>(s->getValue())->isNil())) {
        emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), cur_loc);
        Current_->isDead_ = true;
        return;
    }

    Expr const* value = s->getValue();
    if (value->getKind() == Expr::Kind::CALL && !Current_->isTopLevel) {
        RegMark mark(Current_);
        compileCallImpl(static_cast<CallExpr const*>(value), nullptr,
            /*tail=*/true);
        Current_->isDead_ = true;
        return;
    }

    RegMark mark(Current_);
    uint8_t src = anyReg(compileExprI(value), cur_loc);
    emit(make_ABC(OpCode::RETURN1, src, 0, 0), cur_loc);
    Current_->isDead_ = true;
}

void Compiler::compileFor(ForStmt const* s)
{
    (void)s; // not supported yet
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
        return compileCallImpl(static_cast<CallExpr const*>(e), nullptr);
    case Expr::Kind::LIST:
        return compileListI(static_cast<ListExpr const*>(e));
    case Expr::Kind::INDEX:
        return compileIndexI(static_cast<IndexExpr const*>(e));
    case Expr::Kind::INVALID:
        diagnostic::emit("Invalid expression node");
        break;
    }
    return ExprResult::knil();
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

ExprResult Compiler::compileLiteralI(LiteralExpr const* e)
{
    if (e->isString()) {
        SourceLocation loc = srcLoc(e);
        uint16_t kidx = internString(e->getStr());
        uint32_t pc = emit(make_ABx(OpCode::LOAD_CONST, 0, kidx), loc);
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

    diagnostic::emit("Unknown literal type");
    return ExprResult::knil();
}

ExprResult Compiler::compileNameI(NameExpr const* e)
{
    SourceLocation loc = srcLoc(e);
    VarInfo vi = resolveName(e->getValue());

    switch (vi.kind) {
    case VarInfo::Kind::LOCAL:
        return ExprResult::reg(vi.index);

    case VarInfo::Kind::UPVALUE: {
        uint32_t pc = emit(make_ABC(OpCode::GET_UPVALUE, 0, vi.index, 0), loc);
        return ExprResult::reloc(pc);
    }
    case VarInfo::Kind::GLOBAL: {
        uint16_t kidx = internString(e->getValue());
        uint32_t pc = emit(make_ABx(OpCode::LOAD_GLOBAL, 0, kidx), loc);
        return ExprResult::reloc(pc);
    }
    }
    return ExprResult::knil();
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

    RegMark mark(Current_);
    uint8_t src = anyReg(compileExprI(e->getOperand()), loc);

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
    default:
        diagnostic::emit("Unknown unary operator", diagnostic::Severity::FATAL);
        return ExprResult::knil();
    }

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

    BinaryOp const op = e->getOperator();

    if (op == BinaryOp::OP_AND) {
        uint8_t dst = allocRegister();
        {
            RegMark mark(Current_);
            ExprResult left = compileExprI(e->getLeft());
            discharge(left, dst, loc);
        }
        uint32_t skip = emitJump(OpCode::JUMP_IF_FALSE, dst, loc);
        {
            RegMark mark(Current_);
            ExprResult right = compileExprI(e->getRight());
            discharge(right, dst, loc);
        }
        patchJump(skip);
        return ExprResult::reg(dst);
    }

    if (op == BinaryOp::OP_OR) {
        uint8_t dst = allocRegister();
        {
            RegMark mark(Current_);
            ExprResult left = compileExprI(e->getLeft());
            discharge(left, dst, loc);
        }
        uint32_t skip = emitJump(OpCode::JUMP_IF_TRUE, dst, loc);
        {
            RegMark mark(Current_);
            ExprResult right = compileExprI(e->getRight());
            discharge(right, dst, loc);
        }
        patchJump(skip);
        return ExprResult::reg(dst);
    }

    OpCode bc_op;
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
        bc_op = OpCode::OP_LT;
        swapped = true;
        break;
    case BinaryOp::OP_GTE:
        bc_op = OpCode::OP_LTE;
        swapped = true;
        break;
    default:
        diagnostic::emit("Unknown binary operator", diagnostic::Severity::FATAL);
        return ExprResult::knil();
    }

    if (bc_op == OpCode::OP_LSHIFT || bc_op == OpCode::OP_RSHIFT) {
        auto* lit = static_cast<LiteralExpr const*>(e->getRight());
        int64_t amount = lit->getInt();
        assert(amount >= 0 && amount <= 64);
        RegMark mark(Current_);
        uint8_t L = anyReg(compileExprI(e->getLeft()), loc);
        uint32_t pc = emit(make_ABC(bc_op, 0, L, static_cast<uint8_t>(amount)), loc);
        return ExprResult::reloc(pc);
    }

    if (!swapped) {
        Expr const* rhs_expr = e->getRight();
        if (rhs_expr->getKind() == Expr::Kind::LITERAL) {
            auto* lit = static_cast<LiteralExpr const*>(rhs_expr);
            if (lit->isInteger()) {
                int64_t v = lit->getInt();
                if (v >= -127 && v <= 127) {
                    OpCode ri_op;
                    bool has_ri = true;
                    switch (bc_op) {
                    case OpCode::OP_ADD:
                        ri_op = OpCode::OP_ADD_RI;
                        break;
                    case OpCode::OP_SUB:
                        ri_op = OpCode::OP_SUB_RI;
                        break;
                    case OpCode::OP_MUL:
                        ri_op = OpCode::OP_MUL_RI;
                        break;
                    case OpCode::OP_EQ:
                        ri_op = OpCode::OP_EQ_RI;
                        break;
                    case OpCode::OP_LT:
                        ri_op = OpCode::OP_LT_RI;
                        break;
                    case OpCode::OP_LTE:
                        ri_op = OpCode::OP_LTE_RI;
                        break;
                    default:
                        has_ri = false;
                        break;
                    }
                    if (has_ri) {
                        RegMark mark(Current_);
                        uint8_t L = anyReg(compileExprI(e->getLeft()), loc);
                        uint32_t pc = emit(make_ABC(ri_op, 0, L, static_cast<uint8_t>(v + 128)), loc);
                        return ExprResult::reloc(pc);
                    }
                }
            }
        }
    }

    RegMark mark(Current_);
    uint8_t L = anyReg(compileExprI(e->getLeft()), loc);
    uint8_t R = anyReg(compileExprI(e->getRight()), loc);

    if (swapped)
        std::swap(L, R);

    uint32_t pc = emit(make_ABC(bc_op, 0, L, R), loc);
    return ExprResult::reloc(pc);
}

ExprResult Compiler::compileAssignI(AssignmentExpr const* e)
{
    SourceLocation loc = srcLoc(e);
    Expr const* target = e->getTarget();

    if (target->getKind() == Expr::Kind::INDEX) {
        auto const* idx_expr = static_cast<IndexExpr const*>(target);
        RegMark mark(Current_);
        uint8_t list_reg = anyReg(compileExprI(idx_expr->getObject()), loc);
        uint8_t index_reg = anyReg(compileExprI(idx_expr->getIndex()), loc);
        uint8_t val_reg = anyReg(compileExprI(e->getValue()), loc);
        emit(make_ABC(OpCode::LIST_SET, list_reg, index_reg, val_reg), loc);
        return ExprResult::reg(val_reg);
    }

    // Simple name assignment.
    auto const* name_expr = static_cast<NameExpr const*>(target);
    StringRef name = name_expr->getValue();
    VarInfo vi = resolveName(name);

    switch (vi.kind) {
    case VarInfo::Kind::LOCAL: {
        ExprResult val = compileExprI(e->getValue());
        discharge(val, vi.index, loc);
        return ExprResult::reg(vi.index);
    }
    case VarInfo::Kind::UPVALUE: {
        RegMark mark(Current_);
        uint8_t src = anyReg(compileExprI(e->getValue()), loc);
        emit(make_ABC(OpCode::SET_UPVALUE, src, vi.index, 0), loc);
        return ExprResult::reg(src);
    }
    case VarInfo::Kind::GLOBAL: {
        RegMark mark(Current_);
        uint8_t src = anyReg(compileExprI(e->getValue()), loc);
        uint16_t kidx = internString(name);
        emit(make_ABx(OpCode::STORE_GLOBAL, src, kidx), loc);
        return ExprResult::reg(src);
    }
    }
    return ExprResult::knil();
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

ExprResult Compiler::compileCallImpl(CallExpr const* e, uint8_t* dst, bool tail)
{
    SourceLocation loc = srcLoc(e);
    uint8_t fn_reg = dst ? *dst : allocRegister();
    ExprResult callee_r = compileExprI(e->getCallee());
    discharge(callee_r, fn_reg, loc);

    auto const& args = e->getArgs();
    for (Expr const* arg : args) {
        uint8_t arg_reg = allocRegister();
        ExprResult arg_r = compileExprI(arg);
        discharge(arg_r, arg_reg, loc);
    }

    uint8_t argc = static_cast<uint8_t>(args.size());

    if (tail && !Current_->isTopLevel) {
        emit(make_ABC(OpCode::CALL_TAIL, fn_reg, argc, 0), loc);
        Current_->freeRegsTo(fn_reg);
        return ExprResult::reg(fn_reg);
    }

    if (Current_->isTopLevel) {
        uint8_t ic = currentChunk()->allocIcSlot();
        emit(make_ABC(OpCode::IC_CALL, fn_reg, argc, ic), loc);
    } else {
        emit(make_ABC(OpCode::CALL, fn_reg, argc, 0), loc);
    }

    Current_->freeRegsTo(fn_reg + 1);

    return ExprResult::reg(fn_reg);
}

ExprResult Compiler::compileListI(ListExpr const* e)
{
    SourceLocation loc = srcLoc(e);
    uint8_t dst = allocRegister();
    auto const& elems = e->getElements();
    uint8_t cap = static_cast<uint8_t>(std::min<size_t>(elems.size(), 0xFF));

    emit(make_ABC(OpCode::LIST_NEW, dst, cap, 0), loc);

    RegMark mark(Current_);
    for (Expr const* elem : elems) {
        uint8_t val = anyReg(compileExprI(elem), loc);
        emit(make_ABC(OpCode::LIST_APPEND, dst, val, 0), loc);
    }

    return ExprResult::reg(dst);
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
    return r.reg_; // compileCallImpl always returns reg
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

void Compiler::declareLocal(StringRef const& name, uint8_t const reg) { Current_->locals.push({ name, Current_->scopeDepth, reg, false }); }

Compiler::VarInfo Compiler::resolveName(StringRef const& name)
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

    auto& enc_locals = state->enclosing->locals;
    for (auto i = static_cast<int>(enc_locals.size()) - 1; i >= 0; --i) {
        if (enc_locals[i].name == name) {
            enc_locals[i].isCaptured = true;
            return addUpvalue(state, true, enc_locals[i].reg);
        }
    }

    int up = resolveUpvalue(state->enclosing, name);
    if (up >= 0)
        return addUpvalue(state, false, static_cast<uint8_t>(up));

    return -1;
}

int Compiler::addUpvalue(CompilerState* state, bool const is_local, uint8_t const index)
{
    if (!state)
        return -1;

    auto& uvs = state->upvalues;
    for (int i = 0; i < static_cast<int>(uvs.size()); ++i) {
        if (uvs[i].isLocal == is_local && uvs[i].index == index)
            return i;
    }

    uvs.push({ is_local, index });
    return static_cast<int>(uvs.size() - 1);
}

uint32_t Compiler::emit(uint32_t const instr, SourceLocation loc) { return currentChunk()->emit(instr, loc); }

uint32_t Compiler::emitJump(OpCode const op, uint8_t const cond, SourceLocation loc) { return emit(make_AsBx(op, cond, 0), loc); }

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

    auto& word = currentChunk()->code[instr_idx];
    word = make_AsBx(instr_op(word), instr_A(word), offset);
}

void Compiler::emitLoadValue(uint8_t const dst, Value const v, SourceLocation loc)
{
    if (IS_NIL(v)) {
        emit(make_ABC(OpCode::LOAD_NIL, dst, dst, 1), loc);
    } else if (IS_BOOL(v)) {
        emit(make_ABC(AS_BOOL(v) ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE, dst, 0, 0), loc);
    } else if (IS_INTEGER(v)) {
        int64_t int_v = AS_INTEGER(v);
        if (int_v >= -JUMP_OFFSET && int_v <= JUMP_OFFSET)
            emit(make_ABx(OpCode::LOAD_INT, dst, static_cast<uint16_t>(int_v + JUMP_OFFSET)), loc);
        else
            emit(make_ABx(OpCode::LOAD_CONST, dst, currentChunk()->addConstant(v)), loc);
    } else {
        emit(make_ABx(OpCode::LOAD_CONST, dst, currentChunk()->addConstant(v)), loc);
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

    for (size_t i = pop_from; i < locals.size(); ++i) {
        if (locals[i].isCaptured)
            emit(make_ABC(OpCode::CLOSE_UPVALUE, locals[i].reg, 0, 0), loc);
    }

    if (pop_from < locals.size())
        Current_->nextReg = locals[pop_from].reg;

    locals.resize(pop_from);
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
