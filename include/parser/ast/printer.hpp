#pragma once


#include "../../macros.hpp"
#include "ast.hpp"
#include <iostream>


namespace mylang {
namespace parser {
namespace ast {

class ASTPrinter
{
 private:
  bool          UseColor_;
  std::uint32_t NodeCount_ = 0;

  struct Prefix
  {
    std::string indent;
    bool        last;
  };

  std::string glyph(bool last) const { return last ? "└─ " : "├─ "; }

  std::string pipe(bool last) const { return last ? "   " : "│  "; }

  std::string color(const StringRef& s, const StringRef& c) const
  {
    if (!UseColor_)
    {
      return utf8::utf16to8(s);
    }
    return utf8::utf16to8(c + s + Color::RESET);
  }

  void printExpr(const Expr* e, Prefix p)
  {
    if (!e)
    {
      return;
    }
    NodeCount_++;

    std::cout << p.indent << glyph(p.last);

    switch (e->getKind())
    {
    case Expr::Kind::NAME : {
      const NameExpr* n = static_cast<const NameExpr*>(e);
      std::cout << color(u"Name", Color::CYAN) << "(" << utf8::utf16to8(n->getValue()) << ")\n";
      break;
    }

    case Expr::Kind::LITERAL : {
      const LiteralExpr* l = static_cast<const LiteralExpr*>(e);
      std::cout << color(u"Literal", Color::GREEN) << "(" << utf8::utf16to8(l->getValue()) << ")\n";
      break;
    }

    case Expr::Kind::UNARY : {
      const UnaryExpr* u = static_cast<const UnaryExpr*>(e);
      std::cout << color(u"Unary", Color::BOLD) << " " << utf8::utf16to8(tok::toString(u->getOperator())) << "\n";
      printExpr(u->getOperand(), {p.indent + pipe(p.last), true});
      break;
    }

    case Expr::Kind::BINARY : {
      const BinaryExpr* b = static_cast<const BinaryExpr*>(e);
      std::cout << color(u"Binary", Color::BOLD) << " " << utf8::utf16to8(tok::toString(b->getOperator())) << "\n";
      printExpr(b->getLeft(), {p.indent + pipe(p.last), false});
      printExpr(b->getRight(), {p.indent + pipe(p.last), true});
      break;
    }

    case Expr::Kind::CALL : {
      const CallExpr* c = static_cast<const CallExpr*>(e);
      std::cout << color(u"Call", Color::MAGENTA) << " (" << c->getArgs().size() << " args)\n";
      std::cout << p.indent + pipe(p.last) << "├─ callee:\n";
      printExpr(c->getCallee(), {p.indent + pipe(p.last) + "│  ", true});
      std::cout << p.indent + pipe(p.last) << "└─ args:\n";
      for (SizeType i = 0; i < c->getArgs().size(); ++i)
      {
        printExpr(c->getArgs()[i], {p.indent + pipe(p.last) + "   ", i + 1 == c->getArgs().size()});
      }
      break;
    }

    case Expr::Kind::LIST : {
      const ListExpr* l = static_cast<const ListExpr*>(e);
      std::cout << color(u"List", Color::BLUE) << " [" << l->getElements().size() << "]\n";
      for (SizeType i = 0; i < l->getElements().size(); ++i)
      {
        printExpr(l->getElements()[i], {p.indent + pipe(p.last), i + 1 == l->getElements().size()});
      }
      break;
    }

    case Expr::Kind::ASSIGNMENT : {
      const AssignmentExpr* a = static_cast<const AssignmentExpr*>(e);
      std::cout << color(u"Assignment", Color::YELLOW) << " :=\n";
      std::cout << p.indent + pipe(p.last) << "├─ target:\n";
      printExpr(a->getTarget(), {p.indent + pipe(p.last) + "│  ", true});
      std::cout << p.indent + pipe(p.last) << "└─ value:\n";
      printExpr(a->getValue(), {p.indent + pipe(p.last) + "   ", true});
      break;
    }

    default :
      std::cout << color(u"<unknown expr>", Color::RED) << "\n";
    }
  }

  void printStmt(const Stmt* s, Prefix p)
  {
    if (s == nullptr)
    {
      return;
    }

    NodeCount_++;

    std::cout << p.indent << glyph(p.last);

    switch (s->getKind())
    {
    case Stmt::Kind::FUNC : {
      const FunctionDef* f = static_cast<const FunctionDef*>(s);
      std::cout << color(u"FunctionDef", Color::BOLD) << " " << utf8::utf16to8(f->getName()->getValue()) << "\n";
      std::cout << p.indent + pipe(p.last) << "├─ params:\n";
      for (SizeType i = 0; i < f->getParameters().size(); ++i)
      {
        printExpr(f->getParameters()[i], {p.indent + pipe(p.last) + "│  ", i + 1 == f->getParameters().size()});
      }
      std::cout << p.indent + pipe(p.last) << "└─ body:\n";
      for (SizeType i = 0; i < f->getBody()->getStatements().size(); ++i)
      {
        printStmt(f->getBody()->getStatements()[i], {p.indent + pipe(p.last) + "   ", i + 1 == f->getBody()->getStatements().size()});
      }
      break;
    }

    case Stmt::Kind::RETURN : {
      const ReturnStmt* r = static_cast<const ReturnStmt*>(s);
      std::cout << color(u"Return", Color::BOLD) << "\n";
      printExpr(r->getValue(), {p.indent + pipe(p.last), true});
      break;
    }

    case Stmt::Kind::EXPR : {
      const ExprStmt* e = static_cast<const ExprStmt*>(s);
      std::cout << color(u"ExprStmt", Color::BOLD) << "\n";
      printExpr(e->getExpr(), {p.indent + pipe(p.last), true});
      break;
    }

    case Stmt::Kind::WHILE : {
      const WhileStmt* w = static_cast<const WhileStmt*>(s);
      std::cout << color(u"While", Color::BOLD) << "\n";
      std::cout << p.indent + pipe(p.last) << "├─ condition:\n";
      printExpr(w->getCondition(), {p.indent + pipe(p.last) + "│  ", true});
      std::cout << p.indent + pipe(p.last) << "└─ body:\n";
      printStmt(w->getBlock(), {p.indent + pipe(p.last) + "   ", true});
      break;
    }
      /*
    case Stmt::Kind::FOR : {
      const ForStmt* f = static_cast<const ForStmt*>(s);
      std::cout << color(u"For", Color::BOLD) << "\n";
      std::cout << p.indent + pipe(p.last) << "├─ init:\n";
      if (f->getInit())
        printStmt(f->getInit(), {p.indent + pipe(p.last) + "│  ", true});
      std::cout << p.indent + pipe(p.last) << "├─ condition:\n";
      if (f->getCondition())
        printExpr(f->getCondition(), {p.indent + pipe(p.last) + "│  ", true});
      std::cout << p.indent + pipe(p.last) << "├─ increment:\n";
      if (f->getIncrement())
        printExpr(f->getIncrement(), {p.indent + pipe(p.last) + "│  ", true});
      std::cout << p.indent + pipe(p.last) << "└─ body:\n";
      printStmt(f->getBody(), {p.indent + pipe(p.last) + "   ", true});
      break;
    }
*/
    case Stmt::Kind::IF : {
      const IfStmt* i = static_cast<const IfStmt*>(s);
      std::cout << color(u"If", Color::BOLD) << "\n";
      std::cout << p.indent + pipe(p.last) << "├─ condition:\n";
      printExpr(i->getCondition(), {p.indent + pipe(p.last) + "│  ", true});
      std::cout << p.indent + pipe(p.last) << (i->getElseBlock() ? "├─" : "└─") << " then:\n";
      printStmt(i->getThenBlock(), {p.indent + pipe(p.last) + (i->getElseBlock() ? "│  " : "   "), true});
      if (i->getElseBlock())
      {
        std::cout << p.indent + pipe(p.last) << "└─ else:\n";
        printStmt(i->getElseBlock(), {p.indent + pipe(p.last) + "   ", true});
      }
      break;
    }

    case Stmt::Kind::BLOCK : {
      const BlockStmt* b = static_cast<const BlockStmt*>(s);
      std::cout << color(u"Block", Color::BOLD) << " {" << b->getStatements().size() << " stmts}\n";
      for (SizeType i = 0; i < b->getStatements().size(); ++i)
      {
        printStmt(b->getStatements()[i], {p.indent + pipe(p.last), i + 1 == b->getStatements().size()});
      }
      break;
    }

    default :
      std::cout << color(u"<unknown stmt>", Color::RED) << "\n";
    }
  }

 public:
  explicit ASTPrinter(bool color = true) :
      UseColor_(color)
  {
  }

  void print(const Expr* e) { printExpr(e, {"", true}); }
  void print(const Stmt* s) { printStmt(s, {"", true}); }

  std::uint32_t getNodeCount() const { return NodeCount_; }
};

}  // ast
}  // parser
}  // mylang