#pragma once

#include "lex/lexer.h"
#include "lex/token.h"
#include <memory>
#include <stdio.h>
#include <string>
#include <vector>

namespace mylang {
namespace parser {
namespace ast {

using string_type = std::u16string;

enum class ASTNodeType : int {
    // Program structure
    PROGRAM,
    STATEMENT_LIST,

    // Statements
    ASSIGNMENT,
    IF_STATEMENT,
    WHILE_STATEMENT,
    RETURN_STATEMENT,
    EXPRESSION_STATEMENT,

    // Expressions
    BINARY_OP,
    UNARY_OP,
    LITERAL,
    IDENTIFIER,
    FUNCTION_CALL,

    // Literals
    NUMBER,
    STRING,
    BOOLEAN,

    // Special
    EMPTY,
    ERROR
};

// Helper to convert enum to string for debugging
inline const char* ast_node_type_to_string(ASTNodeType type)
{
    switch (type)
    {
    case ASTNodeType::PROGRAM :
        return "PROGRAM";
    case ASTNodeType::STATEMENT_LIST :
        return "STATEMENT_LIST";
    case ASTNodeType::ASSIGNMENT :
        return "ASSIGNMENT";
    case ASTNodeType::BINARY_OP :
        return "BINARY_OP";
    case ASTNodeType::IDENTIFIER :
        return "IDENTIFIER";
    case ASTNodeType::NUMBER :
        return "NUMBER";
    // ... add all cases
    default :
        return "UNKNOWN";
    }
}

struct ASTNode
{
   private:
    ASTNodeType type_;
    lex::tok::Token token_;
    std::vector<std::unique_ptr<ASTNode>> children_;

   public:
    // Constructors
    explicit ASTNode(ASTNodeType type, lex::tok::Token token = {}) :
        type_(type),
        token_(std::move(token)),
        children_()
    {
    }

    ASTNode(ASTNodeType type, lex::tok::Token token, std::vector<std::unique_ptr<ASTNode>> children) :
        type_(type),
        token_(std::move(token)),
        children_(std::move(children))
    {
    }

    virtual ~ASTNode() = default;

    // Delete copy operations
    ASTNode(const ASTNode&) = delete;
    ASTNode& operator=(const ASTNode&) = delete;

    // Default move operations
    ASTNode(ASTNode&&) = default;
    ASTNode& operator=(ASTNode&&) = default;

    // Accessors
    ASTNodeType type() const { return type_; }
    const lex::tok::Token& token() const { return token_; }
    lex::tok::Token& token() { return token_; }

    const std::vector<std::unique_ptr<ASTNode>>& children() const { return children_; }

    size_t num_children() const { return children_.size(); }

    const ASTNode* child(size_t i) const { return i < children_.size() ? children_[i].get() : nullptr; }

    ASTNode* child(size_t i) { return i < children_.size() ? children_[i].get() : nullptr; }

    // Mutators
    void add_child(std::unique_ptr<ASTNode> child) { children_.push_back(std::move(child)); }

    void set_token(lex::tok::Token token) { token_ = std::move(token); }

    // Utility
    bool is_leaf() const { return children_.empty(); }

    // Debug printing (optional but useful)
    void print(int indent = 0) const
    {
        for (int i = 0; i < indent; ++i)
            std::cout << "  ";
        std::cout << ast_node_type_to_string(type_);
        if (!token_.lexeme().empty())
        {
            // Convert u16string to string for printing
            std::string lexeme;
            for (auto c : token_.lexeme())
            {
                lexeme += static_cast<char>(c);
            }
            std::cout << " (" << lexeme << ")";
        }
        std::cout << "\n";

        for (const auto& child : children_)
        {
            if (child)
            {
                child->print(indent + 1);
            }
        }
    }
};

// Helper factory functions
inline std::unique_ptr<ASTNode> make_ast_node(ASTNodeType type, lex::tok::Token token = {})
{
    return std::make_unique<ASTNode>(type, std::move(token));
}

inline std::unique_ptr<ASTNode> make_binary_op(lex::tok::Token op_token, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
{
    auto node = make_ast_node(ASTNodeType::BINARY_OP, std::move(op_token));
    node->add_child(std::move(left));
    node->add_child(std::move(right));
    return node;
}


}  // namespace ast
}  // namespace parser
}  // namespace mylang