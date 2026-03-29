/// compiler.cc

#include "../include/compiler.hpp"
#include "../include/optim.hpp"
#include "macros.hpp"

#include "gtest/gtest.h"
#include <algorithm>
#include <cassert>
#include <utility>

namespace fairuz::runtime {

using CompilerError = diagnostic::errc::compiler::Code;

#define SRC_LOC(n) Fa_SourceLocation{ n->getLine(), n->getColumn(), 0 }

static bool isTerminalTopLevelCall(AST::Fa_Stmt const* s)
{
    auto const* expr_stmt = dynamic_cast<AST::Fa_ExprStmt const*>(s);
    if (!expr_stmt)
        return false;
    return dynamic_cast<AST::Fa_CallExpr const*>(expr_stmt->getExpr()) != nullptr;
}

static void patchA(Fa_Chunk* chunk, u32 pc, u8 a)
{
    u32 instr = chunk->code[pc];
    chunk->code[pc] = (instr & 0xFF00FFFFu) | (static_cast<u32>(a) << 16);
}

Fa_Chunk* Compiler::compile(Fa_Array<AST::Fa_Stmt*> const& stmts)
{
    Fa_Chunk* chunk = makeChunk();
    chunk->name = "<main>";

    CompilerState state;
    state.chunk = chunk;
    state.funcName = "<main>";
    state.isTopLevel = true;
    state.enclosing = nullptr;
    Current_ = &state;

    for (size_t i = 0; i < stmts.size(); ++i) {
        AST::Fa_Stmt const* stmt = stmts[i];
        if (i + 1 == stmts.size() && stmt && !state.isDead_ && isTerminalTopLevelCall(stmt)) {
            auto const* expr_stmt = static_cast<AST::Fa_ExprStmt const*>(stmt);
            Fa_SourceLocation loc = SRC_LOC(expr_stmt);
            RegMark mark(Current_);
            u8 src = anyReg(compileExprI(expr_stmt->getExpr()), loc);
            emit(Fa_make_ABC(Fa_OpCode::RETURN, src, 1, 0), loc);
            state.isDead_ = true;
            break;
        }
        compileStmt(stmt);
    }

    Fa_SourceLocation loc = { 1, 1, 0 };
    if (!stmts.empty() && stmts.back())
        loc = SRC_LOC(stmts.back());

    if (!state.isDead_)
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);

    chunk->localCount = state.maxReg;
    chunk->upvalueCount = 0;
    Current_ = nullptr;
    return chunk;
}

void Compiler::compileStmt(AST::Fa_Stmt const* s)
{
    if (!s || Current_->isDead_)
        return;

    switch (s->getKind()) {
    case AST::Fa_Stmt::Kind::BLOCK:
        compileBlock(static_cast<AST::Fa_BlockStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::EXPR:
        compileExprStmt(static_cast<AST::Fa_ExprStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::ASSIGNMENT:
        compileAssignmentStmt(static_cast<AST::Fa_AssignmentStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::IF:
        compileIf(static_cast<AST::Fa_IfStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::WHILE:
        compileWhile(static_cast<AST::Fa_WhileStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::FUNC:
        compileFunctionDef(static_cast<AST::Fa_FunctionDef const*>(s));
        break;
    case AST::Fa_Stmt::Kind::RETURN:
        compileReturn(static_cast<AST::Fa_ReturnStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::FOR:
        compileFor(static_cast<AST::Fa_ForStmt const*>(s));
        break;
    case AST::Fa_Stmt::Kind::INVALID:
        diagnostic::emit(CompilerError::INVALID_STATEMENT_NODE);
        break;
    }
}

void Compiler::compileBlock(AST::Fa_BlockStmt const* s)
{
    beginScope();
    for (AST::Fa_Stmt const* child : s->getStatements())
        compileStmt(child);

    Fa_SourceLocation loc = { 1, 1, 0 };
    if (!s->getStatements().empty() && s->getStatements().back())
        loc = SRC_LOC(s->getStatements().back());
    endScope(loc);
}

void Compiler::compileExprStmt(AST::Fa_ExprStmt const* s)
{
    RegMark mark(Current_);
    u8 tmp = allocRegister();
    Fa_ExprResult r = compileExprI(s->getExpr());
    discharge(r, tmp, SRC_LOC(s));
}

void Compiler::compileAssignmentStmt(AST::Fa_AssignmentStmt const* s)
{
    Fa_SourceLocation loc = SRC_LOC(s);
    auto const* name = dynamic_cast<AST::Fa_NameExpr const*>(s->getTarget());
    if (!name) {
        diagnostic::emit(CompilerError::INVALID_ASSIGNMENT_TARGET, "only simple name assignments are supported");
        return;
    }

    if (s->isDeclaration()) {
        u8 reg = allocRegister();
        Fa_ExprResult value = compileExprI(s->getValue());
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
    u8 src = anyReg(compileExprI(s->getValue()), loc);
    u16 kidx = internString(name->getValue());
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
}

void Compiler::compileIf(AST::Fa_IfStmt const* s)
{
    if (!s)
        return;

    Fa_SourceLocation loc = SRC_LOC(s);
    bool incoming_dead = Current_->isDead_;

    if (auto folded = tryFoldExpr(s->getCondition())) {
        if (Fa_IS_TRUTHY(*folded))
            compileStmt(s->getThen());
        else if (AST::Fa_Stmt* else_stmt = s->getElse())
            compileStmt(else_stmt);
        Current_->isDead_ = incoming_dead;
        return;
    }

    RegMark mark(Current_);
    u8 cond = anyReg(compileExprI(s->getCondition()), loc);
    u32 jump_false = emitJump(Fa_OpCode::JUMP_IF_FALSE, cond, loc);
    compileStmt(s->getThen());

    if (AST::Fa_Stmt* else_stmt = s->getElse()) {
        u32 jump_end = emitJump(Fa_OpCode::JUMP, 0, loc);
        patchJump(jump_false);
        compileStmt(else_stmt);
        patchJump(jump_end);
    } else {
        patchJump(jump_false);
    }

    Current_->isDead_ = incoming_dead;
}

void Compiler::compileWhile(AST::Fa_WhileStmt const* s)
{
    if (!s)
        return;

    Fa_SourceLocation loc = SRC_LOC(s);
    bool incoming_dead = Current_->isDead_;

    if (auto folded = tryFoldExpr(s->getCondition())) {
        if (Fa_IS_TRUTHY(*folded)) {
            u32 loop_start = currentOffset();
            pushLoop(loop_start);
            compileStmt(s->getBody());
            emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(currentOffset()) - 1), loc);
            popLoop(currentOffset(), loop_start, loc.line);
        }
        Current_->isDead_ = incoming_dead;
        return;
    }

    u32 loop_start = currentOffset();
    pushLoop(loop_start);
    {
        RegMark mark(Current_);
        u8 cond = anyReg(compileExprI(s->getCondition()), loc);
        u32 exit_jump = emitJump(Fa_OpCode::JUMP_IF_FALSE, cond, loc);
        compileStmt(s->getBody());
        emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(currentOffset()) - 1), loc);
        patchJump(exit_jump);
    }
    popLoop(currentOffset(), loop_start, loc.line);
    Current_->isDead_ = incoming_dead;
}

void Compiler::compileFunctionDef(AST::Fa_FunctionDef const* f)
{
    Fa_SourceLocation loc = SRC_LOC(f);
    AST::Fa_NameExpr* name = f->getName();
    if (!name) {
        diagnostic::emit(CompilerError::NULL_FUNCTION_NAME, diagnostic::Severity::FATAL);
        return;
    }

    Fa_Chunk* fn_chunk = makeChunk();
    fn_chunk->name = name->getValue();
    fn_chunk->arity = f->hasParameters() ? static_cast<int>(f->getParameters().size()) : 0;
    fn_chunk->upvalueCount = 0;

    auto fn_idx = static_cast<u16>(currentChunk()->functions.size());
    currentChunk()->functions.push(fn_chunk);

    CompilerState fn_state;
    fn_state.chunk = fn_chunk;
    fn_state.funcName = name->getValue();
    fn_state.enclosing = Current_;
    Current_ = &fn_state;

    beginScope();
    if (f->hasParameters()) {
        for (AST::Fa_Expr const* param : f->getParameters()) {
            auto const* param_name = dynamic_cast<AST::Fa_NameExpr const*>(param);
            if (!param_name) {
                diagnostic::emit(CompilerError::INVALID_FUNCTION_PARAMETER);
                continue;
            }
            u8 reg = allocRegister();
            declareLocal(param_name->getValue(), reg);
        }
    }

    compileStmt(f->getBody());
    if (!fn_state.isDead_)
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);

    endScope(loc);
    fn_chunk->localCount = fn_state.maxReg;
    Current_ = fn_state.enclosing;

    u8 dst = allocRegister();
    emit(Fa_make_ABx(Fa_OpCode::CLOSURE, dst, fn_idx), loc);
    if (Current_ && Current_->isTopLevel) {
        u16 name_idx = internString(name->getValue());
        emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, dst, name_idx), loc);
    }
    declareLocal(name->getValue(), dst);
}

void Compiler::compileReturn(AST::Fa_ReturnStmt const* s)
{
    Fa_SourceLocation loc = SRC_LOC(s);

    if (!s->hasValue()) {
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);
        Current_->isDead_ = true;
        return;
    }

    AST::Fa_Expr const* value = s->getValue();
    if (value->getKind() == AST::Fa_Expr::Kind::LITERAL
        && static_cast<AST::Fa_LiteralExpr const*>(value)->isNil()) {
        emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), loc);
        Current_->isDead_ = true;
        return;
    }

    if (value->getKind() == AST::Fa_Expr::Kind::CALL && !Current_->isTopLevel) {
        RegMark mark(Current_);
        compileCallImpl(static_cast<AST::Fa_CallExpr const*>(value), nullptr, true);
        Current_->isDead_ = true;
        return;
    }

    RegMark mark(Current_);
    u8 src = anyReg(compileExprI(value), loc);
    emit(Fa_make_ABC(Fa_OpCode::RETURN, src, 1, 0), loc);
    Current_->isDead_ = true;
}

void Compiler::compileFor(AST::Fa_ForStmt const* s)
{
    Fa_SourceLocation loc = SRC_LOC(s);
    auto const* target = dynamic_cast<AST::Fa_NameExpr const*>(s->getTarget());
    if (!target) {
        diagnostic::emit(CompilerError::INVALID_ASSIGNMENT_TARGET, "for loop target must be a name");
        return;
    }

    bool incoming_dead = Current_->isDead_;

    beginScope();

    u8 iter_reg = allocRegister();
    {
        declareLocal("__for_iter", iter_reg);
        RegMark mark(Current_);
        discharge(compileExprI(s->getIter()), iter_reg, loc);
    }

    u8 len_reg = allocRegister();
    declareLocal("__for_len", len_reg);
    emit(Fa_make_ABC(Fa_OpCode::LIST_LEN, len_reg, iter_reg, 0), loc);

    u8 index_reg = allocRegister();
    declareLocal("__for_index", index_reg);
    emitLoadValue(index_reg, Fa_MAKE_INTEGER(0), loc);

    u8 target_reg = allocRegister();
    declareLocal(target->getValue(), target_reg);

    u8 cond_reg = allocRegister();
    declareLocal("__for_cond", cond_reg);

    u8 step_reg = allocRegister();
    declareLocal("__for_step", step_reg);
    emitLoadValue(step_reg, Fa_MAKE_INTEGER(1), loc);

    u32 loop_start = currentOffset();
    pushLoop(loop_start);

    emit(Fa_make_ABC(Fa_OpCode::OP_LT, cond_reg, index_reg, len_reg), loc);
    emit(Fa_make_ABC(Fa_OpCode::NOP, currentChunk()->allocIcSlot(), 0, 0), loc);
    u32 exit_jump = emitJump(Fa_OpCode::JUMP_IF_FALSE, cond_reg, loc);

    emit(Fa_make_ABC(Fa_OpCode::LIST_GET, target_reg, iter_reg, index_reg), loc);
    compileStmt(s->getBody());

    u32 continue_target = currentOffset();
    emit(Fa_make_ABC(Fa_OpCode::OP_ADD, index_reg, index_reg, step_reg), loc);
    emit(Fa_make_ABC(Fa_OpCode::NOP, currentChunk()->allocIcSlot(), 0, 0), loc);
    emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, static_cast<i32>(loop_start) - static_cast<i32>(currentOffset()) - 1), loc);

    patchJump(exit_jump);
    popLoop(currentOffset(), continue_target, loc.line);
    endScope(loc);

    Current_->isDead_ = incoming_dead;
}

Fa_ExprResult Compiler::compileExprI(AST::Fa_Expr const* e)
{
    if (!e)
        return Fa_ExprResult::knil();

    switch (e->getKind()) {
    case AST::Fa_Expr::Kind::LITERAL:
        return compileLiteralI(static_cast<AST::Fa_LiteralExpr const*>(e));
    case AST::Fa_Expr::Kind::NAME:
        return compileNameI(static_cast<AST::Fa_NameExpr const*>(e));
    case AST::Fa_Expr::Kind::UNARY:
        return compileUnaryI(static_cast<AST::Fa_UnaryExpr const*>(e));
    case AST::Fa_Expr::Kind::BINARY:
        return compileBinaryI(static_cast<AST::Fa_BinaryExpr const*>(e));
    case AST::Fa_Expr::Kind::ASSIGNMENT:
        return compileAssignI(static_cast<AST::Fa_AssignmentExpr const*>(e));
    case AST::Fa_Expr::Kind::CALL:
        return compileCallImpl(static_cast<AST::Fa_CallExpr const*>(e), nullptr, false);
    case AST::Fa_Expr::Kind::LIST:
        return compileListI(static_cast<AST::Fa_ListExpr const*>(e));
    case AST::Fa_Expr::Kind::INDEX:
        return compileIndexI(static_cast<AST::Fa_IndexExpr const*>(e));
    case AST::Fa_Expr::Kind::INVALID:
        diagnostic::emit(CompilerError::INVALID_EXPRESSION_NODE);
        return Fa_ExprResult::knil();
    }

    return Fa_ExprResult::knil();
}

Fa_ExprResult Compiler::compileLiteralI(AST::Fa_LiteralExpr const* e)
{
    if (e->isString()) {
        u16 kidx = internString(e->getStr());
        u32 pc = emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, 0, kidx), SRC_LOC(e));
        return Fa_ExprResult::reloc(pc);
    }
    if (e->isInteger())
        return Fa_ExprResult::kint(e->getInt());
    if (e->isDecimal())
        return Fa_ExprResult::kfloat(e->getFloat());
    if (e->isBoolean())
        return Fa_ExprResult::kbool(e->getBool());
    if (e->isNil())
        return Fa_ExprResult::knil();

    diagnostic::emit(CompilerError::UNKNOWN_LITERAL_TYPE);
    return Fa_ExprResult::knil();
}

Fa_ExprResult Compiler::compileNameI(AST::Fa_NameExpr const* e)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    VarInfo vi = resolveName(e->getValue());

    if (vi.kind == VarInfo::Kind::LOCAL)
        return Fa_ExprResult::reg(vi.index);

    u16 kidx = internString(e->getValue());
    u32 pc = emit(Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL, 0, kidx), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ExprResult Compiler::compileUnaryI(AST::Fa_UnaryExpr const* e)
{
    Fa_SourceLocation loc = SRC_LOC(e);

    if (auto folded = tryFoldUnary(e)) {
        Fa_Value v = *folded;
        if (Fa_IS_INTEGER(v))
            return Fa_ExprResult::kint(Fa_AS_INTEGER(v));
        if (Fa_IS_DOUBLE(v))
            return Fa_ExprResult::kfloat(Fa_AS_DOUBLE(v));
        if (Fa_IS_BOOL(v))
            return Fa_ExprResult::kbool(Fa_AS_BOOL(v));
        if (Fa_IS_NIL(v))
            return Fa_ExprResult::knil();
    }

    Fa_OpCode op = Fa_OpCode::NOP;
    switch (e->getOperator()) {
    case AST::Fa_UnaryOp::OP_NEG:
        op = Fa_OpCode::OP_NEG;
        break;
    case AST::Fa_UnaryOp::OP_BITNOT:
        op = Fa_OpCode::OP_BITNOT;
        break;
    case AST::Fa_UnaryOp::OP_NOT:
        op = Fa_OpCode::OP_NOT;
        break;
    default:
        diagnostic::emit(CompilerError::UNKNOWN_UNARY_OPERATOR, diagnostic::Severity::FATAL);
        return Fa_ExprResult::knil();
    }

    RegMark mark(Current_);
    u8 src = anyReg(compileExprI(e->getOperand()), loc);
    u32 pc = emit(Fa_make_ABC(op, 0, src, 0), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ExprResult Compiler::compileBinaryI(AST::Fa_BinaryExpr const* e)
{
    Fa_SourceLocation loc = SRC_LOC(e);

    if (auto folded = tryFoldBinary(e)) {
        Fa_Value v = *folded;
        if (Fa_IS_INTEGER(v))
            return Fa_ExprResult::kint(Fa_AS_INTEGER(v));
        if (Fa_IS_DOUBLE(v))
            return Fa_ExprResult::kfloat(Fa_AS_DOUBLE(v));
        if (Fa_IS_BOOL(v))
            return Fa_ExprResult::kbool(Fa_AS_BOOL(v));
        if (Fa_IS_NIL(v))
            return Fa_ExprResult::knil();
    }

    AST::Fa_BinaryOp op = e->getOperator();
    if (op == AST::Fa_BinaryOp::OP_AND) {
        u8 dst = allocRegister();
        {
            RegMark mark(Current_);
            discharge(compileExprI(e->getLeft()), dst, loc);
        }
        u32 skip = emitJump(Fa_OpCode::JUMP_IF_FALSE, dst, loc);
        {
            RegMark mark(Current_);
            discharge(compileExprI(e->getRight()), dst, loc);
        }
        patchJump(skip);
        return Fa_ExprResult::reg(dst);
    }

    if (op == AST::Fa_BinaryOp::OP_OR) {
        u8 dst = allocRegister();
        {
            RegMark mark(Current_);
            discharge(compileExprI(e->getLeft()), dst, loc);
        }
        u32 skip = emitJump(Fa_OpCode::JUMP_IF_TRUE, dst, loc);
        {
            RegMark mark(Current_);
            discharge(compileExprI(e->getRight()), dst, loc);
        }
        patchJump(skip);
        return Fa_ExprResult::reg(dst);
    }

    Fa_OpCode bc_op = Fa_OpCode::NOP;
    bool swapped = false;
    switch (op) {
    case AST::Fa_BinaryOp::OP_ADD:
        bc_op = Fa_OpCode::OP_ADD;
        break;
    case AST::Fa_BinaryOp::OP_SUB:
        bc_op = Fa_OpCode::OP_SUB;
        break;
    case AST::Fa_BinaryOp::OP_MUL:
        bc_op = Fa_OpCode::OP_MUL;
        break;
    case AST::Fa_BinaryOp::OP_DIV:
        bc_op = Fa_OpCode::OP_DIV;
        break;
    case AST::Fa_BinaryOp::OP_MOD:
        bc_op = Fa_OpCode::OP_MOD;
        break;
    case AST::Fa_BinaryOp::OP_POW:
        bc_op = Fa_OpCode::OP_POW;
        break;
    case AST::Fa_BinaryOp::OP_EQ:
        bc_op = Fa_OpCode::OP_EQ;
        break;
    case AST::Fa_BinaryOp::OP_NEQ:
        bc_op = Fa_OpCode::OP_NEQ;
        break;
    case AST::Fa_BinaryOp::OP_LT:
        bc_op = Fa_OpCode::OP_LT;
        break;
    case AST::Fa_BinaryOp::OP_LTE:
        bc_op = Fa_OpCode::OP_LTE;
        break;
    case AST::Fa_BinaryOp::OP_GT:
        bc_op = Fa_OpCode::OP_LT;
        swapped = true;
        break;
    case AST::Fa_BinaryOp::OP_GTE:
        bc_op = Fa_OpCode::OP_LTE;
        swapped = true;
        break;
    case AST::Fa_BinaryOp::OP_BITAND:
        bc_op = Fa_OpCode::OP_BITAND;
        break;
    case AST::Fa_BinaryOp::OP_BITOR:
        bc_op = Fa_OpCode::OP_BITOR;
        break;
    case AST::Fa_BinaryOp::OP_BITXOR:
        bc_op = Fa_OpCode::OP_BITXOR;
        break;
    case AST::Fa_BinaryOp::OP_LSHIFT:
        bc_op = Fa_OpCode::OP_LSHIFT;
        break;
    case AST::Fa_BinaryOp::OP_RSHIFT:
        bc_op = Fa_OpCode::OP_RSHIFT;
        break;
    default:
        diagnostic::emit(CompilerError::UNKNOWN_BINARY_OPERATOR, diagnostic::Severity::FATAL);
        return Fa_ExprResult::knil();
    }

    if (bc_op == Fa_OpCode::OP_LSHIFT || bc_op == Fa_OpCode::OP_RSHIFT) {
        auto const* amount_expr = dynamic_cast<AST::Fa_LiteralExpr const*>(e->getRight());
        if (!amount_expr || !amount_expr->isInteger()) {
            diagnostic::emit(CompilerError::SHIFT_AMOUNT_NOT_CONSTANT);
            return Fa_ExprResult::knil();
        }
        i64 amount = amount_expr->getInt();
        if (amount < 0 || amount > 63) {
            diagnostic::emit(CompilerError::SHIFT_AMOUNT_OUT_OF_RANGE, std::to_string(amount));
            return Fa_ExprResult::knil();
        }

        RegMark mark(Current_);
        u8 lhs = anyReg(compileExprI(e->getLeft()), loc);
        u32 pc = emit(Fa_make_ABC(bc_op, 0, lhs, static_cast<u8>(amount)), loc);
        u8 ic = currentChunk()->allocIcSlot();
        emit(Fa_make_ABC(Fa_OpCode::NOP, ic, 0, 0), loc);
        return Fa_ExprResult::reloc(pc);
    }

    RegMark mark(Current_);
    u8 lhs = anyReg(compileExprI(e->getLeft()), loc);
    u8 rhs = anyReg(compileExprI(e->getRight()), loc);
    if (swapped)
        std::swap(lhs, rhs);

    u32 pc = emit(Fa_make_ABC(bc_op, 0, lhs, rhs), loc);
    u8 ic = currentChunk()->allocIcSlot();
    emit(Fa_make_ABC(Fa_OpCode::NOP, ic, 0, 0), loc);
    return Fa_ExprResult::reloc(pc);
}

Fa_ExprResult Compiler::compileAssignI(AST::Fa_AssignmentExpr const* e)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    AST::Fa_Expr const* target = e->getTarget();

    if (target->getKind() == AST::Fa_Expr::Kind::INDEX) {
        auto index_expr = static_cast<AST::Fa_IndexExpr const*>(target);
        RegMark mark(Current_);
        u8 list_reg = anyReg(compileExprI(index_expr->getObject()), loc);
        u8 index_reg = anyReg(compileExprI(index_expr->getIndex()), loc);
        u8 value_reg = anyReg(compileExprI(e->getValue()), loc);
        emit(Fa_make_ABC(Fa_OpCode::LIST_SET, list_reg, index_reg, value_reg), loc);
        return Fa_ExprResult::reg(value_reg);
    }

    auto const* name = dynamic_cast<AST::Fa_NameExpr const*>(target);
    if (!name) {
        diagnostic::emit(CompilerError::INVALID_ASSIGNMENT_TARGET);
        return Fa_ExprResult::knil();
    }

    VarInfo vi = resolveName(name->getValue());
    if (vi.kind == VarInfo::Kind::LOCAL) {
        compileExpr(e->getValue(), &vi.index);
        return Fa_ExprResult::reg(vi.index);
    }

    RegMark mark(Current_);
    u8 src = anyReg(compileExprI(e->getValue()), loc);
    u16 kidx = internString(name->getValue());
    emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, src, kidx), loc);
    return Fa_ExprResult::reg(src);
}

Fa_ExprResult Compiler::compileCallImpl(AST::Fa_CallExpr const* e, u8* dst, bool tail)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    u8 fn_reg = dst ? *dst : allocRegister();

    discharge(compileExprI(e->getCallee()), fn_reg, loc);

    for (AST::Fa_Expr const* arg : e->getArgs()) {
        u8 arg_reg = allocRegister();
        discharge(compileExprI(arg), arg_reg, loc);
    }

    u8 argc = static_cast<u8>(e->getArgs().size());
    if (tail && !Current_->isTopLevel) {
        emit(Fa_make_ABC(Fa_OpCode::CALL_TAIL, fn_reg, argc, 0), loc);
        Current_->freeRegsTo(fn_reg);
        return Fa_ExprResult::reg(fn_reg);
    }

    u8 ic = currentChunk()->allocIcSlot();
    emit(Fa_make_ABC(Fa_OpCode::IC_CALL, fn_reg, argc, ic), loc);
    Current_->freeRegsTo(fn_reg + 1);
    return Fa_ExprResult::reg(fn_reg);
}

Fa_ExprResult Compiler::compileListI(AST::Fa_ListExpr const* e)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    u8 dst = allocRegister();
    auto cap = static_cast<u8>(std::min<u32>(e->size(), 0xFF));
    emit(Fa_make_ABC(Fa_OpCode::LIST_NEW, dst, cap, 0), loc);

    RegMark mark(Current_);
    for (AST::Fa_Expr const* elem : e->getElements()) {
        u8 reg = anyReg(compileExprI(elem), loc);
        emit(Fa_make_ABC(Fa_OpCode::LIST_APPEND, dst, reg, 0), loc);
    }

    return Fa_ExprResult::reg(dst);
}

Fa_ExprResult Compiler::compileIndexI(AST::Fa_IndexExpr const* e)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    RegMark mark(Current_);
    u8 list_reg = anyReg(compileExprI(e->getObject()), loc);
    u8 index_reg = anyReg(compileExprI(e->getIndex()), loc);
    u32 pc = emit(Fa_make_ABC(Fa_OpCode::LIST_GET, 0, list_reg, index_reg), loc);
    return Fa_ExprResult::reloc(pc);
}

void Compiler::discharge(Fa_ExprResult const& r, u8 dst, Fa_SourceLocation loc)
{
    switch (r.kind) {
    case Fa_ExprResult::Kind::REG:
        if (r.reg_ != dst)
            emit(Fa_make_ABC(Fa_OpCode::MOVE, dst, r.reg_, 0), loc);
        break;
    case Fa_ExprResult::Kind::RELOC:
        patchA(currentChunk(), r.reloc_pc, dst);
        break;
    case Fa_ExprResult::Kind::KINT:
        emitLoadValue(dst, Fa_MAKE_INTEGER(r.ival), loc);
        break;
    case Fa_ExprResult::Kind::KFLOAT:
        emitLoadValue(dst, Fa_MAKE_REAL(r.dval), loc);
        break;
    case Fa_ExprResult::Kind::KBOOL:
        emitLoadValue(dst, Fa_MAKE_BOOL(r.bval), loc);
        break;
    case Fa_ExprResult::Kind::KNIL:
        emitLoadValue(dst, NIL_VAL, loc);
        break;
    }
}

u8 Compiler::anyReg(Fa_ExprResult const& r, Fa_SourceLocation loc)
{
    if (r.kind == Fa_ExprResult::Kind::REG)
        return r.reg_;

    u8 dst = allocRegister();
    discharge(r, dst, loc);
    return dst;
}

u8 Compiler::compileExpr(AST::Fa_Expr const* e, u8* dst)
{
    if (!e)
        return errorReg();

    Fa_SourceLocation loc = SRC_LOC(e);
    Fa_ExprResult r = compileExprI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

u8 Compiler::compileLiteral(AST::Fa_LiteralExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    Fa_ExprResult r = compileLiteralI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

u8 Compiler::compileName(AST::Fa_NameExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    Fa_ExprResult r = compileNameI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

u8 Compiler::compileUnary(AST::Fa_UnaryExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    Fa_ExprResult r = compileUnaryI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

u8 Compiler::compileBinary(AST::Fa_BinaryExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    Fa_ExprResult r = compileBinaryI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

u8 Compiler::compileAssignmentExpr(AST::Fa_AssignmentExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    Fa_ExprResult r = compileAssignI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

u8 Compiler::compileCall(AST::Fa_CallExpr const* e, u8* dst, bool tail)
{
    Fa_ExprResult r = compileCallImpl(e, dst, tail);
    return r.reg_;
}

u8 Compiler::compileList(AST::Fa_ListExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    Fa_ExprResult r = compileListI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

u8 Compiler::compileIndex(AST::Fa_IndexExpr const* e, u8* dst)
{
    Fa_SourceLocation loc = SRC_LOC(e);
    Fa_ExprResult r = compileIndexI(e);
    if (dst) {
        discharge(r, *dst, loc);
        return *dst;
    }
    return anyReg(r, loc);
}

u8 Compiler::errorReg() const
{
    return 0;
}

u8 Compiler::ensureReg(u8 const* reg)
{
    return reg ? *reg : allocRegister();
}

u8 Compiler::allocRegister()
{
    u8 reg = Current_->allocRegister();
    if (reg >= MAX_REGS) {
        diagnostic::emit(CompilerError::TOO_MANY_REGISTERS, std::string(Current_->funcName.data(), Current_->funcName.len()));
        return 0;
    }
    return reg;
}

void Compiler::declareLocal(Fa_StringRef const& name, u8 reg)
{
    Current_->locals.push({ name, Current_->scopeDepth, reg });
}

Compiler::VarInfo Compiler::resolveName(Fa_StringRef const& name)
{
    auto const& locals = Current_->locals;
    for (auto i = static_cast<int>(locals.size()) - 1; i >= 0; --i) {
        if (locals[i].name == name)
            return { VarInfo::Kind::LOCAL, locals[i].reg };
    }

    return { VarInfo::Kind::GLOBAL, 0 };
}

int Compiler::resolveUpvalue(CompilerState* state, Fa_StringRef const& name)
{
    (void)state;
    (void)name;
    return -1;
}

int Compiler::addUpvalue(CompilerState* state, bool is_local, u8 index)
{
    (void)state;
    (void)is_local;
    (void)index;
    return -1;
}

u32 Compiler::emit(u32 instr, Fa_SourceLocation loc)
{
    return currentChunk()->emit(instr, loc);
}

u32 Compiler::emitJump(Fa_OpCode op, u8 cond, Fa_SourceLocation loc)
{
    return emit(Fa_make_AsBx(op, cond, 0), loc);
}

void Compiler::patchJump(u32 idx)
{
    if (!currentChunk()->patchJump(idx))
        diagnostic::emit(CompilerError::JUMP_OFFSET_OVERFLOW, diagnostic::Severity::FATAL);
}

void Compiler::pushLoop(u32 loop_start)
{
    Current_->loopStack.push({ { }, { }, loop_start });
}

void Compiler::popLoop(u32 loop_exit, u32 continue_target, u32 line)
{
    (void)line;
    assert(!Current_->loopStack.empty());
    auto& ctx = Current_->loopStack.back();

    for (u32 idx : ctx.breakPatches)
        patchJumpTo(idx, loop_exit);
    for (u32 idx : ctx.continuePatches)
        patchJumpTo(idx, continue_target);

    Current_->loopStack.pop();
}

void Compiler::patchJumpTo(u32 instr_idx, u32 target)
{
    auto offset = static_cast<i32>(target) - static_cast<i32>(instr_idx) - 1;
    if (offset > JUMP_OFFSET || offset < -JUMP_OFFSET)
        diagnostic::emit(CompilerError::LOOP_JUMP_OFFSET_OVERFLOW, diagnostic::Severity::FATAL);

    u32 word = currentChunk()->code[instr_idx];
    currentChunk()->code[instr_idx] = Fa_make_AsBx(Fa_instr_op(word), Fa_instr_A(word), offset);
}

void Compiler::emitLoadValue(u8 dst, Fa_Value v, Fa_SourceLocation loc)
{
    if (Fa_IS_NIL(v)) {
        emit(Fa_make_ABC(Fa_OpCode::LOAD_NIL, dst, dst, 1), loc);
        return;
    }

    if (Fa_IS_BOOL(v)) {
        emit(Fa_make_ABC(Fa_AS_BOOL(v) ? Fa_OpCode::LOAD_TRUE : Fa_OpCode::LOAD_FALSE, dst, 0, 0), loc);
        return;
    }

    if (Fa_IS_INTEGER(v)) {
        i64 iv = Fa_AS_INTEGER(v);
        if (iv >= -JUMP_OFFSET && iv <= JUMP_OFFSET) {
            emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, dst, static_cast<u16>(iv + JUMP_OFFSET)), loc);
            return;
        }
    }

    emit(Fa_make_ABx(Fa_OpCode::LOAD_CONST, dst, currentChunk()->addConstant(v)), loc);
}

Fa_Chunk* Compiler::currentChunk() const
{
    return Current_->chunk;
}

u32 Compiler::currentOffset() const
{
    return currentChunk()->code.size();
}

void Compiler::beginScope()
{
    ++Current_->scopeDepth;
}

void Compiler::endScope(Fa_SourceLocation loc)
{
    (void)loc;
    unsigned int depth = --Current_->scopeDepth;
    auto& locals = Current_->locals;
    size_t pop_from = locals.size();

    while (pop_from > 0 && locals[pop_from - 1].depth > depth)
        --pop_from;

    if (pop_from < locals.size())
        Current_->nextReg = locals[pop_from].reg;

    locals.resize(static_cast<u32>(pop_from));
}

u32 Compiler::internString(Fa_StringRef const& str)
{
    Fa_Chunk* chunk = currentChunk();
    auto key = std::make_pair(str, chunk);
    /*
     * 
    auto it = StringCache_.find(key);
    if (it != StringCache_.end())
        return it->second;
    */
    
    auto it = StringCache_.findPtr(key);

    Fa_ObjString* obj = Fa_MAKE_OBJ_STRING(str);
    u16 idx = chunk->addConstant(Fa_MAKE_OBJECT(obj));
    StringCache_[key] = idx;
    return idx;
}

} // namespace fairuz::runtime
