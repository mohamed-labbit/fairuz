#pragma once

#include "../parser/ast/ast.hpp"
#include "../runtime/object/object.hpp"
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
      ast::Expr* expr = dynamic_cast<ast::Expr*>(node);
      if (!expr)
        return object::Value();
      ast::Expr::Kind kind = expr->getKind();
      switch (kind)
      {
      case ast::Expr::Kind::ASSIGNMENT : {
        ast::AssignmentExpr* assignment_expr = dynamic_cast<ast::AssignmentExpr*>(expr);
        if (!assignment_expr)
          return object::Value();

        object::Value target = eval(assignment_expr->getTarget());
        object::Value value  = eval(assignment_expr->getValue());

        return value;
      }
      break;
      case ast::Expr::Kind::BINARY : {
        ast::BinaryExpr* binary_expr = dynamic_cast<ast::BinaryExpr*>(expr);
        if (!binary_expr)
          return object::Value();

        object::Value left  = eval(binary_expr->getLeft());
        object::Value right = eval(binary_expr->getRight());

        tok::TokenType tt = binary_expr->getOperator();

        switch (tt)
        {
        case tok::TokenType::OP_PLUS :
          return left + right;
        case tok::TokenType::OP_MINUS :
          return left - right;
        case tok::TokenType::OP_STAR :
          return left * right;
        case tok::TokenType::OP_SLASH :
          return left / right;
          /// TODO: ...
        }
        // all control paths should be handled in switch
      }
      break;
      case ast::Expr::Kind::CALL : {
        ast::CallExpr* call_expr = dynamic_cast<ast::CallExpr*>(expr);
        if (!call_expr)
          return object::Value();

        // Get function name
        ast::NameExpr* name_expr = dynamic_cast<parser::ast::NameExpr*>(call_expr->getCallee());
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

        throw std::runtime_error("Undefined function: " + func_name);
      }
      break;
      case ast::Expr::Kind::LIST : {
        ast::ListExpr* list_expr = dynamic_cast<ast::ListExpr*>(expr);
        if (!list_expr)
          return object::Value();
      }
      break;
      case ast::Expr::Kind::LITERAL : {
        ast::LiteralExpr* literal_expr = dynamic_cast<ast::LiteralExpr*>(expr);
        if (!literal_expr)
          return object::Value();
      }
      break;
      case ast::Expr::Kind::NAME : {
        ast::NameExpr* name_expr = dynamic_cast<ast::NameExpr*>(expr);
        if (!name_expr)
          return object::Value();
      }
      break;
      case ast::Expr::Kind::UNARY : {
        ast::UnaryExpr* unary_expr = dynamic_cast<ast::UnaryExpr*>(expr);
        if (!unary_expr)
          return object::Value();
      }
      break;
      case ast::Expr::Kind::INVALID : {
      }
      break;
      }
    }
    break;
    case ast::ASTNode::NodeType::STATEMENT :

      break;

    default :
      break;
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
      funcEnv->define(func.parameters[i], args[i]);

    // Save current environment and switch to function environment
    auto previousEnv = env_;
    env_             = funcEnv;

    // Execute function body
    Value result;
    try
    {
      result = evaluate(func.body);
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