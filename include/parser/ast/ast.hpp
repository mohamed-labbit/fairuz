// ast rewrite

#pragma once

#include "../../diag/diagnostic.hpp"
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
  runtime::allocator::ArenaAllocator Allocator_;

 public:
  ASTAllocator() :
      Allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::LINEAR))
  {
  }

  template<typename T, typename... Args>
  T* make(Args&&... args)
  {
    void* mem = Allocator_.allocate<std::byte>(sizeof(T));
    return new (mem) T(std::forward<Args>(args)...);
  }
};

inline ASTAllocator AST_allocator;

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
  Kind Kind_;
  string_type Str_;

 public:
  explicit Expr() = default;

  explicit Expr(string_type s) :
      Str_(s)
  {
    Kind_ = Kind::INVALID;
  }

  void setStr(const string_type s) { Str_ = s; }
  string_type getStr() const { return Str_; }
  Kind getKind() const { return Kind_; }
};

class Stmt: public ASTNode
{
 public:
  enum class Kind : int { INVALID, EXPR, ASSIGNMENT, IF, WHILE, FOR, FUNC, RETURN, BLOCK };

 protected:
  Kind Kind_;
  // string_type Str_;

 public:
  explicit Stmt() = default;

  explicit Stmt(string_type s)
  // : Str_(s)
  {
    Kind_ = Kind::INVALID;
  }

  // string_type getStr() const { return Str_; }
  Kind getKind() const { return Kind_; }
};

class BinaryExpr: public Expr
{
 private:
  Expr* Left_{nullptr};
  Expr* Right_{nullptr};
  lex::tok::TokenType Operator_{lex::tok::TokenType::INVALID};

 public:
  explicit BinaryExpr() = delete;

  explicit BinaryExpr(Expr* left, Expr* right, lex::tok::TokenType op) :
      Left_(left),
      Right_(right),
      Operator_(op)
  {
    Kind_ = Kind::BINARY;

    assert((Left_ != nullptr) && "'left' argument to BinaryExpr is null");
    assert((Right_ != nullptr) && "'right' argument to BinaryExpr is null");
  }

  BinaryExpr(BinaryExpr&&) noexcept = delete;
  BinaryExpr(const BinaryExpr&) noexcept = delete;
  BinaryExpr& operator=(const BinaryExpr&) noexcept = delete;

  Expr* getLeft() const { return Left_; }
  Expr* getRight() const { return Right_; }
  lex::tok::TokenType getOperator() const { return Operator_; }

  void setLeft(Expr* left) { Left_ = left; }
  void setRight(Expr* right) { Right_ = right; }
  void setOperator(lex::tok::TokenType op) { Operator_ = op; }
};

class UnaryExpr: public Expr
{
 private:
  Expr* Operand_{nullptr};
  lex::tok::TokenType Operator_{lex::tok::TokenType::INVALID};

 public:
  explicit UnaryExpr() = delete;

  explicit UnaryExpr(Expr* operand, lex::tok::TokenType op) :
      Operand_(operand),
      Operator_(op)
  {
    Kind_ = Kind::UNARY;
    assert((Operand_ != nullptr) && "'operand' argument to UnaryExpr is null");
  }

  UnaryExpr(UnaryExpr&&) noexcept = delete;
  UnaryExpr(const UnaryExpr&) noexcept = delete;
  UnaryExpr& operator=(const UnaryExpr&) noexcept = delete;

  Expr* getOperand() const { return Operand_; }
  lex::tok::TokenType getOperator() const { return Operator_; }
};

class LiteralExpr: public Expr
{
 public:
  enum class Type { NUMBER, STRING, BOOLEAN, NONE };

 private:
  Type Type_{Type::NONE};
  string_type Literal_;

 public:
  explicit LiteralExpr() = default;

  explicit LiteralExpr(Type t, string_type lit) :
      Type_(std::move(t)),
      Literal_(std::move(lit))
  {
    Kind_ = Kind::LITERAL;
    // assert((Literal_ != u"") && "'literal' argument to LiteralExpr is null");
  }

  LiteralExpr(LiteralExpr&&) noexcept = delete;
  LiteralExpr(const LiteralExpr&) noexcept = delete;
  LiteralExpr& operator=(const LiteralExpr&) noexcept = delete;

  string_type getValue() const { return Literal_; }
  Type getType() const { return Type_; }
};

class NameExpr: public Expr
{
 public:
  explicit NameExpr() = default;

  explicit NameExpr(const Expr* e) :
      Expr(e->getStr())
  {
    assert((Str_ != u"") && "'Str_' argument to NameExpr is null");
  }

  explicit NameExpr(const string_type s) :
      Expr(s)
  {
    assert(!Str_.empty() && "'Str_' argument to NameExpr is null");
  }

  NameExpr(NameExpr&&) noexcept = delete;
  NameExpr(const NameExpr&) noexcept = delete;
  NameExpr& operator=(const NameExpr&) noexcept = delete;

  string_type getValue() const { return Str_; }
};

class ListExpr: public Expr
{
 private:
  std::vector<Expr*> Elements_;

 public:
  explicit ListExpr() = default;

  explicit ListExpr(std::vector<Expr*> elements) :
      Elements_(std::move(elements))
  {
    Kind_ = Kind::LIST;
    // assert(!Elements_.empty() && "'elements' of ListExpr is null");
    if (Elements_.empty())
      diagnostic::engine.emit("args of ListExpr is initially empty!", /*sv=*/diagnostic::DiagnosticEngine::Severity::NOTE);
    else
    {
      std::stringstream os;
      os << std::hex;
      for (auto a : Elements_) os << a << " ";
      diagnostic::engine.emit("args of ListExpr are " + os.str(), /*sv=*/diagnostic::DiagnosticEngine::Severity::NOTE);
    }
  }

  ListExpr(ListExpr&&) noexcept = delete;
  ListExpr(const ListExpr&) noexcept = delete;
  ListExpr& operator=(const ListExpr&) noexcept = delete;

  const std::vector<Expr*>& getElements() const { return Elements_; }
  std::vector<Expr*>& getElementsMutable() { return Elements_; }
  bool isEmpty() const { return Elements_.empty(); }
};

class CallExpr: public Expr
{
 public:
  enum class CallLocation : int { GLOBAL, LOCAL };

 private:
  NameExpr* Callee_{nullptr};
  ListExpr* Args_;
  CallLocation CallLocation_;

 public:
  explicit CallExpr() = delete;

  explicit CallExpr(NameExpr* c, ListExpr* a = nullptr) :
      Callee_(c)
  {
    Args_ = AST_allocator.make<ListExpr>();
    Kind_ = Kind::CALL;
#if DEBUG_PRINT
    if (!Args_ || Args_->isEmpty())
      diagnostic::engine.emit("args of CallExpr is initially empty!");

    else
    {
      std::stringstream os;
      os << std::hex;

      for (auto a : Args_->getElements()) { os << a << " "; }

      diagnostic::engine.emit("args of CallExpr are " + os.str());
    }
#endif
  }

  CallExpr(CallExpr&&) noexcept = delete;
  CallExpr(const CallExpr&) noexcept = delete;
  CallExpr& operator=(const CallExpr&) noexcept = delete;

  NameExpr* getCallee() const { return Callee_; }
  const std::vector<Expr*>& getArgs() const { return Args_->getElements(); }
  std::vector<Expr*>& getArgsMutable() { return Args_->getElementsMutable(); }

  CallLocation getCallLocation() const { return CallLocation_; }
};

class AssignmentExpr: public Expr
{
 private:
  NameExpr* Target_{nullptr};
  Expr* Value_{nullptr};

 public:
  explicit AssignmentExpr() = delete;

  explicit AssignmentExpr(NameExpr* target, Expr* value) :
      Target_(target),
      Value_(value)
  {
    Kind_ = Kind::ASSIGNMENT;

    assert((Target_ != nullptr) && "'target' argument to AssignmentExpr is null");
    assert((Value_ != nullptr) && "'value' argument to AssignmentExpr is null");
  }

  AssignmentExpr(AssignmentExpr&&) noexcept = delete;
  AssignmentExpr(const AssignmentExpr&) noexcept = delete;
  AssignmentExpr& operator=(const AssignmentExpr&) noexcept = delete;

  NameExpr* getTarget() const { return Target_; }
  Expr* getValue() const { return Value_; }
};  // ?

class BlockStmt: public Stmt
{
 private:
  std::vector<Stmt*> Statements_;

 public:
  explicit BlockStmt() = delete;

  explicit BlockStmt(std::vector<Stmt*> stmts) :
      Statements_(std::move(stmts))
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
  NameExpr* Target_{nullptr};

 public:
  explicit AssignmentStmt() = delete;

  explicit AssignmentStmt(NameExpr* target, Expr* value) :
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
  NameExpr* getTarget() const { return Target_; }
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
  std::vector<NameExpr*> Params_;
  BlockStmt* Body_{nullptr};

 public:
  explicit FunctionDef() = delete;

  explicit FunctionDef(NameExpr* name, std::vector<NameExpr*> params, BlockStmt* body) :
      Name_(name),
      Params_(std::move(params)),
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
  const std::vector<NameExpr*>& getParameters() const { return Params_; }
  BlockStmt* getBody() const { return Body_; }
  void setBody(BlockStmt* b) { Body_ = b; }
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
