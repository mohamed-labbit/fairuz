#ifndef FA_AST_HPP
#define FA_AST_HPP

#include "farena.hpp"
#include "farray.hpp"
#include "fmacros.hpp"
#include "fstring.hpp"

#include <cassert>
#include <cstddef>
#include <tuple>

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
class LiteralExpr;
class NameExpr;
class CallExpr;
class AssignmentExpr;
class ListExpr;
class IndexExpr;
class DictExpr;
class GetExpr;

class ExprStmt;
class AssignmentStmt;
class IfStmt;
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

    virtual void visit(BinaryExpr&) = 0;
    virtual void visit(UnaryExpr&) = 0;
    virtual void visit(LiteralExpr&) = 0;
    virtual void visit(NameExpr&) = 0;
    virtual void visit(CallExpr&) = 0;
    virtual void visit(AssignmentExpr&) = 0;
    virtual void visit(ListExpr&) = 0;
    virtual void visit(IndexExpr&) = 0;
    virtual void visit(DictExpr&) = 0;
    virtual void visit(GetExpr&) = 0;
};

class StmtVisitor {
public:
    virtual ~StmtVisitor() = default;

    virtual void visit(ExprStmt&) = 0;
    virtual void visit(AssignmentStmt&) = 0;
    virtual void visit(IfStmt&) = 0;
    virtual void visit(WhileStmt&) = 0;
    virtual void visit(ForStmt&) = 0;
    virtual void visit(FunctionDef&) = 0;
    virtual void visit(ReturnStmt&) = 0;
    virtual void visit(BreakStmt&) = 0;
    virtual void visit(ContinueStmt&) = 0;
    virtual void visit(BlockStmt&) = 0;
    virtual void visit(ClassDef&) = 0;
    virtual void visit(ImportStmt&) = 0;
};

enum class BinaryOp : u8 {
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

enum class UnaryOp : u8 {
    OP_PLUS,
    OP_NEG,
    OP_BITNOT,
    OP_NOT,
    INVALID
}; // enum UnaryOp

/// NOTE: do not know if the assert for the costructors args is a good idea

class Expr : public ASTNode {
public:
    enum class Kind : int {
        BINARY,
        UNARY,
        LITERAL,
        NAME,
        CALL,
        ASSIGNMENT,
        LIST,
        INDEX_READ,
        DICT,
        GET,
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
    virtual void accept(ExprVisitor& v) = 0;

    Kind get_kind() const { return m_kind; }
    NodeType get_node_type() const override { return NodeType::EXPRESSION; }
}; // class Expr

class BinaryExpr final : public Expr {
private:
    Expr* m_left { nullptr };
    Expr* m_right { nullptr };
    BinaryOp m_operator { BinaryOp::INVALID };

public:
    BinaryExpr() = delete;

    BinaryExpr(Expr* l, Expr* r, BinaryOp op, SourceLocation loc)
        : Expr(loc, Kind::BINARY)
        , m_left(l)
        , m_right(r)
        , m_operator(op)
    {
        assert(m_left != nullptr);
        assert(m_right != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto bin = static_cast<BinaryExpr const*>(other);
        return m_operator == bin->get_operator() && m_left->equals(bin->get_left()) && m_right->equals(bin->get_right());
    }
    [[nodiscard]] BinaryExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(BinaryExpr, m_left->clone(), m_right->clone(), m_operator, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Expr* get_left() const { return m_left; }
    [[nodiscard]] Expr* get_right() const { return m_right; }
    [[nodiscard]] BinaryOp get_operator() const { return m_operator; }

    void set_left(Expr* l) { m_left = l; }
    void set_right(Expr* r) { m_right = r; }
    void set_operator(BinaryOp op) { m_operator = op; }
}; // class BinaryExpr

class UnaryExpr final : public Expr {
private:
    Expr* m_operand { nullptr };
    UnaryOp m_operator { UnaryOp::INVALID };

public:
    UnaryExpr() = delete;

    UnaryExpr(Expr* operand, UnaryOp op, SourceLocation loc)
        : Expr(loc, Kind::UNARY)
        , m_operand(operand)
        , m_operator(op)
    {
        assert(m_operand != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto un = static_cast<UnaryExpr const*>(other);
        return m_operator == un->get_operator() && m_operand->equals(un->get_operand());
    }
    [[nodiscard]] UnaryExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(UnaryExpr, m_operand->clone(), m_operator, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Expr* get_operand() const { return m_operand; }
    [[nodiscard]] UnaryOp get_operator() const { return m_operator; }
}; // class UnaryExpr

class LiteralExpr final : public Expr {
public:
    enum class Type {
        INTEGER,
        FLOAT,
        STRING,
        BOOLEAN,
        NIL
    }; // enum Type

private:
    Type m_type { Type::NIL };

    union {
        i64 int_value;
        f64 float_value;
        bool bool_value;
    }; // union

    StringRef str_value;

public:
    explicit LiteralExpr(SourceLocation loc)
        : Expr(loc, Kind::LITERAL)
        , m_type(Type::NIL)
    {
    }

    LiteralExpr(i64 value, Type type, SourceLocation loc)
        : Expr(loc, Kind::LITERAL)
        , m_type(type)
        , int_value(value)
    {
    }
    LiteralExpr(f64 value, Type type, SourceLocation loc)
        : Expr(loc, Kind::LITERAL)
        , m_type(type)
        , float_value(value)
    {
    }
    explicit LiteralExpr(bool value, SourceLocation loc)
        : Expr(loc, Kind::LITERAL)
        , m_type(Type::BOOLEAN)
        , bool_value(value)
    {
    }
    explicit LiteralExpr(StringRef str, SourceLocation loc)
        : Expr(loc, Kind::LITERAL)
        , m_type(Type::STRING)
        , str_value(std::move(str))
    {
    }

    [[nodiscard]] Type get_type() const { return m_type; }

    [[nodiscard]] i64 get_int() const
    {
        assert(is_integer());
        return int_value;
    }

    [[nodiscard]] f64 get_float() const
    {
        assert(is_float());
        return float_value;
    }

    [[nodiscard]] bool get_bool() const
    {
        assert(is_bool());
        return bool_value;
    }

    [[nodiscard]] StringRef get_str() const
    {
        assert(is_string());
        return str_value;
    }

    [[nodiscard]] bool is_integer() const { return m_type == Type::INTEGER; }
    [[nodiscard]] bool is_float() const { return m_type == Type::FLOAT; }
    [[nodiscard]] bool is_bool() const { return m_type == Type::BOOLEAN; }
    [[nodiscard]] bool is_string() const { return m_type == Type::STRING; }
    [[nodiscard]] bool is_numeric() const { return is_integer() || is_float(); }
    [[nodiscard]] bool is_nil() const { return m_type == Type::NIL; }

    [[nodiscard]] f64 as_number() const
    {
        if (is_integer())
            return static_cast<f64>(int_value);
        if (is_float())
            return float_value;

        return 0.0;
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        auto lit = static_cast<LiteralExpr const*>(other);
        if (lit == nullptr || m_type != lit->m_type)
            return false;

        switch (m_type) {
        case Type::INTEGER: return int_value == lit->int_value;
        case Type::FLOAT: return float_value == lit->float_value;
        case Type::BOOLEAN: return bool_value == lit->bool_value;
        case Type::STRING: return str_value == lit->str_value;
        case Type::NIL: return true; // two nil objects are always equal
        }

        return false;
    }
    [[nodiscard]] LiteralExpr* clone() const override
    {
        switch (m_type) {
        case Type::INTEGER: return ALLOCATE_AST_NODE(LiteralExpr, int_value, Type::INTEGER, get_location());
        case Type::FLOAT: return ALLOCATE_AST_NODE(LiteralExpr, float_value, Type::FLOAT, get_location());
        case Type::BOOLEAN: return ALLOCATE_AST_NODE(LiteralExpr, bool_value, get_location());
        case Type::STRING: return ALLOCATE_AST_NODE(LiteralExpr, str_value, get_location());
        case Type::NIL: return ALLOCATE_AST_NODE(LiteralExpr, get_location());
        default: return nullptr; // should never happen
        }
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }
}; // class LiteralExpr

class NameExpr final : public Expr {
private:
    StringRef m_value;
    bool m_is_local { false };

public:
    NameExpr() = default;

    explicit NameExpr(StringRef s, SourceLocation loc)
        : Expr(loc, Kind::NAME)
        , m_value(std::move(s))
    {
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto name = static_cast<NameExpr const*>(other);
        return m_value == name->get_value();
    }
    [[nodiscard]] NameExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(NameExpr, m_value, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] StringRef get_value() const { return m_value; }
    [[nodiscard]] bool is_local() const { return m_is_local; }

    void set_local() { m_is_local = true; }
}; // class NameExpr

class ListExpr final : public Expr {
private:
    Array<Expr*> m_elements;

public:
    ListExpr() = default;

    explicit ListExpr(Array<Expr*> elements, SourceLocation loc)
        : Expr(loc, Kind::LIST)
        , m_elements(std::move(elements))
    {
    }

    Expr* operator[](size_t const i) { return m_elements[i]; }
    Expr const* operator[](size_t const i) const { return m_elements[i]; }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto list = static_cast<ListExpr const*>(other);

        if (m_elements.size() != list->m_elements.size())
            return false;

        for (size_t i = 0; i < m_elements.size(); i++) {
            if (!m_elements[i]->equals(list->m_elements[i]))
                return false;
        }

        return true;
    }
    [[nodiscard]] ListExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(ListExpr, m_elements, get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Array<Expr*> const& get_elements() const { return m_elements; }
    [[nodiscard]] Array<Expr*>& get_elements() { return m_elements; }

    [[nodiscard]] bool is_empty() const { return m_elements.empty(); }
    [[nodiscard]] size_t size() const { return m_elements.size(); }
}; // class ListExpr

class DictExpr final : public Expr {
private:
    Array<std::pair<Expr*, Expr*>> content;

public:
    DictExpr(Array<std::pair<Expr*, Expr*>> content, SourceLocation loc)
        : Expr(loc, Kind::DICT)
        , content(content)
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
    void accept(ExprVisitor& v) override { v.visit(*this); }

    Array<std::pair<Expr*, Expr*>> get_content() const
    {
        return content;
    }
    void set_content(Array<std::pair<Expr*, Expr*>> c)
    {
        content = c;
    }
};

class CallExpr final : public Expr {
public:
    enum class CallLocation : int {
        GLOBAL,
        LOCAL
    }; // enum CallLocation

private:
    Expr* m_callee { nullptr };
    ListExpr* m_args { nullptr };
    CallLocation m_call_location { CallLocation::GLOBAL }; // FIXED: Initialize member

public:
    CallExpr() = delete;

    explicit CallExpr(Expr* c, ListExpr* a, SourceLocation loc, CallLocation call_loc = CallLocation::GLOBAL)
        : Expr(loc, Kind::CALL)
        , m_callee(c)
        , m_args(a)
        , m_call_location(call_loc)
    {
        if (m_args == nullptr)
            m_args = ALLOCATE_AST_NODE(ListExpr, Array<Expr*> { }, loc);

        assert(m_callee != nullptr);
        assert(m_args != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto call = static_cast<CallExpr const*>(other);
        return m_callee->equals(call->get_callee())
            && m_args->equals(call->get_args_as_list_expr())
            && m_call_location == call->get_call_location();
    }
    [[nodiscard]] CallExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(CallExpr, m_callee->clone(), m_args->clone(), get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Expr* get_callee() const { return m_callee; }

    [[nodiscard]] Array<Expr*> const& get_args() const { return m_args->get_elements(); }
    [[nodiscard]] Array<Expr*>& get_args() { return m_args->get_elements(); }

    [[nodiscard]] ListExpr* get_args_as_list_expr() { return m_args; }
    [[nodiscard]] ListExpr const* get_args_as_list_expr() const { return m_args; }

    [[nodiscard]] CallLocation get_call_location() const
    {
        return m_call_location;
    }
    [[nodiscard]] bool has_arguments() const
    {
        return m_args != nullptr && !m_args->is_empty();
    }
}; // class CallExpr

class AssignmentExpr final : public Expr {
private:
    Expr* m_target { nullptr };
    Expr* m_value { nullptr };

public:
    AssignmentExpr(Expr* target, Expr* value, SourceLocation loc)
        : Expr(loc, Kind::ASSIGNMENT)
        , m_target(target)
        , m_value(value)
    {
        assert(m_target != nullptr);
        assert(m_value != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto bin = static_cast<AssignmentExpr const*>(other);
        return m_target->equals(bin->get_target()) && m_value->equals(bin->get_value());
    }
    [[nodiscard]] AssignmentExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(AssignmentExpr, m_target->clone(), m_value->clone(), get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Expr* get_target() const { return m_target; }
    [[nodiscard]] Expr* get_value() const { return m_value; }

    void set_target(Expr* t) { m_target = t; }
    void set_value(Expr* v) { m_value = v; }
}; // AssignmentExpr

class IndexExpr final : public Expr {
private:
    Expr* m_object { nullptr };
    Expr* m_index { nullptr };

    bool m_safe { false };

public:
    IndexExpr(Expr* obj, Expr* idx, SourceLocation loc)
        : Expr(loc, Kind::INDEX_READ)
        , m_object(obj)
        , m_index(idx)
    {
        assert(m_object != nullptr);
        assert(m_index != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::INDEX_READ)
            return false;

        auto idx = static_cast<IndexExpr const*>(other);
        return m_object->equals(idx->get_object()) && m_index->equals(idx->get_index());
    }
    [[nodiscard]] IndexExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(IndexExpr, m_object->clone(), m_index->clone(), get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Expr* get_object() const { return m_object; }
    [[nodiscard]] Expr* get_index() const { return m_index; }

    [[nodiscard]] bool is_safe() const { return m_safe; }
    void make_safe() { m_safe = true; }

}; // class IndexExpr

class GetExpr final : public Expr {
private:
    Expr* m_object { nullptr };
    Expr* m_member { nullptr };

public:
    GetExpr(Expr* obj, Expr* mem, SourceLocation loc)
        : Expr(loc, Kind::GET)
        , m_object(obj)
        , m_member(mem)
    {
        assert(m_object != nullptr);
        assert(m_member != nullptr);
    }

    [[nodiscard]] bool equals(Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        auto get_expr = static_cast<GetExpr const*>(other);
        return m_object->equals(get_expr->get_object()) && m_member->equals(get_expr->get_member());
    }
    [[nodiscard]] GetExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(GetExpr, m_object->clone(), m_member->clone(), get_location());
    }
    void accept(ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Expr* get_object() const { return m_object; }
    [[nodiscard]] Expr* get_member() const { return m_member; }
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
private:
    Array<Stmt*> m_statements;

public:
    explicit BlockStmt(Array<Stmt*> stmts, SourceLocation loc)
        : Stmt(loc, Kind::BLOCK)
        , m_statements(stmts)
    {
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::BLOCK)
            return false;

        auto block = static_cast<BlockStmt const*>(other);

        if (m_statements.size() != block->m_statements.size())
            return false;

        for (size_t i = 0; i < m_statements.size(); i++) {
            if (!m_statements[i]->equals(block->m_statements[i]))
                return false;
        }

        return true;
    }
    [[nodiscard]] BlockStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(BlockStmt, m_statements, get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Array<Stmt*> const& get_statements() const
    {
        return m_statements;
    }
    [[nodiscard]] bool is_empty() const
    {
        return m_statements.empty();
    }
    void set_statements(Array<Stmt*>& stmts)
    {
        m_statements = stmts;
    }
}; // class BlockStmt

class ExprStmt final : public Stmt {
private:
    Expr* m_expr { nullptr };

public:
    explicit ExprStmt(Expr* expr, SourceLocation loc)
        : Stmt(loc, Kind::EXPR)
        , m_expr(expr)
    {
        assert(m_expr != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<ExprStmt const*>(other);
        return m_expr->equals(block->get_expr());
    }
    [[nodiscard]] ExprStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(ExprStmt, m_expr->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Expr* get_expr() const
    {
        return m_expr;
    }
    void set_expr(Expr* e)
    {
        m_expr = e;
    }
}; // class ExprStmt

class AssignmentStmt final : public Stmt {
private:
    AssignmentExpr* m_expr;

public:
    explicit AssignmentStmt(AssignmentExpr* e, SourceLocation loc)
        : Stmt(loc, Kind::ASSIGNMENT)
        , m_expr(e->clone())
    {
        assert(m_expr != nullptr);
    }

    AssignmentStmt(Expr* target, Expr* value, SourceLocation loc)
        : Stmt(loc, Kind::ASSIGNMENT)
    {
        m_expr = ALLOCATE_AST_NODE(AssignmentExpr, target, value, loc); // AssignmentExpr will assert args for us
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<AssignmentStmt const*>(other);
        return m_expr->get_value()->equals(block->get_value()) && m_expr->get_target()->equals(block->get_target());
    }
    [[nodiscard]] AssignmentStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(AssignmentStmt, m_expr->get_target(), m_expr->get_value(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Expr* get_value() const
    {
        return m_expr->get_value();
    }
    [[nodiscard]] Expr* get_target() const
    {
        return m_expr->get_target();
    }
    [[nodiscard]] bool is_declaration() const;
    void set_value(Expr* v)
    {
        m_expr->set_value(v);
    }
    void set_target(Expr* t)
    {
        m_expr->set_target(t);
    }
    void set_decl();

    AssignmentExpr* get_expr() const { return m_expr; }
}; // class AssignmentExpr

class IfStmt final : public Stmt {
private:
    Expr* m_condition { nullptr };
    Stmt* m_then_stmt { nullptr };
    Stmt* m_else_stmt { nullptr };

public:
    IfStmt(Expr* condition, Stmt* then_stmt, SourceLocation loc, Stmt* else_stmt)
        : Stmt(loc, Kind::IF)
        , m_condition(condition)
        , m_then_stmt(then_stmt)
        , m_else_stmt(else_stmt)
    {
        assert(m_condition != nullptr);
        assert(m_then_stmt != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto if_stmt = static_cast<IfStmt const*>(other);
        bool eq_else = false;

        if (m_else_stmt != nullptr && if_stmt->get_else())
            eq_else = m_else_stmt->equals(if_stmt->get_else());

        return m_condition->equals(if_stmt->get_condition()) && m_then_stmt->equals(if_stmt->get_then()) && eq_else;
    }
    [[nodiscard]] IfStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(IfStmt,
            m_condition->clone(),
            m_then_stmt->clone(), get_location(),
            LIKELY(m_else_stmt == nullptr) ? nullptr : m_else_stmt->clone());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Expr* get_condition() const
    {
        return m_condition;
    }
    [[nodiscard]] Stmt* get_then() const
    {
        return m_then_stmt;
    }
    [[nodiscard]] Stmt* get_else() const
    {
        return m_else_stmt;
    }
    void set_then(Stmt* t)
    {
        m_then_stmt = t;
    }
    void set_else(Stmt* e)
    {
        m_else_stmt = e;
    }
}; // class IfStmt

class WhileStmt final : public Stmt {
private:
    Expr* m_condition { nullptr };
    Stmt* m_body { nullptr };

public:
    WhileStmt(Expr* cond, Stmt* body, SourceLocation loc)
        : Stmt(loc, Kind::WHILE)
        , m_condition(cond)
        , m_body(body)
    {
        assert(m_condition != nullptr);
        assert(m_body != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<WhileStmt const*>(other);
        return m_condition->equals(block->get_condition()) && m_body->equals(block->get_body());
    }
    [[nodiscard]] WhileStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(WhileStmt, m_condition->clone(), m_body->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Expr* get_condition() const
    {
        return m_condition;
    }
    [[nodiscard]] Stmt* get_body()
    {
        return m_body;
    }
    [[nodiscard]] Stmt const* get_body() const
    {
        return m_body;
    }

    void set_body(Stmt* b)
    {
        m_body = b;
    }
}; // class WhileStmt

class ForStmt final : public Stmt {
private:
    Expr* m_container { nullptr };
    Expr* m_iter { nullptr };
    Stmt* m_body { nullptr };

public:
    ForStmt(Expr* target, Expr* iter, Stmt* body, SourceLocation loc)
        : Stmt(loc, Kind::FOR)
        , m_container(target)
        , m_iter(iter)
        , m_body(body)
    {
        assert(m_container != nullptr);
        assert(m_iter != nullptr);
        assert(m_body != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<ForStmt const*>(other);
        return m_container->equals(block->get_container()) && m_iter->equals(block->get_iter()) && m_body->equals(block->get_body());
    }
    [[nodiscard]] ForStmt* clone() const override
    {
        return get_allocator().allocate_object<ForStmt>(m_container->clone(), m_iter->clone(), m_body->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Expr* get_container() const { return m_container; }
    [[nodiscard]] NameExpr* get_target() const { return static_cast<NameExpr*>(m_container); }
    [[nodiscard]] Expr* get_iter() const { return m_iter; }
    [[nodiscard]] Stmt* get_body() const { return m_body; }

    void set_body(Stmt* b) { m_body = b; }
}; // class ForStmt

class FunctionDef final : public Stmt {
private:
    NameExpr* m_name { nullptr };
    ListExpr* m_params { nullptr };
    Stmt* m_body { nullptr };

public:
    FunctionDef(NameExpr* name, ListExpr* params, Stmt* body, SourceLocation loc)
        : Stmt(loc, Kind::FUNC)
        , m_name(name)
        , m_params(params)
        , m_body(body)
    {
        assert(m_name != nullptr);
        assert(m_params != nullptr);
        assert(m_body != nullptr);
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<FunctionDef const*>(other);
        return m_name->equals(block->get_name()) && m_params->equals(block->get_parameter_list()) && m_body->equals(block->get_body());
    }
    [[nodiscard]] FunctionDef* clone() const override
    {
        return ALLOCATE_AST_NODE(FunctionDef, m_name->clone(), m_params->clone(), m_body->clone(), get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] NameExpr* get_name() const { return m_name; }
    [[nodiscard]] Array<Expr*> const& get_parameters() const { return m_params->get_elements(); }
    [[nodiscard]] ListExpr* get_parameter_list() const { return m_params; }
    [[nodiscard]] Stmt* get_body() const { return m_body; }
    [[nodiscard]] bool has_parameters() const { return m_params && !m_params->is_empty(); }

    void set_body(Stmt* b) { m_body = b; }
}; // class FunctionDef

class ReturnStmt final : public Stmt {
private:
    Expr* m_value { nullptr };

public:
    explicit ReturnStmt(Expr* value, SourceLocation loc)
        : Stmt(loc, Kind::RETURN)
        , m_value(value)
    {
    }

    [[nodiscard]] bool equals(Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto ret_stmt = static_cast<ReturnStmt const*>(other);
        if (m_value == nullptr || ret_stmt->get_value() == nullptr)
            return m_value == ret_stmt->get_value();

        return m_value->equals(ret_stmt->get_value());
    }
    [[nodiscard]] ReturnStmt* clone() const override
    {
        auto ret_value = m_value == nullptr ? nullptr : m_value->clone();
        return ALLOCATE_AST_NODE(ReturnStmt, ret_value, get_location());
    }
    void accept(StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Expr* get_value() { return m_value; }
    [[nodiscard]] Expr const* get_value() const { return m_value; }
    [[nodiscard]] bool has_value() const { return m_value != nullptr; }

    void set_value(Expr* v) { m_value = v; }
}; // class ReturnStmt

class ClassDef final : public Stmt {
private:
    Expr* m_name { nullptr };
    Expr* m_parent { nullptr };
    Array<Expr*> m_members { nullptr };
    Array<Stmt*> m_methods { nullptr };
    Array<Stmt*> m_sp_methods { nullptr };

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

static inline BinaryExpr* make_binary(Expr* lhs, Expr* rhs, BinaryOp const op, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(BinaryExpr, lhs, rhs, op, loc);
}
static inline UnaryExpr* make_unary(Expr* operand, UnaryOp const op, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(UnaryExpr, operand, op, loc);
}
static inline LiteralExpr* make_literal_nil(SourceLocation loc)
{
    return ALLOCATE_AST_NODE(LiteralExpr, loc);
}
static inline LiteralExpr* make_literal_int(int value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(LiteralExpr, static_cast<i64>(value), LiteralExpr::Type::INTEGER, loc);
}
static inline LiteralExpr* make_literal_int(i64 value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(LiteralExpr, value, LiteralExpr::Type::INTEGER, loc);
}
static inline LiteralExpr* make_literal_float(f64 value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(LiteralExpr, value, LiteralExpr::Type::FLOAT, loc);
}
static inline LiteralExpr* make_literal_string(StringRef value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(LiteralExpr, value, loc);
}
static inline LiteralExpr* make_literal_bool(bool value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(LiteralExpr, value, loc);
}
static inline NameExpr* make_name(StringRef const str, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(NameExpr, str, loc);
}
static inline ListExpr* make_list(Array<Expr*> elements, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ListExpr, elements, loc);
}
static inline DictExpr* make_dict(Array<std::pair<Expr*, Expr*>> content, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(DictExpr, content, loc);
}
static inline GetExpr* make_get_expr(Expr* obj, Expr* member, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(GetExpr, obj, member, loc)
}
static inline CallExpr* make_call(Expr* callee, ListExpr* args, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(CallExpr, callee, args, loc);
}
static inline AssignmentExpr* make_assignment_expr(Expr* target, Expr* value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(AssignmentExpr, target, value, loc);
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
static inline AssignmentStmt* make_assignment_stmt(Expr* target, Expr* value, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(AssignmentStmt, target, value, loc);
}
static inline IfStmt* make_if(Expr* cond, Stmt* then_block, SourceLocation loc, Stmt* else_block = nullptr)
{
    return ALLOCATE_AST_NODE(IfStmt, cond, then_block, loc, else_block);
}
static inline WhileStmt* make_while(Expr* cond, Stmt* body, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(WhileStmt, cond, body, loc);
}
static inline ForStmt* make_for(NameExpr* target, Expr* iter, Stmt* body, SourceLocation loc)
{
    return ALLOCATE_AST_NODE(ForStmt, target, iter, body, loc);
}
static inline FunctionDef* make_function(NameExpr* name, ListExpr* params, Stmt* body, SourceLocation loc)
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

static inline AssignmentExpr* as_assignment(Stmt* s)
{
    if (s == nullptr)
        return nullptr;

    if (s->get_kind() == Stmt::Kind::ASSIGNMENT)
        return static_cast<AssignmentStmt*>(s)->get_expr();
    if (s->get_kind() == Stmt::Kind::EXPR) {
        auto e = static_cast<ExprStmt*>(s)->get_expr();
        if (e->get_kind() == Expr::Kind::ASSIGNMENT)
            return static_cast<AssignmentExpr*>(e);
    }

    return nullptr;
}

static inline AssignmentExpr const* as_assignment(Stmt const* s)
{
    if (s == nullptr)
        return nullptr;

    if (s->get_kind() == Stmt::Kind::ASSIGNMENT)
        return static_cast<AssignmentStmt const*>(s)->get_expr();
    if (s->get_kind() == Stmt::Kind::EXPR) {
        auto e = static_cast<ExprStmt const*>(s)->get_expr();
        if (e->get_kind() == Expr::Kind::ASSIGNMENT)
            return static_cast<AssignmentExpr const*>(e);
    }

    return nullptr;
}

// helper macros

inline IfStmt* as_if(Stmt* s) { return static_cast<IfStmt*>(s); }
inline WhileStmt* as_while(Stmt* s) { return static_cast<WhileStmt*>(s); }
inline ForStmt* as_for(Stmt* s) { return static_cast<ForStmt*>(s); }
inline ReturnStmt* as_return(Stmt* s) { return static_cast<ReturnStmt*>(s); }
inline BreakStmt* as_break(Stmt* s) { return static_cast<BreakStmt*>(s); }
inline ContinueStmt* as_continue(Stmt* s) { return static_cast<ContinueStmt*>(s); }
inline BlockStmt* as_block(Stmt* s) { return static_cast<BlockStmt*>(s); }
inline FunctionDef* as_function_def(Stmt* s) { return static_cast<FunctionDef*>(s); }
inline ClassDef* as_class_def(Stmt* s) { return static_cast<ClassDef*>(s); }
inline ImportStmt* as_import(Stmt* s) { return static_cast<ImportStmt*>(s); }
inline AssignmentStmt* as_assignment_stmt(Stmt* s) { return static_cast<AssignmentStmt*>(s); }
inline ExprStmt* as_expr_stmt(Stmt* s) { return static_cast<ExprStmt*>(s); }

inline BinaryExpr* as_binary(Expr* e) { return static_cast<BinaryExpr*>(e); }
inline UnaryExpr* as_unary(Expr* e) { return static_cast<UnaryExpr*>(e); }
inline LiteralExpr* as_literal(Expr* e) { return static_cast<LiteralExpr*>(e); }
inline NameExpr* as_name(Expr* e) { return static_cast<NameExpr*>(e); }
inline IndexExpr* as_index(Expr* e) { return static_cast<IndexExpr*>(e); }
inline DictExpr* as_dict(Expr* e) { return static_cast<DictExpr*>(e); }
inline ListExpr* as_list(Expr* e) { return static_cast<ListExpr*>(e); }
inline CallExpr* as_call(Expr* e) { return static_cast<CallExpr*>(e); }
inline AssignmentExpr* as_assignment_expr(Expr* e) { return static_cast<AssignmentExpr*>(e); }
inline GetExpr* as_get(Expr* e) { return static_cast<GetExpr*>(e); }

inline IfStmt const* as_if(Stmt const* s) { return static_cast<IfStmt const*>(s); }
inline WhileStmt const* as_while(Stmt const* s) { return static_cast<WhileStmt const*>(s); }
inline ForStmt const* as_for(Stmt const* s) { return static_cast<ForStmt const*>(s); }
inline ReturnStmt const* as_return(Stmt const* s) { return static_cast<ReturnStmt const*>(s); }
inline BreakStmt const* as_break(Stmt const* s) { return static_cast<BreakStmt const*>(s); }
inline ContinueStmt const* as_continue(Stmt const* s) { return static_cast<ContinueStmt const*>(s); }
inline BlockStmt const* as_block(Stmt const* s) { return static_cast<BlockStmt const*>(s); }
inline FunctionDef const* as_function_def(Stmt const* s) { return static_cast<FunctionDef const*>(s); }
inline ClassDef const* as_class_def(Stmt const* s) { return static_cast<ClassDef const*>(s); }
inline ImportStmt const* as_import(Stmt const* s) { return static_cast<ImportStmt const*>(s); }
inline AssignmentStmt const* as_assignment_stmt(Stmt const* s) { return static_cast<AssignmentStmt const*>(s); }
inline ExprStmt const* as_expr_stmt(Stmt const* s) { return static_cast<ExprStmt const*>(s); }

inline BinaryExpr const* as_binary(Expr const* e) { return static_cast<BinaryExpr const*>(e); }
inline UnaryExpr const* as_unary(Expr const* e) { return static_cast<UnaryExpr const*>(e); }
inline LiteralExpr const* as_literal(Expr const* e) { return static_cast<LiteralExpr const*>(e); }
inline NameExpr const* as_name(Expr const* e) { return static_cast<NameExpr const*>(e); }
inline IndexExpr const* as_index(Expr const* e) { return static_cast<IndexExpr const*>(e); }
inline DictExpr const* as_dict(Expr const* e) { return static_cast<DictExpr const*>(e); }
inline ListExpr const* as_list(Expr const* e) { return static_cast<ListExpr const*>(e); }
inline CallExpr const* as_call(Expr const* e) { return static_cast<CallExpr const*>(e); }
inline AssignmentExpr const* as_assignment_expr(Expr const* e) { return static_cast<AssignmentExpr const*>(e); }
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

static inline bool is_binary(Expr const* e) { return e->get_kind() == Expr::Kind::BINARY; }
static inline bool is_unary(Expr const* e) { return e->get_kind() == Expr::Kind::UNARY; }
static inline bool is_literal(Expr const* e) { return e->get_kind() == Expr::Kind::LITERAL; }
static inline bool is_name(Expr const* e) { return e->get_kind() == Expr::Kind::NAME; }
static inline bool is_index(Expr const* e) { return e->get_kind() == Expr::Kind::INDEX_READ; }
static inline bool is_dict(Expr const* e) { return e->get_kind() == Expr::Kind::DICT; }
static inline bool is_list(Expr const* e) { return e->get_kind() == Expr::Kind::LIST; }
static inline bool is_call(Expr const* e) { return e->get_kind() == Expr::Kind::CALL; }
static inline bool is_assignment(Expr const* e) { return e->get_kind() == Expr::Kind::ASSIGNMENT; }
static inline bool is_get(Expr const* e) { return e->get_kind() == Expr::Kind::GET; }

static inline std::tuple<Expr*, Expr*> assignment_parts(AssignmentExpr* e)
{
    return std::make_tuple<Expr*, Expr*>(e->get_target(), e->get_value());
}
static inline std::tuple<Expr*, Expr*> assignment_parts(AssignmentExpr const* e)
{
    return std::make_tuple<Expr*, Expr*>(e->get_target(), e->get_value());
}

static inline int literal_int(Expr const* e) { return as_literal(e)->get_int(); }
static inline StringRef literal_str(Expr const* e) { return as_literal(e)->get_str(); }
static inline float literal_float(Expr const* e) { return as_literal(e)->get_float(); }
static inline bool literal_bool(Expr const* e) { return as_literal(e)->get_bool(); }

} // namespace fairuz::ast

#endif // FA_AST_HPP
