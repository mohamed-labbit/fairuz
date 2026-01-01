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

  std::string colorize(const string_type& text, const string_type color, bool enabled = true)
  {
    if (!enabled) return utf8::utf16to8(text);
    return utf8::utf16to8(color + text + Color::RESET);
  }

  void printIndent()
  {
    for (int i = 0; i < indent; i++)
      std::cout << (useColor ? "│ " : "| ");
  }

 public:
  explicit EnhancedASTPrinter(bool color = true) :
      useColor(color)
  {
  }

  void print(const mylang::parser::ast::Expr* expr)
  {
    if (!expr) return;

    nodeCount++;

    switch (expr->getKind())
    {
    case mylang::parser::ast::Expr::Kind::BINARY : {
      auto* e = static_cast<const mylang::parser::ast::BinaryExpr*>(expr);
      printIndent();
      std::cout << colorize(u"BinaryOp", Color::BOLD, useColor) << " " << colorize(lex::tok::to_string(e->getOperator()), Color::YELLOW, useColor) << "\n";
      indent++;
      print(e->getLeft());
      print(e->getRight());
      indent--;
      break;
    }
    case mylang::parser::ast::Expr::Kind::UNARY : {
      auto* e = static_cast<const mylang::parser::ast::UnaryExpr*>(expr);
      printIndent();
      std::cout << colorize(u"UnaryOp", Color::BOLD, useColor) << " " << colorize(lex::tok::to_string(e->getOperator()), Color::YELLOW, useColor) << "\n";
      indent++;
      print(e);
      indent--;
      break;
    }
    case mylang::parser::ast::Expr::Kind::LITERAL : {
      auto* e = static_cast<const mylang::parser::ast::LiteralExpr*>(expr);
      printIndent();
      std::cout << colorize(u"Literal", Color::GREEN, useColor) << ": " << utf8::utf16to8(e->getValue()->getStr()) << "\n";
      break;
    }
    case mylang::parser::ast::Expr::Kind::NAME : {
      auto* e = static_cast<const mylang::parser::ast::NameExpr*>(expr);
      printIndent();
      std::cout << colorize(u"Var", Color::CYAN, useColor) << ": " << utf8::utf16to8(e->getValue()->getStr()) << "\n";
      break;
    }
    case mylang::parser::ast::Expr::Kind::CALL : {
      auto* e = dynamic_cast<const mylang::parser::ast::CallExpr*>(expr);
      printIndent();
      std::cout << colorize(u"Call", Color::MAGENTA, useColor) << " [" << e->getArgs().size() << " args]\n";
      indent++;
      print(e->getCallee());
      for (const auto& arg : e->getArgs())
        print(arg.get());
      indent--;
      break;
    }
    case mylang::parser::ast::Expr::Kind::LIST : {
      auto* e = dynamic_cast<const mylang::parser::ast::ListExpr*>(expr);
      printIndent();
      std::cout << colorize(u"List", Color::BLUE, useColor) << " [" << e->getElements().size() << "]\n";
      indent++;
      for (const auto& elem : e->getElements())
        print(elem.get());
      indent--;
      break;
    }
    default : break;
    }
  }

  void print(const mylang::parser::ast::Stmt* stmt)
  {
    if (!stmt) return;

    nodeCount++;

    switch (stmt->getKind())
    {
    case mylang::parser::ast::Stmt::Kind::ASSIGNMENT : {
      auto* s = static_cast<const mylang::parser::ast::AssignmentStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"Assign", Color::BOLD, useColor) << " " << colorize(s->getTarget()->getValue()->getStr(), Color::CYAN, useColor) << "\n";
      indent++;
      print(s->getValue());
      indent--;
      break;
    }
    case mylang::parser::ast::Stmt::Kind::IF : {
      auto* s = static_cast<const mylang::parser::ast::IfStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"If", Color::BOLD, useColor) << "\n";
      indent++;
      print(s->getCondition());
      for (const auto& st : s->getThenBlock()->getStatements())
        print(st.get());
      indent--;
      break;
    }
    case mylang::parser::ast::Stmt::Kind::WHILE : {
      auto* s = static_cast<const mylang::parser::ast::WhileStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"While", Color::BOLD, useColor) << "\n";
      indent++;
      print(s->getCondition());
      for (const auto& st : s->getBlock()->getStatements())
        print(st.get());
      indent--;
      break;
    }
    case mylang::parser::ast::Stmt::Kind::FOR : {
      auto* s = static_cast<const mylang::parser::ast::ForStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"For", Color::BOLD, useColor) << " " << utf8::utf16to8(s->getTarget()->getStr()) << "\n";
      indent++;
      print(s->getIter());
      for (const auto& st : s->getBlock()->getStatements())
        print(st.get());
      indent--;
      break;
    }
    case mylang::parser::ast::Stmt::Kind::FUNC : {
      auto* s = dynamic_cast<const mylang::parser::ast::FunctionDef*>(stmt);
      printIndent();
      std::cout << colorize(u"Function", Color::BOLD, useColor) << " " << colorize(s->getName()->getValue()->getStr(), Color::GREEN, useColor) << "(";
      for (std::size_t i = 0; i < s->getParameters().size(); i++)
      {
        std::cout << utf8::utf16to8(s->getParameters()[i]);
        if (i + 1 < s->getParameters().size()) std::cout << ", ";
      }
      std::cout << ")\n";
      indent++;
      for (const auto& st : s->getBody()->getStatements())
        print(st.get());
      indent--;
      break;
    }
    case mylang::parser::ast::Stmt::Kind::RETURN : {
      auto* s = static_cast<const mylang::parser::ast::ReturnStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"Return", Color::BOLD, useColor) << "\n";
      indent++;
      print(s->getValue());
      indent--;
      break;
    }
    case mylang::parser::ast::Stmt::Kind::EXPR : {
      auto* s = static_cast<const mylang::parser::ast::ExprStmt*>(stmt);
      print(s->getExpr());
      break;
    }
    default : break;
    }
  }

  int getNodeCount() const { return nodeCount; }
};

}  // ast
}  // parser
}  // mylang