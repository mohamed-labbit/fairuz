#include "../../include/parser/type_system.hpp"
#include "../../include/diag/diagnostic.hpp"


namespace mylang {
namespace parser {

bool TypeSystem::Type::operator==(const Type& other) const
{
  return base == other.base;  // Simplified
}

string_type TypeSystem::Type::toString() const
{
  switch (base)
  {
  case BaseType::Int : return u"int";
  case BaseType::Float : return u"float";
  case BaseType::String : return u"str";
  case BaseType::Bool : return u"bool";
  case BaseType::None : return u"None";
  case BaseType::List :
    if (!TypeParams.empty()) return u"List[" + TypeParams[0]->toString() + u"]";
    return u"List";
  case BaseType::Dict :
    if (TypeParams.size() >= 2) return u"Dict[" + TypeParams[0]->toString() + u", " + TypeParams[1]->toString() + u"]";
    return u"Dict";
  default : return u"Any";
  }

  return u"Any";
}

std::shared_ptr<TypeSystem::Type> TypeSystem::TypeInference::freshTypeVar()
{
  auto t = std::make_shared<Type>();
  t->base = BaseType::Any;
  return t;
}

void TypeSystem::TypeInference::unify(std::shared_ptr<TypeSystem::Type> t1, std::shared_ptr<TypeSystem::Type> t2)
{
  if (*t1 == *t2) return;

  // Type variable substitution
  if (t1->base == BaseType::Any) { *t1 = *t2; }
  else if (t2->base == BaseType::Any) { *t2 = *t1; }
  else if (t1->base == BaseType::List && t2->base == BaseType::List)
  {
    if (!t1->TypeParams.empty() && !t2->TypeParams.empty()) unify(t1->TypeParams[0], t2->TypeParams[0]);
  }
  else
  {
    diagnostic::engine.panic("Type mismatch: " + utf8::utf16to8(t1->toString()) + " vs " + utf8::utf16to8(t2->toString()));
  }
}

std::shared_ptr<TypeSystem::Type> TypeSystem::TypeInference::inferExpr(const ast::Expr* expr)
{
  if (!expr) return std::make_shared<Type>();

  switch (expr->getKind())
  {
  case ast::Expr::Kind::LITERAL : {
    auto* lit = static_cast<const ast::LiteralExpr*>(expr);
    auto t = std::make_shared<Type>();
    switch (lit->getType())
    {
    case ast::LiteralExpr::Type::NUMBER : t->base = lit->getValue().find('.') != std::string::npos ? BaseType::Float : BaseType::Int; break;
    case ast::LiteralExpr::Type::STRING : t->base = BaseType::String; break;
    case ast::LiteralExpr::Type::BOOLEAN : t->base = BaseType::Bool; break;
    case ast::LiteralExpr::Type::NONE : t->base = BaseType::None; break;
    }
    return t;
  }
  case ast::Expr::Kind::BINARY : {
    auto* bin = static_cast<const ast::BinaryExpr*>(expr);
    auto leftType = inferExpr(bin->getLeft());
    auto rightType = inferExpr(bin->getRight());
    unify(leftType, rightType);
    // Result type based on operator
    if (bin->getOperator() == lex::tok::TokenType::OP_PLUS || bin->getOperator() == lex::tok::TokenType::OP_MINUS
        || bin->getOperator() == lex::tok::TokenType::OP_STAR || bin->getOperator() == lex::tok::TokenType::OP_SLASH)
    {
      return leftType;
    }
    else if (bin->getOperator() == lex::tok::TokenType::OP_EQ || bin->getOperator() == lex::tok::TokenType::OP_NEQ
             || bin->getOperator() == lex::tok::TokenType::OP_LT || bin->getOperator() == lex::tok::TokenType::OP_GT)
    {
      auto t = std::make_shared<Type>();
      t->base = BaseType::Bool;
      return t;
    }
    return leftType;
  }
  case ast::Expr::Kind::LIST : {
    auto* list = static_cast<const ast::ListExpr*>(expr);
    auto t = std::make_shared<Type>();
    t->base = BaseType::List;
    if (!list->getElements().empty())
    {
      auto elemType = inferExpr(list->getElements()[0]);
      t->TypeParams.push_back(elemType);
    }
    return t;
  }
  default : return freshTypeVar();
  }

  return freshTypeVar();
}

}
}