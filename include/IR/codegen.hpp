#pragma once

#include "../IR/value.hpp"
#include "../parser/ast/ast.hpp"

#include <builtins.h>
#include <functional>


namespace mylang {
namespace IR {

using namespace parser;
using namespace runtime;

class CodeGenerator
{
 public:
  Value eval(const ast::ASTNode* node)
  {
    if (!node)
      return Value();

    ast::ASTNode::NodeType type = node->getNodeType();

    switch (type)
    {
    case ast::ASTNode::NodeType::EXPRESSION : {
      const ast::Expr* expr = dynamic_cast<const ast::Expr*>(node);
      if (!expr)
        return Value();

      ast::Expr::Kind kind = expr->getKind();
      switch (kind)
      {
      case ast::Expr::Kind::ASSIGNMENT : {
        const ast::AssignmentExpr* assignment_expr = dynamic_cast<const ast::AssignmentExpr*>(expr);
        if (!assignment_expr)
          return Value();

        // Get the target name
        const ast::NameExpr* target = dynamic_cast<const ast::NameExpr*>(assignment_expr->getTarget());
        if (!target)
          throw std::runtime_error("Invalid assignment target");

        // Evaluate the value
        Value value = eval(assignment_expr->getValue());

        // Store in environment
        Env_->define(target->getValue(), value);

        return value;
      }

      case ast::Expr::Kind::BINARY : {
        const ast::BinaryExpr* binary_expr = dynamic_cast<const ast::BinaryExpr*>(expr);
        if (!binary_expr)
          return Value();

        Value lhs = eval(binary_expr->getLeft());
        Value rhs = eval(binary_expr->getRight());

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
          return Value();

        // Get function name
        ast::NameExpr* name_expr = dynamic_cast<ast::NameExpr*>(call_expr->getCallee());
        if (!name_expr)
          throw std::runtime_error("Invalid function call");

        StringRef func_name = name_expr->getValue();

        // Evaluate all arguments FIRST (eager evaluation)
        std::vector<Value> args;
        for (const auto* arg_expr : call_expr->getArgs())
          args.push_back(eval(arg_expr));

        // Check if it's a built-in function
        // if (builtins_.find(func_name.toUtf8()) != builtins_.end())
        //    return builtins_[func_name](args);

        // Check if it's a user-defined function
        if (Env_->exists(func_name))
        {
          Value funcValue = Env_->get(func_name);
          if (funcValue.isFunction())
            return callUserFunction(funcValue, args);
        }

        throw std::runtime_error("Undefined function: " + func_name.toUtf8());
      }

      case ast::Expr::Kind::LIST : {
        const ast::ListExpr* list_expr = dynamic_cast<const ast::ListExpr*>(expr);
        if (!list_expr || list_expr->isEmpty())
          return Value();  // or return empty list

        std::vector<Value> evaluated_elems;
        for (const ast::Expr* elem : list_expr->getElements())
          evaluated_elems.push_back(eval(elem));

        // Assuming you have a way to create a list Value
        return Value::makeList(evaluated_elems);
      }

      case ast::Expr::Kind::LITERAL : {
        const ast::LiteralExpr* literal_expr = dynamic_cast<const ast::LiteralExpr*>(expr);
        if (!literal_expr)
          return Value();

        return Value(literal_expr->getValue());
      }

      case ast::Expr::Kind::NAME : {
        const ast::NameExpr* name_expr = dynamic_cast<const ast::NameExpr*>(expr);
        if (!name_expr)
          return Value();

        StringRef name = name_expr->getValue();
        if (!Env_->exists(name))
          throw std::runtime_error("Undefined variable: " + name.toUtf8());

        return Env_->get(name);
      }

      case ast::Expr::Kind::UNARY : {
        const ast::UnaryExpr* unary_expr = dynamic_cast<const ast::UnaryExpr*>(expr);
        if (!unary_expr)
          return Value();

        Value          operand = eval(unary_expr->getOperand());
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
  Environment* Env_;

  Value callUserFunction(Value& func_value, const std::vector<Value>& args)
  {
    auto& func = func_value.asFunction();

    // Check argument count
    if (args.size() != func.params.size())
      throw std::runtime_error("Expected " + std::to_string(func.params.size()) + " arguments but got " + std::to_string(args.size()));

    // Create new environment for function execution
    // Use closure environment as parent (for lexical scoping)
    auto* func_env = &func.closure;

    // Bind parameters to arguments
    for (size_t i = 0; i < args.size(); ++i)
    {
      func_env->define(func.params[i], args[i]);
    }

    // Save current environment and switch to function environment
    auto previousEnv = Env_;
    Env_             = func_env;

    // Execute function body
    Value result;
    try
    {
      result = eval(func.body);  /// TODO: define the function closure in a clear manner ../runtime/object/value.hpp
    } catch (...)
    {
      Env_ = previousEnv;  // Restore environment before rethrowing
      throw;
    }

    // Restore previous environment
    Env_ = previousEnv;

    return result;
  }
};

}
}
