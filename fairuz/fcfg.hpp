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

enum class TerminatorTag : int {
    BRANCH,
    JUMP,
    RETURN,
    NORETURN,
    NONE,
};

struct Fa_BasicBlock {
    u64 id { 0 };
    Fa_Array<AST::Fa_Stmt*> stmts;
    Fa_Array<Fa_BasicBlock*> preds;
    Fa_Array<Fa_BasicBlock*> succs;
    TerminatorTag terminator { TerminatorTag::NONE };
    AST::Fa_Expr* cond { nullptr };
    u32 scope_depth { 0 };

    Fa_BasicBlock() = default;

    void add_succ(Fa_BasicBlock* const succ)
    {
        succs.push(succ);
        succ->preds.push(this);
    }
};

struct Fa_CFG {
    Fa_BasicBlock* entry { nullptr };
    Fa_Array<Fa_BasicBlock*> blocks;
};

struct Fa_CFG_Function {
    AST::Fa_FunctionDef* def { nullptr };
    Fa_CFG* cfg { nullptr };
    AST::Fa_ClassDef* owning_class { nullptr }; // non-null => method
};

class Fa_Program {
public:
    Fa_Program() = default;

    void add_function(Fa_CFG_Function* const f)
    {
        assert(f->def != nullptr);
        assert(f->cfg != nullptr);
        m_functions.push(f);
    }

    Fa_Array<Fa_CFG_Function*> const& get_functions() const { return m_functions; }
    Fa_CFG* get_main() const { return m_main; }
    void set_main(Fa_CFG* m) { m_main = m; }

    Fa_Array<Fa_CFG_Function*> methods_of(AST::Fa_ClassDef* cls) const
    {
        Fa_Array<Fa_CFG_Function*> out;
        for (size_t i = 0; i < m_functions.size(); ++i) {
            if (m_functions[i]->owning_class->equals(cls))
                out.push(m_functions[i]);
        }
        return out;
    }

    Fa_CFG_Function* cfg_of_method(AST::Fa_ClassDef* cls, Fa_StringRef const& method_name) const
    {
        for (size_t i = 0; i < m_functions.size(); ++i) {
            if (m_functions[i]->owning_class != cls)
                continue;
            if (m_functions[i]->def->get_name()->get_value() == method_name)
                return m_functions[i];
        }
        return nullptr;
    }

private:
    Fa_Array<Fa_CFG_Function*> m_functions;
    Fa_CFG* m_main;
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
Fa_CFG* lower_to_cfg(Fa_Array<AST::Fa_Stmt*> const& stmts, u32 base_scope_depth = 0);

// Walks `stmts`, lowers the top-level body into its own Fa_CFG, and
// recursively discovers every Fa_FunctionDef (including class methods)
// reachable from it, lowering each one's body into its own independent
// Fa_CFG as well. This is the entry point main.cpp calls. Defined in
// fcfg.cc.
Fa_Program* lower_program(Fa_Array<AST::Fa_Stmt*> const& stmts);

} // namespace fairuz

#endif // FA_CFG_HPP
