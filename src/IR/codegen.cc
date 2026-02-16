#include "../../include/IR/codegen.hpp"

namespace mylang {
namespace IR {

Value CodeGenerator::eval(ast::ASTNode const* node)
{
    if (!node)
        return Value();

    ast::ASTNode::NodeType type = node->getNodeType();

    switch (type) {
    case ast::ASTNode::NodeType::EXPRESSION: {
        ast::Expr const* expr = dynamic_cast<ast::Expr const*>(node);
        if (!expr)
            return Value();

        ast::Expr::Kind kind = expr->getKind();
        switch (kind) {
        case ast::Expr::Kind::ASSIGNMENT: {
            ast::AssignmentExpr const* assignment_expr = dynamic_cast<ast::AssignmentExpr const*>(expr);
            if (!assignment_expr)
                return Value();

            // Get the target name
            ast::NameExpr const* target = dynamic_cast<ast::NameExpr const*>(assignment_expr->getTarget());
            if (!target)
                throw std::runtime_error("Invalid assignment target");

            // Evaluate the value
            Value value = eval(assignment_expr->getValue());

            // Store in environment
            Env_->define(target->getValue(), value);

            return value;
        }

        case ast::Expr::Kind::BINARY: {
            ast::BinaryExpr const* binary_expr = dynamic_cast<ast::BinaryExpr const*>(expr);
            if (!binary_expr)
                return Value();

            Value lhs = eval(binary_expr->getLeft());
            Value rhs = eval(binary_expr->getRight());

            tok::TokenType op = binary_expr->getOperator();

            switch (op) {
            case tok::TokenType::OP_PLUS:
                return lhs + rhs;
            case tok::TokenType::OP_MINUS:
                return lhs - rhs;
            case tok::TokenType::OP_STAR:
                return lhs * rhs;
            case tok::TokenType::OP_SLASH:
                return lhs / rhs; // division by zero is handled in Value
            case tok::TokenType::OP_EQ:
                return lhs == rhs;
            case tok::TokenType::OP_GT:
                return lhs > rhs;
            case tok::TokenType::OP_GTE:
                return lhs >= rhs;
            case tok::TokenType::OP_LT:
                return lhs < rhs;
            case tok::TokenType::OP_LTE:
                return lhs <= rhs;
            default:
                throw std::runtime_error("Unknown binary operator");
            }
        }

        case ast::Expr::Kind::CALL: {
            ast::CallExpr const* call_expr = dynamic_cast<ast::CallExpr const*>(expr);
            if (!call_expr)
                return Value();

            // Get function name
            ast::NameExpr* name_expr = dynamic_cast<ast::NameExpr*>(call_expr->getCallee());
            if (!name_expr)
                throw std::runtime_error("Invalid function call");

            StringRef func_name = name_expr->getValue();

            // Evaluate all arguments FIRST (eager evaluation)
            std::vector<Value> args;
            for (auto const* arg_expr : call_expr->getArgs())
                args.push_back(eval(arg_expr));

            // Check if it's a built-in function
            // if (builtins_.find(func_name.toUtf8()) != builtins_.end())
            //    return builtins_[func_name](args);

            // Check if it's a user-defined function
            if (Env_->exists(func_name)) {
                Value funcValue = Env_->get(func_name);
                if (funcValue.isFunction())
                    return callUserFunction(funcValue, args);
            }

            throw std::runtime_error("Undefined function: " + std::string(func_name.data()));
        }

        case ast::Expr::Kind::LIST: {
            ast::ListExpr const* list_expr = dynamic_cast<ast::ListExpr const*>(expr);
            if (!list_expr || list_expr->isEmpty())
                return Value(); // or return empty list

            std::vector<Value> evaluated_elems;
            for (ast::Expr const* elem : list_expr->getElements())
                evaluated_elems.push_back(eval(elem));

            // Assuming you have a way to create a list Value
            return Value::makeList(evaluated_elems);
        }

        case ast::Expr::Kind::LITERAL: {
            ast::LiteralExpr const* literal_expr = dynamic_cast<ast::LiteralExpr const*>(expr);
            if (!literal_expr)
                return Value();

            return Value(literal_expr->getValue());
        }

        case ast::Expr::Kind::NAME: {
            ast::NameExpr const* name_expr = dynamic_cast<ast::NameExpr const*>(expr);
            if (!name_expr)
                return Value();

            StringRef name = name_expr->getValue();
            if (!Env_->exists(name))
                throw std::runtime_error("Undefined variable: " + std::string(name.data()));

            return Env_->get(name);
        }

        case ast::Expr::Kind::UNARY: {
            ast::UnaryExpr const* unary_expr = dynamic_cast<ast::UnaryExpr const*>(expr);
            if (!unary_expr)
                return Value();

            Value operand = eval(unary_expr->getOperand());
            tok::TokenType op = unary_expr->getOperator();

            switch (op) {
            case tok::TokenType::OP_MINUS:
                return -operand;
            case tok::TokenType::KW_NOT: // logical NOT
                return !operand;
            default:
                throw std::runtime_error("Unknown unary operator");
            }
        }

        case ast::Expr::Kind::INVALID:
            throw std::runtime_error("Invalid expression");

        default:
            throw std::runtime_error("Unknown expression kind");
        }
    }

    case ast::ASTNode::NodeType::STATEMENT:
        // Handle statements here
        throw std::runtime_error("Statement evaluation not implemented");

    default:
        throw std::runtime_error("Unknown AST node type");
    }
}

Value CodeGenerator::callUserFunction(Value& func_value, std::vector<Value> const& args)
{
    Value::Function* func = func_value.asFunction();

    // Check argument count
    if (args.size() != func->params.size())
        throw std::runtime_error("Expected " + std::to_string(func->params.size()) + " arguments but got " + std::to_string(args.size()));

    // Create new environment for function execution
    // Use closure environment as parent (for lexical scoping)
    Environment* func_env = func->closure;

    // Bind parameters to arguments
    for (size_t i = 0; i < args.size(); ++i)
        func_env->define(func->params[i], args[i]);

    // Save current environment and switch to function environment
    auto previousEnv = Env_;
    Env_ = func_env;

    // Execute function body
    Value result;
    try {
        result = eval(func->body);
    } catch (...) {
        Env_ = previousEnv; // Restore environment before rethrowing
        throw;
    }

    // Restore previous environment
    Env_ = previousEnv;

    return result;
}

}
}
