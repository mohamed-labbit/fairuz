#ifndef AST_PRINTER_HPP
#define AST_PRINTER_HPP

#include "ast.hpp"

#include <iostream>

namespace mylang {
namespace ast {

class ASTPrinter {
private:
    bool UseColor_;
    std::uint32_t NodeCount_ = 0;

    struct Prefix {
        std::string indent;
        bool last;
    };

    StringRef const toString(UnaryOp const op)
    {
        switch (op) {
        case UnaryOp::OP_PLUS:
            return "+";
        case UnaryOp::OP_NEG:
            return "-";
        case UnaryOp::OP_BITNOT:
            return "~";
        case UnaryOp::OP_NOT:
            return "ليس";
        default:
            return "";
        }
    }

    StringRef const toString(BinaryOp const op)
    {
        switch (op) {
        case BinaryOp::OP_EQ:
            return "=";
        case BinaryOp::OP_ADD:
            return "+";
        case BinaryOp::OP_SUB:
            return "-";
        case BinaryOp::OP_MUL:
            return "*";
        case BinaryOp::OP_DIV:
            return "/";
        case BinaryOp::OP_MOD:
            return "%";
        case BinaryOp::OP_POW:
            return "**";
        case BinaryOp::OP_LT:
            return "<";
        case BinaryOp::OP_GT:
            return ">";
        case BinaryOp::OP_LTE:
            return "<=";
        case BinaryOp::OP_GTE:
            return ">=";
        case BinaryOp::OP_NEQ:
            return "!=";
        case BinaryOp::OP_BITAND:
            return "&";
        case BinaryOp::OP_BITOR:
            return "|";
        case BinaryOp::OP_BITXOR:
            return "^";
        case BinaryOp::OP_LSHIFT:
            return "<<";
        case BinaryOp::OP_RSHIFT:
            return ">>";
        default:
            return "";
        }
    }

    std::string glyph(bool const last) const { return last ? "└─ " : "├─ "; }
    std::string pipe(bool const last) const { return last ? "   " : "│  "; }

    std::string color(std::string const& s, std::string const& c) const
    {
        if (!UseColor_)
            return s;

        return c + s + Color::RESET;
    }

    void printExpr(Expr const* e, Prefix p)
    {
        if (!e)
            return;

        NodeCount_++;

        std::cout << p.indent << glyph(p.last);

        switch (e->getKind()) {
        case Expr::Kind::NAME: {
            NameExpr const* n = static_cast<NameExpr const*>(e);
            std::cout << color("Name", Color::CYAN) << "(" << n->getValue() << ")\n";
            break;
        }

        case Expr::Kind::LITERAL: {
            LiteralExpr const* l = static_cast<LiteralExpr const*>(e);
            if (l->isNumeric())
                std::cout << color("Literal", Color::GREEN) << "(" << l->toNumber() << ")\n";
            else if (l->isString())
                std::cout << color("Literal", Color::GREEN) << "(" << l->getStr() << ")\n";
            else if (l->isBoolean())
                std::cout << color("Literal", Color::GREEN) << "(" << l->getBool() << ")\n";
            break;
        }

        case Expr::Kind::UNARY: {
            UnaryExpr const* u = static_cast<UnaryExpr const*>(e);
            std::cout << color("Unary", Color::BOLD) << " " << toString(u->getOperator()) << "\n";
            printExpr(u->getOperand(), { p.indent + pipe(p.last), true });
            break;
        }

        case Expr::Kind::BINARY: {
            BinaryExpr const* b = static_cast<BinaryExpr const*>(e);
            std::cout << color("Binary", Color::BOLD) << " " << toString(b->getOperator()) << "\n";
            printExpr(b->getLeft(), { p.indent + pipe(p.last), false });
            printExpr(b->getRight(), { p.indent + pipe(p.last), true });
            break;
        }

        case Expr::Kind::CALL: {
            CallExpr const* c = static_cast<CallExpr const*>(e);
            std::cout << color("Call", Color::MAGENTA) << " (" << c->getArgs().size() << " args)\n";
            std::cout << p.indent + pipe(p.last) << "├─ callee:\n";
            printExpr(c->getCallee(), { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ args:\n";
            for (size_t i = 0; i < c->getArgs().size(); ++i)
                printExpr(c->getArgs()[i], { p.indent + pipe(p.last) + "   ", i + 1 == c->getArgs().size() });
            break;
        }

        case Expr::Kind::LIST: {
            ListExpr const* l = static_cast<ListExpr const*>(e);
            std::cout << color("List", Color::BLUE) << " [" << l->getElements().size() << "]\n";
            for (size_t i = 0; i < l->getElements().size(); ++i)
                printExpr(l->getElements()[i], { p.indent + pipe(p.last), i + 1 == l->getElements().size() });
            break;
        }

        case Expr::Kind::ASSIGNMENT: {
            AssignmentExpr const* a = static_cast<AssignmentExpr const*>(e);
            std::cout << color("Assignment", Color::YELLOW) << " :=\n";
            std::cout << p.indent + pipe(p.last) << "├─ target:\n";
            printExpr(a->getTarget(), { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ value:\n";
            printExpr(a->getValue(), { p.indent + pipe(p.last) + "   ", true });
            break;
        }

        default:
            std::cout << color("<unknown expr>", Color::RED) << "\n";
        }
    }

    void printStmt(Stmt const* s, Prefix p)
    {
        if (!s)
            return;

        NodeCount_++;

        std::cout << p.indent << glyph(p.last);

        switch (s->getKind()) {
        case Stmt::Kind::FUNC: {
            FunctionDef const* f = static_cast<FunctionDef const*>(s);
            std::cout << color("FunctionDef", Color::BOLD) << " " << f->getName()->getValue() << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ params:\n";
            for (size_t i = 0; i < f->getParameters().size(); ++i)
                printExpr(f->getParameters()[i], { p.indent + pipe(p.last) + "│  ", i + 1 == f->getParameters().size() });
            std::cout << p.indent + pipe(p.last) << "└─ body:\n";
            printStmt(f->getBody(), { p.indent + pipe(p.last) + "    ", true });
            break;
        }

        case Stmt::Kind::RETURN: {
            ReturnStmt const* r = static_cast<ReturnStmt const*>(s);
            std::cout << color("Return", Color::BOLD) << "\n";
            printExpr(r->getValue(), { p.indent + pipe(p.last), true });
            break;
        }

        case Stmt::Kind::EXPR: {
            ExprStmt const* e = static_cast<ExprStmt const*>(s);
            std::cout << color("ExprStmt", Color::BOLD) << "\n";
            printExpr(e->getExpr(), { p.indent + pipe(p.last), true });
            break;
        }

        case Stmt::Kind::WHILE: {
            WhileStmt const* w = static_cast<WhileStmt const*>(s);
            std::cout << color("While", Color::BOLD) << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ condition:\n";
            printExpr(w->getCondition(), { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ body:\n";
            printStmt(w->getBody(), { p.indent + pipe(p.last) + "   ", true });
            break;
        }
        case Stmt::Kind::IF: {
            IfStmt const* i = static_cast<IfStmt const*>(s);
            std::cout << color("If", Color::BOLD) << "\n";
            std::cout << p.indent + pipe(p.last) << "├─ condition:\n";
            printExpr(i->getCondition(), { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << (i->getElse() ? "├─" : "└─") << " then:\n";
            printStmt(i->getThen(), { p.indent + pipe(p.last) + (i->getElse() ? "│  " : "   "), true });
            if (i->getElse()) {
                std::cout << p.indent + pipe(p.last) << "└─ else:\n";
                printStmt(i->getElse(), { p.indent + pipe(p.last) + "   ", true });
            }
            break;
        }

        case Stmt::Kind::BLOCK: {
            BlockStmt const* b = static_cast<BlockStmt const*>(s);
            std::cout << color("Block", Color::BOLD) << " {" << b->getStatements().size() << " stmts}\n";
            for (size_t i = 0; i < b->getStatements().size(); ++i)
                printStmt(b->getStatements()[i], { p.indent + pipe(p.last), i + 1 == b->getStatements().size() });
            break;
        }

        case Stmt::Kind::ASSIGNMENT: {
            AssignmentStmt const* a = static_cast<AssignmentStmt const*>(s);
            std::cout << color("Assignment", Color::YELLOW) << " :=\n";
            std::cout << p.indent + pipe(p.last) << "├─ target:\n";
            printExpr(a->getTarget(), { p.indent + pipe(p.last) + "│  ", true });
            std::cout << p.indent + pipe(p.last) << "└─ value:\n";
            printExpr(a->getValue(), { p.indent + pipe(p.last) + "   ", true });
            break;
        }

        default:
            std::cout << color("<unknown stmt>", Color::RED) << "\n";
        }
    }

public:
    explicit ASTPrinter(bool const color = true)
        : UseColor_(color)
    {
    }

    void print(Expr const* e) { printExpr(e, { "", true }); }
    void print(Stmt const* s) { printStmt(s, { "", true }); }

    std::uint32_t getNodeCount() const { return NodeCount_; }
};

} // namespace ast
} // namespace mylang

#endif // AST_PRINTER_HPP
