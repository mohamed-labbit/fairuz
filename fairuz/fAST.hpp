#ifndef FA_AST_HPP
#define FA_AST_HPP

#include "farena.hpp"
#include "farray.hpp"
#include "fmacros.hpp"
#include "fstring.hpp"

#include <cassert>
#include <cstddef>

#define ALLOCATE_AST_NODE(type, ...) get_allocator().allocate_object<type>(__VA_ARGS__);

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
class FunctionDef;
class ReturnStmt;
class BreakStmt;
class ContinueStmt;
class BlockStmt;
class ClassDef;
class ImportStmt;

class ASTNode {
public:
    enum class NodeType : int {
        EXPRESSION,
        STATEMENT,
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
    ASTNode(ASTNode const&) = delete;
    ASTNode(ASTNode&&) = delete;

    ASTNode& operator=(ASTNode const&) = delete;
    ASTNode& operator=(ASTNode&&) = delete;

    [[nodiscard]] virtual NodeType get_node_type() const
    {
        return node_type;
    }
    [[nodiscard]] u32 get_line() const;
    [[nodiscard]] u16 get_column() const;
    SourceLocation get_location() const { return m_loc; }

    virtual ~ASTNode() = default;
}; // class ASTNode

class ExprVisitor {
public:
    virtual ~ExprVisitor() = default;

    virtual void visit(BinaryExpr const&) const = 0;
    virtual void visit(UnaryExpr const&) const = 0;
    virtual void visit(IntLiteralExpr const&) const = 0;
    virtual void visit(FloatLiteralExpr const&) const = 0;
    virtual void visit(StringLiteralExpr const&) const = 0;
    virtual void visit(BoolLiteralExpr const&) const = 0;
    virtual void visit(NilExpr const&) const = 0;
    virtual void visit(IdentifierExpr const&) const = 0;
    virtual void visit(CallExpr const&) const = 0;
    virtual void visit(AssignExpr const&) const = 0;
    virtual void visit(ListExpr const&) const = 0;
    virtual void visit(IndexExpr const&) const = 0;
    virtual void visit(DictExpr const&) const = 0;
    virtual void visit(GetExpr const&) const = 0;
};

class StmtVisitor {
public:
    virtual ~StmtVisitor() = default;

    virtual void visit(ExprStmt const&) const = 0;
    virtual void visit(AssignStmt const&) const = 0;
    virtual void visit(IfElseStmt const&) const = 0;
    virtual void visit(WhileStmt const&) const = 0;
    virtual void visit(ForStmt const&) const = 0;
    virtual void visit(FunctionDef const&) const = 0;
    virtual void visit(ReturnStmt const&) const = 0;
    virtual void visit(BreakStmt const&) const = 0;
    virtual void visit(ContinueStmt const&) const = 0;
    virtual void visit(BlockStmt const&) const = 0;
    virtual void visit(ClassDef const&) const = 0;
    virtual void visit(ImportStmt const&) const = 0;
};

/// NOTE: do not know if the assert for the costructors args is a good idea

class Expr : public ASTNode {
public:
    enum class Kind : int {
        /// final node kind
        INT_LITERAL,
        FLOAT_LITERAL,
        STRING_LITERAL,
        BOOL_LITERAL,
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

    virtual bool equals(Expr const* other) const = 0;
    virtual Expr* clone() const = 0;
    virtual void accept(ExprVisitor const& v) const = 0;

    Kind get_kind() const { return m_kind; }
    NodeType get_node_type() const override { return NodeType::EXPRESSION; }
}; // class Expr

class BinaryExpr final : public Expr {
public:
    Expr const* lhs;
    Expr const* rhs;

    BinaryExpr(Kind kind, Expr* l, Expr* r, SourceLocation loc)
        : Expr(loc, kind)
        , lhs(l)
        , rhs(r)
    {
        assert(lhs != nullptr);
        assert(rhs != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto bin = static_cast<BinaryExpr const*>(other);
        return lhs->equals(bin->lhs) && rhs->equals(bin->rhs);
    }
    [[nodiscard]] BinaryExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(BinaryExpr, m_kind, lhs->clone(), rhs->clone(), get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
}; // class BinaryExpr

class UnaryExpr final : public Expr {
public:
    Expr const* operand;

    UnaryExpr(Kind kind, Expr* o, SourceLocation loc)
        : Expr(loc, kind)
        , operand(o)
    {
        assert(operand != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto un = static_cast<UnaryExpr const*>(other);
        return operand->equals(un->operand);
    }
    [[nodiscard]] UnaryExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(UnaryExpr, m_kind, operand->clone(), get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
}; // class UnaryExpr

class IntLiteralExpr final : public Expr {
public:
    i64 const value { INT64_C(0) };

    IntLiteralExpr(i64 v, SourceLocation loc)
        : Expr(loc, Kind::INT_LITERAL)
        , value(v)
    {
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return value == static_cast<IntLiteralExpr const*>(other)->value;
    }
    [[nodiscard]] IntLiteralExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(IntLiteralExpr, value, get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
};

class FloatLiteralExpr final : public Expr {
public:
    f64 const value { 0.0f };

    FloatLiteralExpr(f64 v, SourceLocation loc)
        : Expr(loc, Kind::FLOAT_LITERAL)
        , value(v)
    {
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return value == static_cast<FloatLiteralExpr const*>(other)->value;
    }
    [[nodiscard]] FloatLiteralExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(FloatLiteralExpr, value, get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
};

class StringLiteralExpr final : public Expr {
public:
    StringRef const str;

    StringLiteralExpr(StringRef s, SourceLocation loc)
        : Expr(loc, Kind::STRING_LITERAL)
        , str(s)
    {
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return str == static_cast<StringLiteralExpr const*>(other)->str;
    }
    [[nodiscard]] StringLiteralExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(StringLiteralExpr, str, get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
};

class BoolLiteralExpr final : public Expr {
public:
    bool const value;

    BoolLiteralExpr(bool v, SourceLocation loc)
        : Expr(loc, Kind::BOOL_LITERAL)
        , value(v)
    {
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return value == static_cast<BoolLiteralExpr const*>(other)->value;
    }
    [[nodiscard]] BoolLiteralExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(BoolLiteralExpr, value, get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
};

class NilExpr final : public Expr {
public:
    NilExpr(SourceLocation loc)
        : Expr(loc, Kind::NIL)
    {
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        return true;
    }
    [[nodiscard]] NilExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(NilExpr, get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
};

class IdentifierExpr final : public Expr {
public:
    StringRef const spelling;

    explicit IdentifierExpr(StringRef s, SourceLocation loc)
        : Expr(loc, Kind::IDENTIFIER)
        , spelling(std::move(s))
    {
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto name = static_cast<IdentifierExpr const*>(other);
        return spelling == name->spelling;
    }
    [[nodiscard]] IdentifierExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(IdentifierExpr, spelling, get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
}; // class IdentifierExpr

class ListExpr final : public Expr {
public:
    Array<Expr*> const elements;

    explicit ListExpr(Array<Expr*> elements, SourceLocation loc)
        : Expr(loc, Kind::LIST)
        , elements(std::move(elements))
    {
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto list = static_cast<ListExpr const*>(other);

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
    void accept(ExprVisitor const& v) const override { v.visit(*this); }

    [[nodiscard]] bool is_empty() const { return elements.empty(); }
    [[nodiscard]] size_t size() const { return elements.size(); }
}; // class ListExpr

class DictExpr final : public Expr {
public:
    Array<std::pair<Expr*, Expr*>> const content;

    DictExpr(Array<std::pair<Expr*, Expr*>> c, SourceLocation loc)
        : Expr(loc, Kind::DICT)
        , content(c)
    {
    }

    bool equals(Expr const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        // a dictionary normally doesn't care about order so this is logically not correct
        // but we gonna stub an implementation that requires order until someone fixes this
        auto dict = static_cast<DictExpr const*>(other)->get_content();
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
    void accept(ExprVisitor const& v) const override { v.visit(*this); }

    Array<std::pair<Expr*, Expr*>> get_content() const
    {
        return content;
    }
};

class CallExpr final : public Expr {
public:
    Expr const* callee;
    ListExpr const* args;

    explicit CallExpr(Expr* c, ListExpr* a, SourceLocation loc)
        : Expr(loc, Kind::CALL)
        , callee(c)
        , args(a)
    {
        if (args == nullptr)
            args = ALLOCATE_AST_NODE(ListExpr, Array<Expr*> { }, loc);

        assert(callee != nullptr);
        assert(args != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto call = static_cast<CallExpr const*>(other);
        return callee->equals(call->callee) && args->equals(call->args);
    }
    [[nodiscard]] CallExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(CallExpr, callee->clone(), args->clone(), get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }

    [[nodiscard]] bool has_arguments() const { return !args->is_empty(); }
}; // class CallExpr

class AssignExpr final : public Expr {
public:
    Expr const* target;
    Expr const* value;

    AssignExpr(Expr* t, Expr* v, SourceLocation loc)
        : Expr(loc, Kind::ASSIGNMENT)
        , target(t)
        , value(v)
    {
        assert(target != nullptr);
        assert(value != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto bin = static_cast<AssignExpr const*>(other);
        return target->equals(bin->target) && value->equals(bin->value);
    }
    [[nodiscard]] AssignExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(AssignExpr, target->clone(), value->clone(), get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
}; // AssignExpr

class IndexExpr final : public Expr {
public:
    Expr const* object;
    Expr const* index;

    IndexExpr(Expr* obj, Expr* idx, SourceLocation loc)
        : Expr(loc, Kind::INDEX_READ)
        , object(obj)
        , index(idx)
    {
        assert(object != nullptr);
        assert(index != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::INDEX_READ)
            return false;

        auto idx = static_cast<IndexExpr const*>(other);
        return object->equals(idx->object) && index->equals(idx->index);
    }
    [[nodiscard]] IndexExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(IndexExpr, object->clone(), index->clone(), get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
}; // class IndexExpr

class GetExpr final : public Expr {
public:
    Expr const* object;
    IdentifierExpr const* member;

    GetExpr(Expr* obj, IdentifierExpr* mem, SourceLocation loc)
        : Expr(loc, Kind::GET)
        , object(obj)
        , member(mem)
    {
        assert(object != nullptr);
        assert(member != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        auto get_expr = static_cast<GetExpr const*>(other);
        return object->equals(get_expr->object) && member->equals(get_expr->member);
    }
    [[nodiscard]] GetExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(GetExpr, object->clone(), member->clone(), get_location());
    }
    void accept(ExprVisitor const& v) const override { v.visit(*this); }
};

class Stmt : public ASTNode {
public:
    enum class Kind : u8 {
        EXPR,
        ASSIGNMENT,
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
        INVALID
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

    virtual Stmt* clone() const = 0;
    virtual bool equals(Stmt const* other) const = 0;
    virtual void accept(StmtVisitor& v) = 0;

    Kind get_kind() const { return m_kind; }
    NodeType get_node_type() const override { return NodeType::STATEMENT; }
}; // class Stmt

class BlockStmt final : public Stmt {
public:
    Array<Stmt*> const stmts;

    explicit BlockStmt(Array<Stmt*> s, SourceLocation loc)
        : Stmt(loc, Kind::BLOCK)
        , stmts(s)
    {
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::BLOCK)
            return false;

        auto block = static_cast<BlockStmt const*>(other);

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
        return ALLOCATE_AST_NODE(BlockStmt, stmts, get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] bool is_empty() const { return stmts.empty(); }
}; // class BlockStmt

class ExprStmt final : public Stmt {
public:
    Expr const* expr;

    explicit ExprStmt(Expr* e, SourceLocation loc)
        : Stmt(loc, Kind::EXPR)
        , expr(e)
    {
        assert(expr != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<ExprStmt const*>(other);
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
    Expr const* condition;
    Stmt const* then_stmt;
    Stmt const* else_stmt;

    IfElseStmt(Expr* c, Stmt* t, SourceLocation loc, Stmt* e)
        : Stmt(loc, Kind::IF)
        , condition(c)
        , then_stmt(t)
        , else_stmt(e)
    {
        assert(condition != nullptr);
        assert(then_stmt != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto if_stmt = static_cast<IfElseStmt const*>(other);
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
    Expr const* condition;
    Stmt const* body;

    WhileStmt(Expr* c, Stmt* b, SourceLocation loc)
        : Stmt(loc, Kind::WHILE)
        , condition(c)
        , body(b)
    {
        assert(condition != nullptr);
        assert(body != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<WhileStmt const*>(other);
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
    Expr const* container;
    Expr const* iter;
    Stmt const* body;

    ForStmt(Expr* t, Expr* i, Stmt* b, SourceLocation loc)
        : Stmt(loc, Kind::FOR)
        , container(t)
        , iter(i)
        , body(b)
    {
        assert(container != nullptr);
        assert(iter != nullptr);
        assert(body != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<ForStmt const*>(other);
        return container->equals(block->container) && iter->equals(block->iter) && body->equals(block->body);
    }
    [[nodiscard]] ForStmt* clone() const override
    {
        return get_allocator().allocate_object<ForStmt>(container->clone(), iter->clone(), body->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
}; // class ForStmt

class FunctionDef final : public Stmt {
public:
    IdentifierExpr const* name;
    ListExpr const* params;
    Stmt const* body;

    FunctionDef(IdentifierExpr* n, ListExpr* p, Stmt* b, SourceLocation loc)
        : Stmt(loc, Kind::FUNC)
        , name(n)
        , params(p)
        , body(b)
    {
        assert(name != nullptr);
        assert(params != nullptr);
        assert(body != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<FunctionDef const*>(other);
        return name->equals(block->name) && params->equals(block->params) && body->equals(block->body);
    }
    [[nodiscard]] FunctionDef* clone() const override
    {
        return ALLOCATE_AST_NODE(FunctionDef, name->clone(), params->clone(), body->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] bool has_parameters() const { return !params->is_empty(); }
}; // class FunctionDef

class ReturnStmt final : public Stmt {
public:
    Expr const* value;

    explicit ReturnStmt(Expr* v, SourceLocation loc)
        : Stmt(loc, Kind::RETURN)
        , value(v)
    {
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto ret_stmt = static_cast<ReturnStmt const*>(other);
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

class ClassDef final : public Stmt {
private:
    Expr* m_name;
    Expr* m_parent;
    Array<Expr*> m_members;
    Array<Stmt*> m_methods;
    Array<Stmt*> m_sp_methods;

public:
    explicit ClassDef(
        Expr* name,
        Expr* parent,
        Array<Expr*> members,
        Array<Stmt*> methods,
        SourceLocation loc)
        : Stmt(loc, Kind::CLASS_DEF)
        , m_name(name)
        , m_parent(parent)
        , m_members(members)
        , m_methods(methods)
    {
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto class_def = static_cast<ClassDef const*>(other);

        Array<Expr*> other_members = class_def->get_members();
        Array<Stmt*> other_methods = class_def->get_methods();
        if (other_members.size() != m_members.size() || other_methods.size() != m_methods.size())
            return false;

        for (u32 i = 0, n = other_members.size(); i < n; ++i) {
            if (!other_members[i]->equals(m_members[i]))
                return false;
        }

        for (u32 i = 0, n = other_methods.size(); i < n; ++i) {
            if (!other_methods[i]->equals(m_methods[i]))
                return false;
        }

        bool const parents_equal = (m_parent == nullptr || class_def->get_parent() == nullptr)
            ? m_parent == class_def->get_parent()
            : m_parent->equals(class_def->get_parent());
        return parents_equal && m_name->equals(class_def->get_name());
    }
    [[nodiscard]] ClassDef* clone() const override
    {
        Array<Expr*> member_clones;
        Array<Stmt*> method_clones;
        for (Expr* mem : m_members)
            member_clones.push(mem->clone());
        for (Stmt* met : m_methods)
            method_clones.push(met->clone());
        return ALLOCATE_AST_NODE(ClassDef, m_name->clone(),
            m_parent == nullptr ? nullptr : m_parent->clone(), member_clones, method_clones, get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Array<Expr*> get_members() const { return m_members; }
    [[nodiscard]] Array<Stmt*> get_methods() const { return m_methods; }
    [[nodiscard]] Expr* get_name() const { return m_name; }
    [[nodiscard]] Expr* get_parent() const { return m_parent; }
}; // class ClassDef

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

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::IMPORT)
            return false;
        auto const* import = static_cast<ImportStmt const*>(other);
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

    [[nodiscard]] bool equals(Stmt const* other) const override
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

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        return other != nullptr && other->get_kind() == Kind::CONTINUE;
    }
    [[nodiscard]] ContinueStmt* clone() const override { return ALLOCATE_AST_NODE(ContinueStmt, get_location()); }
    void accept(StmtVisitor& v) override { v.visit(*this); }
}; // class ContinueStmt

static inline BinaryExpr* make_binary(Expr::Kind kind, Expr* lhs, Expr* rhs, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(BinaryExpr, kind, lhs, rhs, loc);
}
static inline UnaryExpr* make_unary(Expr::Kind kind, Expr* operand, SourceLocation loc)
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
static inline StringLiteralExpr* make_literal_string(StringRef str, SourceLocation loc)
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
static inline ListExpr* make_list(Array<Expr*> elements, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ListExpr, elements, loc);
}
static inline DictExpr* make_dict(Array<std::pair<Expr*, Expr*>> content, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(DictExpr, content, loc);
}
static inline GetExpr* make_get_expr(Expr* obj, IdentifierExpr* member, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(GetExpr, obj, member, loc)
}
static inline CallExpr* make_call(Expr* callee, ListExpr* args, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(CallExpr, callee, args, loc);
}
static inline AssignExpr* make_assignment_expr(Expr* target, Expr* value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(AssignExpr, target, value, loc);
}
static inline IndexExpr* make_index(Expr* obj, Expr* idx, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(IndexExpr, obj, idx, loc);
}
static inline BlockStmt* make_block(Array<Stmt*> stmts, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(BlockStmt, stmts, loc);
}
static inline ExprStmt* make_expr_stmt(Expr* expr, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ExprStmt, expr, loc);
}
static inline ExprStmt* make_assignment_stmt(Expr* target, Expr* value, SourceLocation loc)
{
    auto e = ALLOCATE_AST_NODE(AssignExpr, target, value, loc) return ALLOCATE_AST_NODE(ExprStmt, e, loc);
}
static inline IfElseStmt* make_if(Expr* cond, Stmt* then_block, SourceLocation loc, Stmt* else_block = nullptr)
{
    return ALLOCATE_AST_NODE(IfElseStmt, cond, then_block, loc, else_block);
}
static inline WhileStmt* make_while(Expr* cond, Stmt* body, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(WhileStmt, cond, body, loc);
}
static inline ForStmt* make_for(IdentifierExpr* target, Expr* iter, Stmt* body, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ForStmt, target, iter, body, loc);
}
static inline FunctionDef* make_function(IdentifierExpr* name, ListExpr* params, Stmt* body, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(FunctionDef, name, params, body, loc);
}
static inline ReturnStmt* make_return(SourceLocation loc, Expr* value = nullptr)
{
    return ALLOCATE_AST_NODE(ReturnStmt, value, loc);
}
static inline ClassDef* make_class_def(Expr* name, Array<Expr*> members,
    Array<Stmt*> methods, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ClassDef, name, nullptr, members, methods, loc);
}
static inline ClassDef* make_class_def(Expr* name, Expr* parent,
    Array<Expr*> members, Array<Stmt*> methods, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ClassDef, name, parent, members, methods, loc);
}
static inline ImportStmt* make_import(StringRef module, Array<StringRef> name,
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

// helper macros

inline IfElseStmt* as_if(Stmt* s) { return static_cast<IfElseStmt*>(s); }
inline WhileStmt* as_while(Stmt* s) { return static_cast<WhileStmt*>(s); }
inline ForStmt* as_for(Stmt* s) { return static_cast<ForStmt*>(s); }
inline ReturnStmt* as_return(Stmt* s) { return static_cast<ReturnStmt*>(s); }
inline BreakStmt* as_break(Stmt* s) { return static_cast<BreakStmt*>(s); }
inline ContinueStmt* as_continue(Stmt* s) { return static_cast<ContinueStmt*>(s); }
inline BlockStmt* as_block(Stmt* s) { return static_cast<BlockStmt*>(s); }
inline FunctionDef* as_function_def(Stmt* s) { return static_cast<FunctionDef*>(s); }
inline ClassDef* as_class_def(Stmt* s) { return static_cast<ClassDef*>(s); }
inline ImportStmt* as_import(Stmt* s) { return static_cast<ImportStmt*>(s); }
inline ExprStmt* as_expr_stmt(Stmt* s) { return static_cast<ExprStmt*>(s); }

inline BinaryExpr* as_binary(Expr* e) { return static_cast<BinaryExpr*>(e); }
inline UnaryExpr* as_unary(Expr* e) { return static_cast<UnaryExpr*>(e); }
inline IntLiteralExpr* as_literal_int(Expr* e) { return static_cast<IntLiteralExpr*>(e); }
inline FloatLiteralExpr* as_literal_float(Expr* e) { return static_cast<FloatLiteralExpr*>(e); }
inline BoolLiteralExpr* as_literal_bool(Expr* e) { return static_cast<BoolLiteralExpr*>(e); }
inline StringLiteralExpr* as_literal_string(Expr* e) { return static_cast<StringLiteralExpr*>(e); }
inline NilExpr* as_nil(Expr* e) { return static_cast<NilExpr*>(e); }
inline IdentifierExpr* as_identifier(Expr* e) { return static_cast<IdentifierExpr*>(e); }
inline IndexExpr* as_index(Expr* e) { return static_cast<IndexExpr*>(e); }
inline DictExpr* as_dict(Expr* e) { return static_cast<DictExpr*>(e); }
inline ListExpr* as_list(Expr* e) { return static_cast<ListExpr*>(e); }
inline CallExpr* as_call(Expr* e) { return static_cast<CallExpr*>(e); }
inline AssignExpr* as_assignment_expr(Expr* e) { return static_cast<AssignExpr*>(e); }
inline GetExpr* as_get(Expr* e) { return static_cast<GetExpr*>(e); }

inline IfElseStmt const* as_if(Stmt const* s) { return static_cast<IfElseStmt const*>(s); }
inline WhileStmt const* as_while(Stmt const* s) { return static_cast<WhileStmt const*>(s); }
inline ForStmt const* as_for(Stmt const* s) { return static_cast<ForStmt const*>(s); }
inline ReturnStmt const* as_return(Stmt const* s) { return static_cast<ReturnStmt const*>(s); }
inline BreakStmt const* as_break(Stmt const* s) { return static_cast<BreakStmt const*>(s); }
inline ContinueStmt const* as_continue(Stmt const* s) { return static_cast<ContinueStmt const*>(s); }
inline BlockStmt const* as_block(Stmt const* s) { return static_cast<BlockStmt const*>(s); }
inline FunctionDef const* as_function_def(Stmt const* s) { return static_cast<FunctionDef const*>(s); }
inline ClassDef const* as_class_def(Stmt const* s) { return static_cast<ClassDef const*>(s); }
inline ImportStmt const* as_import(Stmt const* s) { return static_cast<ImportStmt const*>(s); }
inline ExprStmt const* as_expr_stmt(Stmt const* s) { return static_cast<ExprStmt const*>(s); }

inline BinaryExpr const* as_binary(Expr const* e) { return static_cast<BinaryExpr const*>(e); }
inline UnaryExpr const* as_unary(Expr const* e) { return static_cast<UnaryExpr const*>(e); }
inline IntLiteralExpr const* as_literal_int(Expr const* e) { return static_cast<IntLiteralExpr const*>(e); }
inline FloatLiteralExpr const* as_literal_float(Expr const* e) { return static_cast<FloatLiteralExpr const*>(e); }
inline BoolLiteralExpr const* as_literal_bool(Expr const* e) { return static_cast<BoolLiteralExpr const*>(e); }
inline StringLiteralExpr const* as_literal_string(Expr const* e) { return static_cast<StringLiteralExpr const*>(e); }
inline NilExpr const* as_nil(Expr const* e) { return static_cast<NilExpr const*>(e); }
inline IdentifierExpr const* as_identifier(Expr const* e) { return static_cast<IdentifierExpr const*>(e); }
inline IndexExpr const* as_index(Expr const* e) { return static_cast<IndexExpr const*>(e); }
inline DictExpr const* as_dict(Expr const* e) { return static_cast<DictExpr const*>(e); }
inline ListExpr const* as_list(Expr const* e) { return static_cast<ListExpr const*>(e); }
inline CallExpr const* as_call(Expr const* e) { return static_cast<CallExpr const*>(e); }
inline AssignExpr const* as_assignment_expr(Expr const* e) { return static_cast<AssignExpr const*>(e); }
inline GetExpr const* as_get(Expr const* e) { return static_cast<GetExpr const*>(e); }

static inline bool is_class_def(Stmt const* s) { return s->get_kind() == Stmt::Kind::CLASS_DEF; }
static inline bool is_import(Stmt const* s) { return s->get_kind() == Stmt::Kind::IMPORT; }
static inline bool is_if(Stmt const* s) { return s->get_kind() == Stmt::Kind::IF; }
static inline bool is_while(Stmt const* s) { return s->get_kind() == Stmt::Kind::WHILE; }
static inline bool is_for(Stmt const* s) { return s->get_kind() == Stmt::Kind::FOR; }
static inline bool is_return(Stmt const* s) { return s->get_kind() == Stmt::Kind::RETURN; }
static inline bool is_break(Stmt const* s) { return s->get_kind() == Stmt::Kind::BREAK; }
static inline bool is_continue(Stmt const* s) { return s->get_kind() == Stmt::Kind::CONTINUE; }
static inline bool is_func(Stmt const* s) { return s->get_kind() == Stmt::Kind::FUNC; }
static inline bool is_expr(Stmt const* s) { return s->get_kind() == Stmt::Kind::EXPR; }
static inline bool is_block(Stmt const* s) { return s->get_kind() == Stmt::Kind::BLOCK; }

static inline bool is_binary(Expr const* e) { return dynamic_cast<BinaryExpr const*>(e) != nullptr; }
static inline bool is_unary(Expr const* e) { return dynamic_cast<UnaryExpr const*>(e) != nullptr; }
static inline bool is_literal_int(Expr const* e) { return e->get_kind() == Expr::Kind::INT_LITERAL; }
static inline bool is_literal_float(Expr const* e) { return e->get_kind() == Expr::Kind::FLOAT_LITERAL; }
static inline bool is_literal_bool(Expr const* e) { return e->get_kind() == Expr::Kind::BOOL_LITERAL; }
static inline bool is_literal_string(Expr const* e) { return e->get_kind() == Expr::Kind::STRING_LITERAL; }
static inline bool is_nil(Expr const* e) { return e->get_kind() == Expr::Kind::NIL; }
static inline bool is_identifier(Expr const* e) { return e->get_kind() == Expr::Kind::IDENTIFIER; }
static inline bool is_index(Expr const* e) { return e->get_kind() == Expr::Kind::INDEX_READ; }
static inline bool is_dict(Expr const* e) { return e->get_kind() == Expr::Kind::DICT; }
static inline bool is_list(Expr const* e) { return e->get_kind() == Expr::Kind::LIST; }
static inline bool is_call(Expr const* e) { return e->get_kind() == Expr::Kind::CALL; }
static inline bool is_assignment(Expr const* e) { return e->get_kind() == Expr::Kind::ASSIGNMENT; }
static inline bool is_get(Expr const* e) { return e->get_kind() == Expr::Kind::GET; }

} // namespace fairuz::ast

#endif // FA_AST_HPP
