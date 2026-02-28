#pragma once

#include "../types/string.hpp"
#include "ast_node.hpp"
#include "expr.hpp"

#include <cassert>
#include <functional>
#include <vector>

namespace mylang {
namespace ast {

// forward
class Stmt;
class BlockStmt;
class ExprStmt;
class AssignmentStmt;
class IfStmt;
class WhileStmt;
class ForStmt;
class FunctionDef;
class ReturnStmt;

static constexpr BlockStmt* makeBlock(std::vector<Stmt*> stmts = {});
static constexpr ExprStmt* makeExprStmt(Expr* expr);
static constexpr AssignmentStmt* makeAssignmentStmt(Expr* target, Expr* value, bool decl = false);
static constexpr IfStmt* makeIf(Expr* condition, BlockStmt* then_block, BlockStmt* else_block = nullptr);
static constexpr WhileStmt* makeWhile(Expr* condition, BlockStmt* block);
static constexpr ForStmt* makeFor(NameExpr* target, Expr* iter, BlockStmt* block);
static constexpr FunctionDef* makeFunction(NameExpr* name, ListExpr* params, BlockStmt* body);
static constexpr ReturnStmt* makeReturn(Expr* value);

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
    Kind Kind_ { Kind::INVALID };

public:
    explicit Stmt() = default;

    virtual Stmt* clone() const = 0;

    virtual bool equals(Stmt const* other) const = 0;

    Kind getKind() const { return Kind_; }

    NodeType getNodeType() const override { return NodeType::STATEMENT; }
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

    BlockStmt* clone() const override { return makeBlock(Statements_); }

    bool equals(Stmt const* other) const override
    {
        if (!other || other->getKind() != Kind::BLOCK)
            return false;

        BlockStmt const* block = static_cast<BlockStmt const*>(other);

        if (Statements_.size() != block->Statements_.size())
            return false;

        for (size_t i = 0; i < Statements_.size(); ++i)
            if (!Statements_[i]->equals(block->Statements_[i]))
                return false;

        return true;
    }

    std::vector<Stmt*> const& getStatements() const { return Statements_; }

    void setStatements(std::vector<Stmt*>& stmts) { Statements_ = stmts; }

    bool isEmpty() const { return Statements_.empty(); }
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

    ExprStmt* clone() const override { return makeExprStmt(Expr_->clone()); }

    bool equals(Stmt const* other) const override
    {
        if (Kind_ != other->getKind())
            return false;

        ExprStmt const* block = dynamic_cast<ExprStmt const*>(other);
        return Expr_->equals(block->getExpr());
    }

    Expr* getExpr() const { return Expr_; }

    void setExpr(Expr* e) { Expr_ = e; }
};

class AssignmentStmt : public Stmt {
private:
    Expr* Value_ { nullptr };
    Expr* Target_ { nullptr };

    bool isDecl_ { false };

public:
    explicit AssignmentStmt() = delete;

    AssignmentStmt(Expr* target, Expr* value, bool decl = false)
        : Target_(target)
        , Value_(value)
        , isDecl_(decl)
    {
        Kind_ = Kind::ASSIGNMENT;
    }

    AssignmentStmt(AssignmentStmt&&) noexcept = delete;
    AssignmentStmt(AssignmentStmt const&) noexcept = delete;

    AssignmentStmt& operator=(AssignmentStmt const&) noexcept = delete;

    AssignmentStmt* clone() const override { return makeAssignmentStmt(Target_->clone(), Target_->clone()); }

    bool equals(Stmt const* other) const override
    {
        if (Kind_ != other->getKind())
            return false;

        AssignmentStmt const* block = dynamic_cast<AssignmentStmt const*>(other);
        return Value_->equals(block->getValue()) && Target_->equals(block->getTarget());
    }

    Expr* getValue() const { return Value_; }
    Expr* getTarget() const { return Target_; }

    void setValue(Expr* v) { Value_ = v; }
    void setTarget(NameExpr* t) { Target_ = t; }

    bool isDeclaration() const { return isDecl_; }
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

    IfStmt* clone() const override { return makeIf(Condition_->clone(), ThenBlock_->clone(), ElseBlock_->clone()); }

    bool equals(Stmt const* other) const override
    {
        if (Kind_ != other->getKind())
            return false;

        IfStmt const* block = dynamic_cast<IfStmt const*>(other);
        return Condition_->equals(block->getCondition())
            && ThenBlock_->equals(block->getThenBlock())
            && ElseBlock_->equals(block->getElseBlock());
    }

    Expr* getCondition() const { return Condition_; }

    BlockStmt* getThenBlock() const { return ThenBlock_; }
    BlockStmt* getElseBlock() const { return ElseBlock_; }

    void setThenBlock(BlockStmt* t) { ThenBlock_ = t; }
    void setElseBlock(BlockStmt* e) { ElseBlock_ = e; }
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

    WhileStmt* clone() const override { return makeWhile(Condition_->clone(), Block_->clone()); }

    bool equals(Stmt const* other) const override
    {
        if (Kind_ != other->getKind())
            return false;

        WhileStmt const* block = dynamic_cast<WhileStmt const*>(other);
        return Condition_->equals(block->getCondition()) && Block_->equals(block->getBlock());
    }

    Expr* getCondition() const { return Condition_; }

    BlockStmt* getBlock() { return Block_; }
    BlockStmt const* getBlock() const { return Block_; }

    void setBlock(BlockStmt* b) { Block_ = b; }
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

    ForStmt* clone() const override
    {
        return AST_allocator.allocateObject<ForStmt>(Target_->clone(), Iter_->clone(), Block_->clone());
    }

    bool equals(Stmt const* other) const override
    {
        if (Kind_ != other->getKind())
            return false;

        ForStmt const* block = dynamic_cast<ForStmt const*>(other);
        return Target_->equals(block->getTarget())
            && Iter_->equals(block->getIter())
            && Block_->equals(block->getBlock());
    }

    NameExpr* getTarget() const { return Target_; }

    Expr* getIter() const { return Iter_; }

    BlockStmt* getBlock() const { return Block_; }

    void setBlock(BlockStmt* b) { Block_ = b; }
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
            Params_ = makeList(std::vector<Expr*> {});
        if (!Body_)
            Body_ = makeBlock(std::vector<Stmt*> {});
        if (!Name_)
            Name_ = makeName("");
    }

    FunctionDef(FunctionDef&&) noexcept = delete;
    FunctionDef(FunctionDef const&) noexcept = delete;

    FunctionDef& operator=(FunctionDef const&) noexcept = delete;

    FunctionDef* clone() const override { return makeFunction(Name_->clone(), Params_->clone(), Body_->clone()); }

    bool equals(Stmt const* other) const override
    {
        if (Kind_ != other->getKind())
            return false;

        FunctionDef const* block = dynamic_cast<FunctionDef const*>(other);
        return Name_->equals(block->getName())
            && Params_->equals(block->getParameterList())
            && Body_->equals(block->getBody());
    }

    NameExpr* getName() const { return Name_; }

    std::vector<Expr*> const& getParameters() const { return Params_->getElements(); }

    ListExpr* getParameterList() const { return Params_; }

    BlockStmt* getBody() const { return Body_; }

    void setBody(BlockStmt* b) { Body_ = b; }

    bool hasParameters() const { return Params_ && !Params_->isEmpty(); }
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

    ReturnStmt* clone() const override { return makeReturn(Value_->clone()); }

    bool equals(Stmt const* other) const override
    {
        if (Kind_ != other->getKind())
            return false;

        ReturnStmt const* block = dynamic_cast<ReturnStmt const*>(other);
        return Value_->equals(block->getValue());
    }

    Expr const* getValue() const { return Value_; }

    void setValue(Expr* v) { Value_ = v; }

    bool hasValue() const { return Value_ != nullptr; }
};

static constexpr BlockStmt* makeBlock(std::vector<Stmt*> stmts)
{
    return AST_allocator.allocateObject<BlockStmt>(stmts);
}
static constexpr ExprStmt* makeExprStmt(Expr* expr)
{
    return AST_allocator.allocateObject<ExprStmt>(expr);
}
static constexpr AssignmentStmt* makeAssignmentStmt(Expr* target, Expr* value, bool decl)
{
    return AST_allocator.allocateObject<AssignmentStmt>(target, value, decl);
}
static constexpr IfStmt* makeIf(Expr* condition, BlockStmt* then_block, BlockStmt* else_block)
{
    return AST_allocator.allocateObject<IfStmt>(condition, then_block, else_block);
}
static constexpr WhileStmt* makeWhile(Expr* condition, BlockStmt* block)
{
    return AST_allocator.allocateObject<WhileStmt>(condition, block);
}
static constexpr ForStmt* makeFor(NameExpr* target, Expr* iter, BlockStmt* block)
{
    return AST_allocator.allocateObject<ForStmt>(target, iter, block);
}
static constexpr FunctionDef* makeFunction(NameExpr* name, ListExpr* params, BlockStmt* body)
{
    return AST_allocator.allocateObject<FunctionDef>(name, params, body);
}
static constexpr ReturnStmt* makeReturn(Expr* value)
{
    return AST_allocator.allocateObject<ReturnStmt>(value);
}

} // ast
} // mylang
