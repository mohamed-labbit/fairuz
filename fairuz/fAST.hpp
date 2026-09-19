#ifndef FA_AST_HPP
#define FA_AST_HPP

#include "farena.hpp"
#include "farray.hpp"
#include "fmacros.hpp"
#include "fstring.hpp"

#include <cassert>
#include <cstddef>

#define ALLOCATE_AST_NODE(type, ...) get_allocator().allocate_object<type>(__VA_ARGS__)

namespace fairuz::AST {

/// INVARIANT: All AST nodes must be valid pointers (not nullptr)
/// and all their children must also be valid, any node that can be empty
/// like a list of arguments that is empty should be represented by a valid
/// but empty AST node, therefore the compiler can assume all nodes are valid

class Expr;
class Stmt;
class BinaryExpr;
class UnaryExpr;
class IntLiteralExpr;
class FloatLiteralExpr;
class StringLiteralExpr;
class BoolLiteralExpr;
class NilExpr;
class IdentifierExpr;
class CallExpr;
class AssignExpr;
class ListExpr;
class IndexExpr;
class DictExpr;
class GetExpr;

class ExprStmt;
class AssignStmt;
class IfElseStmt;
class WhileStmt;
class ForStmt;
class FuncDefStmt;
class ReturnStmt;
class BreakStmt;
class ContinueStmt;
class BlockStmt;
class ClassDefStmt;
class ImportStmt;

using ExprPtr = Expr*;
using StmtPtr = Stmt*;
using ConstExprPtr = Expr*;
using ConstStmtPtr = Stmt*;

/// AST Node that is the parent class of any derived node in this file
/// NOTE: An AST node pointer must always be a valid pointer (not nullptr)
/// this is should be guaranteed by the parser, subsequent passes that
/// use the AST such as the Compiler and Optimizer do not check for null
/// for each node they process

class ASTNode {
public:
    enum class NodeType : int {
        EXPR,
        STMT,
        INVALID
    }; // enum NodeType

private:
    NodeType node_type { NodeType::INVALID };
    SourceLocation m_loc { };

public:
    ASTNode() = default;
    ASTNode(SourceLocation loc)
        : m_loc(loc)
    {
    }

    /// Nodes are not implicitly nor explicitly copyable
    /// each node is referred to by a constant pointer that must be valid
    /// so the only way to copy data must be using 'clone' below
    ASTNode(ASTNode const&) = delete;
    ASTNode(ASTNode&&) = delete;

    ASTNode& operator=(ASTNode const&) = delete;
    ASTNode& operator=(ASTNode&&) = delete;

    [[nodiscard]] virtual NodeType get_node_type() const { return node_type; }
    [[nodiscard]] u32 get_line() const;
    [[nodiscard]] u16 get_column() const;
    SourceLocation get_location() const { return m_loc; }

    virtual ~ASTNode() = default;
}; // class ASTNode

/// -----------------------------------------------------------------------
///                             AST Visitors
/// -----------------------------------------------------------------------

/// AST visitors for routines that are applied recursively
/// on the children of each node in the tree
class ExprVisitor {
public:
    virtual ~ExprVisitor() = default;

    virtual void visit(BinaryExpr&) = 0;
    virtual void visit(UnaryExpr&) = 0;
    virtual void visit(IntLiteralExpr&) = 0;
    virtual void visit(FloatLiteralExpr&) = 0;
    virtual void visit(StringLiteralExpr&) = 0;
    virtual void visit(BoolLiteralExpr&) = 0;
    virtual void visit(NilExpr&) = 0;
    virtual void visit(IdentifierExpr&) = 0;
    virtual void visit(CallExpr&) = 0;
    virtual void visit(AssignExpr&) = 0;
    virtual void visit(ListExpr&) = 0;
    virtual void visit(IndexExpr&) = 0;
    virtual void visit(DictExpr&) = 0;
    virtual void visit(GetExpr&) = 0;
};

class StmtVisitor {
public:
    virtual ~StmtVisitor() = default;

    virtual void visit(ExprStmt&) = 0;
    virtual void visit(AssignStmt&) = 0;
    virtual void visit(IfElseStmt&) = 0;
    virtual void visit(WhileStmt&) = 0;
    virtual void visit(ForStmt&) = 0;
    virtual void visit(FuncDefStmt&) = 0;
    virtual void visit(ReturnStmt&) = 0;
    virtual void visit(BreakStmt&) = 0;
    virtual void visit(ContinueStmt&) = 0;
    virtual void visit(BlockStmt&) = 0;
    virtual void visit(ClassDefStmt&) = 0;
    virtual void visit(ImportStmt&) = 0;
};

/// -----------------------------------------------------------------------
///                             Expr ASTNode
/// -----------------------------------------------------------------------

class Expr : public ASTNode {
public:
    /// this is basically used to know what kind of
    /// node this is before casting
    enum class Kind : int {
        /// literals
        INT_LITERAL,
        FLOAT_LITERAL,
        STRING_LITERAL,
        BOOL_LITERAL,
        /// common
        IDENTIFIER,
        CALL,
        ASSIGNMENT,
        LIST,
        INDEX_READ,
        DICT,
        GET,
        /// bin op
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
        /// un op
        OP_PLUS,
        OP_NEG,
        OP_BITNOT,
        OP_NOT,
        //
        NIL,
        INVALID,
    }; // enum Kind

protected:
    Kind m_kind { Kind::INVALID };

public:
    Expr()
        : m_kind(Kind::INVALID)
    {
    }

    Expr(SourceLocation loc, Kind kind)
        : ASTNode(loc)
        , m_kind(kind)
    {
    }

    virtual ~Expr() = default;

    virtual bool equals(ConstExprPtr other) const = 0;
    virtual ExprPtr clone() const = 0;
    virtual void accept(ExprVisitor& v) = 0;

    Kind get_kind() const { return m_kind; }
    NodeType get_node_type() const override { return NodeType::EXPR; }
}; // class Expr

class BinaryExpr final : public Expr {
public:
    ConstExprPtr lhs;
    ConstExprPtr rhs;

    BinaryExpr(Kind kind, ExprPtr l, ExprPtr r, SourceLocation loc)
        : Expr(loc, kind)
        , lhs(l)
        , rhs(r)
    {
        assert(lhs != nullptr);
        assert(rhs != nullptr);
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto bin = static_cast<BinaryExpr*>(other);
        return lhs->equals(bin->lhs) && rhs->equals(bin->rhs);
    }
    [[nodiscard]] BinaryExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(BinaryExpr, m_kind, lhs->clone(), rhs->clone(), get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
}; // class BinaryExpr

class UnaryExpr final : public Expr {
public:
    ConstExprPtr operand;

    UnaryExpr(Kind kind, ExprPtr o, SourceLocation loc)
        : Expr(loc, kind)
        , operand(o)
    {
        assert(operand != nullptr);
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto un = static_cast<UnaryExpr*>(other);
        return operand->equals(un->operand);
    }
    [[nodiscard]] UnaryExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(UnaryExpr, m_kind, operand->clone(), get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
}; // class UnaryExpr

class IntLiteralExpr final : public Expr {
public:
    i64 const value { INT64_C(0) };

    IntLiteralExpr(i64 v, SourceLocation loc)
        : Expr(loc, Kind::INT_LITERAL)
        , value(v)
    {
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return value == static_cast<IntLiteralExpr*>(other)->value;
    }
    [[nodiscard]] IntLiteralExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(IntLiteralExpr, value, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

class FloatLiteralExpr final : public Expr {
public:
    f64 const value { 0.0f };

    FloatLiteralExpr(f64 v, SourceLocation loc)
        : Expr(loc, Kind::FLOAT_LITERAL)
        , value(v)
    {
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return value == static_cast<FloatLiteralExpr*>(other)->value;
    }
    [[nodiscard]] FloatLiteralExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(FloatLiteralExpr, value, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

class StringLiteralExpr final : public Expr {
public:
    StringRef const str;

    StringLiteralExpr(StringRef s, SourceLocation loc)
        : Expr(loc, Kind::STRING_LITERAL)
        , str(s)
    {
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return str == static_cast<StringLiteralExpr*>(other)->str;
    }
    [[nodiscard]] StringLiteralExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(StringLiteralExpr, str, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

class BoolLiteralExpr final : public Expr {
public:
    bool const value;

    BoolLiteralExpr(bool v, SourceLocation loc)
        : Expr(loc, Kind::BOOL_LITERAL)
        , value(v)
    {
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return value == static_cast<BoolLiteralExpr*>(other)->value;
    }
    [[nodiscard]] BoolLiteralExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(BoolLiteralExpr, value, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

class NilExpr final : public Expr {
public:
    NilExpr(SourceLocation loc)
        : Expr(loc, Kind::NIL)
    {
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return true;
    }
    [[nodiscard]] NilExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(NilExpr, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

class IdentifierExpr final : public Expr {
public:
    StringRef const spelling;

    explicit IdentifierExpr(StringRef s, SourceLocation loc)
        : Expr(loc, Kind::IDENTIFIER)
        , spelling(std::move(s))
    {
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto name = static_cast<IdentifierExpr*>(other);
        return spelling == name->spelling;
    }
    [[nodiscard]] IdentifierExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(IdentifierExpr, spelling, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
}; // class IdentifierExpr

class ListExpr final : public Expr {
public:
    Array<ExprPtr> const elements;

    explicit ListExpr(Array<ExprPtr> elements, SourceLocation loc)
        : Expr(loc, Kind::LIST)
        , elements(std::move(elements))
    {
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto list = static_cast<ListExpr*>(other);

        if (elements.size() != list->elements.size())
            return false;

        for (size_t i = 0; i < elements.size(); i++) {
            if (!elements[i]->equals(list->elements[i]))
                return false;
        }

        return true;
    }
    [[nodiscard]] ListExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(ListExpr, elements, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] bool is_empty() const { return elements.empty(); }
    [[nodiscard]] size_t size() const { return elements.size(); }
}; // class ListExpr

class DictExpr final : public Expr {
public:
    Array<std::pair<ExprPtr, ExprPtr>> const content;

    DictExpr(Array<std::pair<ExprPtr, ExprPtr>> c, SourceLocation loc)
        : Expr(loc, Kind::DICT)
        , content(c)
    {
    }

    bool equals(ConstExprPtr other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        // a dictionary normally doesn't care about order so this is logically not correct
        // but we gonna stub an implementation that requires order until someone fixes this
        auto dict = static_cast<DictExpr*>(other)->content;
        u32 const s1 = content.size();
        u32 const s2 = dict.size();

        if (s1 != s2)
            return false;

        for (u32 i = 0; i < s1; i++) {
            if (!content[i].first->equals(dict[i].first) || !content[i].second->equals(dict[i].second))
                return false;
        }

        return true;
    }
    DictExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(DictExpr, content, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

class CallExpr final : public Expr {
public:
    ConstExprPtr callee;
    Array<ExprPtr> const args;

    explicit CallExpr(ExprPtr c, Array<ExprPtr> a, SourceLocation loc)
        : Expr(loc, Kind::CALL)
        , callee(c)
        , args(std::move(a))
    {
        assert(callee != nullptr);
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto call = static_cast<CallExpr*>(other);
        if (args.size() != call->args.size())
            return false;
        for (u32 i = 0, n = args.size(); i < n; ++i) {
            if (!args[i]->equals(call->args[i]))
                return false;
        }
        return callee->equals(call->callee);
    }
    [[nodiscard]] CallExpr* clone() const override
    {
        Array<ExprPtr> args_clone;
        for (auto a : args)
            args_clone.push(a->clone());
        return ALLOCATE_AST_NODE(CallExpr, callee->clone(), args_clone, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] bool has_arguments() const { return !args.empty(); }
}; // class CallExpr

class AssignExpr final : public Expr {
public:
    ConstExprPtr target;
    ConstExprPtr value;

    AssignExpr(ExprPtr t, ExprPtr v, SourceLocation loc)
        : Expr(loc, Kind::ASSIGNMENT)
        , target(t)
        , value(v)
    {
        assert(target != nullptr);
        assert(value != nullptr);
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto bin = static_cast<AssignExpr*>(other);
        return target->equals(bin->target) && value->equals(bin->value);
    }
    [[nodiscard]] AssignExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(AssignExpr, target->clone(), value->clone(), get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
}; // AssignExpr

class IndexExpr final : public Expr {
public:
    ConstExprPtr object;
    ConstExprPtr index;

    IndexExpr(ExprPtr obj, ExprPtr idx, SourceLocation loc)
        : Expr(loc, Kind::INDEX_READ)
        , object(obj)
        , index(idx)
    {
        assert(object != nullptr);
        assert(index != nullptr);
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::INDEX_READ)
            return false;

        auto idx = static_cast<IndexExpr*>(other);
        return object->equals(idx->object) && index->equals(idx->index);
    }
    [[nodiscard]] IndexExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(IndexExpr, object->clone(), index->clone(), get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
}; // class IndexExpr

class GetExpr final : public Expr {
public:
    ConstExprPtr object;
    IdentifierExpr* member;

    GetExpr(ExprPtr obj, IdentifierExpr* mem, SourceLocation loc)
        : Expr(loc, Kind::GET)
        , object(obj)
        , member(mem)
    {
        assert(object != nullptr);
        assert(member != nullptr);
    }

    [[nodiscard]] bool equals(ConstExprPtr other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        auto get_expr = static_cast<GetExpr*>(other);
        return object->equals(get_expr->object) && member->equals(get_expr->member);
    }
    [[nodiscard]] GetExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(GetExpr, object->clone(), member->clone(), get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

/// -----------------------------------------------------------------------
///                             Stmt ASTNode
/// -----------------------------------------------------------------------

class Stmt : public ASTNode {
public:
    enum class Kind : u8 {
        EXPR,
        IF,
        WHILE,
        FOR,
        FUNC,
        RETURN,
        BREAK,
        CONTINUE,
        BLOCK,
        CLASS_DEF,
        IMPORT,
        INVALID,
    };

protected:
    Kind m_kind { Kind::INVALID };

public:
    Stmt() = default;

    explicit Stmt(SourceLocation loc, Kind kind)
        : ASTNode(loc)
        , m_kind(kind)
    {
    }

    virtual ~Stmt() = default;

    virtual StmtPtr clone() const = 0;
    virtual bool equals(ConstStmtPtr other) const = 0;
    virtual void accept(StmtVisitor& v) = 0;

    Kind get_kind() const { return m_kind; }
    NodeType get_node_type() const override { return NodeType::STMT; }
}; // class Stmt

class BlockStmt final : public Stmt {
public:
    Array<StmtPtr> const stmts;

    explicit BlockStmt(Array<StmtPtr> s, SourceLocation loc)
        : Stmt(loc, Kind::BLOCK)
        , stmts(std::move(s))
    {
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::BLOCK)
            return false;

        auto block = static_cast<BlockStmt*>(other);

        if (stmts.size() != block->stmts.size())
            return false;

        for (size_t i = 0; i < stmts.size(); i++) {
            if (!stmts[i]->equals(block->stmts[i]))
                return false;
        }

        return true;
    }
    [[nodiscard]] BlockStmt* clone() const override
    {
        Array<StmtPtr> stmts_clone;
        for (auto s : stmts)
            stmts_clone.push(s->clone());
        return ALLOCATE_AST_NODE(BlockStmt, stmts_clone, get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] bool is_empty() const { return stmts.empty(); }
}; // class BlockStmt

class ExprStmt final : public Stmt {
public:
    ConstExprPtr expr;

    explicit ExprStmt(ExprPtr e, SourceLocation loc)
        : Stmt(loc, Kind::EXPR)
        , expr(e)
    {
        assert(expr != nullptr);
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<ExprStmt*>(other);
        return expr->equals(block->expr);
    }
    [[nodiscard]] ExprStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(ExprStmt, expr->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
}; // class ExprStmt

class IfElseStmt final : public Stmt {
public:
    ConstExprPtr condition;
    ConstStmtPtr then_stmt;
    ConstStmtPtr else_stmt;

    IfElseStmt(ExprPtr c, StmtPtr t, SourceLocation loc, StmtPtr e)
        : Stmt(loc, Kind::IF)
        , condition(c)
        , then_stmt(t)
        , else_stmt(e)
    {
        assert(condition != nullptr);
        assert(then_stmt != nullptr);
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto if_stmt = static_cast<IfElseStmt*>(other);
        bool eq_else = false;

        if (else_stmt != nullptr && if_stmt->else_stmt)
            eq_else = else_stmt->equals(if_stmt->else_stmt);

        return condition->equals(if_stmt->condition) && then_stmt->equals(if_stmt->then_stmt) && eq_else;
    }
    [[nodiscard]] IfElseStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(IfElseStmt,
            condition->clone(),
            then_stmt->clone(), get_location(),
            LIKELY(else_stmt == nullptr) ? nullptr : else_stmt->clone());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
}; // class IfElseStmt

class WhileStmt final : public Stmt {
public:
    ConstExprPtr condition;
    ConstStmtPtr body;

    WhileStmt(ExprPtr c, StmtPtr b, SourceLocation loc)
        : Stmt(loc, Kind::WHILE)
        , condition(c)
        , body(b)
    {
        assert(condition != nullptr);
        assert(body != nullptr);
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<WhileStmt*>(other);
        return condition->equals(block->condition) && body->equals(block->body);
    }
    [[nodiscard]] WhileStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(WhileStmt, condition->clone(), body->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
}; // class WhileStmt

class ForStmt final : public Stmt {
public:
    ConstExprPtr container;
    ConstExprPtr iter;
    ConstStmtPtr body;

    ForStmt(ExprPtr t, ExprPtr i, StmtPtr b, SourceLocation loc)
        : Stmt(loc, Kind::FOR)
        , container(t)
        , iter(i)
        , body(b)
    {
        assert(container != nullptr);
        assert(iter != nullptr);
        assert(body != nullptr);
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<ForStmt*>(other);
        return container->equals(block->container) && iter->equals(block->iter) && body->equals(block->body);
    }
    [[nodiscard]] ForStmt* clone() const override
    {
        return get_allocator().allocate_object<ForStmt>(container->clone(), iter->clone(), body->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
}; // class ForStmt

class FuncDefStmt final : public Stmt {
public:
    IdentifierExpr* name;
    Array<ExprPtr> const params;
    ConstStmtPtr body;

    FuncDefStmt(IdentifierExpr* n, Array<ExprPtr> p, StmtPtr b, SourceLocation loc)
        : Stmt(loc, Kind::FUNC)
        , name(n)
        , params(std::move(p))
        , body(b)
    {
        assert(name != nullptr);
        assert(body != nullptr);
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<FuncDefStmt const*>(other);
        /// overloading is not supported therefor equality
        // can be resolved by the name of the function
        return name->equals(block->name);
    }
    [[nodiscard]] FuncDefStmt* clone() const override
    {
        Array<ExprPtr> params_clone;
        for (auto p : params)
            params_clone.push(p->clone());
        return ALLOCATE_AST_NODE(FuncDefStmt, name->clone(), params_clone, body->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] bool has_parameters() const { return !params.empty(); }
}; // class FuncDefStmt

class ReturnStmt final : public Stmt {
public:
    ConstExprPtr value;

    explicit ReturnStmt(ExprPtr v, SourceLocation loc)
        : Stmt(loc, Kind::RETURN)
        , value(v)
    {
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto ret_stmt = static_cast<ReturnStmt*>(other);
        if (value == nullptr || !ret_stmt->has_value())
            return value == ret_stmt->value;

        return value->equals(ret_stmt->value);
    }
    [[nodiscard]] ReturnStmt* clone() const override
    {
        auto ret_value = value == nullptr ? nullptr : value->clone();
        return ALLOCATE_AST_NODE(ReturnStmt, ret_value, get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] bool has_value() const { return value != nullptr; }
}; // class ReturnStmt

class ClassDefStmt final : public Stmt {
public:
    ConstExprPtr name;
    ConstExprPtr parent;
    Array<ExprPtr> const members;
    Array<StmtPtr> const methods;

    explicit ClassDefStmt(
        ExprPtr n,
        ExprPtr p,
        Array<ExprPtr> members,
        Array<StmtPtr> methods,
        SourceLocation loc)
        : Stmt(loc, Kind::CLASS_DEF)
        , name(n)
        , parent(p)
        , members(members)
        , methods(methods)
    {
        assert(name != nullptr);
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto class_def = static_cast<ClassDefStmt const*>(other);

        if (class_def->members.size() != members.size() || class_def->methods.size() != methods.size())
            return false;

        for (u32 i = 0, n = class_def->members.size(); i < n; ++i) {
            if (!class_def->members[i]->equals(members[i]))
                return false;
        }

        for (u32 i = 0, n = class_def->methods.size(); i < n; ++i) {
            if (!class_def->methods[i]->equals(methods[i]))
                return false;
        }

        bool const parents_equal = (parent == nullptr || class_def->parent == nullptr)
            ? parent == class_def->parent
            : parent->equals(class_def->parent);
        return parents_equal && name->equals(class_def->name);
    }
    [[nodiscard]] ClassDefStmt* clone() const override
    {
        Array<ExprPtr> member_clones;
        Array<StmtPtr> method_clones;
        for (ExprPtr mem : members)
            member_clones.push(mem->clone());
        for (StmtPtr met : methods)
            method_clones.push(met->clone());
        return ALLOCATE_AST_NODE(ClassDefStmt, name->clone(),
            parent == nullptr ? nullptr : parent->clone(), member_clones, method_clones, get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
}; // class ClassDefStmt

class ImportStmt final : public Stmt {
private:
    StringRef m_module;
    Array<StringRef> m_names;
    Array<StringRef> m_aliases;

public:
    ImportStmt(StringRef module, Array<StringRef> names, Array<StringRef> aliases, SourceLocation loc)
        : Stmt(loc, Kind::IMPORT)
        , m_module(module)
        , m_names(names)
        , m_aliases(aliases)
    {
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::IMPORT)
            return false;
        auto const* import = static_cast<ImportStmt*>(other);
        return m_module == import->m_module && m_names == import->m_names && m_aliases == import->m_aliases;
    }
    [[nodiscard]] ImportStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(ImportStmt, m_module, m_names, m_aliases, get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] StringRef const& get_module() const { return m_module; }
    [[nodiscard]] Array<StringRef> const& get_names() const { return m_names; }
    [[nodiscard]] Array<StringRef> const& get_aliases() const { return m_aliases; }
    [[nodiscard]] bool imports_member() const { return !m_names.empty(); }
};

class BreakStmt final : public Stmt {
public:
    explicit BreakStmt(SourceLocation loc)
        : Stmt(loc, Kind::BREAK)
    {
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        return other != nullptr && other->get_kind() == Kind::BREAK;
    }
    [[nodiscard]] BreakStmt* clone() const override { return ALLOCATE_AST_NODE(BreakStmt, get_location()); }
    void accept(StmtVisitor& v) override { v.visit(*this); }
}; // class BreakStmt

class ContinueStmt final : public Stmt {
public:
    explicit ContinueStmt(SourceLocation loc)
        : Stmt(loc, Kind::CONTINUE)
    {
    }

    [[nodiscard]] bool equals(ConstStmtPtr other) const override
    {
        return other != nullptr && other->get_kind() == Kind::CONTINUE;
    }
    [[nodiscard]] ContinueStmt* clone() const override { return ALLOCATE_AST_NODE(ContinueStmt, get_location()); }
    void accept(StmtVisitor& v) override { v.visit(*this); }
}; // class ContinueStmt

/// -----------------------------------------------------------------------
///                            Helper Makers
/// -----------------------------------------------------------------------

/// These are wrappers of ALLOCATE_AST_NODE to introdude type checking
/// for arguments and readability
static inline BinaryExpr* make_binary(Expr::Kind kind, ExprPtr lhs, ExprPtr rhs, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(BinaryExpr, kind, lhs, rhs, loc);
}
static inline UnaryExpr* make_unary(Expr::Kind kind, ExprPtr operand, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(UnaryExpr, kind, operand, loc);
}
static inline NilExpr* make_nil(SourceLocation loc)
{
    return ALLOCATE_AST_NODE(NilExpr, loc);
}
static inline IntLiteralExpr* make_literal_int(int value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(IntLiteralExpr, static_cast<i64>(value), loc);
}
static inline IntLiteralExpr* make_literal_int(i64 value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(IntLiteralExpr, value, loc);
}
static inline FloatLiteralExpr* make_literal_float(f64 value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(FloatLiteralExpr, value, loc);
}
static inline StringLiteralExpr* make_literal_string(StringRef const str, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(StringLiteralExpr, str, loc);
}
static inline BoolLiteralExpr* make_literal_bool(bool value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(BoolLiteralExpr, value, loc);
}
static inline IdentifierExpr* make_identifier(StringRef const str, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(IdentifierExpr, str, loc);
}
static inline ListExpr* make_list(Array<ExprPtr> elements, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ListExpr, elements, loc);
}
static inline DictExpr* make_dict(Array<std::pair<ExprPtr, ExprPtr>> content, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(DictExpr, content, loc);
}
static inline GetExpr* make_get_expr(ExprPtr obj, IdentifierExpr* member, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(GetExpr, obj, member, loc);
}
static inline CallExpr* make_call(ExprPtr callee, Array<ExprPtr> args, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(CallExpr, callee, args, loc);
}
static inline AssignExpr* make_assignment_expr(ExprPtr target, ExprPtr value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(AssignExpr, target, value, loc);
}
static inline IndexExpr* make_index(ExprPtr obj, ExprPtr idx, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(IndexExpr, obj, idx, loc);
}
static inline BlockStmt* make_block(Array<StmtPtr> stmts, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(BlockStmt, stmts, loc);
}
static inline ExprStmt* make_expr_stmt(ExprPtr expr, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ExprStmt, expr, loc);
}
/// This one creates an assignment stmt expr that wrappes an assignment expr
/// just so we don't have to write  the exact same two line sequence
static inline ExprStmt* make_assignment_stmt(ExprPtr target, ExprPtr value, SourceLocation loc)
{
    auto e = ALLOCATE_AST_NODE(AssignExpr, target, value, loc);
    return ALLOCATE_AST_NODE(ExprStmt, e, loc);
}
static inline IfElseStmt* make_if(ExprPtr cond, StmtPtr then_block, SourceLocation loc, StmtPtr else_block = nullptr)
{
    return ALLOCATE_AST_NODE(IfElseStmt, cond, then_block, loc, else_block);
}
static inline WhileStmt* make_while(ExprPtr cond, StmtPtr body, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(WhileStmt, cond, body, loc);
}
static inline ForStmt* make_for(IdentifierExpr* target, ExprPtr iter, StmtPtr body, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ForStmt, target, iter, body, loc);
}
static inline FuncDefStmt* make_function(IdentifierExpr* name, Array<ExprPtr> params, StmtPtr body, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(FuncDefStmt, name, params, body, loc);
}
static inline ReturnStmt* make_return(SourceLocation loc, ExprPtr value = nullptr)
{
    return ALLOCATE_AST_NODE(ReturnStmt, value, loc);
}
static inline ClassDefStmt* make_class_def(ExprPtr name, Array<ExprPtr> members,
    Array<StmtPtr> methods, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ClassDefStmt, name, nullptr, members, methods, loc);
}
static inline ClassDefStmt* make_class_def(ExprPtr name, ExprPtr parent,
    Array<ExprPtr> members, Array<StmtPtr> methods, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ClassDefStmt, name, parent, members, methods, loc);
}
static inline ImportStmt* make_import(StringRef const module, Array<StringRef> name,
    Array<StringRef> aliases, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ImportStmt, module, name, aliases, loc);
}
static inline BreakStmt* make_break(SourceLocation loc)
{
    return ALLOCATE_AST_NODE(BreakStmt, loc);
}
static inline ContinueStmt* make_continue(SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ContinueStmt, loc);
}

#undef ALLOCATE_AST_NODE

/// -----------------------------------------------------------------------
///                     helper inline global functions
/// -----------------------------------------------------------------------

/// Casting is only done for constant pointers since AST nodes are immutable data
/// NOTE: Every casting of AST nodes in this project must use these helpers exclusively
/// no dynamic_cast or const_cast is allowed, even if these helpers are essentially just aliases
inline IfElseStmt* as_if(StmtPtr s) { return static_cast<IfElseStmt*>(s); }
inline WhileStmt* as_while(StmtPtr s) { return static_cast<WhileStmt*>(s); }
inline ForStmt* as_for(StmtPtr s) { return static_cast<ForStmt*>(s); }
inline ReturnStmt* as_return(StmtPtr s) { return static_cast<ReturnStmt*>(s); }
inline BreakStmt* as_break(StmtPtr s) { return static_cast<BreakStmt*>(s); }
inline ContinueStmt* as_continue(StmtPtr s) { return static_cast<ContinueStmt*>(s); }
inline BlockStmt* as_block(StmtPtr s) { return static_cast<BlockStmt*>(s); }
inline FuncDefStmt* as_function_def(StmtPtr s) { return static_cast<FuncDefStmt*>(s); }
inline ClassDefStmt* as_class_def(StmtPtr s) { return static_cast<ClassDefStmt*>(s); }
inline ImportStmt* as_import(StmtPtr s) { return static_cast<ImportStmt*>(s); }
inline ExprStmt* as_expr_stmt(StmtPtr s) { return static_cast<ExprStmt*>(s); }

inline BinaryExpr* as_binary(ExprPtr e) { return static_cast<BinaryExpr*>(e); }
inline UnaryExpr* as_unary(ExprPtr e) { return static_cast<UnaryExpr*>(e); }
inline IntLiteralExpr* as_literal_int(ExprPtr e) { return static_cast<IntLiteralExpr*>(e); }
inline FloatLiteralExpr* as_literal_float(ExprPtr e) { return static_cast<FloatLiteralExpr*>(e); }
inline BoolLiteralExpr* as_literal_bool(ExprPtr e) { return static_cast<BoolLiteralExpr*>(e); }
inline StringLiteralExpr* as_literal_string(ExprPtr e) { return static_cast<StringLiteralExpr*>(e); }
inline NilExpr* as_nil(ExprPtr e) { return static_cast<NilExpr*>(e); }
inline IdentifierExpr* as_identifier(ExprPtr e) { return static_cast<IdentifierExpr*>(e); }
inline IndexExpr* as_index(ExprPtr e) { return static_cast<IndexExpr*>(e); }
inline DictExpr* as_dict(ExprPtr e) { return static_cast<DictExpr*>(e); }
inline ListExpr* as_list(ExprPtr e) { return static_cast<ListExpr*>(e); }
inline CallExpr* as_call(ExprPtr e) { return static_cast<CallExpr*>(e); }
inline AssignExpr* as_assignment_expr(ExprPtr e) { return static_cast<AssignExpr*>(e); }
inline GetExpr* as_get(ExprPtr e) { return static_cast<GetExpr*>(e); }

static inline bool is_class_def(StmtPtr s) { return s->get_kind() == Stmt::Kind::CLASS_DEF; }
static inline bool is_import(StmtPtr s) { return s->get_kind() == Stmt::Kind::IMPORT; }
static inline bool is_if(StmtPtr s) { return s->get_kind() == Stmt::Kind::IF; }
static inline bool is_while(StmtPtr s) { return s->get_kind() == Stmt::Kind::WHILE; }
static inline bool is_for(StmtPtr s) { return s->get_kind() == Stmt::Kind::FOR; }
static inline bool is_return(StmtPtr s) { return s->get_kind() == Stmt::Kind::RETURN; }
static inline bool is_break(StmtPtr s) { return s->get_kind() == Stmt::Kind::BREAK; }
static inline bool is_continue(StmtPtr s) { return s->get_kind() == Stmt::Kind::CONTINUE; }
static inline bool is_func(StmtPtr s) { return s->get_kind() == Stmt::Kind::FUNC; }
static inline bool is_expr(StmtPtr s) { return s->get_kind() == Stmt::Kind::EXPR; }
static inline bool is_block(StmtPtr s) { return s->get_kind() == Stmt::Kind::BLOCK; }

static inline bool is_binary(ExprPtr e) { return dynamic_cast<BinaryExpr*>(e) != nullptr; }
static inline bool is_unary(ExprPtr e) { return dynamic_cast<UnaryExpr*>(e) != nullptr; }
static inline bool is_literal_int(ExprPtr e) { return e->get_kind() == Expr::Kind::INT_LITERAL; }
static inline bool is_literal_float(ExprPtr e) { return e->get_kind() == Expr::Kind::FLOAT_LITERAL; }
static inline bool is_literal_bool(ExprPtr e) { return e->get_kind() == Expr::Kind::BOOL_LITERAL; }
static inline bool is_literal_string(ExprPtr e) { return e->get_kind() == Expr::Kind::STRING_LITERAL; }
static inline bool is_nil(ExprPtr e) { return e->get_kind() == Expr::Kind::NIL; }
static inline bool is_identifier(ExprPtr e) { return e->get_kind() == Expr::Kind::IDENTIFIER; }
static inline bool is_index(ExprPtr e) { return e->get_kind() == Expr::Kind::INDEX_READ; }
static inline bool is_dict(ExprPtr e) { return e->get_kind() == Expr::Kind::DICT; }
static inline bool is_list(ExprPtr e) { return e->get_kind() == Expr::Kind::LIST; }
static inline bool is_call(ExprPtr e) { return e->get_kind() == Expr::Kind::CALL; }
static inline bool is_assignment(ExprPtr e) { return e->get_kind() == Expr::Kind::ASSIGNMENT; }
static inline bool is_get(ExprPtr e) { return e->get_kind() == Expr::Kind::GET; }

using ExprKind = Expr::Kind;
using StmtKind = Stmt::Kind;

} // namespace fairuz::ast

#endif // FA_AST_HPP
