#pragma once

#include "../diag/diagnostic.hpp"
#include "../lex/token.hpp"
#include "ast_node.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace mylang {
namespace ast {

/// NOTE: do not know if the assert for the costructors args is a good idea

class Expr : public ASTNode {
public:
    enum class Kind : int { INVALID,
        BINARY,
        UNARY,
        LITERAL,
        NAME,
        CALL,
        ASSIGNMENT,
        LIST };

protected:
    Kind Kind_ { Kind::INVALID };
    StringRef Str_ { "" };

public:
    Expr() = default;

    explicit Expr(StringRef s)
        : Kind_(Kind::INVALID)
        , Str_(s)
    {
    }

    virtual ~Expr() = default;

    void setStr(StringRef const s)
    {
        Str_ = s;
    }

    StringRef getStr() const
    {
        return Str_;
    }

    Kind getKind() const
    {
        return Kind_;
    }

    NodeType getNodeType() const override
    {
        return NodeType::EXPRESSION;
    }
};

class BinaryExpr : public Expr {
private:
    Expr* Left_ { nullptr };
    Expr* Right_ { nullptr };
    tok::TokenType Operator_ { tok::TokenType::INVALID };

public:
    BinaryExpr() = delete;

    BinaryExpr(Expr* left, Expr* right, tok::TokenType op)
        : Left_(left)
        , Right_(right)
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

    Expr* getLeft() const
    {
        return Left_;
    }

    Expr* getRight() const
    {
        return Right_;
    }

    tok::TokenType getOperator() const
    {
        return Operator_;
    }

    void setLeft(Expr* left)
    {
        Left_ = left;
    }

    void setRight(Expr* right)
    {
        Right_ = right;
    }

    void setOperator(tok::TokenType op)
    {
        Operator_ = op;
    }
};

class UnaryExpr : public Expr {
private:
    Expr* Operand_ { nullptr };
    tok::TokenType Operator_ { tok::TokenType::INVALID };

public:
    UnaryExpr() = delete;

    UnaryExpr(Expr* operand, tok::TokenType op)
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

    Expr* getOperand() const
    {
        return Operand_;
    }

    tok::TokenType getOperator() const
    {
        return Operator_;
    }
};

class LiteralExpr : public Expr {
public:
    enum class Type {
        INTEGER,
        DECIMAL,
        HEX,
        OCTAL,
        BINARY,

        STRING,
        BOOLEAN,
        NONE
    };

private:
    Type Type_ { Type::NONE };
    StringRef Literal_;

public:
    LiteralExpr() = default;

    LiteralExpr(Type t, StringRef lit)
        : Type_(t)
        , Literal_(lit)
    {
        Kind_ = Kind::LITERAL;
        // Note: Empty literals can be valid (e.g., empty strings)
    }

    ~LiteralExpr() override = default;

    LiteralExpr(LiteralExpr&&) noexcept = delete;
    LiteralExpr(LiteralExpr const&) noexcept = delete;

    LiteralExpr& operator=(LiteralExpr const&) noexcept = delete;
    LiteralExpr& operator=(LiteralExpr&&) noexcept = delete;

    StringRef getValue() const
    {
        return Literal_;
    }

    Type getType() const
    {
        return Type_;
    }

    bool isNumeric() const
    {
        return Type_ == Type::INTEGER
            || Type_ == Type::DECIMAL
            || Type_ == Type::HEX
            || Type_ == Type::OCTAL
            || Type_ == Type::BINARY;
    }

    float toNumber() const
    {
        return std::stof(getValue().data());
    }
};

class NameExpr : public Expr {
public:
    NameExpr() = default;

    explicit NameExpr(Expr const* e)
        : Expr(e->getStr())
    {
        Kind_ = Kind::NAME;
        // assert(!Str_.empty() && "'Str_' argument to NameExpr is empty");
    }

    NameExpr(StringRef const s)
        : Expr(s)
    {
        Kind_ = Kind::NAME;
        // assert(!Str_.empty() && "'Str_' argument to NameExpr is empty");
    }

    ~NameExpr() override = default;

    NameExpr(NameExpr&&) noexcept = delete;
    NameExpr(NameExpr const&) noexcept = delete;

    NameExpr& operator=(NameExpr const&) noexcept = delete;
    NameExpr& operator=(NameExpr&&) noexcept = delete;

    StringRef getValue() const
    {
        return Str_;
    }
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

    Expr* operator[](std::size_t const i)
    {
        return Elements_[i];
    }

    Expr const* operator[](std::size_t const i) const
    {
        return Elements_[i];
    }

    ListExpr(ListExpr&&) noexcept = delete;
    ListExpr(ListExpr const&) noexcept = delete;

    ListExpr& operator=(ListExpr const&) noexcept = delete;
    ListExpr& operator=(ListExpr&&) noexcept = delete;

    std::vector<Expr*> const& getElements() const
    {
        return Elements_;
    }

    std::vector<Expr*>& getElementsMutable()
    {
        return Elements_;
    }

    bool isEmpty() const
    {
        return Elements_.empty();
    }

    std::size_t size() const
    {
        return Elements_.size();
    }
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

    Expr* getCallee() const
    {
        return Callee_;
    }

    std::vector<Expr*> const& getArgs() const
    {
        static std::vector<Expr*> const empty;
        return Args_ ? Args_->getElements() : empty;
    }

    std::vector<Expr*>& getArgsMutable()
    {
        assert(Args_ && "Attempting to get mutable args when Args_ is null");
        return Args_->getElementsMutable();
    }

    ListExpr* getArgsAsListExpr()
    {
        return Args_;
    }

    ListExpr const* getArgsAsListExpr() const
    {
        return Args_;
    }

    CallLocation getCallLocation() const
    {
        return CallLocation_;
    }

    bool hasArguments() const
    {
        return Args_ && !Args_->isEmpty();
    }
};

class AssignmentExpr : public Expr {
private:
    NameExpr* Target_ { nullptr };
    Expr* Value_ { nullptr };

public:
    AssignmentExpr() = delete;

    AssignmentExpr(NameExpr* target, Expr* value)
        : Target_(target)
        , Value_(value)
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

    NameExpr* getTarget() const
    {
        return Target_;
    }

    Expr* getValue() const
    {
        return Value_;
    }
};

static constexpr Expr* makeExpr(StringRef const str)
{
    return AST_allocator.make<Expr>(str);
}

static constexpr BinaryExpr* makeBinary(Expr* left, Expr* right, tok::TokenType const op)
{
    return AST_allocator.make<BinaryExpr>(left, right, op);
}

static constexpr UnaryExpr* makeUnary(Expr* operand, tok::TokenType const op)
{
    return AST_allocator.make<UnaryExpr>(operand, op);
}

static constexpr LiteralExpr* makeLiteral(LiteralExpr::Type type, StringRef const str)
{
    return AST_allocator.make<LiteralExpr>(type, str);
}

static constexpr NameExpr* makeName(StringRef const str)
{
    return AST_allocator.make<NameExpr>(str);
}

static constexpr ListExpr* makeList(std::vector<Expr*> elements)
{
    return AST_allocator.make<ListExpr>(elements);
}

static constexpr CallExpr* makeCall(Expr* callee, ListExpr* args = nullptr, CallExpr::CallLocation loc = CallExpr::CallLocation::GLOBAL)
{
    return AST_allocator.make<CallExpr>(callee, args, loc);
}

static constexpr AssignmentExpr* makeAssignmentExpr(NameExpr* target, Expr* value)
{
    return AST_allocator.make<AssignmentExpr>(target, value);
}

} // namespace ast
} // namespace mylang
