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
    std::size_t Line_ { 0 };
    std::size_t Column_ { 0 };

    NodeType NodeType_;

public:
    ASTNode() = default;
    ASTNode(ASTNode const&) = delete;
    ASTNode(ASTNode&&) = delete;

    ASTNode& operator=(ASTNode const&) = delete;
    ASTNode& operator=(ASTNode&&) = delete;

    virtual NodeType getNodeType() const;

    virtual std::size_t getLine() const;

    virtual std::size_t getColumn() const;

    virtual void setLine(std::size_t const l);

    virtual void setColumn(std::size_t const c);

    ~ASTNode() = default;
};

} // namespace ast
} // namespace mylang
