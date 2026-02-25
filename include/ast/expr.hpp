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

    bool equals(Expr const* other) const override
    {
        if (other->getKind() != Kind_)
            return false;

        BinaryExpr const* bin = dynamic_cast<BinaryExpr const*>(other);
        return Operator_ == bin->getOperator()
            && Left_->equals(bin->getLeft())
            && Right_->equals(bin->getRight());
    }

    BinaryExpr* clone() const override
    {
        return AST_allocator.make<BinaryExpr>(Left_->clone(), Right_->clone(), Operator_);
    }

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

    bool equals(Expr const* other) const override
    {
        if (other->getKind() != Kind_)
            return false;

        UnaryExpr const* un = dynamic_cast<UnaryExpr const*>(other);
        return Operator_ == un->getOperator() && Operand_->equals(un->getOperand());
    }

    UnaryExpr* clone() const override
    {
        return AST_allocator.make<UnaryExpr>(Operand_->clone(), Operator_);
    }

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
        FLOAT,
        HEX,
        OCTAL,
        BINARY,
        STRING,
        BOOLEAN,
        NONE
    };

private:
    Type Type_ { Type::NONE };

    union {
        int64_t IntValue_;
        double FloatValue_;
        bool BoolValue_;
    };

    StringRef StrValue_;

public:
    LiteralExpr() = default;

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

    Type getType() const
    {
        return Type_;
    }

    int64_t getInt() const
    {
        assert(isInteger());
        return IntValue_;
    }

    double getFloat() const
    {
        assert(isDecimal());
        return FloatValue_;
    }

    bool getBool() const
    {
        assert(isBoolean());
        return BoolValue_;
    }

    StringRef getStr() const
    {
        assert(isString());
        return StrValue_;
    }

    bool isInteger() const
    {
        return Type_ == Type::INTEGER
            || Type_ == Type::HEX
            || Type_ == Type::OCTAL
            || Type_ == Type::BINARY;
    }

    bool isDecimal() const
    {
        return Type_ == Type::FLOAT;
    }

    bool isBoolean() const
    {
        return Type_ == Type::BOOLEAN;
    }

    bool isString() const
    {
        return Type_ == Type::STRING;
    }

    bool isNumeric() const
    {
        return isInteger() || isDecimal();
    }

    bool equals(Expr const* other) const override
    {
        if (!other || other->getKind() != Kind_)
            return false;

        auto const* lit = dynamic_cast<LiteralExpr const*>(other);
        if (!lit || Type_ != lit->Type_)
            return false;

        switch (Type_) {
        case Type::INTEGER:
        case Type::HEX:
        case Type::OCTAL:
        case Type::BINARY:
            return IntValue_ == lit->IntValue_;
        case Type::FLOAT:
            return FloatValue_ == lit->FloatValue_;
        case Type::BOOLEAN:
            return BoolValue_ == lit->BoolValue_;
        case Type::STRING:
            return StrValue_ == lit->StrValue_;
        case Type::NONE:
            return true;
        }
        return false;
    }

    LiteralExpr* clone() const override
    {
        switch (Type_) {
        case Type::INTEGER:
        case Type::HEX:
        case Type::OCTAL:
        case Type::BINARY:
            return AST_allocator.make<LiteralExpr>(IntValue_, Type_);
        case Type::FLOAT:
            return AST_allocator.make<LiteralExpr>(FloatValue_, Type_);
        case Type::BOOLEAN:
            return AST_allocator.make<LiteralExpr>(BoolValue_);
        case Type::STRING:
            return AST_allocator.make<LiteralExpr>(StrValue_);
        case Type::NONE:
            return AST_allocator.make<LiteralExpr>();
        }
        return nullptr; // should never happen
    }

    double toNumber() const
    {
        if (isInteger())
            return static_cast<double>(IntValue_);
        if (isDecimal())
            return FloatValue_;
        return 0.0;
    }
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

    bool equals(Expr const* other) const override
    {
        if (other->getKind() != Kind_)
            return false;

        NameExpr const* name = dynamic_cast<NameExpr const*>(other);
        return Value_ == name->getValue();
    }

    NameExpr* clone() const override
    {
        return AST_allocator.make<NameExpr>(Value_);
    }

    StringRef getValue() const
    {
        return Value_;
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

    bool equals(Expr const* other) const override
    {
        if (other->getKind() != Kind_)
            return false;

        ListExpr const* list = dynamic_cast<ListExpr const*>(other);

        if (Elements_.size() != list->Elements_.size())
            return false;

        for (size_t i = 0; i < Elements_.size(); ++i) {
            if (!Elements_[i]->equals(list->Elements_[i]))
                return false;
        }

        return true;
    }

    ListExpr* clone() const override
    {
        return AST_allocator.make<ListExpr>(Elements_);
    }

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

    bool equals(Expr const* other) const override
    {
        if (other->getKind() != Kind_)
            return false;

        CallExpr const* call = dynamic_cast<CallExpr const*>(other);
        return Callee_->equals(call->getCallee())
            && Args_->equals(call->getArgsAsListExpr())
            && CallLocation_ == call->getCallLocation();
    }

    CallExpr* clone() const override
    {
        return AST_allocator.make<CallExpr>(Callee_->clone(), Args_->clone(), CallLocation_);
    }

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

    bool equals(Expr const* other) const override
    {
        if (other->getKind() != Kind_)
            return false;

        AssignmentExpr const* bin = dynamic_cast<AssignmentExpr const*>(other);
        return Target_->equals(bin->getTarget()) && Value_->equals(bin->getValue());
    }

    AssignmentExpr* clone() const override
    {
        return AST_allocator.make<AssignmentExpr>(Target_->clone(), Value_->clone());
    }

    NameExpr* getTarget() const
    {
        return Target_;
    }

    Expr* getValue() const
    {
        return Value_;
    }
};

static constexpr BinaryExpr* makeBinary(Expr* left, Expr* right, tok::TokenType const op)
{
    return AST_allocator.make<BinaryExpr>(left, right, op);
}

static constexpr UnaryExpr* makeUnary(Expr* operand, tok::TokenType const op)
{
    return AST_allocator.make<UnaryExpr>(operand, op);
}

static constexpr LiteralExpr* makeLiteralInt(int value)
{
    return AST_allocator.make<LiteralExpr>(static_cast<int64_t>(value), LiteralExpr::Type::INTEGER);
}

static constexpr LiteralExpr* makeLiteralInt(int64_t value)
{
    return AST_allocator.make<LiteralExpr>(value, LiteralExpr::Type::INTEGER);
}

static constexpr LiteralExpr* makeLiteralFloat(double value)
{
    return AST_allocator.make<LiteralExpr>(value, LiteralExpr::Type::FLOAT);
}

static constexpr LiteralExpr* makeLiteralString(StringRef value)
{
    return AST_allocator.make<LiteralExpr>(value);
}

static constexpr LiteralExpr* makeLiteralBool(bool value)
{
    return AST_allocator.make<LiteralExpr>(value);
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
