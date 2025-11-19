#pragma once


#include "ast.hpp"
#include <iostream>

// Advanced AST Printer with stats

namespace mylang {
namespace parser {
namespace ast {

class EnhancedASTPrinter
{
   private:
    std::uint8_t indent{0};
    std::uint8_t nodeCount{0};
    bool useColor{true};

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
            std::cout << colorize(u"BinaryOp", Color::BOLD, useColor) << " " << colorize(e->op, Color::YELLOW, useColor)
                      << "\n";
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
            for (std::size_t i = 0; i < s->params.size(); i++)
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

}  // ast
}  // parser
}  // mylang