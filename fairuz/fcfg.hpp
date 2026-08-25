#ifndef FA_CFG_HPP
#define FA_CFG_HPP

#include "fAST.hpp"
#include "farena.hpp"
#include "farray.hpp"
#include "fmacros.hpp"

namespace fairuz {

using StmtPtr = AST::Fa_Stmt*;
using SK = AST::Fa_Stmt::Kind;

/*
BasicBlock:
    id            -- stable integer, assigned in creation order
    stmts         -- an ordered list of straight-line AST statements
    preds         -- list of blocks that can jump/fall into this one
    succs         -- list of blocks this one can jump/fall into
    terminator    -- how this block's execution ends (tag, see below)
    branch_cond   -- the condition expression, only set if terminator is BRANCH

NOTE on preds/succs: these are Fa_Array, not Fa_Set. add_succ() must be
able to record an edge more than once between the same two blocks
without silently deduplicating, and BRANCH order is meaningful:
succs[0] is always the true-target, succs[1] the false-target, by
convention enforced in add_branch_succs() in fcfg.cc.
*/

class Fa_BasicBlock {
public:
    enum class TerminatorTag : int {
        BRANCH,
        JUMP,
        RETURN,
        NORETURN,
        NONE,
    };
    enum class ForRole : int {
        NONE,
        SETUP,
        COND,
        BIND,
        INCREMENT,
    };

    Fa_BasicBlock() = default;

    void set_id(u64 const id) { m_id = id; }
    void append_stmt(AST::Fa_Stmt* const stmt) { m_stmts.push(stmt); }
    void set_terminator(TerminatorTag const tag) { m_terminator = tag; }
    void set_condition(AST::Fa_Expr* const cond) { m_cond = cond; }
    void set_for_role(ForRole role, AST::Fa_ForStmt* stmt)
    {
        m_for_role = role;
        m_for_stmt = stmt;
    }

    // The only way to create an edge. Always mirrors both sides in one
    // atomic step -- there is no separate add_pred. A one-sided edge is
    // a silent graph-correctness bug that every downstream analysis
    // (reachability, dominance, this pass's own block layout) assumes
    // can't happen.
    void add_succ(Fa_BasicBlock* const succ)
    {
        m_succs.push(succ);
        succ->m_preds.push(this);
    }

    u64 get_id() const { return m_id; }
    Fa_Array<AST::Fa_Stmt*> const& get_stmts() const { return m_stmts; }
    Fa_Array<Fa_BasicBlock*> const& get_preds() const { return m_preds; }
    Fa_Array<Fa_BasicBlock*> const& get_succs() const { return m_succs; }
    TerminatorTag get_terminator() const { return m_terminator; }
    AST::Fa_Expr* get_cond() const { return m_cond; }
    ForRole get_for_role() const { return m_for_role; }
    AST::Fa_ForStmt* get_for_stmt() const { return m_for_stmt; }

private:
    ForRole m_for_role { ForRole::NONE };
    AST::Fa_ForStmt* m_for_stmt { nullptr };
    u64 m_id { 0 };
    Fa_Array<AST::Fa_Stmt*> m_stmts;
    Fa_Array<Fa_BasicBlock*> m_preds;
    Fa_Array<Fa_BasicBlock*> m_succs;
    TerminatorTag m_terminator { TerminatorTag::NONE };
    AST::Fa_Expr* m_cond { nullptr };
};

struct Fa_CFG {
    Fa_BasicBlock* entry { nullptr };
    Fa_Array<Fa_BasicBlock*> blocks;
};

struct Fa_CFG_Function {
    AST::Fa_FunctionDef* def { nullptr };
    Fa_CFG* cfg { nullptr };
    AST::Fa_ClassDef* owning_class { nullptr }; // non-null => this is a method
};

class Fa_Program {
public:
    Fa_Program() = default;

    void add_function(Fa_CFG_Function* const f) { m_functions.push(f); }

    Fa_Array<Fa_CFG_Function*> const& get_functions() const { return m_functions; }

    // The top-level script's CFG is the function entry whose def is
    // nullptr -- no separate `main` field to keep in sync by hand.
    Fa_CFG* get_main() const
    {
        for (size_t i = 0; i < m_functions.size(); ++i) {
            if (m_functions[i]->def == nullptr)
                return m_functions[i]->cfg;
        }
        return nullptr;
    }

    Fa_Array<Fa_CFG_Function*> methods_of(AST::Fa_ClassDef* cls) const
    {
        Fa_Array<Fa_CFG_Function*> out;
        for (size_t i = 0; i < m_functions.size(); ++i)
            if (m_functions[i]->owning_class == cls)
                out.push(m_functions[i]);
        return out;
    }

private:
    Fa_Array<Fa_CFG_Function*> m_functions;
};

template<typename... Args>
Fa_BasicBlock* make_basic_block(Args&&... args)
{
    return get_allocator().allocate_object<Fa_BasicBlock>(std::forward<Args>(args)...);
}
template<typename... Args>
Fa_CFG* make_cfg(Args&&... args)
{
    return get_allocator().allocate_object<Fa_CFG>(std::forward<Args>(args)...);
}
template<typename... Args>
Fa_CFG_Function* make_cfg_function(Args&&... args)
{
    return get_allocator().allocate_object<Fa_CFG_Function>(std::forward<Args>(args)...);
}
template<typename... Args>
Fa_Program* make_program(Args&&... args)
{
    return get_allocator().allocate_object<Fa_Program>(std::forward<Args>(args)...);
}

// Lowers ONE statement list (a function body, or the top-level script
// body) into ONE independent Fa_CFG. Does not recurse into nested
// Fa_FunctionDef/Fa_ClassDef bodies -- see lower_program() for lowering
// a whole program including nested functions. Defined in fcfg.cc.
Fa_CFG* lower_to_cfg(Fa_Array<AST::Fa_Stmt*> const& stmts);

// Walks `stmts`, lowers the top-level body into its own Fa_CFG, and
// recursively discovers every Fa_FunctionDef (including class methods)
// reachable from it, lowering each one's body into its own independent
// Fa_CFG as well. This is the entry point main.cpp calls. Defined in
// fcfg.cc.
Fa_Program* lower_program(Fa_Array<AST::Fa_Stmt*> const& stmts);

} // namespace fairuz

#endif // FA_CFG_HPP
