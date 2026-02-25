#pragma once

#include "../ast/ast.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace mylang {
namespace parser {

// Advanced AST optimizer with multiple passes
class ASTOptimizer {
public:
    struct OptimizationStats {
        size_t ConstantFolds { 0 };
        size_t DeadCodeEliminations { 0 };
        size_t CommonSubexprEliminations { 0 };
        size_t LoopInvariants { 0 };
        size_t StrengthReductions { 0 };
    };

private:
    OptimizationStats Stats_;

    // Constant folding evaluator

public:
    std::optional<double> evaluateConstant(ast::Expr const* expr);

    // Pass 1: Constant Folding
    ast::Expr* optimizeConstantFolding(ast::Expr* expr);

    // Pass 2: Dead Code Elimination
    ast::Stmt* eliminateDeadCode(ast::Stmt* stmt);

    // Pass 3: Common Subexpression Elimination
    class CSEPass {
    private:
        std::unordered_map<StringRef, StringRef, StringRefHash, StringRefEqual> ExprCache_;
        int32_t TempCounter_ = 0;

    public:
        StringRef exprToString(ast::Expr const* expr);

        StringRef getTempVar();

        std::optional<StringRef> findCSE(ast::Expr const* expr);

        void recordExpr(ast::Expr const* expr, StringRef const& var);
    };

    // Pass 4: Loop Invariant Code Motion
    bool isLoopInvariant(ast::Expr const* expr, std::unordered_set<StringRef, StringRefHash, StringRefEqual> const& loopVars);

    // Main optimization pipeline
    std::vector<ast::Stmt*> optimize(std::vector<ast::Stmt*> statements, int32_t level = 2);

    OptimizationStats const& getStats() const;

    void printStats() const;
};

}
}
