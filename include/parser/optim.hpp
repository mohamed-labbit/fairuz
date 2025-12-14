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
        std::size_t constantFolds = 0;
        std::size_t deadCodeEliminations = 0;
        std::size_t commonSubexprEliminations = 0;
        std::size_t loopInvariants = 0;
        std::size_t strengthReductions = 0;
    };

    OptimizationStats stats_;

    // Constant folding evaluator
    std::optional<double> evaluateConstant(const ast::Expr* expr)
    {
        if (!expr)
            return std::nullopt;

        if (expr->kind == ast::Expr::Kind::LITERAL)
        {
            auto* lit = static_cast<const ast::LiteralExpr*>(expr);
            if (lit->litType == ast::LiteralExpr::Type::NUMBER)
            {
                try
                {
                    return std::stod(utf8::utf16to8(lit->value));
                } catch (...)
                {
                    return std::nullopt;
                }
            }
        }
        else if (expr->kind == ast::Expr::Kind::BINARY)
        {
            auto* bin = static_cast<const ast::BinaryExpr*>(expr);
            auto left = evaluateConstant(bin->left.get());
            auto right = evaluateConstant(bin->right.get());
            if (!left || !right)
                return std::nullopt;
            if (bin->op == u"+")
                return *left + *right;
            if (bin->op == u"-")
                return *left - *right;
            if (bin->op == u"*")
                return *left * *right;
            if (bin->op == u"/")
            {
                if (*right == 0.0)
                    return std::nullopt;
                return *left / *right;
            }
            if (bin->op == u"%")
            {
                if (*right == 0.0)
                    return std::nullopt;
                return std::fmod(*left, *right);
            }
            if (bin->op == u"**")
                return std::pow(*left, *right);
        }
        else if (expr->kind == ast::Expr::Kind::UNARY)
        {
            auto* un = static_cast<const ast::UnaryExpr*>(expr);
            auto operand = evaluateConstant(un->operand.get());
            if (!operand)
                return std::nullopt;
            if (un->op == u"+")
                return *operand;
            if (un->op == u"-")
                return -*operand;
        }
        return std::nullopt;
    }

   public:
    // Pass 1: Constant Folding
    ast::ExprPtr optimizeConstantFolding(ast::ExprPtr expr)
    {
        if (!expr)
            return expr;

        // First, optimize children
        if (expr->kind == ast::Expr::Kind::BINARY)
        {
            auto* bin = static_cast<ast::BinaryExpr*>(expr.get());
            bin->left = optimizeConstantFolding(std::move(bin->left));
            bin->right = optimizeConstantFolding(std::move(bin->right));
            // Try to evaluate
            if (auto val = evaluateConstant(expr.get()))
            {
                stats_.constantFolds++;
                return std::make_unique<ast::LiteralExpr>(
                  ast::LiteralExpr::Type::NUMBER, utf8::utf8to16(std::to_string(*val)));
            }
            // Algebraic simplifications
            auto* left = bin->left.get();
            auto* right = bin->right.get();
            // x + 0 = x, x - 0 = x
            if ((bin->op == u"+" || bin->op == u"-") && right->kind == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(right);
                if (lit->value == u"0")
                {
                    stats_.strengthReductions++;
                    return std::move(bin->left);
                }
            }
            // x * 1 = x, x / 1 = x
            if ((bin->op == u"*" || bin->op == u"/") && right->kind == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(right);
                if (lit->value == u"1")
                {
                    stats_.strengthReductions++;
                    return std::move(bin->left);
                }
            }
            // x * 0 = 0
            if (bin->op == u"*" && right->kind == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(right);
                if (lit->value == u"0")
                {
                    stats_.strengthReductions++;
                    return std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, u"0");
                }
            }
            // x * 2 = x + x (strength reduction)
            if (bin->op == u"*" && right->kind == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(right);
                if (lit->value == u"2")
                {
                    stats_.strengthReductions++;
                    // Clone left expression
                    auto leftClone = std::make_unique<ast::NameExpr>(static_cast<ast::NameExpr*>(left)->name);
                    return std::make_unique<ast::BinaryExpr>(std::move(bin->left), u"+", std::move(leftClone));
                }
            }
            // x - x = 0
            if (bin->op == u"-" && left->kind == ast::Expr::Kind::NAME && right->kind == ast::Expr::Kind::NAME)
            {
                auto* lname = static_cast<ast::NameExpr*>(left);
                auto* rname = static_cast<ast::NameExpr*>(right);
                if (lname->name == rname->name)
                {
                    stats_.strengthReductions++;
                    return std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, u"0");
                }
            }
        }
        else if (expr->kind == ast::Expr::Kind::UNARY)
        {
            auto* un = static_cast<ast::UnaryExpr*>(expr.get());
            un->operand = optimizeConstantFolding(std::move(un->operand));

            if (auto val = evaluateConstant(expr.get()))
            {
                stats_.constantFolds++;
                return std::make_unique<ast::LiteralExpr>(
                  ast::LiteralExpr::Type::NUMBER, utf8::utf8to16(std::to_string(*val)));
            }
            // Double negation: --x = x
            if (un->op == u"-" && un->operand->kind == ast::Expr::Kind::UNARY)
            {
                auto* innerUn = static_cast<ast::UnaryExpr*>(un->operand.get());
                if (innerUn->op == u"-")
                {
                    stats_.strengthReductions++;
                    return std::move(innerUn->operand);
                }
            }
        }
        else if (expr->kind == ast::Expr::Kind::CALL)
        {
            auto* call = static_cast<ast::CallExpr*>(expr.get());
            for (auto& arg : call->args)
                arg = optimizeConstantFolding(std::move(arg));
        }
        else if (expr->kind == ast::Expr::Kind::LIST)
        {
            auto* list = static_cast<ast::ListExpr*>(expr.get());
            for (auto& elem : list->elements)
                elem = optimizeConstantFolding(std::move(elem));
        }
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

        return expr;
    }

    // Pass 2: Dead Code Elimination
    ast::StmtPtr eliminateDeadCode(ast::StmtPtr stmt)
    {
        if (!stmt)
            return stmt;

        if (stmt->kind == ast::Stmt::Kind::IF)
        {
            auto* ifStmt = static_cast<ast::IfStmt*>(stmt.get());
            // Constant condition elimination
            if (ifStmt->condition->kind == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(ifStmt->condition.get());
                if (lit->litType == ast::LiteralExpr::Type::BOOLEAN)
                {
                    stats_.deadCodeEliminations++;
                    if (lit->value == u"true")
                        // Return then block as block statement
                        return std::make_unique<ast::BlockStmt>(std::move(ifStmt->thenBlock));
                    else
                        // Return else block or nothing
                        if (!ifStmt->elseBlock.empty())
                            return std::make_unique<ast::BlockStmt>(std::move(ifStmt->elseBlock));
                    // TODO : return std::make_unique<StmtPtr>();
                }
            }
            // Recursively eliminate in blocks
            std::vector<ast::StmtPtr> newThen, newElse;
            for (auto& s : ifStmt->thenBlock)
                if (auto opt = eliminateDeadCode(std::move(s)))
                    newThen.push_back(std::move(opt));
            for (auto& s : ifStmt->elseBlock)
                if (auto opt = eliminateDeadCode(std::move(s)))
                    newElse.push_back(std::move(opt));
            ifStmt->thenBlock = std::move(newThen);
            ifStmt->elseBlock = std::move(newElse);
        }
        else if (stmt->kind == ast::Stmt::Kind::WHILE)
        {
            auto* whileStmt = static_cast<ast::WhileStmt*>(stmt.get());
            // Infinite loop with false condition
            if (whileStmt->condition->kind == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(whileStmt->condition.get());
                if (lit->litType == ast::LiteralExpr::Type::BOOLEAN && lit->value == u"false")
                    stats_.deadCodeEliminations++;
                // TODO : return std::make_unique<PassStmt>();
            }
            std::vector<ast::StmtPtr> newBody;
            for (auto& s : whileStmt->body)
                if (auto opt = eliminateDeadCode(std::move(s)))
                    newBody.push_back(std::move(opt));
            whileStmt->body = std::move(newBody);
        }
        else if (stmt->kind == ast::Stmt::Kind::FOR)
        {
            auto* forStmt = static_cast<ast::ForStmt*>(stmt.get());

            std::vector<ast::StmtPtr> newBody;
            for (auto& s : forStmt->body)
                if (auto opt = eliminateDeadCode(std::move(s)))
                    newBody.push_back(std::move(opt));
            forStmt->body = std::move(newBody);
        }
        else if (stmt->kind == ast::Stmt::Kind::FUNCTION_DEF)
        {
            auto* funcDef = static_cast<ast::FunctionDef*>(stmt.get());
            std::vector<ast::StmtPtr> newBody;
            bool seenReturn = false;
            for (auto& s : funcDef->body)
            {
                if (seenReturn)
                {
                    stats_.deadCodeEliminations++;
                    continue;  // Skip statements after return
                }
                if (s->kind == ast::Stmt::Kind::RETURN)
                    seenReturn = true;
                if (auto opt = eliminateDeadCode(std::move(s)))
                    newBody.push_back(std::move(opt));
            }
            funcDef->body = std::move(newBody);
        }

        return stmt;
    }

    // Pass 3: Common Subexpression Elimination
    class CSEPass
    {
       private:
        std::unordered_map<std::u16string, std::u16string> exprCache;
        std::int32_t tempCounter = 0;

        std::u16string exprToString(const ast::Expr* expr)
        {
            if (!expr)
                return u"";

            switch (expr->kind)
            {
            case ast::Expr::Kind::LITERAL : {
                auto* lit = static_cast<const ast::LiteralExpr*>(expr);
                return lit->value;
            }
            case ast::Expr::Kind::NAME : {
                auto* name = static_cast<const ast::NameExpr*>(expr);
                return name->name;
            }
            case ast::Expr::Kind::BINARY : {
                auto* bin = static_cast<const ast::BinaryExpr*>(expr);
                return u"(" + exprToString(bin->left.get()) + u" " + bin->op + u" " + exprToString(bin->right.get())
                  + u")";
            }
            case ast::Expr::Kind::UNARY : {
                auto* un = static_cast<const ast::UnaryExpr*>(expr);
                return un->op + exprToString(un->operand.get());
            }
            default :
                return u"";
            }
        }

       public:
        std::u16string getTempVar() { return u"__cse_temp_" + utf8::utf8to16(std::to_string(tempCounter++)); }

        std::optional<std::u16string> findCSE(const ast::Expr* expr)
        {
            std::u16string exprStr = exprToString(expr);
            if (exprStr.empty())
                return std::nullopt;
            auto it = exprCache.find(exprStr);
            if (it != exprCache.end())
                return it->second;
            return std::nullopt;
        }

        void recordExpr(const ast::Expr* expr, const std::u16string& var)
        {
            std::u16string exprStr = exprToString(expr);
            if (!exprStr.empty())
                exprCache[exprStr] = var;
        }
    };

    // Pass 4: Loop Invariant Code Motion
    bool isLoopInvariant(const ast::Expr* expr, const std::unordered_set<std::u16string>& loopVars)
    {
        if (!expr)
            return true;
        if (expr->kind == ast::Expr::Kind::NAME)
        {
            auto* name = static_cast<const ast::NameExpr*>(expr);
            return !loopVars.count(name->name);
        }
        else if (expr->kind == ast::Expr::Kind::BINARY)
        {
            auto* bin = static_cast<const ast::BinaryExpr*>(expr);
            return isLoopInvariant(bin->left.get(), loopVars) && isLoopInvariant(bin->right.get(), loopVars);
        }
        else if (expr->kind == ast::Expr::Kind::UNARY)
        {
            auto* un = static_cast<const ast::UnaryExpr*>(expr);
            return isLoopInvariant(un->operand.get(), loopVars);
        }
        else if (expr->kind == ast::Expr::Kind::LITERAL)
            return true;
        return false;
    }

   public:
    // Main optimization pipeline
    std::vector<ast::StmtPtr> optimize(std::vector<ast::StmtPtr> statements, std::int32_t level = 2)
    {
        std::vector<ast::StmtPtr> result;
        for (auto& stmt : statements)
        {
            // Apply optimizations based on level
            if (level >= 1)
            {
                // O1: Basic optimizations
                if (stmt->kind == ast::Stmt::Kind::ASSIGNMENT)
                {
                    auto* assign = static_cast<ast::AssignmentStmt*>(stmt.get());
                    assign->value = optimizeConstantFolding(std::move(assign->value));
                }
                else if (stmt->kind == ast::Stmt::Kind::EXPRESSION)
                {
                    auto* exprStmt = static_cast<ast::ExprStmt*>(stmt.get());
                    exprStmt->expression = optimizeConstantFolding(std::move(exprStmt->expression));
                }
            }

            if (level >= 2)
                // O2: Dead code elimination
                stmt = eliminateDeadCode(std::move(stmt));
            if (stmt)
                result.push_back(std::move(stmt));
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
                  << (stats_.constantFolds + stats_.deadCodeEliminations + stats_.strengthReductions
                       + stats_.commonSubexprEliminations + stats_.loopInvariants)
                  << "\n";
    }
};

}
}
}