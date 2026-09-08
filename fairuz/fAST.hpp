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

class Fa_Expr;
class Fa_Stmt;
class Fa_BinaryExpr;
class Fa_UnaryExpr;
class Fa_LiteralExpr;
class Fa_NameExpr;
class Fa_CallExpr;
class Fa_AssignmentExpr;
class Fa_ListExpr;
class Fa_IndexExpr;
class Fa_DictExpr;
class Fa_GetExpr;

class Fa_ExprStmt;
class Fa_AssignmentStmt;
class Fa_IfStmt;
class Fa_WhileStmt;
class Fa_ForStmt;
class Fa_FunctionDef;
class Fa_ReturnStmt;
class Fa_BreakStmt;
class Fa_ContinueStmt;
class Fa_BlockStmt;
class Fa_ClassDef;

class Fa_ASTNode {
public:
    enum class NodeType : int {
        EXPRESSION,
        STATEMENT,
        INVALID
    }; // enum NodeType

private:
    NodeType node_type { NodeType::INVALID };
    Fa_SourceLocation m_loc { };

public:
    Fa_ASTNode() = default;
    Fa_ASTNode(Fa_SourceLocation loc)
        : m_loc(loc)
    {
    }
    Fa_ASTNode(Fa_ASTNode const&) = delete;
    Fa_ASTNode(Fa_ASTNode&&) = delete;

    Fa_ASTNode& operator=(Fa_ASTNode const&) = delete;
    Fa_ASTNode& operator=(Fa_ASTNode&&) = delete;

    [[nodiscard]] virtual NodeType get_node_type() const
    {
        return node_type;
    }
    [[nodiscard]] u32 get_line() const;
    [[nodiscard]] u16 get_column() const;
    Fa_SourceLocation get_location() const { return m_loc; }

    virtual ~Fa_ASTNode() = default;
}; // class Fa_ASTNode

class Fa_ExprVisitor {
public:
    virtual ~Fa_ExprVisitor() = default;

    virtual void visit(Fa_BinaryExpr&) = 0;
    virtual void visit(Fa_UnaryExpr&) = 0;
    virtual void visit(Fa_LiteralExpr&) = 0;
    virtual void visit(Fa_NameExpr&) = 0;
    virtual void visit(Fa_CallExpr&) = 0;
    virtual void visit(Fa_AssignmentExpr&) = 0;
    virtual void visit(Fa_ListExpr&) = 0;
    virtual void visit(Fa_IndexExpr&) = 0;
    virtual void visit(Fa_DictExpr&) = 0;
    virtual void visit(Fa_GetExpr&) = 0;
};

class Fa_StmtVisitor {
public:
    virtual ~Fa_StmtVisitor() = default;

    virtual void visit(Fa_ExprStmt&) = 0;
    virtual void visit(Fa_AssignmentStmt&) = 0;
    virtual void visit(Fa_IfStmt&) = 0;
    virtual void visit(Fa_WhileStmt&) = 0;
    virtual void visit(Fa_ForStmt&) = 0;
    virtual void visit(Fa_FunctionDef&) = 0;
    virtual void visit(Fa_ReturnStmt&) = 0;
    virtual void visit(Fa_BreakStmt&) = 0;
    virtual void visit(Fa_ContinueStmt&) = 0;
    virtual void visit(Fa_BlockStmt&) = 0;
    virtual void visit(Fa_ClassDef&) = 0;
};

enum class Fa_BinaryOp : u8 {
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
}; // enum Fa_BinaryOp

enum class Fa_UnaryOp : u8 {
    OP_PLUS,
    OP_NEG,
    OP_BITNOT,
    OP_NOT,
    INVALID
}; // enum Fa_UnaryOp

/// NOTE: do not know if the assert for the costructors args is a good idea

class Fa_Expr : public Fa_ASTNode {
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
    Fa_Expr()
        : m_kind(Kind::INVALID)
    {
    }

    Fa_Expr(Fa_SourceLocation loc, Kind kind)
        : Fa_ASTNode(loc)
        , m_kind(kind)
    {
    }

    virtual ~Fa_Expr() = default;

    virtual bool equals(Fa_Expr const* other) const = 0;
    virtual Fa_Expr* clone() const = 0;
    virtual void accept(Fa_ExprVisitor& v) = 0;

    Kind get_kind() const { return m_kind; }
    NodeType get_node_type() const override { return NodeType::EXPRESSION; }
}; // class Fa_Expr

class Fa_BinaryExpr final : public Fa_Expr {
private:
    Fa_Expr* m_left { nullptr };
    Fa_Expr* m_right { nullptr };
    Fa_BinaryOp m_operator { Fa_BinaryOp::INVALID };

public:
    Fa_BinaryExpr() = delete;

    Fa_BinaryExpr(Fa_Expr* l, Fa_Expr* r, Fa_BinaryOp op, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::BINARY)
        , m_left(l)
        , m_right(r)
        , m_operator(op)
    {
        assert(m_left != nullptr);
        assert(m_right != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto bin = static_cast<Fa_BinaryExpr const*>(other);
        return m_operator == bin->get_operator() && m_left->equals(bin->get_left()) && m_right->equals(bin->get_right());
    }
    [[nodiscard]] Fa_BinaryExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_BinaryExpr, m_left->clone(), m_right->clone(), m_operator, get_location());
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Fa_Expr* get_left() const { return m_left; }
    [[nodiscard]] Fa_Expr* get_right() const { return m_right; }
    [[nodiscard]] Fa_BinaryOp get_operator() const { return m_operator; }

    void set_left(Fa_Expr* l) { m_left = l; }
    void set_right(Fa_Expr* r) { m_right = r; }
    void set_operator(Fa_BinaryOp op) { m_operator = op; }
}; // class Fa_BinaryExpr

class Fa_UnaryExpr final : public Fa_Expr {
private:
    Fa_Expr* m_operand { nullptr };
    Fa_UnaryOp m_operator { Fa_UnaryOp::INVALID };

public:
    Fa_UnaryExpr() = delete;

    Fa_UnaryExpr(Fa_Expr* operand, Fa_UnaryOp op, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::UNARY)
        , m_operand(operand)
        , m_operator(op)
    {
        assert(m_operand != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto un = static_cast<Fa_UnaryExpr const*>(other);
        return m_operator == un->get_operator() && m_operand->equals(un->get_operand());
    }
    [[nodiscard]] Fa_UnaryExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_UnaryExpr, m_operand->clone(), m_operator, get_location());
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Fa_Expr* get_operand() const { return m_operand; }
    [[nodiscard]] Fa_UnaryOp get_operator() const { return m_operator; }
}; // class Fa_UnaryExpr

class Fa_LiteralExpr final : public Fa_Expr {
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

    Fa_StringRef str_value;

public:
    explicit Fa_LiteralExpr(Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::LITERAL)
        , m_type(Type::NIL)
    {
    }

    Fa_LiteralExpr(i64 value, Type type, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::LITERAL)
        , m_type(type)
        , int_value(value)
    {
    }
    Fa_LiteralExpr(f64 value, Type type, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::LITERAL)
        , m_type(type)
        , float_value(value)
    {
    }
    explicit Fa_LiteralExpr(bool value, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::LITERAL)
        , m_type(Type::BOOLEAN)
        , bool_value(value)
    {
    }
    explicit Fa_LiteralExpr(Fa_StringRef str, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::LITERAL)
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

    [[nodiscard]] Fa_StringRef get_str() const
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

    [[nodiscard]] bool equals(Fa_Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        auto lit = static_cast<Fa_LiteralExpr const*>(other);
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
    [[nodiscard]] Fa_LiteralExpr* clone() const override
    {
        switch (m_type) {
        case Type::INTEGER: return ALLOCATE_AST_NODE(Fa_LiteralExpr, int_value, Type::INTEGER, get_location());
        case Type::FLOAT: return ALLOCATE_AST_NODE(Fa_LiteralExpr, float_value, Type::FLOAT, get_location());
        case Type::BOOLEAN: return ALLOCATE_AST_NODE(Fa_LiteralExpr, bool_value, get_location());
        case Type::STRING: return ALLOCATE_AST_NODE(Fa_LiteralExpr, str_value, get_location());
        case Type::NIL: return ALLOCATE_AST_NODE(Fa_LiteralExpr, get_location());
        default: return nullptr; // should never happen
        }
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }
}; // class Fa_LiteralExpr

class Fa_NameExpr final : public Fa_Expr {
private:
    Fa_StringRef m_value;
    bool m_is_local { false };

public:
    Fa_NameExpr() = default;

    explicit Fa_NameExpr(Fa_StringRef s, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::NAME)
        , m_value(std::move(s))
    {
    }

    [[nodiscard]] bool equals(Fa_Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto name = static_cast<Fa_NameExpr const*>(other);
        return m_value == name->get_value();
    }
    [[nodiscard]] Fa_NameExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_NameExpr, m_value, get_location());
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Fa_StringRef get_value() const { return m_value; }
    [[nodiscard]] bool is_local() const { return m_is_local; }

    void set_local() { m_is_local = true; }
}; // class Fa_NameExpr

class Fa_ListExpr final : public Fa_Expr {
private:
    Fa_Array<Fa_Expr*> m_elements;

public:
    Fa_ListExpr() = default;

    explicit Fa_ListExpr(Fa_Array<Fa_Expr*> elements, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::LIST)
        , m_elements(std::move(elements))
    {
    }

    Fa_Expr* operator[](size_t const i) { return m_elements[i]; }
    Fa_Expr const* operator[](size_t const i) const { return m_elements[i]; }

    [[nodiscard]] bool equals(Fa_Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto list = static_cast<Fa_ListExpr const*>(other);

        if (m_elements.size() != list->m_elements.size())
            return false;

        for (size_t i = 0; i < m_elements.size(); i += 1) {
            if (!m_elements[i]->equals(list->m_elements[i]))
                return false;
        }

        return true;
    }
    [[nodiscard]] Fa_ListExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_ListExpr, m_elements, get_location());
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Fa_Array<Fa_Expr*> const& get_elements() const { return m_elements; }
    [[nodiscard]] Fa_Array<Fa_Expr*>& get_elements() { return m_elements; }

    [[nodiscard]] bool is_empty() const { return m_elements.empty(); }
    [[nodiscard]] size_t size() const { return m_elements.size(); }
}; // class Fa_ListExpr

class Fa_DictExpr final : public Fa_Expr {
private:
    Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> content;

public:
    Fa_DictExpr(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> content, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::DICT)
        , content(content)
    {
    }

    bool equals(Fa_Expr const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        // a dictionary normally doesn't care about order so this is logically not correct
        // but we gonna stub an implementation that requires order until someone fixes this
        auto dict = static_cast<Fa_DictExpr const*>(other)->get_content();
        u32 const s1 = content.size();
        u32 const s2 = dict.size();

        if (s1 != s2)
            return false;

        for (u32 i = 0; i < s1; i += 1) {
            if (!content[i].first->equals(dict[i].first) || !content[i].second->equals(dict[i].second))
                return false;
        }

        return true;
    }
    Fa_DictExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_DictExpr, content, get_location());
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }

    Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> get_content() const
    {
        return content;
    }
    void set_content(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> c)
    {
        content = c;
    }
};

class Fa_CallExpr final : public Fa_Expr {
public:
    enum class CallLocation : int {
        GLOBAL,
        LOCAL
    }; // enum CallLocation

private:
    Fa_Expr* m_callee { nullptr };
    Fa_ListExpr* m_args { nullptr };
    CallLocation m_call_location { CallLocation::GLOBAL }; // FIXED: Initialize member

public:
    Fa_CallExpr() = delete;

    explicit Fa_CallExpr(Fa_Expr* c, Fa_ListExpr* a, Fa_SourceLocation loc, CallLocation call_loc = CallLocation::GLOBAL)
        : Fa_Expr(loc, Kind::CALL)
        , m_callee(c)
        , m_args(a)
        , m_call_location(call_loc)
    {
        if (m_args == nullptr)
            m_args = ALLOCATE_AST_NODE(Fa_ListExpr, Fa_Array<Fa_Expr*> { }, loc);

        assert(m_callee != nullptr);
        assert(m_args != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto call = static_cast<Fa_CallExpr const*>(other);
        return m_callee->equals(call->get_callee())
            && m_args->equals(call->get_args_as_list_expr())
            && m_call_location == call->get_call_location();
    }
    [[nodiscard]] Fa_CallExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_CallExpr, m_callee->clone(), m_args->clone(), get_location());
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Fa_Expr* get_callee() const { return m_callee; }

    [[nodiscard]] Fa_Array<Fa_Expr*> const& get_args() const { return m_args->get_elements(); }
    [[nodiscard]] Fa_Array<Fa_Expr*>& get_args() { return m_args->get_elements(); }

    [[nodiscard]] Fa_ListExpr* get_args_as_list_expr() { return m_args; }
    [[nodiscard]] Fa_ListExpr const* get_args_as_list_expr() const { return m_args; }

    [[nodiscard]] CallLocation get_call_location() const
    {
        return m_call_location;
    }
    [[nodiscard]] bool has_arguments() const
    {
        return m_args != nullptr && !m_args->is_empty();
    }
}; // class Fa_CallExpr

class Fa_AssignmentExpr final : public Fa_Expr {
private:
    Fa_Expr* m_target { nullptr };
    Fa_Expr* m_value { nullptr };

public:
    Fa_AssignmentExpr(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::ASSIGNMENT)
        , m_target(target)
        , m_value(value)
    {
        assert(m_target != nullptr);
        assert(m_value != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Expr const* other) const override
    {
        if (other->get_kind() != m_kind)
            return false;

        auto bin = static_cast<Fa_AssignmentExpr const*>(other);
        return m_target->equals(bin->get_target()) && m_value->equals(bin->get_value());
    }
    [[nodiscard]] Fa_AssignmentExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_AssignmentExpr, m_target->clone(), m_value->clone(), get_location());
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Fa_Expr* get_target() const { return m_target; }
    [[nodiscard]] Fa_Expr* get_value() const { return m_value; }

    void set_target(Fa_Expr* t) { m_target = t; }
    void set_value(Fa_Expr* v) { m_value = v; }
}; // Fa_AssignmentExpr

class Fa_IndexExpr final : public Fa_Expr {
private:
    Fa_Expr* m_object { nullptr };
    Fa_Expr* m_index { nullptr };

    bool m_safe { false };

public:
    Fa_IndexExpr(Fa_Expr* obj, Fa_Expr* idx, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::INDEX_READ)
        , m_object(obj)
        , m_index(idx)
    {
        assert(m_object != nullptr);
        assert(m_index != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::INDEX_READ)
            return false;

        auto idx = static_cast<Fa_IndexExpr const*>(other);
        return m_object->equals(idx->get_object()) && m_index->equals(idx->get_index());
    }
    [[nodiscard]] Fa_IndexExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_IndexExpr, m_object->clone(), m_index->clone(), get_location());
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Fa_Expr* get_object() const { return m_object; }
    [[nodiscard]] Fa_Expr* get_index() const { return m_index; }

    [[nodiscard]] bool is_safe() const { return m_safe; }
    void make_safe() { m_safe = true; }

}; // class Fa_IndexExpr

class Fa_GetExpr final : public Fa_Expr {
private:
    Fa_Expr* m_object { nullptr };
    Fa_Expr* m_member { nullptr };

public:
    Fa_GetExpr(Fa_Expr* obj, Fa_Expr* mem, Fa_SourceLocation loc)
        : Fa_Expr(loc, Kind::GET)
        , m_object(obj)
        , m_member(mem)
    {
        assert(m_object != nullptr);
        assert(m_member != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Expr const* other) const override
    {
        if (other == nullptr || other->get_kind() != m_kind)
            return false;

        auto get_expr = static_cast<Fa_GetExpr const*>(other);
        return m_object->equals(get_expr->get_object()) && m_member->equals(get_expr->get_member());
    }
    [[nodiscard]] Fa_GetExpr* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_GetExpr, m_object->clone(), m_member->clone(), get_location());
    }
    void accept(Fa_ExprVisitor& v) override { v.visit(*this); }

    [[nodiscard]] Fa_Expr* get_object() const { return m_object; }
    [[nodiscard]] Fa_Expr* get_member() const { return m_member; }
};

class Fa_Stmt : public Fa_ASTNode {
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
        INVALID
    };

protected:
    Kind m_kind { Kind::INVALID };

public:
    Fa_Stmt() = default;

    explicit Fa_Stmt(Fa_SourceLocation loc, Kind kind)
        : Fa_ASTNode(loc)
        , m_kind(kind)
    {
    }

    virtual ~Fa_Stmt() = default;

    virtual Fa_Stmt* clone() const = 0;
    virtual bool equals(Fa_Stmt const* other) const = 0;
    virtual void accept(Fa_StmtVisitor& v) = 0;

    Kind get_kind() const { return m_kind; }
    NodeType get_node_type() const override { return NodeType::STATEMENT; }
}; // class Fa_Stmt

class Fa_BlockStmt final : public Fa_Stmt {
private:
    Fa_Array<Fa_Stmt*> m_statements;

public:
    explicit Fa_BlockStmt(Fa_Array<Fa_Stmt*> stmts, Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::BLOCK)
        , m_statements(stmts)
    {
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        if (other == nullptr || other->get_kind() != Kind::BLOCK)
            return false;

        auto block = static_cast<Fa_BlockStmt const*>(other);

        if (m_statements.size() != block->m_statements.size())
            return false;

        for (size_t i = 0; i < m_statements.size(); i += 1) {
            if (!m_statements[i]->equals(block->m_statements[i]))
                return false;
        }

        return true;
    }
    [[nodiscard]] Fa_BlockStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_BlockStmt, m_statements, get_location());
    }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Fa_Array<Fa_Stmt*> const& get_statements() const
    {
        return m_statements;
    }
    [[nodiscard]] bool is_empty() const
    {
        return m_statements.empty();
    }
    void set_statements(Fa_Array<Fa_Stmt*>& stmts)
    {
        m_statements = stmts;
    }
}; // class Fa_BlockStmt

class Fa_ExprStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_expr { nullptr };

public:
    explicit Fa_ExprStmt(Fa_Expr* expr, Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::EXPR)
        , m_expr(expr)
    {
        assert(m_expr != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<Fa_ExprStmt const*>(other);
        return m_expr->equals(block->get_expr());
    }
    [[nodiscard]] Fa_ExprStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_ExprStmt, m_expr->clone(), get_location());
    }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Fa_Expr* get_expr() const
    {
        return m_expr;
    }
    void set_expr(Fa_Expr* e)
    {
        m_expr = e;
    }
}; // class Fa_ExprStmt

class Fa_AssignmentStmt final : public Fa_Stmt {
private:
    Fa_AssignmentExpr* m_expr;

public:
    explicit Fa_AssignmentStmt(Fa_AssignmentExpr* e, Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::ASSIGNMENT)
        , m_expr(e->clone())
    {
        assert(m_expr != nullptr);
    }

    Fa_AssignmentStmt(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::ASSIGNMENT)
    {
        m_expr = ALLOCATE_AST_NODE(Fa_AssignmentExpr, target, value, loc); // Fa_AssignmentExpr will assert args for us
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<Fa_AssignmentStmt const*>(other);
        return m_expr->get_value()->equals(block->get_value()) && m_expr->get_target()->equals(block->get_target());
    }
    [[nodiscard]] Fa_AssignmentStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_AssignmentStmt, m_expr->get_target(), m_expr->get_value(), get_location());
    }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Fa_Expr* get_value() const
    {
        return m_expr->get_value();
    }
    [[nodiscard]] Fa_Expr* get_target() const
    {
        return m_expr->get_target();
    }
    [[nodiscard]] bool is_declaration() const;
    void set_value(Fa_Expr* v)
    {
        m_expr->set_value(v);
    }
    void set_target(Fa_Expr* t)
    {
        m_expr->set_target(t);
    }
    void set_decl();

    Fa_AssignmentExpr* get_expr() const { return m_expr; }
}; // class Fa_AssignmentExpr

class Fa_IfStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_condition { nullptr };
    Fa_Stmt* m_then_stmt { nullptr };
    Fa_Stmt* m_else_stmt { nullptr };

public:
    Fa_IfStmt(Fa_Expr* condition, Fa_Stmt* then_stmt, Fa_SourceLocation loc, Fa_Stmt* else_stmt)
        : Fa_Stmt(loc, Kind::IF)
        , m_condition(condition)
        , m_then_stmt(then_stmt)
        , m_else_stmt(else_stmt)
    {
        assert(m_condition != nullptr);
        assert(m_then_stmt != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto if_stmt = static_cast<Fa_IfStmt const*>(other);
        bool eq_else = false;

        if (m_else_stmt != nullptr && if_stmt->get_else())
            eq_else = m_else_stmt->equals(if_stmt->get_else());

        return m_condition->equals(if_stmt->get_condition()) && m_then_stmt->equals(if_stmt->get_then()) && eq_else;
    }
    [[nodiscard]] Fa_IfStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_IfStmt,
            m_condition->clone(),
            m_then_stmt->clone(), get_location(),
            LIKELY(m_else_stmt == nullptr) ? nullptr : m_else_stmt->clone());
    }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Fa_Expr* get_condition() const
    {
        return m_condition;
    }
    [[nodiscard]] Fa_Stmt* get_then() const
    {
        return m_then_stmt;
    }
    [[nodiscard]] Fa_Stmt* get_else() const
    {
        return m_else_stmt;
    }
    void set_then(Fa_Stmt* t)
    {
        m_then_stmt = t;
    }
    void set_else(Fa_Stmt* e)
    {
        m_else_stmt = e;
    }
}; // class Fa_IfStmt

class Fa_WhileStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_condition { nullptr };
    Fa_Stmt* m_body { nullptr };

public:
    Fa_WhileStmt(Fa_Expr* cond, Fa_Stmt* body, Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::WHILE)
        , m_condition(cond)
        , m_body(body)
    {
        assert(m_condition != nullptr);
        assert(m_body != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<Fa_WhileStmt const*>(other);
        return m_condition->equals(block->get_condition()) && m_body->equals(block->get_body());
    }
    [[nodiscard]] Fa_WhileStmt* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_WhileStmt, m_condition->clone(), m_body->clone(), get_location());
    }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Fa_Expr* get_condition() const
    {
        return m_condition;
    }
    [[nodiscard]] Fa_Stmt* get_body()
    {
        return m_body;
    }
    [[nodiscard]] Fa_Stmt const* get_body() const
    {
        return m_body;
    }

    void set_body(Fa_Stmt* b)
    {
        m_body = b;
    }
}; // class Fa_WhileStmt

class Fa_ForStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_container { nullptr };
    Fa_Expr* m_iter { nullptr };
    Fa_Stmt* m_body { nullptr };

public:
    Fa_ForStmt(Fa_Expr* target, Fa_Expr* iter, Fa_Stmt* body, Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::FOR)
        , m_container(target)
        , m_iter(iter)
        , m_body(body)
    {
        assert(m_container != nullptr);
        assert(m_iter != nullptr);
        assert(m_body != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<Fa_ForStmt const*>(other);
        return m_container->equals(block->get_container()) && m_iter->equals(block->get_iter()) && m_body->equals(block->get_body());
    }
    [[nodiscard]] Fa_ForStmt* clone() const override
    {
        return get_allocator().allocate_object<Fa_ForStmt>(m_container->clone(), m_iter->clone(), m_body->clone(), get_location());
    }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Fa_Expr* get_container() const { return m_container; }
    [[nodiscard]] Fa_NameExpr* get_target() const { return static_cast<Fa_NameExpr*>(m_container); }
    [[nodiscard]] Fa_Expr* get_iter() const { return m_iter; }
    [[nodiscard]] Fa_Stmt* get_body() const { return m_body; }

    void set_body(Fa_Stmt* b) { m_body = b; }
}; // class Fa_ForStmt

class Fa_FunctionDef final : public Fa_Stmt {
private:
    Fa_NameExpr* m_name { nullptr };
    Fa_ListExpr* m_params { nullptr };
    Fa_Stmt* m_body { nullptr };

public:
    Fa_FunctionDef(Fa_NameExpr* name, Fa_ListExpr* params, Fa_Stmt* body, Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::FUNC)
        , m_name(name)
        , m_params(params)
        , m_body(body)
    {
        assert(m_name != nullptr);
        assert(m_params != nullptr);
        assert(m_body != nullptr);
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto block = static_cast<Fa_FunctionDef const*>(other);
        return m_name->equals(block->get_name()) && m_params->equals(block->get_parameter_list()) && m_body->equals(block->get_body());
    }
    [[nodiscard]] Fa_FunctionDef* clone() const override
    {
        return ALLOCATE_AST_NODE(Fa_FunctionDef, m_name->clone(), m_params->clone(), m_body->clone(), get_location());
    }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Fa_NameExpr* get_name() const { return m_name; }
    [[nodiscard]] Fa_Array<Fa_Expr*> const& get_parameters() const { return m_params->get_elements(); }
    [[nodiscard]] Fa_ListExpr* get_parameter_list() const { return m_params; }
    [[nodiscard]] Fa_Stmt* get_body() const { return m_body; }
    [[nodiscard]] bool has_parameters() const { return m_params && !m_params->is_empty(); }

    void set_body(Fa_Stmt* b) { m_body = b; }
}; // class Fa_FunctionDef

class Fa_ReturnStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_value { nullptr };

public:
    explicit Fa_ReturnStmt(Fa_Expr* value, Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::RETURN)
        , m_value(value)
    {
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto ret_stmt = static_cast<Fa_ReturnStmt const*>(other);
        if (m_value == nullptr || ret_stmt->get_value() == nullptr)
            return m_value == ret_stmt->get_value();

        return m_value->equals(ret_stmt->get_value());
    }
    [[nodiscard]] Fa_ReturnStmt* clone() const override
    {
        auto ret_value = m_value == nullptr ? nullptr : m_value->clone();
        return ALLOCATE_AST_NODE(Fa_ReturnStmt, ret_value, get_location());
    }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Fa_Expr* get_value() { return m_value; }
    [[nodiscard]] Fa_Expr const* get_value() const { return m_value; }
    [[nodiscard]] bool has_value() const { return m_value != nullptr; }

    void set_value(Fa_Expr* v) { m_value = v; }
}; // class Fa_ReturnStmt

class Fa_ClassDef final : public Fa_Stmt {
private:
    Fa_Expr* m_name { nullptr };
    Fa_Array<Fa_Expr*> m_members { nullptr };
    Fa_Array<Fa_Stmt*> m_methods { nullptr };
    Fa_Array<Fa_Stmt*> m_sp_methods { nullptr };

public:
    explicit Fa_ClassDef(
        Fa_Expr* name,
        Fa_Array<Fa_Expr*> members,
        Fa_Array<Fa_Stmt*> methods,
        Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::CLASS_DEF)
        , m_name(name)
        , m_members(members)
        , m_methods(methods)
    {
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        if (m_kind != other->get_kind())
            return false;

        auto class_def = static_cast<Fa_ClassDef const*>(other);

        Fa_Array<Fa_Expr*> other_members = class_def->get_members();
        Fa_Array<Fa_Stmt*> other_methods = class_def->get_methods();
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

        return m_name->equals(class_def->get_name());
    }
    [[nodiscard]] Fa_ClassDef* clone() const override
    {
        Fa_Array<Fa_Expr*> member_clones;
        Fa_Array<Fa_Stmt*> method_clones;
        for (Fa_Expr* mem : m_members)
            member_clones.push(mem->clone());
        for (Fa_Stmt* met : m_methods)
            method_clones.push(met->clone());
        return ALLOCATE_AST_NODE(Fa_ClassDef, m_name->clone(), member_clones, method_clones, get_location());
    }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
    [[nodiscard]] Fa_Array<Fa_Expr*> get_members() const { return m_members; }
    [[nodiscard]] Fa_Array<Fa_Stmt*> get_methods() const { return m_methods; }
    [[nodiscard]] Fa_Expr* get_name() const { return m_name; }
}; // class Fa_ClassDef

class Fa_BreakStmt final : public Fa_Stmt {
public:
    explicit Fa_BreakStmt(Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::BREAK)
    {
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        return other != nullptr && other->get_kind() == Kind::BREAK;
    }
    [[nodiscard]] Fa_BreakStmt* clone() const override { return ALLOCATE_AST_NODE(Fa_BreakStmt, get_location()); }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
}; // class Fa_BreakStmt

class Fa_ContinueStmt final : public Fa_Stmt {
public:
    explicit Fa_ContinueStmt(Fa_SourceLocation loc)
        : Fa_Stmt(loc, Kind::CONTINUE)
    {
    }

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override
    {
        return other != nullptr && other->get_kind() == Kind::CONTINUE;
    }
    [[nodiscard]] Fa_ContinueStmt* clone() const override { return ALLOCATE_AST_NODE(Fa_ContinueStmt, get_location()); }
    void accept(Fa_StmtVisitor& v) override { v.visit(*this); }
}; // class Fa_ContinueStmt

static inline Fa_BinaryExpr* Fa_make_binary(Fa_Expr* lhs, Fa_Expr* rhs, Fa_BinaryOp const op, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_BinaryExpr, lhs, rhs, op, loc);
}
static inline Fa_UnaryExpr* Fa_make_unary(Fa_Expr* operand, Fa_UnaryOp const op, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_UnaryExpr, operand, op, loc);
}
static inline Fa_LiteralExpr* Fa_make_literal_nil(Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_LiteralExpr, loc);
}
static inline Fa_LiteralExpr* Fa_make_literal_int(int value, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_LiteralExpr, static_cast<i64>(value), Fa_LiteralExpr::Type::INTEGER, loc);
}
static inline Fa_LiteralExpr* Fa_make_literal_int(i64 value, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_LiteralExpr, value, Fa_LiteralExpr::Type::INTEGER, loc);
}
static inline Fa_LiteralExpr* Fa_make_literal_float(f64 value, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_LiteralExpr, value, Fa_LiteralExpr::Type::FLOAT, loc);
}
static inline Fa_LiteralExpr* Fa_make_literal_string(Fa_StringRef value, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_LiteralExpr, value, loc);
}
static inline Fa_LiteralExpr* Fa_make_literal_bool(bool value, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_LiteralExpr, value, loc);
}
static inline Fa_NameExpr* Fa_make_name(Fa_StringRef const str, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_NameExpr, str, loc);
}
static inline Fa_ListExpr* Fa_make_list(Fa_Array<Fa_Expr*> elements, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_ListExpr, elements, loc);
}
static inline Fa_DictExpr* Fa_make_dict(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> content, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_DictExpr, content, loc);
}
static inline Fa_GetExpr* Fa_make_get_expr(Fa_Expr* obj, Fa_Expr* member, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_GetExpr, obj, member, loc)
}
static inline Fa_CallExpr* Fa_make_call(Fa_Expr* callee, Fa_ListExpr* args, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_CallExpr, callee, args, loc);
}
static inline Fa_AssignmentExpr* Fa_make_assignment_expr(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_AssignmentExpr, target, value, loc);
}
static inline Fa_IndexExpr* Fa_make_index(Fa_Expr* obj, Fa_Expr* idx, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_IndexExpr, obj, idx, loc);
}
static inline Fa_BlockStmt* Fa_make_block(Fa_Array<Fa_Stmt*> stmts, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_BlockStmt, stmts, loc);
}
static inline Fa_ExprStmt* Fa_make_expr_stmt(Fa_Expr* expr, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_ExprStmt, expr, loc);
}
static inline Fa_AssignmentStmt* Fa_make_assignment_stmt(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_AssignmentStmt, target, value, loc);
}
static inline Fa_IfStmt* Fa_make_if(Fa_Expr* cond, Fa_Stmt* then_block, Fa_SourceLocation loc, Fa_Stmt* else_block = nullptr)
{
    return ALLOCATE_AST_NODE(Fa_IfStmt, cond, then_block, loc, else_block);
}
static inline Fa_WhileStmt* Fa_make_while(Fa_Expr* cond, Fa_Stmt* body, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_WhileStmt, cond, body, loc);
}
static inline Fa_ForStmt* Fa_make_for(Fa_NameExpr* target, Fa_Expr* iter, Fa_Stmt* body, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_ForStmt, target, iter, body, loc);
}
static inline Fa_FunctionDef* Fa_make_function(Fa_NameExpr* name, Fa_ListExpr* params, Fa_Stmt* body, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_FunctionDef, name, params, body, loc);
}
static inline Fa_ReturnStmt* Fa_make_return(Fa_SourceLocation loc, Fa_Expr* value = nullptr)
{
    return ALLOCATE_AST_NODE(Fa_ReturnStmt, value, loc);
}
static inline Fa_ClassDef* Fa_make_class_def(Fa_Expr* name, Fa_Array<Fa_Expr*> members,
    Fa_Array<Fa_Stmt*> methods, Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_ClassDef, name, members, methods, loc);
}
static inline Fa_BreakStmt* Fa_make_break(Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_BreakStmt, loc);
}
static inline Fa_ContinueStmt* Fa_make_continue(Fa_SourceLocation loc)
{
    return ALLOCATE_AST_NODE(Fa_ContinueStmt, loc);
}

#undef ALLOCATE_AST_NODE

static inline Fa_AssignmentExpr* as_assignment(Fa_Stmt* s)
{
    if (s == nullptr)
        return nullptr;

    if (s->get_kind() == Fa_Stmt::Kind::ASSIGNMENT)
        return static_cast<Fa_AssignmentStmt*>(s)->get_expr();
    if (s->get_kind() == Fa_Stmt::Kind::EXPR) {
        auto e = static_cast<Fa_ExprStmt*>(s)->get_expr();
        if (e->get_kind() == Fa_Expr::Kind::ASSIGNMENT)
            return static_cast<Fa_AssignmentExpr*>(e);
    }

    return nullptr;
}

static inline Fa_AssignmentExpr const* as_assignment(Fa_Stmt const* s)
{
    if (s == nullptr)
        return nullptr;

    if (s->get_kind() == Fa_Stmt::Kind::ASSIGNMENT)
        return static_cast<Fa_AssignmentStmt const*>(s)->get_expr();
    if (s->get_kind() == Fa_Stmt::Kind::EXPR) {
        auto e = static_cast<Fa_ExprStmt const*>(s)->get_expr();
        if (e->get_kind() == Fa_Expr::Kind::ASSIGNMENT)
            return static_cast<Fa_AssignmentExpr const*>(e);
    }

    return nullptr;
}

// helper macros

inline Fa_IfStmt* as_if(Fa_Stmt* s) { return static_cast<Fa_IfStmt*>(s); }
inline Fa_WhileStmt* as_while(Fa_Stmt* s) { return static_cast<Fa_WhileStmt*>(s); }
inline Fa_ForStmt* as_for(Fa_Stmt* s) { return static_cast<Fa_ForStmt*>(s); }
inline Fa_ReturnStmt* as_return(Fa_Stmt* s) { return static_cast<Fa_ReturnStmt*>(s); }
inline Fa_BreakStmt* as_break(Fa_Stmt* s) { return static_cast<Fa_BreakStmt*>(s); }
inline Fa_ContinueStmt* as_continue(Fa_Stmt* s) { return static_cast<Fa_ContinueStmt*>(s); }
inline Fa_BlockStmt* as_block(Fa_Stmt* s) { return static_cast<Fa_BlockStmt*>(s); }
inline Fa_FunctionDef* as_function_def(Fa_Stmt* s) { return static_cast<Fa_FunctionDef*>(s); }
inline Fa_ClassDef* as_class_def(Fa_Stmt* s) { return static_cast<Fa_ClassDef*>(s); }
inline Fa_AssignmentStmt* as_assignment_stmt(Fa_Stmt* s) { return static_cast<Fa_AssignmentStmt*>(s); }
inline Fa_ExprStmt* as_expr_stmt(Fa_Stmt* s) { return static_cast<Fa_ExprStmt*>(s); }

inline Fa_BinaryExpr* as_binary(Fa_Expr* e) { return static_cast<Fa_BinaryExpr*>(e); }
inline Fa_UnaryExpr* as_unary(Fa_Expr* e) { return static_cast<Fa_UnaryExpr*>(e); }
inline Fa_LiteralExpr* as_literal(Fa_Expr* e) { return static_cast<Fa_LiteralExpr*>(e); }
inline Fa_NameExpr* as_name(Fa_Expr* e) { return static_cast<Fa_NameExpr*>(e); }
inline Fa_IndexExpr* as_index(Fa_Expr* e) { return static_cast<Fa_IndexExpr*>(e); }
inline Fa_DictExpr* as_dict(Fa_Expr* e) { return static_cast<Fa_DictExpr*>(e); }
inline Fa_ListExpr* as_list(Fa_Expr* e) { return static_cast<Fa_ListExpr*>(e); }
inline Fa_CallExpr* as_call(Fa_Expr* e) { return static_cast<Fa_CallExpr*>(e); }
inline Fa_AssignmentExpr* as_assignment_expr(Fa_Expr* e) { return static_cast<Fa_AssignmentExpr*>(e); }
inline Fa_GetExpr* as_get_expr(Fa_Expr* e) { return static_cast<Fa_GetExpr*>(e); }

inline Fa_IfStmt const* as_if(Fa_Stmt const* s) { return static_cast<Fa_IfStmt const*>(s); }
inline Fa_WhileStmt const* as_while(Fa_Stmt const* s) { return static_cast<Fa_WhileStmt const*>(s); }
inline Fa_ForStmt const* as_for(Fa_Stmt const* s) { return static_cast<Fa_ForStmt const*>(s); }
inline Fa_ReturnStmt const* as_return(Fa_Stmt const* s) { return static_cast<Fa_ReturnStmt const*>(s); }
inline Fa_BreakStmt const* as_break(Fa_Stmt const* s) { return static_cast<Fa_BreakStmt const*>(s); }
inline Fa_ContinueStmt const* as_continue(Fa_Stmt const* s) { return static_cast<Fa_ContinueStmt const*>(s); }
inline Fa_BlockStmt const* as_block(Fa_Stmt const* s) { return static_cast<Fa_BlockStmt const*>(s); }
inline Fa_FunctionDef const* as_function_def(Fa_Stmt const* s) { return static_cast<Fa_FunctionDef const*>(s); }
inline Fa_ClassDef const* as_class_def(Fa_Stmt const* s) { return static_cast<Fa_ClassDef const*>(s); }
inline Fa_AssignmentStmt const* as_assignment_stmt(Fa_Stmt const* s) { return static_cast<Fa_AssignmentStmt const*>(s); }
inline Fa_ExprStmt const* as_expr_stmt(Fa_Stmt const* s) { return static_cast<Fa_ExprStmt const*>(s); }

inline Fa_BinaryExpr const* as_binary(Fa_Expr const* e) { return static_cast<Fa_BinaryExpr const*>(e); }
inline Fa_UnaryExpr const* as_unary(Fa_Expr const* e) { return static_cast<Fa_UnaryExpr const*>(e); }
inline Fa_LiteralExpr const* as_literal(Fa_Expr const* e) { return static_cast<Fa_LiteralExpr const*>(e); }
inline Fa_NameExpr const* as_name(Fa_Expr const* e) { return static_cast<Fa_NameExpr const*>(e); }
inline Fa_IndexExpr const* as_index(Fa_Expr const* e) { return static_cast<Fa_IndexExpr const*>(e); }
inline Fa_DictExpr const* as_dict(Fa_Expr const* e) { return static_cast<Fa_DictExpr const*>(e); }
inline Fa_ListExpr const* as_list(Fa_Expr const* e) { return static_cast<Fa_ListExpr const*>(e); }
inline Fa_CallExpr const* as_call(Fa_Expr const* e) { return static_cast<Fa_CallExpr const*>(e); }
inline Fa_AssignmentExpr const* as_assignment_expr(Fa_Expr const* e) { return static_cast<Fa_AssignmentExpr const*>(e); }
inline Fa_GetExpr const* as_get_expr(Fa_Expr const* e) { return static_cast<Fa_GetExpr const*>(e); }

static inline bool is_class_def(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::CLASS_DEF; }
static inline bool is_if(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::IF; }
static inline bool is_while(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::WHILE; }
static inline bool is_for(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::FOR; }
static inline bool is_return(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::RETURN; }
static inline bool is_break(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::BREAK; }
static inline bool is_continue(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::CONTINUE; }
static inline bool is_func(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::FUNC; }
static inline bool is_expr(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::EXPR; }
static inline bool is_block(Fa_Stmt const* s) { return s->get_kind() == Fa_Stmt::Kind::BLOCK; }

static inline bool is_binary(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::BINARY; }
static inline bool is_unary(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::UNARY; }
static inline bool is_literal(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::LITERAL; }
static inline bool is_name(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::NAME; }
static inline bool is_index(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::INDEX_READ; }
static inline bool is_dict(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::DICT; }
static inline bool is_list(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::LIST; }
static inline bool is_call(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::CALL; }
static inline bool is_assignment(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::ASSIGNMENT; }
static inline bool is_get(Fa_Expr const* e) { return e->get_kind() == Fa_Expr::Kind::GET; }

static inline std::tuple<Fa_Expr*, Fa_Expr*> assignment_parts(Fa_AssignmentExpr* e)
{
    return std::make_tuple<Fa_Expr*, Fa_Expr*>(e->get_target(), e->get_value());
}
static inline std::tuple<Fa_Expr*, Fa_Expr*> assignment_parts(Fa_AssignmentExpr const* e)
{
    return std::make_tuple<Fa_Expr*, Fa_Expr*>(e->get_target(), e->get_value());
}

static inline int literal_int(Fa_Expr const* e) { return as_literal(e)->get_int(); }
static inline Fa_StringRef literal_str(Fa_Expr const* e) { return as_literal(e)->get_str(); }
static inline float literal_float(Fa_Expr const* e) { return as_literal(e)->get_float(); }
static inline bool literal_bool(Fa_Expr const* e) { return as_literal(e)->get_bool(); }

} // namespace fairuz::ast

#endif // FA_AST_HPP
