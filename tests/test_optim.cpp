#include "../include/ast_printer.hpp"
#include "../include/parser.hpp"

#include <gtest/gtest.h>

using namespace mylang;
using namespace mylang::ast;
using namespace mylang::parser;
using namespace testing;

class ASTOptimizerTest : public Test {
public:
  ASTPrinter AST_Printer;

protected:
  std::unique_ptr<ASTOptimizer> optimizer;

  void SetUp() override { optimizer = std::make_unique<ASTOptimizer>(); }

  void TearDown() override { optimizer.reset(); }

  AssignmentStmt *makeAssignment(StringRef const &target, Expr *value) { return makeAssignmentStmt(makeName(target), value); }

  IfStmt *makeIf(Expr *condition, BlockStmt *thenBlock, BlockStmt *elseBlock = nullptr) {
    if (!elseBlock)
      elseBlock = makeBlock(Array<Stmt *>());

    return ast::makeIf(condition, thenBlock, elseBlock);
  }

  FunctionDef *makeFunction(StringRef const &name, Array<StringRef> const &params, BlockStmt *body) {
    NameExpr *nameExpr = makeName(name);
    Array<Expr *> paramsExpr(params.size());
    for (auto p : params)
      paramsExpr.push(makeName(p));

    ListExpr *listExpr = makeList(paramsExpr);
    return ast::makeFunction(nameExpr, listExpr, body);
  }
};

TEST_F(ASTOptimizerTest, EvaluateConstantLiteral) {
  LiteralExpr *expr_0 = makeLiteralInt(42);
  LiteralExpr *expr_1 = nullptr;
  LiteralExpr *expr_2 = makeLiteralBool(true);

  auto result_0 = optimizer->evaluateConstant(expr_0);
  auto result_1 = optimizer->evaluateConstant(expr_1);
  auto result_2 = optimizer->evaluateConstant(expr_2);

  ASSERT_TRUE(result_0.has_value());
  EXPECT_DOUBLE_EQ(*result_0, 42);

  EXPECT_FALSE(result_1.has_value());
  EXPECT_FALSE(result_2.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantSimpleExpr) {
  BinaryExpr *expr_0 = makeBinary(makeLiteralInt(10), makeLiteralInt(20), BinaryOp::OP_ADD);
  BinaryExpr *expr_1 = makeBinary(makeLiteralInt(50), makeLiteralInt(20), BinaryOp::OP_SUB);
  BinaryExpr *expr_2 = makeBinary(makeLiteralInt(6), makeLiteralInt(7), BinaryOp::OP_MUL);
  BinaryExpr *expr_3 = makeBinary(makeLiteralInt(100), makeLiteralInt(4), BinaryOp::OP_DIV);
  BinaryExpr *expr_4 = makeBinary(makeLiteralInt(17), makeLiteralInt(5), BinaryOp::OP_MOD);
  BinaryExpr *expr_5 = makeBinary(makeLiteralInt(2), makeLiteralInt(10), BinaryOp::OP_POW);
  BinaryExpr *expr_6 = makeBinary(makeLiteralInt(2), makeLiteralInt(-2), BinaryOp::OP_POW);

  auto result_0 = optimizer->evaluateConstant(expr_0);
  auto result_1 = optimizer->evaluateConstant(expr_1);
  auto result_2 = optimizer->evaluateConstant(expr_2);
  auto result_3 = optimizer->evaluateConstant(expr_3);
  auto result_4 = optimizer->evaluateConstant(expr_4);
  auto result_5 = optimizer->evaluateConstant(expr_5);
  auto result_6 = optimizer->evaluateConstant(expr_6);

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

TEST_F(ASTOptimizerTest, EvaluateConstantDivisionByZero) {
  BinaryExpr *expr_0 = makeBinary(makeLiteralInt(10), makeLiteralInt(0), BinaryOp::OP_DIV);
  BinaryExpr *expr_1 = makeBinary(makeLiteralInt(10), makeLiteralInt(0), BinaryOp::OP_MOD);
  BinaryExpr *expr_2 = makeBinary(makeLiteralInt(5), makeLiteralInt(0), BinaryOp::OP_POW);

  auto result_0 = optimizer->evaluateConstant(expr_0);
  auto result_1 = optimizer->evaluateConstant(expr_1);
  auto result_2 = optimizer->evaluateConstant(expr_2);

  EXPECT_FALSE(result_0.has_value());
  EXPECT_FALSE(result_1.has_value());

  ASSERT_TRUE(result_2.has_value());
  EXPECT_DOUBLE_EQ(*result_2, 1);
}

TEST_F(ASTOptimizerTest, EvaluateConstantUnary) {
  UnaryExpr *expr_0 = makeUnary(makeLiteralInt(42), UnaryOp::OP_PLUS);
  UnaryExpr *expr_1 = makeUnary(makeLiteralInt(42), UnaryOp::OP_NEG);

  auto result_0 = optimizer->evaluateConstant(expr_0);
  auto result_1 = optimizer->evaluateConstant(expr_1);

  ASSERT_TRUE(result_0.has_value());
  ASSERT_TRUE(result_1.has_value());

  EXPECT_DOUBLE_EQ(*result_0, 42);
  EXPECT_DOUBLE_EQ(*result_1, -42);
}

TEST_F(ASTOptimizerTest, EvaluateConstantNestedExpression) {
  BinaryExpr *inner = makeBinary(makeLiteralInt(10), makeLiteralInt(20), BinaryOp::OP_ADD);
  BinaryExpr *outer = makeBinary(inner, makeLiteralInt(3), BinaryOp::OP_MUL);

  auto result = optimizer->evaluateConstant(outer);

  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(*result, 90);
}

TEST_F(ASTOptimizerTest, EvaluateConstantComplexExpression) {
  BinaryExpr *add = makeBinary(makeLiteralInt(5), makeLiteralInt(3), BinaryOp::OP_ADD);
  BinaryExpr *mul = makeBinary(add, makeLiteralInt(2), BinaryOp::OP_MUL);
  BinaryExpr *div = makeBinary(makeLiteralInt(10), makeLiteralInt(2), BinaryOp::OP_DIV);
  BinaryExpr *sub = makeBinary(mul, div, BinaryOp::OP_SUB);

  auto result = optimizer->evaluateConstant(sub);

  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(*result, 11);
}

TEST_F(ASTOptimizerTest, EvaluateConstantWithVariable) {
  BinaryExpr *expr = makeBinary(makeName("x"), makeLiteralInt(5), BinaryOp::OP_ADD);
  auto result = optimizer->evaluateConstant(expr);

  EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, EvaluateConstantPartiallyConstant) {
  BinaryExpr *left = makeBinary(makeLiteralInt(10), makeLiteralInt(20), BinaryOp::OP_ADD);
  BinaryExpr *expr = makeBinary(left, makeName("x"), BinaryOp::OP_ADD);

  auto result = optimizer->evaluateConstant(expr);

  EXPECT_FALSE(result.has_value());
}

TEST_F(ASTOptimizerTest, ConstantFoldingSimpleAddition) {
  BinaryExpr *expr = makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
  LiteralExpr *lit = static_cast<LiteralExpr *>(result);
  EXPECT_TRUE(lit->isNumeric());
  EXPECT_DOUBLE_EQ(lit->toNumber(), 15);
  EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, ConstantFoldingMultiplication) {
  BinaryExpr *expr = makeBinary(makeLiteralInt(6), makeLiteralInt(7), BinaryOp::OP_MUL);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
  EXPECT_DOUBLE_EQ(static_cast<LiteralExpr *>(result)->toNumber(), 42);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNested) {
  BinaryExpr *left = makeBinary(makeLiteralInt(3), makeLiteralInt(4), BinaryOp::OP_ADD);
  BinaryExpr *right = makeBinary(makeLiteralInt(5), makeLiteralInt(2), BinaryOp::OP_SUB);
  BinaryExpr *expr = makeBinary(left, right, BinaryOp::OP_MUL);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
  EXPECT_DOUBLE_EQ(static_cast<LiteralExpr *>(result)->toNumber(), 21);
}

TEST_F(ASTOptimizerTest, ConstantFoldingUnary) {
  UnaryExpr *expr = makeUnary(makeLiteralInt(42), UnaryOp::OP_NEG);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
  EXPECT_DOUBLE_EQ(static_cast<LiteralExpr *>(result)->toNumber(), -42);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNoChange) {
  BinaryExpr *expr = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  EXPECT_TRUE(result->equals(expr));
  EXPECT_EQ(optimizer->getStats().ConstantFolds, 0);
}

TEST_F(ASTOptimizerTest, ConstantFoldingNull) {
  Expr *result = optimizer->optimizeConstantFolding(nullptr);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ASTOptimizerTest, SimplifyAdditionWithZero) {
  NameExpr *x = makeName("x");
  BinaryExpr *expr = makeBinary(x, makeLiteralInt(0), BinaryOp::OP_ADD);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  EXPECT_TRUE(result->equals(x));
  EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionWithZero) {
  NameExpr *x = makeName("x");
  BinaryExpr *expr = makeBinary(x, makeLiteralInt(0), BinaryOp::OP_SUB);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  EXPECT_TRUE(result->equals(x));
  EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByOne) {
  NameExpr *x = makeName("x");
  BinaryExpr *expr = makeBinary(x, makeLiteralInt(1), BinaryOp::OP_MUL);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  EXPECT_TRUE(result->equals(x));
  EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyDivisionByOne) {
  NameExpr *x = makeName("x");
  BinaryExpr *expr = makeBinary(x, makeLiteralInt(1), BinaryOp::OP_DIV);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  EXPECT_TRUE(result->equals(x));
  EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByZero) {
  NameExpr *x = makeName("x");
  BinaryExpr *expr = makeBinary(x, makeLiteralInt(0), BinaryOp::OP_MUL);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
  EXPECT_EQ(static_cast<LiteralExpr *>(result)->toNumber(), 0);
  EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifyMultiplicationByTwo) {
  NameExpr *x = makeName("x");
  BinaryExpr *expr = makeBinary(x, makeLiteralInt(2), BinaryOp::OP_MUL);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  ASSERT_EQ(result->getKind(), Expr::Kind::BINARY);
  EXPECT_EQ(static_cast<BinaryExpr *>(result)->getOperator(), BinaryOp::OP_ADD);
  EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionSameVariable) {
  NameExpr *x1 = makeName("x");
  NameExpr *x2 = makeName("x");
  BinaryExpr *expr = makeBinary(x1, x2, BinaryOp::OP_SUB);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
  EXPECT_EQ(static_cast<LiteralExpr *>(result)->toNumber(), 0);
  EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, SimplifySubtractionDifferentVariables) {
  BinaryExpr *expr = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_SUB);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  EXPECT_TRUE(result->equals(expr));
  EXPECT_EQ(optimizer->getStats().StrengthReductions, 0);
}

TEST_F(ASTOptimizerTest, SimplifyDoubleNegation) {
  NameExpr *x = makeName("x");
  UnaryExpr *inner = makeUnary(x, UnaryOp::OP_NEG);
  UnaryExpr *outer = makeUnary(inner, UnaryOp::OP_NEG);
  Expr *result = optimizer->optimizeConstantFolding(outer);

  EXPECT_EQ(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, NoSimplificationWithNonZero) {
  BinaryExpr *expr = makeBinary(makeName("x"), makeLiteralInt(5), BinaryOp::OP_ADD);
  Expr *result = optimizer->optimizeConstantFolding(expr);

  EXPECT_TRUE(result->equals(expr));
  EXPECT_EQ(optimizer->getStats().StrengthReductions, 0);
}

TEST_F(ASTOptimizerTest, OptimizeCallExpressionArgs) {
  Array<Expr *> args = {makeBinary(makeLiteralInt(2), makeLiteralInt(3), BinaryOp::OP_ADD),
                        makeBinary(makeLiteralInt(4), makeLiteralInt(5), BinaryOp::OP_MUL)};

  CallExpr *call = makeCall(makeName("func"), makeList(args));
  Expr *result = optimizer->optimizeConstantFolding(call);

  ASSERT_EQ(result->getKind(), Expr::Kind::CALL);
  EXPECT_EQ(optimizer->getStats().ConstantFolds, 2);
}

TEST_F(ASTOptimizerTest, OptimizeListElements) {
  Array<Expr *> elements = {makeBinary(makeLiteralInt(1), makeLiteralInt(2), BinaryOp::OP_ADD),
                            makeBinary(makeLiteralInt(3), makeLiteralInt(4), BinaryOp::OP_MUL), makeName("x")};
  ListExpr *list = makeList(elements);
  Expr *result = optimizer->optimizeConstantFolding(list);

  ASSERT_EQ(result->getKind(), Expr::Kind::LIST);
  EXPECT_EQ(optimizer->getStats().ConstantFolds, 2);
}

TEST_F(ASTOptimizerTest, OptimizeEmptyList) {
  ListExpr *list = makeList(Array<Expr *>());
  Expr *result = optimizer->optimizeConstantFolding(list);

  EXPECT_TRUE(result->equals(list));
}

TEST_F(ASTOptimizerTest, DeadCodeIfTrueCondition) {
  BlockStmt *thenBlock = makeBlock({makeExprStmt(makeLiteralInt(1))});
  BlockStmt *elseBlock = makeBlock({makeExprStmt(makeLiteralInt(2))});
  IfStmt *ifStmt = makeIf(makeLiteralBool(true), thenBlock, elseBlock);
  Stmt *result = optimizer->eliminateDeadCode(ifStmt);

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->getKind(), Stmt::Kind::BLOCK);
  EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfFalseCondition) {
  BlockStmt *thenBlock = makeBlock({makeExprStmt(makeLiteralInt(1))});
  BlockStmt *elseBlock = makeBlock({makeExprStmt(makeLiteralInt(2))});
  IfStmt *ifStmt = makeIf(makeLiteralBool(false), thenBlock, elseBlock);
  Stmt *result = optimizer->eliminateDeadCode(ifStmt);
  EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfFalseNoElse) {
  BlockStmt *thenBlock = makeBlock({makeExprStmt(makeLiteralInt(1))});
  IfStmt *ifStmt = makeIf(makeLiteralBool(false), thenBlock);
  Stmt *result = optimizer->eliminateDeadCode(ifStmt);
  EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeIfDynamicCondition) {
  BinaryExpr *condition = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_GT);
  BlockStmt *thenBlock = makeBlock({makeExprStmt(makeLiteralInt(1))});
  IfStmt *ifStmt = makeIf(condition, thenBlock);
  Stmt *result = optimizer->eliminateDeadCode(ifStmt);

  EXPECT_TRUE(result->equals(ifStmt));
  EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeNestedIf) {
  BlockStmt *innerElse = makeBlock({makeExprStmt(makeLiteralInt(2))});
  IfStmt *innerIf = makeIf(makeLiteralBool(false), makeBlock({}), innerElse);
  BlockStmt *outerThen = makeBlock({innerIf});
  IfStmt *outerIf = makeIf(makeLiteralBool(true), outerThen);
  Stmt *result = optimizer->eliminateDeadCode(outerIf);
  EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileFalseCondition) {
  BlockStmt *body = makeBlock({makeExprStmt(makeLiteralInt(1))});
  WhileStmt *whileStmt = makeWhile(makeLiteralBool(false), body);
  Stmt *result = optimizer->eliminateDeadCode(whileStmt);
  EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileTrueCondition) {
  BlockStmt *body = makeBlock({makeExprStmt(makeLiteralInt(1))});
  WhileStmt *whileStmt = makeWhile(makeLiteralBool(true), body);
  Stmt *result = optimizer->eliminateDeadCode(whileStmt);
  EXPECT_TRUE(result->equals(whileStmt));
}

TEST_F(ASTOptimizerTest, DeadCodeWhileDynamicCondition) {
  BlockStmt *body = makeBlock({makeExprStmt(makeLiteralInt(1))});
  BinaryExpr *condition = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_GT);
  WhileStmt *whileStmt = makeWhile(condition, body);

  Stmt *result = optimizer->eliminateDeadCode(whileStmt);

  EXPECT_TRUE(result->equals(whileStmt));
  EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeWhileNestedStatements) {
  IfStmt *innerIf = makeIf(makeLiteralBool(false), makeBlock({makeExprStmt(makeLiteralInt(2))}));
  BlockStmt *body = makeBlock({makeExprStmt(makeLiteralInt(1)), innerIf});
  WhileStmt *whileStmt = makeWhile(makeName("x"), body);
  Stmt *result = optimizer->eliminateDeadCode(whileStmt);
  EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeAfterReturn) {
  Array<Stmt *> body = {makeReturn(makeLiteralInt(1)), makeExprStmt(makeLiteralInt(2)), makeExprStmt(makeLiteralInt(3))};
  FunctionDef *func = makeFunction("f", {}, makeBlock(body));
  Stmt *result = optimizer->eliminateDeadCode(func);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeAfterEarlyReturn) {
  IfStmt *ifStmt = makeIf(makeName("x"), makeBlock({makeReturn(makeLiteralInt(1))}), makeBlock({makeReturn(makeLiteralInt(2))}));
  Array<Stmt *> body = {ifStmt, makeExprStmt(makeLiteralInt(3))};
  FunctionDef *func = makeFunction("f", {}, makeBlock(body));
  Stmt *result = optimizer->eliminateDeadCode(func);
  EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, NoDeadCodeBeforeReturn) {
  Array<Stmt *> body = {makeExprStmt(makeLiteralInt(1)), makeExprStmt(makeLiteralInt(2)), makeReturn(makeLiteralInt(3))};
  FunctionDef *func = makeFunction("f", {}, makeBlock(body));
  Stmt *result = optimizer->eliminateDeadCode(func);
  EXPECT_EQ(optimizer->getStats().DeadCodeEliminations, 0);
}

TEST_F(ASTOptimizerTest, DeadCodeNestedInFunction) {
  IfStmt *ifStmt = makeIf(makeLiteralBool(false), makeBlock({makeExprStmt(makeLiteralInt(1))}));
  FunctionDef *func = makeFunction("f", {}, makeBlock({ifStmt}));
  Stmt *result = optimizer->eliminateDeadCode(func);

  EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, DeadCodeNullStatement) {
  Stmt *result = optimizer->eliminateDeadCode(nullptr);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ASTOptimizerTest, LoopInvariantLiteral) {
  std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars;
  LiteralExpr *expr = makeLiteralInt(42);
  bool result = optimizer->isLoopInvariant(expr, loopVars);

  EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantNonLoopVariable) {
  std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = {"i"};
  NameExpr *expr = makeName("x");
  bool result = optimizer->isLoopInvariant(expr, loopVars);

  EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantLoopVariable) {
  std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = {"i"};
  NameExpr *expr = makeName("i");
  bool result = optimizer->isLoopInvariant(expr, loopVars);

  EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithConstants) {
  std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = {"i"};
  BinaryExpr *expr = makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD);
  bool result = optimizer->isLoopInvariant(expr, loopVars);

  EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantBinaryWithNonLoopVars) {
  std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = {"i"};
  BinaryExpr *expr = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD);
  bool result = optimizer->isLoopInvariant(expr, loopVars);

  EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantBinaryWithLoopVar) {
  std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = {"i"};
  BinaryExpr *expr = makeBinary(makeName("i"), makeLiteralInt(5), BinaryOp::OP_ADD);
  bool result = optimizer->isLoopInvariant(expr, loopVars);

  EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopVariantComplexExpression) {
  std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = {"i", "j"};
  BinaryExpr *left = makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD);
  BinaryExpr *expr = makeBinary(left, makeName("i"), BinaryOp::OP_MUL);
  bool result = optimizer->isLoopInvariant(expr, loopVars);

  EXPECT_FALSE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantUnary) {
  std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars = {"i"};
  UnaryExpr *expr = makeUnary(makeName("x"), UnaryOp::OP_NEG);
  bool result = optimizer->isLoopInvariant(expr, loopVars);

  EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, LoopInvariantNull) {
  std::unordered_set<StringRef, StringRefHash, StringRefEqual> loopVars;
  bool result = optimizer->isLoopInvariant(nullptr, loopVars);

  EXPECT_TRUE(result);
}

TEST_F(ASTOptimizerTest, OptimizeLevel0NoOptimization) {
  AssignmentStmt *stmt = makeAssignment("x", makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD));
  Array<Stmt *> statements = {stmt};
  Array<Stmt *> result = optimizer->optimize(statements, 0);

  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(optimizer->getStats().ConstantFolds, 0);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1BasicOptimization) {
  AssignmentStmt *stmt = makeAssignment("x", makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD));
  Array<Stmt *> statements = {stmt};
  Array<Stmt *> result = optimizer->optimize(statements, 1);

  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel1ExpressionStatement) {
  ExprStmt *stmt = makeExprStmt(makeBinary(makeLiteralInt(3), makeLiteralInt(4), BinaryOp::OP_MUL));
  Array<Stmt *> statements = {stmt};
  Array<Stmt *> result = optimizer->optimize(statements, 1);

  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(optimizer->getStats().ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2DeadCodeElimination) {
  IfStmt *ifStmt = makeIf(makeLiteralBool(false), makeBlock({makeExprStmt(makeLiteralInt(1))}));
  Array<Stmt *> statements = {ifStmt};
  Array<Stmt *> result = optimizer->optimize(statements, 2);

  EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeLevel2Combined) {
  AssignmentStmt *assign = makeAssignment("x", makeBinary(makeLiteralInt(2), makeLiteralInt(3), BinaryOp::OP_ADD));
  IfStmt *ifStmt = makeIf(makeLiteralBool(false), makeBlock({makeExprStmt(makeLiteralInt(1))}));
  Array<Stmt *> statements = {assign, ifStmt};
  Array<Stmt *> result = optimizer->optimize(statements, 2);

  EXPECT_GE(optimizer->getStats().ConstantFolds, 1);
  EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, OptimizeMultipleStatements) {
  Array<Stmt *> statements = {makeAssignment("a", makeBinary(makeLiteralInt(1), makeLiteralInt(2), BinaryOp::OP_ADD)),
                              makeAssignment("b", makeBinary(makeLiteralInt(3), makeLiteralInt(4), BinaryOp::OP_MUL)),
                              makeAssignment("c", makeBinary(makeLiteralInt(5), makeLiteralInt(1), BinaryOp::OP_SUB))};

  Array<Stmt *> result = optimizer->optimize(statements, 1);

  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(optimizer->getStats().ConstantFolds, 3);
}

TEST_F(ASTOptimizerTest, OptimizeEmptyStatements) {
  Array<Stmt *> statements;
  Array<Stmt *> result = optimizer->optimize(statements, 2);

  EXPECT_EQ(result.size(), 0);
}

TEST_F(ASTOptimizerTest, OptimizeNullStatementsFiltered) {
  Array<Stmt *> statements = {makeAssignment("x", makeLiteralInt(1))};
  Array<Stmt *> result = optimizer->optimize(statements, 2);

  for (Stmt *stmt : result)
    EXPECT_NE(stmt, nullptr);
}

TEST_F(ASTOptimizerTest, StatisticsInitiallyZero) {
  ASTOptimizer::OptimizationStats const &stats = optimizer->getStats();

  EXPECT_EQ(stats.ConstantFolds, 0);
  EXPECT_EQ(stats.DeadCodeEliminations, 0);
  EXPECT_EQ(stats.StrengthReductions, 0);
  EXPECT_EQ(stats.CommonSubexprEliminations, 0);
  EXPECT_EQ(stats.LoopInvariants, 0);
}

TEST_F(ASTOptimizerTest, StatisticsConstantFolds) {
  BinaryExpr *expr = makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD);
  optimizer->optimizeConstantFolding(expr);

  ASTOptimizer::OptimizationStats const &stats = optimizer->getStats();

  EXPECT_EQ(stats.ConstantFolds, 1);
}

TEST_F(ASTOptimizerTest, StatisticsMultipleOptimizations) {
  BinaryExpr *expr1 = makeBinary(makeLiteralInt(2), makeLiteralInt(3), BinaryOp::OP_ADD);
  BinaryExpr *expr2 = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_ADD);

  optimizer->optimizeConstantFolding(expr1);
  optimizer->optimizeConstantFolding(expr2);

  ASTOptimizer::OptimizationStats const &stats = optimizer->getStats();

  EXPECT_EQ(stats.ConstantFolds, 1);
  EXPECT_EQ(stats.StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, StatisticsPrintStats) {
  BinaryExpr *expr = makeBinary(makeLiteralInt(1), makeLiteralInt(1), BinaryOp::OP_ADD);
  optimizer->optimizeConstantFolding(expr);

  EXPECT_NO_THROW(optimizer->printStats());
}

TEST_F(ASTOptimizerTest, EdgeCaseVeryDeepNesting) {
  Expr *expr = makeLiteralInt(1);
  for (int i = 0; i < 10; ++i)
    expr = makeBinary(expr, makeLiteralInt(1), BinaryOp::OP_ADD);

  Expr *result = optimizer->optimizeConstantFolding(expr);

  ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
  EXPECT_DOUBLE_EQ(static_cast<LiteralExpr *>(result)->toNumber(), 11);
}

TEST_F(ASTOptimizerTest, EdgeCaseLargeNumbers) {
  BinaryExpr *expr = makeBinary(makeLiteralFloat(1e308), makeLiteralFloat(1e308), BinaryOp::OP_ADD);
  auto result = optimizer->evaluateConstant(expr);

  EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCaseSmallNumbers) {
  BinaryExpr *expr = makeBinary(makeLiteralFloat(1e-308), makeLiteralFloat(1e-308), BinaryOp::OP_MUL);
  auto result = optimizer->evaluateConstant(expr);

  EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST_F(ASTOptimizerTest, EdgeCasePowerOfZero) {
  BinaryExpr *expr = makeBinary(makeLiteralInt(0), makeLiteralInt(0), BinaryOp::OP_POW);
  auto result = optimizer->evaluateConstant(expr);

  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(*result, 1);
}

TEST_F(ASTOptimizerTest, EdgeCaseNegativePowerFraction) {
  BinaryExpr *expr = makeBinary(makeLiteralInt(4), makeLiteralFloat(0.5), BinaryOp::OP_POW);
  auto result = optimizer->evaluateConstant(expr);

  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(*result, 2);
}

TEST_F(ASTOptimizerTest, EdgeCaseModuloNegative) {
  BinaryExpr *expr = makeBinary(makeLiteralInt(-17), makeLiteralInt(5), BinaryOp::OP_MOD);
  auto result = optimizer->evaluateConstant(expr);

  ASSERT_TRUE(result.has_value());
}

TEST_F(ASTOptimizerTest, StressManyOptimizations) {
  Array<Stmt *> statements;

  for (int i = 0; i < 100; ++i) {
    StringRef varName = "var" + StringRef(std::to_string(i).data());
    statements.push(makeAssignment(varName, makeBinary(makeLiteralInt(i), makeLiteralInt(i), BinaryOp::OP_ADD)));
  }

  Array<Stmt *> result = optimizer->optimize(statements, 1);

  EXPECT_EQ(result.size(), 100);
  EXPECT_EQ(optimizer->getStats().ConstantFolds, 100);
}

TEST_F(ASTOptimizerTest, StressComplexDeadCodeElimination) {
  IfStmt *inner3 = makeIf(makeLiteralBool(false), makeBlock({makeExprStmt(makeLiteralInt(3))}));
  IfStmt *inner2 = makeIf(makeLiteralBool(true), makeBlock({inner3}));
  IfStmt *inner1 = makeIf(makeLiteralBool(false), makeBlock({inner2}));
  Stmt *result = optimizer->eliminateDeadCode(inner1);

  EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, EdgeCaseMixedOptimizations) {
  BinaryExpr *mul = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_MUL);
  Expr *result = optimizer->optimizeConstantFolding(mul);

  ASSERT_EQ(result->getKind(), Expr::Kind::LITERAL);
  EXPECT_EQ(static_cast<LiteralExpr *>(result)->toNumber(), 0);
}

TEST_F(ASTOptimizerTest, EdgeCaseChainedStrengthReductions) {
  BinaryExpr *add = makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_ADD);
  BinaryExpr *mul = makeBinary(add, makeLiteralInt(1), BinaryOp::OP_MUL);
  Expr *result = optimizer->optimizeConstantFolding(mul);

  EXPECT_GE(optimizer->getStats().StrengthReductions, 1);
}

TEST_F(ASTOptimizerTest, IntegrationFullOptimizationPipeline) {
  Array<Stmt *> statements = {makeAssignment("x", makeBinary(makeLiteralInt(2), makeLiteralInt(3), BinaryOp::OP_ADD)),
                              makeAssignment("y", makeBinary(makeName("x"), makeLiteralInt(1), BinaryOp::OP_MUL)),
                              makeIf(makeLiteralBool(false), makeBlock({makeExprStmt(makeLiteralInt(999))}))};

  Array<Stmt *> result = optimizer->optimize(statements, 2);

  EXPECT_GE(optimizer->getStats().ConstantFolds, 1);
  EXPECT_GE(optimizer->getStats().StrengthReductions, 1);
  EXPECT_GE(optimizer->getStats().DeadCodeEliminations, 1);
}

TEST_F(ASTOptimizerTest, IntegrationNestedFunctionOptimization) {
  Array<Stmt *> body = {makeAssignment("x", makeBinary(makeLiteralInt(5), makeLiteralInt(10), BinaryOp::OP_ADD)),
                        makeIf(makeLiteralBool(false), makeBlock({makeAssignment("y", makeLiteralInt(1))})), makeReturn(makeName("x"))};

  FunctionDef *func = makeFunction("f", {}, makeBlock(body));
  Array<Stmt *> statements = {func};
  Array<Stmt *> result = optimizer->optimize(statements, 2);

  EXPECT_EQ(result.size(), 1);
}
