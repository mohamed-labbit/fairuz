#pragma once

#include "../macros.hpp"
#include <utility>

namespace mylang {
namespace ast {

class ASTNode {
public:
    enum NodeType {
        EXPRESSION,
        STATEMENT
    };

private:
    uint32_t Line_ { 0 };
    uint32_t Column_ { 0 };

    NodeType NodeType_;

public:
    ASTNode() = default;
    ASTNode(ASTNode const&) = delete;
    ASTNode(ASTNode&&) = delete;

    ASTNode& operator=(ASTNode const&) = delete;
    ASTNode& operator=(ASTNode&&) = delete;

    virtual NodeType getNodeType() const;

    virtual uint32_t getLine() const;

    virtual uint32_t getColumn() const;

    virtual void setLine(uint32_t const l);

    virtual void setColumn(uint32_t const c);

    ~ASTNode() = default;
};

} // namespace ast
} // namespace mylang
