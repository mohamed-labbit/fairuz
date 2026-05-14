#ifndef AST_HPP
#define AST_HPP

#include "arena.hpp"
#include "array.hpp"
#include "loop_header.hpp"
#include "macros.hpp"
#include "string.hpp"
#include "table.hpp"

#include <cassert>
#include <cstddef>
#include <unordered_map>

namespace fairuz::AST {

class Fa_Expr;
class Fa_ListExpr;
class Fa_NameExpr;
class Fa_AssignmentExpr;
class Fa_Stmt;
class Fa_BlockStmt;
class Fa_AssignmentStmt;

static Fa_ListExpr* Fa_make_list(Fa_Array<Fa_Expr*> elements, Fa_SourceLocation loc);
static Fa_BlockStmt* Fa_make_block(Fa_Array<Fa_Stmt*> stmts, Fa_SourceLocation loc);
static Fa_NameExpr* Fa_make_name(Fa_StringRef const str, Fa_SourceLocation loc);
static Fa_AssignmentExpr* Fa_make_assignment_expr(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc, bool decl = false);
static Fa_AssignmentStmt* Fa_make_assignment_stmt(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc, bool decl = false);

class Fa_ASTNode {
public:
    enum class NodeType : int {
        EXPRESSION,
        STATEMENT,
        INVALID
    }; // enum NodeType

private:
    NodeType node_type { NodeType::INVALID };

public:
    Fa_ASTNode() = default;
    Fa_ASTNode(Fa_ASTNode const&) = delete;
    Fa_ASTNode(Fa_ASTNode&&) = delete;

    Fa_ASTNode& operator=(Fa_ASTNode const&) = delete;
    Fa_ASTNode& operator=(Fa_ASTNode&&) = delete;

    [[nodiscard]] virtual NodeType get_node_type() const;
    [[nodiscard]] u32 get_line() const;
    [[nodiscard]] u16 get_column() const;

    virtual ~Fa_ASTNode() = default;
}; // class Fa_ASTNode

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
        INDEX,
        DICT,
        INVALID
    }; // enum Kind

protected:
    Fa_SourceLocation m_loc;
    Kind m_kind { Kind::INVALID };

public:
    Fa_Expr()
        : m_kind(Kind::INVALID)
    {
    }

    Fa_Expr(Fa_SourceLocation loc)
        : m_kind(Kind::INVALID)
        , m_loc(loc)
    {
    }

    virtual ~Fa_Expr() = default;

    virtual bool equals(Fa_Expr const* other) const = 0;
    virtual Fa_Expr* clone() const = 0;

    Kind get_kind() const { return m_kind; }
    Fa_SourceLocation get_location() const { return m_loc; }
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
        : m_left(l)
        , m_right(r)
        , m_operator(op)
    {
        assert(m_left != nullptr);
        assert(m_right != nullptr);
        m_kind = Kind::BINARY;
        m_loc = loc;
    }

    ~Fa_BinaryExpr() override = default;

    Fa_BinaryExpr(Fa_BinaryExpr&&) noexcept = delete;
    Fa_BinaryExpr(Fa_BinaryExpr const&) noexcept = delete;

    Fa_BinaryExpr& operator=(Fa_BinaryExpr const&) noexcept = delete;
    Fa_BinaryExpr& operator=(Fa_BinaryExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_BinaryExpr* clone() const override;

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
        : m_operand(operand)
        , m_operator(op)
    {
        assert(m_operand != nullptr);
        m_kind = Kind::UNARY;
        m_loc = loc;
    }

    ~Fa_UnaryExpr() override = default;

    Fa_UnaryExpr(Fa_UnaryExpr&&) noexcept = delete;
    Fa_UnaryExpr(Fa_UnaryExpr const&) noexcept = delete;

    Fa_UnaryExpr& operator=(Fa_UnaryExpr const&) noexcept = delete;
    Fa_UnaryExpr& operator=(Fa_UnaryExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_UnaryExpr* clone() const override;

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
        : m_type(Type::NIL)
    {
        m_kind = Kind::LITERAL;
        m_loc = loc;
    }

    Fa_LiteralExpr(i64 value, Type type, Fa_SourceLocation loc)
        : m_type(type)
        , int_value(value)
    {
        m_kind = Kind::LITERAL;
        m_loc = loc;
    }
    Fa_LiteralExpr(f64 value, Type type, Fa_SourceLocation loc)
        : m_type(type)
        , float_value(value)
    {
        m_kind = Kind::LITERAL;
        m_loc = loc;
    }

    explicit Fa_LiteralExpr(bool value, Fa_SourceLocation loc)
        : m_type(Type::BOOLEAN)
        , bool_value(value)
    {
        m_kind = Kind::LITERAL;
        m_loc = loc;
    }
    explicit Fa_LiteralExpr(Fa_StringRef str, Fa_SourceLocation loc)
        : m_type(Type::STRING)
        , str_value(std::move(str))
    {
        m_kind = Kind::LITERAL;
        m_loc = loc;
    }

    ~Fa_LiteralExpr() override = default;

    Fa_LiteralExpr(Fa_LiteralExpr&&) noexcept = delete;
    Fa_LiteralExpr(Fa_LiteralExpr const&) noexcept = delete;

    Fa_LiteralExpr& operator=(Fa_LiteralExpr const&) noexcept = delete;
    Fa_LiteralExpr& operator=(Fa_LiteralExpr&&) noexcept = delete;

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

    [[nodiscard]] f64 is_number() const
    {
        if (is_integer())
            return static_cast<f64>(int_value);
        if (is_float())
            return float_value;

        return 0.0;
    }

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_LiteralExpr* clone() const override;
}; // class Fa_LiteralExpr

class Fa_NameExpr final : public Fa_Expr {
private:
    Fa_StringRef m_value;

    bool m_is_local { false };

public:
    Fa_NameExpr() = default;

    explicit Fa_NameExpr(Fa_StringRef s, Fa_SourceLocation loc)
        : m_value(std::move(s))
    {
        m_kind = Kind::NAME;
        m_loc = loc;
    }

    ~Fa_NameExpr() override = default;

    Fa_NameExpr(Fa_NameExpr&&) noexcept = delete;
    Fa_NameExpr(Fa_NameExpr const&) noexcept = delete;

    Fa_NameExpr& operator=(Fa_NameExpr const&) noexcept = delete;
    Fa_NameExpr& operator=(Fa_NameExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_NameExpr* clone() const override;

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
        : m_elements(std::move(elements))
    {
        m_kind = Kind::LIST;
        m_loc = loc;
    }

    ~Fa_ListExpr() override = default;

    Fa_Expr* operator[](size_t const i) { return m_elements[i]; }
    Fa_Expr const* operator[](size_t const i) const { return m_elements[i]; }

    Fa_ListExpr(Fa_ListExpr&&) noexcept = delete;
    Fa_ListExpr(Fa_ListExpr const&) noexcept = delete;

    Fa_ListExpr& operator=(Fa_ListExpr const&) noexcept = delete;
    Fa_ListExpr& operator=(Fa_ListExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_ListExpr* clone() const override;

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
        : content(content)
    {
        m_kind = Kind::DICT;
        m_loc = loc;
    }

    bool equals(Fa_Expr const* other) const override;
    Fa_Expr* clone() const override;

    Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> get_content() const;
    void set_content(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> c);
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
        : m_callee(c)
        , m_args(a)
        , m_call_location(call_loc)
    {
        if (m_args == nullptr)
            m_args = Fa_make_list({ }, loc);

        assert(m_callee != nullptr);
        assert(m_args != nullptr);
        m_kind = Kind::CALL;
        m_loc = loc;
    }

    ~Fa_CallExpr() override = default;

    Fa_CallExpr(Fa_CallExpr&&) noexcept = delete;
    Fa_CallExpr(Fa_CallExpr const&) noexcept = delete;

    Fa_CallExpr& operator=(Fa_CallExpr const&) noexcept = delete;
    Fa_CallExpr& operator=(Fa_CallExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_CallExpr* clone() const override;

    [[nodiscard]] Fa_Expr* get_callee() const { return m_callee; }

    [[nodiscard]] Fa_Array<Fa_Expr*> const& get_args() const
    {
        static Fa_Array<Fa_Expr*> const empty;
        return m_args ? m_args->get_elements() : empty;
    }
    [[nodiscard]] Fa_Array<Fa_Expr*>& get_args()
    {
        assert(m_args && "Attempting to get mutable args when Args_ is null");
        return m_args->get_elements();
    }

    [[nodiscard]] Fa_ListExpr* get_args_as_list_expr() { return m_args; }
    [[nodiscard]] Fa_ListExpr const* get_args_as_list_expr() const { return m_args; }

    [[nodiscard]] CallLocation get_call_location() const;
    [[nodiscard]] bool has_arguments() const;
}; // class Fa_CallExpr

class Fa_AssignmentExpr final : public Fa_Expr {
private:
    Fa_Expr* m_target { nullptr };
    Fa_Expr* m_value { nullptr };

    bool m_is_decl = false;

public:
    Fa_AssignmentExpr() = delete;

    Fa_AssignmentExpr(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc, bool decl = false)
        : m_target(target)
        , m_value(value)
        , m_is_decl(decl)
    {
        assert(m_target != nullptr);
        assert(m_value != nullptr);
        m_kind = Kind::ASSIGNMENT;
        m_loc = loc;
    }

    ~Fa_AssignmentExpr() override = default;

    Fa_AssignmentExpr(Fa_AssignmentExpr&&) noexcept = delete;
    Fa_AssignmentExpr(Fa_AssignmentExpr const&) noexcept = delete;

    Fa_AssignmentExpr& operator=(Fa_AssignmentExpr const&) noexcept = delete;
    Fa_AssignmentExpr& operator=(Fa_AssignmentExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_AssignmentExpr* clone() const override;

    [[nodiscard]] Fa_Expr* get_target() const { return m_target; }
    [[nodiscard]] Fa_Expr* get_value() const { return m_value; }

    void set_target(Fa_Expr* t) { m_target = t; }
    void set_value(Fa_Expr* v) { m_value = v; }
    void set_decl() { m_is_decl = true; }

    [[nodiscard]] bool is_declaration() const { return m_is_decl; }
}; // Fa_AssignmentExpr

class Fa_IndexExpr final : public Fa_Expr {
private:
    Fa_Expr* m_object { nullptr };
    Fa_Expr* m_index { nullptr };

    bool m_safe { false };

public:
    Fa_IndexExpr() = delete;

    Fa_IndexExpr(Fa_Expr* obj, Fa_Expr* idx, Fa_SourceLocation loc)
        : m_object(obj)
        , m_index(idx)
    {
        assert(m_object != nullptr);
        assert(m_index != nullptr);
        m_kind = Kind::INDEX;
        m_loc = loc;
    }

    ~Fa_IndexExpr() override = default;

    Fa_IndexExpr(Fa_IndexExpr&&) noexcept = delete;
    Fa_IndexExpr(Fa_IndexExpr const&) noexcept = delete;

    Fa_IndexExpr& operator=(Fa_IndexExpr const&) noexcept = delete;
    Fa_IndexExpr& operator=(Fa_IndexExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_IndexExpr* clone() const override;

    [[nodiscard]] Fa_Expr* get_object() const { return m_object; }
    [[nodiscard]] Fa_Expr* get_index() const { return m_index; }

    [[nodiscard]] bool is_safe() const { return m_safe; }
    void make_safe() { m_safe = true; }

}; // class Fa_IndexExpr

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
    Fa_SourceLocation m_loc;
    Kind m_kind { Kind::INVALID };

public:
    Fa_Stmt() = default;

    explicit Fa_Stmt(Fa_SourceLocation loc)
        : m_kind(Kind::INVALID)
        , m_loc(loc)
    {
    }

    virtual ~Fa_Stmt() = default;

    virtual Fa_Stmt* clone() const = 0;
    virtual bool equals(Fa_Stmt const* other) const = 0;

    Kind get_kind() const { return m_kind; }
    Fa_SourceLocation get_location() const { return m_loc; }
    NodeType get_node_type() const override { return NodeType::STATEMENT; }
}; // class Fa_Stmt

class Fa_BlockStmt final : public Fa_Stmt {
private:
    Fa_Array<Fa_Stmt*> m_statements;

public:
    Fa_BlockStmt() = delete;

    explicit Fa_BlockStmt(Fa_Array<Fa_Stmt*> stmts, Fa_SourceLocation loc)
        : m_statements(stmts)
    {
        m_kind = Kind::BLOCK;
        m_loc = loc;
    }

    Fa_BlockStmt(Fa_BlockStmt&&) noexcept = delete;
    Fa_BlockStmt(Fa_BlockStmt const&) noexcept = delete;

    Fa_BlockStmt& operator=(Fa_BlockStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_BlockStmt* clone() const override;
    [[nodiscard]] Fa_Array<Fa_Stmt*> const& get_statements() const;
    [[nodiscard]] bool is_empty() const;
    void set_statements(Fa_Array<Fa_Stmt*>& stmts);
}; // class Fa_BlockStmt

class Fa_ExprStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_expr { nullptr };

public:
    Fa_ExprStmt() = delete;

    explicit Fa_ExprStmt(Fa_Expr* m_expr, Fa_SourceLocation loc)
        : m_expr(m_expr)
    {
        assert(m_expr != nullptr);
        m_kind = Kind::EXPR;
        m_loc = loc;
    }

    Fa_ExprStmt(Fa_ExprStmt&&) noexcept = delete;
    Fa_ExprStmt(Fa_ExprStmt const&) noexcept = delete;

    Fa_ExprStmt& operator=(Fa_ExprStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ExprStmt* clone() const override;
    [[nodiscard]] Fa_Expr* get_expr() const;
    void set_expr(Fa_Expr* e);
}; // class Fa_ExprStmt

class Fa_AssignmentStmt final : public Fa_Stmt {
private:
    Fa_AssignmentExpr* m_expr;

public:
    Fa_AssignmentStmt() = delete;

    explicit Fa_AssignmentStmt(Fa_AssignmentExpr* e, Fa_SourceLocation loc)
        : m_expr(e->clone())
    {
        assert(m_expr != nullptr);
        m_kind = Kind::ASSIGNMENT;
        m_loc = loc;
    }

    Fa_AssignmentStmt(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc, bool decl = false)
    {
        m_expr = Fa_make_assignment_expr(target, value, loc, decl); // Fa_AssignmentExpr will assert args for us
        m_kind = Kind::ASSIGNMENT;
        m_loc = loc;
    }

    Fa_AssignmentStmt(Fa_AssignmentStmt&&) noexcept = delete;
    Fa_AssignmentStmt(Fa_AssignmentStmt const&) noexcept = delete;

    Fa_AssignmentStmt& operator=(Fa_AssignmentStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_AssignmentStmt* clone() const override;
    [[nodiscard]] Fa_Expr* get_value() const;
    [[nodiscard]] Fa_Expr* get_target() const;
    [[nodiscard]] bool is_declaration() const;
    void set_value(Fa_Expr* v);
    void set_target(Fa_Expr* t);
    void set_decl();

    Fa_AssignmentExpr* get_expr() const { return m_expr; }
}; // class Fa_AssignmentExpr

class Fa_IfStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_condition { nullptr };
    Fa_Stmt* m_then_stmt { nullptr };
    Fa_Stmt* m_else_stmt { nullptr };

public:
    Fa_IfStmt() = delete;

    Fa_IfStmt(Fa_Expr* condition, Fa_Stmt* then_stmt, Fa_SourceLocation loc, Fa_Stmt* else_stmt)
        : m_condition(condition)
        , m_then_stmt(then_stmt)
        , m_else_stmt(else_stmt)
    {
        assert(m_condition != nullptr);
        assert(m_then_stmt != nullptr);
        m_kind = Kind::IF;
        m_loc = loc;
    }

    Fa_IfStmt(Fa_IfStmt&&) noexcept = delete;
    Fa_IfStmt(Fa_IfStmt const&) noexcept = delete;

    Fa_IfStmt& operator=(Fa_IfStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_IfStmt* clone() const override;
    [[nodiscard]] Fa_Expr* get_condition() const;
    [[nodiscard]] Fa_Stmt* get_then() const;
    [[nodiscard]] Fa_Stmt* get_else() const;
    void set_then(Fa_Stmt* t);
    void set_else(Fa_Stmt* e);
}; // class Fa_IfStmt

class Fa_WhileStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_condition { nullptr };
    Fa_Stmt* m_body { nullptr };

    LoopHeader m_header;

public:
    Fa_WhileStmt() = delete;

    Fa_WhileStmt(Fa_Expr* cond, Fa_Stmt* body, Fa_SourceLocation loc)
        : m_condition(cond)
        , m_body(body)
    {
        assert(m_condition != nullptr);
        assert(m_body != nullptr);
        m_kind = Kind::WHILE;
        m_loc = loc;
    }

    Fa_WhileStmt(Fa_WhileStmt&&) noexcept = delete;
    Fa_WhileStmt(Fa_WhileStmt const&) noexcept = delete;

    Fa_WhileStmt& operator=(Fa_WhileStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_WhileStmt* clone() const override;
    [[nodiscard]] Fa_Expr* get_condition() const;
    [[nodiscard]] Fa_Stmt* get_body();
    [[nodiscard]] Fa_Stmt const* get_body() const;
    [[nodiscard]] LoopHeader get_header() const;

    void set_header(LoopHeader h);
    void set_body(Fa_Stmt* b);
}; // class Fa_WhileStmt

class Fa_ForStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_container { nullptr };
    Fa_Expr* m_iter { nullptr };
    Fa_Stmt* m_body { nullptr };

    LoopHeader m_header;

public:
    Fa_ForStmt() = delete;

    Fa_ForStmt(Fa_Expr* target, Fa_Expr* iter, Fa_Stmt* body, Fa_SourceLocation loc)
        : m_container(target)
        , m_iter(iter)
        , m_body(body)
    {
        assert(m_container != nullptr);
        assert(m_iter != nullptr);
        assert(m_body != nullptr);
        m_kind = Kind::FOR;
        m_loc = loc;
    }

    Fa_ForStmt(Fa_ForStmt&&) noexcept = delete;
    Fa_ForStmt(Fa_ForStmt const&) noexcept = delete;

    Fa_ForStmt& operator=(Fa_ForStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ForStmt* clone() const override;
    [[nodiscard]] Fa_Expr* get_container() const;
    [[nodiscard]] Fa_NameExpr* get_target() const;
    [[nodiscard]] Fa_Expr* get_iter() const;
    [[nodiscard]] Fa_Stmt* get_body() const;
    [[nodiscard]] LoopHeader get_header() const;

    void set_header(LoopHeader h);
    void set_body(Fa_Stmt* b);
}; // class Fa_ForStmt

class Fa_FunctionDef final : public Fa_Stmt {
private:
    Fa_NameExpr* m_name { nullptr };
    Fa_ListExpr* m_params { nullptr };
    Fa_Stmt* m_body { nullptr };

public:
    Fa_FunctionDef() = delete;

    Fa_FunctionDef(Fa_NameExpr* name, Fa_ListExpr* params, Fa_Stmt* body, Fa_SourceLocation loc)
        : m_name(name)
        , m_params(params)
        , m_body(body)
    {
        assert(m_name != nullptr);
        assert(m_params != nullptr);
        assert(m_body != nullptr);
        m_kind = Kind::FUNC;
        m_loc = loc;
    }

    Fa_FunctionDef(Fa_FunctionDef&&) noexcept = delete;
    Fa_FunctionDef(Fa_FunctionDef const&) noexcept = delete;

    Fa_FunctionDef& operator=(Fa_FunctionDef const&) noexcept = delete;
    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_FunctionDef* clone() const override;
    [[nodiscard]] Fa_NameExpr* get_name() const;
    [[nodiscard]] Fa_Array<Fa_Expr*> const& get_parameters() const;
    [[nodiscard]] Fa_ListExpr* get_parameter_list() const;
    [[nodiscard]] Fa_Stmt* get_body() const;
    [[nodiscard]] bool has_parameters() const;

    void set_body(Fa_Stmt* b);
}; // class Fa_FunctionDef

class Fa_ReturnStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_value { nullptr };

public:
    Fa_ReturnStmt() = delete;

    explicit Fa_ReturnStmt(Fa_Expr* value, Fa_SourceLocation loc)
        : m_value(value)
    {
        m_kind = Kind::RETURN;
        m_loc = loc;
    }

    Fa_ReturnStmt(Fa_ReturnStmt&&) noexcept = delete;
    Fa_ReturnStmt(Fa_ReturnStmt const&) noexcept = delete;

    Fa_ReturnStmt& operator=(Fa_ReturnStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ReturnStmt* clone() const override;
    [[nodiscard]] Fa_Expr* get_value();
    [[nodiscard]] Fa_Expr const* get_value() const;
    [[nodiscard]] bool has_value() const;

    void set_value(Fa_Expr* v);
}; // class Fa_ReturnStmt

class Fa_ClassDef final : public Fa_Stmt {
private:
    Fa_Expr* m_name { nullptr };
    Fa_Array<Fa_Expr*> m_members { nullptr };
    Fa_Array<Fa_Stmt*> m_methods { nullptr };

public:
    Fa_ClassDef() = delete;

    explicit Fa_ClassDef(Fa_Expr* name, Fa_Array<Fa_Expr*> members, Fa_Array<Fa_Stmt*> methods, Fa_SourceLocation loc)
        : m_name(name)
        , m_members(members)
        , m_methods(methods)
    {
        m_kind = Kind::CLASS_DEF;
        m_loc = loc;
    }

    Fa_ClassDef(Fa_ClassDef&&) noexcept = delete;
    Fa_ClassDef(Fa_ClassDef const&) noexcept = delete;

    Fa_ClassDef& operator=(Fa_ClassDef const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ClassDef* clone() const override;
    [[nodiscard]] Fa_Array<Fa_Expr*> get_members() const;
    [[nodiscard]] Fa_Array<Fa_Stmt*> get_methods() const;
    [[nodiscard]] Fa_Expr* get_name() const;
}; // class Fa_ClassDef

class Fa_BreakStmt final : public Fa_Stmt {
public:
    explicit Fa_BreakStmt(Fa_SourceLocation loc)
    {
        m_kind = Kind::BREAK;
        m_loc = loc;
    }

    Fa_BreakStmt(Fa_BreakStmt&&) noexcept = delete;
    Fa_BreakStmt(Fa_BreakStmt const&) noexcept = delete;

    Fa_BreakStmt& operator=(Fa_BreakStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_BreakStmt* clone() const override;
}; // class Fa_BreakStmt

class Fa_ContinueStmt final : public Fa_Stmt {
public:
    explicit Fa_ContinueStmt(Fa_SourceLocation loc)
    {
        m_kind = Kind::CONTINUE;
        m_loc = loc;
    }

    Fa_ContinueStmt(Fa_ContinueStmt&&) noexcept = delete;
    Fa_ContinueStmt(Fa_ContinueStmt const&) noexcept = delete;

    Fa_ContinueStmt& operator=(Fa_ContinueStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ContinueStmt* clone() const override;
}; // class Fa_ContinueStmt

#define ALLOCATE_AST_NODE(type, ...) get_allocator().allocate_object<type>(__VA_ARGS__);

static Fa_BinaryExpr* Fa_make_binary(Fa_Expr* lhs, Fa_Expr* rhs, Fa_BinaryOp const op, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_BinaryExpr, lhs, rhs, op, loc); }
static Fa_UnaryExpr* Fa_make_unary(Fa_Expr* operand, Fa_UnaryOp const op, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_UnaryExpr, operand, op, loc); }
static Fa_LiteralExpr* Fa_make_literal_nil(Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_LiteralExpr, loc); }
static Fa_LiteralExpr* Fa_make_literal_int(int value, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_LiteralExpr, static_cast<i64>(value), Fa_LiteralExpr::Type::INTEGER, loc); }
static Fa_LiteralExpr* Fa_make_literal_int(i64 value, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_LiteralExpr, value, Fa_LiteralExpr::Type::INTEGER, loc); }
static Fa_LiteralExpr* Fa_make_literal_float(f64 value, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_LiteralExpr, value, Fa_LiteralExpr::Type::FLOAT, loc); }
static Fa_LiteralExpr* Fa_make_literal_string(Fa_StringRef value, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_LiteralExpr, value, loc); }
static Fa_LiteralExpr* Fa_make_literal_bool(bool value, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_LiteralExpr, value, loc); }
static Fa_NameExpr* Fa_make_name(Fa_StringRef const str, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_NameExpr, str, loc); }
static Fa_ListExpr* Fa_make_list(Fa_Array<Fa_Expr*> elements, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_ListExpr, elements, loc); }
static Fa_DictExpr* Fa_make_dict(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> content, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_DictExpr, content, loc); }
static Fa_CallExpr* Fa_make_call(Fa_Expr* callee, Fa_ListExpr* args, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_CallExpr, callee, args, loc); }
static Fa_AssignmentExpr* Fa_make_assignment_expr(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc, bool decl) { return ALLOCATE_AST_NODE(Fa_AssignmentExpr, target, value, loc, decl); }
static Fa_IndexExpr* Fa_make_index(Fa_Expr* obj, Fa_Expr* idx, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_IndexExpr, obj, idx, loc); }
static Fa_BlockStmt* Fa_make_block(Fa_Array<Fa_Stmt*> stmts, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_BlockStmt, stmts, loc); }
static Fa_ExprStmt* Fa_make_expr_stmt(Fa_Expr* expr, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_ExprStmt, expr, loc); }
static Fa_AssignmentStmt* Fa_make_assignment_stmt(Fa_Expr* target, Fa_Expr* value, Fa_SourceLocation loc, bool decl) { return ALLOCATE_AST_NODE(Fa_AssignmentStmt, target, value, loc, decl); }
static Fa_IfStmt* Fa_make_if(Fa_Expr* cond, Fa_Stmt* then_block, Fa_SourceLocation loc, Fa_Stmt* else_block = nullptr) { return ALLOCATE_AST_NODE(Fa_IfStmt, cond, then_block, loc, else_block); }
static Fa_WhileStmt* Fa_make_while(Fa_Expr* cond, Fa_Stmt* body, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_WhileStmt, cond, body, loc); }
static Fa_ForStmt* Fa_make_for(Fa_NameExpr* target, Fa_Expr* iter, Fa_Stmt* body, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_ForStmt, target, iter, body, loc); }
static Fa_FunctionDef* Fa_make_function(Fa_NameExpr* name, Fa_ListExpr* params, Fa_Stmt* body, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_FunctionDef, name, params, body, loc); }
static Fa_ReturnStmt* Fa_make_return(Fa_SourceLocation loc, Fa_Expr* value = nullptr) { return ALLOCATE_AST_NODE(Fa_ReturnStmt, value, loc); }
static Fa_ClassDef* Fa_make_class_def(Fa_Expr* name, Fa_Array<Fa_Expr*> members, Fa_Array<Fa_Stmt*> methods, Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_ClassDef, name, members, methods, loc); }
static Fa_BreakStmt* Fa_make_break(Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_BreakStmt, loc); }
static Fa_ContinueStmt* Fa_make_continue(Fa_SourceLocation loc) { return ALLOCATE_AST_NODE(Fa_ContinueStmt, loc); }

#undef ALLOCATE_AST_NODE

} // namespace fairuz::ast

#endif // AST_HPP
