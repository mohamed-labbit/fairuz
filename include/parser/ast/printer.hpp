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

  std::string color(const StringType& s, const StringType& c) const
  {
    if (!UseColor_)
      return utf8::utf16to8(s);
    return utf8::utf16to8(c + s + Color::RESET);
  }

  void printExpr(const Expr* e, Prefix p)
  {
    if (!e)
      return;
    NodeCount_++;

    std::cout << p.indent << glyph(p.last);

    switch (e->getKind())
    {
    case Expr::Kind::NAME : {
      auto* n = static_cast<const NameExpr*>(e);
      std::cout << color(u"Name", Color::CYAN) << "(" << utf8::utf16to8(n->getValue()) << ")\n";
      break;
    }

    case Expr::Kind::LITERAL : {
      auto* l = static_cast<const LiteralExpr*>(e);
      std::cout << color(u"Literal", Color::GREEN) << "(" << utf8::utf16to8(l->getValue()) << ")\n";
      break;
    }

    case Expr::Kind::UNARY : {
      auto* u = static_cast<const UnaryExpr*>(e);
      std::cout << color(u"Unary", Color::BOLD) << " " << utf8::utf16to8(lex::tok::toString(u->getOperator())) << "\n";
      printExpr(u->getOperand(), {p.indent + pipe(p.last), true});
      break;
    }

    case Expr::Kind::BINARY : {
      auto* b = static_cast<const BinaryExpr*>(e);
      std::cout << color(u"Binary", Color::BOLD) << " " << utf8::utf16to8(lex::tok::toString(b->getOperator())) << "\n";
      printExpr(b->getLeft(), {p.indent + pipe(p.last), false});
      printExpr(b->getRight(), {p.indent + pipe(p.last), true});
      break;
    }

    case Expr::Kind::CALL : {
      auto* c = static_cast<const CallExpr*>(e);
      std::cout << color(u"Call", Color::MAGENTA) << " (" << c->getArgs().size() << " args)\n";
      std::cout << p.indent + pipe(p.last) << "├─ callee:\n";
      printExpr(c->getCallee(), {p.indent + pipe(p.last) + "│  ", true});
      std::cout << p.indent + pipe(p.last) << "└─ args:\n";
      for (std::size_t i = 0; i < c->getArgs().size(); ++i)
        printExpr(c->getArgs()[i], {p.indent + pipe(p.last) + "   ", i + 1 == c->getArgs().size()});
      break;
    }

    case Expr::Kind::LIST : {
      auto* l = static_cast<const ListExpr*>(e);
      std::cout << color(u"List", Color::BLUE) << " [" << l->getElements().size() << "]\n";
      for (std::size_t i = 0; i < l->getElements().size(); ++i)
        printExpr(l->getElements()[i], {p.indent + pipe(p.last), i + 1 == l->getElements().size()});
      break;
    }

    case Expr::Kind::ASSIGNMENT : {
      auto* a = static_cast<const AssignmentExpr*>(e);
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
    if (!s)
      return;
    NodeCount_++;

    std::cout << p.indent << glyph(p.last);

    switch (s->getKind())
    {
    case Stmt::Kind::FUNC : {
      auto* f = static_cast<const FunctionDef*>(s);
      std::cout << color(u"FunctionDef", Color::BOLD) << " " << utf8::utf16to8(f->getName()->getValue()) << "\n";
      std::cout << p.indent + pipe(p.last) << "├─ params:\n";
      for (std::size_t i = 0; i < f->getParameters().size(); ++i)
        printExpr(f->getParameters()[i], {p.indent + pipe(p.last) + "│  ", i + 1 == f->getParameters().size()});
      std::cout << p.indent + pipe(p.last) << "└─ body:\n";
      for (const Stmt* st : f->getBody()->getStatements())
        printStmt(st, {p.indent + pipe(p.last) + "   ", true});
      break;
    }

    case Stmt::Kind::RETURN : {
      auto* r = static_cast<const ReturnStmt*>(s);
      std::cout << color(u"Return", Color::BOLD) << "\n";
      printExpr(r->getValue(), {p.indent + pipe(p.last), true});
      break;
    }

    case Stmt::Kind::EXPR : {
      auto* e = static_cast<const ExprStmt*>(s);
      std::cout << color(u"ExprStmt", Color::BOLD) << "\n";
      printExpr(e->getExpr(), {p.indent + pipe(p.last), true});
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