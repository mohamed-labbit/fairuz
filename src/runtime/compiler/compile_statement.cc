#include "../../../include/ast/ast.hpp"
#include "../../../include/ast/printer.hpp"
#include "../../../include/runtime/compiler/compiler.hpp"
#include "../../../include/runtime/runtime_allocator.hpp"

namespace mylang {
namespace runtime {

using namespace ast;

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

}
}
