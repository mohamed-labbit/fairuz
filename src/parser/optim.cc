#include "../../include/parser/optim.hpp"

namespace mylang {
namespace parser {

std::optional<double> ASTOptimizer::evaluateConstant(ast::Expr const* expr)
{
    if (!expr)
        return std::nullopt;

    if (expr->getKind() == ast::Expr::Kind::LITERAL) {
        ast::LiteralExpr const* lit = dynamic_cast<ast::LiteralExpr const*>(expr);
        if (lit->isNumeric())
            return lit->getValue().toDouble();
    } else if (expr->getKind() == ast::Expr::Kind::BINARY) {
        ast::BinaryExpr const* bin = dynamic_cast<ast::BinaryExpr const*>(expr);

        std::optional<double> left = evaluateConstant(bin->getLeft());
        std::optional<double> right = evaluateConstant(bin->getRight());

        if (left == std::nullopt || right == std::nullopt)
            return std::nullopt;

        if (bin->getOperator() == tok::TokenType::OP_PLUS)
            return *left + *right;

        if (bin->getOperator() == tok::TokenType::OP_MINUS)
            return *left - *right;

        if (bin->getOperator() == tok::TokenType::OP_STAR)
            return *left * *right;

        if (bin->getOperator() == tok::TokenType::OP_SLASH) {
            if (*right == 0.0)
                return std::nullopt;

            return *left / *right;
        }

        if (bin->getOperator() == tok::TokenType::OP_PERCENT) {
            if (*right == 0.0)
                return std::nullopt;

            return std::fmod(*left, *right);
        }

        if (bin->getOperator() == tok::TokenType::OP_POWER)
            return std::pow(*left, *right);
    } else if (expr->getKind() == ast::Expr::Kind::UNARY) {
        ast::UnaryExpr const* un = dynamic_cast<ast::UnaryExpr const*>(expr);
        std::optional<double> operand = evaluateConstant(un->getOperand());

        if (operand == std::nullopt)
            return std::nullopt;

        if (un->getOperator() == tok::TokenType::OP_PLUS)
            return *operand;

        if (un->getOperator() == tok::TokenType::OP_MINUS)
            return -*operand;
    }
    return std::nullopt;
}

ast::Expr* ASTOptimizer::optimizeConstantFolding(ast::Expr* expr)
{
    if (!expr)
        return nullptr;

    // First, optimize children
    if (expr->getKind() == ast::Expr::Kind::BINARY) {
        ast::BinaryExpr* bin = dynamic_cast<ast::BinaryExpr*>(expr);

        bin->setLeft(optimizeConstantFolding(bin->getLeft()));
        bin->setRight(optimizeConstantFolding(bin->getRight()));

        // Try to evaluate
        if (std::optional<double> val = evaluateConstant(expr)) {
            ++Stats_.ConstantFolds;
            return ast::makeLiteral(ast::LiteralExpr::Type::DECIMAL, StringRef(std::to_string(*val).data()));
        }

        // Algebraic simplifications
        ast::Expr* left = bin->getLeft();
        ast::Expr* right = bin->getRight();

        if (!left || !right)
            return expr;

        tok::TokenType op = bin->getOperator();

        ast::Expr::Kind r_kind = right->getKind();
        ast::Expr::Kind l_kind = left->getKind();

        // x + 0 = x, x - 0 = x
        if ((op == tok::TokenType::OP_PLUS || op == tok::TokenType::OP_MINUS) && right->getKind() == ast::Expr::Kind::LITERAL) {
            ast::LiteralExpr* lit = dynamic_cast<ast::LiteralExpr*>(right);
            if (lit->getValue().toDouble() == 0.0) {
                ++Stats_.StrengthReductions;
                return bin->getLeft();
            }
        }

        // x * 1 = x, x / 1 = x
        if ((op == tok::TokenType::OP_STAR || op == tok::TokenType::OP_SLASH) && r_kind == ast::Expr::Kind::LITERAL) {
            ast::LiteralExpr* lit = dynamic_cast<ast::LiteralExpr*>(right);
            if (lit->getValue().toDouble() == 1.0) {
                ++Stats_.StrengthReductions;
                return bin->getLeft();
            }
        }

        // x * 0 = 0
        /// TODO: find a way to exclude IEEE floats since they could be inf or NaN
        if (op == tok::TokenType::OP_STAR && r_kind == ast::Expr::Kind::LITERAL) {
            ast::LiteralExpr* lit = dynamic_cast<ast::LiteralExpr*>(right);
            if (lit->getValue().toDouble() == 0.0) {
                ++Stats_.StrengthReductions;
                return ast::makeLiteral(ast::LiteralExpr::Type::INTEGER, "0");
            }
        }

        // x * 2 = x + x (strength reduction)
        if (op == tok::TokenType::OP_STAR && r_kind == ast::Expr::Kind::LITERAL) {
            auto* lit = dynamic_cast<ast::LiteralExpr*>(right);
            if (lit->getValue().toDouble() == 2.0) {
                ++Stats_.StrengthReductions;
                ast::NameExpr* leftClone = ast::makeName(dynamic_cast<ast::NameExpr*>(left)->getValue());
                return ast::makeBinary(bin->getLeft(), leftClone, tok::TokenType::OP_PLUS);
            }
        }

        // x - x = 0
        if (op == tok::TokenType::OP_MINUS && l_kind == ast::Expr::Kind::NAME && r_kind == ast::Expr::Kind::NAME) {
            ast::NameExpr* lname = dynamic_cast<ast::NameExpr*>(left);
            ast::NameExpr* rname = dynamic_cast<ast::NameExpr*>(right);
            if (lname->getValue() == rname->getValue()) {
                ++Stats_.StrengthReductions;
                return ast::makeLiteral(ast::LiteralExpr::Type::INTEGER, "0");
            }
        }

        // string concatenation
        if (l_kind == ast::Expr::Kind::LITERAL && r_kind == ast::Expr::Kind::LITERAL && op == tok::TokenType::OP_PLUS) {
            ast::LiteralExpr* l_expr = dynamic_cast<ast::LiteralExpr*>(left);
            ast::LiteralExpr* r_expr = dynamic_cast<ast::LiteralExpr*>(right);

            if (l_expr->getType() == ast::LiteralExpr::Type::STRING && r_expr->getType() == ast::LiteralExpr::Type::STRING) {
                StringRef l_str = l_expr->getValue();
                StringRef r_str = r_expr->getValue();

                return ast::makeLiteral(ast::LiteralExpr::Type::STRING, l_str + r_str);
            }
        }
    } else if (expr->getKind() == ast::Expr::Kind::UNARY) {
        auto* outer = static_cast<ast::UnaryExpr*>(expr);
        ast::Expr* optimizedOperand = optimizeConstantFolding(outer->getOperand());
        bool operandChanged = (optimizedOperand != outer->getOperand());
        ast::UnaryExpr* rebuiltUnary = nullptr;

        if (operandChanged)
            rebuiltUnary = ast::makeUnary(optimizedOperand, outer->getOperator());
        else 
            rebuiltUnary = outer;

        if (auto val = evaluateConstant(rebuiltUnary)) {
            ++Stats_.ConstantFolds;
            std::string s = std::to_string(*val);
            return ast::makeLiteral(ast::LiteralExpr::Type::DECIMAL /* ?? */, StringRef(s.data()));
        }

        if (rebuiltUnary->getOperator() == tok::TokenType::OP_MINUS) {
            if (auto* inner = dynamic_cast<ast::UnaryExpr*>(optimizedOperand)) {
                if (inner->getOperator() == tok::TokenType::OP_MINUS) {
                    ++Stats_.StrengthReductions;
                    // return operand of inner minus (x)
                    return inner->getOperand();
                }
            }
        }

        return rebuiltUnary;
    } else if (expr->getKind() == ast::Expr::Kind::CALL) {
        ast::CallExpr* call = dynamic_cast<ast::CallExpr*>(expr);
        for (ast::Expr*& arg : call->getArgsMutable())
            arg = optimizeConstantFolding(arg);
    } else if (expr->getKind() == ast::Expr::Kind::LIST) {
        ast::ListExpr* list = dynamic_cast<ast::ListExpr*>(expr);
        for (ast::Expr*& elem : list->getElementsMutable())
            elem = optimizeConstantFolding(elem);
    }

    return expr;
}

ast::Stmt* ASTOptimizer::eliminateDeadCode(ast::Stmt* stmt)
{
    if (!stmt)
        return nullptr;

    if (stmt->getKind() == ast::Stmt::Kind::IF) {
        ast::IfStmt* ifStmt = dynamic_cast<ast::IfStmt*>(stmt);

        // Constant condition elimination
        if (ifStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL) {
            ast::LiteralExpr* lit = dynamic_cast<ast::LiteralExpr*>(ifStmt->getCondition());
            if (lit->getType() == ast::LiteralExpr::Type::BOOLEAN) {
                ++Stats_.DeadCodeEliminations;
                if (lit->getValue() == "true")
                    // Return then block as block statement
                    return dynamic_cast<ast::Stmt*>(ifStmt->getThenBlock());
                else {
                    // Return else block or nothing
                    if (!ifStmt->getElseBlock()->getStatements().empty())
                        return dynamic_cast<ast::Stmt*>(ifStmt->getElseBlock());
                    else
                        return nullptr;
                }
            }
        }

        ifStmt->setThenBlock(dynamic_cast<ast::BlockStmt*>(eliminateDeadCode(ifStmt->getThenBlock())));
        ifStmt->setElseBlock(dynamic_cast<ast::BlockStmt*>(eliminateDeadCode(ifStmt->getElseBlock())));

        return ifStmt;
    } else if (stmt->getKind() == ast::Stmt::Kind::WHILE) {
        ast::WhileStmt* whileStmt = dynamic_cast<ast::WhileStmt*>(stmt);

        // Infinite loop with false condition
        if (whileStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL) {
            ast::LiteralExpr* lit = dynamic_cast<ast::LiteralExpr*>(whileStmt->getCondition());
            if (lit->getType() == ast::LiteralExpr::Type::BOOLEAN && lit->getValue() == "خطا") {
                ++Stats_.DeadCodeEliminations;
                return nullptr; // kill loop
            }
        }

        whileStmt->setBlock(dynamic_cast<ast::BlockStmt*>(eliminateDeadCode(whileStmt->getBlockMutable())));

        return whileStmt;
    } else if (stmt->getKind() == ast::Stmt::Kind::FOR) {
        ast::ForStmt* forStmt = dynamic_cast<ast::ForStmt*>(stmt);
        /// TODO: evaluate for loop condition and kill it if it doesn't run
        forStmt->setBlock(dynamic_cast<ast::BlockStmt*>(eliminateDeadCode(forStmt->getBlock())));
    } else if (stmt->getKind() == ast::Stmt::Kind::FUNC) {
        ast::FunctionDef* funcDef = dynamic_cast<ast::FunctionDef*>(stmt);
        funcDef->setBody(dynamic_cast<ast::BlockStmt*>(eliminateDeadCode(funcDef->getBody())));
        return funcDef;
    } else if (stmt->getKind() == ast::Stmt::Kind::BLOCK) {
        ast::BlockStmt* block_stmt = dynamic_cast<ast::BlockStmt*>(stmt);
        std::vector<ast::Stmt*> new_stmts;
        new_stmts.reserve(block_stmt->getStatements().size());
        bool seen_return = false;
        for (ast::Stmt* s : block_stmt->getStatements()) {
            if (seen_return) {
                Stats_.DeadCodeEliminations++;
                break;
            }

            if (s->getKind() == ast::Stmt::Kind::RETURN)
                seen_return = true;

            new_stmts.push_back(eliminateDeadCode(s));
        }

        block_stmt->setStatements(new_stmts);

        return block_stmt;
    }

    return stmt;
}

StringRef ASTOptimizer::CSEPass::exprToString(ast::Expr const* expr)
{
    if (!expr)
        return "";

    switch (expr->getKind()) {
    case ast::Expr::Kind::LITERAL: {
        ast::LiteralExpr const* lit = dynamic_cast<ast::LiteralExpr const*>(expr);
        return lit->getValue();
    }
    case ast::Expr::Kind::NAME: {
        ast::NameExpr const* name = dynamic_cast<ast::NameExpr const*>(expr);
        return name->getValue();
    }
    case ast::Expr::Kind::BINARY: {
        ast::BinaryExpr const* bin = dynamic_cast<ast::BinaryExpr const*>(expr);
        return "(" + exprToString(bin->getLeft()) + " " + tok::Token::toString(bin->getOperator()) + " " + exprToString(bin->getRight()) + ")";
    }
    case ast::Expr::Kind::UNARY: {
        ast::UnaryExpr const* un = dynamic_cast<ast::UnaryExpr const*>(expr);
        return tok::Token::toString(un->getOperator()) + exprToString(un->getOperand());
    }
    default:
        return "";
    }
}

StringRef ASTOptimizer::CSEPass::getTempVar()
{
    return StringRef("__cse_temp_") + static_cast<char>(TempCounter_++);
}

std::optional<StringRef> ASTOptimizer::CSEPass::findCSE(ast::Expr const* expr)
{
    StringRef exprStr = exprToString(expr);
    if (exprStr.empty())
        return std::nullopt;

    auto it = ExprCache_.find(exprStr);
    if (it != ExprCache_.end())
        return it->second;

    return std::nullopt;
}

void ASTOptimizer::CSEPass::recordExpr(ast::Expr const* expr, StringRef const& var)
{
    StringRef exprStr = exprToString(expr);
    if (!exprStr.empty())
        ExprCache_[exprStr] = var;
}

bool ASTOptimizer::isLoopInvariant(ast::Expr const* expr, std::unordered_set<StringRef, StringRefHash, StringRefEqual> const& loopVars)
{
    if (!expr)
        return true;

    if (expr->getKind() == ast::Expr::Kind::NAME) {
        ast::NameExpr const* name = dynamic_cast<ast::NameExpr const*>(expr);
        return !loopVars.count(name->getValue());
    } else if (expr->getKind() == ast::Expr::Kind::BINARY) {
        ast::BinaryExpr const* bin = dynamic_cast<ast::BinaryExpr const*>(expr);
        return isLoopInvariant(bin->getLeft(), loopVars) && isLoopInvariant(bin->getRight(), loopVars);
    } else if (expr->getKind() == ast::Expr::Kind::UNARY) {
        ast::UnaryExpr const* un = dynamic_cast<ast::UnaryExpr const*>(expr);
        return isLoopInvariant(un->getOperand(), loopVars);
    } else if (expr->getKind() == ast::Expr::Kind::LITERAL)
        return true;

    return false;
}

std::vector<ast::Stmt*> ASTOptimizer::optimize(std::vector<ast::Stmt*> statements, int32_t level)
{
    std::vector<ast::Stmt*> result;
    for (ast::Stmt*& stmt : statements) {
        // Apply optimizations based on level
        if (level >= 1) {
            // O1: Basic optimizations
            if (stmt->getKind() == ast::Stmt::Kind::ASSIGNMENT) {
                ast::AssignmentStmt* assign = dynamic_cast<ast::AssignmentStmt*>(stmt);
                assign->setValue(optimizeConstantFolding(assign->getValue()));
            } else if (stmt->getKind() == ast::Stmt::Kind::EXPR) {
                ast::ExprStmt* exprStmt = dynamic_cast<ast::ExprStmt*>(stmt);
                exprStmt->setExpr(optimizeConstantFolding(exprStmt->getExpr()));
            }
        }

        // O2: Dead code elimination
        if (level >= 2)
            stmt = eliminateDeadCode(stmt);

        if (stmt)
            result.push_back(stmt);
    }
    return result;
}

typename ASTOptimizer::OptimizationStats const& ASTOptimizer::getStats() const
{
    return Stats_;
}

void ASTOptimizer::printStats() const
{
    std::cout << "\n=== Optimization Statistics ===\n";
    std::cout << "Constant folds: " << Stats_.ConstantFolds << "\n";
    std::cout << "Dead code eliminations: " << Stats_.DeadCodeEliminations << "\n";
    std::cout << "Strength reductions: " << Stats_.StrengthReductions << "\n";
    std::cout << "Common subexpr eliminations: " << Stats_.CommonSubexprEliminations << "\n";
    std::cout << "Loop invariants moved: " << Stats_.LoopInvariants << "\n";
    std::cout << "Total optimizations: "
              << (Stats_.ConstantFolds + Stats_.DeadCodeEliminations + Stats_.StrengthReductions + Stats_.CommonSubexprEliminations
                     + Stats_.LoopInvariants)
              << "\n";
}

}

}
