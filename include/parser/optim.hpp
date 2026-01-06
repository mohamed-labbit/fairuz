#pragma once


#include "../../utfcpp/source/utf8.h"
#include "ast/ast.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>


namespace mylang {
namespace parser {
namespace optim {

// Advanced AST optimizer with multiple passes
class ASTOptimizer
{
 private:
  struct OptimizationStats
  {
    std::size_t constantFolds{0};
    std::size_t deadCodeEliminations{0};
    std::size_t commonSubexprEliminations{0};
    std::size_t loopInvariants{0};
    std::size_t strengthReductions{0};
  };

  OptimizationStats stats_;

  // Constant folding evaluator
  std::optional<double> evaluateConstant(const ast::Expr* expr)
  {
    if (!expr) return std::nullopt;

    if (expr->getKind() == ast::Expr::Kind::LITERAL)
    {
      auto* lit = static_cast<const ast::LiteralExpr*>(expr);
      if (lit->getType() == ast::LiteralExpr::Type::NUMBER)
      {
        try
        {
          return std::stod(utf8::utf16to8(lit->getValue()));
        } catch (...) { return std::nullopt; }
      }
    }
    else if (expr->getKind() == ast::Expr::Kind::BINARY)
    {
      auto* bin = static_cast<const ast::BinaryExpr*>(expr);
      auto left = evaluateConstant(bin->getLeft());
      auto right = evaluateConstant(bin->getRight());
      if (!left || !right) return std::nullopt;
      if (bin->getOperator() == lex::tok::TokenType::OP_PLUS) return *left + *right;
      if (bin->getOperator() == lex::tok::TokenType::OP_MINUS) return *left - *right;
      if (bin->getOperator() == lex::tok::TokenType::OP_STAR) return *left * *right;
      if (bin->getOperator() == lex::tok::TokenType::OP_SLASH)
      {
        if (*right == 0.0) return std::nullopt;
        return *left / *right;
      }
      if (bin->getOperator() == lex::tok::TokenType::OP_PERCENT)
      {
        if (*right == 0.0) return std::nullopt;
        return std::fmod(*left, *right);
      }
      if (bin->getOperator() == lex::tok::TokenType::OP_POWER) return std::pow(*left, *right);
    }
    else if (expr->getKind() == ast::Expr::Kind::UNARY)
    {
      const ast::UnaryExpr* un = static_cast<const ast::UnaryExpr*>(expr);
      auto operand = evaluateConstant(dynamic_cast<const ast::UnaryExpr*>(un));
      if (!operand) return std::nullopt;
      if (un->getOperator() == lex::tok::TokenType::OP_PLUS) return *operand;
      if (un->getOperator() == lex::tok::TokenType::OP_MINUS) return -*operand;
    }
    return std::nullopt;
  }

 public:
  // Pass 1: Constant Folding
  ast::Expr* optimizeConstantFolding(ast::Expr* expr)
  {
    if (!expr) return expr;
    // First, optimize children
    if (expr->getKind() == ast::Expr::Kind::BINARY)
    {
      auto* bin = static_cast<ast::BinaryExpr*>(expr);
      bin->setLeft(optimizeConstantFolding(std::move(bin->getLeft())));
      bin->setRight(optimizeConstantFolding(std::move(bin->getRight())));
      // Try to evaluate
      if (auto val = evaluateConstant(expr))
      {
        stats_.constantFolds++;
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, utf8::utf8to16(std::to_string(*val)));
      }
      // Algebraic simplifications
      auto* left = bin->getLeft();
      auto* right = bin->getRight();
      // x + 0 = x, x - 0 = x
      if ((bin->getOperator() == lex::tok::TokenType::OP_PLUS || bin->getOperator() == lex::tok::TokenType::OP_MINUS)
          && right->getKind() == ast::Expr::Kind::LITERAL)
      {
        auto* lit = static_cast<ast::LiteralExpr*>(right);
        if (lit->getValue() == u"0")
        {
          stats_.strengthReductions++;
          return std::move(bin->getLeft());
        }
      }
      // x * 1 = x, x / 1 = x
      if ((bin->getOperator() == lex::tok::TokenType::OP_STAR || bin->getOperator() == lex::tok::TokenType::OP_SLASH)
          && right->getKind() == ast::Expr::Kind::LITERAL)
      {
        auto* lit = static_cast<ast::LiteralExpr*>(right);
        if (lit->getValue() == u"1")
        {
          stats_.strengthReductions++;
          return std::move(bin->getLeft());
        }
      }
      // x * 0 = 0
      if (bin->getOperator() == lex::tok::TokenType::OP_STAR && right->getKind() == ast::Expr::Kind::LITERAL)
      {
        auto* lit = static_cast<ast::LiteralExpr*>(right);
        if (lit->getValue() == u"0")
        {
          stats_.strengthReductions++;
          return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, u"0");
        }
      }
      // x * 2 = x + x (strength reduction)
      if (bin->getOperator() == lex::tok::TokenType::OP_STAR && right->getKind() == ast::Expr::Kind::LITERAL)
      {
        auto* lit = static_cast<ast::LiteralExpr*>(right);
        if (lit->getValue() == u"2")
        {
          stats_.strengthReductions++;
          // Clone left expression
          // auto leftClone = std::make_unique<ast::NameExpr>(static_cast<ast::NameExpr*>(left)->getValue());
          auto* leftClone = ast::AST_allocator.make<ast::NameExpr>(static_cast<ast::NameExpr*>(left)->getValue());
          return ast::AST_allocator.make<ast::BinaryExpr>(bin->getLeft(), leftClone, lex::tok::TokenType::OP_PLUS);
        }
      }
      // x - x = 0
      if (bin->getOperator() == lex::tok::TokenType::OP_MINUS && left->getKind() == ast::Expr::Kind::NAME
          && right->getKind() == ast::Expr::Kind::NAME)
      {
        auto* lname = static_cast<ast::NameExpr*>(left);
        auto* rname = static_cast<ast::NameExpr*>(right);
        if (lname->getValue() == rname->getValue())
        {
          stats_.strengthReductions++;
          return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, u"0");
        }
      }
    }
    /// @todo: ???
    else if (expr->getKind() == ast::Expr::Kind::UNARY)
    {
      auto* un = static_cast<ast::UnaryExpr*>(expr);
      un = static_cast<ast::UnaryExpr*>(optimizeConstantFolding(un));

      if (auto val = evaluateConstant(expr))
      {
        stats_.constantFolds++;
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, utf8::utf8to16(std::to_string(*val)));
      }
      // Double negation: --x = x
      if (un->getOperator() == lex::tok::TokenType::OP_MINUS && un->getKind() == ast::Expr::Kind::UNARY)
      {
        ast::UnaryExpr* innerUn = static_cast<ast::UnaryExpr*>(un);
        if (innerUn->getOperator() == lex::tok::TokenType::OP_MINUS)
        {
          stats_.strengthReductions++;
          return dynamic_cast<ast::Expr*>(innerUn);
        }
      }
    }
    else if (expr->getKind() == ast::Expr::Kind::CALL)
    {
      auto* call = static_cast<ast::CallExpr*>(expr);
      for (auto& arg : call->getArgsMutable()) arg = optimizeConstantFolding(std::move(arg));
    }
    else if (expr->getKind() == ast::Expr::Kind::LIST)
    {
      auto* list = static_cast<ast::ListExpr*>(expr);
      for (auto& elem : list->getElementsMutable()) elem = optimizeConstantFolding(std::move(elem));
    }
    /*
    else if (expr->kind == ast::Expr::Kind::TERNARY)
    {
      auto* tern = static_cast<ast::TernaryExpr*>(expr.get());
      tern->condition = optimizeConstantFolding(std::move(tern->condition));
      tern->trueExpr = optimizeConstantFolding(std::move(tern->trueExpr));
      // Constant ternary evaluation
      if (tern->condition->kind == ast::Expr::Kind::LITERAL)
      {
        auto* lit = static_cast<ast::LiteralExpr*>(tern->condition.get());
        if (lit->litType == ast::LiteralExpr::Type::BOOLEAN)
        {
          stats_.constantFolds++;
          return lit->value == u"true" ? std::move(tern->trueExpr) : std::move(tern->falseExpr);
        }
      }
    }
    */
    return expr;
  }

  // Pass 2: Dead Code Elimination
  ast::Stmt* eliminateDeadCode(ast::Stmt* stmt)
  {
    if (!stmt) return stmt;
    if (stmt->getKind() == ast::Stmt::Kind::IF)
    {
      auto* ifStmt = static_cast<ast::IfStmt*>(stmt);
      // Constant condition elimination
      if (ifStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL)
      {
        auto* lit = static_cast<ast::LiteralExpr*>(ifStmt->getCondition());
        if (lit->getType() == ast::LiteralExpr::Type::BOOLEAN)
        {
          stats_.deadCodeEliminations++;
          if (lit->getValue() == u"true")
            // Return then block as block statement
            return dynamic_cast<ast::Stmt*>(ifStmt->getThenBlock());
          else
            // Return else block or nothing
            if (!ifStmt->getElseBlock()->getStatements().empty()) return dynamic_cast<ast::Stmt*>(ifStmt->getElseBlock());
          /// @todo: return std::make_unique<Stmt*>();
        }
      }
      // Recursively eliminate in blocks
      std::vector<ast::Stmt*> newThenStmts;
      std::vector<ast::Stmt*> newElseStmts;
      for (auto& s : ifStmt->getThenBlock()->getStatements())
        if (auto opt = eliminateDeadCode(std::move(s))) newThenStmts.push_back(std::move(opt));
      for (auto& s : ifStmt->getElseBlock()->getStatements())
        if (auto opt = eliminateDeadCode(std::move(s))) newElseStmts.push_back(std::move(opt));
      ast::BlockStmt* newThen = ast::AST_allocator.make<ast::BlockStmt>(newThenStmts);
      ast::BlockStmt* newElse = ast::AST_allocator.make<ast::BlockStmt>(newElseStmts);
      ifStmt->setThenBlock(newThen);
      ifStmt->setElseBlock(newElse);
    }
    else if (stmt->getKind() == ast::Stmt::Kind::WHILE)
    {
      auto* whileStmt = static_cast<ast::WhileStmt*>(stmt);
      // Infinite loop with false condition
      if (whileStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL)
      {
        auto* lit = static_cast<ast::LiteralExpr*>(whileStmt->getCondition());
        if (lit->getType() == ast::LiteralExpr::Type::BOOLEAN && lit->getValue() == u"false") stats_.deadCodeEliminations++;
        /// @todo: return std::make_unique<PassStmt>();
      }
      std::vector<ast::Stmt*> newBody;
      for (auto& s : whileStmt->getBlock()->getStatements())
        if (auto opt = eliminateDeadCode(std::move(s))) newBody.push_back(std::move(opt));
      whileStmt->getBlockMutable()->setStatements(newBody);
    }
    else if (stmt->getKind() == ast::Stmt::Kind::FOR)
    {
      auto* forStmt = static_cast<ast::ForStmt*>(stmt);

      std::vector<ast::Stmt*> newBodyStmts;
      for (auto& s : forStmt->getBlock()->getStatements())
        if (auto opt = eliminateDeadCode(s)) newBodyStmts.push_back(std::move(opt));
      auto* newBody = ast::AST_allocator.make<ast::BlockStmt>(newBodyStmts);
      forStmt->setBlock(newBody);
    }
    else if (stmt->getKind() == ast::Stmt::Kind::FUNC)
    {
      auto* funcDef = static_cast<ast::FunctionDef*>(stmt);
      std::vector<ast::Stmt*> newBodyStmts;
      bool seenReturn = false;
      for (auto& s : funcDef->getBody()->getStatements())
      {
        if (seenReturn)
        {
          stats_.deadCodeEliminations++;
          continue;  // Skip statements after return
        }
        if (s->getKind() == ast::Stmt::Kind::RETURN) seenReturn = true;
        if (auto opt = eliminateDeadCode(std::move(s))) newBodyStmts.push_back(std::move(opt));
      }
      auto* newBody = ast::AST_allocator.make<ast::BlockStmt>(newBodyStmts);
      funcDef->setBody(newBody);
    }

    return stmt;
  }

  // Pass 3: Common Subexpression Elimination
  class CSEPass
  {
   private:
    std::unordered_map<string_type, string_type> exprCache;
    std::int32_t tempCounter = 0;

    string_type exprToString(const ast::Expr* expr)
    {
      if (!expr) return u"";

      switch (expr->getKind())
      {
      case ast::Expr::Kind::LITERAL : {
        auto* lit = static_cast<const ast::LiteralExpr*>(expr);
        return lit->getValue();
      }
      case ast::Expr::Kind::NAME : {
        auto* name = static_cast<const ast::NameExpr*>(expr);
        return name->getValue();
      }
      case ast::Expr::Kind::BINARY : {
        auto* bin = static_cast<const ast::BinaryExpr*>(expr);
        return u"(" + exprToString(bin->getLeft()) + u" " + lex::tok::to_string(bin->getOperator()) + u" " + exprToString(bin->getRight()) + u")";
      }
      case ast::Expr::Kind::UNARY : {
        auto* un = static_cast<const ast::UnaryExpr*>(expr);
        return lex::tok::to_string(un->getOperator()) + exprToString(un);
      }
      default : return u"";
      }
    }

   public:
    string_type getTempVar() { return u"__cse_temp_" + utf8::utf8to16(std::to_string(tempCounter++)); }

    std::optional<string_type> findCSE(const ast::Expr* expr)
    {
      string_type exprStr = exprToString(expr);
      if (exprStr.empty()) return std::nullopt;
      auto it = exprCache.find(exprStr);
      if (it != exprCache.end()) return it->second;
      return std::nullopt;
    }

    void recordExpr(const ast::Expr* expr, const string_type& var)
    {
      string_type exprStr = exprToString(expr);
      if (!exprStr.empty()) exprCache[exprStr] = var;
    }
  };

  // Pass 4: Loop Invariant Code Motion
  bool isLoopInvariant(const ast::Expr* expr, const std::unordered_set<string_type>& loopVars)
  {
    if (!expr) return true;
    if (expr->getKind() == ast::Expr::Kind::NAME)
    {
      auto* name = static_cast<const ast::NameExpr*>(expr);
      return !loopVars.count(name->getValue());
    }
    else if (expr->getKind() == ast::Expr::Kind::BINARY)
    {
      auto* bin = static_cast<const ast::BinaryExpr*>(expr);
      return isLoopInvariant(bin->getLeft(), loopVars) && isLoopInvariant(bin->getRight(), loopVars);
    }
    else if (expr->getKind() == ast::Expr::Kind::UNARY)
    {
      auto* un = static_cast<const ast::UnaryExpr*>(expr);
      return isLoopInvariant(un, loopVars);
    }
    else if (expr->getKind() == ast::Expr::Kind::LITERAL)
      return true;
    return false;
  }

 public:
  // Main optimization pipeline
  std::vector<ast::Stmt*> optimize(std::vector<ast::Stmt*> statements, std::int32_t level = 2)
  {
    std::vector<ast::Stmt*> result;
    for (auto& stmt : statements)
    {
      // Apply optimizations based on level
      if (level >= 1)
      {
        // O1: Basic optimizations
        if (stmt->getKind() == ast::Stmt::Kind::ASSIGNMENT)
        {
          auto* assign = static_cast<ast::AssignmentStmt*>(stmt);
          assign->setValue(optimizeConstantFolding(assign->getValue()));
        }
        else if (stmt->getKind() == ast::Stmt::Kind::EXPR)
        {
          auto* exprStmt = static_cast<ast::ExprStmt*>(stmt);
          exprStmt->setExpr(optimizeConstantFolding(exprStmt->getExpr()));
        }
      }
      if (level >= 2)
        // O2: Dead code elimination
        stmt = eliminateDeadCode(std::move(stmt));
      if (stmt) result.push_back(std::move(stmt));
    }
    return result;
  }

  const OptimizationStats& getStats() const { return stats_; }

  void printStats() const
  {
    std::cout << "\n=== Optimization Statistics ===\n";
    std::cout << "Constant folds: " << stats_.constantFolds << "\n";
    std::cout << "Dead code eliminations: " << stats_.deadCodeEliminations << "\n";
    std::cout << "Strength reductions: " << stats_.strengthReductions << "\n";
    std::cout << "Common subexpr eliminations: " << stats_.commonSubexprEliminations << "\n";
    std::cout << "Loop invariants moved: " << stats_.loopInvariants << "\n";
    std::cout << "Total optimizations: "
              << (stats_.constantFolds + stats_.deadCodeEliminations + stats_.strengthReductions + stats_.commonSubexprEliminations
                  + stats_.loopInvariants)
              << "\n";
  }
};

}
}
}