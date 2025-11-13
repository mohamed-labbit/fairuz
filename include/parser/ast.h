#pragma once


#include <memory>
#include <string>
#include <vector>


namespace mylang {
namespace parser {
namespace ast {

// Forward declarations
struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// Base classes
struct ASTNode
{
    std::size_t line, column;
    virtual ~ASTNode() = default;
};

struct Expr: ASTNode
{
    enum class Kind { BINARY, UNARY, LITERAL, NAME, CALL, TERNARY, ASSIGNMENT, LIST, TUPLE };
    Kind kind;
};

struct Stmt: ASTNode
{
    enum class Kind { EXPRESSION, ASSIGNMENT, IF, WHILE, FOR, FUNCTION_DEF, RETURN, BLOCK };
    Kind kind;
};

// Expression nodes
struct BinaryExpr: Expr
{
    ExprPtr left, right;
    std::u16string op;

    BinaryExpr(ExprPtr l, std::u16string o, ExprPtr r) :
        left(std::move(l)),
        op(std::move(o)),
        right(std::move(r))
    {
        kind = Kind::BINARY;
    }
};

struct UnaryExpr: Expr
{
    std::u16string op;
    ExprPtr operand;

    UnaryExpr(std::u16string o, ExprPtr expr) :
        op(std::move(o)),
        operand(std::move(expr))
    {
        kind = Kind::UNARY;
    }
};

struct LiteralExpr: Expr
{
    enum class Type { NUMBER, STRING, BOOLEAN, NONE };
    Type litType;
    std::u16string value;

    LiteralExpr(Type t, std::u16string v) :
        litType(t),
        value(std::move(v))
    {
        kind = Kind::LITERAL;
    }
};

struct NameExpr: Expr
{
    std::u16string name;

    explicit NameExpr(std::u16string n) :
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

struct TernaryExpr: Expr
{
    ExprPtr condition, trueExpr, falseExpr;

    TernaryExpr(ExprPtr cond, ExprPtr t, ExprPtr f) :
        condition(std::move(cond)),
        trueExpr(std::move(t)),
        falseExpr(std::move(f))
    {
        kind = Kind::TERNARY;
    }
};

struct AssignmentExpr: Expr
{
    std::u16string target;
    ExprPtr value;

    AssignmentExpr(std::u16string t, ExprPtr v) :
        target(std::move(t)),
        value(std::move(v))
    {
        kind = Kind::ASSIGNMENT;
    }
};

struct ListExpr: Expr
{
    std::vector<ExprPtr> elements;

    explicit ListExpr(std::vector<ExprPtr> elems) :
        elements(std::move(elems))
    {
        kind = Kind::LIST;
    }
};

// Statement nodes
struct ExprStmt: Stmt
{
    ExprPtr expression;

    explicit ExprStmt(ExprPtr e) :
        expression(std::move(e))
    {
        kind = Kind::EXPRESSION;
    }
};

struct AssignmentStmt: Stmt
{
    std::u16string target;
    ExprPtr value;

    AssignmentStmt(std::u16string t, ExprPtr v) :
        target(std::move(t)),
        value(std::move(v))
    {
        kind = Kind::ASSIGNMENT;
    }
};

struct IfStmt: Stmt
{
    ExprPtr condition;
    std::vector<StmtPtr> thenBlock;
    std::vector<StmtPtr> elseBlock;

    IfStmt(ExprPtr cond, std::vector<StmtPtr> tb, std::vector<StmtPtr> eb) :
        condition(std::move(cond)),
        thenBlock(std::move(tb)),
        elseBlock(std::move(eb))
    {
        kind = Kind::IF;
    }
};

struct WhileStmt: Stmt
{
    ExprPtr condition;
    std::vector<StmtPtr> body;

    WhileStmt(ExprPtr cond, std::vector<StmtPtr> b) :
        condition(std::move(cond)),
        body(std::move(b))
    {
        kind = Kind::WHILE;
    }
};

struct ForStmt: Stmt
{
    std::u16string target;
    ExprPtr iter;
    std::vector<StmtPtr> body;

    ForStmt(std::u16string t, ExprPtr i, std::vector<StmtPtr> b) :
        target(std::move(t)),
        iter(std::move(i)),
        body(std::move(b))
    {
        kind = Kind::FOR;
    }
};

struct FunctionDef: Stmt
{
    std::u16string name;
    std::vector<std::u16string> params;
    std::vector<StmtPtr> body;

    FunctionDef(std::u16string n, std::vector<std::u16string> p, std::vector<StmtPtr> b) :
        name(std::move(n)),
        params(std::move(p)),
        body(std::move(b))
    {
        kind = Kind::FUNCTION_DEF;
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