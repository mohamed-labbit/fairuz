//
// fcfg.cc
//
// Recursive lowering: every lower_* function takes "the block control is
// currently standing in" and returns "the block control stands in
// afterward, or nullptr if there is no afterward" (a Range). Branch/loop
// constructs recurse into their sub-bodies to get a Range back, then wire
// that Range's entry/exit into the surrounding graph.
//
// for/while are NOT given the same shape -- verified against
// fcompiler.cc's compile_for/compile_while/pop_loop:
//   while: continue jumps to the condition re-check (the header).
//   for:   continue jumps to the increment step, a DIFFERENT block from
//          the condition re-check -- continue must skip the per-
//          iteration bind (`target = LIST_GET(iter, index)`).
//

#include "fcfg.hpp"
#include "fAST.hpp"

#include <cassert>

namespace fairuz {

namespace {

// What a lowering function hands back to its caller.
//   entry -- always valid: where control enters this lowered piece.
//   exit  -- where control goes if this piece finishes normally, or
//            nullptr if it never finishes normally (every path
//            returns, or ends in break/continue).
struct Range {
    Fa_BasicBlock* entry { nullptr };
    Fa_BasicBlock* exit { nullptr };
};

// The innermost enclosing loop's two exits, visible during recursion
// via a simple linked stack (push on the way into a loop body, pop on
// the way out). break/continue just read the top of this stack.
struct LoopContext {
    Fa_BasicBlock* continue_target { nullptr };
    Fa_BasicBlock* break_target { nullptr };
    LoopContext const* parent { nullptr };
};

// The only place a BRANCH block's two edges are created. Guarantees
// succs[0] == true_target, succs[1] == false_target by construction.
void add_branch_succs(Fa_BasicBlock* block, AST::Fa_Expr* cond,
    Fa_BasicBlock* true_target, Fa_BasicBlock* false_target)
{
    block->set_condition(cond);
    block->set_terminator(Fa_BasicBlock::TerminatorTag::BRANCH);
    block->add_succ(true_target);
    block->add_succ(false_target);
}

void add_jump_succ(Fa_BasicBlock* block, Fa_BasicBlock* target)
{
    block->set_terminator(Fa_BasicBlock::TerminatorTag::JUMP);
    block->add_succ(target);
}

class Lowerer {
public:
    explicit Lowerer(Fa_CFG& cfg)
        : m_cfg(cfg)
    {
    }

    // Lowers a whole statement list into one Range. entry is always
    // valid; exit is nullptr if control can never fall off the end of
    // this list (every path returns/breaks/continues).
    Range lower_block(Fa_Array<AST::Fa_Stmt*> const& stmts)
    {
        Fa_BasicBlock* first = new_block();
        Fa_BasicBlock* current = first;

        for (size_t i = 0; i < stmts.size(); ++i) {
            if (current == nullptr) {
                // Prior statement already terminated control flow --
                // everything after it is unreachable. Don't lower
                // dead code into real blocks with real edges.
                break;
            }
            current = lower_stmt_into(current, stmts[i]);
        }

        return { first, current };
    }

private:
    Fa_CFG& m_cfg;
    LoopContext const* m_loop_stack { nullptr };

    Fa_BasicBlock* new_block()
    {
        Fa_BasicBlock* b = make_basic_block();
        b->set_id(m_cfg.blocks.size());
        m_cfg.blocks.push(b);
        return b;
    }

    // Lowers one statement into `block`. Returns the block later
    // statements should append into, or nullptr if control can no
    // longer fall through linearly past this statement.
    Fa_BasicBlock* lower_stmt_into(Fa_BasicBlock* block, AST::Fa_Stmt* stmt)
    {
        switch (stmt->get_kind()) {
        case SK::IF:
            return lower_if(block, AS_IF(stmt));
        case SK::WHILE:
            return lower_while(block, AS_WHILE(stmt));
        case SK::FOR:
            return lower_for(block, AS_FOR(stmt));
        case SK::RETURN:
            block->append_stmt(stmt);
            block->set_terminator(Fa_BasicBlock::TerminatorTag::RETURN);
            return nullptr;
        case SK::BREAK:
            return lower_break(block);
        case SK::CONTINUE:
            return lower_continue(block);
        case SK::BLOCK: {
            Range inner = lower_block(AS_BLOCK(stmt)->get_statements());
            add_jump_succ(block, inner.entry);
            return inner.exit;
        }
        default:
            // Straight-line: expr-stmt, assignment, class-def, etc.
            // Fa_FunctionDef also falls here deliberately -- a nested
            // function's body is a separate CFG with its own fresh
            // loop-context, lowered by lower_program's discovery
            // walk, not inline here.
            block->append_stmt(stmt);
            return block;
        }
    }

    // if (cond) { then } [else { else }]
    //
    //        [block: BRANCH on cond]
    //        /                    \
    //  [then entry]           [else entry]   (or straight to merge, if no else)
    //        |                       |
    //  [then exit]             [else exit]
    //        \                      /
    //          [merge]   (created only if at least one path is reachable)
    Fa_BasicBlock* lower_if(Fa_BasicBlock* block, AST::Fa_IfStmt* if_stmt)
    {
        bool has_else = if_stmt->get_else() != nullptr;

        // Both arms are fully lowered BEFORE any wiring decision is
        // made -- we don't know whether a merge block is even needed
        // until we know whether either arm can fall through.
        Range then_range = lower_as_range(if_stmt->get_then());
        Range else_range;
        if (has_else)
            else_range = lower_as_range(if_stmt->get_else());

        bool then_falls = then_range.exit != nullptr;
        bool else_falls = has_else ? (else_range.exit != nullptr) : true;

        if (!then_falls && !else_falls) {
            // Both arms terminate -- only possible when has_else is
            // true (with no else, the implicit false-edge always
            // falls through, forcing else_falls true above).
            assert(has_else);
            add_branch_succs(block, if_stmt->get_condition(), then_range.entry, else_range.entry);
            return nullptr;
        }

        Fa_BasicBlock* merge = new_block();
        Fa_BasicBlock* false_target = has_else ? else_range.entry : merge;
        add_branch_succs(block, if_stmt->get_condition(), then_range.entry, false_target);

        if (then_falls)
            add_jump_succ(then_range.exit, merge);
        if (has_else && else_falls)
            add_jump_succ(else_range.exit, merge);

        return merge;
    }

    // while (cond) { body }
    // continue -> header (the condition re-check itself), matching
    // fcompiler.cc::compile_while's continue_target == loop_start.
    Fa_BasicBlock* lower_while(Fa_BasicBlock* preheader, AST::Fa_WhileStmt* while_stmt)
    {
        Fa_BasicBlock* header = new_block();
        add_jump_succ(preheader, header);

        Fa_BasicBlock* after_loop = new_block();

        LoopContext ctx { /*continue_target=*/header, /*break_target=*/after_loop, m_loop_stack };
        LoopContext const* saved = m_loop_stack;
        m_loop_stack = &ctx;
        Range body_range = lower_as_range(while_stmt->get_body());
        m_loop_stack = saved;

        add_branch_succs(header, while_stmt->get_condition(), body_range.entry, after_loop);

        if (body_range.exit != nullptr)
            add_jump_succ(body_range.exit, header); // back-edge

        return after_loop;
    }

    // for (target in iter) { body }
    // Matches fcompiler.cc::compile_for's index-based desugaring.
    // continue -> the increment step, NOT the condition check -- this
    // is the real asymmetry with while, confirmed against
    // pop_loop's continue_target argument in compile_for.
    Fa_BasicBlock* lower_for(Fa_BasicBlock* preheader, AST::Fa_ForStmt* for_stmt)
    {
        Fa_BasicBlock* cond_block = new_block(); // __for_index < __for_len check; back-edge target
        add_jump_succ(preheader, cond_block);

        Fa_BasicBlock* after_loop = new_block();
        Fa_BasicBlock* bind_block = new_block();      // target = LIST_GET(iter, index); true-edge only
        Fa_BasicBlock* increment_block = new_block(); // __for_index += __for_step; continue-target

        LoopContext ctx { /*continue_target=*/increment_block, /*break_target=*/after_loop, m_loop_stack };
        LoopContext const* saved = m_loop_stack;
        m_loop_stack = &ctx;
        Range body_range = lower_as_range(for_stmt->get_body());
        m_loop_stack = saved;

        // The condition here is synthetic (index < len); for_stmt's
        // iter expr is passed only as debug/provenance, not as the
        // literal comparison to emit -- codegen consuming this CFG
        // must not treat get_cond() on this block as literal.
        add_branch_succs(cond_block, for_stmt->get_iter(), bind_block, after_loop);
        add_jump_succ(bind_block, body_range.entry);

        if (body_range.exit != nullptr)
            add_jump_succ(body_range.exit, increment_block);

        add_jump_succ(increment_block, cond_block); // back-edge

        return after_loop;
    }

    Fa_BasicBlock* lower_break(Fa_BasicBlock* block)
    {
        assert(m_loop_stack != nullptr && "break outside a loop -- should be rejected by parser/sema");
        add_jump_succ(block, m_loop_stack->break_target);
        return nullptr;
    }

    Fa_BasicBlock* lower_continue(Fa_BasicBlock* block)
    {
        assert(m_loop_stack != nullptr && "continue outside a loop -- should be rejected by parser/sema");
        add_jump_succ(block, m_loop_stack->continue_target);
        return nullptr;
    }

    // Lowers a single Fa_Stmt* used as a sub-body (if-then, if-else,
    // loop body) into its own Range, whether it's a Fa_BlockStmt or a
    // single bare statement.
    Range lower_as_range(AST::Fa_Stmt* stmt)
    {
        if (stmt->get_kind() == SK::BLOCK)
            return lower_block(AS_BLOCK(stmt)->get_statements());

        Fa_BasicBlock* b = new_block();
        Fa_BasicBlock* exit = lower_stmt_into(b, stmt);
        return { b, exit };
    }
};

} // namespace

Fa_CFG* lower_to_cfg(Fa_Array<AST::Fa_Stmt*> const& stmts)
{
    Fa_CFG* cfg = make_cfg();
    Lowerer lowerer(*cfg);

    Range top = lowerer.lower_block(stmts);
    cfg->entry = top.entry;

    // If control can fall off the end (no explicit return on every
    // path), that final block still needs an explicit terminator --
    // NONE is never a valid final state.
    if (top.exit != nullptr)
        top.exit->set_terminator(Fa_BasicBlock::TerminatorTag::NORETURN);

    return cfg;
}

namespace {

// Recursively finds every Fa_FunctionDef reachable from `stmts`
// (top-level functions, class methods, and functions nested inside
// other functions' bodies), lowering each one's body into its own
// independent Fa_CFG. Kept separate from Lowerer: Lowerer's job is
// "one statement list -> one CFG", full stop; it must not recurse
// into sibling/nested function bodies itself.
void discover_functions(Fa_Array<AST::Fa_Stmt*> const& stmts, Fa_Program& program, AST::Fa_ClassDef* owning_class = nullptr)
{
    for (size_t i = 0; i < stmts.size(); ++i) {
        AST::Fa_Stmt* stmt = stmts[i];

        switch (stmt->get_kind()) {
        case SK::FUNC: {
            AST::Fa_FunctionDef* fn = AS_FUNCTION_DEF(stmt);
            AST::Fa_Stmt* body = fn->get_body();

            Fa_Array<AST::Fa_Stmt*> body_stmts;
            if (body->get_kind() == SK::BLOCK)
                body_stmts = AS_BLOCK(body)->get_statements();
            else
                body_stmts.push(body);

            Fa_CFG_Function* entry = make_cfg_function();
            entry->def = fn;
            entry->cfg = lower_to_cfg(body_stmts);
            entry->owning_class = owning_class;
            program.add_function(entry);
            discover_functions(body_stmts, program, owning_class);
            break;
        }
        case SK::CLASS_DEF:
            discover_functions(AS_CLASS_DEF(stmt)->get_methods(), program, AS_CLASS_DEF(stmt));
            break;
        case SK::IF: {
            auto* if_stmt = AS_IF(stmt);
            Fa_Array<AST::Fa_Stmt*> single;
            single.push(if_stmt->get_then());
            discover_functions(single, program);
            if (if_stmt->get_else() != nullptr) {
                Fa_Array<AST::Fa_Stmt*> else_single;
                else_single.push(if_stmt->get_else());
                discover_functions(else_single, program);
            }
            break;
        }
        case SK::WHILE: {
            Fa_Array<AST::Fa_Stmt*> single;
            single.push(AS_WHILE(stmt)->get_body());
            discover_functions(single, program);
            break;
        }
        case SK::FOR: {
            Fa_Array<AST::Fa_Stmt*> single;
            single.push(AS_FOR(stmt)->get_body());
            discover_functions(single, program);
            break;
        }
        case SK::BLOCK:
            discover_functions(AS_BLOCK(stmt)->get_statements(), program);
            break;
        default:
            break;
        }
    }
}

} // namespace

Fa_Program* lower_program(Fa_Array<AST::Fa_Stmt*> const& stmts)
{
    Fa_Program* program = make_program();

    Fa_CFG_Function* script_entry = make_cfg_function();
    script_entry->def = nullptr;
    script_entry->cfg = lower_to_cfg(stmts);
    program->add_function(script_entry);

    discover_functions(stmts, *program);

    return program;
}

} // namespace fairuz
