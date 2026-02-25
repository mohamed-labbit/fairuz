#pragma once

#include "../types/string.hpp"
#include "ast_node.hpp"
#include "expr.hpp"

#include <cassert>
#include <functional>
#include <vector>

namespace mylang {
namespace ast {

class Stmt : public ASTNode {
public:
    enum class Kind : int {
        INVALID,
        EXPR,
        ASSIGNMENT,
        IF,
        WHILE,
        FOR,
        FUNC,
        RETURN,
        BLOCK
    };

protected:
    Kind Kind_;

public:
    explicit Stmt()
    {
        Kind_ = Kind::INVALID;
    }

    Kind getKind() const
    {
        return Kind_;
    }

    NodeType getNodeType() const override
    {
        return NodeType::STATEMENT;
    }
};

class BlockStmt : public Stmt {
private:
    std::vector<Stmt*> Statements_;

public:
    explicit BlockStmt() = delete;

    BlockStmt(std::vector<Stmt*> stmts)
        : Statements_(stmts)
    {
        Kind_ = Kind::BLOCK;
    }

    BlockStmt(BlockStmt&&) noexcept = delete;
    BlockStmt(BlockStmt const&) noexcept = delete;

    BlockStmt& operator=(BlockStmt const&) noexcept = delete;

    std::vector<Stmt*> const& getStatements() const
    {
        return Statements_;
    }

    void setStatements(std::vector<Stmt*>& stmts)
    {
        Statements_ = stmts;
    }

    bool isEmpty() const
    {
        return Statements_.empty();
    }
};

class ExprStmt : public Stmt {
private:
    Expr* Expr_ { nullptr };

public:
    explicit ExprStmt() = default;

    ExprStmt(Expr* expr)
        : Expr_(expr)
    {
        Kind_ = Kind::EXPR;
    }

    ExprStmt(ExprStmt&&) noexcept = delete;
    ExprStmt(ExprStmt const&) noexcept = delete;

    ExprStmt& operator=(ExprStmt const&) noexcept = delete;

    Expr* getExpr() const
    {
        return Expr_;
    }

    void setExpr(Expr* e)
    {
        Expr_ = e;
    }
};

class AssignmentStmt : public Stmt {
private:
    Expr* Value_ { nullptr };
    Expr* Target_ { nullptr };

public:
    explicit AssignmentStmt() = delete;

    AssignmentStmt(Expr* target, Expr* value)
        : Target_(target)
        , Value_(value)
    {
        Kind_ = Kind::ASSIGNMENT;
    }

    AssignmentStmt(AssignmentStmt&&) noexcept = delete;
    AssignmentStmt(AssignmentStmt const&) noexcept = delete;

    AssignmentStmt& operator=(AssignmentStmt const&) noexcept = delete;

    Expr* getValue() const
    {
        return Value_;
    }

    Expr* getTarget() const
    {
        return Target_;
    }

    void setValue(Expr* v)
    {
        Value_ = v;
    }

    void setTarget(NameExpr* t)
    {
        Target_ = t;
    }
};

class IfStmt : public Stmt {
private:
    Expr* Condition_ { nullptr };
    BlockStmt* ThenBlock_ { nullptr };
    BlockStmt* ElseBlock_ { nullptr };

public:
    explicit IfStmt() = delete;

    IfStmt(Expr* condition, BlockStmt* then_block, BlockStmt* else_block)
        : Condition_(condition)
        , ThenBlock_(then_block)
        , ElseBlock_(else_block)
    {
        Kind_ = Kind::IF;
    }

    IfStmt(IfStmt&&) noexcept = delete;
    IfStmt(IfStmt const&) noexcept = delete;

    IfStmt& operator=(IfStmt const&) noexcept = delete;

    Expr* getCondition() const
    {
        return Condition_;
    }

    BlockStmt* getThenBlock() const
    {
        return ThenBlock_;
    }

    BlockStmt* getElseBlock() const
    {
        return ElseBlock_;
    }

    void setThenBlock(BlockStmt* t)
    {
        ThenBlock_ = t;
    }

    void setElseBlock(BlockStmt* e)
    {
        ElseBlock_ = e;
    }
};

class WhileStmt : public Stmt {
private:
    Expr* Condition_ { nullptr };
    BlockStmt* Block_ { nullptr };

public:
    explicit WhileStmt() = delete;

    WhileStmt(Expr* condition, BlockStmt* block)
        : Condition_(condition)
        , Block_(block)
    {
        Kind_ = Kind::WHILE;
    }

    WhileStmt(WhileStmt&&) noexcept = delete;
    WhileStmt(WhileStmt const&) noexcept = delete;

    WhileStmt& operator=(WhileStmt const&) noexcept = delete;

    Expr* getCondition() const
    {
        return Condition_;
    }

    BlockStmt const* getBlock() const
    {
        return Block_;
    }

    void setBlock(BlockStmt* b)
    {
        Block_ = b;
    }

    BlockStmt*& getBlockMutable()
    {
        return std::ref<BlockStmt*>(Block_);
    }
};

class ForStmt : public Stmt {
private:
    NameExpr* Target_ { nullptr };
    Expr* Iter_ { nullptr };
    BlockStmt* Block_ { nullptr };

public:
    explicit ForStmt() = delete;

    ForStmt(NameExpr* target, Expr* iter, BlockStmt* block)
        : Target_(target)
        , Iter_(iter)
        , Block_(block)
    {
        Kind_ = Kind::FOR;
    }

    ForStmt(ForStmt&&) noexcept = delete;
    ForStmt(ForStmt const&) noexcept = delete;

    ForStmt& operator=(ForStmt const&) noexcept = delete;

    NameExpr* getTarget() const
    {
        return Target_;
    }

    Expr* getIter() const
    {
        return Iter_;
    }

    BlockStmt* getBlock() const
    {
        return Block_;
    }

    void setBlock(BlockStmt* b)
    {
        Block_ = b;
    }
};

class FunctionDef : public Stmt {
private:
    NameExpr* Name_ { nullptr };
    ListExpr* Params_ { nullptr };
    BlockStmt* Body_ { nullptr };

public:
    explicit FunctionDef() = delete;

    FunctionDef(NameExpr* name, ListExpr* params, BlockStmt* body)
        : Name_(name)
        , Params_(params)
        , Body_(body)
    {
        Kind_ = Kind::FUNC;

        if (!Params_)
            Params_ = AST_allocator.make<ListExpr>(std::vector<Expr*> {});

        if (!Body_)
            Body_ = AST_allocator.make<BlockStmt>(std::vector<Stmt*> {});

        if (!Name_)
            Name_ = AST_allocator.make<NameExpr>("");
    }

    FunctionDef(FunctionDef&&) noexcept = delete;
    FunctionDef(FunctionDef const&) noexcept = delete;

    FunctionDef& operator=(FunctionDef const&) noexcept = delete;

    NameExpr* getName() const
    {
        return Name_;
    }

    std::vector<Expr*> const& getParameters() const
    {
        return Params_->getElements();
    }

    ListExpr* getParameterList() const
    {
        return Params_;
    }

    BlockStmt const* getBody() const
    {
        return Body_;
    }

    void setBody(BlockStmt* b)
    {
        Body_ = b;
    }

    bool hasParameters() const
    {
        return !Params_ || Params_->isEmpty();
    }
};

class ReturnStmt : public Stmt {
private:
    Expr* Value_ { nullptr };

public:
    explicit ReturnStmt() = delete;

    ReturnStmt(Expr* value)
        : Value_(value)
    {
        Kind_ = Kind::RETURN;
    }

    ReturnStmt(ReturnStmt&&) noexcept = delete;
    ReturnStmt(ReturnStmt const&) noexcept = delete;

    ReturnStmt& operator=(ReturnStmt const&) noexcept = delete;

    Expr const* getValue() const
    {
        return Value_;
    }

    void setValue(Expr* v)
    {
        Value_ = v;
    }
};

static constexpr Stmt* makeStmt()
{
    return AST_allocator.make<Stmt>();
}

static constexpr BlockStmt* makeBlock(std::vector<Stmt*> stmts = {})
{
    return AST_allocator.make<BlockStmt>(stmts);
}

static constexpr ExprStmt* makeExprStmt(Expr* expr)
{
    return AST_allocator.make<ExprStmt>(expr);
}

static constexpr AssignmentStmt* makeAssignmentStmt(Expr* target, Expr* value)
{
    return AST_allocator.make<AssignmentStmt>(target, value);
}

static constexpr IfStmt* makeIf(Expr* condition, BlockStmt* then_block, BlockStmt* else_block = nullptr)
{
    return AST_allocator.make<IfStmt>(condition, then_block, else_block);
}

static constexpr WhileStmt* makeWhile(Expr* condition, BlockStmt* block)
{
    return AST_allocator.make<WhileStmt>(condition, block);
}

static constexpr ForStmt* makeFor(NameExpr* target, Expr* iter, BlockStmt* block)
{
    return AST_allocator.make<ForStmt>(target, iter, block);
}

static constexpr FunctionDef* makeFunction(NameExpr* name, ListExpr* params, BlockStmt* body)
{
    return AST_allocator.make<FunctionDef>(name, params, body);
}

static constexpr ReturnStmt* makeReturn(Expr* value)
{
    return AST_allocator.make<ReturnStmt>(value);
}

} // ast
} // mylang
