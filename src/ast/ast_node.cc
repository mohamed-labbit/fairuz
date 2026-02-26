// ast_node.cc

#include "../../include/ast/ast_node.hpp"

namespace mylang {
namespace ast {

typename ASTNode::NodeType ASTNode::getNodeType() const
{
    return NodeType_;
}

uint32_t ASTNode::getLine() const
{
    return Line_;
}

uint32_t ASTNode::getColumn() const
{
    return Column_;
}

void ASTNode::setLine(uint32_t const l)
{
    Line_ = l;
}

void ASTNode::setColumn(uint32_t const c)
{
    Column_ = c;
}

}
}
