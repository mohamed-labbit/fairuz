#pragma once


#include "../../macros.hpp"
#include "ast.hpp"
#include <iostream>

// Advanced AST Printer with stats

namespace mylang {
namespace parser {
namespace ast {

class ASTPrinter
{
 private:
  std::uint8_t Indent_{0};
  std::uint8_t NodeCount_{0};
  bool UseColor_{true};

  std::string colorize(const StringType& text, const StringType color, bool enabled = true)
  {
    if (!enabled) return utf8::utf16to8(text);
    return utf8::utf16to8(color + text + Color::RESET);
  }

  void printIndent()
  {
    for (int i = 0; i < Indent_; i++) std::cout << (UseColor_ ? "│ " : "| ");
  }

 public:
  explicit ASTPrinter(bool color = true) :
      UseColor_(color)
  {
  }

  void print(const ast::Expr* expr)
  {
    if (expr == nullptr) return;

    NodeCount_++;

    switch (expr->getKind())
    {
    case ast::Expr::Kind::BINARY : {
      const ast::BinaryExpr* e = static_cast<const ast::BinaryExpr*>(expr);
      printIndent();
      std::cout << colorize(u"BinaryOp", Color::BOLD, UseColor_) << " " << colorize(lex::tok::toString(e->getOperator()), Color::YELLOW, UseColor_)
                << "\n";
      Indent_++;
      print(e->getLeft());
      print(e->getRight());
      Indent_--;
      break;
    }
    case ast::Expr::Kind::UNARY : {
      const ast::UnaryExpr* e = static_cast<const ast::UnaryExpr*>(expr);
      printIndent();
      std::cout << colorize(u"UnaryOp", Color::BOLD, UseColor_) << " " << colorize(lex::tok::toString(e->getOperator()), Color::YELLOW, UseColor_)
                << "\n";
      Indent_++;
      print(e->getOperand());  // FIX: Print the operand, not the UnaryExpr itself!
      Indent_--;
      break;
    }
    case ast::Expr::Kind::LITERAL : {
      const ast::LiteralExpr* e = static_cast<const ast::LiteralExpr*>(expr);
      printIndent();
      std::cout << colorize(u"Literal", Color::GREEN, UseColor_) << ": " << utf8::utf16to8(e->getValue()) << "\n";
      break;
    }
    case ast::Expr::Kind::NAME : {
      const ast::NameExpr* e = static_cast<const ast::NameExpr*>(expr);
      printIndent();
      std::cout << colorize(u"Var", Color::CYAN, UseColor_) << ": " << utf8::utf16to8(e->getValue()) << "\n";
      break;
    }
    case ast::Expr::Kind::CALL : {
      const ast::CallExpr* e = dynamic_cast<const ast::CallExpr*>(expr);
      printIndent();
      std::cout << colorize(u"Call", Color::MAGENTA, UseColor_) << " [" << e->getArgs().size() << " args]\n";
      Indent_++;
      print(e->getCallee());
      for (const ast::Expr* const& arg : e->getArgs()) print(arg);
      Indent_--;
      break;
    }
    case ast::Expr::Kind::LIST : {
      const ast::ListExpr* e = dynamic_cast<const ast::ListExpr*>(expr);
      printIndent();
      std::cout << colorize(u"List", Color::BLUE, UseColor_) << " [" << e->getElements().size() << "]\n";
      Indent_++;
      for (const ast::Expr* const& elem : e->getElements()) print(elem);
      Indent_--;
      break;
    }
    default : break;
    }
  }

  void print(const ast::Stmt* stmt)
  {
    if (stmt == nullptr) return;

    NodeCount_++;

    switch (stmt->getKind())
    {
    case ast::Stmt::Kind::ASSIGNMENT : {
      const ast::AssignmentStmt* s = static_cast<const ast::AssignmentStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"Assign", Color::BOLD, UseColor_) << " " << colorize(s->getTarget()->getValue(), Color::CYAN, UseColor_) << "\n";
      Indent_++;
      print(s->getValue());
      Indent_--;
      break;
    }
    case ast::Stmt::Kind::IF : {
      const ast::IfStmt* s = static_cast<const ast::IfStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"If", Color::BOLD, UseColor_) << "\n";
      Indent_++;
      print(s->getCondition());
      for (const ast::Stmt* const& st : s->getThenBlock()->getStatements()) print(st);
      Indent_--;
      break;
    }
    case ast::Stmt::Kind::WHILE : {
      const ast::WhileStmt* s = static_cast<const ast::WhileStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"While", Color::BOLD, UseColor_) << "\n";
      Indent_++;
      print(s->getCondition());
      for (const ast::Stmt* const& st : s->getBlock()->getStatements()) print(st);
      Indent_--;
      break;
    }
    case ast::Stmt::Kind::FOR : {
      const ast::ForStmt* s = static_cast<const ast::ForStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"For", Color::BOLD, UseColor_) << " " << utf8::utf16to8(s->getTarget()->getValue()) << "\n";
      Indent_++;
      print(s->getIter());
      for (const ast::Stmt* const& st : s->getBlock()->getStatements()) print(st);
      Indent_--;
      break;
    }
    case ast::Stmt::Kind::FUNC : {
      const ast::FunctionDef* s = dynamic_cast<const ast::FunctionDef*>(stmt);
      printIndent();
      std::cout << colorize(u"Function", Color::BOLD, UseColor_) << " " << colorize(s->getName()->getValue(), Color::GREEN, UseColor_) << "(";
      for (std::size_t i = 0; i < s->getParameters().size(); i++)
      {
        std::cout << utf8::utf16to8(s->getParameters()[i]->getValue());
        if (i + 1 < s->getParameters().size()) std::cout << ", ";
      }
      std::cout << ")\n";
      Indent_++;
      for (const ast::Stmt* const& st : s->getBody()->getStatements()) print(st);
      Indent_--;
      break;
    }
    case ast::Stmt::Kind::RETURN : {
      const ast::ReturnStmt* s = static_cast<const ast::ReturnStmt*>(stmt);
      printIndent();
      std::cout << colorize(u"Return", Color::BOLD, UseColor_) << "\n";
      Indent_++;
      print(s->getValue());
      Indent_--;
      break;
    }
    case ast::Stmt::Kind::EXPR : {
      const ast::ExprStmt* s = static_cast<const ast::ExprStmt*>(stmt);
      print(s->getExpr());
      break;
    }
    default : break;
    }
  }

  int getNodeCount() const { return NodeCount_; }
};

}  // ast
}  // parser
}  // mylang