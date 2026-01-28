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
  explicit Stmt(StringRef s) { Kind_ = Kind::INVALID; }

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

    // assert((!Statements_.empty() && Statements_[0]) && "'statements' argument to BlockStmt is null");
  }

  BlockStmt(BlockStmt&&) MYLANG_NOEXCEPT                 = delete;
  BlockStmt(const BlockStmt&) MYLANG_NOEXCEPT            = delete;
  BlockStmt& operator=(const BlockStmt&) MYLANG_NOEXCEPT = delete;

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
  }

  ExprStmt(ExprStmt&&) MYLANG_NOEXCEPT                 = delete;
  ExprStmt(const ExprStmt&) MYLANG_NOEXCEPT            = delete;
  ExprStmt& operator=(const ExprStmt&) MYLANG_NOEXCEPT = delete;

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
  }

  AssignmentStmt(AssignmentStmt&&) MYLANG_NOEXCEPT                 = delete;
  AssignmentStmt(const AssignmentStmt&) MYLANG_NOEXCEPT            = delete;
  AssignmentStmt& operator=(const AssignmentStmt&) MYLANG_NOEXCEPT = delete;

  Expr* getValue() const { return Value_; }

  Expr* getTarget() const { return Target_; }

  void setValue(Expr* v) { Value_ = v; }

  void setTarget(NameExpr* t) { Target_ = t; }
};

class IfStmt: public Stmt
{
 private:
  Expr*      Condition_{nullptr};
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
  }

  IfStmt(IfStmt&&) MYLANG_NOEXCEPT                 = delete;
  IfStmt(const IfStmt&) MYLANG_NOEXCEPT            = delete;
  IfStmt& operator=(const IfStmt&) MYLANG_NOEXCEPT = delete;

  Expr* getCondition() const { return Condition_; }

  BlockStmt* getThenBlock() const { return ThenBlock_; }

  BlockStmt* getElseBlock() const { return ElseBlock_; }

  void setThenBlock(BlockStmt* t) { ThenBlock_ = t; }

  void setElseBlock(BlockStmt* e) { ElseBlock_ = e; }
};

class WhileStmt: public Stmt
{
 private:
  Expr*      Condition_{nullptr};
  BlockStmt* Block_{nullptr};

 public:
  explicit WhileStmt() = delete;

  explicit WhileStmt(Expr* condition, BlockStmt* block) :
      Condition_(condition),
      Block_(block)
  {
    Kind_ = Kind::WHILE;
  }

  WhileStmt(WhileStmt&&) MYLANG_NOEXCEPT                 = delete;
  WhileStmt(const WhileStmt&) MYLANG_NOEXCEPT            = delete;
  WhileStmt& operator=(const WhileStmt&) MYLANG_NOEXCEPT = delete;

  Expr* getCondition() const { return Condition_; }

  BlockStmt* getBlock() const { return Block_; }

  BlockStmt*& getBlockMutable() { return std::ref<BlockStmt*>(Block_); }
};

class ForStmt: public Stmt
{
 private:
  NameExpr*  Target_{nullptr};
  Expr*      Iter_{nullptr};
  BlockStmt* Block_{nullptr};

 public:
  explicit ForStmt() = delete;

  explicit ForStmt(NameExpr* target, Expr* iter, BlockStmt* block) :
      Target_(target),
      Iter_(iter),
      Block_(block)
  {
    Kind_ = Kind::FOR;
  }

  ForStmt(ForStmt&&) MYLANG_NOEXCEPT                 = delete;
  ForStmt(const ForStmt&) MYLANG_NOEXCEPT            = delete;
  ForStmt& operator=(const ForStmt&) MYLANG_NOEXCEPT = delete;

  NameExpr* getTarget() const { return Target_; }

  Expr* getIter() const { return Iter_; }

  BlockStmt* getBlock() const { return Block_; }

  void setBlock(BlockStmt* b) { Block_ = b; }
};

class FunctionDef: public Stmt
{
 private:
  NameExpr*  Name_{nullptr};
  ListExpr*  Params_;
  BlockStmt* Body_{nullptr};

 public:
  explicit FunctionDef() = delete;

  explicit FunctionDef(NameExpr* name, ListExpr* params, BlockStmt* body) :
      Name_(name),
      Params_(params),
      Body_(body)
  {
    Kind_ = Kind::FUNC;
  }

  FunctionDef(FunctionDef&&) MYLANG_NOEXCEPT                 = delete;
  FunctionDef(const FunctionDef&) MYLANG_NOEXCEPT            = delete;
  FunctionDef& operator=(const FunctionDef&) MYLANG_NOEXCEPT = delete;

  NameExpr* getName() const { return Name_; }

  const std::vector<Expr*>& getParameters() const { return Params_->getElements(); }

  BlockStmt* getBody() const { return Body_; }

  void setBody(BlockStmt* b) { Body_ = b; }

  bool hasParameters() const { return !Params_ || Params_->isEmpty(); }
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
  }

  ReturnStmt(ReturnStmt&&) MYLANG_NOEXCEPT                 = delete;
  ReturnStmt(const ReturnStmt&) MYLANG_NOEXCEPT            = delete;
  ReturnStmt& operator=(const ReturnStmt&) MYLANG_NOEXCEPT = delete;

  Expr* getValue() const { return Value_; }
};


}  // ast
}  // parser
}  // mylang