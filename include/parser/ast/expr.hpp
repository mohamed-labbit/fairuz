#pragma once

#include "../../diag/diagnostic.hpp"
#include "../../lex/token.hpp"
#include "ast_node.hpp"

#include <cassert>
#include <vector>


namespace mylang {
namespace parser {
namespace ast {

class Expr: public ASTNode
{
 public:
  enum class Kind : int { INVALID, BINARY, UNARY, LITERAL, NAME, CALL, ASSIGNMENT, LIST };

 protected:
  Kind Kind_;
  StringType Str_;

 public:
  explicit Expr() = default;

  explicit Expr(StringType s) :
      Str_(s)
  {
    Kind_ = Kind::INVALID;
  }

  void setStr(const StringType s) { Str_ = s; }
  StringType getStr() const { return Str_; }
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
  StringType Literal_;

 public:
  explicit LiteralExpr() = default;

  explicit LiteralExpr(Type t, StringType lit) :
      Type_(t),
      Literal_(lit)
  {
    Kind_ = Kind::LITERAL;
    // assert((Literal_ != u"") && "'literal' argument to LiteralExpr is null");
  }

  LiteralExpr(LiteralExpr&&) noexcept = delete;
  LiteralExpr(const LiteralExpr&) noexcept = delete;
  LiteralExpr& operator=(const LiteralExpr&) noexcept = delete;

  StringType getValue() const { return Literal_; }
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

  explicit NameExpr(const StringType s) :
      Expr(s)
  {
    Kind_ = Kind::NAME;
    assert(!Str_.empty() && "'Str_' argument to NameExpr is null");
  }

  NameExpr(NameExpr&&) noexcept = delete;
  NameExpr(const NameExpr&) noexcept = delete;
  NameExpr& operator=(const NameExpr&) noexcept = delete;

  StringType getValue() const { return Str_; }
};

class ListExpr: public Expr
{
 private:
  std::vector<Expr*> Elements_;

 public:
  explicit ListExpr() = default;

  explicit ListExpr(std::vector<Expr*> elements) :
      Elements_(elements)
  {
    Kind_ = Kind::LIST;
    // assert(!Elements_.empty() && "'elements' of ListExpr is null");
    if (Elements_.empty())
      diagnostic::engine.emit("args of ListExpr is initially empty!", /*sv=*/diagnostic::DiagnosticEngine::Severity::NOTE);
    else
    {
      std::stringstream os;
      os << std::hex;
      for (ast::Expr* a : Elements_) os << a << " ";
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
  Expr* Callee_{nullptr};
  ListExpr* Args_;
  CallLocation CallLocation_;

 public:
  explicit CallExpr() = delete;

  explicit CallExpr(Expr* c, ListExpr* a = nullptr) :
      Callee_(c),
      Args_(a)
  {
    Kind_ = Kind::CALL;
    if (Args_ == nullptr || Args_->isEmpty())
      diagnostic::engine.emit("args of CallExpr is initially empty!", diagnostic::DiagnosticEngine::Severity::NOTE);
    else
    {
      std::stringstream os;
      os << std::hex;

      for (auto a : Args_->getElements()) { os << a << " "; }

      diagnostic::engine.emit("args of CallExpr are " + os.str(), diagnostic::DiagnosticEngine::Severity::NOTE);
    }
  }

  CallExpr(CallExpr&&) noexcept = delete;
  CallExpr(const CallExpr&) noexcept = delete;
  CallExpr& operator=(const CallExpr&) noexcept = delete;

  Expr* getCallee() const { return Callee_; }
  const std::vector<Expr*>& getArgs() const { return Args_->getElements(); }
  std::vector<Expr*>& getArgsMutable() { return Args_->getElementsMutable(); }
  ListExpr* getArgsAsListExpr() { return Args_; }
  CallLocation getCallLocation() const { return CallLocation_; }

  bool hasArguments() const { return Args_ == nullptr || Args_->isEmpty(); }
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

}
}
}