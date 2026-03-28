#ifndef AST_HPP
#define AST_HPP

#include "array.hpp"
#include "string.hpp"
#include <cassert>
#include <cstddef>

namespace fairuz::AST {

struct Fa_Expr;
struct Fa_ListExpr;
struct Fa_NameExpr;
struct Fa_AssignmentExpr;
struct Fa_Stmt;
struct Fa_BlockStmt;
struct Fa_AssignmentStmt;

static Fa_ListExpr* Fa_makeList(Fa_Array<Fa_Expr*> elements = { });
static Fa_BlockStmt* Fa_makeBlock(Fa_Array<Fa_Stmt*> stmts = { });
static Fa_NameExpr* Fa_makeName(Fa_StringRef const str);
static Fa_AssignmentExpr* Fa_makeAssignmentExpr(Fa_Expr* target, Fa_Expr* value, bool decl = false);
static Fa_AssignmentStmt* Fa_makeAssignmentStmt(Fa_Expr* target, Fa_Expr* value, bool decl = false);

struct Fa_ASTNode {
public:
    enum class NodeType : int {
        EXPRESSION,
        STATEMENT,
        INVALID
    }; // enum NodeType

private:
    u32 Line_ { 0 };
    u16 Column_ { 0 };

    NodeType NodeType_ { NodeType::INVALID };

public:
    Fa_ASTNode() = default;
    Fa_ASTNode(Fa_ASTNode const&) = delete;
    Fa_ASTNode(Fa_ASTNode&&) = delete;

    Fa_ASTNode& operator=(Fa_ASTNode const&) = delete;
    Fa_ASTNode& operator=(Fa_ASTNode&&) = delete;

    [[nodiscard]] virtual NodeType getNodeType() const;
    [[nodiscard]] u32 getLine() const;
    [[nodiscard]] u16 getColumn() const;
    void setLine(u32 line);
    void setColumn(u16 col);
    virtual ~Fa_ASTNode() = default;
}; // struct Fa_ASTNode

enum class Fa_BinaryOp : u8 {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_POW,
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LTE,
    OP_GTE,
    OP_BITAND,
    OP_BITOR,
    OP_BITXOR,
    OP_LSHIFT,
    OP_RSHIFT,
    OP_AND,
    OP_OR,
    INVALID
}; // enum Fa_BinaryOp

enum class Fa_UnaryOp : u8 {
    OP_PLUS,
    OP_NEG,
    OP_BITNOT,
    OP_NOT,
    INVALID
}; // enum Fa_UnaryOp

/// NOTE: do not know if the assert for the costructors args is a good idea

struct Fa_Expr : public Fa_ASTNode {
public:
    enum class Kind : int {
        BINARY,
        UNARY,
        LITERAL,
        NAME,
        CALL,
        ASSIGNMENT,
        LIST,
        INDEX,
        INVALID
    }; // enum Kind

protected:
    Kind Kind_ { Kind::INVALID };

public:
    Fa_Expr()
        : Kind_(Kind::INVALID)
    {
    }

    virtual ~Fa_Expr() = default;

    virtual bool equals(Fa_Expr const* other) const = 0;
    virtual Fa_Expr* clone() const = 0;

    Kind getKind() const { return Kind_; }
    NodeType getNodeType() const override { return NodeType::EXPRESSION; }
}; // struct Fa_Expr

struct Fa_BinaryExpr final : public Fa_Expr {
private:
    Fa_Expr* Left_ { nullptr };
    Fa_Expr* Right_ { nullptr };
    Fa_BinaryOp Operator_ { Fa_BinaryOp::INVALID };

public:
    Fa_BinaryExpr() = delete;

    Fa_BinaryExpr(Fa_Expr* l, Fa_Expr* r, Fa_BinaryOp op)
        : Left_(l)
        , Right_(r)
        , Operator_(op)
    {
        assert(Left_ != nullptr && Right_ != nullptr);
        Kind_ = Kind::BINARY;
    }

    ~Fa_BinaryExpr() override = default;

    Fa_BinaryExpr(Fa_BinaryExpr&&) noexcept = delete;
    Fa_BinaryExpr(Fa_BinaryExpr const&) noexcept = delete;

    Fa_BinaryExpr& operator=(Fa_BinaryExpr const&) noexcept = delete;
    Fa_BinaryExpr& operator=(Fa_BinaryExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_BinaryExpr* clone() const override;
    [[nodiscard]] Fa_Expr* getLeft() const;
    [[nodiscard]] Fa_Expr* getRight() const;
    [[nodiscard]] Fa_BinaryOp getOperator() const;

    void setLeft(Fa_Expr* l);
    void setRight(Fa_Expr* r);
    void setOperator(Fa_BinaryOp op);
}; // struct Fa_BinaryExpr

struct Fa_UnaryExpr final : public Fa_Expr {
private:
    Fa_Expr* Operand_ { nullptr };
    Fa_UnaryOp Operator_ { Fa_UnaryOp::INVALID };

public:
    Fa_UnaryExpr() = delete;

    Fa_UnaryExpr(Fa_Expr* operand, Fa_UnaryOp op)
        : Operand_(operand)
        , Operator_(op)
    {
        assert(operand != nullptr);
        Kind_ = Kind::UNARY;
    }

    ~Fa_UnaryExpr() override = default;

    Fa_UnaryExpr(Fa_UnaryExpr&&) noexcept = delete;
    Fa_UnaryExpr(Fa_UnaryExpr const&) noexcept = delete;

    Fa_UnaryExpr& operator=(Fa_UnaryExpr const&) noexcept = delete;
    Fa_UnaryExpr& operator=(Fa_UnaryExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_UnaryExpr* clone() const override;
    [[nodiscard]] Fa_Expr* getOperand() const;
    [[nodiscard]] Fa_UnaryOp getOperator() const;
}; // struct Fa_UnaryExpr

struct Fa_LiteralExpr final : public Fa_Expr {
public:
    enum class Type {
        INTEGER,
        FLOAT,
        STRING,
        BOOLEAN,
        NIL
    }; // enum Type

private:
    Type Type_ { Type::NIL };

    union {
        i64 IntValue_;
        f64 FloatValue_;
        bool BoolValue_;
    }; // union

    Fa_StringRef StrValue_;

public:
    explicit Fa_LiteralExpr()
        : Type_(Type::NIL)
    {
        Kind_ = Kind::LITERAL;
    }

    Fa_LiteralExpr(i64 value, Type type)
        : Type_(type)
        , IntValue_(value)
    {
        Kind_ = Kind::LITERAL;
    }
    Fa_LiteralExpr(f64 value, Type type)
        : Type_(type)
        , FloatValue_(value)
    {
        Kind_ = Kind::LITERAL;
    }

    explicit Fa_LiteralExpr(bool value)
        : Type_(Type::BOOLEAN)
        , BoolValue_(value)
    {
        Kind_ = Kind::LITERAL;
    }
    explicit Fa_LiteralExpr(Fa_StringRef str)
        : Type_(Type::STRING)
        , StrValue_(std::move(str))
    {
        Kind_ = Kind::LITERAL;
    }

    ~Fa_LiteralExpr() override = default;

    Fa_LiteralExpr(Fa_LiteralExpr&&) noexcept = delete;
    Fa_LiteralExpr(Fa_LiteralExpr const&) noexcept = delete;

    Fa_LiteralExpr& operator=(Fa_LiteralExpr const&) noexcept = delete;
    Fa_LiteralExpr& operator=(Fa_LiteralExpr&&) noexcept = delete;

    [[nodiscard]] Type getType() const;
    [[nodiscard]] i64 getInt() const;
    [[nodiscard]] f64 getFloat() const;
    [[nodiscard]] bool getBool() const;
    [[nodiscard]] Fa_StringRef getStr() const;

    [[nodiscard]] bool isInteger() const;
    [[nodiscard]] bool isDecimal() const;
    [[nodiscard]] bool isBoolean() const;
    [[nodiscard]] bool isString() const;
    [[nodiscard]] bool isNumeric() const;
    [[nodiscard]] bool isNil() const;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_LiteralExpr* clone() const override;
    [[nodiscard]] f64 toNumber() const;
}; // struct Fa_LiteralExpr

struct Fa_NameExpr final : public Fa_Expr {
private:
    Fa_StringRef Value_;

public:
    Fa_NameExpr() = default;

    explicit Fa_NameExpr(Fa_StringRef s)
        : Value_(std::move(s))
    {
        Kind_ = Kind::NAME;
    }

    ~Fa_NameExpr() override = default;

    Fa_NameExpr(Fa_NameExpr&&) noexcept = delete;
    Fa_NameExpr(Fa_NameExpr const&) noexcept = delete;

    Fa_NameExpr& operator=(Fa_NameExpr const&) noexcept = delete;
    Fa_NameExpr& operator=(Fa_NameExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_NameExpr* clone() const override;
    [[nodiscard]] Fa_StringRef getValue() const;
}; // struct Fa_NameExpr

struct Fa_ListExpr final : public Fa_Expr {
private:
    Fa_Array<Fa_Expr*> Elements_;

public:
    Fa_ListExpr() = default;

    explicit Fa_ListExpr(Fa_Array<Fa_Expr*> elements)
        : Elements_(std::move(elements))
    {
        Kind_ = Kind::LIST;
    }

    ~Fa_ListExpr() override = default;

    Fa_Expr* operator[](size_t const i);
    Fa_Expr const* operator[](size_t const i) const;

    Fa_ListExpr(Fa_ListExpr&&) noexcept = delete;
    Fa_ListExpr(Fa_ListExpr const&) noexcept = delete;

    Fa_ListExpr& operator=(Fa_ListExpr const&) noexcept = delete;
    Fa_ListExpr& operator=(Fa_ListExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_ListExpr* clone() const override;
    [[nodiscard]] Fa_Array<Fa_Expr*> const& getElements() const;
    [[nodiscard]] Fa_Array<Fa_Expr*>& getElements();
    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] size_t size() const;
}; // struct Fa_ListExpr

struct Fa_CallExpr final : public Fa_Expr {
public:
    enum class CallLocation : int {
        GLOBAL,
        LOCAL
    }; // enum CallLocation

private:
    Fa_Expr* Callee_ { nullptr };
    Fa_ListExpr* Args_ { nullptr };
    CallLocation CallLocation_ { CallLocation::GLOBAL }; // FIXED: Initialize member

public:
    Fa_CallExpr() = delete;

    explicit Fa_CallExpr(Fa_Expr* c, Fa_ListExpr* a = nullptr, CallLocation loc = CallLocation::GLOBAL)
        : Callee_(c)
        , Args_(a)
        , CallLocation_(loc)
    {
        if (!Args_)
            Args_ = Fa_makeList();

        assert(Callee_ != nullptr && Args_ != nullptr);
        Kind_ = Kind::CALL;
    }

    ~Fa_CallExpr() override = default;

    Fa_CallExpr(Fa_CallExpr&&) noexcept = delete;
    Fa_CallExpr(Fa_CallExpr const&) noexcept = delete;

    Fa_CallExpr& operator=(Fa_CallExpr const&) noexcept = delete;
    Fa_CallExpr& operator=(Fa_CallExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_CallExpr* clone() const override;
    [[nodiscard]] Fa_Expr* getCallee() const;
    [[nodiscard]] Fa_Array<Fa_Expr*> const& getArgs() const;
    [[nodiscard]] Fa_Array<Fa_Expr*>& getArgs();
    [[nodiscard]] Fa_ListExpr* getArgsAsListExpr();
    [[nodiscard]] Fa_ListExpr const* getArgsAsListExpr() const;
    [[nodiscard]] CallLocation getCallLocation() const;
    [[nodiscard]] bool hasArguments() const;
}; // struct Fa_CallExpr

struct Fa_AssignmentExpr final : public Fa_Expr {
private:
    Fa_Expr* Target_ { nullptr };
    Fa_Expr* Value_ { nullptr };

    bool isDecl_ = false;

public:
    Fa_AssignmentExpr() = delete;

    Fa_AssignmentExpr(Fa_Expr* target, Fa_Expr* value, bool decl = false)
        : Target_(target)
        , Value_(value)
        , isDecl_(decl)
    {
        assert(Target_ != nullptr && Value_ != nullptr);
        Kind_ = Kind::ASSIGNMENT;
    }

    ~Fa_AssignmentExpr() override = default;

    Fa_AssignmentExpr(Fa_AssignmentExpr&&) noexcept = delete;
    Fa_AssignmentExpr(Fa_AssignmentExpr const&) noexcept = delete;

    Fa_AssignmentExpr& operator=(Fa_AssignmentExpr const&) noexcept = delete;
    Fa_AssignmentExpr& operator=(Fa_AssignmentExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_AssignmentExpr* clone() const override;
    [[nodiscard]] Fa_Expr* getTarget() const;
    [[nodiscard]] Fa_Expr* getValue() const;
    void setTarget(Fa_Expr* t);
    void setValue(Fa_Expr* v);
    [[nodiscard]] bool isDeclaration() const;
}; // Fa_AssignmentExpr

struct Fa_IndexExpr final : public Fa_Expr {
private:
    Fa_Expr* Object_ { nullptr };
    Fa_Expr* Index_ { nullptr };

public:
    Fa_IndexExpr() = delete;

    Fa_IndexExpr(Fa_Expr* obj, Fa_Expr* idx)
        : Object_(obj)
        , Index_(idx)
    {
        assert(Object_ != nullptr && Index_ != nullptr);
        Kind_ = Kind::INDEX;
    }

    ~Fa_IndexExpr() override = default;

    Fa_IndexExpr(Fa_IndexExpr&&) noexcept = delete;
    Fa_IndexExpr(Fa_IndexExpr const&) noexcept = delete;

    Fa_IndexExpr& operator=(Fa_IndexExpr const&) noexcept = delete;
    Fa_IndexExpr& operator=(Fa_IndexExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_IndexExpr* clone() const override;
    [[nodiscard]] Fa_Expr* getObject() const;
    [[nodiscard]] Fa_Expr* getIndex() const;
}; // struct Fa_IndexExpr

struct Fa_Stmt : public Fa_ASTNode {
public:
    enum class Kind : u8 {
        EXPR,
        ASSIGNMENT,
        IF,
        WHILE,
        FOR,
        FUNC,
        RETURN,
        BLOCK,
        INVALID
    };

protected:
    Kind Kind_ { Kind::INVALID };

public:
    explicit Fa_Stmt() = default;
    virtual ~Fa_Stmt() = default;

    virtual Fa_Stmt* clone() const = 0;
    virtual bool equals(Fa_Stmt const* other) const = 0;

    Kind getKind() const { return Kind_; }
    NodeType getNodeType() const override { return NodeType::STATEMENT; }
}; // struct Fa_Stmt

struct Fa_BlockStmt final : public Fa_Stmt {
private:
    Fa_Array<Fa_Stmt*> Statements_;

public:
    Fa_BlockStmt() = delete;

    explicit Fa_BlockStmt(Fa_Array<Fa_Stmt*> stmts)
        : Statements_(stmts)
    {
        Kind_ = Kind::BLOCK;
    }

    Fa_BlockStmt(Fa_BlockStmt&&) noexcept = delete;
    Fa_BlockStmt(Fa_BlockStmt const&) noexcept = delete;

    Fa_BlockStmt& operator=(Fa_BlockStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_BlockStmt* clone() const override;
    [[nodiscard]] Fa_Array<Fa_Stmt*> const& getStatements() const;
    [[nodiscard]] bool isEmpty() const;
    void setStatements(Fa_Array<Fa_Stmt*>& stmts);
}; // struct Fa_BlockStmt

struct Fa_ExprStmt final : public Fa_Stmt {
private:
    Fa_Expr* Expr_ { nullptr };

public:
    Fa_ExprStmt() = delete;

    explicit Fa_ExprStmt(Fa_Expr* expr)
        : Expr_(expr)
    {
        assert(Expr_ != nullptr);
        Kind_ = Kind::EXPR;
    }

    Fa_ExprStmt(Fa_ExprStmt&&) noexcept = delete;
    Fa_ExprStmt(Fa_ExprStmt const&) noexcept = delete;

    Fa_ExprStmt& operator=(Fa_ExprStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ExprStmt* clone() const override;
    [[nodiscard]] Fa_Expr* getExpr() const;
    void setExpr(Fa_Expr* e);
}; // struct Fa_ExprStmt

struct Fa_AssignmentStmt final : public Fa_Stmt {
private:
    Fa_AssignmentExpr* Expr_;

public:
    Fa_AssignmentStmt() = delete;

    explicit Fa_AssignmentStmt(Fa_AssignmentExpr* e)
        : Expr_(e->clone())
    {
        assert(Expr_ != nullptr);
        Kind_ = Kind::ASSIGNMENT;
    }

    Fa_AssignmentStmt(Fa_Expr* target, Fa_Expr* value, bool decl = false)
    {
        Expr_ = Fa_makeAssignmentExpr(target, value, decl); // Fa_AssignmentExpr will assert args for us
        Kind_ = Kind::ASSIGNMENT;
    }

    Fa_AssignmentStmt(Fa_AssignmentStmt&&) noexcept = delete;
    Fa_AssignmentStmt(Fa_AssignmentStmt const&) noexcept = delete;

    Fa_AssignmentStmt& operator=(Fa_AssignmentStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_AssignmentStmt* clone() const override;
    [[nodiscard]] Fa_Expr* getValue() const;
    [[nodiscard]] Fa_Expr* getTarget() const;
    [[nodiscard]] bool isDeclaration() const;
    void setValue(Fa_Expr* v);
    void setTarget(Fa_Expr* t);
}; // struct Fa_AssignmentExpr

struct Fa_IfStmt final : public Fa_Stmt {
private:
    Fa_Expr* Condition_ { nullptr };
    Fa_Stmt* ThenStmt_ { nullptr };
    Fa_Stmt* ElseStmt_ { nullptr };

public:
    Fa_IfStmt() = delete;

    Fa_IfStmt(Fa_Expr* condition, Fa_Stmt* then_stmt, Fa_Stmt* else_stmt)
        : Condition_(condition)
        , ThenStmt_(then_stmt)
        , ElseStmt_(else_stmt)
    {
        assert(Condition_ != nullptr && then_stmt != nullptr);
        Kind_ = Kind::IF;
    }

    Fa_IfStmt(Fa_IfStmt&&) noexcept = delete;
    Fa_IfStmt(Fa_IfStmt const&) noexcept = delete;

    Fa_IfStmt& operator=(Fa_IfStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_IfStmt* clone() const override;
    [[nodiscard]] Fa_Expr* getCondition() const;
    [[nodiscard]] Fa_Stmt* getThen() const;
    [[nodiscard]] Fa_Stmt* getElse() const;
    void setThen(Fa_Stmt* t);
    void setElse(Fa_Stmt* e);
}; // struct Fa_IfStmt

struct Fa_WhileStmt final : public Fa_Stmt {
private:
    Fa_Expr* Condition_ { nullptr };
    Fa_Stmt* Body_ { nullptr };

public:
    Fa_WhileStmt() = delete;

    Fa_WhileStmt(Fa_Expr* condition, Fa_Stmt* body)
        : Condition_(condition)
        , Body_(body)
    {
        assert(Condition_ != nullptr && Body_ != nullptr);
        Kind_ = Kind::WHILE;
    }

    Fa_WhileStmt(Fa_WhileStmt&&) noexcept = delete;
    Fa_WhileStmt(Fa_WhileStmt const&) noexcept = delete;

    Fa_WhileStmt& operator=(Fa_WhileStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_WhileStmt* clone() const override;
    [[nodiscard]] Fa_Expr* getCondition() const;
    [[nodiscard]] Fa_Stmt* getBody();
    [[nodiscard]] Fa_Stmt const* getBody() const;
    void setBody(Fa_Stmt* b);
}; // struct Fa_WhileStmt

struct Fa_ForStmt final : public Fa_Stmt {
private:
    Fa_Expr* Container_ { nullptr };
    Fa_Expr* Iter_ { nullptr };
    Fa_Stmt* Body_ { nullptr };

public:
    Fa_ForStmt() = delete;

    Fa_ForStmt(Fa_Expr* target, Fa_Expr* iter, Fa_Stmt* body)
        : Container_(target)
        , Iter_(iter)
        , Body_(body)
    {
        assert(Target_ != nullptr && Iter_ != nullptr && Body_ != nullptr);
        Kind_ = Kind::FOR;
    }

    Fa_ForStmt(Fa_ForStmt&&) noexcept = delete;
    Fa_ForStmt(Fa_ForStmt const&) noexcept = delete;

    Fa_ForStmt& operator=(Fa_ForStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ForStmt* clone() const override;
    [[nodiscard]] Fa_Expr* getContainer() const;
    [[nodiscard]] Fa_Expr* getIter() const;
    [[nodiscard]] Fa_Stmt* getBody() const;
    void setBody(Fa_Stmt* b);
}; // struct Fa_ForStmt

struct Fa_FunctionDef final : public Fa_Stmt {
private:
    Fa_NameExpr* Name_ { nullptr };
    Fa_ListExpr* Params_ { nullptr };
    Fa_Stmt* Body_ { nullptr };

public:
    Fa_FunctionDef() = delete;

    Fa_FunctionDef(Fa_NameExpr* name, Fa_ListExpr* params, Fa_Stmt* body)
        : Name_(name)
        , Params_(params)
        , Body_(body)
    {
        assert(Name_ != nullptr && Params_ != nullptr && Body_ != nullptr);
        Kind_ = Kind::FUNC;
    }

    Fa_FunctionDef(Fa_FunctionDef&&) noexcept = delete;
    Fa_FunctionDef(Fa_FunctionDef const&) noexcept = delete;

    Fa_FunctionDef& operator=(Fa_FunctionDef const&) noexcept = delete;
    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_FunctionDef* clone() const override;
    [[nodiscard]] Fa_NameExpr* getName() const;
    [[nodiscard]] Fa_Array<Fa_Expr*> const& getParameters() const;
    [[nodiscard]] Fa_ListExpr* getParameterList() const;
    [[nodiscard]] Fa_Stmt* getBody() const;
    [[nodiscard]] bool hasParameters() const;

    void setBody(Fa_Stmt* b);
}; // struct Fa_FunctionDef

struct Fa_ReturnStmt final : public Fa_Stmt {
private:
    Fa_Expr* Value_ { nullptr };

public:
    Fa_ReturnStmt() = delete;

    explicit Fa_ReturnStmt(Fa_Expr* value)
        : Value_(value)
    {
        assert(Value_ != nullptr);
        Kind_ = Kind::RETURN;
    }

    Fa_ReturnStmt(Fa_ReturnStmt&&) noexcept = delete;
    Fa_ReturnStmt(Fa_ReturnStmt const&) noexcept = delete;

    Fa_ReturnStmt& operator=(Fa_ReturnStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ReturnStmt* clone() const override;
    [[nodiscard]] Fa_Expr const* getValue() const;
    [[nodiscard]] bool hasValue() const;

    void setValue(Fa_Expr* v);
}; // struct Fa_ReturnStmt

static Fa_BinaryExpr* Fa_makeBinary(Fa_Expr* l, Fa_Expr* r, Fa_BinaryOp const op)
{
    return getAllocator().allocateObject<Fa_BinaryExpr>(l, r, op);
}
static Fa_UnaryExpr* Fa_makeUnary(Fa_Expr* operand, Fa_UnaryOp const op)
{
    return getAllocator().allocateObject<Fa_UnaryExpr>(operand, op);
}
static Fa_LiteralExpr* Fa_makeLiteralNil()
{
    return getAllocator().allocateObject<Fa_LiteralExpr>();
}
static Fa_LiteralExpr* Fa_makeLiteralInt(int value)
{
    return getAllocator().allocateObject<Fa_LiteralExpr>(static_cast<i64>(value), Fa_LiteralExpr::Type::INTEGER);
}
static Fa_LiteralExpr* Fa_makeLiteralInt(i64 value)
{
    return getAllocator().allocateObject<Fa_LiteralExpr>(value, Fa_LiteralExpr::Type::INTEGER);
}
static Fa_LiteralExpr* Fa_makeLiteralFloat(f64 value)
{
    return getAllocator().allocateObject<Fa_LiteralExpr>(value, Fa_LiteralExpr::Type::FLOAT);
}
static Fa_LiteralExpr* Fa_makeLiteralString(Fa_StringRef value)
{
    return getAllocator().allocateObject<Fa_LiteralExpr>(value);
}
static Fa_LiteralExpr* Fa_makeLiteralBool(bool value)
{
    return getAllocator().allocateObject<Fa_LiteralExpr>(value);
}
static Fa_NameExpr* Fa_makeName(Fa_StringRef const str)
{
    return getAllocator().allocateObject<Fa_NameExpr>(str);
}
static Fa_ListExpr* Fa_makeList(Fa_Array<Fa_Expr*> elements)
{
    return getAllocator().allocateObject<Fa_ListExpr>(elements);
}
static Fa_CallExpr* Fa_makeCall(Fa_Expr* callee, Fa_ListExpr* args)
{
    return getAllocator().allocateObject<Fa_CallExpr>(callee, args);
}
static Fa_AssignmentExpr* Fa_makeAssignmentExpr(Fa_Expr* target, Fa_Expr* value, bool decl)
{
    return getAllocator().allocateObject<Fa_AssignmentExpr>(target, value, decl);
}
static Fa_IndexExpr* Fa_makeIndex(Fa_Expr* obj, Fa_Expr* idx)
{
    return getAllocator().allocateObject<Fa_IndexExpr>(obj, idx);
}
static Fa_BlockStmt* Fa_makeBlock(Fa_Array<Fa_Stmt*> stmts)
{
    return getAllocator().allocateObject<Fa_BlockStmt>(stmts);
}
static Fa_ExprStmt* Fa_makeExprStmt(Fa_Expr* expr)
{
    return getAllocator().allocateObject<Fa_ExprStmt>(expr);
}
static Fa_AssignmentStmt* Fa_makeAssignmentStmt(Fa_Expr* target, Fa_Expr* value, bool decl)
{
    return getAllocator().allocateObject<Fa_AssignmentStmt>(target, value, decl);
}
static Fa_IfStmt* Fa_makeIf(Fa_Expr* condition, Fa_Stmt* then_block, Fa_Stmt* else_block = nullptr)
{
    return getAllocator().allocateObject<Fa_IfStmt>(condition, then_block, else_block);
}
static Fa_WhileStmt* Fa_makeWhile(Fa_Expr* condition, Fa_Stmt* body)
{
    return getAllocator().allocateObject<Fa_WhileStmt>(condition, body);
}
static Fa_ForStmt* Fa_makeFor(Fa_NameExpr* target, Fa_Expr* iter, Fa_Stmt* body)
{
    return getAllocator().allocateObject<Fa_ForStmt>(target, iter, body);
}
static Fa_FunctionDef* Fa_makeFunction(Fa_NameExpr* name, Fa_ListExpr* params, Fa_Stmt* body)
{
    return getAllocator().allocateObject<Fa_FunctionDef>(name, params, body);
}
static Fa_ReturnStmt* Fa_makeReturn(Fa_Expr* value = nullptr)
{
    return getAllocator().allocateObject<Fa_ReturnStmt>(value);
}

} // namespace fairuz::ast

#endif // AST_HPP
