#include "../../include/parser/type_system.hpp"
#include "../../include/diag/diagnostic.hpp"

namespace mylang {
namespace parser {

bool TypeSystem::Type::operator==(Type const& other) const
{
    return base == other.base; // Simplified
}

StringRef TypeSystem::Type::toString() const
{
    switch (base) {
    case BaseType::Int:
        return u"int";
    case BaseType::Float:
        return u"float";
    case BaseType::String:
        return u"str";
    case BaseType::Bool:
        return u"bool";
    case BaseType::None:
        return u"None";
    case BaseType::List:
        if (!TypeParams.empty())
            return u"List[" + TypeParams[0]->toString() + u"]";
        return u"List";
    case BaseType::Dict:
        if (TypeParams.size() >= 2)
            return u"Dict[" + TypeParams[0]->toString() + u", " + TypeParams[1]->toString() + u"]";
        return u"Dict";
    default:
        return u"Any";
    }

    return u"Any";
}

std::shared_ptr<TypeSystem::Type> TypeSystem::TypeInference::freshTypeVar()
{
    std::shared_ptr<TypeSystem::Type> t = std::make_shared<Type>();
    t->base = BaseType::Any;
    return t;
}

void TypeSystem::TypeInference::unify(std::shared_ptr<TypeSystem::Type> t1, std::shared_ptr<TypeSystem::Type> t2)
{
    if (*t1 == *t2)
        return;

    // Type variable substitution
    if (t1->base == BaseType::Any)
        *t1 = *t2;
    else if (t2->base == BaseType::Any)
        *t2 = *t1;
    else if (t1->base == BaseType::List && t2->base == BaseType::List) {
        if (!t1->TypeParams.empty() && !t2->TypeParams.empty())
            unify(t1->TypeParams[0], t2->TypeParams[0]);
    } else {
        /// TODO: diagnostic::engine.panic(StringRef("Type mismatch: ") + t1->toString() + " vs " + t2->toString());
    }
}

std::shared_ptr<typename TypeSystem::Type> TypeSystem::TypeInference::inferExpr(ast::Expr const* expr)
{
    if (!expr)
        return std::make_shared<Type>();

    switch (expr->getKind()) {
    case ast::Expr::Kind::LITERAL: {
        ast::LiteralExpr const* lit = static_cast<ast::LiteralExpr const*>(expr);
        std::shared_ptr<Type> t = std::make_shared<Type>();
        switch (lit->getType()) {
        case ast::LiteralExpr::Type::NUMBER:
            t->base = lit->getValue().find('.') ? BaseType::Float : BaseType::Int;
            break;
        case ast::LiteralExpr::Type::STRING:
            t->base = BaseType::String;
            break;
        case ast::LiteralExpr::Type::BOOLEAN:
            t->base = BaseType::Bool;
            break;
        case ast::LiteralExpr::Type::NONE:
            t->base = BaseType::None;
            break;
        }
        return t;
    }
    case ast::Expr::Kind::BINARY: {
        ast::BinaryExpr const* bin = static_cast<ast::BinaryExpr const*>(expr);
        std::shared_ptr<Type> leftType = inferExpr(bin->getLeft());
        std::shared_ptr<Type> rightType = inferExpr(bin->getRight());
        unify(leftType, rightType);
        // Result type based on operator
        if (bin->getOperator() == tok::TokenType::OP_PLUS || bin->getOperator() == tok::TokenType::OP_MINUS
            || bin->getOperator() == tok::TokenType::OP_STAR || bin->getOperator() == tok::TokenType::OP_SLASH)
            return leftType;
        else if (bin->getOperator() == tok::TokenType::OP_EQ || bin->getOperator() == tok::TokenType::OP_NEQ
            || bin->getOperator() == tok::TokenType::OP_LT || bin->getOperator() == tok::TokenType::OP_GT) {
            std::shared_ptr<Type> t = std::make_shared<Type>();
            t->base = BaseType::Bool;
            return t;
        }
        return leftType;
    }
    case ast::Expr::Kind::LIST: {
        ast::ListExpr const* list = static_cast<ast::ListExpr const*>(expr);
        std::shared_ptr<Type> t = std::make_shared<Type>();
        t->base = BaseType::List;
        if (!list->getElements().empty()) {
            std::shared_ptr<Type> elemType = inferExpr(list->getElements()[0]);
            t->TypeParams.push_back(elemType);
        }
        return t;
    }
    default:
        return freshTypeVar();
    }

    return freshTypeVar();
}

}
}
