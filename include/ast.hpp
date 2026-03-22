#ifndef AST_HPP
#define AST_HPP

#include "array.hpp"
#include "string.hpp"

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
static AssignmentExpr* makeAssignmentExpr(Expr* target, Expr* value, bool decl = true);
static AssignmentStmt* makeAssignmentStmt(Expr* target, Expr* value, bool decl = true);
static ErrExpr* makeErrExpr(uint32_t line, uint32_t col);
static ErrStmt* makeErrStmt(uint32_t line, uint32_t col);

struct ASTNode {
public:
    enum class NodeType : int {
        EXPRESSION,
        STATEMENT,

        INVALID
    };

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

    MY_NODISCARD virtual NodeType getNodeType() const;
    MY_NODISCARD uint32_t getLine() const;
    MY_NODISCARD uint16_t getColumn() const;
    void setLine(uint32_t line);
    void setColumn(uint16_t col);
    virtual ~ASTNode() = default;
};

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
};

enum class UnaryOp : uint8_t {
    OP_PLUS,
    OP_NEG,
    OP_BITNOT,
    OP_NOT,

    INVALID
};

/// NOTE: do not know if the assert for the costructors args is a good idea

struct Expr : public ASTNode {
public:
    enum class Kind : int { INVALID,
        BINARY,
        UNARY,
        LITERAL,
        NAME,
        CALL,
        ASSIGNMENT,
        LIST,
        INDEX };

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
};

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
        Kind_ = Kind::BINARY;
    }

    ~BinaryExpr() override = default;

    BinaryExpr(BinaryExpr&&) noexcept = delete;
    BinaryExpr(BinaryExpr const&) noexcept = delete;

    BinaryExpr& operator=(BinaryExpr const&) noexcept = delete;
    BinaryExpr& operator=(BinaryExpr&&) noexcept = delete;

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD BinaryExpr* clone() const override;
    MY_NODISCARD Expr* getLeft() const;
    MY_NODISCARD Expr* getRight() const;
    MY_NODISCARD BinaryOp getOperator() const;
    void setLeft(Expr* l);
    void setRight(Expr* r);
    void setOperator(BinaryOp op);
};

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
        Kind_ = Kind::UNARY;

        // assert(Operand_ && "'operand' argument to UnaryExpr is null");
    }

    ~UnaryExpr() override = default;

    UnaryExpr(UnaryExpr&&) noexcept = delete;
    UnaryExpr(UnaryExpr const&) noexcept = delete;

    UnaryExpr& operator=(UnaryExpr const&) noexcept = delete;
    UnaryExpr& operator=(UnaryExpr&&) noexcept = delete;

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD UnaryExpr* clone() const override;
    MY_NODISCARD Expr* getOperand() const;
    MY_NODISCARD UnaryOp getOperator() const;
};

struct LiteralExpr final : public Expr {
public:
    enum class Type { INTEGER,
        FLOAT,
        HEX,
        OCTAL,
        BINARY,
        STRING,
        BOOLEAN,
        NIL };

private:
    Type Type_ { Type::NIL };

    union {
        int64_t IntValue_;
        double FloatValue_;
        bool BoolValue_;
    };

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

    MY_NODISCARD Type getType() const;
    MY_NODISCARD int64_t getInt() const;
    MY_NODISCARD double getFloat() const;
    MY_NODISCARD bool getBool() const;
    MY_NODISCARD StringRef getStr() const;

    MY_NODISCARD bool isInteger() const;
    MY_NODISCARD bool isDecimal() const;
    MY_NODISCARD bool isBoolean() const;
    MY_NODISCARD bool isString() const;
    MY_NODISCARD bool isNumeric() const;
    MY_NODISCARD bool isNil() const;
    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD LiteralExpr* clone() const override;
    MY_NODISCARD double toNumber() const;
};

struct NameExpr final : public Expr {
private:
    StringRef Value_;

public:
    NameExpr() = default;

    explicit NameExpr(StringRef const& s)
        : Value_(s)
    {
        Kind_ = Kind::NAME;
        // assert(!Str_.empty() && "'Str_' argument to NameExpr is empty");
    }

    ~NameExpr() override = default;

    NameExpr(NameExpr&&) noexcept = delete;
    NameExpr(NameExpr const&) noexcept = delete;

    NameExpr& operator=(NameExpr const&) noexcept = delete;
    NameExpr& operator=(NameExpr&&) noexcept = delete;

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD NameExpr* clone() const override;
    MY_NODISCARD StringRef getValue() const;
};

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

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD ListExpr* clone() const override;
    MY_NODISCARD Array<Expr*> const& getElements() const;
    MY_NODISCARD Array<Expr*>& getElements();
    MY_NODISCARD bool isEmpty() const;
    MY_NODISCARD size_t size() const;
};

struct CallExpr final : public Expr {
public:
    enum class CallLocation : int { GLOBAL,
        LOCAL };

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
        Kind_ = Kind::CALL;
    }

    ~CallExpr() override = default;

    CallExpr(CallExpr&&) noexcept = delete;
    CallExpr(CallExpr const&) noexcept = delete;

    CallExpr& operator=(CallExpr const&) noexcept = delete;
    CallExpr& operator=(CallExpr&&) noexcept = delete;

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD CallExpr* clone() const override;
    MY_NODISCARD Expr* getCallee() const;
    MY_NODISCARD Array<Expr*> const& getArgs() const;
    MY_NODISCARD Array<Expr*>& getArgs();
    MY_NODISCARD ListExpr* getArgsAsListExpr();
    MY_NODISCARD ListExpr const* getArgsAsListExpr() const;
    MY_NODISCARD CallLocation getCallLocation() const;
    MY_NODISCARD bool hasArguments() const;
};

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
        Kind_ = Kind::ASSIGNMENT;
    }

    ~AssignmentExpr() override = default;

    AssignmentExpr(AssignmentExpr&&) noexcept = delete;
    AssignmentExpr(AssignmentExpr const&) noexcept = delete;

    AssignmentExpr& operator=(AssignmentExpr const&) noexcept = delete;
    AssignmentExpr& operator=(AssignmentExpr&&) noexcept = delete;

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD AssignmentExpr* clone() const override;
    MY_NODISCARD Expr* getTarget() const;
    MY_NODISCARD Expr* getValue() const;
    MY_NODISCARD bool isDeclaration() const;
};

struct IndexExpr final : public Expr {
private:
    Expr* Object_ { nullptr };
    Expr* Index_ { nullptr };

public:
    IndexExpr(Expr* obj, Expr* idx)
        : Object_(obj)
        , Index_(idx)
    {
        Kind_ = Kind::INDEX;
    }

    ~IndexExpr() override = default;

    IndexExpr(IndexExpr&&) noexcept = delete;
    IndexExpr(IndexExpr const&) noexcept = delete;

    IndexExpr& operator=(IndexExpr const&) noexcept = delete;
    IndexExpr& operator=(IndexExpr&&) noexcept = delete;

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD IndexExpr* clone() const override;
    MY_NODISCARD Expr* getObject() const;
    MY_NODISCARD Expr* getIndex() const;
};

struct Stmt : public ASTNode {
public:
    enum class Kind : uint8_t { INVALID,
        EXPR,
        ASSIGNMENT,
        IF,
        WHILE,
        FOR,
        FUNC,
        RETURN,
        BLOCK };

protected:
    Kind Kind_ { Kind::INVALID };

public:
    explicit Stmt() = default;
    virtual ~Stmt() = default;

    virtual Stmt* clone() const = 0;
    virtual bool equals(Stmt const* other) const = 0;

    Kind getKind() const { return Kind_; }
    NodeType getNodeType() const override { return NodeType::STATEMENT; }
};

struct BlockStmt final : public Stmt {
private:
    Array<Stmt*> Statements_;

public:
    explicit BlockStmt() = delete;
    explicit BlockStmt(Array<Stmt*> stmts)
        : Statements_(stmts)
    {
        Kind_ = Kind::BLOCK;
    }

    BlockStmt(BlockStmt&&) noexcept = delete;
    BlockStmt(BlockStmt const&) noexcept = delete;

    BlockStmt& operator=(BlockStmt const&) noexcept = delete;

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD BlockStmt* clone() const override;
    MY_NODISCARD Array<Stmt*> const& getStatements() const;
    MY_NODISCARD bool isEmpty() const;
    void setStatements(Array<Stmt*>& stmts);
};

struct ExprStmt final : public Stmt {
private:
    Expr* Expr_ { nullptr };

public:
    explicit ExprStmt() = default;
    explicit ExprStmt(Expr* expr)
        : Expr_(expr)
    {
        Kind_ = Kind::EXPR;
    }

    ExprStmt(ExprStmt&&) noexcept = delete;
    ExprStmt(ExprStmt const&) noexcept = delete;

    ExprStmt& operator=(ExprStmt const&) noexcept = delete;

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD ExprStmt* clone() const override;
    MY_NODISCARD Expr* getExpr() const;
    void setExpr(Expr* e);
};

struct AssignmentStmt final : public Stmt {
private:
    Expr* Value_ { nullptr };
    Expr* Target_ { nullptr };

    bool isDecl_ { false };

public:
    explicit AssignmentStmt() = delete;

    AssignmentStmt(Expr* target, Expr* value, bool decl = false)
        : Target_(target)
        , Value_(value)
        , isDecl_(decl)
    {
        Kind_ = Kind::ASSIGNMENT;
    }
    AssignmentStmt(AssignmentStmt&&) noexcept = delete;
    AssignmentStmt(AssignmentStmt const&) noexcept = delete;

    AssignmentStmt& operator=(AssignmentStmt const&) noexcept = delete;

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD AssignmentStmt* clone() const override;
    MY_NODISCARD Expr* getValue() const;
    MY_NODISCARD Expr* getTarget() const;
    MY_NODISCARD bool isDeclaration() const;
    void setValue(Expr* v);
    void setTarget(Expr* t);
};

struct IfStmt final : public Stmt {
private:
    Expr* Condition_ { nullptr };
    Stmt* ThenStmt_ { nullptr };
    Stmt* ElseStmt_ { nullptr };

public:
    explicit IfStmt() = delete;

    IfStmt(Expr* condition, Stmt* then_stmt, Stmt* else_stmt)
        : Condition_(condition)
        , ThenStmt_(then_stmt)
        , ElseStmt_(else_stmt)
    {
        Kind_ = Kind::IF;
    }
    IfStmt(IfStmt&&) noexcept = delete;
    IfStmt(IfStmt const&) noexcept = delete;

    IfStmt& operator=(IfStmt const&) noexcept = delete;

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD IfStmt* clone() const override;
    MY_NODISCARD Expr* getCondition() const;
    MY_NODISCARD Stmt* getThen() const;
    MY_NODISCARD Stmt* getElse() const;
    void setThen(Stmt* t);
    void setElse(Stmt* e);
};

struct WhileStmt final : public Stmt {
private:
    Expr* Condition_ { nullptr };
    Stmt* Body_ { nullptr };

public:
    explicit WhileStmt() = delete;

    WhileStmt(Expr* condition, Stmt* body)
        : Condition_(condition)
        , Body_(body)
    {
        Kind_ = Kind::WHILE;
    }
    WhileStmt(WhileStmt&&) noexcept = delete;
    WhileStmt(WhileStmt const&) noexcept = delete;

    WhileStmt& operator=(WhileStmt const&) noexcept = delete;

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD WhileStmt* clone() const override;
    MY_NODISCARD Expr* getCondition() const;
    MY_NODISCARD Stmt* getBody();
    MY_NODISCARD Stmt const* getBody() const;
    void setBody(Stmt* b);
};

struct ForStmt final : public Stmt {
private:
    NameExpr* Target_ { nullptr };
    Expr* Iter_ { nullptr };
    Stmt* Body_ { nullptr };

public:
    explicit ForStmt() = delete;

    ForStmt(NameExpr* target, Expr* iter, Stmt* body)
        : Target_(target)
        , Iter_(iter)
        , Body_(body)
    {
        Kind_ = Kind::FOR;
    }
    ForStmt(ForStmt&&) noexcept = delete;
    ForStmt(ForStmt const&) noexcept = delete;

    ForStmt& operator=(ForStmt const&) noexcept = delete;

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD ForStmt* clone() const override;
    MY_NODISCARD NameExpr* getTarget() const;
    MY_NODISCARD Expr* getIter() const;
    MY_NODISCARD Stmt* getBody() const;
    void setBody(Stmt* b);
};

struct FunctionDef final : public Stmt {
private:
    NameExpr* Name_ { nullptr };
    ListExpr* Params_ { nullptr };
    Stmt* Body_ { nullptr };

public:
    explicit FunctionDef() = delete;

    FunctionDef(NameExpr* name, ListExpr* params, Stmt* body)
        : Name_(name)
        , Params_(params)
        , Body_(body)
    {
        Kind_ = Kind::FUNC;

        if (!Params_)
            Params_ = makeList();
        if (!Body_)
            Body_ = makeBlock();
        if (!Name_)
            Name_ = makeName("");
    }
    FunctionDef(FunctionDef&&) noexcept = delete;
    FunctionDef(FunctionDef const&) noexcept = delete;

    FunctionDef& operator=(FunctionDef const&) noexcept = delete;
    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD FunctionDef* clone() const override;
    MY_NODISCARD NameExpr* getName() const;
    MY_NODISCARD Array<Expr*> const& getParameters() const;
    MY_NODISCARD ListExpr* getParameterList() const;
    MY_NODISCARD Stmt* getBody() const;
    MY_NODISCARD bool hasParameters() const;

    void setBody(Stmt* b);
};

struct ReturnStmt final : public Stmt {
private:
    Expr* Value_ { nullptr };

public:
    explicit ReturnStmt() = delete;
    explicit ReturnStmt(Expr* value)
        : Value_(value)
    {
        Kind_ = Kind::RETURN;
    }

    ReturnStmt(ReturnStmt&&) noexcept = delete;
    ReturnStmt(ReturnStmt const&) noexcept = delete;

    ReturnStmt& operator=(ReturnStmt const&) noexcept = delete;

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD ReturnStmt* clone() const override;
    MY_NODISCARD Expr const* getValue() const;
    MY_NODISCARD bool hasValue() const;

    void setValue(Expr* v);
};

struct ErrStmt final : public Stmt {
public:
    ErrStmt(uint32_t line, uint32_t col)
    {
        setLine(line);
        setColumn(col);
    }

    Stmt* clone() const override { return makeErrStmt(getLine(), getColumn()); }
    bool equals(Stmt const* other) const override { return getKind() == other->getKind(); }
};

struct ErrExpr final : public Expr {
public:
    ErrExpr(uint32_t line, uint32_t col)
    {
        setLine(line);
        setColumn(col);
    }

    Expr* clone() const override { return makeErrExpr(getLine(), getColumn()); }
    bool equals(Expr const* other) const override { return getKind() == other->getKind(); }
};

static ErrStmt* makeErrStmt(uint32_t line, uint32_t col)
{
    return getAllocator().allocateObject<ErrStmt>(line, col);
}
static ErrExpr* makeErrExpr(uint32_t line, uint32_t col)
{
    return getAllocator().allocateObject<ErrExpr>(line, col);
}
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
