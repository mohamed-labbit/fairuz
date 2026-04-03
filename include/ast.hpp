#ifndef AST_HPP
#define AST_HPP

#include "arena.hpp"
#include "array.hpp"
#include "string.hpp"
#include <cassert>
#include <cstddef>

namespace fairuz::AST {

struct Fa_Expr;
struct Fa_ListExpr;
struct Fa_NameExpr;
struct Fa_AssignmentExpr;
struct Fa_Stmt;
struct Fa_BlockStmt;
struct Fa_AssignmentStmt;

static Fa_ListExpr* Fa_makeList(Fa_Array<Fa_Expr*> m_elements = { });
static Fa_BlockStmt* Fa_makeBlock(Fa_Array<Fa_Stmt*> stmts = { });
static Fa_NameExpr* Fa_makeName(Fa_StringRef const str);
static Fa_AssignmentExpr* Fa_makeAssignmentExpr(Fa_Expr* m_target, Fa_Expr* m_value, bool decl = false);
static Fa_AssignmentStmt* Fa_makeAssignmentStmt(Fa_Expr* m_target, Fa_Expr* m_value, bool decl = false);

struct Fa_ASTNode {
public:
    enum class NodeType : int {
        EXPRESSION,
        STATEMENT,
        INVALID
    }; // enum NodeType

private:
    u32 m_line { 0 };
    u16 m_column { 0 };

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
    void set_line(u32 m_line);
    void set_column(u16 col);
    virtual ~Fa_ASTNode() = default;
}; // struct Fa_ASTNode

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

struct Fa_Expr : public Fa_ASTNode {
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
    Kind kind { Kind::INVALID };

public:
    Fa_Expr()
        : kind(Kind::INVALID)
    {
    }

    virtual ~Fa_Expr() = default;

    virtual bool equals(Fa_Expr const* other) const = 0;
    virtual Fa_Expr* clone() const = 0;

    Kind get_kind() const { return kind; }
    NodeType get_node_type() const override { return NodeType::EXPRESSION; }
}; // struct Fa_Expr

struct Fa_BinaryExpr final : public Fa_Expr {
private:
    Fa_Expr* m_left { nullptr };
    Fa_Expr* m_right { nullptr };
    Fa_BinaryOp m_operator { Fa_BinaryOp::INVALID };

public:
    Fa_BinaryExpr() = delete;

    Fa_BinaryExpr(Fa_Expr* l, Fa_Expr* r, Fa_BinaryOp op)
        : m_left(l)
        , m_right(r)
        , m_operator(op)
    {
        assert(m_left != nullptr);
        assert(m_right != nullptr);
        kind = Kind::BINARY;
    }

    ~Fa_BinaryExpr() override = default;

    Fa_BinaryExpr(Fa_BinaryExpr&&) noexcept = delete;
    Fa_BinaryExpr(Fa_BinaryExpr const&) noexcept = delete;

    Fa_BinaryExpr& operator=(Fa_BinaryExpr const&) noexcept = delete;
    Fa_BinaryExpr& operator=(Fa_BinaryExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_BinaryExpr* clone() const override;
    [[nodiscard]] Fa_Expr* get_left() const;
    [[nodiscard]] Fa_Expr* get_right() const;
    [[nodiscard]] Fa_BinaryOp get_operator() const;

    void set_left(Fa_Expr* l);
    void set_right(Fa_Expr* r);
    void set_operator(Fa_BinaryOp op);
}; // struct Fa_BinaryExpr

struct Fa_UnaryExpr final : public Fa_Expr {
private:
    Fa_Expr* m_operand { nullptr };
    Fa_UnaryOp m_operator { Fa_UnaryOp::INVALID };

public:
    Fa_UnaryExpr() = delete;

    Fa_UnaryExpr(Fa_Expr* m_operand, Fa_UnaryOp op)
        : m_operand(m_operand)
        , m_operator(op)
    {
        assert(m_operand != nullptr);
        kind = Kind::UNARY;
    }

    ~Fa_UnaryExpr() override = default;

    Fa_UnaryExpr(Fa_UnaryExpr&&) noexcept = delete;
    Fa_UnaryExpr(Fa_UnaryExpr const&) noexcept = delete;

    Fa_UnaryExpr& operator=(Fa_UnaryExpr const&) noexcept = delete;
    Fa_UnaryExpr& operator=(Fa_UnaryExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_UnaryExpr* clone() const override;
    [[nodiscard]] Fa_Expr* get_operand() const;
    [[nodiscard]] Fa_UnaryOp get_operator() const;
}; // struct Fa_UnaryExpr

struct Fa_LiteralExpr final : public Fa_Expr {
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
    explicit Fa_LiteralExpr()
        : m_type(Type::NIL)
    {
        kind = Kind::LITERAL;
    }

    Fa_LiteralExpr(i64 m_value, Type m_type)
        : m_type(m_type)
        , int_value(m_value)
    {
        kind = Kind::LITERAL;
    }
    Fa_LiteralExpr(f64 m_value, Type m_type)
        : m_type(m_type)
        , float_value(m_value)
    {
        kind = Kind::LITERAL;
    }

    explicit Fa_LiteralExpr(bool m_value)
        : m_type(Type::BOOLEAN)
        , bool_value(m_value)
    {
        kind = Kind::LITERAL;
    }
    explicit Fa_LiteralExpr(Fa_StringRef str)
        : m_type(Type::STRING)
        , str_value(std::move(str))
    {
        kind = Kind::LITERAL;
    }

    ~Fa_LiteralExpr() override = default;

    Fa_LiteralExpr(Fa_LiteralExpr&&) noexcept = delete;
    Fa_LiteralExpr(Fa_LiteralExpr const&) noexcept = delete;

    Fa_LiteralExpr& operator=(Fa_LiteralExpr const&) noexcept = delete;
    Fa_LiteralExpr& operator=(Fa_LiteralExpr&&) noexcept = delete;

    [[nodiscard]] Type get_type() const;
    [[nodiscard]] i64 get_int() const;
    [[nodiscard]] f64 get_float() const;
    [[nodiscard]] bool get_bool() const;
    [[nodiscard]] Fa_StringRef get_str() const;

    [[nodiscard]] bool is_integer() const;
    [[nodiscard]] bool is_float() const;
    [[nodiscard]] bool is_bool() const;
    [[nodiscard]] bool is_string() const;
    [[nodiscard]] bool is_numeric() const;
    [[nodiscard]] bool is_nil() const;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_LiteralExpr* clone() const override;
    [[nodiscard]] f64 is_number() const;
}; // struct Fa_LiteralExpr

struct Fa_NameExpr final : public Fa_Expr {
private:
    Fa_StringRef m_value;

public:
    Fa_NameExpr() = default;

    explicit Fa_NameExpr(Fa_StringRef s)
        : m_value(std::move(s))
    {
        kind = Kind::NAME;
    }

    ~Fa_NameExpr() override = default;

    Fa_NameExpr(Fa_NameExpr&&) noexcept = delete;
    Fa_NameExpr(Fa_NameExpr const&) noexcept = delete;

    Fa_NameExpr& operator=(Fa_NameExpr const&) noexcept = delete;
    Fa_NameExpr& operator=(Fa_NameExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_NameExpr* clone() const override;
    [[nodiscard]] Fa_StringRef get_value() const;
}; // struct Fa_NameExpr

struct Fa_ListExpr final : public Fa_Expr {
private:
    Fa_Array<Fa_Expr*> m_elements;

public:
    Fa_ListExpr() = default;

    explicit Fa_ListExpr(Fa_Array<Fa_Expr*> m_elements)
        : m_elements(std::move(m_elements))
    {
        kind = Kind::LIST;
    }

    ~Fa_ListExpr() override = default;

    Fa_Expr* operator[](size_t const i);
    Fa_Expr const* operator[](size_t const i) const;

    Fa_ListExpr(Fa_ListExpr&&) noexcept = delete;
    Fa_ListExpr(Fa_ListExpr const&) noexcept = delete;

    Fa_ListExpr& operator=(Fa_ListExpr const&) noexcept = delete;
    Fa_ListExpr& operator=(Fa_ListExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_ListExpr* clone() const override;
    [[nodiscard]] Fa_Array<Fa_Expr*> const& get_elements() const;
    [[nodiscard]] Fa_Array<Fa_Expr*>& get_elements();
    [[nodiscard]] bool is_empty() const;
    [[nodiscard]] size_t size() const;
}; // struct Fa_ListExpr

struct Fa_DictExpr final : public Fa_Expr {
private:
    Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> content;

public:
    Fa_DictExpr(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> content)
        : content(content)
    {
        kind = Kind::DICT;
    }

    bool equals(Fa_Expr const* other) const override;
    Fa_Expr* clone() const override;

    Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> get_content() const;
    void set_content(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> c);
};

struct Fa_CallExpr final : public Fa_Expr {
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

    explicit Fa_CallExpr(Fa_Expr* c, Fa_ListExpr* a = nullptr, CallLocation loc = CallLocation::GLOBAL)
        : m_callee(c)
        , m_args(a)
        , m_call_location(loc)
    {
        if (m_args == nullptr)
            m_args = Fa_makeList();

        assert(m_callee != nullptr);
        assert(m_args != nullptr);
        kind = Kind::CALL;
    }

    ~Fa_CallExpr() override = default;

    Fa_CallExpr(Fa_CallExpr&&) noexcept = delete;
    Fa_CallExpr(Fa_CallExpr const&) noexcept = delete;

    Fa_CallExpr& operator=(Fa_CallExpr const&) noexcept = delete;
    Fa_CallExpr& operator=(Fa_CallExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_CallExpr* clone() const override;
    [[nodiscard]] Fa_Expr* get_callee() const;
    [[nodiscard]] Fa_Array<Fa_Expr*> const& get_args() const;
    [[nodiscard]] Fa_Array<Fa_Expr*>& get_args();
    [[nodiscard]] Fa_ListExpr* get_args_as_list_expr();
    [[nodiscard]] Fa_ListExpr const* get_args_as_list_expr() const;
    [[nodiscard]] CallLocation get_call_location() const;
    [[nodiscard]] bool has_arguments() const;
}; // struct Fa_CallExpr

struct Fa_AssignmentExpr final : public Fa_Expr {
private:
    Fa_Expr* m_target { nullptr };
    Fa_Expr* m_value { nullptr };

    bool m_is_decl = false;

public:
    Fa_AssignmentExpr() = delete;

    Fa_AssignmentExpr(Fa_Expr* m_target, Fa_Expr* m_value, bool decl = false)
        : m_target(m_target)
        , m_value(m_value)
        , m_is_decl(decl)
    {
        assert(m_target != nullptr);
        assert(m_value != nullptr);
        kind = Kind::ASSIGNMENT;
    }

    ~Fa_AssignmentExpr() override = default;

    Fa_AssignmentExpr(Fa_AssignmentExpr&&) noexcept = delete;
    Fa_AssignmentExpr(Fa_AssignmentExpr const&) noexcept = delete;

    Fa_AssignmentExpr& operator=(Fa_AssignmentExpr const&) noexcept = delete;
    Fa_AssignmentExpr& operator=(Fa_AssignmentExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_AssignmentExpr* clone() const override;
    [[nodiscard]] Fa_Expr* get_target() const;
    [[nodiscard]] Fa_Expr* get_value() const;
    void set_target(Fa_Expr* t);
    void set_value(Fa_Expr* v);
    void set_decl();
    [[nodiscard]] bool is_declaration() const;
}; // Fa_AssignmentExpr

struct Fa_IndexExpr final : public Fa_Expr {
private:
    Fa_Expr* m_object { nullptr };
    Fa_Expr* m_index { nullptr };

public:
    Fa_IndexExpr() = delete;

    Fa_IndexExpr(Fa_Expr* obj, Fa_Expr* idx)
        : m_object(obj)
        , m_index(idx)
    {
        assert(m_object != nullptr);
        assert(m_index != nullptr);
        kind = Kind::INDEX;
    }

    ~Fa_IndexExpr() override = default;

    Fa_IndexExpr(Fa_IndexExpr&&) noexcept = delete;
    Fa_IndexExpr(Fa_IndexExpr const&) noexcept = delete;

    Fa_IndexExpr& operator=(Fa_IndexExpr const&) noexcept = delete;
    Fa_IndexExpr& operator=(Fa_IndexExpr&&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Expr const* other) const override;
    [[nodiscard]] Fa_IndexExpr* clone() const override;
    [[nodiscard]] Fa_Expr* get_object() const;
    [[nodiscard]] Fa_Expr* get_index() const;
}; // struct Fa_IndexExpr

struct Fa_Stmt : public Fa_ASTNode {
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
        INVALID
    };

protected:
    Kind kind { Kind::INVALID };

public:
    explicit Fa_Stmt() = default;
    virtual ~Fa_Stmt() = default;

    virtual Fa_Stmt* clone() const = 0;
    virtual bool equals(Fa_Stmt const* other) const = 0;

    Kind get_kind() const { return kind; }
    NodeType get_node_type() const override { return NodeType::STATEMENT; }
}; // struct Fa_Stmt

struct Fa_BlockStmt final : public Fa_Stmt {
private:
    Fa_Array<Fa_Stmt*> m_statements;

public:
    Fa_BlockStmt() = delete;

    explicit Fa_BlockStmt(Fa_Array<Fa_Stmt*> stmts)
        : m_statements(stmts)
    {
        kind = Kind::BLOCK;
    }

    Fa_BlockStmt(Fa_BlockStmt&&) noexcept = delete;
    Fa_BlockStmt(Fa_BlockStmt const&) noexcept = delete;

    Fa_BlockStmt& operator=(Fa_BlockStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_BlockStmt* clone() const override;
    [[nodiscard]] Fa_Array<Fa_Stmt*> const& get_statements() const;
    [[nodiscard]] bool is_empty() const;
    void set_statements(Fa_Array<Fa_Stmt*>& stmts);
}; // struct Fa_BlockStmt

struct Fa_ExprStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_expr { nullptr };

public:
    Fa_ExprStmt() = delete;

    explicit Fa_ExprStmt(Fa_Expr* m_expr)
        : m_expr(m_expr)
    {
        assert(m_expr != nullptr);
        kind = Kind::EXPR;
    }

    Fa_ExprStmt(Fa_ExprStmt&&) noexcept = delete;
    Fa_ExprStmt(Fa_ExprStmt const&) noexcept = delete;

    Fa_ExprStmt& operator=(Fa_ExprStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ExprStmt* clone() const override;
    [[nodiscard]] Fa_Expr* get_expr() const;
    void set_expr(Fa_Expr* e);
}; // struct Fa_ExprStmt

struct Fa_AssignmentStmt final : public Fa_Stmt {
private:
    Fa_AssignmentExpr* m_expr;

public:
    Fa_AssignmentStmt() = delete;

    explicit Fa_AssignmentStmt(Fa_AssignmentExpr* e)
        : m_expr(e->clone())
    {
        assert(m_expr != nullptr);
        kind = Kind::ASSIGNMENT;
    }

    Fa_AssignmentStmt(Fa_Expr* m_target, Fa_Expr* m_value, bool decl = false)
    {
        m_expr = Fa_makeAssignmentExpr(m_target, m_value, decl); // Fa_AssignmentExpr will assert args for us
        kind = Kind::ASSIGNMENT;
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
    
    Fa_AssignmentExpr* get_expr() { return m_expr; }
}; // struct Fa_AssignmentExpr

struct Fa_IfStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_condition { nullptr };
    Fa_Stmt* m_then_stmt { nullptr };
    Fa_Stmt* m_else_stmt { nullptr };

public:
    Fa_IfStmt() = delete;

    Fa_IfStmt(Fa_Expr* m_condition, Fa_Stmt* m_then_stmt, Fa_Stmt* m_else_stmt)
        : m_condition(m_condition)
        , m_then_stmt(m_then_stmt)
        , m_else_stmt(m_else_stmt)
    {
        assert(m_condition != nullptr);
        assert(m_then_stmt != nullptr);
        kind = Kind::IF;
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
}; // struct Fa_IfStmt

struct Fa_WhileStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_condition { nullptr };
    Fa_Stmt* m_body { nullptr };

public:
    Fa_WhileStmt() = delete;

    Fa_WhileStmt(Fa_Expr* m_condition, Fa_Stmt* m_body)
        : m_condition(m_condition)
        , m_body(m_body)
    {
        assert(m_condition != nullptr);
        assert(m_body != nullptr);
        kind = Kind::WHILE;
    }

    Fa_WhileStmt(Fa_WhileStmt&&) noexcept = delete;
    Fa_WhileStmt(Fa_WhileStmt const&) noexcept = delete;

    Fa_WhileStmt& operator=(Fa_WhileStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_WhileStmt* clone() const override;
    [[nodiscard]] Fa_Expr* get_condition() const;
    [[nodiscard]] Fa_Stmt* get_body();
    [[nodiscard]] Fa_Stmt const* get_body() const;
    void set_body(Fa_Stmt* b);
}; // struct Fa_WhileStmt

struct Fa_ForStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_container { nullptr };
    Fa_Expr* m_iter { nullptr };
    Fa_Stmt* m_body { nullptr };

public:
    Fa_ForStmt() = delete;

    Fa_ForStmt(Fa_Expr* m_target, Fa_Expr* m_iter, Fa_Stmt* m_body)
        : m_container(m_target)
        , m_iter(m_iter)
        , m_body(m_body)
    {
        assert(m_container != nullptr);
        assert(m_iter != nullptr);
        assert(m_body != nullptr);
        kind = Kind::FOR;
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
    void set_body(Fa_Stmt* b);
}; // struct Fa_ForStmt

struct Fa_FunctionDef final : public Fa_Stmt {
private:
    Fa_NameExpr* m_name { nullptr };
    Fa_ListExpr* m_params { nullptr };
    Fa_Stmt* m_body { nullptr };

public:
    Fa_FunctionDef() = delete;

    Fa_FunctionDef(Fa_NameExpr* m_name, Fa_ListExpr* m_params, Fa_Stmt* m_body)
        : m_name(m_name)
        , m_params(m_params)
        , m_body(m_body)
    {
        assert(m_name != nullptr);
        assert(m_params != nullptr);
        assert(m_body != nullptr);
        kind = Kind::FUNC;
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
}; // struct Fa_FunctionDef

struct Fa_ReturnStmt final : public Fa_Stmt {
private:
    Fa_Expr* m_value { nullptr };

public:
    Fa_ReturnStmt() = delete;

    explicit Fa_ReturnStmt(Fa_Expr* m_value)
        : m_value(m_value)
    {
        kind = Kind::RETURN;
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
}; // struct Fa_ReturnStmt

struct Fa_BreakStmt final : public Fa_Stmt {
    Fa_BreakStmt() { kind = Kind::BREAK; }

    Fa_BreakStmt(Fa_BreakStmt&&) noexcept = delete;
    Fa_BreakStmt(Fa_BreakStmt const&) noexcept = delete;

    Fa_BreakStmt& operator=(Fa_BreakStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_BreakStmt* clone() const override;
}; // struct Fa_BreakStmt

struct Fa_ContinueStmt final : public Fa_Stmt {
    Fa_ContinueStmt() { kind = Kind::CONTINUE; }

    Fa_ContinueStmt(Fa_ContinueStmt&&) noexcept = delete;
    Fa_ContinueStmt(Fa_ContinueStmt const&) noexcept = delete;

    Fa_ContinueStmt& operator=(Fa_ContinueStmt const&) noexcept = delete;

    [[nodiscard]] bool equals(Fa_Stmt const* other) const override;
    [[nodiscard]] Fa_ContinueStmt* clone() const override;
}; // struct Fa_ContinueStmt

static Fa_BinaryExpr* Fa_makeBinary(Fa_Expr* l, Fa_Expr* r, Fa_BinaryOp const op)
{
    return get_allocator().allocate_object<Fa_BinaryExpr>(l, r, op);
}
static Fa_UnaryExpr* Fa_makeUnary(Fa_Expr* m_operand, Fa_UnaryOp const op)
{
    return get_allocator().allocate_object<Fa_UnaryExpr>(m_operand, op);
}
static Fa_LiteralExpr* Fa_makeLiteralNil()
{
    return get_allocator().allocate_object<Fa_LiteralExpr>();
}
static Fa_LiteralExpr* Fa_makeLiteralInt(int m_value)
{
    return get_allocator().allocate_object<Fa_LiteralExpr>(static_cast<i64>(m_value), Fa_LiteralExpr::Type::INTEGER);
}
static Fa_LiteralExpr* Fa_makeLiteralInt(i64 m_value)
{
    return get_allocator().allocate_object<Fa_LiteralExpr>(m_value, Fa_LiteralExpr::Type::INTEGER);
}
static Fa_LiteralExpr* Fa_makeLiteralFloat(f64 m_value)
{
    return get_allocator().allocate_object<Fa_LiteralExpr>(m_value, Fa_LiteralExpr::Type::FLOAT);
}
static Fa_LiteralExpr* Fa_makeLiteralString(Fa_StringRef m_value)
{
    return get_allocator().allocate_object<Fa_LiteralExpr>(m_value);
}
static Fa_LiteralExpr* Fa_makeLiteralBool(bool m_value)
{
    return get_allocator().allocate_object<Fa_LiteralExpr>(m_value);
}
static Fa_NameExpr* Fa_makeName(Fa_StringRef const str)
{
    return get_allocator().allocate_object<Fa_NameExpr>(str);
}
static Fa_ListExpr* Fa_makeList(Fa_Array<Fa_Expr*> m_elements)
{
    return get_allocator().allocate_object<Fa_ListExpr>(m_elements);
}
static Fa_DictExpr* Fa_makeDict(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> content)
{
    return get_allocator().allocate_object<Fa_DictExpr>(content);
}
static Fa_CallExpr* Fa_makeCall(Fa_Expr* m_callee, Fa_ListExpr* m_args)
{
    return get_allocator().allocate_object<Fa_CallExpr>(m_callee, m_args);
}
static Fa_AssignmentExpr* Fa_makeAssignmentExpr(Fa_Expr* m_target, Fa_Expr* m_value, bool decl)
{
    return get_allocator().allocate_object<Fa_AssignmentExpr>(m_target, m_value, decl);
}
static Fa_IndexExpr* Fa_makeIndex(Fa_Expr* obj, Fa_Expr* idx)
{
    return get_allocator().allocate_object<Fa_IndexExpr>(obj, idx);
}
static Fa_BlockStmt* Fa_makeBlock(Fa_Array<Fa_Stmt*> stmts)
{
    return get_allocator().allocate_object<Fa_BlockStmt>(stmts);
}
static Fa_ExprStmt* Fa_makeExprStmt(Fa_Expr* m_expr)
{
    return get_allocator().allocate_object<Fa_ExprStmt>(m_expr);
}
static Fa_AssignmentStmt* Fa_makeAssignmentStmt(Fa_Expr* m_target, Fa_Expr* m_value, bool decl)
{
    return get_allocator().allocate_object<Fa_AssignmentStmt>(m_target, m_value, decl);
}
static Fa_IfStmt* Fa_makeIf(Fa_Expr* m_condition, Fa_Stmt* then_block, Fa_Stmt* else_block = nullptr)
{
    return get_allocator().allocate_object<Fa_IfStmt>(m_condition, then_block, else_block);
}
static Fa_WhileStmt* Fa_makeWhile(Fa_Expr* m_condition, Fa_Stmt* m_body)
{
    return get_allocator().allocate_object<Fa_WhileStmt>(m_condition, m_body);
}
static Fa_ForStmt* Fa_makeFor(Fa_NameExpr* m_target, Fa_Expr* m_iter, Fa_Stmt* m_body)
{
    return get_allocator().allocate_object<Fa_ForStmt>(m_target, m_iter, m_body);
}
static Fa_FunctionDef* Fa_makeFunction(Fa_NameExpr* m_name, Fa_ListExpr* m_params, Fa_Stmt* m_body)
{
    return get_allocator().allocate_object<Fa_FunctionDef>(m_name, m_params, m_body);
}
static Fa_ReturnStmt* Fa_makeReturn(Fa_Expr* m_value = nullptr)
{
    return get_allocator().allocate_object<Fa_ReturnStmt>(m_value);
}
static Fa_BreakStmt* Fa_makeBreak()
{
    return get_allocator().allocate_object<Fa_BreakStmt>();
}
static Fa_ContinueStmt* Fa_makeContinue()
{
    return get_allocator().allocate_object<Fa_ContinueStmt>();
}

} // namespace fairuz::ast

#endif // AST_HPP
