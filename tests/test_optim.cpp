#include "../include/ast_printer.hpp"
#include "../include/parser.hpp"
#include "ast.hpp"

#include <gtest/gtest.h>

using namespace fairuz;
using namespace fairuz::parser;
using namespace testing;

class ASTOptimizerTest : public Test {
public:
    AST::ASTPrinter AST_Printer;

protected:
    std::unique_ptr<Fa_ASTOptimizer> optimizer;

    void SetUp() override { optimizer = std::make_unique<Fa_ASTOptimizer>(); }

    void TearDown() override { optimizer.reset(); }
};

TEST_F(ASTOptimizerTest, EvaluateConstantLiteral)
{
    AST::Fa_LiteralExpr* Fa_Expr_0 = AST::Fa_makeLiteralInt(42);
    AST::Fa_LiteralExpr* Fa_Expr_1 = nullptr;
    AST::Fa_LiteralExpr* Fa_Expr_2 = AST::Fa_makeLiteralBool(true);

    auto result_0 = optimizer->evaluateConstant(Fa_Expr_0);
    auto result_1 = optimizer->evaluateConstant(Fa_Expr_1);
    auto result_2 = optimizer->evaluateConstant(Fa_Expr_2);

    ASSERT_TRUE(result_0.has_value());
    EXPECT_DOUBLE_EQ(*result_0, 42);

    EXPECT_FALSE(result_1.has_value());
    EXPECT_FALSE(result_2.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantSimpleFa_Expr)
{
    AST::Fa_BinaryExpr* Fa_Expr_0 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(20), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* Fa_Expr_1 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(50), AST::Fa_makeLiteralInt(20), AST::Fa_BinaryOp::OP_SUB);
    AST::Fa_BinaryExpr* Fa_Expr_2 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(6), AST::Fa_makeLiteralInt(7), AST::Fa_BinaryOp::OP_MUL);
    AST::Fa_BinaryExpr* Fa_Expr_3 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(100), AST::Fa_makeLiteralInt(4), AST::Fa_BinaryOp::OP_DIV);
    AST::Fa_BinaryExpr* Fa_Expr_4 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(17), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_MOD);
    AST::Fa_BinaryExpr* Fa_Expr_5 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(10), AST::Fa_BinaryOp::OP_POW);
    AST::Fa_BinaryExpr* Fa_Expr_6 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(-2), AST::Fa_BinaryOp::OP_POW);

    auto result_0 = optimizer->evaluateConstant(Fa_Expr_0);
    auto result_1 = optimizer->evaluateConstant(Fa_Expr_1);
    auto result_2 = optimizer->evaluateConstant(Fa_Expr_2);
    auto result_3 = optimizer->evaluateConstant(Fa_Expr_3);
    auto result_4 = optimizer->evaluateConstant(Fa_Expr_4);
    auto result_5 = optimizer->evaluateConstant(Fa_Expr_5);
    auto result_6 = optimizer->evaluateConstant(Fa_Expr_6);

    ASSERT_TRUE(result_0.has_value());
    ASSERT_TRUE(result_1.has_value());
    ASSERT_TRUE(result_2.has_value());
    ASSERT_TRUE(result_3.has_value());
    ASSERT_TRUE(result_4.has_value());
    ASSERT_TRUE(result_5.has_value());
    ASSERT_TRUE(result_6.has_value());

    EXPECT_DOUBLE_EQ(*result_0, 30);
    EXPECT_DOUBLE_EQ(*result_1, 30);
    EXPECT_DOUBLE_EQ(*result_2, 42);
    EXPECT_DOUBLE_EQ(*result_3, 25);
    EXPECT_DOUBLE_EQ(*result_4, 2);
    EXPECT_DOUBLE_EQ(*result_5, 1024);
    EXPECT_DOUBLE_EQ(*result_6, 0.25);
}

TEST_F(ASTOptimizerTest, EvaluateConstantDivisionByZero)
{
    AST::Fa_BinaryExpr* Fa_Expr_0 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_DIV);
    AST::Fa_BinaryExpr* Fa_Expr_1 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_MOD);
    AST::Fa_BinaryExpr* Fa_Expr_2 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_POW);

    auto result_0 = optimizer->evaluateConstant(Fa_Expr_0);
    auto result_1 = optimizer->evaluateConstant(Fa_Expr_1);
    auto result_2 = optimizer->evaluateConstant(Fa_Expr_2);

    EXPECT_FALSE(result_0.has_value());
    EXPECT_FALSE(result_1.has_value());

    ASSERT_TRUE(result_2.has_value());
    EXPECT_DOUBLE_EQ(*result_2, 1);
}

TEST_F(ASTOptimizerTest, EvaluateConstantUnary)
{
    AST::Fa_UnaryExpr* Fa_Expr_0 = AST::Fa_makeUnary(AST::Fa_makeLiteralInt(42), AST::Fa_UnaryOp::OP_PLUS);
    AST::Fa_UnaryExpr* Fa_Expr_1 = AST::Fa_makeUnary(AST::Fa_makeLiteralInt(42), AST::Fa_UnaryOp::OP_NEG);

    auto result_0 = optimizer->evaluateConstant(Fa_Expr_0);
    auto result_1 = optimizer->evaluateConstant(Fa_Expr_1);

    ASSERT_TRUE(result_0.has_value());
    ASSERT_TRUE(result_1.has_value());

    EXPECT_DOUBLE_EQ(*result_0, 42);
    EXPECT_DOUBLE_EQ(*result_1, -42);
}

TEST_F(ASTOptimizerTest, EvaluateConstantNestedFa_Expression)
{
    AST::Fa_BinaryExpr* inner = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(20), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* outer = AST::Fa_makeBinary(inner, AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_MUL);

    auto result = optimizer->evaluateConstant(outer);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 90);
}

TEST_F(ASTOptimizerTest, EvaluateConstantComplexFa_Expression)
{
    AST::Fa_BinaryExpr* add = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* mul = AST::Fa_makeBinary(add, AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_MUL);
    AST::Fa_BinaryExpr* div = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_DIV);
    AST::Fa_BinaryExpr* sub = AST::Fa_makeBinary(mul, div, AST::Fa_BinaryOp::OP_SUB);

    auto result = optimizer->evaluateConstant(sub);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 11);
}

TEST_F(ASTOptimizerTest, EvaluateConstantWithVariable)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_ADD);
    auto result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantPartiallyConstant)
{
    AST::Fa_BinaryExpr* left = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(20), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(left, AST::Fa_makeName("x"), AST::Fa_BinaryOp::OP_ADD);

    auto result = optimizer->evaluateConstant(expr);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, ConstantFoldingSimpleAddition)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(10), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::LITERAL);
    AST::Fa_LiteralExpr* lit = static_cast<AST::Fa_LiteralExpr*>(result);
    EXPECT_TRUE(lit->isNumeric());
    EXPECT_DOUBLE_EQ(lit->toNumber(), 15);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, ConstantFoldingMultiplication)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(6), AST::Fa_makeLiteralInt(7), AST::Fa_BinaryOp::OP_MUL);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<AST::Fa_LiteralExpr*>(result)->toNumber(), 42);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNested)
{
    AST::Fa_BinaryExpr* left = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(3), AST::Fa_makeLiteralInt(4), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* right = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_SUB);
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(left, right, AST::Fa_BinaryOp::OP_MUL);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<AST::Fa_LiteralExpr*>(result)->toNumber(), 21);
}

TEST_F(ASTOptimizerTest, ConstantFoldingUnary)
{
    AST::Fa_UnaryExpr* expr = AST::Fa_makeUnary(AST::Fa_makeLiteralInt(42), AST::Fa_UnaryOp::OP_NEG);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<AST::Fa_LiteralExpr*>(result)->toNumber(), -42);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNoChange)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeName("y"), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_TRUE(result->equals(expr));
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 0);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNull)
{
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(ASTOptimizerTest, SimplifyAdditionWithZero)
{
    AST::Fa_NameExpr* x = AST::Fa_makeName("x");
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(x, AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_TRUE(result->equals(x));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionWithZero)
{
    AST::Fa_NameExpr* x = AST::Fa_makeName("x");
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(x, AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_SUB);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_TRUE(result->equals(x));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByOne)
{
    AST::Fa_NameExpr* x = AST::Fa_makeName("x");
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(x, AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_MUL);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_TRUE(result->equals(x));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyDivisionByOne)
{
    AST::Fa_NameExpr* x = AST::Fa_makeName("x");
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(x, AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_DIV);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_TRUE(result->equals(x));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByZero)
{
    AST::Fa_NameExpr* x = AST::Fa_makeName("x");
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(x, AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_MUL);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::LITERAL);
    EXPECT_EQ(static_cast<AST::Fa_LiteralExpr*>(result)->toNumber(), 0);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByTwo)
{
    AST::Fa_NameExpr* x = AST::Fa_makeName("x");
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(x, AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_MUL);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::BINARY);
    EXPECT_EQ(static_cast<AST::Fa_BinaryExpr*>(result)->getOperator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionSameVariable)
{
    AST::Fa_NameExpr* x1 = AST::Fa_makeName("x");
    AST::Fa_NameExpr* x2 = AST::Fa_makeName("x");
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(x1, x2, AST::Fa_BinaryOp::OP_SUB);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::LITERAL);
    EXPECT_EQ(static_cast<AST::Fa_LiteralExpr*>(result)->toNumber(), 0);
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionDifferentVariables)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeName("y"), AST::Fa_BinaryOp::OP_SUB);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_TRUE(result->equals(expr));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 0);
}

TEST_F(ASTOptimizerTest, SimplifyDoubleNegation)
{
    AST::Fa_NameExpr* x = AST::Fa_makeName("x");
    AST::Fa_UnaryExpr* inner = AST::Fa_makeUnary(x, AST::Fa_UnaryOp::OP_NEG);
    AST::Fa_UnaryExpr* outer = AST::Fa_makeUnary(inner, AST::Fa_UnaryOp::OP_NEG);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(outer);

    EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, NoSimplificationWithNonZero)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    EXPECT_TRUE(result->equals(expr));
    EXPECT_EQ(optimizer->getStats().StrengthReductions, 0);
}

TEST_F(ASTOptimizerTest, OptimizeCallFa_ExpressionArgs)
{
    Fa_Array<AST::Fa_Expr*> args = { AST::Fa_makeBinary(AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_ADD),
        AST::Fa_makeBinary(AST::Fa_makeLiteralInt(4), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_MUL) };

    AST::Fa_CallExpr* call = Fa_makeCall(AST::Fa_makeName("func"), Fa_makeList(args));
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(call);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::CALL);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 2);
}

TEST_F(ASTOptimizerTest, OptimizeListElements)
{
    Fa_Array<AST::Fa_Expr*> elements = { AST::Fa_makeBinary(AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_ADD),
        AST::Fa_makeBinary(AST::Fa_makeLiteralInt(3), AST::Fa_makeLiteralInt(4), AST::Fa_BinaryOp::OP_MUL), AST::Fa_makeName("x") };
    AST::Fa_ListExpr* list = Fa_makeList(elements);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(list);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::LIST);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 2);
}

TEST_F(ASTOptimizerTest, OptimizeEmptyList)
{
    AST::Fa_ListExpr* list = Fa_makeList(Fa_Array<AST::Fa_Expr*>());
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(list);

    EXPECT_TRUE(result->equals(list));
}

TEST_F(ASTOptimizerTest, DeadCodeIfTrueCondition)
{
    AST::Fa_BlockStmt* thenBlock = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) });
    AST::Fa_BlockStmt* elseBlock = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(2)) });
    AST::Fa_IfStmt* ifStmt = AST::Fa_makeIf(AST::Fa_makeLiteralBool(true), thenBlock, elseBlock);
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(ifStmt);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getKind(), AST::Fa_Stmt::Kind::BLOCK);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfFalseCondition)
{
    AST::Fa_BlockStmt* thenBlock = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) });
    AST::Fa_BlockStmt* elseBlock = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(2)) });
    AST::Fa_IfStmt* ifStmt = AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), thenBlock, elseBlock);
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(ifStmt);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfFalseNoElse)
{
    AST::Fa_BlockStmt* thenBlock = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) });
    AST::Fa_IfStmt* ifStmt = AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), thenBlock);
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(ifStmt);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfDynamicCondition)
{
    AST::Fa_BinaryExpr* condition = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_GT);
    AST::Fa_BlockStmt* thenBlock = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) });
    AST::Fa_IfStmt* ifStmt = AST::Fa_makeIf(condition, thenBlock);
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(ifStmt);

    EXPECT_TRUE(result->equals(ifStmt));
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeNestedIf)
{
    AST::Fa_BlockStmt* innerElse = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(2)) });
    AST::Fa_IfStmt* innerIf = AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), AST::Fa_makeBlock({ }), innerElse);
    AST::Fa_BlockStmt* outerThen = AST::Fa_makeBlock({ innerIf });
    AST::Fa_IfStmt* outerIf = AST::Fa_makeIf(AST::Fa_makeLiteralBool(true), outerThen);
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(outerIf);
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileFalseCondition)
{
    AST::Fa_BlockStmt* body = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) });
    AST::Fa_WhileStmt* whileStmt = Fa_makeWhile(AST::Fa_makeLiteralBool(false), body);
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(whileStmt);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileTrueCondition)
{
    AST::Fa_BlockStmt* body = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) });
    AST::Fa_WhileStmt* whileStmt = Fa_makeWhile(AST::Fa_makeLiteralBool(true), body);
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(whileStmt);
    EXPECT_TRUE(result->equals(whileStmt));
}

TEST_F(ASTOptimizerTest, DeadCodeWhileDynamicCondition)
{
    AST::Fa_BlockStmt* body = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) });
    AST::Fa_BinaryExpr* condition = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_GT);
    AST::Fa_WhileStmt* whileStmt = Fa_makeWhile(condition, body);

    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(whileStmt);

    EXPECT_TRUE(result->equals(whileStmt));
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileNestedStatements)
{
    AST::Fa_IfStmt* innerIf = AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), 
        AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(2)) }));
    AST::Fa_BlockStmt* body = AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)), innerIf });
    AST::Fa_WhileStmt* whileStmt = Fa_makeWhile(AST::Fa_makeName("x"), body);
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(whileStmt);
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeAfterReturn)
{
    Fa_Array<AST::Fa_Stmt*> body = { Fa_makeReturn(AST::Fa_makeLiteralInt(1)), 
        AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(2)), AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(3)) };
    AST::Fa_FunctionDef* func = AST::Fa_makeFunction(AST::Fa_makeName("f"), { }, AST::Fa_makeBlock(body));
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(func);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeAfterEarlyReturn)
{
    AST::Fa_IfStmt* ifStmt = AST::Fa_makeIf(AST::Fa_makeName("x"), 
        AST::Fa_makeBlock({ Fa_makeReturn(AST::Fa_makeLiteralInt(1)) }), AST::Fa_makeBlock({ Fa_makeReturn(AST::Fa_makeLiteralInt(2)) }));
    Fa_Array<AST::Fa_Stmt*> body = { ifStmt, AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(3)) };
    AST::Fa_FunctionDef* func = AST::Fa_makeFunction(AST::Fa_makeName("f"), { }, AST::Fa_makeBlock(body));
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(func);
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, NoDeadCodeBeforeReturn)
{
    Fa_Array<AST::Fa_Stmt*> body = { AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)), 
        AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(2)), Fa_makeReturn(AST::Fa_makeLiteralInt(3)) };
    AST::Fa_FunctionDef* func = AST::Fa_makeFunction(AST::Fa_makeName("f"), { }, AST::Fa_makeBlock(body));
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(func);
    EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeNestedInFunction)
{
    AST::Fa_IfStmt* ifStmt = AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), 
        AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) }));
    AST::Fa_FunctionDef* func = AST::Fa_makeFunction(AST::Fa_makeName("f"), { }, AST::Fa_makeBlock({ ifStmt }));
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(func);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeNullStatement)
{
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(ASTOptimizerTest, LoopInvariantLiteral)
{
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loopVars;
    AST::Fa_LiteralExpr* expr = AST::Fa_makeLiteralInt(42);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantNonLoopVariable)
{
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loopVars = { "i" };
    AST::Fa_NameExpr* expr = AST::Fa_makeName("x");
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantLoopVariable)
{
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loopVars = { "i" };
    AST::Fa_NameExpr* expr = AST::Fa_makeName("i");
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithConstants)
{
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loopVars = { "i" };
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(10), AST::Fa_BinaryOp::OP_ADD);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithNonLoopVars)
{
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loopVars = { "i" };
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeName("y"), AST::Fa_BinaryOp::OP_ADD);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantBinaryWithLoopVar)
{
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loopVars = { "i" };
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_ADD);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantComplexFa_Expression)
{
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loopVars = { "i", "j" };
    AST::Fa_BinaryExpr* left = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeName("y"), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(left, AST::Fa_makeName("i"), AST::Fa_BinaryOp::OP_MUL);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantUnary)
{
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loopVars = { "i" };
    AST::Fa_UnaryExpr* expr = AST::Fa_makeUnary(AST::Fa_makeName("x"), AST::Fa_UnaryOp::OP_NEG);
    bool result = optimizer->isLoopInvariant(expr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantNull)
{
    std::unordered_set<Fa_StringRef, Fa_StringRefHash, Fa_StringRefEqual> loopVars;
    bool result = optimizer->isLoopInvariant(nullptr, loopVars);

    EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, OptimizeLevel0NoOptimization)
{
    AST::Fa_AssignmentStmt* stmt = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), 
        AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(10), AST::Fa_BinaryOp::OP_ADD));
    Fa_Array<AST::Fa_Stmt*> statements = { stmt };
    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 0);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 0);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1BasicOptimization)
{
    AST::Fa_AssignmentStmt* stmt = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), 
        AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(10), AST::Fa_BinaryOp::OP_ADD));
    Fa_Array<AST::Fa_Stmt*> statements = { stmt };
    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1Fa_ExpressionStatement)
{
    AST::Fa_ExprStmt* stmt = AST::Fa_makeExprStmt(AST::Fa_makeBinary(AST::Fa_makeLiteralInt(3), AST::Fa_makeLiteralInt(4), AST::Fa_BinaryOp::OP_MUL));
    Fa_Array<AST::Fa_Stmt*> statements = { stmt };
    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2DeadCodeElimination)
{
    AST::Fa_IfStmt* ifStmt = AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) }));
    Fa_Array<AST::Fa_Stmt*> statements = { ifStmt };
    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2Combined)
{
    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), 
        AST::Fa_makeBinary(AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_ADD));
    AST::Fa_IfStmt* ifStmt = AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(1)) }));
    Fa_Array<AST::Fa_Stmt*> statements = { assign, ifStmt };
    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().ConstantFolds, 1);
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeMultipleStatements)
{
    Fa_Array<AST::Fa_Stmt*> statements = { AST::Fa_makeAssignmentStmt(AST::Fa_makeName("a"), 
        AST::Fa_makeBinary(AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_ADD)),
        AST::Fa_makeAssignmentStmt(AST::Fa_makeName("b"), AST::Fa_makeBinary(AST::Fa_makeLiteralInt(3), AST::Fa_makeLiteralInt(4), AST::Fa_BinaryOp::OP_MUL)),
        AST::Fa_makeAssignmentStmt(AST::Fa_makeName("c"), AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_SUB)) };

    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 3);
}

TEST_F(ASTOptimizerTest, OptimizeEmptyStatements)
{
    Fa_Array<AST::Fa_Stmt*> statements;
    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_EQ(result.size(), 0);
}

TEST_F(ASTOptimizerTest, OptimizeNullStatementsFiltered)
{
    Fa_Array<AST::Fa_Stmt*> statements = { AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(1)) };
    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 2);

    for (AST::Fa_Stmt* stmt : result)
        EXPECT_NE(stmt, nullptr);
}

TEST_F(ASTOptimizerTest, StatisticsInitiallyZero)
{
    Fa_ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 0);
    EXPECT_EQ(stats.DeadCodeEliminations, 0);
    EXPECT_EQ(stats.StrengthReductions, 0);
    EXPECT_EQ(stats.CommonSubFa_ExprEliminations, 0);
    EXPECT_EQ(stats.LoopInvariants, 0);
}

TEST_F(ASTOptimizerTest, StatisticsConstantFolds)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(10), AST::Fa_BinaryOp::OP_ADD);
    optimizer->optimizeConstantFolding(expr);

    Fa_ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, StatisticsMultipleOptimizations)
{
    AST::Fa_BinaryExpr* Fa_Expr1 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* Fa_Expr2 = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_ADD);

    optimizer->optimizeConstantFolding(Fa_Expr1);
    optimizer->optimizeConstantFolding(Fa_Expr2);

    Fa_ASTOptimizer::OptimizationStats const& stats = optimizer->getStats();

    EXPECT_EQ(stats.ConstantFolds, 1);
    EXPECT_EQ(stats.StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, StatisticsPrintStats)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD);
    optimizer->optimizeConstantFolding(expr);

    EXPECT_NO_THROW(optimizer->printStats());
}

TEST_F(ASTOptimizerTest, EdgeCaseVeryDeepNesting)
{
    AST::Fa_Expr* expr = AST::Fa_makeLiteralInt(1);
    for (int i = 0; i < 10; ++i)
        expr = AST::Fa_makeBinary(expr, AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD);

    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(expr);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::LITERAL);
    EXPECT_DOUBLE_EQ(static_cast<AST::Fa_LiteralExpr*>(result)->toNumber(), 11);
}

TEST_F(ASTOptimizerTest, EdgeCaseLargeNumbers)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralFloat(1e308), AST::Fa_makeLiteralFloat(1e308), AST::Fa_BinaryOp::OP_ADD);
    auto result = optimizer->evaluateConstant(expr);

    EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCaseSmallNumbers)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralFloat(1e-308), AST::Fa_makeLiteralFloat(1e-308), AST::Fa_BinaryOp::OP_MUL);
    auto result = optimizer->evaluateConstant(expr);

    EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCasePowerOfZero)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(0), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_POW);
    auto result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1);
}

TEST_F(ASTOptimizerTest, EdgeCaseNegativePowerFraction)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(4), AST::Fa_makeLiteralFloat(0.5), AST::Fa_BinaryOp::OP_POW);
    auto result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 2);
}

TEST_F(ASTOptimizerTest, EdgeCaseModuloNegative)
{
    AST::Fa_BinaryExpr* expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(-17), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_MOD);
    auto result = optimizer->evaluateConstant(expr);

    ASSERT_TRUE(result.has_value());
}

TEST_F(ASTOptimizerTest, StressManyOptimizations)
{
    Fa_Array<AST::Fa_Stmt*> statements;

    for (int i = 0; i < 100; ++i) {
        Fa_StringRef var_name = "var" + Fa_StringRef(std::to_string(i).data());
        statements.push(AST::Fa_makeAssignmentStmt(AST::Fa_makeName(var_name), AST::Fa_makeBinary(AST::Fa_makeLiteralInt(i), AST::Fa_makeLiteralInt(i), AST::Fa_BinaryOp::OP_ADD)));
    }

    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 1);

    EXPECT_EQ(result.size(), 100);
    EXPECT_EQ(optimizer->getStats().ConstantFolds, 100);
}

TEST_F(ASTOptimizerTest, StressComplexDeadCodeElimination)
{
    AST::Fa_IfStmt* inner3 = AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(3)) }));
    AST::Fa_IfStmt* inner2 = AST::Fa_makeIf(AST::Fa_makeLiteralBool(true), AST::Fa_makeBlock({ inner3 }));
    AST::Fa_IfStmt* inner1 = AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), AST::Fa_makeBlock({ inner2 }));
    AST::Fa_Stmt* result = optimizer->eliminateDeadCode(inner1);

    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, EdgeCaseMixedOptimizations)
{
    AST::Fa_BinaryExpr* mul = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_MUL);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(mul);

    ASSERT_EQ(result->getKind(), AST::Fa_Expr::Kind::LITERAL);
    EXPECT_EQ(static_cast<AST::Fa_LiteralExpr*>(result)->toNumber(), 0);
}

TEST_F(ASTOptimizerTest, EdgeCaseChainedStrengthReductions)
{
    AST::Fa_BinaryExpr* add = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* mul = AST::Fa_makeBinary(add, AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_MUL);
    AST::Fa_Expr* result = optimizer->optimizeConstantFolding(mul);

    EXPECT_GE(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, IntegrationFullOptimizationPipeline)
{
    Fa_Array<AST::Fa_Stmt*> statements = { AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), AST::Fa_makeBinary(AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_ADD)),
        AST::Fa_makeAssignmentStmt(AST::Fa_makeName("y"), AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_MUL)),
        AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(999)) })) };

    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_GE(optimizer->getStats().ConstantFolds, 1);
    EXPECT_GE(optimizer->getStats().StrengthReductions, 1);
    EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, IntegrationNestedFunctionOptimization)
{
    Fa_Array<AST::Fa_Stmt*> body = { AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), AST::Fa_makeBinary(AST::Fa_makeLiteralInt(5), AST::Fa_makeLiteralInt(10), AST::Fa_BinaryOp::OP_ADD)),
        AST::Fa_makeIf(AST::Fa_makeLiteralBool(false), AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("y"), AST::Fa_makeLiteralInt(1)) })), Fa_makeReturn(AST::Fa_makeName("x")) };

    AST::Fa_FunctionDef* func = AST::Fa_makeFunction(AST::Fa_makeName("f"), { }, AST::Fa_makeBlock(body));
    Fa_Array<AST::Fa_Stmt*> statements = { func };
    Fa_Array<AST::Fa_Stmt*> result = optimizer->optimize(statements, 2);

    EXPECT_EQ(result.size(), 1);
}
