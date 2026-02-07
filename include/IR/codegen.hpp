#pragma once

#include "../parser/ast/ast.hpp"
#include "../runtime/object/value.hpp"
#include "env.hpp"

#include <builtins.h>
#include <functional>


namespace mylang {
namespace IR {

using namespace parser;
using namespace runtime;

class CodeGenerator
{
 public:
  object::Value eval(const ast::ASTNode* node)
  {
    if (!node)
      return object::Value();

    ast::ASTNode::NodeType type = node->getNodeType();

    switch (type)
    {
    case ast::ASTNode::NodeType::EXPRESSION : {
      const ast::Expr* expr = dynamic_cast<const ast::Expr*>(node);
      if (!expr)
        return object::Value();

      ast::Expr::Kind kind = expr->getKind();
      switch (kind)
      {
      case ast::Expr::Kind::ASSIGNMENT : {
        const ast::AssignmentExpr* assignment_expr = dynamic_cast<const ast::AssignmentExpr*>(expr);
        if (!assignment_expr)
          return object::Value();

        // Get the target name
        const ast::NameExpr* target = dynamic_cast<const ast::NameExpr*>(assignment_expr->getTarget());
        if (!target)
          throw std::runtime_error("Invalid assignment target");

        // Evaluate the value
        object::Value value = eval(assignment_expr->getValue());

        // Store in environment
        env_->set(target->getValue(), value);

        return value;
      }

      case ast::Expr::Kind::BINARY : {
        const ast::BinaryExpr* binary_expr = dynamic_cast<const ast::BinaryExpr*>(expr);
        if (!binary_expr)
          return object::Value();

        object::Value lhs = eval(binary_expr->getLeft());
        object::Value rhs = eval(binary_expr->getRight());

        tok::TokenType op = binary_expr->getOperator();

        switch (op)
        {
        case tok::TokenType::OP_PLUS : return lhs + rhs;
        case tok::TokenType::OP_MINUS : return lhs - rhs;
        case tok::TokenType::OP_STAR : return lhs * rhs;
        case tok::TokenType::OP_SLASH : return lhs / rhs;  // division by zero is handled in Value
        case tok::TokenType::OP_EQ : return lhs == rhs;
        case tok::TokenType::OP_GT : return lhs > rhs;
        case tok::TokenType::OP_GTE : return lhs >= rhs;
        case tok::TokenType::OP_LT : return lhs < rhs;
        case tok::TokenType::OP_LTE : return lhs <= rhs;
        default : throw std::runtime_error("Unknown binary operator");
        }
      }

      case ast::Expr::Kind::CALL : {
        const ast::CallExpr* call_expr = dynamic_cast<const ast::CallExpr*>(expr);
        if (!call_expr)
          return object::Value();

        // Get function name
        ast::NameExpr* name_expr = dynamic_cast<ast::NameExpr*>(call_expr->getCallee());
        if (!name_expr)
          throw std::runtime_error("Invalid function call");

        StringRef func_name = name_expr->getValue();

        // Evaluate all arguments FIRST (eager evaluation)
        std::vector<object::Value> args;
        for (const auto* arg_expr : call_expr->getArgs())
          args.push_back(eval(arg_expr));

        // Check if it's a built-in function
        // if (builtins_.find(func_name.toUtf8()) != builtins_.end())
        //    return builtins_[func_name](args);

        // Check if it's a user-defined function
        if (env_->exists(func_name))
        {
          object::Value funcValue = env_->get(func_name);
          if (funcValue.isFunction())
            return callUserFunction(funcValue, args);
        }

        throw std::runtime_error("Undefined function: " + func_name.toUtf8());
      }

      case ast::Expr::Kind::LIST : {
        const ast::ListExpr* list_expr = dynamic_cast<const ast::ListExpr*>(expr);
        if (!list_expr || list_expr->isEmpty())
          return object::Value();  // or return empty list

        std::vector<object::Value> evaluated_elems;
        for (const ast::Expr* elem : list_expr->getElements())
          evaluated_elems.push_back(eval(elem));

        // Assuming you have a way to create a list Value
        return object::Value::makeList(evaluated_elems);
      }

      case ast::Expr::Kind::LITERAL : {
        const ast::LiteralExpr* literal_expr = dynamic_cast<const ast::LiteralExpr*>(expr);
        if (!literal_expr)
          return object::Value();

        return object::Value(literal_expr->getValue());
      }

      case ast::Expr::Kind::NAME : {
        const ast::NameExpr* name_expr = dynamic_cast<const ast::NameExpr*>(expr);
        if (!name_expr)
          return object::Value();

        StringRef name = name_expr->getValue();
        if (!env_->exists(name))
          throw std::runtime_error("Undefined variable: " + name.toUtf8());

        return env_->get(name);
      }

      case ast::Expr::Kind::UNARY : {
        const ast::UnaryExpr* unary_expr = dynamic_cast<const ast::UnaryExpr*>(expr);
        if (!unary_expr)
          return object::Value();

        object::Value  operand = eval(unary_expr->getOperand());
        tok::TokenType op      = unary_expr->getOperator();

        switch (op)
        {
        case tok::TokenType::OP_MINUS : return -operand;
        case tok::TokenType::KW_NOT :  // logical NOT
          return !operand;
        default : throw std::runtime_error("Unknown unary operator");
        }
      }

      case ast::Expr::Kind::INVALID : throw std::runtime_error("Invalid expression");

      default : throw std::runtime_error("Unknown expression kind");
      }
    }

    case ast::ASTNode::NodeType::STATEMENT :
      // Handle statements here
      throw std::runtime_error("Statement evaluation not implemented");

    default : throw std::runtime_error("Unknown AST node type");
    }
  }

 private:
  object::Value callUserFunction(const object::Value& funcValue, const std::vector<object::Value>& args)
  {
    const auto& func = funcValue.asFunction();

    // Check argument count
    if (args.size() != func.params.size())
      throw std::runtime_error("Expected " + std::to_string(func.params.size()) + " arguments but got " + std::to_string(args.size()));

    // Create new environment for function execution
    // Use closure environment as parent (for lexical scoping)
    auto funcEnv = std::make_shared<Environment>(func.closure);

    // Bind parameters to arguments
    for (size_t i = 0; i < args.size(); ++i)
      funcEnv->define(func.params[i], args[i]);

    // Save current environment and switch to function environment
    auto previousEnv = env_;
    env_             = funcEnv;

    // Execute function body
    object::Value result;
    try
    {
      result = eval(func.closure);  /// TODO: define the function closure in a clear manner ../runtime/object/value.hpp
    } catch (...)
    {
      env_ = previousEnv;  // Restore environment before rethrowing
      throw;
    }

    // Restore previous environment
    env_ = previousEnv;

    return result;
  }
};

}
}
