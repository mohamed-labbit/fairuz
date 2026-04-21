#ifndef AST_PRINTER_HPP
#define AST_PRINTER_HPP

#include "ast.hpp"

#include <iostream>

namespace fairuz::AST {

class ASTPrinter {
private:
    bool m_use_color;
    u32 m_node_count = 0;

    struct Prefix {
        std::string indent { "" };
        bool last { false };
    }; // struct Prefix

    Fa_StringRef const to_string(Fa_UnaryOp const op)
    {
        switch (op) {
        case Fa_UnaryOp::OP_PLUS:
            return "+";
        case Fa_UnaryOp::OP_NEG:
            return "-";
        case Fa_UnaryOp::OP_BITNOT:
            return "~";
        case Fa_UnaryOp::OP_NOT:
            return "ليس";
        default:
            return "";
        }
    }

    Fa_StringRef const to_string(Fa_BinaryOp const op)
    {
        switch (op) {
        case Fa_BinaryOp::OP_EQ:
            return "=";
        case Fa_BinaryOp::OP_ADD:
            return "+";
        case Fa_BinaryOp::OP_SUB:
            return "-";
        case Fa_BinaryOp::OP_MUL:
            return "*";
        case Fa_BinaryOp::OP_DIV:
            return "/";
        case Fa_BinaryOp::OP_MOD:
            return "%";
        case Fa_BinaryOp::OP_POW:
            return "**";
        case Fa_BinaryOp::OP_LT:
            return "<";
        case Fa_BinaryOp::OP_GT:
            return ">";
        case Fa_BinaryOp::OP_LTE:
            return "<=";
        case Fa_BinaryOp::OP_GTE:
            return ">=";
        case Fa_BinaryOp::OP_NEQ:
            return "!=";
        case Fa_BinaryOp::OP_BITAND:
            return "&";
        case Fa_BinaryOp::OP_BITOR:
            return "|";
        case Fa_BinaryOp::OP_BITXOR:
            return "^";
        case Fa_BinaryOp::OP_LSHIFT:
            return "<<";
        case Fa_BinaryOp::OP_RSHIFT:
            return ">>";
        default:
            return "";
        }
    }

    std::string glyph(bool const last) const { return last ? "└─ " : "├─ "; }
    std::string pipe(bool const last) const { return last ? "   " : "│  "; }

    std::string color(std::string const& s, std::string const& c) const
    {
        if (!m_use_color)
            return s;

        return c + s + Color::RESET;
    }

    void print_expr(Fa_Expr const* e, Prefix p)
    {
        if (e == nullptr)
            return;

        m_node_count += 1;

        std::cout << p.indent << glyph(p.last);

        switch (e->get_kind()) {
        case Fa_Expr::Kind::NAME: {
            auto n = static_cast<Fa_NameExpr const*>(e);
            std::cout << color("Name", Color::CYAN) << "(" << n->get_value() << ")\n";
            break;
        }

        case Fa_Expr::Kind::LITERAL: {
            auto l = static_cast<Fa_LiteralExpr const*>(e);
            if (l->is_numeric())
                std::cout << color("Literal", Color::GREEN) << "(" << l->is_number() << ")\n";
            else if (l->is_string())
                std::cout << color("Literal", Color::GREEN) << "(" << l->get_str() << ")\n";
            else if (l->is_bool())
                std::cout << color("Literal", Color::GREEN) << "(" << l->get_bool() << ")\n";
            break;
        }

        case Fa_Expr::Kind::UNARY: {
            auto u = static_cast<Fa_UnaryExpr const*>(e);
            std::cout << color("Unary", Color::BOLD) << " " << to_string(u->get_operator()) << "\n";
            print_expr(u->get_operand(), { p.indent + pipe(p.last), true });
            break;
        }

        case Fa_Expr::Kind::BINARY: {
            auto b = static_cast<Fa_BinaryExpr const*>(e);
            std::cout << color("Binary", Color::BOLD) << " " << to_string(b->get_operator()) << "\n";
            print_expr(b->get_left(), { p.indent + pipe(p.last), false });
            print_expr(b->get_right(), { p.indent + pipe(p.last), true });
            break;
        }

        case Fa_Expr::Kind::CALL: {
            auto c = static_cast<Fa_CallExpr const*>(e);
            std::cout << color("Call", Color::MAGENTA) << " (" << c->get_args().size() << " args)\n";
            std::cout << p.indent + pipe(p.last) << "├─ callee:\n";
            print_expr(c->get_callee(), { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ args:\n";
            for (size_t i = 0; i < c->get_args().size(); i += 1)
                print_expr(c->get_args()[i], { p.indent + pipe(p.last) + "   ", i + 1 == c->get_args().size() });
            break;
        }

        case Fa_Expr::Kind::LIST: {
            auto l = static_cast<Fa_ListExpr const*>(e);
            std::cout << color("List", Color::BLUE) << " [" << l->get_elements().size() << "]\n";
            for (size_t i = 0; i < l->get_elements().size(); i += 1)
                print_expr(l->get_elements()[i], { p.indent + pipe(p.last), i + 1 == l->get_elements().size() });
            break;
        }

        case Fa_Expr::Kind::ASSIGNMENT: {
            auto a = static_cast<Fa_AssignmentExpr const*>(e);
            std::cout << color("Assignment", Color::YELLOW) << " :=\n";
            std::cout << p.indent + pipe(p.last) << "├─ target:\n";
            print_expr(a->get_target(), { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ value:\n";
            print_expr(a->get_value(), { p.indent + pipe(p.last) + "   ", true });
            break;
        }

        default:
            std::cout << color("<unknown expr>", Color::RED) << "\n";
        }
    }

    void print_stmt(Fa_Stmt const* s, Prefix p)
    {
        if (s == nullptr)
            return;

        m_node_count += 1;

        std::cout << p.indent << glyph(p.last);

        switch (s->get_kind()) {
        case Fa_Stmt::Kind::FUNC: {
            auto f = static_cast<Fa_FunctionDef const*>(s);
            std::cout << color("Fa_FunctionDef", Color::BOLD) << " " << f->get_name()->get_value() << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ params:\n";
            for (size_t i = 0; i < f->get_parameters().size(); i += 1)
                print_expr(f->get_parameters()[i], { p.indent + pipe(p.last) + "│  ", i + 1 == f->get_parameters().size() });
            std::cout << p.indent + pipe(p.last) << "└─ body:\n";
            print_stmt(f->get_body(), { p.indent + pipe(p.last) + "    ", true });
            break;
        }

        case Fa_Stmt::Kind::RETURN: {
            auto r = static_cast<Fa_ReturnStmt const*>(s);
            std::cout << color("Return", Color::BOLD) << "\n";
            print_expr(r->get_value(), { p.indent + pipe(p.last), true });
            break;
        }

        case Fa_Stmt::Kind::EXPR: {
            auto e = static_cast<Fa_ExprStmt const*>(s);
            std::cout << color("Fa_ExprStmt", Color::BOLD) << "\n";
            print_expr(e->get_expr(), { p.indent + pipe(p.last), true });
            break;
        }

        case Fa_Stmt::Kind::WHILE: {
            auto w = static_cast<Fa_WhileStmt const*>(s);
            std::cout << color("While", Color::BOLD) << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ condition:\n";
            print_expr(w->get_condition(), { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ body:\n";
            print_stmt(w->get_body(), { p.indent + pipe(p.last) + "   ", true });
            break;
        }
        case Fa_Stmt::Kind::IF: {
            auto i = static_cast<Fa_IfStmt const*>(s);
            std::cout << color("If", Color::BOLD) << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ condition:\n";
            print_expr(i->get_condition(), { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << (i->get_else() ? "├─" : "└─") << " then:\n";
            print_stmt(i->get_then(), { p.indent + pipe(p.last) + (i->get_else() ? "│  " : "   "), true });
            if (i->get_else() != nullptr) {
                std::cout << p.indent + pipe(p.last) << "└─ else:\n";
                print_stmt(i->get_else(), { p.indent + pipe(p.last) + "   ", true });
            }
            break;
        }

        case Fa_Stmt::Kind::BLOCK: {
            auto b = static_cast<Fa_BlockStmt const*>(s);
            std::cout << color("Block", Color::BOLD) << " {" << b->get_statements().size() << " stmts}\n";
            for (size_t i = 0; i < b->get_statements().size(); i += 1)
                print_stmt(b->get_statements()[i], { p.indent + pipe(p.last), i + 1 == b->get_statements().size() });
            break;
        }
            /*
             *
                    case Fa_Stmt::Kind::ASSIGNMENT: {
                        auto a = static_cast<Fa_AssignmentStmt const*>(s);
                        std::cout << color("Assignment", Color::YELLOW) << " :=\n";
                        std::cout << p.indent + pipe(p.last) << "├─ target:\n";
                        print_expr(a->get_target(), { p.indent + pipe(p.last) + "│  ", true });
                        std::cout << p.indent + pipe(p.last) << "└─ value:\n";
                        print_expr(a->get_value(), { p.indent + pipe(p.last) + "   ", true });
                        break;
                    }
             */

        default:
            std::cout << color("<unknown stmt>", Color::RED) << "\n";
        }
    }

public:
    explicit ASTPrinter(bool const color = true)
        : m_use_color(color)
    {
    }

    void print(Fa_Expr const* e) { print_expr(e, { "", true }); }
    void print(Fa_Stmt const* s) { print_stmt(s, { "", true }); }

    u32 get_node_count() const { return m_node_count; }
}; // class ASTPrinter

} // namespace fairuz::ast

#endif // AST_PRINTER_HPP
