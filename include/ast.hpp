#ifndef AST_HPP
#define AST_HPP

#include "array.hpp"
#include "string.hpp"
#include <cassert>
#include <cstddef>

namespace mylang::ast {

// inline Allocator getAllocator()("AST allocator");

struct Expr;
struct ListExpr;
struct NameExpr;
struct AssignmentExpr;
struct ErrExpr;
struct Stmt;
struct BlockStmt;
struct AssignmentStmt;
struct ErrStmt;

static ListExpr* makeList(Array<Expr*> elements = { });
static BlockStmt* makeBlock(Array<Stmt*> stmts = { });
static NameExpr* makeName(StringRef const str);
static AssignmentExpr* makeAssignmentExpr(Expr* target, Expr* value, bool decl = false);
static AssignmentStmt* makeAssignmentStmt(Expr* target, Expr* value, bool decl = false);
static ErrExpr* makeErrExpr(uint32_t line, uint32_t col);
static ErrStmt* makeErrStmt(uint32_t line, uint32_t col);

struct ASTNode {
public:
    enum class NodeType : int {
        EXPRESSION,
        STATEMENT,
        INVALID
    }; // enum NodeType

private:
    uint32_t Line_ { 0 };
    uint16_t Column_ { 0 };

    NodeType NodeType_ { NodeType::INVALID };

public:
    ASTNode() = default;
    ASTNode(ASTNode const&) = delete;
    ASTNode(ASTNode&&) = delete;

    ASTNode& operator=(ASTNode const&) = delete;
    ASTNode& operator=(ASTNode&&) = delete;

    [[nodiscard]] virtual NodeType getNodeType() const;
    [[nodiscard]] uint32_t getLine() const;
    [[nodiscard]] uint16_t getColumn() const;
    void setLine(uint32_t line);
    void setColumn(uint16_t col);
    virtual ~ASTNode() = default;
}; // struct ASTNode

enum class BinaryOp : uint8_t {
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
}; // enum BinaryOp

enum class UnaryOp : uint8_t {
    OP_PLUS,
    OP_NEG,
    OP_BITNOT,
    OP_NOT,
    INVALID
}; // enum UnaryOp

/// NOTE: do not know if the assert for the costructors args is a good idea

struct Expr : public ASTNode {
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
    Expr()
        : Kind_(Kind::INVALID)
    {
    }

    virtual ~Expr() = default;

    virtual bool equals(Expr const* other) const = 0;
    virtual Expr* clone() const = 0;

    Kind getKind() const { return Kind_; }
    NodeType getNodeType() const override { return NodeType::EXPRESSION; }
}; // struct Expr

struct BinaryExpr final : public Expr {
private:
    Expr* Left_ { nullptr };
    Expr* Right_ { nullptr };
    BinaryOp Operator_ { BinaryOp::INVALID };

public:
    BinaryExpr() = delete;

    BinaryExpr(Expr* l, Expr* r, BinaryOp op)
        : Left_(l)
        , Right_(r)
        , Operator_(op)
    {
        assert(Left_ != nullptr && Right_ != nullptr);
        Kind_ = Kind::BINARY;
    }

    ~BinaryExpr() override = default;

    BinaryExpr(BinaryExpr&&) noexcept = delete;
    BinaryExpr(BinaryExpr const&) noexcept = delete;

    BinaryExpr& operator=(BinaryExpr const&) noexcept = delete;
    BinaryExpr& operator=(BinaryExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Expr const* other) const override;
    [[nodiscard]] BinaryExpr* clone() const override;
    [[nodiscard]] Expr* getLeft() const;
    [[nodiscard]] Expr* getRight() const;
    [[nodiscard]] BinaryOp getOperator() const;
    void setLeft(Expr* l);
    void setRight(Expr* r);
    void setOperator(BinaryOp op);
}; // struct BinaryExpr

struct UnaryExpr final : public Expr {
private:
    Expr* Operand_ { nullptr };
    UnaryOp Operator_ { UnaryOp::INVALID };

public:
    UnaryExpr() = delete;

    UnaryExpr(Expr* operand, UnaryOp op)
        : Operand_(operand)
        , Operator_(op)
    {
        assert(operand != nullptr);
        Kind_ = Kind::UNARY;
    }

    ~UnaryExpr() override = default;

    UnaryExpr(UnaryExpr&&) noexcept = delete;
    UnaryExpr(UnaryExpr const&) noexcept = delete;

    UnaryExpr& operator=(UnaryExpr const&) noexcept = delete;
    UnaryExpr& operator=(UnaryExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Expr const* other) const override;
    [[nodiscard]] UnaryExpr* clone() const override;
    [[nodiscard]] Expr* getOperand() const;
    [[nodiscard]] UnaryOp getOperator() const;
}; // struct UnaryExpr

struct LiteralExpr final : public Expr {
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
        int64_t IntValue_;
        double FloatValue_;
        bool BoolValue_;
    }; // union

    StringRef StrValue_;

public:
    explicit LiteralExpr()
        : Type_(Type::NIL)
    {
        Kind_ = Kind::LITERAL;
    }

    LiteralExpr(int64_t value, Type type)
        : Type_(type)
        , IntValue_(value)
    {
        Kind_ = Kind::LITERAL;
    }
    LiteralExpr(double value, Type type)
        : Type_(type)
        , FloatValue_(value)
    {
        Kind_ = Kind::LITERAL;
    }

    explicit LiteralExpr(bool value)
        : Type_(Type::BOOLEAN)
        , BoolValue_(value)
    {
        Kind_ = Kind::LITERAL;
    }
    explicit LiteralExpr(StringRef str)
        : Type_(Type::STRING)
        , StrValue_(std::move(str))
    {
        Kind_ = Kind::LITERAL;
    }

    ~LiteralExpr() override = default;

    LiteralExpr(LiteralExpr&&) noexcept = delete;
    LiteralExpr(LiteralExpr const&) noexcept = delete;

    LiteralExpr& operator=(LiteralExpr const&) noexcept = delete;
    LiteralExpr& operator=(LiteralExpr&&) noexcept = delete;

    [[nodiscard]] Type getType() const;
    [[nodiscard]] int64_t getInt() const;
    [[nodiscard]] double getFloat() const;
    [[nodiscard]] bool getBool() const;
    [[nodiscard]] StringRef getStr() const;

    [[nodiscard]] bool isInteger() const;
    [[nodiscard]] bool isDecimal() const;
    [[nodiscard]] bool isBoolean() const;
    [[nodiscard]] bool isString() const;
    [[nodiscard]] bool isNumeric() const;
    [[nodiscard]] bool isNil() const;
    [[nodiscard]] bool equals(Expr const* other) const override;
    [[nodiscard]] LiteralExpr* clone() const override;
    [[nodiscard]] double toNumber() const;
}; // struct LiteralExpr

struct NameExpr final : public Expr {
private:
    StringRef Value_;

public:
    NameExpr() = default;

    explicit NameExpr(StringRef s)
        : Value_(std::move(s))
    {
        Kind_ = Kind::NAME;
    }

    ~NameExpr() override = default;

    NameExpr(NameExpr&&) noexcept = delete;
    NameExpr(NameExpr const&) noexcept = delete;

    NameExpr& operator=(NameExpr const&) noexcept = delete;
    NameExpr& operator=(NameExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Expr const* other) const override;
    [[nodiscard]] NameExpr* clone() const override;
    [[nodiscard]] StringRef getValue() const;
}; // struct NameExpr

struct ListExpr final : public Expr {
private:
    Array<Expr*> Elements_;

public:
    ListExpr() = default;

    explicit ListExpr(Array<Expr*> elements)
        : Elements_(std::move(elements))
    {
        Kind_ = Kind::LIST;
    }

    ~ListExpr() override = default;

    Expr* operator[](size_t const i);
    Expr const* operator[](size_t const i) const;

    ListExpr(ListExpr&&) noexcept = delete;
    ListExpr(ListExpr const&) noexcept = delete;

    ListExpr& operator=(ListExpr const&) noexcept = delete;
    ListExpr& operator=(ListExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Expr const* other) const override;
    [[nodiscard]] ListExpr* clone() const override;
    [[nodiscard]] Array<Expr*> const& getElements() const;
    [[nodiscard]] Array<Expr*>& getElements();
    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] size_t size() const;
}; // struct ListExpr

struct CallExpr final : public Expr {
public:
    enum class CallLocation : int {
        GLOBAL,
        LOCAL
    }; // enum CallLocation

private:
    Expr* Callee_ { nullptr };
    ListExpr* Args_ { nullptr };
    CallLocation CallLocation_ { CallLocation::GLOBAL }; // FIXED: Initialize member

public:
    CallExpr() = delete;

    explicit CallExpr(Expr* c, ListExpr* a = nullptr, CallLocation loc = CallLocation::GLOBAL)
        : Callee_(c)
        , Args_(a)
        , CallLocation_(loc)
    {
        if (!Args_)
            Args_ = makeList();

        assert(Callee_ != nullptr && Args_ != nullptr);
        Kind_ = Kind::CALL;
    }

    ~CallExpr() override = default;

    CallExpr(CallExpr&&) noexcept = delete;
    CallExpr(CallExpr const&) noexcept = delete;

    CallExpr& operator=(CallExpr const&) noexcept = delete;
    CallExpr& operator=(CallExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Expr const* other) const override;
    [[nodiscard]] CallExpr* clone() const override;
    [[nodiscard]] Expr* getCallee() const;
    [[nodiscard]] Array<Expr*> const& getArgs() const;
    [[nodiscard]] Array<Expr*>& getArgs();
    [[nodiscard]] ListExpr* getArgsAsListExpr();
    [[nodiscard]] ListExpr const* getArgsAsListExpr() const;
    [[nodiscard]] CallLocation getCallLocation() const;
    [[nodiscard]] bool hasArguments() const;
}; // struct CallExpr

struct AssignmentExpr final : public Expr {
private:
    Expr* Target_ { nullptr };
    Expr* Value_ { nullptr };

    bool isDecl_ = false;

public:
    AssignmentExpr() = delete;

    AssignmentExpr(Expr* target, Expr* value, bool decl = false)
        : Target_(target)
        , Value_(value)
        , isDecl_(decl)
    {
        assert(Target_ != nullptr && Value_ != nullptr);
        Kind_ = Kind::ASSIGNMENT;
    }

    ~AssignmentExpr() override = default;

    AssignmentExpr(AssignmentExpr&&) noexcept = delete;
    AssignmentExpr(AssignmentExpr const&) noexcept = delete;

    AssignmentExpr& operator=(AssignmentExpr const&) noexcept = delete;
    AssignmentExpr& operator=(AssignmentExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Expr const* other) const override;
    [[nodiscard]] AssignmentExpr* clone() const override;
    [[nodiscard]] Expr* getTarget() const;
    [[nodiscard]] Expr* getValue() const;
    void setTarget(Expr* t);
    void setValue(Expr* v);
    [[nodiscard]] bool isDeclaration() const;
}; // AssignmentExpr

struct IndexExpr final : public Expr {
private:
    Expr* Object_ { nullptr };
    Expr* Index_ { nullptr };

public:
    IndexExpr() = delete;

    IndexExpr(Expr* obj, Expr* idx)
        : Object_(obj)
        , Index_(idx)
    {
        assert(Object_ != nullptr && Index_ != nullptr);
        Kind_ = Kind::INDEX;
    }

    ~IndexExpr() override = default;

    IndexExpr(IndexExpr&&) noexcept = delete;
    IndexExpr(IndexExpr const&) noexcept = delete;

    IndexExpr& operator=(IndexExpr const&) noexcept = delete;
    IndexExpr& operator=(IndexExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Expr const* other) const override;
    [[nodiscard]] IndexExpr* clone() const override;
    [[nodiscard]] Expr* getObject() const;
    [[nodiscard]] Expr* getIndex() const;
}; // struct IndexExpr

struct Stmt : public ASTNode {
public:
    enum class Kind : uint8_t {
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
    explicit Stmt() = default;
    virtual ~Stmt() = default;

    virtual Stmt* clone() const = 0;
    virtual bool equals(Stmt const* other) const = 0;

    Kind getKind() const { return Kind_; }
    NodeType getNodeType() const override { return NodeType::STATEMENT; }
}; // struct Stmt

struct BlockStmt final : public Stmt {
private:
    Array<Stmt*> Statements_;

public:
    BlockStmt() = delete;

    explicit BlockStmt(Array<Stmt*> stmts)
        : Statements_(stmts)
    {
        Kind_ = Kind::BLOCK;
    }

    BlockStmt(BlockStmt&&) noexcept = delete;
    BlockStmt(BlockStmt const&) noexcept = delete;

    BlockStmt& operator=(BlockStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Stmt const* other) const override;
    [[nodiscard]] BlockStmt* clone() const override;
    [[nodiscard]] Array<Stmt*> const& getStatements() const;
    [[nodiscard]] bool isEmpty() const;
    void setStatements(Array<Stmt*>& stmts);
}; // struct BlockStmt

struct ExprStmt final : public Stmt {
private:
    Expr* Expr_ { nullptr };

public:
    ExprStmt() = delete;

    explicit ExprStmt(Expr* expr)
        : Expr_(expr)
    {
        assert(Expr_ != nullptr);
        Kind_ = Kind::EXPR;
    }

    ExprStmt(ExprStmt&&) noexcept = delete;
    ExprStmt(ExprStmt const&) noexcept = delete;

    ExprStmt& operator=(ExprStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Stmt const* other) const override;
    [[nodiscard]] ExprStmt* clone() const override;
    [[nodiscard]] Expr* getExpr() const;
    void setExpr(Expr* e);
}; // struct ExprStmt

struct AssignmentStmt final : public Stmt {
private:
    AssignmentExpr* Expr_;

public:
    AssignmentStmt() = delete;

    explicit AssignmentStmt(AssignmentExpr* e)
        : Expr_(e->clone())
    {
        assert(Expr_ != nullptr);
        Kind_ = Kind::ASSIGNMENT;
    }

    AssignmentStmt(Expr* target, Expr* value, bool decl = false)
    {
        Expr_ = makeAssignmentExpr(target, value, decl); // AssignmentExpr will assert args for us
        Kind_ = Kind::ASSIGNMENT;
    }

    AssignmentStmt(AssignmentStmt&&) noexcept = delete;
    AssignmentStmt(AssignmentStmt const&) noexcept = delete;

    AssignmentStmt& operator=(AssignmentStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Stmt const* other) const override;
    [[nodiscard]] AssignmentStmt* clone() const override;
    [[nodiscard]] Expr* getValue() const;
    [[nodiscard]] Expr* getTarget() const;
    [[nodiscard]] bool isDeclaration() const;
    void setValue(Expr* v);
    void setTarget(Expr* t);
}; // struct AssignmentExpr

struct IfStmt final : public Stmt {
private:
    Expr* Condition_ { nullptr };
    Stmt* ThenStmt_ { nullptr };
    Stmt* ElseStmt_ { nullptr };

public:
    IfStmt() = delete;

    IfStmt(Expr* condition, Stmt* then_stmt, Stmt* else_stmt)
        : Condition_(condition)
        , ThenStmt_(then_stmt)
        , ElseStmt_(else_stmt)
    {
        assert(Condition_ != nullptr && then_stmt != nullptr);
        Kind_ = Kind::IF;
    }

    IfStmt(IfStmt&&) noexcept = delete;
    IfStmt(IfStmt const&) noexcept = delete;

    IfStmt& operator=(IfStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Stmt const* other) const override;
    [[nodiscard]] IfStmt* clone() const override;
    [[nodiscard]] Expr* getCondition() const;
    [[nodiscard]] Stmt* getThen() const;
    [[nodiscard]] Stmt* getElse() const;
    void setThen(Stmt* t);
    void setElse(Stmt* e);
}; // struct IfStmt

struct WhileStmt final : public Stmt {
private:
    Expr* Condition_ { nullptr };
    Stmt* Body_ { nullptr };

public:
    WhileStmt() = delete;

    WhileStmt(Expr* condition, Stmt* body)
        : Condition_(condition)
        , Body_(body)
    {
        assert(Condition_ != nullptr && Body_ != nullptr);
        Kind_ = Kind::WHILE;
    }

    WhileStmt(WhileStmt&&) noexcept = delete;
    WhileStmt(WhileStmt const&) noexcept = delete;

    WhileStmt& operator=(WhileStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Stmt const* other) const override;
    [[nodiscard]] WhileStmt* clone() const override;
    [[nodiscard]] Expr* getCondition() const;
    [[nodiscard]] Stmt* getBody();
    [[nodiscard]] Stmt const* getBody() const;
    void setBody(Stmt* b);
}; // struct WhileStmt

struct ForStmt final : public Stmt {
private:
    NameExpr* Target_ { nullptr };
    Expr* Iter_ { nullptr };
    Stmt* Body_ { nullptr };

public:
    ForStmt() = delete;

    ForStmt(NameExpr* target, Expr* iter, Stmt* body)
        : Target_(target)
        , Iter_(iter)
        , Body_(body)
    {
        assert(Target_ != nullptr && Iter_ != nullptr && Body_ != nullptr);
        Kind_ = Kind::FOR;
    }

    ForStmt(ForStmt&&) noexcept = delete;
    ForStmt(ForStmt const&) noexcept = delete;

    ForStmt& operator=(ForStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Stmt const* other) const override;
    [[nodiscard]] ForStmt* clone() const override;
    [[nodiscard]] NameExpr* getTarget() const;
    [[nodiscard]] Expr* getIter() const;
    [[nodiscard]] Stmt* getBody() const;
    void setBody(Stmt* b);
}; // struct ForStmt

struct FunctionDef final : public Stmt {
private:
    NameExpr* Name_ { nullptr };
    ListExpr* Params_ { nullptr };
    Stmt* Body_ { nullptr };

public:
    FunctionDef() = delete;

    FunctionDef(NameExpr* name, ListExpr* params, Stmt* body)
        : Name_(name)
        , Params_(params)
        , Body_(body)
    {
        assert(Name_ != nullptr && Params_ != nullptr && Body_ != nullptr);
        Kind_ = Kind::FUNC;
    }

    FunctionDef(FunctionDef&&) noexcept = delete;
    FunctionDef(FunctionDef const&) noexcept = delete;

    FunctionDef& operator=(FunctionDef const&) noexcept = delete;
    [[nodiscard]] bool equals(Stmt const* other) const override;
    [[nodiscard]] FunctionDef* clone() const override;
    [[nodiscard]] NameExpr* getName() const;
    [[nodiscard]] Array<Expr*> const& getParameters() const;
    [[nodiscard]] ListExpr* getParameterList() const;
    [[nodiscard]] Stmt* getBody() const;
    [[nodiscard]] bool hasParameters() const;

    void setBody(Stmt* b);
}; // struct FunctionDef

struct ReturnStmt final : public Stmt {
private:
    Expr* Value_ { nullptr };

public:
    ReturnStmt() = delete;

    explicit ReturnStmt(Expr* value)
        : Value_(value)
    {
        assert(Value_ != nullptr);
        Kind_ = Kind::RETURN;
    }

    ReturnStmt(ReturnStmt&&) noexcept = delete;
    ReturnStmt(ReturnStmt const&) noexcept = delete;

    ReturnStmt& operator=(ReturnStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Stmt const* other) const override;
    [[nodiscard]] ReturnStmt* clone() const override;
    [[nodiscard]] Expr const* getValue() const;
    [[nodiscard]] bool hasValue() const;

    void setValue(Expr* v);
}; // struct ReturnStmt

static BinaryExpr* makeBinary(Expr* l, Expr* r, BinaryOp const op)
{
    return getAllocator().allocateObject<BinaryExpr>(l, r, op);
}
static UnaryExpr* makeUnary(Expr* operand, UnaryOp const op)
{
    return getAllocator().allocateObject<UnaryExpr>(operand, op);
}
static LiteralExpr* makeLiteralNil()
{
    return getAllocator().allocateObject<LiteralExpr>();
}
static LiteralExpr* makeLiteralInt(int value)
{
    return getAllocator().allocateObject<LiteralExpr>(static_cast<int64_t>(value), LiteralExpr::Type::INTEGER);
}
static LiteralExpr* makeLiteralInt(int64_t value)
{
    return getAllocator().allocateObject<LiteralExpr>(value, LiteralExpr::Type::INTEGER);
}
static LiteralExpr* makeLiteralFloat(double value)
{
    return getAllocator().allocateObject<LiteralExpr>(value, LiteralExpr::Type::FLOAT);
}
static LiteralExpr* makeLiteralString(StringRef value)
{
    return getAllocator().allocateObject<LiteralExpr>(value);
}
static LiteralExpr* makeLiteralBool(bool value)
{
    return getAllocator().allocateObject<LiteralExpr>(value);
}
static NameExpr* makeName(StringRef const str)
{
    return getAllocator().allocateObject<NameExpr>(str);
}
static ListExpr* makeList(Array<Expr*> elements)
{
    return getAllocator().allocateObject<ListExpr>(elements);
}
static CallExpr* makeCall(Expr* callee, ListExpr* args)
{
    return getAllocator().allocateObject<CallExpr>(callee, args);
}
static AssignmentExpr* makeAssignmentExpr(Expr* target, Expr* value, bool decl)
{
    return getAllocator().allocateObject<AssignmentExpr>(target, value, decl);
}
static IndexExpr* makeIndex(Expr* obj, Expr* idx)
{
    return getAllocator().allocateObject<IndexExpr>(obj, idx);
}
static BlockStmt* makeBlock(Array<Stmt*> stmts)
{
    return getAllocator().allocateObject<BlockStmt>(stmts);
}
static ExprStmt* makeExprStmt(Expr* expr)
{
    return getAllocator().allocateObject<ExprStmt>(expr);
}
static AssignmentStmt* makeAssignmentStmt(Expr* target, Expr* value, bool decl)
{
    return getAllocator().allocateObject<AssignmentStmt>(target, value, decl);
}
static IfStmt* makeIf(Expr* condition, Stmt* then_block, Stmt* else_block = nullptr)
{
    return getAllocator().allocateObject<IfStmt>(condition, then_block, else_block);
}
static WhileStmt* makeWhile(Expr* condition, Stmt* body)
{
    return getAllocator().allocateObject<WhileStmt>(condition, body);
}
static ForStmt* makeFor(NameExpr* target, Expr* iter, Stmt* body)
{
    return getAllocator().allocateObject<ForStmt>(target, iter, body);
}
static FunctionDef* makeFunction(NameExpr* name, ListExpr* params, Stmt* body)
{
    return getAllocator().allocateObject<FunctionDef>(name, params, body);
}
static ReturnStmt* makeReturn(Expr* value = nullptr)
{
    return getAllocator().allocateObject<ReturnStmt>(value);
}

} // namespace mylang::ast

#endif // AST_HPP
