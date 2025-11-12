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
    Type litType_;
    std::u16string value_;
    LiteralExpr(Type t, std::u16string v) :
        litType_(t),
        value_(std::move(v))
    {
        kind = Kind::LITERAL;
    }
};

struct NameExpr: Expr
{
    std::u16string name_;
    explicit NameExpr(std::u16string n) :
        name_(std::move(n))
    {
        kind = Kind::NAME;
    }
};

struct CallExpr: Expr
{
    ExprPtr callee_;
    std::vector<ExprPtr> args_;
    CallExpr(ExprPtr c, std::vector<ExprPtr> a) :
        callee_(std::move(c)),
        args_(std::move(a))
    {
        kind = Kind::CALL;
    }
};

struct TernaryExpr: Expr
{
    ExprPtr condition_, trueExpr_, falseExpr_;
    TernaryExpr(ExprPtr cond, ExprPtr t, ExprPtr f) :
        condition_(std::move(cond)),
        trueExpr_(std::move(t)),
        falseExpr_(std::move(f))
    {
        kind = Kind::TERNARY;
    }
};

struct AssignmentExpr: Expr
{
    std::u16string target_;
    ExprPtr value_;
    AssignmentExpr(std::u16string t, ExprPtr v) :
        target_(std::move(t)),
        value_(std::move(v))
    {
        kind = Kind::ASSIGNMENT;
    }
};

struct ListExpr: Expr
{
    std::vector<ExprPtr> elements_;
    explicit ListExpr(std::vector<ExprPtr> elems) :
        elements_(std::move(elems))
    {
        kind = Kind::LIST;
    }
};

// Statement nodes
struct ExprStmt: Stmt
{
    ExprPtr expression_;
    explicit ExprStmt(ExprPtr e) :
        expression_(std::move(e))
    {
        kind = Kind::EXPRESSION;
    }
};

struct AssignmentStmt: Stmt
{
    std::u16string target_;
    ExprPtr value_;
    AssignmentStmt(std::u16string t, ExprPtr v) :
        target_(std::move(t)),
        value_(std::move(v))
    {
        kind = Kind::ASSIGNMENT;
    }
};

struct IfStmt: Stmt
{
    ExprPtr condition_;
    std::vector<StmtPtr> thenBlock_;
    std::vector<StmtPtr> elseBlock_;
    IfStmt(ExprPtr cond, std::vector<StmtPtr> tb, std::vector<StmtPtr> eb) :
        condition_(std::move(cond)),
        thenBlock_(std::move(tb)),
        elseBlock_(std::move(eb))
    {
        kind = Kind::IF;
    }
};

struct WhileStmt: Stmt
{
    ExprPtr condition_;
    std::vector<StmtPtr> body_;
    WhileStmt(ExprPtr cond, std::vector<StmtPtr> b) :
        condition_(std::move(cond)),
        body_(std::move(b))
    {
        kind = Kind::WHILE;
    }
};

struct ForStmt: Stmt
{
    std::u16string target_;
    ExprPtr iter_;
    std::vector<StmtPtr> body_;
    ForStmt(std::u16string t, ExprPtr i, std::vector<StmtPtr> b) :
        target_(std::move(t)),
        iter_(std::move(i)),
        body_(std::move(b))
    {
        kind = Kind::FOR;
    }
};

struct FunctionDef: Stmt
{
    std::u16string name_;
    std::vector<std::u16string> params_;
    std::vector<StmtPtr> body_;
    FunctionDef(std::u16string n, std::vector<std::u16string> p, std::vector<StmtPtr> b) :
        name_(std::move(n)),
        params_(std::move(p)),
        body_(std::move(b))
    {
        kind = Kind::FUNCTION_DEF;
    }
};

struct ReturnStmt: Stmt
{
    ExprPtr value_;
    explicit ReturnStmt(ExprPtr v) :
        value_(std::move(v))
    {
        kind = Kind::RETURN;
    }
};

struct BlockStmt: Stmt
{
    std::vector<StmtPtr> statements_;
    explicit BlockStmt(std::vector<StmtPtr> stmts) :
        statements_(std::move(stmts))
    {
        kind = Kind::BLOCK;
    }
};

}
}
}