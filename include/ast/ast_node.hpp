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
    std::size_t Line_ {0};
    std::size_t Column_ {0};

    NodeType NodeType_;

public:
    ASTNode() = default;
    ASTNode(ASTNode const&) = delete;
    ASTNode(ASTNode&&) = delete;

    virtual ASTNode& operator=(ASTNode const&) = delete;
    virtual ASTNode& operator=(ASTNode&&) = delete;

    virtual NodeType getNodeType() const
    {
        return NodeType_;
    }

    virtual void setNodeType(NodeType const t)
    {
        NodeType_ = t;
    }

    virtual std::size_t getLine() const
    {
        return Line_;
    }

    virtual std::size_t getColumn() const
    {
        return Column_;
    }

    virtual void setLine(std::size_t const l)
    {
        Line_ = l;
    }

    virtual void setColumn(std::size_t const c)
    {
        Column_ = c;
    }

    virtual ~ASTNode() = default;
};

} // namespace ast
} // namespace mylang
