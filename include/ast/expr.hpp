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
    StringRef Str_;

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

        assert(Left_ && "'left' argument to BinaryExpr is null");
        assert(Right_ && "'right' argument to BinaryExpr is null");
    }

    ~BinaryExpr() override = default;

    BinaryExpr(BinaryExpr&&) MYLANG_NOEXCEPT = delete;
    BinaryExpr(BinaryExpr const&) MYLANG_NOEXCEPT = delete;
    BinaryExpr& operator=(BinaryExpr const&) MYLANG_NOEXCEPT = delete;
    BinaryExpr& operator=(BinaryExpr&&) MYLANG_NOEXCEPT = delete;

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
        assert(Operand_ && "'operand' argument to UnaryExpr is null");
    }

    ~UnaryExpr() override = default;

    UnaryExpr(UnaryExpr&&) MYLANG_NOEXCEPT = delete;
    UnaryExpr(UnaryExpr const&) MYLANG_NOEXCEPT = delete;
    UnaryExpr& operator=(UnaryExpr const&) MYLANG_NOEXCEPT = delete;
    UnaryExpr& operator=(UnaryExpr&&) MYLANG_NOEXCEPT = delete;

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
    enum class Type { NUMBER,
        STRING,
        BOOLEAN,
        NONE };

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

    LiteralExpr(LiteralExpr&&) MYLANG_NOEXCEPT = delete;
    LiteralExpr(LiteralExpr const&) MYLANG_NOEXCEPT = delete;
    LiteralExpr& operator=(LiteralExpr const&) MYLANG_NOEXCEPT = delete;
    LiteralExpr& operator=(LiteralExpr&&) MYLANG_NOEXCEPT = delete;

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
        return Type_ == Type::NUMBER;
    }

    float toNumber() const
    {
        return std::stof(getValue().data());
    }
};

class NameExpr : public Expr {
public:
    NameExpr() = default;

    // FIXED: Properly initialize Kind_ before assertion
    explicit NameExpr(Expr const* e)
        : Expr(e->getStr())
    {
        Kind_ = Kind::NAME;
        assert(!Str_.empty() && "'Str_' argument to NameExpr is empty");
    }

    explicit NameExpr(StringRef const s)
        : Expr(s)
    {
        Kind_ = Kind::NAME;
        assert(!Str_.empty() && "'Str_' argument to NameExpr is empty");
    }

    ~NameExpr() override = default;

    NameExpr(NameExpr&&) MYLANG_NOEXCEPT = delete;
    NameExpr(NameExpr const&) MYLANG_NOEXCEPT = delete;
    NameExpr& operator=(NameExpr const&) MYLANG_NOEXCEPT = delete;
    NameExpr& operator=(NameExpr&&) MYLANG_NOEXCEPT = delete;

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

    explicit ListExpr(std::vector<Expr*> elements)
        : Elements_(elements)
    {
        Kind_ = Kind::LIST;
        // Note: Empty lists can be valid
    }

    ~ListExpr() override = default;

    // FIXED: Added const version of operator[]
    Expr* operator[](SizeType const i)
    {
        return Elements_[i];
    }

    Expr const* operator[](SizeType const i) const
    {
        return Elements_[i];
    }

    ListExpr(ListExpr&&) MYLANG_NOEXCEPT = delete;
    ListExpr(ListExpr const&) MYLANG_NOEXCEPT = delete;
    ListExpr& operator=(ListExpr const&) MYLANG_NOEXCEPT = delete;
    ListExpr& operator=(ListExpr&&) MYLANG_NOEXCEPT = delete;

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

    SizeType size() const
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

    explicit CallExpr(Expr* c, ListExpr* a = nullptr, CallLocation loc = CallLocation::GLOBAL)
        : Callee_(c)
        , Args_(a)
        , CallLocation_(loc)
    {
        Kind_ = Kind::CALL;
        assert(Callee_ && "'callee' argument to CallExpr is null");
    }

    ~CallExpr() override = default;

    CallExpr(CallExpr&&) MYLANG_NOEXCEPT = delete;
    CallExpr(CallExpr const&) MYLANG_NOEXCEPT = delete;
    CallExpr& operator=(CallExpr const&) MYLANG_NOEXCEPT = delete;
    CallExpr& operator=(CallExpr&&) MYLANG_NOEXCEPT = delete;

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

    // FIXED: Corrected logic - returns true when there ARE arguments
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

        assert(Target_ && "'target' argument to AssignmentExpr is null");
        assert(Value_ && "'value' argument to AssignmentExpr is null");
    }

    ~AssignmentExpr() override = default;

    AssignmentExpr(AssignmentExpr&&) MYLANG_NOEXCEPT = delete;
    AssignmentExpr(AssignmentExpr const&) MYLANG_NOEXCEPT = delete;
    AssignmentExpr& operator=(AssignmentExpr const&) MYLANG_NOEXCEPT = delete;
    AssignmentExpr& operator=(AssignmentExpr&&) MYLANG_NOEXCEPT = delete;

    NameExpr* getTarget() const
    {
        return Target_;
    }

    Expr* getValue() const
    {
        return Value_;
    }
};

} // namespace ast
} // namespace mylang
