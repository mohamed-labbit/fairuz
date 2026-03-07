// ast

#ifndef AST_HPP
#define AST_HPP

#include "allocator.hpp"
#include "string.hpp"

#include <cassert>

namespace mylang::ast {

inline Allocator AST_allocator("AST allocator");

class ASTNode {
public:
    enum class NodeType : int {
        EXPRESSION,
        STATEMENT,

        INVALID
    };

private:
    uint32_t Line_ { 0 };
    uint32_t Column_ { 0 };

    NodeType NodeType_ { NodeType::INVALID };

public:
    ASTNode() = default;
    ASTNode(ASTNode const&) = delete;
    ASTNode(ASTNode&&) = delete;

    ASTNode& operator=(ASTNode const&) = delete;
    ASTNode& operator=(ASTNode&&) = delete;

    MY_NODISCARD virtual NodeType getNodeType() const;

    MY_NODISCARD virtual uint32_t getLine() const;

    MY_NODISCARD virtual uint32_t getColumn() const;

    virtual void setLine(uint32_t line);

    virtual void setColumn(uint32_t col);

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
    OP_BITNOT,
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

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD BinaryExpr* clone() const override;
    MY_NODISCARD Expr* getLeft() const;
    MY_NODISCARD Expr* getRight() const;
    MY_NODISCARD BinaryOp getOperator() const;
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

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD UnaryExpr* clone() const override;
    MY_NODISCARD Expr* getOperand() const;
    MY_NODISCARD UnaryOp getOperator() const;
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

class NameExpr : public Expr {
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

class ListExpr : public Expr {
private:
    std::vector<Expr*> Elements_;

public:
    ListExpr() = default;

    explicit ListExpr(std::vector<Expr*> elements)
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
    MY_NODISCARD std::vector<Expr*> const& getElements() const;
    MY_NODISCARD std::vector<Expr*>& getElements();
    MY_NODISCARD bool isEmpty() const;
    MY_NODISCARD size_t size() const;
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

    explicit CallExpr(Expr* c, ListExpr* a = nullptr, CallLocation loc = CallLocation::GLOBAL)
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

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD CallExpr* clone() const override;
    MY_NODISCARD Expr* getCallee() const;
    MY_NODISCARD std::vector<Expr*> const& getArgs() const;
    MY_NODISCARD std::vector<Expr*>& getArgs();
    MY_NODISCARD ListExpr* getArgsAsListExpr();
    MY_NODISCARD ListExpr const* getArgsAsListExpr() const;
    MY_NODISCARD CallLocation getCallLocation() const;
    MY_NODISCARD bool hasArguments() const;
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

    MY_NODISCARD bool equals(Expr const* other) const override;
    MY_NODISCARD AssignmentExpr* clone() const override;
    MY_NODISCARD NameExpr* getTarget() const;
    MY_NODISCARD Expr* getValue() const;
    MY_NODISCARD bool isDeclaration() const;
};

class Stmt : public ASTNode {
public:
    enum class Kind : uint8_t {
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

    virtual ~Stmt() = default;

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

    explicit BlockStmt(std::vector<Stmt*> stmts)
        : Statements_(stmts)
    {
        Kind_ = Kind::BLOCK;
    }

    BlockStmt(BlockStmt&&) noexcept = delete;
    BlockStmt(BlockStmt const&) noexcept = delete;

    BlockStmt& operator=(BlockStmt const&) noexcept = delete;

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD BlockStmt* clone() const override;
    MY_NODISCARD std::vector<Stmt*> const& getStatements() const;
    MY_NODISCARD bool isEmpty() const;
    void setStatements(std::vector<Stmt*>& stmts);
};

class ExprStmt : public Stmt {
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

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD AssignmentStmt* clone() const override;
    MY_NODISCARD Expr* getValue() const;
    MY_NODISCARD Expr* getTarget() const;
    MY_NODISCARD bool isDeclaration() const;
    void setValue(Expr* v);
    void setTarget(NameExpr* t);
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

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD IfStmt* clone() const override;
    MY_NODISCARD Expr* getCondition() const;
    MY_NODISCARD BlockStmt* getThenBlock() const;
    MY_NODISCARD BlockStmt* getElseBlock() const;
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

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD WhileStmt* clone() const override;
    MY_NODISCARD Expr* getCondition() const;
    MY_NODISCARD BlockStmt* getBlock();
    MY_NODISCARD BlockStmt const* getBlock() const;
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

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD ForStmt* clone() const override;
    MY_NODISCARD NameExpr* getTarget() const;
    MY_NODISCARD Expr* getIter() const;
    MY_NODISCARD BlockStmt* getBlock() const;
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

    MY_NODISCARD bool equals(Stmt const* other) const override;
    MY_NODISCARD FunctionDef* clone() const override;
    MY_NODISCARD NameExpr* getName() const;
    MY_NODISCARD std::vector<Expr*> const& getParameters() const;
    MY_NODISCARD ListExpr* getParameterList() const;
    MY_NODISCARD BlockStmt* getBody() const;
    MY_NODISCARD bool hasParameters() const;
    void setBody(BlockStmt* b);
};

class ReturnStmt : public Stmt {
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

static constexpr BinaryExpr* makeBinary(Expr* l, Expr* r, BinaryOp const op) { return AST_allocator.allocateObject<BinaryExpr>(l, r, op); }
static constexpr UnaryExpr* makeUnary(Expr* operand, UnaryOp const op) { return AST_allocator.allocateObject<UnaryExpr>(operand, op); }
static constexpr LiteralExpr* makeLiteralNil() { return AST_allocator.allocateObject<LiteralExpr>(); }
static constexpr LiteralExpr* makeLiteralInt(int value) { return AST_allocator.allocateObject<LiteralExpr>(static_cast<int64_t>(value), LiteralExpr::Type::INTEGER); }
static constexpr LiteralExpr* makeLiteralInt(int64_t value) { return AST_allocator.allocateObject<LiteralExpr>(value, LiteralExpr::Type::INTEGER); }
static constexpr LiteralExpr* makeLiteralFloat(double value) { return AST_allocator.allocateObject<LiteralExpr>(value, LiteralExpr::Type::FLOAT); }
static constexpr LiteralExpr* makeLiteralString(StringRef value) { return AST_allocator.allocateObject<LiteralExpr>(value); }
static constexpr LiteralExpr* makeLiteralBool(bool value) { return AST_allocator.allocateObject<LiteralExpr>(value); }
static constexpr NameExpr* makeName(StringRef const str) { return AST_allocator.allocateObject<NameExpr>(str); }
static constexpr ListExpr* makeList(std::vector<Expr*> elements) { return AST_allocator.allocateObject<ListExpr>(elements); }
static constexpr CallExpr* makeCall(Expr* callee, ListExpr* args) { return AST_allocator.allocateObject<CallExpr>(callee, args); }
static constexpr AssignmentExpr* makeAssignmentExpr(NameExpr* target, Expr* value, bool decl) { return AST_allocator.allocateObject<AssignmentExpr>(target, value, decl); }
static constexpr BlockStmt* makeBlock(std::vector<Stmt*> stmts) { return AST_allocator.allocateObject<BlockStmt>(stmts); }
static constexpr ExprStmt* makeExprStmt(Expr* expr) { return AST_allocator.allocateObject<ExprStmt>(expr); }
static constexpr AssignmentStmt* makeAssignmentStmt(Expr* target, Expr* value, bool decl) { return AST_allocator.allocateObject<AssignmentStmt>(target, value, decl); }
static constexpr IfStmt* makeIf(Expr* condition, BlockStmt* then_block, BlockStmt* else_block) { return AST_allocator.allocateObject<IfStmt>(condition, then_block, else_block); }
static constexpr WhileStmt* makeWhile(Expr* condition, BlockStmt* block) { return AST_allocator.allocateObject<WhileStmt>(condition, block); }
static constexpr ForStmt* makeFor(NameExpr* target, Expr* iter, BlockStmt* block) { return AST_allocator.allocateObject<ForStmt>(target, iter, block); }
static constexpr FunctionDef* makeFunction(NameExpr* name, ListExpr* params, BlockStmt* body) { return AST_allocator.allocateObject<FunctionDef>(name, params, body); }
static constexpr ReturnStmt* makeReturn(Expr* value) { return AST_allocator.allocateObject<ReturnStmt>(value); }

}

#endif // AST_HPP
