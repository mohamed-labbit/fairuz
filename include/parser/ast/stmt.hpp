#pragma once

#include "ast_node.hpp"

#include <cassert>
#include <functional>
#include <vector>


namespace mylang {
namespace parser {
namespace ast {

class Stmt: public ASTNode
{
 public:
  enum class Kind : int { INVALID, EXPR, ASSIGNMENT, IF, WHILE, FOR, FUNC, RETURN, BLOCK };

 protected:
  Kind Kind_;

 public:
  explicit Stmt() = default;

  explicit Stmt(StringType s) { Kind_ = Kind::INVALID; }

  Kind getKind() const { return Kind_; }
};


class BlockStmt: public Stmt
{
 private:
  std::vector<Stmt*> Statements_;

 public:
  explicit BlockStmt() = delete;

  explicit BlockStmt(std::vector<Stmt*> stmts) :
      Statements_(stmts)
  {
    Kind_ = Kind::BLOCK;

    assert((!Statements_.empty() && Statements_[0] != nullptr) && "'statements' argument to BlockStmt is null");
  }

  BlockStmt(BlockStmt&&) noexcept = delete;
  BlockStmt(const BlockStmt&) noexcept = delete;
  BlockStmt& operator=(const BlockStmt&) noexcept = delete;

  const std::vector<Stmt*>& getStatements() const { return Statements_; }

  void setStatements(std::vector<Stmt*>& stmts) { Statements_ = stmts; }

  bool isEmpty() const { return Statements_.empty(); }
};

class ExprStmt: public Stmt
{
 private:
  Expr* Expr_{nullptr};

 public:
  explicit ExprStmt() = default;

  explicit ExprStmt(Expr* expr) :
      Expr_(expr)
  {
    Kind_ = Kind::EXPR;
    assert((Expr_ != nullptr) && "'expr' argument to ExprStmt is null");
  }

  ExprStmt(ExprStmt&&) noexcept = delete;
  ExprStmt(const ExprStmt&) noexcept = delete;
  ExprStmt& operator=(const ExprStmt&) noexcept = delete;

  Expr* getExpr() const { return Expr_; }

  void setExpr(Expr* e) { Expr_ = e; }
};

class AssignmentStmt: public Stmt
{
 private:
  Expr* Value_{nullptr};
  Expr* Target_{nullptr};

 public:
  explicit AssignmentStmt() = delete;

  explicit AssignmentStmt(Expr* target, Expr* value) :
      Target_(target),
      Value_(value)
  {
    Kind_ = Kind::ASSIGNMENT;

    assert((Value_ != nullptr) && "'value' argument to AssignmentStmt is null");
    assert((Target_ != nullptr) && "'target' argument to AssignmentStmt is null");
  }

  AssignmentStmt(AssignmentStmt&&) noexcept = delete;
  AssignmentStmt(const AssignmentStmt&) noexcept = delete;
  AssignmentStmt& operator=(const AssignmentStmt&) noexcept = delete;

  Expr* getValue() const { return Value_; }
  Expr* getTarget() const { return Target_; }

  void setValue(Expr* v) { Value_ = v; }
  void setTarget(NameExpr* t) { Target_ = t; }
};

class IfStmt: public Stmt
{
 private:
  Expr* Condition_{nullptr};
  BlockStmt* ThenBlock_{nullptr};
  BlockStmt* ElseBlock_{nullptr};

 public:
  explicit IfStmt() = delete;

  explicit IfStmt(Expr* condition, BlockStmt* then_block, BlockStmt* else_block) :
      Condition_(condition),
      ThenBlock_(then_block),
      ElseBlock_(else_block)
  {
    Kind_ = Kind::IF;

    assert((Condition_ != nullptr) && "'condition' argument to IfStmt is null");
    assert((ThenBlock_ != nullptr) && "'then_block' argument to IfStmt is null");
    assert((ElseBlock_ != nullptr) && "'else_block' argument to IfStmt is null");
  }

  IfStmt(IfStmt&&) noexcept = delete;
  IfStmt(const IfStmt&) noexcept = delete;
  IfStmt& operator=(const IfStmt&) noexcept = delete;

  Expr* getCondition() const { return Condition_; }
  BlockStmt* getThenBlock() const { return ThenBlock_; }
  BlockStmt* getElseBlock() const { return ElseBlock_; }

  void setThenBlock(BlockStmt* t) { ThenBlock_ = t; }
  void setElseBlock(BlockStmt* e) { ElseBlock_ = e; }
};

class WhileStmt: public Stmt
{
 private:
  Expr* Condition_{nullptr};
  BlockStmt* Block_{nullptr};

 public:
  explicit WhileStmt() = delete;

  explicit WhileStmt(Expr* condition, BlockStmt* block) :
      Condition_(condition),
      Block_(block)
  {
    Kind_ = Kind::WHILE;

    assert((Condition_ != nullptr) && "'condition' argument to WhileStmt is null");
    assert((Block_ != nullptr) && "'block' argument to WhileStmt is null");
  }

  WhileStmt(WhileStmt&&) noexcept = delete;
  WhileStmt(const WhileStmt&) noexcept = delete;
  WhileStmt& operator=(const WhileStmt&) noexcept = delete;

  Expr* getCondition() const { return Condition_; }
  BlockStmt* getBlock() const { return Block_; }
  BlockStmt*& getBlockMutable() { return std::ref<BlockStmt*>(Block_); }
};

class ForStmt: public Stmt
{
 private:
  NameExpr* Target_{nullptr};
  Expr* Iter_{nullptr};
  BlockStmt* Block_{nullptr};

 public:
  explicit ForStmt() = delete;

  explicit ForStmt(NameExpr* target, Expr* iter, BlockStmt* block) :
      Target_(target),
      Iter_(iter),
      Block_(block)
  {
    Kind_ = Kind::FOR;

    assert((Target_ != nullptr) && "'target' argument to ForStmt is null");
    assert((Iter_ != nullptr) && "'iter' argument to ForStmt is null");
    assert((Block_ != nullptr) && "'block' argument to ForStmt is null");
  }

  ForStmt(ForStmt&&) noexcept = delete;
  ForStmt(const ForStmt&) noexcept = delete;
  ForStmt& operator=(const ForStmt&) noexcept = delete;

  NameExpr* getTarget() const { return Target_; }
  Expr* getIter() const { return Iter_; }
  BlockStmt* getBlock() const { return Block_; }

  void setBlock(BlockStmt* b) { Block_ = b; }
};

class FunctionDef: public Stmt
{
 private:
  NameExpr* Name_{nullptr};
  ListExpr* Params_;
  BlockStmt* Body_{nullptr};

 public:
  explicit FunctionDef() = delete;

  explicit FunctionDef(NameExpr* name, ListExpr* params, BlockStmt* body) :
      Name_(name),
      Params_(params),
      Body_(body)
  {
    Kind_ = Kind::FUNC;

    assert((Name_ != nullptr) && "'name' argument to FunctionDef is null");
    assert((Body_ != nullptr) && "'body' argument to FunctionDef is null");
  }

  FunctionDef(FunctionDef&&) noexcept = delete;
  FunctionDef(const FunctionDef&) noexcept = delete;
  FunctionDef& operator=(const FunctionDef&) noexcept = delete;

  NameExpr* getName() const { return Name_; }
  const std::vector<Expr*>& getParameters() const { return Params_->getElements(); }
  BlockStmt* getBody() const { return Body_; }

  void setBody(BlockStmt* b) { Body_ = b; }

  bool hasParameters() const { return Params_ == nullptr || Params_->isEmpty(); }
};

class ReturnStmt: public Stmt
{
 private:
  Expr* Value_{nullptr};

 public:
  explicit ReturnStmt() = delete;

  explicit ReturnStmt(Expr* value) :
      Value_(value)
  {
    Kind_ = Kind::RETURN;
    assert((Value_ != nullptr) && "'value' argument to ReturnStmt is null");
  }

  ReturnStmt(ReturnStmt&&) noexcept = delete;
  ReturnStmt(const ReturnStmt&) noexcept = delete;
  ReturnStmt& operator=(const ReturnStmt&) noexcept = delete;

  Expr* getValue() const { return Value_; }
};


}
}
}