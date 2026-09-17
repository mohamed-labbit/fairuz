#ifndef FA_AST_PRINTER_HPP
#define FA_AST_PRINTER_HPP

#include "fAST.hpp"
#include "fmacros.hpp"

#include <iostream>

namespace fairuz::AST {

using ExprKind = AST::Expr::Kind;

class ASTPrinter {
private:
    bool m_use_color;
    u32 m_node_count = 0;

    struct Prefix {
        std::string indent { "" };
        bool last { false };
    }; // struct Prefix

    StringRef const to_string(ExprKind const op)
    {
        switch (op) {
        case ExprKind::OP_PLUS: return "+";
        case ExprKind::OP_NEG: return "-";
        case ExprKind::OP_BITNOT: return "~";
        case ExprKind::OP_NOT: return "ليس";
        case ExprKind::OP_EQ: return "=";
        case ExprKind::OP_ADD: return "+";
        case ExprKind::OP_SUB: return "-";
        case ExprKind::OP_MUL: return "*";
        case ExprKind::OP_DIV: return "/";
        case ExprKind::OP_MOD: return "%";
        case ExprKind::OP_POW: return "**";
        case ExprKind::OP_LT: return "<";
        case ExprKind::OP_GT: return ">";
        case ExprKind::OP_LTE: return "<=";
        case ExprKind::OP_GTE: return ">=";
        case ExprKind::OP_NEQ: return "!=";
        case ExprKind::OP_BITAND: return "&";
        case ExprKind::OP_BITOR: return "|";
        case ExprKind::OP_BITXOR: return "^";
        case ExprKind::OP_LSHIFT: return "<<";
        case ExprKind::OP_RSHIFT: return ">>";
        case ExprKind::OP_AND: return "و"; // logical and
        case ExprKind::OP_OR: return "أو"; // logical or
        default: return "";
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

    void print_expr(Expr const* e, Prefix p)
    {
        if (e == nullptr)
            return;

        m_node_count++;

        std::cout << p.indent << glyph(p.last);

        switch (e->get_kind()) {
        case ExprKind::IDENTIFIER: {
            auto n = as_identifier(e);
            std::cout << color("Name", Color::CYAN) << "(" << n->spelling << ")\n";
            break;
        }

        case ExprKind::INT_LITERAL: {
            std::cout << color("Int Literal", Color::GREEN) << "(" << as_literal_int(e)->value << ")\n";
            break;
        }
        case ExprKind::FLOAT_LITERAL: {
            std::cout << color("Float Literal", Color::GREEN) << "(" << as_literal_float(e)->value << ")\n";
            break;
        }
        case ExprKind::BOOL_LITERAL: {
            std::cout << color("Bool Literal", Color::GREEN) << "(" << (as_literal_bool(e)->value ? "صحيح" : "خطا") << ")\n";
            break;
        }
        case ExprKind::STRING_LITERAL: {
            std::cout << color("String Literal", Color::GREEN) << "(" << as_literal_string(e)->str << ")\n";
            break;
        }
        case ExprKind::NIL: {
            std::cout << color("Nil Literal", Color::GREEN) << "(عدم)\n";
            break;
        }

        case ExprKind::OP_PLUS:
        case ExprKind::OP_NEG:
        case ExprKind::OP_BITNOT:
        case ExprKind::OP_NOT: {
            auto u = static_cast<UnaryExpr const*>(e);
            std::cout << color("Unary", Color::BOLD) << " " << to_string(u->get_kind()) << "\n";
            print_expr(u->operand, { p.indent + pipe(p.last), true });
            break;
        }

        case ExprKind::OP_ADD:
        case ExprKind::OP_SUB:
        case ExprKind::OP_MUL:
        case ExprKind::OP_DIV:
        case ExprKind::OP_MOD:
        case ExprKind::OP_POW:
        case ExprKind::OP_EQ:
        case ExprKind::OP_NEQ:
        case ExprKind::OP_LT:
        case ExprKind::OP_GT:
        case ExprKind::OP_LTE:
        case ExprKind::OP_GTE:
        case ExprKind::OP_BITAND:
        case ExprKind::OP_BITOR:
        case ExprKind::OP_BITXOR:
        case ExprKind::OP_LSHIFT:
        case ExprKind::OP_RSHIFT:
        case ExprKind::OP_AND:
        case ExprKind::OP_OR: {
            auto b = as_binary(e);
            std::cout << color("Binary", Color::BOLD) << " " << to_string(b->get_kind()) << "\n";
            print_expr(b->lhs, { p.indent + pipe(p.last), false });
            print_expr(b->rhs, { p.indent + pipe(p.last), true });
            break;
        }

        case Expr::Kind::CALL: {
            auto c = static_cast<CallExpr const*>(e);
            std::cout << color("Call", Color::MAGENTA) << " (" << c->args->size() << " args)\n";
            std::cout << p.indent + pipe(p.last) << "├─ callee:\n";
            print_expr(c->callee, { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ args:\n";
            for (size_t i = 0; i < c->args->size(); i++)
                print_expr(c->args->elements[i], { p.indent + pipe(p.last) + "   ", i + 1 == c->args->size() });
            break;
        }

        case Expr::Kind::LIST: {
            auto l = as_list(e);
            std::cout << color("List", Color::BLUE) << " [" << l->elements.size() << "]\n";
            for (size_t i = 0; i < l->elements.size(); i++)
                print_expr(l->elements[i], { p.indent + pipe(p.last), i + 1 == l->elements.size() });
            break;
        }

        case Expr::Kind::ASSIGNMENT: {
            auto a = static_cast<AssignExpr const*>(e);
            std::cout << color("Assignment", Color::YELLOW) << " :=\n";
            std::cout << p.indent + pipe(p.last) << "├─ target:\n";
            print_expr(a->target, { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ value:\n";
            print_expr(a->value, { p.indent + pipe(p.last) + "   ", true });
            break;
        }

        case Expr::Kind::INDEX_READ: {
            auto ix = static_cast<IndexExpr const*>(e);
            std::cout << color("Index", Color::MAGENTA) << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ object:\n";
            print_expr(ix->object, { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ index:\n";
            print_expr(ix->index, { p.indent + pipe(p.last) + "   ", true });
            break;
        }

        case Expr::Kind::DICT: {
            auto d = as_dict(e);
            auto content = d->get_content();
            std::cout << color("Dict", Color::BLUE) << " {" << content.size() << "}\n";
            for (size_t i = 0; i < content.size(); i++) {
                bool const last_pair = i + 1 == content.size();
                std::cout << p.indent + pipe(p.last) << glyph(last_pair) << "pair:\n";
                std::string const inner = p.indent + pipe(p.last) + pipe(last_pair);
                print_expr(content[i].first, { inner, false });
                print_expr(content[i].second, { inner, true });
            }
            break;
        }

        case Expr::Kind::GET: {
            auto g = as_get(e);
            std::cout << color("Get", Color::MAGENTA) << " .\n";
            std::cout << p.indent + pipe(p.last) << "├─ object:\n";
            print_expr(g->object, { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ member:\n";
            print_expr(g->member, { p.indent + pipe(p.last) + "   ", true });
            break;
        }

        default:
            std::cout << color("<unknown expr>", Color::RED) << "\n";
        }
    }

    void print_stmt(Stmt const* s, Prefix p)
    {
        if (s == nullptr)
            return;

        m_node_count++;

        std::cout << p.indent << glyph(p.last);

        switch (s->get_kind()) {
        case Stmt::Kind::FUNC: {
            auto f = static_cast<FunctionDef const*>(s);
            std::cout << color("FunctionDef", Color::BOLD) << " " << f->name->spelling << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ params:\n";
            for (size_t i = 0; i < f->params->size(); i++)
                print_expr(f->params->elements[i], { p.indent + pipe(p.last) + "│  ", i + 1 == f->params->size() });
            std::cout << p.indent + pipe(p.last) << "└─ body:\n";
            print_stmt(f->body, { p.indent + pipe(p.last) + "    ", true });
        } break;

        case Stmt::Kind::RETURN: {
            auto r = static_cast<ReturnStmt const*>(s);
            std::cout << color("Return", Color::BOLD) << "\n";
            print_expr(r->value, { p.indent + pipe(p.last), true });
        } break;

        case Stmt::Kind::EXPR: {
            auto e = static_cast<ExprStmt const*>(s);
            std::cout << color("ExprStmt", Color::BOLD) << "\n";
            print_expr(e->expr, { p.indent + pipe(p.last), true });
        } break;

        case Stmt::Kind::WHILE: {
            auto w = static_cast<WhileStmt const*>(s);
            std::cout << color("While", Color::BOLD) << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ condition:\n";
            print_expr(w->condition, { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ body:\n";
            print_stmt(w->body, { p.indent + pipe(p.last) + "   ", true });
        } break;

        case Stmt::Kind::IF: {
            auto i = static_cast<IfElseStmt const*>(s);
            std::cout << color("If", Color::BOLD) << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ condition:\n";
            print_expr(i->condition, { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << (i->else_stmt ? "├─" : "└─") << " then:\n";
            print_stmt(i->then_stmt, { p.indent + pipe(p.last) + (i->else_stmt ? "│  " : "   "), true });
            if (i->else_stmt != nullptr) {
                std::cout << p.indent + pipe(p.last) << "└─ else:\n";
                print_stmt(i->else_stmt, { p.indent + pipe(p.last) + "   ", true });
            }
        } break;

        case Stmt::Kind::BLOCK: {
            auto b = static_cast<BlockStmt const*>(s);
            std::cout << color("Block", Color::BOLD) << " {" << b->stmts.size() << " stmts}\n";
            for (size_t i = 0; i < b->stmts.size(); i++)
                print_stmt(b->stmts[i], { p.indent + pipe(p.last), i + 1 == b->stmts.size() });
        } break;

        case Stmt::Kind::FOR: {
            auto f = as_for(s);
            std::cout << color("For", Color::BOLD) << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ container:\n";
            print_expr(f->container, { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "├─ iter:\n";
            print_expr(f->iter, { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ body:\n";
            print_stmt(f->body, { p.indent + pipe(p.last) + "   ", true });
        } break;

        case Stmt::Kind::BREAK: std::cout << color("Break", Color::BOLD) << "\n"; break;
        case Stmt::Kind::CONTINUE: std::cout << color("Continue", Color::BOLD) << "\n"; break;

        case Stmt::Kind::CLASS_DEF: {
            auto c = as_class_def(s);
            auto members = c->get_members();
            auto methods = c->get_methods();
            std::cout << color("ClassDef", Color::BOLD) << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ name:\n";
            print_expr(c->get_name(), { p.indent + pipe(p.last) + "│  ", true });

            bool const has_methods = !methods.empty();
            std::cout << p.indent + pipe(p.last) << (has_methods ? "├─" : "└─") << " members:\n";
            std::string const members_prefix = p.indent + pipe(p.last) + (has_methods ? "│  " : "   ");
            for (size_t i = 0; i < members.size(); i++)
                print_expr(members[i], { members_prefix, i + 1 == members.size() });

            if (has_methods) {
                std::cout << p.indent + pipe(p.last) << "└─ methods:\n";
                for (size_t i = 0; i < methods.size(); i++)
                    print_stmt(methods[i], { p.indent + pipe(p.last) + "   ", i + 1 == methods.size() });
            }
        } break;

        case Stmt::Kind::IMPORT: {
            auto import = as_import(s);
            std::cout << color("Import", Color::BOLD) << " " << import->get_module();
            if (import->imports_member())
                std::cout << "." << import->get_names()[0];
            std::cout << " as " << import->get_aliases()[0] << "\n";
        } break;

        default: std::cout << color("<unknown stmt>", Color::RED) << "\n";
        }
    }

public:
    explicit ASTPrinter(bool const color = true)
        : m_use_color(color)
    {
    }

    void print(Expr const* e) { print_expr(e, { "", true }); }
    void print(Stmt const* s) { print_stmt(s, { "", true }); }

    u32 get_node_count() const { return m_node_count; }
}; // class ASTPrinter

} // namespace fairuz::ast

#endif // FA_AST_PRINTER_HPP
