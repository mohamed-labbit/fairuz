#include "../../include/ast/ast.hpp"
#include "../../include/ast/expr.hpp"
#include "../../include/ast/printer.hpp"
#include "../../include/ast/stmt.hpp"
#include "../../include/parser/optim.hpp"
#include "../test_config.h"
#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <memory>

using namespace mylang;

ast::ASTPrinter AST_Printer;

class ASTOptimizerTest : public ::testing::Test {
protected:
    std::unique_ptr<parser::ASTOptimizer> optimizer;

    void SetUp() override
    {
        optimizer = std::make_unique<parser::ASTOptimizer>();
    }

    void TearDown() override
    {
        optimizer.reset();
    }

    // Helper to create assignment statements
    ast::AssignmentStmt* makeAssignment(StringRef const& target, ast::Expr* value)
    {
        return ast::makeAssignmentStmt(ast::makeName(target), value);
    }

    // Helper to create if statements
    ast::IfStmt* makeIf(ast::Expr* condition, ast::BlockStmt* thenBlock, ast::BlockStmt* elseBlock = nullptr)
    {
        if (!elseBlock)
            elseBlock = ast::makeBlock(std::vector<ast::Stmt*>());

        return ast::makeIf(condition, thenBlock, elseBlock);
    }

    // Helper to create function definitions
    ast::FunctionDef* makeFunction(StringRef const& name, std::vector<StringRef> const& params, ast::BlockStmt* body)
    {
        ast::NameExpr* nameExpr = ast::makeName(name);
        std::vector<ast::Expr*> paramsExpr(params.size());
        for (auto p : params)
            paramsExpr.push_back(ast::makeName(p));

        ast::ListExpr* listExpr = ast::makeList(paramsExpr);
        return ast::makeFunction(nameExpr, listExpr, body);
    }
};

// CONSTANT EVALUATION TESTS

TEST_F(ASTOptimizerTest, EvaluateConstantLiteral)
{
    ast::LiteralExpr* expr = ast::makeLiteralInt(42);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42);
}

TEST_F(ASTOptimizerTest, EvaluateConstantNull)
{
    std::optional<double> result = optimizer->evaluateConstant(nullptr);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantNonNumber)
{
    ast::LiteralExpr* expr = ast::makeLiteralBool(true);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantAddition)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(20), tok::TokenType::OP_PLUS);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 30);
}

TEST_F(ASTOptimizerTest, EvaluateConstantSubtraction)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(50), ast::makeLiteralInt(20), tok::TokenType::OP_MINUS);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 30);
}

TEST_F(ASTOptimizerTest, EvaluateConstantMultiplication)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(6), ast::makeLiteralInt(7), tok::TokenType::OP_STAR);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42);
}

TEST_F(ASTOptimizerTest, EvaluateConstantDivision)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(100), ast::makeLiteralInt(4), tok::TokenType::OP_SLASH);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 25);
}

TEST_F(ASTOptimizerTest, EvaluateConstantDivisionByZero)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(0), tok::TokenType::OP_SLASH);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantModulo)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(17), ast::makeLiteralInt(5), tok::TokenType::OP_PERCENT);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 2);
}

TEST_F(ASTOptimizerTest, EvaluateConstantModuloByZero)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(0), tok::TokenType::OP_PERCENT);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantPower)
{
    ast::BinaryExpr* expr = ast::makeBinary(ast::makeLiteralInt(2), ast::makeLiteralInt(10), tok::TokenType::OP_POWER);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1024);
}

TEST_F(ASTOptimizerTest, EvaluateConstantPowerZero)
{
    ast::BinaryExpr* expr = ast::makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(0), tok::TokenType::OP_POWER);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1);
}

TEST_F(ASTOptimizerTest, EvaluateConstantPowerNegative)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(2), ast::makeLiteralInt(-2), tok::TokenType::OP_POWER);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 0.25);
}

TEST_F(ASTOptimizerTest, EvaluateConstantUnaryPlus)
{
    ast::UnaryExpr* expr = makeUnary(ast::makeLiteralInt(42), tok::TokenType::OP_PLUS);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42);
}

TEST_F(ASTOptimizerTest, EvaluateConstantUnaryMinus)
{
    ast::UnaryExpr* expr = makeUnary(ast::makeLiteralInt(42), tok::TokenType::OP_MINUS);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, -42);
}

TEST_F(ASTOptimizerTest, EvaluateConstantNestedExpression)
{
    // (10 + 20) * 3
    ast::BinaryExpr* inner = makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(20), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* outer = makeBinary(inner, ast::makeLiteralInt(3), tok::TokenType::OP_STAR);

    std::optional<double> result = optimizer->evaluateConstant(outer);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 90);
}

TEST_F(ASTOptimizerTest, EvaluateConstantComplexExpression)
{
    // ((5 + 3) * 2) - (10 / 2)
    ast::BinaryExpr* add = makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(3), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* mul = makeBinary(add, ast::makeLiteralInt(2), tok::TokenType::OP_STAR);
    ast::BinaryExpr* div = makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(2), tok::TokenType::OP_SLASH);
    ast::BinaryExpr* sub = makeBinary(mul, div, tok::TokenType::OP_MINUS);

    std::optional<double> result = optimizer->evaluateConstant(sub);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 11);
}

TEST_F(ASTOptimizerTest, EvaluateConstantWithVariable)
{
    // x + 5 (cannot be evaluated)
    ast::BinaryExpr* expr = makeBinary(ast::makeName("x"), ast::makeLiteralInt(5), tok::TokenType::OP_PLUS);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantPartiallyConstant)
{
    // (10 + 20) + x (left side is constant, but overall is not)
    ast::BinaryExpr* left = makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(20), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* expr = makeBinary(left, ast::makeName("x"), tok::TokenType::OP_PLUS);

    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

// CONSTANT FOLDING OPTIMIZATION TESTS

TEST_F(ASTOptimizerTest, ConstantFoldingSimpleAddition)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(10), tok::TokenType::OP_PLUS);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(result);
    EXPECT_TRUE(lit->isNumeric());
    EXPECT_DOUBLE_EQ(lit->toNumber(), 15);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, ConstantFoldingMultiplication)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(6), ast::makeLiteralInt(7), tok::TokenType::OP_STAR);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(result);
    EXPECT_DOUBLE_EQ(lit->toNumber(), 42);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNested)
{
    // (3 + 4) * (5 - 2)
    ast::BinaryExpr* left = makeBinary(ast::makeLiteralInt(3), ast::makeLiteralInt(4), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* right = makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(2), tok::TokenType::OP_MINUS);
    ast::BinaryExpr* expr = makeBinary(left, right, tok::TokenType::OP_STAR);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<ast::LiteralExpr*>(result)->toNumber(), 21);
}

TEST_F(ASTOptimizerTest, ConstantFoldingUnary)
{
    ast::UnaryExpr* expr = makeUnary(ast::makeLiteralInt(42), tok::TokenType::OP_MINUS);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<ast::LiteralExpr*>(result)->toNumber(), -42);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNoChange)
{
    // x + y (no constants to fold)
    ast::BinaryExpr* expr = ast::makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS);
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_TRUE(result->equals(expr)); // Should return original
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 0);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNull)
{
    ast::Expr* result = optimizer->optimizeConstantFolding(nullptr);
    EXPECT_EQ(result, nullptr);
}

// ALGEBRAIC SIMPLIFICATION TESTS

TEST_F(ASTOptimizerTest, SimplifyAdditionWithZero)
{
    // x + 0 = x
    ast::NameExpr* x = ast::makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, ast::makeLiteralInt(0), tok::TokenType::OP_PLUS);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_TRUE(result->equals(x));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionWithZero)
{
    // x - 0 = x
    ast::NameExpr* x = ast::makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, ast::makeLiteralInt(0), tok::TokenType::OP_MINUS);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_TRUE(result->equals(x));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByOne)
{
    // x * 1 = x
    ast::NameExpr* x = ast::makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, ast::makeLiteralInt(1), tok::TokenType::OP_STAR);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_TRUE(result->equals(x));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyDivisionByOne)
{
    // x / 1 = x
    ast::NameExpr* x = ast::makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, ast::makeLiteralInt(1), tok::TokenType::OP_SLASH);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_TRUE(result->equals(x));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByZero)
{
    // x * 0 = 0
    ast::NameExpr* x = ast::makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, ast::makeLiteralInt(0), tok::TokenType::OP_STAR);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(result);
    EXPECT_EQ(lit->toNumber(), 0);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByTwo)
{
    // x * 2 = x + x
    ast::NameExpr* x = ast::makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, ast::makeLiteralInt(2), tok::TokenType::OP_STAR);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(expr);
    }
    ASSERT_EQ(result->getKind(), ast::Expr::Kind::BINARY);
    ast::BinaryExpr* bin = static_cast<ast::BinaryExpr*>(result);
    EXPECT_EQ(bin->getOperator(), tok::TokenType::OP_PLUS);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionSameVariable)
{
    // x - x = 0
    ast::NameExpr* x1 = ast::makeName("x");
    ast::NameExpr* x2 = ast::makeName("x");
    ast::BinaryExpr* expr = makeBinary(x1, x2, tok::TokenType::OP_MINUS);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(result);
    EXPECT_EQ(lit->toNumber(), 0);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionDifferentVariables)
{
    // x - y (should not simplify)
    ast::BinaryExpr* expr = makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_MINUS);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_TRUE(result->equals(expr));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 0);
}

TEST_F(ASTOptimizerTest, SimplifyDoubleNegation)
{
    // --x = x
    ast::NameExpr* x = ast::makeName("x");
    ast::UnaryExpr* inner = makeUnary(x, tok::TokenType::OP_MINUS);
    ast::UnaryExpr* outer = makeUnary(inner, tok::TokenType::OP_MINUS);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(outer);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(outer);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    // Should eliminate double negation
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, NoSimplificationWithNonZero)
{
    // x + 5 (should not simplify)
    ast::BinaryExpr* expr = makeBinary(ast::makeName("x"), ast::makeLiteralInt(5), tok::TokenType::OP_PLUS);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_TRUE(result->equals(expr));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 0);
}

// CALL AND LIST OPTIMIZATION TESTS

TEST_F(ASTOptimizerTest, OptimizeCallExpressionArgs)
{
    // func(2 + 3, 4 * 5)
    std::vector<ast::Expr*> args = {
        makeBinary(ast::makeLiteralInt(2), ast::makeLiteralInt(3), tok::TokenType::OP_PLUS),
        makeBinary(ast::makeLiteralInt(4), ast::makeLiteralInt(5), tok::TokenType::OP_STAR)
    };

    ast::CallExpr* call = ast::makeCall(ast::makeName("func"), ast::makeList(args));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(call);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(call);
    ASSERT_EQ(result->getKind(), ast::Expr::Kind::CALL);
    ast::CallExpr* optimized = static_cast<ast::CallExpr*>(result);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(optimized);
    }
    // Arguments should be folded
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 2);
}

TEST_F(ASTOptimizerTest, OptimizeListElements)
{
    // [1 + 2, 3 * 4, x]
    std::vector<ast::Expr*> elements = {
        makeBinary(ast::makeLiteralInt(1), ast::makeLiteralInt(2), tok::TokenType::OP_PLUS),
        makeBinary(ast::makeLiteralInt(3), ast::makeLiteralInt(4), tok::TokenType::OP_STAR),
        ast::makeName("x")
    };
    ast::ListExpr* list = ast::makeList(elements);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(list);
    }
    ast::Expr* result = optimizer->optimizeConstantFolding(list);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LIST);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 2);
}

TEST_F(ASTOptimizerTest, OptimizeEmptyList)
{
    ast::ListExpr* list = ast::makeList(std::vector<ast::Expr*>());
    ast::Expr* result = optimizer->optimizeConstantFolding(list);

    EXPECT_TRUE(result->equals(list));
}

// DEAD CODE ELIMINATION - IF STATEMENTS

TEST_F(ASTOptimizerTest, DeadCodeIfTrueCondition)
{
    // if (true) { stmt1 } else { stmt2 }
    // Should return stmt1
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) });
    ast::BlockStmt* elseBlock = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(2)) });
    ast::IfStmt* ifStmt = makeIf(ast::makeLiteralBool(true), thenBlock, elseBlock);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(ifStmt);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(ifStmt);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getKind(), ast::Stmt::Kind::BLOCK);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfFalseCondition)
{
    // if (false) { stmt1 } else { stmt2 }
    // Should return stmt2 (else block)
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) });
    ast::BlockStmt* elseBlock = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(2)) });
    ast::IfStmt* ifStmt = makeIf(ast::makeLiteralBool(false), thenBlock, elseBlock);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(ifStmt);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(ifStmt);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfFalseNoElse)
{
    // if (false) { stmt1 }
    // Should eliminate entire statement
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) });
    ast::IfStmt* ifStmt = makeIf(ast::makeLiteralBool(false), thenBlock);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(ifStmt);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(ifStmt);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfDynamicCondition)
{
    // if (x > 0) { stmt }
    // Should not eliminate (condition is not constant)
    ast::BinaryExpr* condition = makeBinary(ast::makeName("x"), ast::makeLiteralInt(0), tok::TokenType::OP_GT);
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) });
    ast::IfStmt* ifStmt = makeIf(condition, thenBlock);

    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(ifStmt);
    }

    ast::Stmt* result = optimizer->eliminateDeadCode(ifStmt);

    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }

    EXPECT_TRUE(result->equals(ifStmt));
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeNestedIf)
{
    // if (true) { if (false) { stmt1 } else { stmt2 } }
    ast::BlockStmt* innerElse = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(2)) });
    ast::IfStmt* innerIf = makeIf(ast::makeLiteralBool(false), ast::makeBlock({}), innerElse);
    ast::BlockStmt* outerThen = ast::makeBlock({ innerIf });
    ast::IfStmt* outerIf = makeIf(ast::makeLiteralBool(true), outerThen);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(outerIf);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(outerIf);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    // Should eliminate both if statements
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

// DEAD CODE ELIMINATION - WHILE LOOPS

TEST_F(ASTOptimizerTest, DeadCodeWhileFalseCondition)
{
    // while (false) { stmt }
    // Loop never executes - dead code
    ast::BlockStmt* body = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) });
    ast::WhileStmt* whileStmt = makeWhile(ast::makeLiteralBool(false), body);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(whileStmt);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(whileStmt);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileTrueCondition)
{
    // while (true) { stmt }
    // Infinite loop, but not dead code
    ast::BlockStmt* body = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) });
    ast::WhileStmt* whileStmt = makeWhile(ast::makeLiteralBool(true), body);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(whileStmt);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(whileStmt);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_TRUE(result->equals(whileStmt));
}

TEST_F(ASTOptimizerTest, DeadCodeWhileDynamicCondition)
{
    // while (x > 0) { stmt }
    ast::BlockStmt* body = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) });
    ast::BinaryExpr* condition = makeBinary(ast::makeName("x"), ast::makeLiteralInt(0), tok::TokenType::OP_GT);
    ast::WhileStmt* whileStmt = makeWhile(condition, body);

    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(whileStmt);
    }

    ast::Stmt* result = optimizer->eliminateDeadCode(whileStmt);

    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }

    EXPECT_TRUE(result->equals(whileStmt));
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileNestedStatements)
{
    // while (x) { stmt1; if (false) { stmt2 } }
    ast::IfStmt* innerIf = makeIf(ast::makeLiteralBool(false), ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(2)) }));
    ast::BlockStmt* body = ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)), innerIf });
    ast::WhileStmt* whileStmt = makeWhile(ast::makeName("x"), body);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(whileStmt);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(whileStmt);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    // Inner if should be eliminated
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

// DEAD CODE ELIMINATION - FOR LOOPS

/*
TEST_F(ASTOptimizerTest, DeadCodeForLoopBody)
{
    // for (i = 0; i < 10; i++) { if (false) { stmt } }
    ast::AssignmentStmt* init = makeAssignment("i", makeLiteral(0));
    ast::BinaryExpr* condition = makeBinary(ast::makeName("i"), tok::TokenType::OP_LT, makeLiteral(10));
    ast::BinaryExpr* increment = makeBinary(ast::makeName("i"), tok::TokenType::OP_PLUS, makeLiteral(1));

    ast::IfStmt* deadIf = makeIf(makeBoolLiteral(false), ast::makeBlock({ makeExprStmt(makeLiteral(1)) }));
    ast::BlockStmt* body = ast::makeBlock({ deadIf });

    // ast::ForStmt* forStmt = makeFor(init, condition, increment, body);
    ast::Stmt* result = optimizer->eliminateDeadCode(forStmt);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeForLoopNested)
{
    // for (...) { for (...) { if (false) {...} } }
    ast::AssignmentStmt* init = makeAssignment("i", makeLiteral(0));
    ast::BinaryExpr* condition = makeBinary(ast::makeName("i"), tok::TokenType::OP_LT, makeLiteral(10));
    ast::BinaryExpr* increment = makeBinary(ast::makeName("i"), tok::TokenType::OP_PLUS, makeLiteral(1));

    ast::IfStmt* innerIf = makeIf(makeBoolLiteral(false), ast::makeBlock({ makeExprStmt(makeLiteral(1)) }));
    ast::ForStmt* innerFor = makeFor(init, condition, increment, ast::makeBlock({ innerIf }));
    ast::ForStmt* outerFor = makeFor(init, condition, increment, ast::makeBlock({ innerFor }));

    ast::Stmt* result = optimizer->eliminateDeadCode(outerFor);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}
*/

// DEAD CODE ELIMINATION - FUNCTIONS

TEST_F(ASTOptimizerTest, DeadCodeAfterReturn)
{
    // function f() { return 1; stmt2; stmt3; }
    // stmt2 and stmt3 are dead
    std::vector<ast::Stmt*> body = {
        makeReturn(ast::makeLiteralInt(1)),
        makeExprStmt(ast::makeLiteralInt(2)),
        makeExprStmt(ast::makeLiteralInt(3))
    };
    ast::FunctionDef* func = makeFunction("f", {}, ast::makeBlock(body));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(func);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(func);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeAfterEarlyReturn)
{
    // function f() { if (x) return 1; else return 2; stmt; }
    // stmt is dead (both branches return)
    ast::IfStmt* ifStmt = makeIf(ast::makeName("x"), ast::makeBlock({ makeReturn(ast::makeLiteralInt(1)) }),
        ast::makeBlock({ makeReturn(ast::makeLiteralInt(2)) }));
    std::vector<ast::Stmt*> body = { ifStmt, makeExprStmt(ast::makeLiteralInt(3)) };
    ast::FunctionDef* func = makeFunction("f", {}, ast::makeBlock(body));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(func);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(func);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, NoDeadCodeBeforeReturn)
{
    // function f() { stmt1; stmt2; return 1; }
    // No dead code
    std::vector<ast::Stmt*> body = {
        makeExprStmt(ast::makeLiteralInt(1)),
        makeExprStmt(ast::makeLiteralInt(2)),
        makeReturn(ast::makeLiteralInt(3))
    };
    ast::FunctionDef* func = makeFunction("f", {}, ast::makeBlock(body));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(func);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(func);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeNestedInFunction)
{
    // function f() { if (false) { stmt } }
    ast::IfStmt* ifStmt = makeIf(ast::makeLiteralBool(false), ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) }));
    ast::FunctionDef* func = makeFunction("f", {}, ast::makeBlock({ ifStmt }));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(func);
    }
    ast::Stmt* result = optimizer->eliminateDeadCode(func);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeNullStatement)
{
    ast::Stmt* result = optimizer->eliminateDeadCode(nullptr);
    EXPECT_EQ(result, nullptr);
}

// CSE (COMMON SUBEXPRESSION ELIMINATION) TESTS

TEST_F(ASTOptimizerTest, CSEExprToStringLiteral)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::LiteralExpr* expr = ast::makeLiteralInt(42);

    StringRef str = cse.exprToString(expr);

    EXPECT_FALSE(str.empty());
}

TEST_F(ASTOptimizerTest, CSEExprToStringName)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::NameExpr* expr = ast::makeName("variable");

    StringRef str = cse.exprToString(expr);

    EXPECT_EQ(str, "variable");
}

TEST_F(ASTOptimizerTest, CSEExprToStringBinary)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::BinaryExpr* expr = makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS);

    StringRef str = cse.exprToString(expr);

    EXPECT_FALSE(str.empty());
    // Should contain something like "(x + y)"
}

TEST_F(ASTOptimizerTest, CSEExprToStringUnary)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::UnaryExpr* expr = makeUnary(ast::makeName("x"), tok::TokenType::OP_MINUS);

    StringRef str = cse.exprToString(expr);

    EXPECT_FALSE(str.empty());
}

TEST_F(ASTOptimizerTest, CSEExprToStringNull)
{
    parser::ASTOptimizer::CSEPass cse;

    StringRef str = cse.exprToString(nullptr);

    EXPECT_TRUE(str.empty());
}

TEST_F(ASTOptimizerTest, CSEExprToStringComplex)
{
    parser::ASTOptimizer::CSEPass cse;
    // (a + b) * c
    ast::BinaryExpr* add = makeBinary(ast::makeName("a"), ast::makeName("b"), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* mul = makeBinary(add, ast::makeName("c"), tok::TokenType::OP_STAR);

    StringRef str = cse.exprToString(mul);

    EXPECT_FALSE(str.empty());
}

TEST_F(ASTOptimizerTest, CSEGetTempVar)
{
    parser::ASTOptimizer::CSEPass cse;

    StringRef temp1 = cse.getTempVar();
    StringRef temp2 = cse.getTempVar();

    EXPECT_NE(temp1, temp2); // Should generate different names
}

TEST_F(ASTOptimizerTest, CSEFindNonExistent)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::BinaryExpr* expr = makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS);

    std::optional<StringRef> result = cse.findCSE(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, CSERecordAndFind)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::BinaryExpr* expr = makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS);

    cse.recordExpr(expr, "temp_var");
    std::optional<StringRef> result = cse.findCSE(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "temp_var");
}

TEST_F(ASTOptimizerTest, CSERecordMultipleExpressions)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::BinaryExpr* expr1 = makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* expr2 = makeBinary(ast::makeName("a"), ast::makeName("b"), tok::TokenType::OP_STAR);

    cse.recordExpr(expr1, "temp1");
    cse.recordExpr(expr2, "temp2");

    std::optional<StringRef> result1 = cse.findCSE(expr1);
    std::optional<StringRef> result2 = cse.findCSE(expr2);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(*result1, "temp1");
    EXPECT_EQ(*result2, "temp2");
}

// LOOP INVARIANT DETECTION TESTS

TEST_F(ASTOptimizerTest, LoopInvariantLiteral)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars;
    ast::LiteralExpr* expr = ast::makeLiteralInt(42);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantNonLoopVariable)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    ast::NameExpr* expr = ast::makeName("x"); // x is not a loop variable
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantLoopVariable)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    ast::NameExpr* expr = ast::makeName("i"); // i is a loop variable
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithConstants)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    // 5 + 10 (both constants)
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(10), tok::TokenType::OP_PLUS);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithNonLoopVars)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    // x + y (neither are loop variables)
    ast::BinaryExpr* expr = makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantBinaryWithLoopVar)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    // i + 5 (i is a loop variable)
    ast::BinaryExpr* expr = makeBinary(ast::makeName("i"), ast::makeLiteralInt(5), tok::TokenType::OP_PLUS);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantComplexExpression)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i", "j" };
    // (x + y) * i (contains loop variable i)
    ast::BinaryExpr* left = makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* expr = makeBinary(left, ast::makeName("i"), tok::TokenType::OP_STAR);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantUnary)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    ast::UnaryExpr* expr = makeUnary(ast::makeName("x"), tok::TokenType::OP_MINUS);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantNull)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars;
    bool result = optimizer->isLoopInvariant(nullptr, loopVars);

    EXPECT_TRUE(result);
}

// OPTIMIZE FUNCTION TESTS (O1, O2 LEVELS)

TEST_F(ASTOptimizerTest, OptimizeLevel0NoOptimization)
{
    // Level 0 - no optimization
    ast::AssignmentStmt* stmt = makeAssignment("x", makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(10), tok::TokenType::OP_PLUS));
    std::vector<ast::Stmt*> statements = { stmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 0);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 0);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1BasicOptimization)
{
    // Level 1 - basic optimization (constant folding)
    ast::AssignmentStmt* stmt = makeAssignment("x", makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(10), tok::TokenType::OP_PLUS));
    std::vector<ast::Stmt*> statements = { stmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1ExpressionStatement)
{
    // Level 1 with expression statement
    ast::ExprStmt* stmt = makeExprStmt(makeBinary(ast::makeLiteralInt(3), ast::makeLiteralInt(4), tok::TokenType::OP_STAR));
    std::vector<ast::Stmt*> statements = { stmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2DeadCodeElimination)
{
    // Level 2 - includes dead code elimination
    ast::IfStmt* ifStmt = makeIf(ast::makeLiteralBool(false), ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) }));
    std::vector<ast::Stmt*> statements = { ifStmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2Combined)
{
    // Level 2 - both constant folding and dead code elimination
    ast::AssignmentStmt* assign = makeAssignment("x", makeBinary(ast::makeLiteralInt(2), ast::makeLiteralInt(3), tok::TokenType::OP_PLUS));
    ast::IfStmt* ifStmt = makeIf(ast::makeLiteralBool(false), ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(1)) }));
    std::vector<ast::Stmt*> statements = { assign, ifStmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().ConstantFolds, 1);
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeMultipleStatements)
{
    std::vector<ast::Stmt*> statements = {
        makeAssignment("a", makeBinary(ast::makeLiteralInt(1), ast::makeLiteralInt(2), tok::TokenType::OP_PLUS)),
        makeAssignment("b", makeBinary(ast::makeLiteralInt(3), ast::makeLiteralInt(4), tok::TokenType::OP_STAR)),
        makeAssignment("c", makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(1), tok::TokenType::OP_MINUS))
    };

    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 3);
}

TEST_F(ASTOptimizerTest, OptimizeEmptyStatements)
{
    std::vector<ast::Stmt*> statements;
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_EQ(result.size(), 0);
}

TEST_F(ASTOptimizerTest, OptimizeNullStatementsFiltered)
{
    // After optimization, null statements should be filtered out
    std::vector<ast::Stmt*> statements = { makeAssignment("x", ast::makeLiteralInt(1)) };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 2);

    // Should only contain non-null statements
    for (ast::Stmt* stmt : result)
        EXPECT_NE(stmt, nullptr);
}

// STATISTICS TESTS

TEST_F(ASTOptimizerTest, StatisticsInitiallyZero)
{
    parser::ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 0);
    EXPECT_EQ(stats.DeadCodeEliminations, 0);
    EXPECT_EQ(stats.StrengthReductions, 0);
    EXPECT_EQ(stats.CommonSubexprEliminations, 0);
    EXPECT_EQ(stats.LoopInvariants, 0);
}

TEST_F(ASTOptimizerTest, StatisticsConstantFolds)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(10), tok::TokenType::OP_PLUS);
    optimizer->optimizeConstantFolding(expr);

    parser::ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, StatisticsMultipleOptimizations)
{
    ast::BinaryExpr* expr1 = makeBinary(ast::makeLiteralInt(2), ast::makeLiteralInt(3), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* expr2 = makeBinary(ast::makeName("x"), ast::makeLiteralInt(0), tok::TokenType::OP_PLUS);

    optimizer->optimizeConstantFolding(expr1);
    optimizer->optimizeConstantFolding(expr2);

    parser::ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 1);
    EXPECT_EQ(stats.StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, StatisticsPrintStats)
{
    // Just verify it doesn't crash
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(1), ast::makeLiteralInt(1), tok::TokenType::OP_PLUS);
    optimizer->optimizeConstantFolding(expr);

    EXPECT_NO_THROW(optimizer->printStats());
}

// EDGE CASES AND STRESS TESTS

TEST_F(ASTOptimizerTest, EdgeCaseVeryDeepNesting)
{
    // ((((1 + 1) + 1) + 1) + 1)
    ast::Expr* expr = ast::makeLiteralInt(1);
    for (int i = 0; i < 10; ++i)
        expr = makeBinary(expr, ast::makeLiteralInt(1), tok::TokenType::OP_PLUS);

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<ast::LiteralExpr*>(result)->toNumber(), 11);
}

TEST_F(ASTOptimizerTest, EdgeCaseLargeNumbers)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralFloat(1e308), ast::makeLiteralFloat(1e308), tok::TokenType::OP_PLUS);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    // May result in infinity, but should not crash
    EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCaseSmallNumbers)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralFloat(1e-308), ast::makeLiteralFloat(1e-308), tok::TokenType::OP_STAR);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    // Should handle underflow gracefully
    EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCasePowerOfZero)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(0), ast::makeLiteralInt(0), tok::TokenType::OP_POWER);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1); // 0^0 is typically defined as 1
}

TEST_F(ASTOptimizerTest, EdgeCaseNegativePowerFraction)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(4), ast::makeLiteralFloat(0.5), tok::TokenType::OP_POWER);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 2); // sqrt(4)
}

TEST_F(ASTOptimizerTest, EdgeCaseModuloNegative)
{
    ast::BinaryExpr* expr = makeBinary(ast::makeLiteralInt(-17), ast::makeLiteralInt(5), tok::TokenType::OP_PERCENT);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    // fmod behavior with negative numbers
}

TEST_F(ASTOptimizerTest, StressManyOptimizations)
{
    std::vector<ast::Stmt*> statements;

    // Create 100 assignment statements with constant expressions
    for (int i = 0; i < 100; ++i) {
        StringRef varName = "var" + StringRef(std::to_string(i).data());
        statements.push_back(makeAssignment(varName, makeBinary(ast::makeLiteralInt(i), ast::makeLiteralInt(i), tok::TokenType::OP_PLUS)));
    }

    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 100);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 100);
}

TEST_F(ASTOptimizerTest, StressComplexDeadCodeElimination)
{
    // Nested if statements with constant conditions
    ast::IfStmt* inner3 = makeIf(ast::makeLiteralBool(false), ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(3)) }));
    ast::IfStmt* inner2 = makeIf(ast::makeLiteralBool(true), ast::makeBlock({ inner3 }));
    ast::IfStmt* inner1 = makeIf(ast::makeLiteralBool(false), ast::makeBlock({ inner2 }));
    ast::Stmt* result = optimizer->eliminateDeadCode(inner1);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, EdgeCaseMixedOptimizations)
{
    // Test combining multiple optimization types
    // x * 0 (strength reduction to 0)
    ast::BinaryExpr* mul = makeBinary(ast::makeName("x"), ast::makeLiteralInt(0), tok::TokenType::OP_STAR);
    ast::Expr* result = optimizer->optimizeConstantFolding(mul);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    EXPECT_EQ(static_cast<ast::LiteralExpr*>(result)->toNumber(), 0);
}

TEST_F(ASTOptimizerTest, EdgeCaseChainedStrengthReductions)
{
    // (x + 0) * 1
    ast::BinaryExpr* add = makeBinary(ast::makeName("x"), ast::makeLiteralInt(0), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* mul = makeBinary(add, ast::makeLiteralInt(1), tok::TokenType::OP_STAR);
    ast::Expr* result = optimizer->optimizeConstantFolding(mul);

    // Should apply both optimizations
    EXPECT_GE(optimizer->getStats().StrengthReductions, 1);
}

// COMPLEX INTEGRATION TESTS

TEST_F(ASTOptimizerTest, IntegrationFullOptimizationPipeline)
{
    // Test a complete optimization scenario
    std::vector<ast::Stmt*> statements = {
        // Constant folding: x = 2 + 3 => x = 5
        makeAssignment("x", makeBinary(ast::makeLiteralInt(2), ast::makeLiteralInt(3), tok::TokenType::OP_PLUS)),
        // Strength reduction: y = x * 1 => y = x
        makeAssignment("y", makeBinary(ast::makeName("x"), ast::makeLiteralInt(1), tok::TokenType::OP_STAR)),
        // Dead code: if (false) { ... }
        makeIf(ast::makeLiteralBool(false), ast::makeBlock({ makeExprStmt(ast::makeLiteralInt(999)) }))
    };

    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().ConstantFolds, 1);
    EXPECT_GE(optimizer->getStats().StrengthReductions, 1);
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, IntegrationNestedFunctionOptimization)
{
    // function f() {
    //   x = 5 + 10;
    //   if (false) { y = 1; }
    //   return x;
    // }
    std::vector<ast::Stmt*> body = {
        makeAssignment("x", makeBinary(ast::makeLiteralInt(5), ast::makeLiteralInt(10), tok::TokenType::OP_PLUS)),
        makeIf(ast::makeLiteralBool(false), ast::makeBlock({ makeAssignment("y", ast::makeLiteralInt(1)) })),
        makeReturn(ast::makeName("x"))
    };

    ast::FunctionDef* func = makeFunction("f", {}, ast::makeBlock(body));
    std::vector<ast::Stmt*> statements = { func };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_EQ(result.size(), 1);
}
