#pragma once


#include <memory>
#include <string>
#include <vector>


// Forward declarations
struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// Base classes
struct ASTNode
{
    std::size_t line_, column_;
    virtual ~ASTNode() = default;
};

struct Expr: ASTNode
{
    enum class Kind { BINARY, UNARY, LITERAL, NAME, CALL, TERNARY, ASSIGNMENT, LIST, TUPLE };
    Kind kind_;
};

struct Stmt: ASTNode
{
    enum class Kind { EXPRESSION, ASSIGNMENT, IF, WHILE, FOR, FUNCTION_DEF, RETURN, BLOCK };
    Kind kind_;
};

// Expression nodes
struct BinaryExpr: Expr
{
    ExprPtr left_, right_;
    std::u16string op_;
    BinaryExpr(ExprPtr l, std::u16string o, ExprPtr r) :
        left_(std::move(l)),
        op_(std::move(o)),
        right_(std::move(r))
    {
        kind_ = Kind::BINARY;
    }
};

struct UnaryExpr: Expr
{
    std::u16string op_;
    ExprPtr operand_;
    UnaryExpr(std::u16string o, ExprPtr expr) :
        op_(std::move(o)),
        operand_(std::move(expr))
    {
        kind_ = Kind::UNARY;
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
        kind_ = Kind::LITERAL;
    }
};

struct NameExpr: Expr
{
    std::u16string name_;
    explicit NameExpr(std::u16string n) :
        name_(std::move(n))
    {
        kind_ = Kind::NAME;
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
        kind_ = Kind::CALL;
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
        kind_ = Kind::TERNARY;
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
        kind_ = Kind::ASSIGNMENT;
    }
};

struct ListExpr: Expr
{
    std::vector<ExprPtr> elements;
    explicit ListExpr(std::vector<ExprPtr> elems) :
        elements(std::move(elems))
    {
        kind_ = Kind::LIST;
    }
};

// Statement nodes
struct ExprStmt: Stmt
{
    ExprPtr expression_;
    explicit ExprStmt(ExprPtr e) :
        expression_(std::move(e))
    {
        kind_ = Kind::EXPRESSION;
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
        kind_ = Kind::ASSIGNMENT;
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
        kind_ = Kind::IF;
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
        kind_ = Kind::WHILE;
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
        kind_ = Kind::FOR;
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
        kind_ = Kind::FUNCTION_DEF;
    }
};

struct ReturnStmt: Stmt
{
    ExprPtr value_;
    explicit ReturnStmt(ExprPtr v) :
        value_(std::move(v))
    {
        kind_ = Kind::RETURN;
    }
};

struct BlockStmt: Stmt
{
    std::vector<StmtPtr> statements_;
    explicit BlockStmt(std::vector<StmtPtr> stmts) :
        statements_(std::move(stmts))
    {
        kind_ = Kind::BLOCK;
    }
};