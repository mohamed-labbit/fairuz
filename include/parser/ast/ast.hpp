// ast rewrite

#pragma once

#include "../../lex/token.hpp"
#include "macros.hpp"

#include <array>
#include <cassert>
#include <memory>
#include <string>
#include <vector>

// structs
namespace mylang {
namespace parser {
namespace ast {

class Expr;
class Stmt;
class BlockStmt;

typedef std::unique_ptr<Expr> ExprPtr;
typedef std::unique_ptr<Stmt> StmtPtr;

struct ASTNode
{
  unsigned line;
  unsigned column;
  virtual ~ASTNode() = default;
};

class Expr: public ASTNode
{
 public:
  enum class Kind : int { BINARY, UNARY, LITERAL, NAME, CALL, ASSIGNMENT, LIST };

 protected:
  Kind kind_;
  string_type str_;

 public:
  explicit Expr() = default;

  explicit Expr(string_type s) :
      str_(s)
  {
  }

  string_type getStr() const { return str_; }
  Kind getKind() const { return kind_; }
};

class Stmt: public ASTNode
{
 public:
  enum class Kind : int { EXPR, ASSIGNMENT, IF, WHILE, FOR, FUNC, RETURN, BLOCK };

 protected:
  Kind kind_;
  string_type str_;

 public:
  explicit Stmt() = default;

  explicit Stmt(string_type s) :
      str_(s)
  {
  }

  string_type getStr() const { return str_; }
  Kind getKind() const { return kind_; }
};

class BinaryExpr: public Expr
{
 private:
  Expr* left_{nullptr};
  Expr* right_{nullptr};
  lex::tok::TokenType operator_{lex::tok::TokenType::INVALID};

 public:
  explicit BinaryExpr() = default;

  explicit BinaryExpr(Expr* l, Expr* r, lex::tok::TokenType o) :
      left_(std::move(l)),
      right_(std::move(r)),
      operator_(std::move(o))
  {
    kind_ = Kind::BINARY;

    assert((left_ != nullptr) && "'left' argument to BinaryExpr is null");
    assert((right_ != nullptr) && "'right' argument to BinaryExpr is null");
  }

  Expr* getLeft() const { return left_; }
  Expr* getRight() const { return right_; }
  lex::tok::TokenType getOperator() const { return operator_; }

  ~BinaryExpr()
  {
    if (left_ != nullptr) delete left_;
    if (right_ != nullptr) delete right_;
  }
};

class UnaryExpr: public Expr
{
 private:
  Expr* operand_{nullptr};
  lex::tok::TokenType operator_{lex::tok::TokenType::INVALID};

 public:
  explicit UnaryExpr() = default;

  explicit UnaryExpr(Expr* operand, lex::tok::TokenType op) :
      operand_(std::move(operand)),
      operator_(std::move(op))
  {
    kind_ = Kind::UNARY;
    assert((operand_ != nullptr) && "'operand' argument to UnaryExpr is null");
  }

  Expr* getOperand() const { return operand_; }
  lex::tok::TokenType getOperator() const { return operator_; }

  ~UnaryExpr()
  {
    if (operand_ != nullptr) delete operand_;
  }
};

class LiteralExpr: public Expr
{
 public:
  enum class Type { NUMBER, STRING, BOOLEAN, NONE };

 private:
  Type type_{Type::NONE};
  Expr* literal_{nullptr};

 public:
  explicit LiteralExpr() = default;

  explicit LiteralExpr(Type t, Expr* lit) :
      type_(std::move(t)),
      literal_(std::move(lit))
  {
    kind_ = Kind::LITERAL;
    assert((literal_ != nullptr) && "'literal' argument to LiteralExpr is null");
  }

  Expr* getValue() const { return literal_; }
  Type getType() const { return type_; }

  ~LiteralExpr()
  {
    if (literal_ != nullptr) delete literal_;
  }
};

class NameExpr: public Expr
{
 private:
  Expr* value_{nullptr};

 public:
  explicit NameExpr() = default;

  explicit NameExpr(Expr* n) :
      value_(std::move(n))
  {
    kind_ = Kind::NAME;
    assert((value_ != nullptr) && "'name_' argument to NameExpr is null");
  }

  Expr* getValue() const { return value_; }

  ~NameExpr()
  {
    if (value_ != nullptr) delete value_;
  }
};

class CallExpr: public Expr
{
 private:
  Expr* callee_{nullptr};
  std::vector<Expr*> args_{nullptr};

 public:
  explicit CallExpr() = default;

  explicit CallExpr(Expr* c, std::optional<std::vector<Expr*>> a = std::nullopt) :
      callee_(std::move(c)),
      args_(std::move(a.value_or(std::vector<Expr*>())))
  {
    kind_ = Kind::CALL;
    assert((callee_ != nullptr) && "'callee' argument to CallExpr is null");
  }

  Expr* getCallee() const { return callee_; }
  std::vector<Expr*> getArgs() const { return args_; }

  ~CallExpr()
  {
    if (callee_ != nullptr) delete callee_;
    for (auto* a : args_)
    {
      if (a != nullptr) delete a;
    }
  }
};

class AssignmentExpr: public Expr
{
 private:
  NameExpr* target_{nullptr};
  Expr* value_{nullptr};

 public:
  explicit AssignmentExpr() = default;

  explicit AssignmentExpr(NameExpr* target, Expr* value) :
      target_(std::move(target)),
      value_(std::move(value))
  {
    kind_ = Kind::ASSIGNMENT;

    assert((target_ != nullptr) && "'target' argument to AssignmentExpr is null");
    assert((value_ != nullptr) && "'value' argument to AssignmentExpr is null");
  }

  NameExpr* getTarget() const { return target_; }
  Expr* getValue() const { return value_; }

  ~AssignmentExpr()
  {
    if (target_ != nullptr) delete target_;
    if (value_ != nullptr) delete value_;
  }
};  // ?


class BlockStmt: public Stmt
{
 private:
  std::vector<Stmt*> statements_{nullptr};

 public:
  explicit BlockStmt() = default;

  explicit BlockStmt(std::vector<Stmt*> stmts) :
      statements_(std::move(stmts))
  {
    kind_ = Kind::BLOCK;

    assert((!statements_.empty() && statements_[0] != nullptr) && "'statements' argument to BlockStmt is null");
  }

  std::vector<Stmt*> getStatements() const { return statements_; }

  ~BlockStmt()
  {
    for (auto* stmt : statements_)
    {
      if (stmt != nullptr) delete stmt;
    }
  }
};

class ListExpr: public Expr
{
 private:
  std::vector<Expr*> elements_{nullptr};

 public:
  explicit ListExpr() = default;

  explicit ListExpr(std::vector<Expr*> elements) :
      elements_(std::move(elements))
  {
    kind_ = Kind::LIST;
  }

  std::vector<Expr*> getElements() const { return elements_; }

  ~ListExpr()
  {
    for (auto* e : elements_)
    {
      if (e != nullptr) delete e;
    }
  }
};

class ExprStmt: public Stmt
{
 private:
  Expr* expr_{nullptr};

 public:
  explicit ExprStmt() = default;

  explicit ExprStmt(Expr* expr) :
      expr_(std::move(expr))
  {
    kind_ = Kind::EXPR;
    assert((expr_ != nullptr) && "'expr' argument to ExprStmt is null");
  }

  Expr* getExpr() const { return expr_; }

  ~ExprStmt()
  {
    if (expr_ != nullptr) delete expr_;
  }
};

class AssignmentStmt: public Stmt
{
 private:
  Expr* value_{nullptr};
  NameExpr* target_{nullptr};

 public:
  explicit AssignmentStmt() = default;

  explicit AssignmentStmt(NameExpr* target, Expr* value) :
      target_(std::move(target)),
      value_(std::move(value))
  {
    kind_ = Kind::ASSIGNMENT;

    assert((value_ != nullptr) && "'value' argument to AssignmentStmt is null");
    assert((target_ != nullptr) && "'target' argument to AssignmentStmt is null");
  }

  Expr* getValue() const { return value_; }
  NameExpr* getTarget() const { return target_; }

  ~AssignmentStmt()
  {
    if (value_ != nullptr) delete value_;
    if (target_ != nullptr) delete target_;
  }
};

class IfStmt: public Stmt
{
 private:
  Expr* condition_{nullptr};
  BlockStmt* then_block_{nullptr};
  BlockStmt* else_block_{nullptr};

 public:
  explicit IfStmt() = default;

  explicit IfStmt(Expr* condition, BlockStmt* then_block, BlockStmt* else_block) :
      condition_(std::move(condition)),
      then_block_(std::move(then_block)),
      else_block_(std::move(else_block))
  {
    kind_ = Kind::IF;

    assert((condition_ != nullptr) && "'condition' argument to IfStmt is null");
    assert((then_block_ != nullptr) && "'then_block' argument to IfStmt is null");
    assert((else_block_ != nullptr) && "'else_block' argument to IfStmt is null");
  }

  Expr* getCondition() const { return condition_; }
  BlockStmt* getThenBlock() const { return then_block_; }
  BlockStmt* getElseBlock() const { return else_block_; }

  ~IfStmt()
  {
    if (condition_ != nullptr) delete condition_;
    if (then_block_ != nullptr) delete then_block_;
    if (else_block_ != nullptr) delete else_block_;
  }
};

class WhileStmt: public Stmt
{
 private:
  Expr* condition_{nullptr};
  BlockStmt* block_{nullptr};

 public:
  explicit WhileStmt() = default;

  explicit WhileStmt(Expr* condition, BlockStmt* block) :
      condition_(std::move(condition)),
      block_(std::move(block))
  {
    kind_ = Kind::WHILE;

    assert((condition_ != nullptr) && "'condition' argument to WhileStmt is null");
    assert((block_ != nullptr) && "'block' argument to WhileStmt is null");
  }

  Expr* getCondition() const { return condition_; }
  BlockStmt* getBlock() const { return block_; }

  ~WhileStmt()
  {
    if (condition_ != nullptr) delete condition_;
    if (block_ != nullptr) delete block_;
  }
};

class ForStmt: public Stmt
{
 private:
  Expr* target_{nullptr};
  Expr* iter_{nullptr};
  BlockStmt* block_{nullptr};

 public:
  explicit ForStmt() = default;

  explicit ForStmt(Expr* target, Expr* iter, BlockStmt* block) :
      target_(std::move(target)),
      iter_(std::move(iter)),
      block_(std::move(block))
  {
    kind_ = Kind::FOR;

    assert((target_ != nullptr) && "'target' argument to ForStmt is null");
    assert((iter_ != nullptr) && "'iter' argument to ForStmt is null");
    assert((block_ != nullptr) && "'block' argument to ForStmt is null");
  }

  Expr* getTarget() const { return target_; }
  Expr* getIter() const { return iter_; }
  BlockStmt* getBlock() const { return block_; }

  ~ForStmt()
  {
    if (target_ != nullptr) delete target_;
    if (iter_ != nullptr) delete iter_;
    if (block_ != nullptr) delete block_;
  }
};

class FunctionDef: public Stmt
{
 private:
  NameExpr* name_{nullptr};
  std::vector<Expr*> params_{nullptr};
  BlockStmt* body_{nullptr};

 public:
  explicit FunctionDef() = default;

  explicit FunctionDef(NameExpr* name, std::vector<Expr*> params, BlockStmt* body) :
      name_(std::move(name)),
      params_(std::move(params)),
      body_(std::move(body))
  {
    kind_ = Kind::FUNC;

    assert((name_ != nullptr) && "'name' argument to FunctionDef is null");
    assert((body_ != nullptr) && "'body' argument to FunctionDef is null");
  }

  NameExpr* getName() const { return name_; }
  std::vector<Expr*> getParameters() const { return params_; }
  BlockStmt* getBody() const { return body_; }

  ~FunctionDef()
  {
    if (name_ != nullptr) delete name_;
    if (body_ != nullptr) delete body_;

    for (auto* p : params_)
    {
      if (p != nullptr) delete p;
    }
  }
};

class ReturnStmt: public Stmt
{
 private:
  Expr* value_{nullptr};

 public:
  explicit ReturnStmt() = default;

  explicit ReturnStmt(Expr* value) :
      value_(std::move(value))
  {
    kind_ = Kind::RETURN;
    assert((value_ != nullptr) && "'value' argument to ReturnStmt is null");
  }

  Expr* getValue() const { return value_; }

  ~ReturnStmt()
  {
    if (value_ != nullptr) delete value_;
  }
};

}
}
}