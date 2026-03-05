#include "../../include/parser/type_system.hpp"
#include "../../include/diagnostic.hpp"

namespace mylang {
namespace parser {

using namespace ast;

bool TypeSystem::Type::operator==(Type const& other) const
{
    return base == other.base; // Simplified
}

StringRef TypeSystem::Type::toString() const
{
    switch (base) {
    case BaseType::INT: return "int";
    case BaseType::FLOAT: return "float";
    case BaseType::STRING: return "str";
    case BaseType::BOOL: return "bool";
    case BaseType::NIL: return "nil";
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

std::shared_ptr<typename TypeSystem::Type> TypeSystem::TypeInference::inferExpr(Expr const* expr)
{
    if (!expr)
        return std::make_shared<Type>();

    switch (expr->getKind()) {
    case Expr::Kind::LITERAL: {
        LiteralExpr const* lit = static_cast<LiteralExpr const*>(expr);
        std::shared_ptr<Type> t = std::make_shared<Type>();

        LiteralExpr::Type lit_type = lit->getType();

        if (lit_type == LiteralExpr::Type::INTEGER)
            t->base = BaseType::INT;
        else if (lit_type == LiteralExpr::Type::FLOAT)
            t->base = BaseType::FLOAT;
        else if (lit_type == LiteralExpr::Type::STRING)
            t->base = BaseType::STRING;
        else if (lit_type == LiteralExpr::Type::BOOLEAN)
            t->base = BaseType::BOOL;
        else if (lit_type == LiteralExpr::Type::NIL)
            t->base = BaseType::NIL;

        return t;
    }
    case Expr::Kind::BINARY: {
        BinaryExpr const* bin = static_cast<BinaryExpr const*>(expr);

        std::shared_ptr<Type> leftType = inferExpr(bin->getLeft());
        std::shared_ptr<Type> rightType = inferExpr(bin->getRight());

        unify(leftType, rightType);

        // Result type based on operator
        if (bin->getOperator() == BinaryOp::OP_ADD
            || bin->getOperator() == BinaryOp::OP_SUB
            || bin->getOperator() == BinaryOp::OP_MUL
            || bin->getOperator() == BinaryOp::OP_DIV)

            return leftType;
        else if (bin->getOperator() == BinaryOp::OP_EQ
            || bin->getOperator() == BinaryOp::OP_NEQ
            || bin->getOperator() == BinaryOp::OP_LT
            || bin->getOperator() == BinaryOp::OP_GT) {

            std::shared_ptr<Type> t = std::make_shared<Type>();
            t->base = BaseType::BOOL;
            return t;
        }

        return leftType;
    }
    case Expr::Kind::LIST: {
        ListExpr const* list = static_cast<ListExpr const*>(expr);
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
