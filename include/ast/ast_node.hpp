#pragma once

#include "../macros.hpp"
#include <utility>

namespace mylang {
namespace ast {

class ASTNode {
public:
    enum NodeType { EXPRESSION,
        STATEMENT };

private:
    SizeType Line_ {};
    SizeType Column_ {};

    NodeType NodeType_;

public:
    ASTNode() = default;
    ASTNode(ASTNode const&) = delete;
    ASTNode(ASTNode&&) = delete;

    ASTNode& operator=(ASTNode const&) = delete;
    ASTNode& operator=(ASTNode&&) = delete;

    NodeType getNodeType() const
    {
        return NodeType_;
    }

    void setNodeType(NodeType const t)
    {
        NodeType_ = t;
    }

    SizeType getLine() const
    {
        return Line_;
    }

    SizeType getColumn() const
    {
        return Column_;
    }

    void setLine(SizeType const l)
    {
        Line_ = l;
    }

    void setColumn(SizeType const c)
    {
        Column_ = c;
    }

    virtual ~ASTNode() = default;
};

} // namespace ast
} // namespace mylang
