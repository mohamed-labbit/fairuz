// ast

#pragma once

#include "../allocator.hpp"
#include "../types/string.hpp"

#include <cassert>

namespace mylang {
namespace ast {

inline Allocator AST_allocator("AST allocator");

class ASTNode {
public:
    enum NodeType {
        EXPRESSION,
        STATEMENT
    };

private:
    uint32_t Line_ { 0 };
    uint32_t Column_ { 0 };

    NodeType NodeType_;

public:
    ASTNode() = default;
    ASTNode(ASTNode const&) = delete;
    ASTNode(ASTNode&&) = delete;

    ASTNode& operator=(ASTNode const&) = delete;
    ASTNode& operator=(ASTNode&&) = delete;

    virtual NodeType getNodeType() const;

    virtual uint32_t getLine() const;

    virtual uint32_t getColumn() const;

    virtual void setLine(uint32_t const l);

    virtual void setColumn(uint32_t const c);

    ~ASTNode() = default;
};

enum class BinaryOp {
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
    OP_BITNOT,
    OP_LSHIFT,
    OP_RSHIFT,
    OP_AND,
    OP_OR,

    INVALID
};

enum class UnaryOp {
    OP_PLUS,
    OP_NEG,
    OP_BITNOT,
    OP_NOT,

    INVALID
};

/// NOTE: do not know if the assert for the costructors args is a good idea

// forward
class Expr;
class BinaryExpr;
class UnaryExpr;
class LiteralExpr;
class NameExpr;
class ListExpr;
class CallExpr;
class AssignmentExpr;
class Stmt;
class BlockStmt;
class ExprStmt;
class AssignmentStmt;
class IfStmt;
class WhileStmt;
class ForStmt;
class FunctionDef;
class ReturnStmt;

static constexpr BinaryExpr* makeBinary(Expr* l, Expr* r, BinaryOp const op);
static constexpr UnaryExpr* makeUnary(Expr* operand, UnaryOp const op);
static constexpr LiteralExpr* makeLiteralNil();
static constexpr LiteralExpr* makeLiteralInt(int value);
static constexpr LiteralExpr* makeLiteralInt(int64_t value);
static constexpr LiteralExpr* makeLiteralFloat(double value);
static constexpr LiteralExpr* makeLiteralString(StringRef value);
static constexpr LiteralExpr* makeLiteralBool(bool value);
static constexpr NameExpr* makeName(StringRef const str);
static constexpr ListExpr* makeList(std::vector<Expr*> elements = { });
static constexpr CallExpr* makeCall(Expr* callee, ListExpr* args = nullptr);
static constexpr AssignmentExpr* makeAssignmentExpr(NameExpr* target, Expr* value, bool decl = false);
static constexpr BlockStmt* makeBlock(std::vector<Stmt*> stmts = { });
static constexpr ExprStmt* makeExprStmt(Expr* expr);
static constexpr AssignmentStmt* makeAssignmentStmt(Expr* target, Expr* value, bool decl = false);
static constexpr IfStmt* makeIf(Expr* condition, BlockStmt* then_block, BlockStmt* else_block = nullptr);
static constexpr WhileStmt* makeWhile(Expr* condition, BlockStmt* block);
static constexpr ForStmt* makeFor(NameExpr* target, Expr* iter, BlockStmt* block);
static constexpr FunctionDef* makeFunction(NameExpr* name, ListExpr* params, BlockStmt* body);
static constexpr ReturnStmt* makeReturn(Expr* value);

class Expr : public ASTNode {
public:
    enum class Kind : int {
        INVALID,
        BINARY,
        UNARY,
        LITERAL,
        NAME,
        CALL,
        ASSIGNMENT,
        LIST
    };

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

class BinaryExpr : public Expr {
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

        // assert(Left_ && "'left' argument to BinaryExpr is null");
        // assert(Right_ && "'right' argument to BinaryExpr is null");
    }

    ~BinaryExpr() override = default;

    BinaryExpr(BinaryExpr&&) noexcept = delete;
    BinaryExpr(BinaryExpr const&) noexcept = delete;

    BinaryExpr& operator=(BinaryExpr const&) noexcept = delete;
    BinaryExpr& operator=(BinaryExpr&&) noexcept = delete;

    bool equals(Expr const* other) const override;
    BinaryExpr* clone() const override;
    Expr* getLeft() const;
    Expr* getRight() const;
    BinaryOp getOperator() const;
    void setLeft(Expr* l);
    void setRight(Expr* r);
    void setOperator(BinaryOp op);
};

class UnaryExpr : public Expr {
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

    bool equals(Expr const* other) const override;
    UnaryExpr* clone() const override;
    Expr* getOperand() const;
    UnaryOp getOperator() const;
};

class LiteralExpr : public Expr {
public:
    enum class Type {
        INTEGER,
        FLOAT,
        HEX,
        OCTAL,
        BINARY,
        STRING,
        BOOLEAN,
        NIL
    };

private:
    Type Type_ { Type::NIL };

    union {
        int64_t IntValue_;
        double FloatValue_;
        bool BoolValue_;
    };

    StringRef StrValue_;

public:
    LiteralExpr()
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

    LiteralExpr(bool value)
        : Type_(Type::BOOLEAN)
        , BoolValue_(value)
    {
        Kind_ = Kind::LITERAL;
    }

    LiteralExpr(StringRef str)
        : Type_(Type::STRING)
        , StrValue_(str)
    {
        Kind_ = Kind::LITERAL;
    }

    ~LiteralExpr() override = default;

    LiteralExpr(LiteralExpr&&) noexcept = delete;
    LiteralExpr(LiteralExpr const&) noexcept = delete;

    LiteralExpr& operator=(LiteralExpr const&) noexcept = delete;
    LiteralExpr& operator=(LiteralExpr&&) noexcept = delete;

    Type getType() const;
    int64_t getInt() const;
    double getFloat() const;
    bool getBool() const;
    StringRef getStr() const;
    bool isInteger() const;

    bool isDecimal() const;
    bool isBoolean() const;
    bool isString() const;
    bool isNumeric() const;
    bool isNil() const;

    bool equals(Expr const* other) const override;
    LiteralExpr* clone() const override;
    double toNumber() const;
};

class NameExpr : public Expr {
private:
    StringRef Value_;

public:
    NameExpr() = default;

    NameExpr(StringRef const s)
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

    bool equals(Expr const* other) const override;
    NameExpr* clone() const override;
    StringRef getValue() const;
};

class ListExpr : public Expr {
private:
    std::vector<Expr*> Elements_;

public:
    ListExpr() = default;

    ListExpr(std::vector<Expr*> elements)
        : Elements_(elements)
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

    bool equals(Expr const* other) const override;
    ListExpr* clone() const override;
    std::vector<Expr*> const& getElements() const;
    std::vector<Expr*>& getElements();
    bool isEmpty() const;
    size_t size() const;
};

class CallExpr : public Expr {
public:
    enum class CallLocation : int { GLOBAL,
        LOCAL };

private:
    Expr* Callee_ { nullptr };
    ListExpr* Args_ { nullptr };
    CallLocation CallLocation_ { CallLocation::GLOBAL }; // FIXED: Initialize member

public:
    CallExpr() = delete;

    CallExpr(Expr* c, ListExpr* a = nullptr, CallLocation loc = CallLocation::GLOBAL)
        : Callee_(c)
        , Args_(a)
        , CallLocation_(loc)
    {
        Kind_ = Kind::CALL;
        // assert(Callee_ && "'callee' argument to CallExpr is null");
    }

    ~CallExpr() override = default;

    CallExpr(CallExpr&&) noexcept = delete;
    CallExpr(CallExpr const&) noexcept = delete;

    CallExpr& operator=(CallExpr const&) noexcept = delete;
    CallExpr& operator=(CallExpr&&) noexcept = delete;

    bool equals(Expr const* other) const override;
    CallExpr* clone() const override;
    Expr* getCallee() const;
    std::vector<Expr*> const& getArgs() const;
    std::vector<Expr*>& getArgs();
    ListExpr* getArgsAsListExpr();
    ListExpr const* getArgsAsListExpr() const;
    CallLocation getCallLocation() const;
    bool hasArguments() const;
};

class AssignmentExpr : public Expr {
private:
    NameExpr* Target_ { nullptr };
    Expr* Value_ { nullptr };

    bool isDecl_ = false;

public:
    AssignmentExpr() = delete;

    AssignmentExpr(NameExpr* target, Expr* value, bool decl = false)
        : Target_(target)
        , Value_(value)
        , isDecl_(decl)
    {
        Kind_ = Kind::ASSIGNMENT;

        // assert(Target_ && "'target' argument to AssignmentExpr is null");
        // assert(Value_ && "'value' argument to AssignmentExpr is null");
    }

    ~AssignmentExpr() override = default;

    AssignmentExpr(AssignmentExpr&&) noexcept = delete;
    AssignmentExpr(AssignmentExpr const&) noexcept = delete;

    AssignmentExpr& operator=(AssignmentExpr const&) noexcept = delete;
    AssignmentExpr& operator=(AssignmentExpr&&) noexcept = delete;

    bool equals(Expr const* other) const override;
    AssignmentExpr* clone() const override;
    NameExpr* getTarget() const;
    Expr* getValue() const;
    bool isDeclaration() const;
};

class Stmt : public ASTNode {
public:
    enum class Kind : int {
        INVALID,
        EXPR,
        ASSIGNMENT,
        IF,
        WHILE,
        FOR,
        FUNC,
        RETURN,
        BLOCK
    };

protected:
    Kind Kind_ { Kind::INVALID };

public:
    explicit Stmt() = default;

    virtual Stmt* clone() const = 0;

    virtual bool equals(Stmt const* other) const = 0;

    Kind getKind() const { return Kind_; }

    NodeType getNodeType() const override { return NodeType::STATEMENT; }
};

class BlockStmt : public Stmt {
private:
    std::vector<Stmt*> Statements_;

public:
    explicit BlockStmt() = delete;

    BlockStmt(std::vector<Stmt*> stmts)
        : Statements_(stmts)
    {
        Kind_ = Kind::BLOCK;
    }

    BlockStmt(BlockStmt&&) noexcept = delete;
    BlockStmt(BlockStmt const&) noexcept = delete;

    BlockStmt& operator=(BlockStmt const&) noexcept = delete;

    bool equals(Stmt const* other) const override;
    BlockStmt* clone() const override;
    std::vector<Stmt*> const& getStatements() const;
    void setStatements(std::vector<Stmt*>& stmts);
    bool isEmpty() const;
};

class ExprStmt : public Stmt {
private:
    Expr* Expr_ { nullptr };

public:
    explicit ExprStmt() = default;

    ExprStmt(Expr* expr)
        : Expr_(expr)
    {
        Kind_ = Kind::EXPR;
    }

    ExprStmt(ExprStmt&&) noexcept = delete;
    ExprStmt(ExprStmt const&) noexcept = delete;

    ExprStmt& operator=(ExprStmt const&) noexcept = delete;

    bool equals(Stmt const* other) const override;
    ExprStmt* clone() const override;
    Expr* getExpr() const;
    void setExpr(Expr* e);
};

class AssignmentStmt : public Stmt {
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

    bool equals(Stmt const* other) const override;
    AssignmentStmt* clone() const override;
    Expr* getValue() const;
    Expr* getTarget() const;
    void setValue(Expr* v);
    void setTarget(NameExpr* t);
    bool isDeclaration() const;
};

class IfStmt : public Stmt {
private:
    Expr* Condition_ { nullptr };
    BlockStmt* ThenBlock_ { nullptr };
    BlockStmt* ElseBlock_ { nullptr };

public:
    explicit IfStmt() = delete;

    IfStmt(Expr* condition, BlockStmt* then_block, BlockStmt* else_block)
        : Condition_(condition)
        , ThenBlock_(then_block)
        , ElseBlock_(else_block)
    {
        Kind_ = Kind::IF;
    }

    IfStmt(IfStmt&&) noexcept = delete;
    IfStmt(IfStmt const&) noexcept = delete;

    IfStmt& operator=(IfStmt const&) noexcept = delete;

    bool equals(Stmt const* other) const override;
    IfStmt* clone() const override;
    Expr* getCondition() const;
    BlockStmt* getThenBlock() const;
    BlockStmt* getElseBlock() const;
    void setThenBlock(BlockStmt* t);
    void setElseBlock(BlockStmt* e);
};

class WhileStmt : public Stmt {
private:
    Expr* Condition_ { nullptr };
    BlockStmt* Block_ { nullptr };

public:
    explicit WhileStmt() = delete;

    WhileStmt(Expr* condition, BlockStmt* block)
        : Condition_(condition)
        , Block_(block)
    {
        Kind_ = Kind::WHILE;
    }

    WhileStmt(WhileStmt&&) noexcept = delete;
    WhileStmt(WhileStmt const&) noexcept = delete;

    WhileStmt& operator=(WhileStmt const&) noexcept = delete;

    bool equals(Stmt const* other) const override;
    WhileStmt* clone() const override;
    Expr* getCondition() const;
    BlockStmt* getBlock();
    BlockStmt const* getBlock() const;
    void setBlock(BlockStmt* b);
};

class ForStmt : public Stmt {
private:
    NameExpr* Target_ { nullptr };
    Expr* Iter_ { nullptr };
    BlockStmt* Block_ { nullptr };

public:
    explicit ForStmt() = delete;

    ForStmt(NameExpr* target, Expr* iter, BlockStmt* block)
        : Target_(target)
        , Iter_(iter)
        , Block_(block)
    {
        Kind_ = Kind::FOR;
    }

    ForStmt(ForStmt&&) noexcept = delete;
    ForStmt(ForStmt const&) noexcept = delete;

    ForStmt& operator=(ForStmt const&) noexcept = delete;

    bool equals(Stmt const* other) const override;
    ForStmt* clone() const override;
    NameExpr* getTarget() const;
    Expr* getIter() const;
    BlockStmt* getBlock() const;
    void setBlock(BlockStmt* b);
};

class FunctionDef : public Stmt {
private:
    NameExpr* Name_ { nullptr };
    ListExpr* Params_ { nullptr };
    BlockStmt* Body_ { nullptr };

public:
    explicit FunctionDef() = delete;

    FunctionDef(NameExpr* name, ListExpr* params, BlockStmt* body)
        : Name_(name)
        , Params_(params)
        , Body_(body)
    {
        Kind_ = Kind::FUNC;

        if (!Params_)
            Params_ = makeList(std::vector<Expr*> { });
        if (!Body_)
            Body_ = makeBlock(std::vector<Stmt*> { });
        if (!Name_)
            Name_ = makeName("");
    }

    FunctionDef(FunctionDef&&) noexcept = delete;
    FunctionDef(FunctionDef const&) noexcept = delete;

    FunctionDef& operator=(FunctionDef const&) noexcept = delete;

    bool equals(Stmt const* other) const override;
    FunctionDef* clone() const override;
    NameExpr* getName() const;
    std::vector<Expr*> const& getParameters() const;
    ListExpr* getParameterList() const;
    BlockStmt* getBody() const;
    void setBody(BlockStmt* b);
    bool hasParameters() const;
};

class ReturnStmt : public Stmt {
private:
    Expr* Value_ { nullptr };

public:
    explicit ReturnStmt() = delete;

    ReturnStmt(Expr* value)
        : Value_(value)
    {
        Kind_ = Kind::RETURN;
    }

    ReturnStmt(ReturnStmt&&) noexcept = delete;
    ReturnStmt(ReturnStmt const&) noexcept = delete;

    ReturnStmt& operator=(ReturnStmt const&) noexcept = delete;

    bool equals(Stmt const* other) const override;
    ReturnStmt* clone() const override ;
    Expr const* getValue() const ;
    void setValue(Expr* v);
    bool hasValue() const;
};

}
}
