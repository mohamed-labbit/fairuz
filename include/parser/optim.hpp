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

// Advanced AST optimizer with multiple passes
class ASTOptimizer
{
 private:
  struct OptimizationStats
  {
    std::size_t ConstantFolds{0};
    std::size_t DeadCodeEliminations{0};
    std::size_t CommonSubexprEliminations{0};
    std::size_t LoopInvariants{0};
    std::size_t StrengthReductions{0};
  };

  OptimizationStats Stats_;

  // Constant folding evaluator
  std::optional<double> evaluateConstant(const ast::Expr* expr);

 public:
  // Pass 1: Constant Folding
  ast::Expr* optimizeConstantFolding(ast::Expr* expr);

  // Pass 2: Dead Code Elimination
  ast::Stmt* eliminateDeadCode(ast::Stmt* stmt);

  // Pass 3: Common Subexpression Elimination
  class CSEPass
  {
   private:
    std::unordered_map<StringType, StringType> ExprCache_;
    std::int32_t TempCounter_ = 0;

    StringType exprToString(const ast::Expr* expr);

   public:
    StringType getTempVar();

    std::optional<StringType> findCSE(const ast::Expr* expr);

    void recordExpr(const ast::Expr* expr, const StringType& var);
  };

  // Pass 4: Loop Invariant Code Motion
  bool isLoopInvariant(const ast::Expr* expr, const std::unordered_set<StringType>& loopVars);

 public:
  // Main optimization pipeline
  std::vector<ast::Stmt*> optimize(std::vector<ast::Stmt*> statements, std::int32_t level = 2);

  const OptimizationStats& getStats() const;

  void printStats() const;
};

}
}