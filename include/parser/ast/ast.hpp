// ast rewrite

#pragma once

#include "../../lex/token.hpp"
#include "../../runtime/allocator/arena.hpp"
#include "macros.hpp"

#include <array>
#include <cassert>
#include <memory>
#include <random>
#include <string>
#include <vector>

// structs
namespace mylang {
namespace parser {
namespace ast {

class ASTAllocator
{
 private:
  runtime::allocator::ArenaAllocator allocator_;

 public:
  ASTAllocator() :
      allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::LINEAR))
  {
  }

  template<typename T, typename... Args>
  T* make(Args&&... args)
  {
    void* mem = allocator_.allocate<std::byte>(sizeof(T));
    return new (mem) T(std::forward<Args>(args)...);
  }
};

extern ASTAllocator AST_allocator;

class Expr;
class Stmt;
class BlockStmt;

struct ASTNode
{
  unsigned line{};
  unsigned column{};
  ASTNode() = default;
  ASTNode(const ASTNode&) = delete;
  ASTNode& operator=(const ASTNode&) = delete;
  ASTNode(ASTNode&&) = delete;
  ASTNode& operator=(ASTNode&&) = delete;
  virtual ~ASTNode() = default;
};

class Expr: public ASTNode
{
 public:
  enum class Kind : int { INVALID, BINARY, UNARY, LITERAL, NAME, CALL, ASSIGNMENT, LIST };

 protected:
  Kind kind_;
  // string_type str_;

 public:
  explicit Expr() = default;

  explicit Expr(string_type s)
  // : str_(s)
  {
    kind_ = Kind::INVALID;
  }

  // string_type getStr() const { return str_; }
  Kind getKind() const { return kind_; }
};

class Stmt: public ASTNode
{
 public:
  enum class Kind : int { INVALID, EXPR, ASSIGNMENT, IF, WHILE, FOR, FUNC, RETURN, BLOCK };

 protected:
  Kind kind_;
  // string_type str_;

 public:
  explicit Stmt() = default;

  explicit Stmt(string_type s)
  // : str_(s)
  {
    kind_ = Kind::INVALID;
  }

  // string_type getStr() const { return str_; }
  Kind getKind() const { return kind_; }
};

class BinaryExpr: public Expr
{
 private:
  Expr* left_{nullptr};
  Expr* right_{nullptr};
  lex::tok::TokenType operator_{lex::tok::TokenType::INVALID};

 public:
  explicit BinaryExpr() = delete;

  explicit BinaryExpr(Expr* left, Expr* right, lex::tok::TokenType op) :
      left_(left),
      right_(right),
      operator_(op)
  {
    kind_ = Kind::BINARY;

    assert((left_ != nullptr) && "'left' argument to BinaryExpr is null");
    assert((right_ != nullptr) && "'right' argument to BinaryExpr is null");
  }

  BinaryExpr(BinaryExpr&&) noexcept = delete;
  BinaryExpr(const BinaryExpr&) noexcept = delete;
  BinaryExpr& operator=(const BinaryExpr&) noexcept = delete;

  Expr* getLeft() const { return left_; }
  Expr* getRight() const { return right_; }
  lex::tok::TokenType getOperator() const { return operator_; }

  void setLeft(Expr* left) { left_ = left; }
  void setRight(Expr* right) { right_ = right; }
  void setOperator(lex::tok::TokenType op) { operator_ = op; }
};

class UnaryExpr: public Expr
{
 private:
  Expr* operand_{nullptr};
  lex::tok::TokenType operator_{lex::tok::TokenType::INVALID};

 public:
  explicit UnaryExpr() = delete;

  explicit UnaryExpr(Expr* operand, lex::tok::TokenType op) :
      operand_(operand),
      operator_(op)
  {
    kind_ = Kind::UNARY;
    assert((operand_ != nullptr) && "'operand' argument to UnaryExpr is null");
  }

  UnaryExpr(UnaryExpr&&) noexcept = delete;
  UnaryExpr(const UnaryExpr&) noexcept = delete;
  UnaryExpr& operator=(const UnaryExpr&) noexcept = delete;

  Expr* getOperand() const { return operand_; }
  lex::tok::TokenType getOperator() const { return operator_; }
};

class LiteralExpr: public Expr
{
 public:
  enum class Type { NUMBER, STRING, BOOLEAN, NONE };

 private:
  Type type_{Type::NONE};
  string_type literal_;

 public:
  explicit LiteralExpr() = default;

  explicit LiteralExpr(Type t, string_type lit) :
      type_(std::move(t)),
      literal_(std::move(lit))
  {
    kind_ = Kind::LITERAL;
    assert((literal_ != u"") && "'literal' argument to LiteralExpr is null");
  }

  LiteralExpr(LiteralExpr&&) noexcept = delete;
  LiteralExpr(const LiteralExpr&) noexcept = delete;
  LiteralExpr& operator=(const LiteralExpr&) noexcept = delete;

  string_type getValue() const { return literal_; }
  Type getType() const { return type_; }
};

class NameExpr: public Expr
{
 private:
  string_type value_;

 public:
  explicit NameExpr() = default;

  explicit NameExpr(string_type n) :
      value_(std::move(n))
  {
    kind_ = Kind::NAME;
    assert((value_ != u"") && "'name_' argument to NameExpr is null");
  }

  NameExpr(NameExpr&&) noexcept = delete;
  NameExpr(const NameExpr&) noexcept = delete;
  NameExpr& operator=(const NameExpr&) noexcept = delete;

  string_type getValue() const { return value_; }
};

class CallExpr: public Expr
{
 private:
  Expr* callee_{nullptr};
  std::vector<Expr*> args_;

 public:
  explicit CallExpr() = delete;

  explicit CallExpr(Expr* c, std::optional<std::vector<Expr*>> a = std::nullopt) :
      callee_(c),
      args_(std::move(a.value_or(std::vector<Expr*>())))
  {
    kind_ = Kind::CALL;
    assert((callee_ != nullptr) && "'callee' argument to CallExpr is null");
  }

  CallExpr(CallExpr&&) noexcept = delete;
  CallExpr(const CallExpr&) noexcept = delete;
  CallExpr& operator=(const CallExpr&) noexcept = delete;

  Expr* getCallee() const { return callee_; }
  const std::vector<Expr*>& getArgs() const { return args_; }
  std::vector<Expr*>& getArgsMutable() { return args_; }
};

class AssignmentExpr: public Expr
{
 private:
  NameExpr* target_{nullptr};
  Expr* value_{nullptr};

 public:
  explicit AssignmentExpr() = delete;

  explicit AssignmentExpr(NameExpr* target, Expr* value) :
      target_(target),
      value_(value)
  {
    kind_ = Kind::ASSIGNMENT;

    assert((target_ != nullptr) && "'target' argument to AssignmentExpr is null");
    assert((value_ != nullptr) && "'value' argument to AssignmentExpr is null");
  }

  AssignmentExpr(AssignmentExpr&&) noexcept = delete;
  AssignmentExpr(const AssignmentExpr&) noexcept = delete;
  AssignmentExpr& operator=(const AssignmentExpr&) noexcept = delete;

  NameExpr* getTarget() const { return target_; }
  Expr* getValue() const { return value_; }
};  // ?


class BlockStmt: public Stmt
{
 private:
  std::vector<Stmt*> statements_;

 public:
  explicit BlockStmt() = delete;

  explicit BlockStmt(std::vector<Stmt*> stmts) :
      statements_(std::move(stmts))
  {
    kind_ = Kind::BLOCK;

    assert((!statements_.empty() && statements_[0] != nullptr) && "'statements' argument to BlockStmt is null");
  }

  BlockStmt(BlockStmt&&) noexcept = delete;
  BlockStmt(const BlockStmt&) noexcept = delete;
  BlockStmt& operator=(const BlockStmt&) noexcept = delete;

  const std::vector<Stmt*>& getStatements() const { return statements_; }
  void setStatements(std::vector<Stmt*>& stmts) { statements_ = stmts; }
};

class ListExpr: public Expr
{
 private:
  std::vector<Expr*> elements_;

 public:
  explicit ListExpr() = delete;

  explicit ListExpr(std::vector<Expr*> elements) :
      elements_(std::move(elements))
  {
    kind_ = Kind::LIST;
  }

  ListExpr(ListExpr&&) noexcept = delete;
  ListExpr(const ListExpr&) noexcept = delete;
  ListExpr& operator=(const ListExpr&) noexcept = delete;

  const std::vector<Expr*>& getElements() const { return elements_; }
  std::vector<Expr*>& getElementsMutable() { return elements_; }
};

class ExprStmt: public Stmt
{
 private:
  Expr* expr_{nullptr};

 public:
  explicit ExprStmt() = default;

  explicit ExprStmt(Expr* expr) :
      expr_(expr)
  {
    kind_ = Kind::EXPR;
    assert((expr_ != nullptr) && "'expr' argument to ExprStmt is null");
  }

  ExprStmt(ExprStmt&&) noexcept = delete;
  ExprStmt(const ExprStmt&) noexcept = delete;
  ExprStmt& operator=(const ExprStmt&) noexcept = delete;

  Expr* getExpr() const { return expr_; }
  void setExpr(Expr* e) { expr_ = e; }
};

class AssignmentStmt: public Stmt
{
 private:
  Expr* value_{nullptr};
  NameExpr* target_{nullptr};

 public:
  explicit AssignmentStmt() = delete;

  explicit AssignmentStmt(NameExpr* target, Expr* value) :
      target_(target),
      value_(value)
  {
    kind_ = Kind::ASSIGNMENT;

    assert((value_ != nullptr) && "'value' argument to AssignmentStmt is null");
    assert((target_ != nullptr) && "'target' argument to AssignmentStmt is null");
  }

  AssignmentStmt(AssignmentStmt&&) noexcept = delete;
  AssignmentStmt(const AssignmentStmt&) noexcept = delete;
  AssignmentStmt& operator=(const AssignmentStmt&) noexcept = delete;

  Expr* getValue() const { return value_; }
  NameExpr* getTarget() const { return target_; }
  void setValue(Expr* v) { value_ = v; }
  void setTarget(NameExpr* t) { target_ = t; }
};

class IfStmt: public Stmt
{
 private:
  Expr* condition_{nullptr};
  BlockStmt* then_block_{nullptr};
  BlockStmt* else_block_{nullptr};

 public:
  explicit IfStmt() = delete;

  explicit IfStmt(Expr* condition, BlockStmt* then_block, BlockStmt* else_block) :
      condition_(condition),
      then_block_(then_block),
      else_block_(else_block)
  {
    kind_ = Kind::IF;

    assert((condition_ != nullptr) && "'condition' argument to IfStmt is null");
    assert((then_block_ != nullptr) && "'then_block' argument to IfStmt is null");
    assert((else_block_ != nullptr) && "'else_block' argument to IfStmt is null");
  }

  IfStmt(IfStmt&&) noexcept = delete;
  IfStmt(const IfStmt&) noexcept = delete;
  IfStmt& operator=(const IfStmt&) noexcept = delete;

  Expr* getCondition() const { return condition_; }
  BlockStmt* getThenBlock() const { return then_block_; }
  BlockStmt* getElseBlock() const { return else_block_; }

  void setThenBlock(BlockStmt* t) { then_block_ = t; }
  void setElseBlock(BlockStmt* e) { else_block_ = e; }
};

class WhileStmt: public Stmt
{
 private:
  Expr* condition_{nullptr};
  BlockStmt* block_{nullptr};

 public:
  explicit WhileStmt() = delete;

  explicit WhileStmt(Expr* condition, BlockStmt* block) :
      condition_(condition),
      block_(block)
  {
    kind_ = Kind::WHILE;

    assert((condition_ != nullptr) && "'condition' argument to WhileStmt is null");
    assert((block_ != nullptr) && "'block' argument to WhileStmt is null");
  }

  WhileStmt(WhileStmt&&) noexcept = delete;
  WhileStmt(const WhileStmt&) noexcept = delete;
  WhileStmt& operator=(const WhileStmt&) noexcept = delete;

  Expr* getCondition() const { return condition_; }
  BlockStmt* getBlock() const { return block_; }
  BlockStmt*& getBlockMutable() { return std::ref<BlockStmt*>(block_); }
};

class ForStmt: public Stmt
{
 private:
  NameExpr* target_{nullptr};
  Expr* iter_{nullptr};
  BlockStmt* block_{nullptr};

 public:
  explicit ForStmt() = delete;

  explicit ForStmt(NameExpr* target, Expr* iter, BlockStmt* block) :
      target_(target),
      iter_(iter),
      block_(block)
  {
    kind_ = Kind::FOR;

    assert((target_ != nullptr) && "'target' argument to ForStmt is null");
    assert((iter_ != nullptr) && "'iter' argument to ForStmt is null");
    assert((block_ != nullptr) && "'block' argument to ForStmt is null");
  }

  ForStmt(ForStmt&&) noexcept = delete;
  ForStmt(const ForStmt&) noexcept = delete;
  ForStmt& operator=(const ForStmt&) noexcept = delete;

  NameExpr* getTarget() const { return target_; }
  Expr* getIter() const { return iter_; }
  BlockStmt* getBlock() const { return block_; }
  void setBlock(BlockStmt* b) { block_ = b; }
};

class FunctionDef: public Stmt
{
 private:
  NameExpr* name_{nullptr};
  std::vector<NameExpr*> params_;
  BlockStmt* body_{nullptr};

 public:
  explicit FunctionDef() = delete;

  explicit FunctionDef(NameExpr* name, std::vector<NameExpr*> params, BlockStmt* body) :
      name_(name),
      params_(std::move(params)),
      body_(body)
  {
    kind_ = Kind::FUNC;

    assert((name_ != nullptr) && "'name' argument to FunctionDef is null");
    assert((body_ != nullptr) && "'body' argument to FunctionDef is null");
  }

  FunctionDef(FunctionDef&&) noexcept = delete;
  FunctionDef(const FunctionDef&) noexcept = delete;
  FunctionDef& operator=(const FunctionDef&) noexcept = delete;

  NameExpr* getName() const { return name_; }
  const std::vector<NameExpr*>& getParameters() const { return params_; }
  BlockStmt* getBody() const { return body_; }
  void setBody(BlockStmt* b) { body_ = b; }
};

class ReturnStmt: public Stmt
{
 private:
  Expr* value_{nullptr};

 public:
  explicit ReturnStmt() = delete;

  explicit ReturnStmt(Expr* value) :
      value_(value)
  {
    kind_ = Kind::RETURN;
    assert((value_ != nullptr) && "'value' argument to ReturnStmt is null");
  }

  ReturnStmt(ReturnStmt&&) noexcept = delete;
  ReturnStmt(const ReturnStmt&) noexcept = delete;
  ReturnStmt& operator=(const ReturnStmt&) noexcept = delete;

  Expr* getValue() const { return value_; }
};

}
}
}
