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
    SizeType ConstantFolds{0};
    SizeType DeadCodeEliminations{0};
    SizeType CommonSubexprEliminations{0};
    SizeType LoopInvariants{0};
    SizeType StrengthReductions{0};
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
    std::unordered_map<StringRef, StringRef, StringRefHash, StringRefEqual> ExprCache_;
    std::int32_t                             TempCounter_ = 0;

    StringRef exprToString(const ast::Expr* expr);

   public:
    StringRef getTempVar();

    std::optional<StringRef> findCSE(const ast::Expr* expr);

    void recordExpr(const ast::Expr* expr, const StringRef& var);
  };

  // Pass 4: Loop Invariant Code Motion
  bool isLoopInvariant(const ast::Expr* expr, const std::unordered_set<StringRef, StringRefHash, StringRefEqual>& loopVars);

 public:
  // Main optimization pipeline
  std::vector<ast::Stmt*> optimize(std::vector<ast::Stmt*> statements, std::int32_t level = 2);

  const OptimizationStats& getStats() const;

  void printStats() const;
};

}
}