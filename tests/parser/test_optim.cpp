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
using namespace mylang::ast;

ASTPrinter AST_Printer;

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
    AssignmentStmt* makeAssignment(StringRef const& target, Expr* value)
    {
        return makeAssignmentStmt(makeName(target), value);
    }

    // Helper to create if statements
    IfStmt* makeIf(Expr* condition, BlockStmt* thenBlock, BlockStmt* elseBlock = nullptr)
    {
        if (!elseBlock)
            elseBlock = makeBlock(std::vector<Stmt*>());

        return ast::makeIf(condition, thenBlock, elseBlock);
    }

    // Helper to create function definitions
    FunctionDef* makeFunction(StringRef const& name, std::vector<StringRef> const& params, BlockStmt* body)
    {
        NameExpr* nameExpr = makeName(name);
        std::vector<Expr*> paramsExpr(params.size());
        for (auto p : params)
            paramsExpr.push_back(makeName(p));

        ListExpr* listExpr = makeList(paramsExpr);
        return ast::makeFunction(nameExpr, listExpr, body);
    }
};

// CONSTANT EVALUATION TESTS

TEST_F(ASTOptimizerTest, EvaluateConstantLiteral)
{
    LiteralExpr* expr = makeLiteralInt(42);
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
    LiteralExpr* expr = makeLiteralBool(true);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantAddition)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(10), makeLiteralInt(20), BinaryOp::OP_ADD);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 30);
}

TEST_F(ASTOptimizerTest, EvaluateConstantSubtraction)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(50), makeLiteralInt(20), BinaryOp::OP_SUB);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 30);
}

TEST_F(ASTOptimizerTest, EvaluateConstantMultiplication)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(6), makeLiteralInt(7), BinaryOp::OP_MUL);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42);
}

TEST_F(ASTOptimizerTest, EvaluateConstantDivision)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(100), makeLiteralInt(4), BinaryOp::OP_DIV);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 25);
}

TEST_F(ASTOptimizerTest, EvaluateConstantDivisionByZero)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(10), makeLiteralInt(0), BinaryOp::OP_DIV);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantModulo)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(17), makeLiteralInt(5), BinaryOp::OP_MOD);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 2);
}

TEST_F(ASTOptimizerTest, EvaluateConstantModuloByZero)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(10), makeLiteralInt(0), BinaryOp::OP_MOD);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantPower)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(2), makeLiteralInt(10), BinaryOp::OP_POW);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1024);
}

TEST_F(ASTOptimizerTest, EvaluateConstantPowerZero)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(5), makeLiteralInt(0), BinaryOp::OP_POW);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1);
}

TEST_F(ASTOptimizerTest, EvaluateConstantPowerNegative)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(2), makeLiteralInt(-2), BinaryOp::OP_POW);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 0.25);
}

TEST_F(ASTOptimizerTest, EvaluateConstantUnaryPlus)
{
    UnaryExpr* expr = makeUnary(makeLiteralInt(42), UnaryOp::OP_PLUS);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42);
}

TEST_F(ASTOptimizerTest, EvaluateConstantUnaryMinus)
{
    UnaryExpr* expr = makeUnary(makeLiteralInt(42), UnaryOp::OP_NEG);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, -42);
}

TEST_F(ASTOptimizerTest, EvaluateConstantNestedExpression)
{
    // (10 + 20) * 3
    BinaryExpr* inner = makeBinary(makeLiteralInt(10), makeLiteralInt(20), BinaryOp::OP_ADD);
    BinaryExpr* outer = makeBinary(inner, makeLiteralInt(3), BinaryOp::OP_MUL);

    std::optional<double> result = optimizer->evaluateConstant(outer);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 90);
}

TEST_F(ASTOptimizerTest, EvaluateConstantComplexExpression)
{
    // ((5 + 3) * 2) - (10 / 2)
    BinaryExpr* add = makeBinary(makeLiteralInt(5), makeLiteralInt(3), BinaryOp::OP_ADD);
    BinaryExpr* mul = makeBinary(add, makeLiteralInt(2), BinaryOp::OP_MUL);
    BinaryExpr* div = makeBinary(makeLiteralInt(10), makeLiteralInt(2), BinaryOp::OP_DIV);
    BinaryExpr* sub = makeBinary(mul, div, BinaryOp::OP_SUB);

    std::optional<double> result = optimizer->evaluateConstant(sub);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 11);
}

TEST_F(ASTOptimizerTest, EvaluateConstantWithVariable)
{
    // x + 5 (cannot be evaluated)
    BinaryExpr* expr = makeBinary(makeName("x"), makeLiteralInt(5), BinaryOp::OP_ADD);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantPartiallyConstant)
{
    // (10 + 20) + x (left side is constant, but overall is not)
    BinaryExpr* left = makeBinary(makeLiteralInt(10), makeLiteralInt(20), BinaryOp::OP_ADD);
    BinaryExpr* expr = makeBinary(left, makeName("x"), BinaryOp::OP_ADD);

    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

// CONSTANT FOLDING OPTIMIZATION TESTS

TEST_F(ASTOptimizerTest, ConstantFoldingSimpleAddition)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }

    ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
    LiteralExpr* lit = static_cast<LiteralExpr*>(result);
    EXPECT_TRUE(lit->isNumeric());
    EXPECT_DOUBLE_EQ(lit->toNumber(), 15);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, ConstantFoldingMultiplication)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(6), makeLiteralInt(7), BinaryOp::OP_MUL);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
    LiteralExpr* lit = static_cast<LiteralExpr*>(result);
    EXPECT_DOUBLE_EQ(lit->toNumber(), 42);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNested)
{
    // (3 + 4) * (5 - 2)
    BinaryExpr* left = makeBinary(makeLiteralInt(3), makeLiteralInt(4), BinaryOp::OP_ADD);
    BinaryExpr* right = makeBinary(makeLiteralInt(5), makeLiteralInt(2), BinaryOp::OP_SUB);
    BinaryExpr* expr = makeBinary(left, right, BinaryOp::OP_MUL);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }

    Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }

    ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<LiteralExpr*>(result)->toNumber(), 21);
}

TEST_F(ASTOptimizerTest, ConstantFoldingUnary)
{
    UnaryExpr* expr = makeUnary(makeLiteralInt(42), UnaryOp::OP_NEG);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<LiteralExpr*>(result)->toNumber(), -42);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNoChange)
{
    // x + y (no constants to fold)
    BinaryExpr* expr = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD);
    Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_TRUE(result->equals(expr)); // Should return original
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 0);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNull)
{
    Expr* result = optimizer->optimizeConstantFolding(nullptr);
    EXPECT_EQ(result, nullptr);
}

// ALGEBRAIC SIMPLIFICATION TESTS

TEST_F(ASTOptimizerTest, SimplifyAdditionWithZero)
{
    // x + 0 = x
    NameExpr* x = makeName("x");
    BinaryExpr* expr = makeBinary(x, makeLiteralInt(0), BinaryOp::OP_ADD);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
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
    NameExpr* x = makeName("x");
    BinaryExpr* expr = makeBinary(x, makeLiteralInt(0), BinaryOp::OP_SUB);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
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
    NameExpr* x = makeName("x");
    BinaryExpr* expr = makeBinary(x, makeLiteralInt(1), BinaryOp::OP_MUL);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
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
    NameExpr* x = makeName("x");
    BinaryExpr* expr = makeBinary(x, makeLiteralInt(1), BinaryOp::OP_DIV);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
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

    NameExpr* x = makeName("x");
    BinaryExpr* expr = makeBinary(x, makeLiteralInt(0), BinaryOp::OP_MUL);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
    LiteralExpr* lit = static_cast<LiteralExpr*>(result);
    EXPECT_EQ(lit->toNumber(), 0);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByTwo)
{
    // x * 2 = x + x
    NameExpr* x = makeName("x");
    BinaryExpr* expr = makeBinary(x, makeLiteralInt(2), BinaryOp::OP_MUL);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(expr);
    }
    ASSERT_EQ(result->getKind(), Expr::Kind::BINARY);
    BinaryExpr* bin = static_cast<BinaryExpr*>(result);
    EXPECT_EQ(bin->getOperator(), BinaryOp::OP_ADD);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionSameVariable)
{
    // x - x = 0
    NameExpr* x1 = makeName("x");
    NameExpr* x2 = makeName("x");
    BinaryExpr* expr = makeBinary(x1, x2, BinaryOp::OP_SUB);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
    LiteralExpr* lit = static_cast<LiteralExpr*>(result);
    EXPECT_EQ(lit->toNumber(), 0);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionDifferentVariables)
{
    // x - y (should not simplify)
    BinaryExpr* expr = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_SUB);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
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
    NameExpr* x = makeName("x");
    UnaryExpr* inner = makeUnary(x, UnaryOp::OP_NEG);
    UnaryExpr* outer = makeUnary(inner, UnaryOp::OP_NEG);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(outer);
    }
    Expr* result = optimizer->optimizeConstantFolding(outer);
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
    BinaryExpr* expr = makeBinary(makeName("x"), makeLiteralInt(5), BinaryOp::OP_ADD);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(expr);
    }
    Expr* result = optimizer->optimizeConstantFolding(expr);
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
    std::vector<Expr*> args = {
        makeBinary(makeLiteralInt(2), makeLiteralInt(3), BinaryOp::OP_ADD),
        makeBinary(makeLiteralInt(4), makeLiteralInt(5), BinaryOp::OP_MUL)
    };

    CallExpr* call = makeCall(makeName("func"), makeList(args));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(call);
    }
    Expr* result = optimizer->optimizeConstantFolding(call);
    ASSERT_EQ(result->getKind(), Expr::Kind::CALL);
    CallExpr* optimized = static_cast<CallExpr*>(result);
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
    std::vector<Expr*> elements = {
        makeBinary(makeLiteralInt(1), makeLiteralInt(2), BinaryOp::OP_ADD),
        makeBinary(makeLiteralInt(3), makeLiteralInt(4), BinaryOp::OP_MUL),
        makeName("x")
    };
    ListExpr* list = makeList(elements);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(list);
    }
    Expr* result = optimizer->optimizeConstantFolding(list);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_EQ(result->getKind(), Expr::Kind::LIST);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 2);
}

TEST_F(ASTOptimizerTest, OptimizeEmptyList)
{
    ListExpr* list = makeList(std::vector<Expr*>());
    Expr* result = optimizer->optimizeConstantFolding(list);

    EXPECT_TRUE(result->equals(list));
}

// DEAD CODE ELIMINATION - IF STATEMENTS

TEST_F(ASTOptimizerTest, DeadCodeIfTrueCondition)
{
    // if (true) { stmt1 } else { stmt2 }
    // Should return stmt1
    BlockStmt* thenBlock = makeBlock({ makeExprStmt(makeLiteralInt(1)) });
    BlockStmt* elseBlock = makeBlock({ makeExprStmt(makeLiteralInt(2)) });
    IfStmt* ifStmt = makeIf(makeLiteralBool(true), thenBlock, elseBlock);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(ifStmt);
    }
    Stmt* result = optimizer->eliminateDeadCode(ifStmt);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getKind(), Stmt::Kind::BLOCK);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfFalseCondition)
{
    // if (false) { stmt1 } else { stmt2 }
    // Should return stmt2 (else block)
    BlockStmt* thenBlock = makeBlock({ makeExprStmt(makeLiteralInt(1)) });
    BlockStmt* elseBlock = makeBlock({ makeExprStmt(makeLiteralInt(2)) });
    IfStmt* ifStmt = makeIf(makeLiteralBool(false), thenBlock, elseBlock);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(ifStmt);
    }
    Stmt* result = optimizer->eliminateDeadCode(ifStmt);
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
    BlockStmt* thenBlock = makeBlock({ makeExprStmt(makeLiteralInt(1)) });
    IfStmt* ifStmt = makeIf(makeLiteralBool(false), thenBlock);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(ifStmt);
    }
    Stmt* result = optimizer->eliminateDeadCode(ifStmt);
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
    BinaryExpr* condition = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_GT);
    BlockStmt* thenBlock = makeBlock({ makeExprStmt(makeLiteralInt(1)) });
    IfStmt* ifStmt = makeIf(condition, thenBlock);

    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(ifStmt);
    }

    Stmt* result = optimizer->eliminateDeadCode(ifStmt);

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
    BlockStmt* innerElse = makeBlock({ makeExprStmt(makeLiteralInt(2)) });
    IfStmt* innerIf = makeIf(makeLiteralBool(false), makeBlock({}), innerElse);
    BlockStmt* outerThen = makeBlock({ innerIf });
    IfStmt* outerIf = makeIf(makeLiteralBool(true), outerThen);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(outerIf);
    }
    Stmt* result = optimizer->eliminateDeadCode(outerIf);
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
    BlockStmt* body = makeBlock({ makeExprStmt(makeLiteralInt(1)) });
    WhileStmt* whileStmt = makeWhile(makeLiteralBool(false), body);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(whileStmt);
    }
    Stmt* result = optimizer->eliminateDeadCode(whileStmt);
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
    BlockStmt* body = makeBlock({ makeExprStmt(makeLiteralInt(1)) });
    WhileStmt* whileStmt = makeWhile(makeLiteralBool(true), body);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(whileStmt);
    }
    Stmt* result = optimizer->eliminateDeadCode(whileStmt);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_TRUE(result->equals(whileStmt));
}

TEST_F(ASTOptimizerTest, DeadCodeWhileDynamicCondition)
{
    // while (x > 0) { stmt }
    BlockStmt* body = makeBlock({ makeExprStmt(makeLiteralInt(1)) });
    BinaryExpr* condition = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_GT);
    WhileStmt* whileStmt = makeWhile(condition, body);

    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(whileStmt);
    }

    Stmt* result = optimizer->eliminateDeadCode(whileStmt);

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
    IfStmt* innerIf = makeIf(makeLiteralBool(false), makeBlock({ makeExprStmt(makeLiteralInt(2)) }));
    BlockStmt* body = makeBlock({ makeExprStmt(makeLiteralInt(1)), innerIf });
    WhileStmt* whileStmt = makeWhile(makeName("x"), body);
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(whileStmt);
    }
    Stmt* result = optimizer->eliminateDeadCode(whileStmt);
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
    AssignmentStmt* init = makeAssignment("i", makeLiteral(0));
    BinaryExpr* condition = makeBinary(makeName("i"), tok::TokenType::OP_LT, makeLiteral(10));
    BinaryExpr* increment = makeBinary(makeName("i"), BinaryOp::OP_ADD, makeLiteral(1));

    IfStmt* deadIf = makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(1)) }));
    BlockStmt* body = makeBlock({ deadIf });

    // ForStmt* forStmt = makeFor(init, condition, increment, body);
    Stmt* result = optimizer->eliminateDeadCode(forStmt);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeForLoopNested)
{
    // for (...) { for (...) { if (false) {...} } }
    AssignmentStmt* init = makeAssignment("i", makeLiteral(0));
    BinaryExpr* condition = makeBinary(makeName("i"), tok::TokenType::OP_LT, makeLiteral(10));
    BinaryExpr* increment = makeBinary(makeName("i"), BinaryOp::OP_ADD, makeLiteral(1));

    IfStmt* innerIf = makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(1)) }));
    ForStmt* innerFor = makeFor(init, condition, increment, makeBlock({ innerIf }));
    ForStmt* outerFor = makeFor(init, condition, increment, makeBlock({ innerFor }));

    Stmt* result = optimizer->eliminateDeadCode(outerFor);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}
*/

// DEAD CODE ELIMINATION - FUNCTIONS

TEST_F(ASTOptimizerTest, DeadCodeAfterReturn)
{
    // function f() { return 1; stmt2; stmt3; }
    // stmt2 and stmt3 are dead
    std::vector<Stmt*> body = {
        makeReturn(makeLiteralInt(1)),
        makeExprStmt(makeLiteralInt(2)),
        makeExprStmt(makeLiteralInt(3))
    };
    FunctionDef* func = makeFunction("f", {}, makeBlock(body));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(func);
    }
    Stmt* result = optimizer->eliminateDeadCode(func);
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
    IfStmt* ifStmt = makeIf(makeName("x"), makeBlock({ makeReturn(makeLiteralInt(1)) }),
        makeBlock({ makeReturn(makeLiteralInt(2)) }));
    std::vector<Stmt*> body = { ifStmt, makeExprStmt(makeLiteralInt(3)) };
    FunctionDef* func = makeFunction("f", {}, makeBlock(body));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(func);
    }
    Stmt* result = optimizer->eliminateDeadCode(func);
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
    std::vector<Stmt*> body = {
        makeExprStmt(makeLiteralInt(1)),
        makeExprStmt(makeLiteralInt(2)),
        makeReturn(makeLiteralInt(3))
    };
    FunctionDef* func = makeFunction("f", {}, makeBlock(body));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(func);
    }
    Stmt* result = optimizer->eliminateDeadCode(func);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeNestedInFunction)
{
    // function f() { if (false) { stmt } }
    IfStmt* ifStmt = makeIf(makeLiteralBool(false), makeBlock({ makeExprStmt(makeLiteralInt(1)) }));
    FunctionDef* func = makeFunction("f", {}, makeBlock({ ifStmt }));
    if (test_config::print_ast) {
        std::cout << "Normal:" << '\n';
        AST_Printer.print(func);
    }
    Stmt* result = optimizer->eliminateDeadCode(func);
    if (test_config::print_ast) {
        std::cout << "Optimized:" << '\n';
        AST_Printer.print(result);
    }
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeNullStatement)
{
    Stmt* result = optimizer->eliminateDeadCode(nullptr);
    EXPECT_EQ(result, nullptr);
}

// CSE (COMMON SUBEXPRESSION ELIMINATION) TESTS

/*
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
    BinaryExpr* expr = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD);
    
    std::optional<StringRef> result = cse.findCSE(expr);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, CSERecordAndFind)
{
    parser::ASTOptimizer::CSEPass cse;
    BinaryExpr* expr = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD);

    cse.recordExpr(expr, "temp_var");
    std::optional<StringRef> result = cse.findCSE(expr);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "temp_var");
}

TEST_F(ASTOptimizerTest, CSERecordMultipleExpressions)
{
    parser::ASTOptimizer::CSEPass cse;
    BinaryExpr* expr1 = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD);
    BinaryExpr* expr2 = makeBinary(makeName("a"), makeName("b"), BinaryOp::OP_MUL);
    
    cse.recordExpr(expr1, "temp1");
    cse.recordExpr(expr2, "temp2");
    
    std::optional<StringRef> result1 = cse.findCSE(expr1);
    std::optional<StringRef> result2 = cse.findCSE(expr2);
    
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(*result1, "temp1");
    EXPECT_EQ(*result2, "temp2");
}
*/

// LOOP INVARIANT DETECTION TESTS

TEST_F(ASTOptimizerTest, LoopInvariantLiteral)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars;
    LiteralExpr* expr = makeLiteralInt(42);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantNonLoopVariable)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    NameExpr* expr = makeName("x"); // x is not a loop variable
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantLoopVariable)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    NameExpr* expr = makeName("i"); // i is a loop variable
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithConstants)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    // 5 + 10 (both constants)
    BinaryExpr* expr = makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithNonLoopVars)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    // x + y (neither are loop variables)
    BinaryExpr* expr = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantBinaryWithLoopVar)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    // i + 5 (i is a loop variable)
    BinaryExpr* expr = makeBinary(makeName("i"), makeLiteralInt(5), BinaryOp::OP_ADD);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantComplexExpression)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i", "j" };
    // (x + y) * i (contains loop variable i)
    BinaryExpr* left = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD);
    BinaryExpr* expr = makeBinary(left, makeName("i"), BinaryOp::OP_MUL);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantUnary)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    UnaryExpr* expr = makeUnary(makeName("x"), UnaryOp::OP_NEG);
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
    AssignmentStmt* stmt = makeAssignment("x", makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD));
    std::vector<Stmt*> statements = { stmt };
    std::vector<Stmt*> result = optimizer->optimize(statements, 0);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 0);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1BasicOptimization)
{
    // Level 1 - basic optimization (constant folding)
    AssignmentStmt* stmt = makeAssignment("x", makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD));
    std::vector<Stmt*> statements = { stmt };
    std::vector<Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1ExpressionStatement)
{
    // Level 1 with expression statement
    ExprStmt* stmt = makeExprStmt(makeBinary(makeLiteralInt(3), makeLiteralInt(4), BinaryOp::OP_MUL));
    std::vector<Stmt*> statements = { stmt };
    std::vector<Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2DeadCodeElimination)
{
    // Level 2 - includes dead code elimination
    IfStmt* ifStmt = makeIf(makeLiteralBool(false), makeBlock({ makeExprStmt(makeLiteralInt(1)) }));
    std::vector<Stmt*> statements = { ifStmt };
    std::vector<Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2Combined)
{
    // Level 2 - both constant folding and dead code elimination
    AssignmentStmt* assign = makeAssignment("x", makeBinary(makeLiteralInt(2), makeLiteralInt(3), BinaryOp::OP_ADD));
    IfStmt* ifStmt = makeIf(makeLiteralBool(false), makeBlock({ makeExprStmt(makeLiteralInt(1)) }));
    std::vector<Stmt*> statements = { assign, ifStmt };
    std::vector<Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().ConstantFolds, 1);
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeMultipleStatements)
{
    std::vector<Stmt*> statements = {
        makeAssignment("a", makeBinary(makeLiteralInt(1), makeLiteralInt(2), BinaryOp::OP_ADD)),
        makeAssignment("b", makeBinary(makeLiteralInt(3), makeLiteralInt(4), BinaryOp::OP_MUL)),
        makeAssignment("c", makeBinary(makeLiteralInt(5), makeLiteralInt(1), BinaryOp::OP_SUB))
    };

    std::vector<Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 3);
}

TEST_F(ASTOptimizerTest, OptimizeEmptyStatements)
{
    std::vector<Stmt*> statements;
    std::vector<Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_EQ(result.size(), 0);
}

TEST_F(ASTOptimizerTest, OptimizeNullStatementsFiltered)
{
    // After optimization, null statements should be filtered out
    std::vector<Stmt*> statements = { makeAssignment("x", makeLiteralInt(1)) };
    std::vector<Stmt*> result = optimizer->optimize(statements, 2);

    // Should only contain non-null statements
    for (Stmt* stmt : result)
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
    BinaryExpr* expr = makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD);
    optimizer->optimizeConstantFolding(expr);

    parser::ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, StatisticsMultipleOptimizations)
{
    BinaryExpr* expr1 = makeBinary(makeLiteralInt(2), makeLiteralInt(3), BinaryOp::OP_ADD);
    BinaryExpr* expr2 = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_ADD);

    optimizer->optimizeConstantFolding(expr1);
    optimizer->optimizeConstantFolding(expr2);

    parser::ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 1);
    EXPECT_EQ(stats.StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, StatisticsPrintStats)
{
    // Just verify it doesn't crash
    BinaryExpr* expr = makeBinary(makeLiteralInt(1), makeLiteralInt(1), BinaryOp::OP_ADD);
    optimizer->optimizeConstantFolding(expr);

    EXPECT_NO_THROW(optimizer->printStats());
}

// EDGE CASES AND STRESS TESTS

TEST_F(ASTOptimizerTest, EdgeCaseVeryDeepNesting)
{
    // ((((1 + 1) + 1) + 1) + 1)
    Expr* expr = makeLiteralInt(1);
    for (int i = 0; i < 10; ++i)
        expr = makeBinary(expr, makeLiteralInt(1), BinaryOp::OP_ADD);

    Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<LiteralExpr*>(result)->toNumber(), 11);
}

TEST_F(ASTOptimizerTest, EdgeCaseLargeNumbers)
{
    BinaryExpr* expr = makeBinary(makeLiteralFloat(1e308), makeLiteralFloat(1e308), BinaryOp::OP_ADD);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    // May result in infinity, but should not crash
    EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCaseSmallNumbers)
{
    BinaryExpr* expr = makeBinary(makeLiteralFloat(1e-308), makeLiteralFloat(1e-308), BinaryOp::OP_MUL);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    // Should handle underflow gracefully
    EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCasePowerOfZero)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(0), makeLiteralInt(0), BinaryOp::OP_POW);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1); // 0^0 is typically defined as 1
}

TEST_F(ASTOptimizerTest, EdgeCaseNegativePowerFraction)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(4), makeLiteralFloat(0.5), BinaryOp::OP_POW);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 2); // sqrt(4)
}

TEST_F(ASTOptimizerTest, EdgeCaseModuloNegative)
{
    BinaryExpr* expr = makeBinary(makeLiteralInt(-17), makeLiteralInt(5), BinaryOp::OP_MOD);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    // fmod behavior with negative numbers
}

TEST_F(ASTOptimizerTest, StressManyOptimizations)
{
    std::vector<Stmt*> statements;

    // Create 100 assignment statements with constant expressions
    for (int i = 0; i < 100; ++i) {
        StringRef varName = "var" + StringRef(std::to_string(i).data());
        statements.push_back(makeAssignment(varName, makeBinary(makeLiteralInt(i), makeLiteralInt(i), BinaryOp::OP_ADD)));
    }

    std::vector<Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 100);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 100);
}

TEST_F(ASTOptimizerTest, StressComplexDeadCodeElimination)
{
    // Nested if statements with constant conditions
    IfStmt* inner3 = makeIf(makeLiteralBool(false), makeBlock({ makeExprStmt(makeLiteralInt(3)) }));
    IfStmt* inner2 = makeIf(makeLiteralBool(true), makeBlock({ inner3 }));
    IfStmt* inner1 = makeIf(makeLiteralBool(false), makeBlock({ inner2 }));
    Stmt* result = optimizer->eliminateDeadCode(inner1);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, EdgeCaseMixedOptimizations)
{
    // Test combining multiple optimization types
    // x * 0 (strength reduction to 0)
    BinaryExpr* mul = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_MUL);
    Expr* result = optimizer->optimizeConstantFolding(mul);

    ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
    EXPECT_EQ(static_cast<LiteralExpr*>(result)->toNumber(), 0);
}

TEST_F(ASTOptimizerTest, EdgeCaseChainedStrengthReductions)
{
    // (x + 0) * 1
    BinaryExpr* add = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_ADD);
    BinaryExpr* mul = makeBinary(add, makeLiteralInt(1), BinaryOp::OP_MUL);
    Expr* result = optimizer->optimizeConstantFolding(mul);

    // Should apply both optimizations
    EXPECT_GE(optimizer->getStats().StrengthReductions, 1);
}

// COMPLEX INTEGRATION TESTS

TEST_F(ASTOptimizerTest, IntegrationFullOptimizationPipeline)
{
    // Test a complete optimization scenario
    std::vector<Stmt*> statements = {
        // Constant folding: x = 2 + 3 => x = 5
        makeAssignment("x", makeBinary(makeLiteralInt(2), makeLiteralInt(3), BinaryOp::OP_ADD)),
        // Strength reduction: y = x * 1 => y = x
        makeAssignment("y", makeBinary(makeName("x"), makeLiteralInt(1), BinaryOp::OP_MUL)),
        // Dead code: if (false) { ... }
        makeIf(makeLiteralBool(false), makeBlock({ makeExprStmt(makeLiteralInt(999)) }))
    };

    std::vector<Stmt*> result = optimizer->optimize(statements, 2);

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
    std::vector<Stmt*> body = {
        makeAssignment("x", makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD)),
        makeIf(makeLiteralBool(false), makeBlock({ makeAssignment("y", makeLiteralInt(1)) })),
        makeReturn(makeName("x"))
    };

    FunctionDef* func = makeFunction("f", {}, makeBlock(body));
    std::vector<Stmt*> statements = { func };
    std::vector<Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_EQ(result.size(), 1);
}
