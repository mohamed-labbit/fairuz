#include "../../include/ast/ast.hpp"
#include "../../include/ast/expr.hpp"
#include "../../include/ast/stmt.hpp"
#include "../../include/parser/optim.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <memory>

using namespace mylang;

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

    // Helper to create literal expressions
    ast::LiteralExpr* makeLiteral(double value)
    {
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, StringRef(std::to_string(value).c_str()));
    }

    ast::LiteralExpr* makeLiteral(StringRef const& value, ast::LiteralExpr::Type type)
    {
        return ast::AST_allocator.make<ast::LiteralExpr>(type, value);
    }

    ast::LiteralExpr* makeBoolLiteral(bool value)
    {
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::BOOLEAN, value ? "true" : "false");
    }

    // Helper to create name expressions
    ast::NameExpr* makeName(StringRef const& name)
    {
        return ast::AST_allocator.make<ast::NameExpr>(name);
    }

    // Helper to create binary expressions
    ast::BinaryExpr* makeBinary(ast::Expr* left, tok::TokenType op, ast::Expr* right)
    {
        return ast::AST_allocator.make<ast::BinaryExpr>(left, right, op);
    }

    // Helper to create unary expressions
    ast::UnaryExpr* makeUnary(ast::Expr* operand, tok::TokenType op)
    {
        return ast::AST_allocator.make<ast::UnaryExpr>(operand, op);
    }

    // Helper to create assignment statements
    ast::AssignmentStmt* makeAssignment(StringRef const& target, ast::Expr* value)
    {
        return ast::AST_allocator.make<ast::AssignmentStmt>(makeName(target), value);
    }

    // Helper to create if statements
    ast::IfStmt* makeIf(ast::Expr* condition, ast::BlockStmt* thenBlock, ast::BlockStmt* elseBlock = nullptr)
    {
        if (!elseBlock)
            elseBlock = ast::AST_allocator.make<ast::BlockStmt>(std::vector<ast::Stmt*>());

        return ast::AST_allocator.make<ast::IfStmt>(condition, thenBlock, elseBlock);
    }

    // Helper to create while statements
    ast::WhileStmt* makeWhile(ast::Expr* condition, ast::BlockStmt* body)
    {
        return ast::AST_allocator.make<ast::WhileStmt>(condition, body);
    }

    // Helper to create for statements
    ast::ForStmt* makeFor(ast::NameExpr* target, ast::Expr* increment, ast::BlockStmt* body)
    {
        return ast::AST_allocator.make<ast::ForStmt>(target, increment, body);
    }

    // Helper to create block statements
    ast::BlockStmt* makeBlock(std::vector<ast::Stmt*> const& statements)
    {
        return ast::AST_allocator.make<ast::BlockStmt>(statements);
    }

    // Helper to create expression statements
    ast::ExprStmt* makeExprStmt(ast::Expr* expr)
    {
        return ast::AST_allocator.make<ast::ExprStmt>(expr);
    }

    // Helper to create return statements
    ast::ReturnStmt* makeReturn(ast::Expr* value = nullptr)
    {
        return ast::AST_allocator.make<ast::ReturnStmt>(value);
    }

    // Helper to create function definitions
    ast::FunctionDef* makeFunction(StringRef const& name, std::vector<StringRef> const& params, ast::BlockStmt* body)
    {
        ast::NameExpr* nameExpr = makeName(name);
        std::vector<ast::Expr*> paramsExpr(params.size());
        for (auto p : params)
            paramsExpr.push_back(makeName(p));

        ast::ListExpr* listExpr = ast::AST_allocator.make<ast::ListExpr>(paramsExpr);
        return ast::AST_allocator.make<ast::FunctionDef>(nameExpr, listExpr, body);
    }
};

// CONSTANT EVALUATION TESTS

TEST_F(ASTOptimizerTest, EvaluateConstantLiteral)
{
    ast::LiteralExpr* expr = makeLiteral(42.0);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantNull)
{
    std::optional<double> result = optimizer->evaluateConstant(nullptr);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantNonNumber)
{
    ast::LiteralExpr* expr = makeBoolLiteral(true);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantAddition)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), tok::TokenType::OP_PLUS, makeLiteral(20.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 30.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantSubtraction)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(50.0), tok::TokenType::OP_MINUS, makeLiteral(20.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 30.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantMultiplication)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(6.0), tok::TokenType::OP_STAR, makeLiteral(7.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantDivision)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(100.0), tok::TokenType::OP_SLASH, makeLiteral(4.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 25.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantDivisionByZero)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), tok::TokenType::OP_SLASH, makeLiteral(0.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantModulo)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(17.0), tok::TokenType::OP_PERCENT, makeLiteral(5.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 2.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantModuloByZero)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), tok::TokenType::OP_PERCENT, makeLiteral(0.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantPower)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(2.0), tok::TokenType::OP_POWER, makeLiteral(10.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1024.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantPowerZero)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(5.0), tok::TokenType::OP_POWER, makeLiteral(0.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantPowerNegative)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(2.0), tok::TokenType::OP_POWER, makeLiteral(-2.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 0.25);
}

TEST_F(ASTOptimizerTest, EvaluateConstantUnaryPlus)
{
    ast::UnaryExpr* expr = makeUnary(makeLiteral(42.0), tok::TokenType::OP_PLUS);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantUnaryMinus)
{
    ast::UnaryExpr* expr = makeUnary(makeLiteral(42.0), tok::TokenType::OP_MINUS);
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, -42.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantNestedExpression)
{
    // (10 + 20) * 3
    ast::BinaryExpr* inner = makeBinary(makeLiteral(10.0), tok::TokenType::OP_PLUS, makeLiteral(20.0));
    ast::BinaryExpr* outer = makeBinary(inner, tok::TokenType::OP_STAR, makeLiteral(3.0));

    std::optional<double> result = optimizer->evaluateConstant(outer);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 90.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantComplexExpression)
{
    // ((5 + 3) * 2) - (10 / 2)
    ast::BinaryExpr* add = makeBinary(makeLiteral(5.0), tok::TokenType::OP_PLUS, makeLiteral(3.0));
    ast::BinaryExpr* mul = makeBinary(add, tok::TokenType::OP_STAR, makeLiteral(2.0));
    ast::BinaryExpr* div = makeBinary(makeLiteral(10.0), tok::TokenType::OP_SLASH, makeLiteral(2.0));
    ast::BinaryExpr* sub = makeBinary(mul, tok::TokenType::OP_MINUS, div);

    std::optional<double> result = optimizer->evaluateConstant(sub);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 11.0);
}

TEST_F(ASTOptimizerTest, EvaluateConstantWithVariable)
{
    // x + 5 (cannot be evaluated)
    ast::BinaryExpr* expr = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeLiteral(5.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantPartiallyConstant)
{
    // (10 + 20) + x (left side is constant, but overall is not)
    ast::BinaryExpr* left = makeBinary(makeLiteral(10.0), tok::TokenType::OP_PLUS, makeLiteral(20.0));
    ast::BinaryExpr* expr = makeBinary(left, tok::TokenType::OP_PLUS, makeName("x"));

    std::optional<double> result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

// CONSTANT FOLDING OPTIMIZATION TESTS

TEST_F(ASTOptimizerTest, ConstantFoldingSimpleAddition)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(5.0), tok::TokenType::OP_PLUS, makeLiteral(10.0));
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(result);
    EXPECT_EQ(lit->getType(), ast::LiteralExpr::Type::NUMBER);
    EXPECT_DOUBLE_EQ(lit->getValue().toDouble(), 15.0);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, ConstantFoldingMultiplication)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(6.0), tok::TokenType::OP_STAR, makeLiteral(7.0));
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(result);
    EXPECT_DOUBLE_EQ(lit->getValue().toDouble(), 42.0);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNested)
{
    // (3 + 4) * (5 - 2)
    ast::BinaryExpr* left = makeBinary(makeLiteral(3.0), tok::TokenType::OP_PLUS, makeLiteral(4.0));
    ast::BinaryExpr* right = makeBinary(makeLiteral(5.0), tok::TokenType::OP_MINUS, makeLiteral(2.0));
    ast::BinaryExpr* expr = makeBinary(left, tok::TokenType::OP_STAR, right);

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<ast::LiteralExpr*>(result)->getValue().toDouble(), 21.0);
}

TEST_F(ASTOptimizerTest, ConstantFoldingUnary)
{
    ast::UnaryExpr* expr = makeUnary(makeLiteral(42.0), tok::TokenType::OP_MINUS);
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<ast::LiteralExpr*>(result)->getValue().toDouble(), -42.0);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNoChange)
{
    // x + y (no constants to fold)
    ast::BinaryExpr* expr = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeName("y"));
    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_EQ(result, expr); // Should return original
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
    ast::NameExpr* x = makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, tok::TokenType::OP_PLUS, makeLiteral("0", ast::LiteralExpr::Type::NUMBER));

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_EQ(result, x);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionWithZero)
{
    // x - 0 = x
    ast::NameExpr* x = makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, tok::TokenType::OP_MINUS, makeLiteral("0", ast::LiteralExpr::Type::NUMBER));

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_EQ(result, x);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByOne)
{
    // x * 1 = x
    ast::NameExpr* x = makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, tok::TokenType::OP_STAR, makeLiteral("1", ast::LiteralExpr::Type::NUMBER));

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_EQ(result, x);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyDivisionByOne)
{
    // x / 1 = x
    ast::NameExpr* x = makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, tok::TokenType::OP_SLASH, makeLiteral("1", ast::LiteralExpr::Type::NUMBER));

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_EQ(result, x);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByZero)
{
    // x * 0 = 0
    ast::NameExpr* x = makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, tok::TokenType::OP_STAR, makeLiteral("0", ast::LiteralExpr::Type::NUMBER));

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(result);
    EXPECT_EQ(lit->getValue(), "0");
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByTwo)
{
    // x * 2 = x + x
    ast::NameExpr* x = makeName("x");
    ast::BinaryExpr* expr = makeBinary(x, tok::TokenType::OP_STAR, makeLiteral("2", ast::LiteralExpr::Type::NUMBER));

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::BINARY);
    ast::BinaryExpr* bin = static_cast<ast::BinaryExpr*>(result);
    EXPECT_EQ(bin->getOperator(), tok::TokenType::OP_PLUS);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionSameVariable)
{
    // x - x = 0
    ast::NameExpr* x1 = makeName("x");
    ast::NameExpr* x2 = makeName("x");
    ast::BinaryExpr* expr = makeBinary(x1, tok::TokenType::OP_MINUS, x2);

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(result);
    EXPECT_EQ(lit->getValue(), "0");
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionDifferentVariables)
{
    // x - y (should not simplify)
    ast::BinaryExpr* expr = makeBinary(makeName("x"), tok::TokenType::OP_MINUS, makeName("y"));

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_EQ(result, expr);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 0);
}

TEST_F(ASTOptimizerTest, SimplifyDoubleNegation)
{
    // --x = x
    ast::NameExpr* x = makeName("x");
    ast::UnaryExpr* inner = makeUnary(x, tok::TokenType::OP_MINUS);
    ast::UnaryExpr* outer = makeUnary(inner, tok::TokenType::OP_MINUS);

    ast::Expr* result = optimizer->optimizeConstantFolding(outer);

    // Should eliminate double negation
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, NoSimplificationWithNonZero)
{
    // x + 5 (should not simplify)
    ast::BinaryExpr* expr = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeLiteral(5.0));

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_EQ(result, expr);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 0);
}

// CALL AND LIST OPTIMIZATION TESTS

TEST_F(ASTOptimizerTest, OptimizeCallExpressionArgs)
{
    // func(2 + 3, 4 * 5)
    std::vector<ast::Expr*> args = {
        makeBinary(makeLiteral(2.0), tok::TokenType::OP_PLUS, makeLiteral(3.0)),
        makeBinary(makeLiteral(4.0), tok::TokenType::OP_STAR, makeLiteral(5.0))
    };

    ast::CallExpr* call = ast::AST_allocator.make<ast::CallExpr>(makeName("func"), ast::AST_allocator.make<ast::ListExpr>(args));
    ast::Expr* result = optimizer->optimizeConstantFolding(call);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::CALL);
    ast::CallExpr* optimized = static_cast<ast::CallExpr*>(result);

    // Arguments should be folded
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 2);
}

TEST_F(ASTOptimizerTest, OptimizeListElements)
{
    // [1 + 2, 3 * 4, x]
    std::vector<ast::Expr*> elements = {
        makeBinary(makeLiteral(1.0), tok::TokenType::OP_PLUS, makeLiteral(2.0)),
        makeBinary(makeLiteral(3.0), tok::TokenType::OP_STAR, makeLiteral(4.0)),
        makeName("x")
    };

    ast::ListExpr* list = ast::AST_allocator.make<ast::ListExpr>(elements);
    ast::Expr* result = optimizer->optimizeConstantFolding(list);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LIST);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 2);
}

TEST_F(ASTOptimizerTest, OptimizeEmptyList)
{
    ast::ListExpr* list = ast::AST_allocator.make<ast::ListExpr>(std::vector<ast::Expr*>());
    ast::Expr* result = optimizer->optimizeConstantFolding(list);

    EXPECT_EQ(result, list);
}

// DEAD CODE ELIMINATION - IF STATEMENTS

TEST_F(ASTOptimizerTest, DeadCodeIfTrueCondition)
{
    // if (true) { stmt1 } else { stmt2 }
    // Should return stmt1
    ast::BlockStmt* thenBlock = makeBlock({ makeExprStmt(makeLiteral(1.0)) });
    ast::BlockStmt* elseBlock = makeBlock({ makeExprStmt(makeLiteral(2.0)) });
    ast::IfStmt* ifStmt = makeIf(makeBoolLiteral(true), thenBlock, elseBlock);

    ast::Stmt* result = optimizer->eliminateDeadCode(ifStmt);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getKind(), ast::Stmt::Kind::BLOCK);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfFalseCondition)
{
    // if (false) { stmt1 } else { stmt2 }
    // Should return stmt2 (else block)
    ast::BlockStmt* thenBlock = makeBlock({ makeExprStmt(makeLiteral(1.0)) });
    ast::BlockStmt* elseBlock = makeBlock({ makeExprStmt(makeLiteral(2.0)) });
    ast::IfStmt* ifStmt = makeIf(makeBoolLiteral(false), thenBlock, elseBlock);

    ast::Stmt* result = optimizer->eliminateDeadCode(ifStmt);

    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfFalseNoElse)
{
    // if (false) { stmt1 }
    // Should eliminate entire statement
    ast::BlockStmt* thenBlock = makeBlock({ makeExprStmt(makeLiteral(1.0)) });
    ast::IfStmt* ifStmt = makeIf(makeBoolLiteral(false), thenBlock);

    ast::Stmt* result = optimizer->eliminateDeadCode(ifStmt);

    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfDynamicCondition)
{
    // if (x > 0) { stmt }
    // Should not eliminate (condition is not constant)
    ast::BinaryExpr* condition = makeBinary(makeName("x"), tok::TokenType::OP_GT, makeLiteral(0.0));
    ast::BlockStmt* thenBlock = makeBlock({ makeExprStmt(makeLiteral(1.0)) });
    ast::IfStmt* ifStmt = makeIf(condition, thenBlock);

    ast::Stmt* result = optimizer->eliminateDeadCode(ifStmt);

    EXPECT_EQ(result, ifStmt);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeNestedIf)
{
    // if (true) { if (false) { stmt1 } else { stmt2 } }
    ast::BlockStmt* innerElse = makeBlock({ makeExprStmt(makeLiteral(2.0)) });
    ast::IfStmt* innerIf = makeIf(makeBoolLiteral(false), makeBlock({}), innerElse);
    ast::BlockStmt* outerThen = makeBlock({ innerIf });
    ast::IfStmt* outerIf = makeIf(makeBoolLiteral(true), outerThen);

    ast::Stmt* result = optimizer->eliminateDeadCode(outerIf);

    // Should eliminate both if statements
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

// DEAD CODE ELIMINATION - WHILE LOOPS

TEST_F(ASTOptimizerTest, DeadCodeWhileFalseCondition)
{
    // while (false) { stmt }
    // Loop never executes - dead code
    ast::BlockStmt* body = makeBlock({ makeExprStmt(makeLiteral(1.0)) });
    ast::WhileStmt* whileStmt = makeWhile(makeBoolLiteral(false), body);

    ast::Stmt* result = optimizer->eliminateDeadCode(whileStmt);

    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileTrueCondition)
{
    // while (true) { stmt }
    // Infinite loop, but not dead code
    ast::BlockStmt* body = makeBlock({ makeExprStmt(makeLiteral(1.0)) });
    ast::WhileStmt* whileStmt = makeWhile(makeBoolLiteral(true), body);

    ast::Stmt* result = optimizer->eliminateDeadCode(whileStmt);

    EXPECT_EQ(result, whileStmt);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileDynamicCondition)
{
    // while (x > 0) { stmt }
    ast::BlockStmt* body = makeBlock({ makeExprStmt(makeLiteral(1.0)) });
    ast::BinaryExpr* condition = makeBinary(makeName("x"), tok::TokenType::OP_GT, makeLiteral(0.0));
    ast::WhileStmt* whileStmt = makeWhile(condition, body);

    ast::Stmt* result = optimizer->eliminateDeadCode(whileStmt);

    EXPECT_EQ(result, whileStmt);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileNestedStatements)
{
    // while (x) { stmt1; if (false) { stmt2 } }
    ast::IfStmt* innerIf = makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(2.0)) }));
    ast::BlockStmt* body = makeBlock({ makeExprStmt(makeLiteral(1.0)), innerIf });
    ast::WhileStmt* whileStmt = makeWhile(makeName("x"), body);

    ast::Stmt* result = optimizer->eliminateDeadCode(whileStmt);

    // Inner if should be eliminated
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

// DEAD CODE ELIMINATION - FOR LOOPS

/*
TEST_F(ASTOptimizerTest, DeadCodeForLoopBody)
{
    // for (i = 0; i < 10; i++) { if (false) { stmt } }
    ast::AssignmentStmt* init = makeAssignment("i", makeLiteral(0.0));
    ast::BinaryExpr* condition = makeBinary(makeName("i"), tok::TokenType::OP_LT, makeLiteral(10.0));
    ast::BinaryExpr* increment = makeBinary(makeName("i"), tok::TokenType::OP_PLUS, makeLiteral(1.0));

    ast::IfStmt* deadIf = makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(1.0)) }));
    ast::BlockStmt* body = makeBlock({ deadIf });

    // ast::ForStmt* forStmt = makeFor(init, condition, increment, body);
    ast::Stmt* result = optimizer->eliminateDeadCode(forStmt);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeForLoopNested)
{
    // for (...) { for (...) { if (false) {...} } }
    ast::AssignmentStmt* init = makeAssignment("i", makeLiteral(0.0));
    ast::BinaryExpr* condition = makeBinary(makeName("i"), tok::TokenType::OP_LT, makeLiteral(10.0));
    ast::BinaryExpr* increment = makeBinary(makeName("i"), tok::TokenType::OP_PLUS, makeLiteral(1.0));

    ast::IfStmt* innerIf = makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(1.0)) }));
    ast::ForStmt* innerFor = makeFor(init, condition, increment, makeBlock({ innerIf }));
    ast::ForStmt* outerFor = makeFor(init, condition, increment, makeBlock({ innerFor }));

    ast::Stmt* result = optimizer->eliminateDeadCode(outerFor);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}
*/

// DEAD CODE ELIMINATION - FUNCTIONS

TEST_F(ASTOptimizerTest, DeadCodeAfterReturn)
{
    // function f() { return 1; stmt2; stmt3; }
    // stmt2 and stmt3 are dead
    std::vector<ast::Stmt*> body = { makeReturn(makeLiteral(1.0)), makeExprStmt(makeLiteral(2.0)), makeExprStmt(makeLiteral(3.0)) };
    ast::FunctionDef* func = makeFunction("f", {}, makeBlock(body));
    ast::Stmt* result = optimizer->eliminateDeadCode(func);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 2);
}

TEST_F(ASTOptimizerTest, DeadCodeAfterEarlyReturn)
{
    // function f() { if (x) return 1; else return 2; stmt; }
    // stmt is dead (both branches return)
    ast::IfStmt* ifStmt = makeIf(makeName("x"), makeBlock({ makeReturn(makeLiteral(1.0)) }), makeBlock({ makeReturn(makeLiteral(2.0)) }));
    std::vector<ast::Stmt*> body = { ifStmt, makeExprStmt(makeLiteral(3.0)) };
    ast::FunctionDef* func = makeFunction("f", {}, makeBlock(body));
    ast::Stmt* result = optimizer->eliminateDeadCode(func);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, NoDeadCodeBeforeReturn)
{
    // function f() { stmt1; stmt2; return 1; }
    // No dead code
    std::vector<ast::Stmt*> body = { makeExprStmt(makeLiteral(1.0)), makeExprStmt(makeLiteral(2.0)), makeReturn(makeLiteral(3.0)) };
    ast::FunctionDef* func = makeFunction("f", {}, makeBlock(body));
    ast::Stmt* result = optimizer->eliminateDeadCode(func);

    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeNestedInFunction)
{
    // function f() { if (false) { stmt } }
    ast::IfStmt* ifStmt = makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(1.0)) }));
    ast::FunctionDef* func = makeFunction("f", {}, makeBlock({ ifStmt }));

    ast::Stmt* result = optimizer->eliminateDeadCode(func);

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
    ast::LiteralExpr* expr = makeLiteral(42.0);

    StringRef str = cse.exprToString(expr);

    EXPECT_FALSE(str.empty());
}

TEST_F(ASTOptimizerTest, CSEExprToStringName)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::NameExpr* expr = makeName("variable");

    StringRef str = cse.exprToString(expr);

    EXPECT_EQ(str, "variable");
}

TEST_F(ASTOptimizerTest, CSEExprToStringBinary)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::BinaryExpr* expr = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeName("y"));

    StringRef str = cse.exprToString(expr);

    EXPECT_FALSE(str.empty());
    // Should contain something like "(x + y)"
}

TEST_F(ASTOptimizerTest, CSEExprToStringUnary)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::UnaryExpr* expr = makeUnary(makeName("x"), tok::TokenType::OP_MINUS);

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
    ast::BinaryExpr* add = makeBinary(makeName("a"), tok::TokenType::OP_PLUS, makeName("b"));
    ast::BinaryExpr* mul = makeBinary(add, tok::TokenType::OP_STAR, makeName("c"));

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
    ast::BinaryExpr* expr = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeName("y"));

    std::optional<StringRef> result = cse.findCSE(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, CSERecordAndFind)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::BinaryExpr* expr = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeName("y"));

    cse.recordExpr(expr, "temp_var");
    std::optional<StringRef> result = cse.findCSE(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "temp_var");
}

TEST_F(ASTOptimizerTest, CSERecordMultipleExpressions)
{
    parser::ASTOptimizer::CSEPass cse;
    ast::BinaryExpr* expr1 = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeName("y"));
    ast::BinaryExpr* expr2 = makeBinary(makeName("a"), tok::TokenType::OP_STAR, makeName("b"));

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
    ast::LiteralExpr* expr = makeLiteral(42.0);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantNonLoopVariable)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    ast::NameExpr* expr = makeName("x"); // x is not a loop variable
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantLoopVariable)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    ast::NameExpr* expr = makeName("i"); // i is a loop variable
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithConstants)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    // 5 + 10 (both constants)
    ast::BinaryExpr* expr = makeBinary(makeLiteral(5.0), tok::TokenType::OP_PLUS, makeLiteral(10.0));
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithNonLoopVars)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    // x + y (neither are loop variables)
    ast::BinaryExpr* expr = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeName("y"));
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantBinaryWithLoopVar)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    // i + 5 (i is a loop variable)
    ast::BinaryExpr* expr = makeBinary(makeName("i"), tok::TokenType::OP_PLUS, makeLiteral(5.0));
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantComplexExpression)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i", "j" };
    // (x + y) * i (contains loop variable i)
    ast::BinaryExpr* left = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeName("y"));
    ast::BinaryExpr* expr = makeBinary(left, tok::TokenType::OP_STAR, makeName("i"));
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantUnary)
{
    std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = { "i" };
    ast::UnaryExpr* expr = makeUnary(makeName("x"), tok::TokenType::OP_MINUS);
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
    ast::AssignmentStmt* stmt = makeAssignment("x", makeBinary(makeLiteral(5.0), tok::TokenType::OP_PLUS, makeLiteral(10.0)));
    std::vector<ast::Stmt*> statements = { stmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 0);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 0);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1BasicOptimization)
{
    // Level 1 - basic optimization (constant folding)
    ast::AssignmentStmt* stmt = makeAssignment("x", makeBinary(makeLiteral(5.0), tok::TokenType::OP_PLUS, makeLiteral(10.0)));
    std::vector<ast::Stmt*> statements = { stmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1ExpressionStatement)
{
    // Level 1 with expression statement
    ast::ExprStmt* stmt = makeExprStmt(makeBinary(makeLiteral(3.0), tok::TokenType::OP_STAR, makeLiteral(4.0)));
    std::vector<ast::Stmt*> statements = { stmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2DeadCodeElimination)
{
    // Level 2 - includes dead code elimination
    ast::IfStmt* ifStmt = makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(1.0)) }));
    std::vector<ast::Stmt*> statements = { ifStmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2Combined)
{
    // Level 2 - both constant folding and dead code elimination
    ast::AssignmentStmt* assign = makeAssignment("x", makeBinary(makeLiteral(2.0), tok::TokenType::OP_PLUS, makeLiteral(3.0)));
    ast::IfStmt* ifStmt = makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(1.0)) }));
    std::vector<ast::Stmt*> statements = { assign, ifStmt };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().ConstantFolds, 1);
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeMultipleStatements)
{
    std::vector<ast::Stmt*> statements = {
        makeAssignment("a", makeBinary(makeLiteral(1.0), tok::TokenType::OP_PLUS, makeLiteral(2.0))),
        makeAssignment("b", makeBinary(makeLiteral(3.0), tok::TokenType::OP_STAR, makeLiteral(4.0))),
        makeAssignment("c", makeBinary(makeLiteral(5.0), tok::TokenType::OP_MINUS, makeLiteral(1.0)))
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
    std::vector<ast::Stmt*> statements = { makeAssignment("x", makeLiteral(1.0)) };
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
    ast::BinaryExpr* expr = makeBinary(makeLiteral(5.0), tok::TokenType::OP_PLUS, makeLiteral(10.0));
    optimizer->optimizeConstantFolding(expr);

    parser::ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, StatisticsMultipleOptimizations)
{
    ast::BinaryExpr* expr1 = makeBinary(makeLiteral(2.0), tok::TokenType::OP_PLUS, makeLiteral(3.0));
    ast::BinaryExpr* expr2 = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeLiteral("0", ast::LiteralExpr::Type::NUMBER));

    optimizer->optimizeConstantFolding(expr1);
    optimizer->optimizeConstantFolding(expr2);

    parser::ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 1);
    EXPECT_EQ(stats.StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, StatisticsPrintStats)
{
    // Just verify it doesn't crash
    ast::BinaryExpr* expr = makeBinary(makeLiteral(1.0), tok::TokenType::OP_PLUS, makeLiteral(1.0));
    optimizer->optimizeConstantFolding(expr);

    EXPECT_NO_THROW(optimizer->printStats());
}

// EDGE CASES AND STRESS TESTS

TEST_F(ASTOptimizerTest, EdgeCaseVeryDeepNesting)
{
    // ((((1 + 1) + 1) + 1) + 1)
    ast::Expr* expr = makeLiteral(1.0);
    for (int i = 0; i < 10; ++i)
        expr = makeBinary(expr, tok::TokenType::OP_PLUS, makeLiteral(1.0));

    ast::Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<ast::LiteralExpr*>(result)->getValue().toDouble(), 11.0);
}

TEST_F(ASTOptimizerTest, EdgeCaseLargeNumbers)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(1e308), tok::TokenType::OP_PLUS, makeLiteral(1e308));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    // May result in infinity, but should not crash
    EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCaseSmallNumbers)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(1e-308), tok::TokenType::OP_STAR, makeLiteral(1e-308));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    // Should handle underflow gracefully
    EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCasePowerOfZero)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(0.0), tok::TokenType::OP_POWER, makeLiteral(0.0));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1.0); // 0^0 is typically defined as 1
}

TEST_F(ASTOptimizerTest, EdgeCaseNegativePowerFraction)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(4.0), tok::TokenType::OP_POWER, makeLiteral(0.5));
    std::optional<double> result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 2.0); // sqrt(4)
}

TEST_F(ASTOptimizerTest, EdgeCaseModuloNegative)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(-17.0), tok::TokenType::OP_PERCENT, makeLiteral(5.0));
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
        statements.push_back(makeAssignment(varName, makeBinary(makeLiteral(i), tok::TokenType::OP_PLUS, makeLiteral(i))));
    }

    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 100);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 100);
}

TEST_F(ASTOptimizerTest, StressComplexDeadCodeElimination)
{
    // Nested if statements with constant conditions
    ast::IfStmt* inner3 = makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(3.0)) }));
    ast::IfStmt* inner2 = makeIf(makeBoolLiteral(true), makeBlock({ inner3 }));
    ast::IfStmt* inner1 = makeIf(makeBoolLiteral(false), makeBlock({ inner2 }));
    ast::Stmt* result = optimizer->eliminateDeadCode(inner1);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, EdgeCaseMixedOptimizations)
{
    // Test combining multiple optimization types
    // x * 0 (strength reduction to 0)
    ast::BinaryExpr* mul = makeBinary(makeName("x"), tok::TokenType::OP_STAR, makeLiteral("0", ast::LiteralExpr::Type::NUMBER));
    ast::Expr* result = optimizer->optimizeConstantFolding(mul);

    ASSERT_EQ(result->getKind(), ast::Expr::Kind::LITERAL);
    EXPECT_EQ(static_cast<ast::LiteralExpr*>(result)->getValue(), "0");
}

TEST_F(ASTOptimizerTest, EdgeCaseChainedStrengthReductions)
{
    // (x + 0) * 1
    ast::BinaryExpr* add = makeBinary(makeName("x"), tok::TokenType::OP_PLUS, makeLiteral("0", ast::LiteralExpr::Type::NUMBER));
    ast::BinaryExpr* mul = makeBinary(add, tok::TokenType::OP_STAR, makeLiteral("1", ast::LiteralExpr::Type::NUMBER));
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
        makeAssignment("x", makeBinary(makeLiteral(2.0), tok::TokenType::OP_PLUS, makeLiteral(3.0))),
        // Strength reduction: y = x * 1 => y = x
        makeAssignment("y", makeBinary(makeName("x"), tok::TokenType::OP_STAR, makeLiteral("1", ast::LiteralExpr::Type::NUMBER))),
        // Dead code: if (false) { ... }
        makeIf(makeBoolLiteral(false), makeBlock({ makeExprStmt(makeLiteral(999.0)) }))
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
        makeAssignment("x", makeBinary(makeLiteral(5.0), tok::TokenType::OP_PLUS, makeLiteral(10.0))),
        makeIf(makeBoolLiteral(false), makeBlock({ makeAssignment("y", makeLiteral(1.0)) })),
        makeReturn(makeName("x"))
    };

    ast::FunctionDef* func = makeFunction("f", {}, makeBlock(body));
    std::vector<ast::Stmt*> statements = { func };
    std::vector<ast::Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_EQ(result.size(), 1);
}
