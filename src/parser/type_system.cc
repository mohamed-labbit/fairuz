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
    case BaseType::INT:
        return "int";
    case BaseType::FLOAT:
        return "float";
    case BaseType::STRING:
        return "str";
    case BaseType::BOOL:
        return "bool";
    case BaseType::NONE:
        return "None";
    case BaseType::LIST:
        if (!TypeParams.empty())
            return "List[" + TypeParams[0]->toString() + "]";
        return "List";
    case BaseType::DICT:
        if (TypeParams.size() >= 2)
            return "Dict[" + TypeParams[0]->toString() + ", " + TypeParams[1]->toString() + "]";
        return "Dict";
    default:
        return "Any";
    }

    return "Any";
}

std::shared_ptr<TypeSystem::Type> TypeSystem::TypeInference::freshTypeVar()
{
    std::shared_ptr<TypeSystem::Type> t = std::make_shared<Type>();
    t->base = BaseType::ANY;
    return t;
}

void TypeSystem::TypeInference::unify(std::shared_ptr<TypeSystem::Type> t1, std::shared_ptr<TypeSystem::Type> t2)
{
    if (*t1 == *t2)
        return;

    // Type variable substitution
    if (t1->base == BaseType::ANY)
        *t1 = *t2;
    else if (t2->base == BaseType::ANY)
        *t2 = *t1;
    else if (t1->base == BaseType::LIST && t2->base == BaseType::LIST) {
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
        case ast::LiteralExpr::Type::INTEGER:
            t->base = BaseType::INT;
            break;
        case ast::LiteralExpr::Type::FLOAT:
            t->base = BaseType::FLOAT;
            break;
        case ast::LiteralExpr::Type::STRING:
            t->base = BaseType::STRING;
            break;
        case ast::LiteralExpr::Type::BOOLEAN:
            t->base = BaseType::BOOL;
            break;
        case ast::LiteralExpr::Type::NONE:
            t->base = BaseType::NONE;
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
            t->base = BaseType::BOOL;
            return t;
        }

        return leftType;
    }
    case ast::Expr::Kind::LIST: {
        ast::ListExpr const* list = static_cast<ast::ListExpr const*>(expr);
        std::shared_ptr<Type> t = std::make_shared<Type>();
        t->base = BaseType::LIST;

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
