// ast_node.cc

#include "../../include/ast/ast_node.hpp"

namespace mylang {
namespace ast {

typename ASTNode::NodeType ASTNode::getNodeType() const
{
    return NodeType_;
}

std::size_t ASTNode::getLine() const
{
    return Line_;
}

std::size_t ASTNode::getColumn() const
{
    return Column_;
}

void ASTNode::setLine(std::size_t const l)
{
    Line_ = l;
}

void ASTNode::setColumn(std::size_t const c)
{
    Column_ = c;
}

}
}
