// ast rewrite

#pragma once

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

struct Expr;
struct Stmt;

typedef std::unique_ptr<Expr> ExprPtr;
typedef std::unique_ptr<Stmt> StmtPtr;

const unsigned MAX_ARGS = 10;  // TODO: change to comprehensive uppder bound

struct ASTNode
{
  unsigned line;
  unsigned column;
  virtual ~ASTNode() = default;
};

struct Expr: ASTNode
{
  enum class Kind : int { BINARY, UNARY, LITERAL, NAME, CALL, ASSIGNMENT, LIST };
  Kind kind;
};

struct Stmt: ASTNode
{
  enum class Kind : int { EXPR, ASSIGNMENT, IF, WHILE, FOR, FUNC, RETURN, BLOCK };
  Kind kind;
};

struct BinaryExpr: Expr
{
  ExprPtr left;
  ExprPtr right;
  string_type op;

  BinaryExpr(ExprPtr l, ExprPtr r, string_type o) :
      left(std::move(l)),
      right(std::move(r)),
      op(std::move(o))
  {
    kind = Kind::BINARY;
  }
};

struct UnaryExpr: Expr
{
  ExprPtr self;
  string_type op;

  UnaryExpr(ExprPtr s, string_type o) :
      self(std::move(s)),
      op(std::move(o))
  {
    kind = Kind::UNARY;
  }
};

struct LiteralExpr: Expr
{
  enum class Type { NUMBER, STRING, BOOLEAN, NONE } type;
  string_type literal;

  LiteralExpr(Type t, string_type s) :
      type(std::move(t)),
      literal(std::move(s))
  {
    kind = Kind::LITERAL;
  }
};

struct NameExpr: Expr
{
  string_type name;

  NameExpr(string_type n) :
      name(std::move(n))
  {
    kind = Kind::NAME;
  }
};

struct CallExpr: Expr
{
  ExprPtr callee;
  std::vector<ExprPtr> args;

  CallExpr(ExprPtr c, std::vector<ExprPtr> a) :
      callee(std::move(c)),
      args(std::move(a))
  {
    kind = Kind::CALL;
  }
};

#if 0
struct TernaryExpr: Expr
{
};
#endif

struct AssignmentExpr: Expr
{
  ExprPtr value;
  NameExpr target;

  AssignmentExpr(NameExpr v, ExprPtr t) :
      target(std::move(v)),
      value(std::move(t))
  {
    kind = Kind::ASSIGNMENT;
  }
};  // ?

struct ListExpr: Expr
{
  std::vector<ExprPtr> elements;

  ListExpr(std::vector<ExprPtr> els) :
      elements(std::move(els))
  {
    kind = Kind::LIST;
  }
};

struct ExprStmt: Stmt
{
  ExprPtr expr;

  ExprStmt(ExprPtr e) :
      expr(std::move(e))
  {
    kind = Kind::EXPR;
  }
};

struct AssignmentStmt: Stmt
{
  ExprPtr value;
  NameExpr target;

  AssignmentStmt(NameExpr t, ExprPtr v) :
      target(std::move(t)),
      value(std::move(v))
  {
    kind = Kind::ASSIGNMENT;
  }
};

struct IfStmt: Stmt
{
  ExprPtr condition;
  std::vector<StmtPtr> then_stmts;
  std::vector<StmtPtr> else_stmts;

  IfStmt(ExprPtr c, std::vector<StmtPtr> t, std::vector<StmtPtr> e) :
      condition(std::move(c)),
      then_stmts(std::move(t)),
      else_stmts(std::move(e))
  {
    kind = Kind::IF;
  }
};

struct WhileStmt: Stmt
{
  ExprPtr condition;
  std::vector<StmtPtr> stmts;

  WhileStmt(ExprPtr c, std::vector<StmtPtr> s) :
      condition(std::move(c)),
      stmts(std::move(s))
  {
  }
};

struct ForStmt: Stmt
{
  string_type target;
  ExprPtr iter;
  std::vector<StmtPtr> body;

  ForStmt(string_type t, ExprPtr i, std::vector<StmtPtr> b) :
      target(std::move(t)),
      iter(std::move(i)),
      body(std::move(b))
  {
    kind = Kind::FOR;
  }
};

struct FunctionDef: Stmt
{
  string_type name;
  std::vector<string_type> params;
  std::vector<StmtPtr> body;

  FunctionDef(string_type n, std::vector<string_type> p, std::vector<StmtPtr> b) :
      name(std::move(n)),
      params(std::move(p)),
      body(std::move(b))
  {
    kind = Kind::FUNC;
  }
};

struct ReturnStmt: Stmt
{
  ExprPtr value;

  explicit ReturnStmt(ExprPtr v) :
      value(std::move(v))
  {
    kind = Kind::RETURN;
  }
};

struct BlockStmt: Stmt
{
  std::vector<StmtPtr> statements;

  explicit BlockStmt(std::vector<StmtPtr> stmts) :
      statements(std::move(stmts))
  {
    kind = Kind::BLOCK;
  }
};

}
}
}