#include "../../include/parser/optim.hpp"

namespace mylang {
namespace parser {

using namespace ast;

namespace {

bool isDefinitelyIntegerExpr(Expr const* expr)
{
    if (!expr)
        return false;

    switch (expr->getKind()) {
    case Expr::Kind::LITERAL: {
        auto const* lit = static_cast<LiteralExpr const*>(expr);
        return lit->isInteger();
    }
    case Expr::Kind::UNARY: {
        auto const* un = static_cast<UnaryExpr const*>(expr);
        UnaryOp op = un->getOperator();
        if (op != UnaryOp::OP_PLUS && op != UnaryOp::OP_NEG)
            return false;
        return isDefinitelyIntegerExpr(un->getOperand());
    }
    case Expr::Kind::BINARY: {
        auto const* bin = static_cast<BinaryExpr const*>(expr);
        if (!isDefinitelyIntegerExpr(bin->getLeft()) || !isDefinitelyIntegerExpr(bin->getRight()))
            return false;

        BinaryOp op = bin->getOperator();
        return op == BinaryOp::OP_ADD
            || op == BinaryOp::OP_SUB
            || op == BinaryOp::OP_MUL
            || op == BinaryOp::OP_MOD;
    }
    default:
        return false;
    }
}

}

std::optional<double> ASTOptimizer::evaluateConstant(Expr const* expr)
{
    if (!expr)
        return std::nullopt;

    if (expr->getKind() == Expr::Kind::LITERAL) {
        LiteralExpr const* lit = dynamic_cast<LiteralExpr const*>(expr);

        if (lit->isNumeric())
            return lit->toNumber();
    } else if (expr->getKind() == Expr::Kind::BINARY) {
        BinaryExpr const* bin = dynamic_cast<BinaryExpr const*>(expr);

        std::optional<double> L = evaluateConstant(bin->getLeft());
        std::optional<double> R = evaluateConstant(bin->getRight());

        if (!L || !R)
            return std::nullopt;

        switch (bin->getOperator()) {
        case BinaryOp::OP_ADD: return *L + *R;
        case BinaryOp::OP_SUB: return *L - *R;
        case BinaryOp::OP_MUL: return *L * *R;
        case BinaryOp::OP_POW: return std::pow(*L, *R);
        case BinaryOp::OP_DIV:
            if (*R == 0.0)
                return std::nullopt;

            return *L / *R;

        case BinaryOp::OP_MOD:
            if (*R == 0.0)
                return std::nullopt;

            return std::fmod(*L, *R);
        default:
            return std::nullopt;
        }
    } else if (expr->getKind() == Expr::Kind::UNARY) {
        UnaryExpr const* un = dynamic_cast<UnaryExpr const*>(expr);
        std::optional<double> operand = evaluateConstant(un->getOperand());

        if (!operand)
            return std::nullopt;

        if (un->getOperator() == UnaryOp::OP_PLUS)
            return *operand;

        if (un->getOperator() == UnaryOp::OP_NEG)
            return -*operand;
    }
    return std::nullopt;
}

Expr* ASTOptimizer::optimizeConstantFolding(Expr* expr)
{
    if (!expr)
        return nullptr;

    // First, optimize children
    if (expr->getKind() == Expr::Kind::BINARY) {
        BinaryExpr* bin = dynamic_cast<BinaryExpr*>(expr);

        bin->setLeft(optimizeConstantFolding(bin->getLeft()));
        bin->setRight(optimizeConstantFolding(bin->getRight()));

        // Try to evaluate
        if (std::optional<double> val = evaluateConstant(expr)) {
            ++Stats_.ConstantFolds;
            return makeLiteralFloat(*val);
        }

        // Algebraic simplifications
        Expr* L = bin->getLeft();
        Expr* R = bin->getRight();

        if (!L || !R)
            return expr->clone();

        BinaryOp op = bin->getOperator();

        Expr::Kind r_kind = R->getKind();
        Expr::Kind l_kind = L->getKind();

        // x + 0 = x, x - 0 = x
        if ((op == BinaryOp::OP_ADD || op == BinaryOp::OP_SUB) && r_kind == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(R);
            if (lit->isNumeric() && lit->toNumber() == 0) {
                ++Stats_.StrengthReductions;
                return L->clone();
            }
        }

        // inverse (0 - x != x, but 0 + x = x)
        if (op == BinaryOp::OP_ADD && l_kind == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(L);
            if (lit->isNumeric() && lit->toNumber() == 0) {
                ++Stats_.StrengthReductions;
                return R->clone();
            }
        }

        // x * 1 = x, x / 1 = x
        if ((op == BinaryOp::OP_MUL || op == BinaryOp::OP_DIV) && r_kind == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(R);
            if (lit->isNumeric() && lit->toNumber() == 1) {
                ++Stats_.StrengthReductions;
                return L->clone();
            }
        }

        // 1 * x
        if (op == BinaryOp::OP_MUL && l_kind == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(L);
            if (lit->isNumeric() && lit->toNumber() == 1) {
                ++Stats_.StrengthReductions;
                return R->clone();
            }
        }

        // x * 0 = 0 (only when L side is definitely integer; IEEE floats can yield NaN for inf * 0)
        if (op == BinaryOp::OP_MUL && r_kind == Expr::Kind::LITERAL) {
            auto* r_lit = static_cast<LiteralExpr*>(R);
            if (r_lit->isNumeric() && r_lit->toNumber() == 0 /*&& isDefinitelyIntegerExpr(L)*/) {
                ++Stats_.StrengthReductions;
                return makeLiteralInt(0);
            }
        }

        // x * 2 = x + x (strength reduction)
        if (op == BinaryOp::OP_MUL && r_kind == Expr::Kind::LITERAL) {
            auto* lit = dynamic_cast<LiteralExpr*>(R);
            if (lit->isNumeric() && lit->toNumber() == 2) {
                ++Stats_.StrengthReductions;
                return makeBinary(L->clone(), L->clone(), BinaryOp::OP_ADD);
            }
        }

        if (op == BinaryOp::OP_MUL && l_kind == Expr::Kind::LITERAL) {
            auto* lit = dynamic_cast<LiteralExpr*>(L);
            if (lit->isNumeric() && lit->toNumber() == 2) {
                ++Stats_.StrengthReductions;
                return makeBinary(R->clone(), R->clone(), BinaryOp::OP_ADD);
            }
        }

        // x - x = 0
        if (op == BinaryOp::OP_SUB) {
            if (L->equals(R)) {
                ++Stats_.StrengthReductions;
                return makeLiteralInt(0);
            }
        }

        // string concatenation
        if (l_kind == Expr::Kind::LITERAL && r_kind == Expr::Kind::LITERAL && op == BinaryOp::OP_ADD) {
            LiteralExpr* l_lit = dynamic_cast<LiteralExpr*>(L);
            LiteralExpr* r_lit = dynamic_cast<LiteralExpr*>(R);

            if (l_lit->getType() == LiteralExpr::Type::STRING && r_lit->getType() == LiteralExpr::Type::STRING)
                return makeLiteralString(l_lit->getStr() + r_lit->getStr());
        }
    } else if (expr->getKind() == Expr::Kind::UNARY) {
        auto* outer = static_cast<UnaryExpr*>(expr);
        Expr* optimizedOperand = optimizeConstantFolding(outer->getOperand());
        bool operandChanged = (optimizedOperand != outer->getOperand());
        UnaryExpr* rebuiltUnary = nullptr;

        if (operandChanged)
            rebuiltUnary = makeUnary(optimizedOperand, outer->getOperator());
        else
            rebuiltUnary = outer->clone();

        if (auto val = evaluateConstant(rebuiltUnary)) {
            ++Stats_.ConstantFolds;
            return makeLiteralFloat(*val);
        }

        if (rebuiltUnary->getOperator() == UnaryOp::OP_NEG) {
            if (auto* inner = dynamic_cast<UnaryExpr*>(optimizedOperand)) {
                if (inner->getOperator() == UnaryOp::OP_NEG) {
                    ++Stats_.StrengthReductions;
                    return inner->getOperand()->clone();
                }
            }
        }

        return rebuiltUnary;
    } else if (expr->getKind() == Expr::Kind::CALL) {
        CallExpr* call = dynamic_cast<CallExpr*>(expr);
        for (Expr*& arg : call->getArgsMutable())
            arg = optimizeConstantFolding(arg);
    } else if (expr->getKind() == Expr::Kind::LIST) {
        ListExpr* list = dynamic_cast<ListExpr*>(expr);
        for (Expr*& elem : list->getElementsMutable())
            elem = optimizeConstantFolding(elem);
    }

    return expr->clone();
}

Stmt* ASTOptimizer::eliminateDeadCode(Stmt* stmt)
{
    if (!stmt)
        return nullptr;

    if (stmt->getKind() == Stmt::Kind::IF) {
        IfStmt* ifStmt = dynamic_cast<IfStmt*>(stmt);

        // Constant condition elimination
        if (ifStmt->getCondition()->getKind() == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(ifStmt->getCondition());
            if (lit->getType() == LiteralExpr::Type::BOOLEAN) {
                ++Stats_.DeadCodeEliminations;
                if (lit->getBool() == true)
                    // Return then block as block statement
                    return ifStmt->getThenBlock()->clone();
                else {
                    // Return else block or nothing
                    auto* elseBlock = ifStmt->getElseBlock();
                    if (elseBlock && !elseBlock->getStatements().empty())
                        return ifStmt->getElseBlock()->clone();
                    else
                        return nullptr;
                }
            }
        }

        IfStmt* clone = ifStmt->clone();

        clone->setThenBlock(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getThenBlock())));
        clone->setElseBlock(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getElseBlock())));

        return clone;
    } else if (stmt->getKind() == Stmt::Kind::WHILE) {
        WhileStmt* whileStmt = dynamic_cast<WhileStmt*>(stmt);

        // Infinite loop with false condition
        if (whileStmt->getCondition()->getKind() == Expr::Kind::LITERAL) {
            LiteralExpr* lit = dynamic_cast<LiteralExpr*>(whileStmt->getCondition());
            if (lit->getType() == LiteralExpr::Type::BOOLEAN && lit->getBool() == false) {
                ++Stats_.DeadCodeEliminations;
                return nullptr; // kill loop
            }
        }

        WhileStmt* clone = whileStmt->clone();
        clone->setBlock(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getBlockMutable())));
        return clone;
    } else if (stmt->getKind() == Stmt::Kind::FOR) {
        ForStmt* forStmt = dynamic_cast<ForStmt*>(stmt);
        /// TODO: evaluate for loop condition and kill it if it doesn't run
        ForStmt* clone = forStmt->clone();
        clone->setBlock(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getBlock())));
        return clone;
    } else if (stmt->getKind() == Stmt::Kind::FUNC) {
        FunctionDef* funcDef = dynamic_cast<FunctionDef*>(stmt);
        FunctionDef* clone = funcDef->clone();
        clone->setBody(dynamic_cast<BlockStmt*>(eliminateDeadCode(clone->getBody())));
        return clone;
    } else if (stmt->getKind() == Stmt::Kind::BLOCK) {
        BlockStmt* block_stmt = dynamic_cast<BlockStmt*>(stmt);
        std::vector<Stmt*> new_stmts;
        new_stmts.reserve(block_stmt->getStatements().size());
        bool seen_return = false;
        for (Stmt* s : block_stmt->getStatements()) {
            if (seen_return) {
                Stats_.DeadCodeEliminations++;
                break;
            }

            if (s->getKind() == Stmt::Kind::RETURN)
                seen_return = true;

            new_stmts.push_back(eliminateDeadCode(s));
        }

        return makeBlock(new_stmts);
    }

    return stmt->clone();
}

StringRef ASTOptimizer::CSEPass::exprToString(Expr const* expr)
{
    if (!expr)
        return "";

    switch (expr->getKind()) {
    case Expr::Kind::LITERAL: {
        LiteralExpr const* lit = dynamic_cast<LiteralExpr const*>(expr);
        if (lit->isString())
            return lit->getStr();
        else if (lit->isNumeric())
            return std::to_string(lit->toNumber()).data();
        else if (lit->isBoolean())
            return lit->getBool() ? "true" : "false";
        else
            return "";
    }
    case Expr::Kind::NAME: {
        NameExpr const* name = dynamic_cast<NameExpr const*>(expr);
        return name->getValue();
    }
    case Expr::Kind::BINARY: {
        /*
        BinaryExpr const* bin = dynamic_cast<BinaryExpr const*>(expr);
        return "(" + exprToString(bin->getLeft()) + " " + tok::Token::toString(bin->getOperator()) + " " + exprToString(bin->getRight()) + ")";
        */
        break;
    }
    case Expr::Kind::UNARY: {
        /*
        UnaryExpr const* un = dynamic_cast<UnaryExpr const*>(expr);
        return tok::Token::toString(un->getOperator()) + exprToString(un->getOperand());
        */
        break;
    }
    default:
        return "";
    }

    return "";
}

StringRef ASTOptimizer::CSEPass::getTempVar()
{
    return StringRef("__cse_temp_") + static_cast<char>(TempCounter_++);
}

std::optional<StringRef> ASTOptimizer::CSEPass::findCSE(Expr const* expr)
{
    StringRef exprStr = exprToString(expr);
    if (exprStr.empty())
        return std::nullopt;

    auto it = ExprCache_.find(exprStr);
    if (it != ExprCache_.end())
        return it->second;

    return std::nullopt;
}

void ASTOptimizer::CSEPass::recordExpr(Expr const* expr, StringRef const& var)
{
    StringRef exprStr = exprToString(expr);
    if (!exprStr.empty())
        ExprCache_[exprStr] = var;
}

bool ASTOptimizer::isLoopInvariant(Expr const* expr, std::unordered_set<StringRef, StringRefHash, StringRefEqual> const& loopVars)
{
    if (!expr)
        return true;

    if (expr->getKind() == Expr::Kind::NAME) {
        NameExpr const* name = dynamic_cast<NameExpr const*>(expr);
        return !loopVars.count(name->getValue());
    } else if (expr->getKind() == Expr::Kind::BINARY) {
        BinaryExpr const* bin = dynamic_cast<BinaryExpr const*>(expr);
        return isLoopInvariant(bin->getLeft(), loopVars) && isLoopInvariant(bin->getRight(), loopVars);
    } else if (expr->getKind() == Expr::Kind::UNARY) {
        UnaryExpr const* un = dynamic_cast<UnaryExpr const*>(expr);
        return isLoopInvariant(un->getOperand(), loopVars);
    } else if (expr->getKind() == Expr::Kind::LITERAL)
        return true;

    return false;
}

std::vector<Stmt*> ASTOptimizer::optimize(std::vector<Stmt*> statements, int32_t level)
{
    std::vector<Stmt*> result;
    for (Stmt*& stmt : statements) {
        // Apply optimizations based on level
        if (level >= 1) {
            // O1: Basic optimizations
            if (stmt->getKind() == Stmt::Kind::ASSIGNMENT) {
                AssignmentStmt* assign = dynamic_cast<AssignmentStmt*>(stmt);
                assign->setValue(optimizeConstantFolding(assign->getValue()));
            } else if (stmt->getKind() == Stmt::Kind::EXPR) {
                ExprStmt* exprStmt = dynamic_cast<ExprStmt*>(stmt);
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
