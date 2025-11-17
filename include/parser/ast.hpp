#pragma once


#include <memory>
#include <string>
#include <vector>
#include <iostream>

#include "util.hpp"
#include "../../utfcpp/source/utf8.h"


namespace mylang {
namespace parser {
namespace ast {

// Forward declarations
struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// Base classes
struct ASTNode
{
    std::uint64_t line, column;
    virtual ~ASTNode() = default;
};

struct Expr: ASTNode
{
    enum class Kind { BINARY, UNARY, LITERAL, NAME, CALL, TERNARY, ASSIGNMENT, LIST, TUPLE };
    Kind kind;
};

struct Stmt: ASTNode
{
    enum class Kind { EXPRESSION, ASSIGNMENT, IF, WHILE, FOR, FUNCTION_DEF, RETURN, BLOCK };
    Kind kind;
};

// Expression nodes
struct BinaryExpr: Expr
{
    ExprPtr left, right;
    std::u16string op;

    BinaryExpr(ExprPtr l, std::u16string o, ExprPtr r) :
        left(std::move(l)),
        op(std::move(o)),
        right(std::move(r))
    {
        kind = Kind::BINARY;
    }
};

struct UnaryExpr: Expr
{
    std::u16string op;
    ExprPtr operand;

    UnaryExpr(std::u16string o, ExprPtr expr) :
        op(std::move(o)),
        operand(std::move(expr))
    {
        kind = Kind::UNARY;
    }
};

struct LiteralExpr: Expr
{
    enum class Type { NUMBER, STRING, BOOLEAN, NONE };
    Type litType;
    std::u16string value;

    LiteralExpr(Type t, std::u16string v) :
        litType(t),
        value(std::move(v))
    {
        kind = Kind::LITERAL;
    }
};

struct NameExpr: Expr
{
    std::u16string name;

    explicit NameExpr(std::u16string n) :
        name(std::move(n))
    {
        kind = Kind::NAME;
    }
};

struct CallExpr: Expr
{
    ExprPtr callee;
    std::vector<ExprPtr> args;

    CallExpr(ExprPtr c, std::vector<ExprPtr> a) :
        callee(std::move(c)),
        args(std::move(a))
    {
        kind = Kind::CALL;
    }
};

struct TernaryExpr: Expr
{
    ExprPtr condition, trueExpr, falseExpr;

    TernaryExpr(ExprPtr cond, ExprPtr t, ExprPtr f) :
        condition(std::move(cond)),
        trueExpr(std::move(t)),
        falseExpr(std::move(f))
    {
        kind = Kind::TERNARY;
    }
};

struct AssignmentExpr: Expr
{
    std::u16string target;
    ExprPtr value;

    AssignmentExpr(std::u16string t, ExprPtr v) :
        target(std::move(t)),
        value(std::move(v))
    {
        kind = Kind::ASSIGNMENT;
    }
};

struct ListExpr: Expr
{
    std::vector<ExprPtr> elements;

    explicit ListExpr(std::vector<ExprPtr> elems) :
        elements(std::move(elems))
    {
        kind = Kind::LIST;
    }
};

// Statement nodes
struct ExprStmt: Stmt
{
    ExprPtr expression;

    explicit ExprStmt(ExprPtr e) :
        expression(std::move(e))
    {
        kind = Kind::EXPRESSION;
    }
};

struct AssignmentStmt: Stmt
{
    std::u16string target;
    ExprPtr value;

    AssignmentStmt(std::u16string t, ExprPtr v) :
        target(std::move(t)),
        value(std::move(v))
    {
        kind = Kind::ASSIGNMENT;
    }
};

struct IfStmt: Stmt
{
    ExprPtr condition;
    std::vector<StmtPtr> thenBlock;
    std::vector<StmtPtr> elseBlock;

    IfStmt(ExprPtr cond, std::vector<StmtPtr> tb, std::vector<StmtPtr> eb) :
        condition(std::move(cond)),
        thenBlock(std::move(tb)),
        elseBlock(std::move(eb))
    {
        kind = Kind::IF;
    }
};

struct WhileStmt: Stmt
{
    ExprPtr condition;
    std::vector<StmtPtr> body;

    WhileStmt(ExprPtr cond, std::vector<StmtPtr> b) :
        condition(std::move(cond)),
        body(std::move(b))
    {
        kind = Kind::WHILE;
    }
};

struct ForStmt: Stmt
{
    std::u16string target;
    ExprPtr iter;
    std::vector<StmtPtr> body;

    ForStmt(std::u16string t, ExprPtr i, std::vector<StmtPtr> b) :
        target(std::move(t)),
        iter(std::move(i)),
        body(std::move(b))
    {
        kind = Kind::FOR;
    }
};

struct FunctionDef: Stmt
{
    std::u16string name;
    std::vector<std::u16string> params;
    std::vector<StmtPtr> body;

    FunctionDef(std::u16string n, std::vector<std::u16string> p, std::vector<StmtPtr> b) :
        name(std::move(n)),
        params(std::move(p)),
        body(std::move(b))
    {
        kind = Kind::FUNCTION_DEF;
    }
};

struct ReturnStmt: Stmt
{
    ExprPtr value;

    explicit ReturnStmt(ExprPtr v) :
        value(std::move(v))
    {
        kind = Kind::RETURN;
    }
};

struct BlockStmt: Stmt
{
    std::vector<StmtPtr> statements;

    explicit BlockStmt(std::vector<StmtPtr> stmts) :
        statements(std::move(stmts))
    {
        kind = Kind::BLOCK;
    }
};


// Advanced AST Printer with stats
class EnhancedASTPrinter
{
   private:
    int indent = 0;
    bool useColor;
    int nodeCount = 0;

    std::string colorize(const std::u16string& text, const std::u16string color, bool enabled = true)
    {
        if (!enabled)
        {
            return utf8::utf16to8(text);
        }
        return utf8::utf16to8(color + text + Color::RESET);
    }

    void printIndent()
    {
        for (int i = 0; i < indent; i++)
        {
            std::cout << (useColor ? "│ " : "| ");
        }
    }

   public:
    explicit EnhancedASTPrinter(bool color = true) :
        useColor(color)
    {
    }

    void print(const mylang::parser::ast::Expr* expr)
    {
        if (!expr)
        {
            return;
        }
        nodeCount++;

        switch (expr->kind)
        {
        case mylang::parser::ast::Expr::Kind::BINARY : {
            auto* e = static_cast<const mylang::parser::ast::BinaryExpr*>(expr);
            printIndent();
            std::cout << colorize(u"BinaryOp", Color::BOLD, useColor) << " "
                      << colorize(e->op, Color::YELLOW, useColor) << "\n";
            indent++;
            print(e->left.get());
            print(e->right.get());
            indent--;
            break;
        }
        case mylang::parser::ast::Expr::Kind::UNARY : {
            auto* e = static_cast<const mylang::parser::ast::UnaryExpr*>(expr);
            printIndent();
            std::cout << colorize(u"UnaryOp", Color::BOLD, useColor) << " " << colorize(e->op, Color::YELLOW, useColor)
                      << "\n";
            indent++;
            print(e->operand.get());
            indent--;
            break;
        }
        case mylang::parser::ast::Expr::Kind::LITERAL : {
            auto* e = static_cast<const mylang::parser::ast::LiteralExpr*>(expr);
            printIndent();
            std::cout << colorize(u"Literal", Color::GREEN, useColor) << ": " << utf8::utf16to8(e->value) << "\n";
            break;
        }
        case mylang::parser::ast::Expr::Kind::NAME : {
            auto* e = static_cast<const mylang::parser::ast::NameExpr*>(expr);
            printIndent();
            std::cout << colorize(u"Var", Color::CYAN, useColor) << ": " << utf8::utf16to8(e->name) << "\n";
            break;
        }
        case mylang::parser::ast::Expr::Kind::CALL : {
            auto* e = static_cast<const mylang::parser::ast::CallExpr*>(expr);
            printIndent();
            std::cout << colorize(u"Call", Color::MAGENTA, useColor) << " [" << e->args.size() << " args]\n";
            indent++;
            print(e->callee.get());
            for (const auto& arg : e->args)
            {
                print(arg.get());
            }
            indent--;
            break;
        }
        case mylang::parser::ast::Expr::Kind::LIST : {
            auto* e = static_cast<const mylang::parser::ast::ListExpr*>(expr);
            printIndent();
            std::cout << colorize(u"List", Color::BLUE, useColor) << " [" << e->elements.size() << "]\n";
            indent++;
            for (const auto& elem : e->elements)
            {
                print(elem.get());
            }
            indent--;
            break;
        }
        default :
            break;
        }
    }

    void print(const mylang::parser::ast::Stmt* stmt)
    {
        if (!stmt)
        {
            return;
        }
        nodeCount++;

        switch (stmt->kind)
        {
        case mylang::parser::ast::Stmt::Kind::ASSIGNMENT : {
            auto* s = static_cast<const mylang::parser::ast::AssignmentStmt*>(stmt);
            printIndent();
            std::cout << colorize(u"Assign", Color::BOLD, useColor) << " " << colorize(s->target, Color::CYAN, useColor)
                      << "\n";
            indent++;
            print(s->value.get());
            indent--;
            break;
        }
        case mylang::parser::ast::Stmt::Kind::IF : {
            auto* s = static_cast<const mylang::parser::ast::IfStmt*>(stmt);
            printIndent();
            std::cout << colorize(u"If", Color::BOLD, useColor) << "\n";
            indent++;
            print(s->condition.get());
            for (const auto& st : s->thenBlock)
            {
                print(st.get());
            }
            indent--;
            break;
        }
        case mylang::parser::ast::Stmt::Kind::WHILE : {
            auto* s = static_cast<const mylang::parser::ast::WhileStmt*>(stmt);
            printIndent();
            std::cout << colorize(u"While", Color::BOLD, useColor) << "\n";
            indent++;
            print(s->condition.get());
            for (const auto& st : s->body)
            {
                print(st.get());
            }
            indent--;
            break;
        }
        case mylang::parser::ast::Stmt::Kind::FOR : {
            auto* s = static_cast<const mylang::parser::ast::ForStmt*>(stmt);
            printIndent();
            std::cout << colorize(u"For", Color::BOLD, useColor) << " " << utf8::utf16to8(s->target) << "\n";
            indent++;
            print(s->iter.get());
            for (const auto& st : s->body)
            {
                print(st.get());
            }
            indent--;
            break;
        }
        case mylang::parser::ast::Stmt::Kind::FUNCTION_DEF : {
            auto* s = static_cast<const mylang::parser::ast::FunctionDef*>(stmt);
            printIndent();
            std::cout << colorize(u"Function", Color::BOLD, useColor) << " "
                      << colorize(s->name, Color::GREEN, useColor) << "(";
            for (size_t i = 0; i < s->params.size(); i++)
            {
                std::cout << utf8::utf16to8(s->params[i]);
                if (i + 1 < s->params.size())
                {
                    std::cout << ", ";
                }
            }
            std::cout << ")\n";
            indent++;
            for (const auto& st : s->body)
            {
                print(st.get());
            }
            indent--;
            break;
        }
        case mylang::parser::ast::Stmt::Kind::RETURN : {
            auto* s = static_cast<const mylang::parser::ast::ReturnStmt*>(stmt);
            printIndent();
            std::cout << colorize(u"Return", Color::BOLD, useColor) << "\n";
            indent++;
            print(s->value.get());
            indent--;
            break;
        }
        case mylang::parser::ast::Stmt::Kind::EXPRESSION : {
            auto* s = static_cast<const mylang::parser::ast::ExprStmt*>(stmt);
            print(s->expression.get());
            break;
        }
        default :
            break;
        }
    }

    int getNodeCount() const { return nodeCount; }
};

}
}
}