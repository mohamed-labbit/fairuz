#pragma once


#include "../../utfcpp/source/utf8.h"
#include "ast.hpp"
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
        unsigned constantFolds_ = 0;
        unsigned deadCodeEliminations_ = 0;
        unsigned commonSubexprEliminations_ = 0;
        unsigned loopInvariants_ = 0;
        unsigned strengthReductions_ = 0;
    };

    OptimizationStats stats_;

    // Constant folding evaluator
    std::optional<double> evaluateConstant(const ast::Expr* expr)
    {
        if (!expr)
        {
            return std::nullopt;
        }

        if (expr->kind_ == ast::Expr::Kind::LITERAL)
        {
            auto* lit = static_cast<const ast::LiteralExpr*>(expr);
            if (lit->litType_ == ast::LiteralExpr::Type::NUMBER)
            {
                try
                {
                    return std::stod(utf8::utf16to8(lit->value_));
                } catch (...)
                {
                    return std::nullopt;
                }
            }
        }
        else if (expr->kind_ == ast::Expr::Kind::BINARY)
        {
            auto* bin = static_cast<const ast::BinaryExpr*>(expr);
            auto left = evaluateConstant(bin->left_.get());
            auto right = evaluateConstant(bin->right_.get());

            if (!left || !right)
            {
                return std::nullopt;
            }

            if (bin->op_ == u"+")
            {
                return *left + *right;
            }

            if (bin->op_ == u"-")
            {
                return *left - *right;
            }

            if (bin->op_ == u"*")
            {
                return *left * *right;
            }

            if (bin->op_ == u"/")
            {
                if (*right == 0.0)
                {
                    return std::nullopt;
                }
                return *left / *right;
            }

            if (bin->op_ == u"%")
            {
                if (*right == 0.0)
                {
                    return std::nullopt;
                }
                return std::fmod(*left, *right);
            }

            if (bin->op_ == u"**")
            {
                return std::pow(*left, *right);
            }
        }
        else if (expr->kind_ == ast::Expr::Kind::UNARY)
        {
            auto* un = static_cast<const ast::UnaryExpr*>(expr);
            auto operand = evaluateConstant(un->operand_.get());

            if (!operand)
            {
                return std::nullopt;
            }

            if (un->op_ == u"+")
            {
                return *operand;
            }

            if (un->op_ == u"-")
            {
                return -*operand;
            }
        }

        return std::nullopt;
    }

   public:
    // Pass 1: Constant Folding
    ast::ExprPtr optimizeConstantFolding(ast::ExprPtr expr)
    {
        if (!expr)
        {
            return expr;
        }

        // First, optimize children
        if (expr->kind_ == ast::Expr::Kind::BINARY)
        {
            auto* bin = static_cast<ast::BinaryExpr*>(expr.get());
            bin->left_ = optimizeConstantFolding(std::move(bin->left_));
            bin->right_ = optimizeConstantFolding(std::move(bin->right_));

            // Try to evaluate
            if (auto val = evaluateConstant(expr.get()))
            {
                stats_.constantFolds_++;
                return std::make_unique<ast::LiteralExpr>(
                  ast::LiteralExpr::Type::NUMBER, utf8::utf8to16(std::to_string(*val)));
            }

            // Algebraic simplifications
            auto* left = bin->left_.get();
            auto* right = bin->right_.get();

            // x + 0 = x, x - 0 = x
            if ((bin->op_ == u"+" || bin->op_ == u"-") && right->kind_ == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(right);
                if (lit->value_ == u"0")
                {
                    stats_.strengthReductions_++;
                    return std::move(bin->left_);
                }
            }

            // x * 1 = x, x / 1 = x
            if ((bin->op_ == u"*" || bin->op_ == u"/") && right->kind_ == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(right);
                if (lit->value_ == u"1")
                {
                    stats_.strengthReductions_++;
                    return std::move(bin->left_);
                }
            }

            // x * 0 = 0
            if (bin->op_ == u"*" && right->kind_ == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(right);
                if (lit->value_ == u"0")
                {
                    stats_.strengthReductions_++;
                    return std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, u"0");
                }
            }

            // x * 2 = x + x (strength reduction)
            if (bin->op_ == u"*" && right->kind_ == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(right);
                if (lit->value_ == u"2")
                {
                    stats_.strengthReductions_++;
                    // Clone left expression
                    auto leftClone = std::make_unique<ast::NameExpr>(static_cast<ast::NameExpr*>(left)->name_);
                    return std::make_unique<ast::BinaryExpr>(std::move(bin->left_), u"+", std::move(leftClone));
                }
            }

            // x - x = 0
            if (bin->op_ == u"-" && left->kind_ == ast::Expr::Kind::NAME && right->kind_ == ast::Expr::Kind::NAME)
            {
                auto* lname = static_cast<ast::NameExpr*>(left);
                auto* rname = static_cast<ast::NameExpr*>(right);
                if (lname->name_ == rname->name_)
                {
                    stats_.strengthReductions_++;
                    return std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, u"0");
                }
            }
        }
        else if (expr->kind_ == ast::Expr::Kind::UNARY)
        {
            auto* un = static_cast<ast::UnaryExpr*>(expr.get());
            un->operand_ = optimizeConstantFolding(std::move(un->operand_));

            if (auto val = evaluateConstant(expr.get()))
            {
                stats_.constantFolds_++;
                return std::make_unique<ast::LiteralExpr>(
                  ast::LiteralExpr::Type::NUMBER, utf8::utf8to16(std::to_string(*val)));
            }

            // Double negation: --x = x
            if (un->op_ == u"-" && un->operand_->kind_ == ast::Expr::Kind::UNARY)
            {
                auto* innerUn = static_cast<ast::UnaryExpr*>(un->operand_.get());
                if (innerUn->op_ == u"-")
                {
                    stats_.strengthReductions_++;
                    return std::move(innerUn->operand_);
                }
            }
        }
        else if (expr->kind_ == ast::Expr::Kind::CALL)
        {
            auto* call = static_cast<ast::CallExpr*>(expr.get());
            for (auto& arg : call->args_)
            {
                arg = optimizeConstantFolding(std::move(arg));
            }
        }
        else if (expr->kind_ == ast::Expr::Kind::LIST)
        {
            auto* list = static_cast<ast::ListExpr*>(expr.get());
            for (auto& elem : list->elements_)
            {
                elem = optimizeConstantFolding(std::move(elem));
            }
        }
        else if (expr->kind_ == ast::Expr::Kind::TERNARY)
        {
            auto* tern = static_cast<ast::TernaryExpr*>(expr.get());
            tern->condition_ = optimizeConstantFolding(std::move(tern->condition_));
            tern->trueExpr_ = optimizeConstantFolding(std::move(tern->trueExpr_));

            // Constant ternary evaluation
            if (tern->condition_->kind_ == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(tern->condition_.get());
                if (lit->litType_ == ast::LiteralExpr::Type::BOOLEAN)
                {
                    stats_.constantFolds_++;
                    return lit->value_ == u"true" ? std::move(tern->trueExpr_) : std::move(tern->falseExpr_);
                }
            }
        }

        return expr;
    }

    // Pass 2: Dead Code Elimination
    ast::StmtPtr eliminateDeadCode(ast::StmtPtr stmt)
    {
        if (!stmt)
        {
            return stmt;
        }

        if (stmt->kind_ == ast::Stmt::Kind::IF)
        {
            auto* ifStmt = static_cast<ast::IfStmt*>(stmt.get());

            // Constant condition elimination
            if (ifStmt->condition_->kind_ == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(ifStmt->condition_.get());
                if (lit->litType_ == ast::LiteralExpr::Type::BOOLEAN)
                {
                    stats_.deadCodeEliminations_++;

                    if (lit->value_ == u"true")
                    {
                        // Return then block as block statement
                        return std::make_unique<ast::BlockStmt>(std::move(ifStmt->thenBlock_));
                    }
                    else
                    {
                        // Return else block or nothing
                        if (!ifStmt->elseBlock_.empty())
                        {
                            return std::make_unique<ast::BlockStmt>(std::move(ifStmt->elseBlock_));
                        }
                        // TODO : return std::make_unique<StmtPtr>();
                    }
                }
            }

            // Recursively eliminate in blocks
            std::vector<ast::StmtPtr> newThen, newElse;
            for (auto& s : ifStmt->thenBlock_)
            {
                if (auto opt = eliminateDeadCode(std::move(s)))
                {
                    newThen.push_back(std::move(opt));
                }
            }
            for (auto& s : ifStmt->elseBlock_)
            {
                if (auto opt = eliminateDeadCode(std::move(s)))
                {
                    newElse.push_back(std::move(opt));
                }
            }
            ifStmt->thenBlock_ = std::move(newThen);
            ifStmt->elseBlock_ = std::move(newElse);
        }
        else if (stmt->kind_ == ast::Stmt::Kind::WHILE)
        {
            auto* whileStmt = static_cast<ast::WhileStmt*>(stmt.get());

            // Infinite loop with false condition
            if (whileStmt->condition_->kind_ == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<ast::LiteralExpr*>(whileStmt->condition_.get());
                if (lit->litType_ == ast::LiteralExpr::Type::BOOLEAN && lit->value_ == u"false")
                {
                    stats_.deadCodeEliminations_++;
                    // TODO : return std::make_unique<PassStmt>();
                }
            }

            std::vector<ast::StmtPtr> newBody;
            for (auto& s : whileStmt->body_)
            {
                if (auto opt = eliminateDeadCode(std::move(s)))
                {
                    newBody.push_back(std::move(opt));
                }
            }
            whileStmt->body_ = std::move(newBody);
        }
        else if (stmt->kind_ == ast::Stmt::Kind::FOR)
        {
            auto* forStmt = static_cast<ast::ForStmt*>(stmt.get());

            std::vector<ast::StmtPtr> newBody;
            for (auto& s : forStmt->body_)
            {
                if (auto opt = eliminateDeadCode(std::move(s)))
                {
                    newBody.push_back(std::move(opt));
                }
            }
            forStmt->body_ = std::move(newBody);
        }
        else if (stmt->kind_ == ast::Stmt::Kind::FUNCTION_DEF)
        {
            auto* funcDef = static_cast<ast::FunctionDef*>(stmt.get());

            std::vector<ast::StmtPtr> newBody;
            bool seenReturn = false;
            for (auto& s : funcDef->body_)
            {
                if (seenReturn)
                {
                    stats_.deadCodeEliminations_++;
                    continue;  // Skip statements after return
                }
                if (s->kind_ == ast::Stmt::Kind::RETURN)
                {
                    seenReturn = true;
                }
                if (auto opt = eliminateDeadCode(std::move(s)))
                {
                    newBody.push_back(std::move(opt));
                }
            }
            funcDef->body_ = std::move(newBody);
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
            {
                return u"";
            }

            switch (expr->kind_)
            {
            case ast::Expr::Kind::LITERAL : {
                auto* lit = static_cast<const ast::LiteralExpr*>(expr);
                return lit->value_;
            }
            case ast::Expr::Kind::NAME : {
                auto* name = static_cast<const ast::NameExpr*>(expr);
                return name->name_;
            }
            case ast::Expr::Kind::BINARY : {
                auto* bin = static_cast<const ast::BinaryExpr*>(expr);
                return u"(" + exprToString(bin->left_.get()) + u" " + bin->op_ + u" " + exprToString(bin->right_.get())
                  + u")";
            }
            case ast::Expr::Kind::UNARY : {
                auto* un = static_cast<const ast::UnaryExpr*>(expr);
                return un->op_ + exprToString(un->operand_.get());
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
            {
                return std::nullopt;
            }

            auto it = exprCache.find(exprStr);
            if (it != exprCache.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        void recordExpr(const ast::Expr* expr, const std::u16string& var)
        {
            std::u16string exprStr = exprToString(expr);
            if (!exprStr.empty())
            {
                exprCache[exprStr] = var;
            }
        }
    };

    // Pass 4: Loop Invariant Code Motion
    bool isLoopInvariant(const ast::Expr* expr, const std::unordered_set<std::u16string>& loopVars)
    {
        if (!expr)
        {
            return true;
        }

        if (expr->kind_ == ast::Expr::Kind::NAME)
        {
            auto* name = static_cast<const ast::NameExpr*>(expr);
            return !loopVars.count(name->name_);
        }
        else if (expr->kind_ == ast::Expr::Kind::BINARY)
        {
            auto* bin = static_cast<const ast::BinaryExpr*>(expr);
            return isLoopInvariant(bin->left_.get(), loopVars) && isLoopInvariant(bin->right_.get(), loopVars);
        }
        else if (expr->kind_ == ast::Expr::Kind::UNARY)
        {
            auto* un = static_cast<const ast::UnaryExpr*>(expr);
            return isLoopInvariant(un->operand_.get(), loopVars);
        }
        else if (expr->kind_ == ast::Expr::Kind::LITERAL)
        {
            return true;
        }

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
                if (stmt->kind_ == ast::Stmt::Kind::ASSIGNMENT)
                {
                    auto* assign = static_cast<ast::AssignmentStmt*>(stmt.get());
                    assign->value_ = optimizeConstantFolding(std::move(assign->value_));
                }
                else if (stmt->kind_ == ast::Stmt::Kind::EXPRESSION)
                {
                    auto* exprStmt = static_cast<ast::ExprStmt*>(stmt.get());
                    exprStmt->expression_ = optimizeConstantFolding(std::move(exprStmt->expression_));
                }
            }

            if (level >= 2)
            {
                // O2: Dead code elimination
                stmt = eliminateDeadCode(std::move(stmt));
            }

            if (stmt)
            {
                result.push_back(std::move(stmt));
            }
        }

        return result;
    }

    const OptimizationStats& getStats() const { return stats_; }

    void printStats() const
    {
        std::cout << "\n=== Optimization Statistics ===\n";
        std::cout << "Constant folds: " << stats_.constantFolds_ << "\n";
        std::cout << "Dead code eliminations: " << stats_.deadCodeEliminations_ << "\n";
        std::cout << "Strength reductions: " << stats_.strengthReductions_ << "\n";
        std::cout << "Common subexpr eliminations: " << stats_.commonSubexprEliminations_ << "\n";
        std::cout << "Loop invariants moved: " << stats_.loopInvariants_ << "\n";
        std::cout << "Total optimizations: "
                  << (stats_.constantFolds_ + stats_.deadCodeEliminations_ + stats_.strengthReductions_
                       + stats_.commonSubexprEliminations_ + stats_.loopInvariants_)
                  << "\n";
    }
};

}
}
}