#include "../../include/IR/codegen.hpp"
#include "../../include/IR/env.hpp"
#include "../../include/IR/value.hpp"
#include "../../include/ast/ast.hpp"
#include "../../include/ast/expr.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>

using namespace mylang;

// Test fixture for CodeGenerator tests
class CodeGeneratorTest : public ::testing::Test {
protected:
    std::unique_ptr<IR::Environment> env;
    std::unique_ptr<IR::CodeGenerator> codegen;

    void SetUp() override
    {
        env = std::make_unique<IR::Environment>();
        codegen = std::make_unique<IR::CodeGenerator>(env.get());
    }

    void TearDown() override
    {
        codegen.reset();
        env.reset();
    }

    // Helper to create literal expressions
    ast::LiteralExpr* makeLiteral(double value)
    {
        return ast::makeLiteral(ast::LiteralExpr::Type::NUMBER, StringRef(std::to_string(value).data()));
    }

    ast::LiteralExpr* makeLiteral(bool value)
    {
        return ast::makeLiteral(ast::LiteralExpr::Type::BOOLEAN, value ? "true" : "false");
    }

    // Helper to create call expressions
    ast::CallExpr* makeCall(StringRef const& funcName, std::vector<ast::Expr*> const& args)
    {
        return ast::makeCall(ast::makeName(funcName), ast::makeList(args));
    }
};

// LITERAL EXPRESSION TESTS

TEST_F(CodeGeneratorTest, EvalLiteralInteger)
{
    ast::LiteralExpr* literal = makeLiteral(42.0);
    IR::Value result = codegen->eval(literal);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, EvalLiteralZero)
{
    ast::LiteralExpr* literal = makeLiteral(0.0);
    IR::Value result = codegen->eval(literal);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 0.0);
}

TEST_F(CodeGeneratorTest, EvalLiteralNegative)
{
    ast::LiteralExpr* literal = makeLiteral(-100.5);
    IR::Value result = codegen->eval(literal);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), -100.5);
}

TEST_F(CodeGeneratorTest, EvalLiteralBoolean)
{
    ast::LiteralExpr* literal = makeLiteral(true);
    IR::Value result = codegen->eval(literal);

    EXPECT_TRUE(result.isBool());
    EXPECT_TRUE(*result.asBool());
}

TEST_F(CodeGeneratorTest, EvalNullNode)
{
    IR::Value result = codegen->eval(nullptr);
    EXPECT_TRUE(result.isNone() /*|| !result.isValid()*/);
}

// BINARY EXPRESSION TESTS - ARITHMETIC

TEST_F(CodeGeneratorTest, BinaryAddition)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 30.0);
}

TEST_F(CodeGeneratorTest, BinarySubtraction)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(50.0), makeLiteral(30.0), tok::TokenType::OP_MINUS);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 20.0);
}

TEST_F(CodeGeneratorTest, BinaryMultiplication)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(6.0), makeLiteral(7.0), tok::TokenType::OP_STAR);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, BinaryDivision)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(100.0), makeLiteral(4.0), tok::TokenType::OP_SLASH);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 25.0);
}

TEST_F(CodeGeneratorTest, BinaryDivisionByZero)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), makeLiteral(0.0), tok::TokenType::OP_SLASH);

    // Division by zero should be handled in IR::Value class
    // This test verifies it doesn't crash
    IR::Value result = codegen->eval(expr);
    // Result might be inf, nan, or throw - depends on IR::Value implementation
}

TEST_F(CodeGeneratorTest, BinaryChainedOperations)
{
    // (10 + 20) * 3
    ast::BinaryExpr* inner = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* outer = makeBinary(inner, makeLiteral(3.0), tok::TokenType::OP_STAR);

    IR::Value result = codegen->eval(outer);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 90.0);
}

TEST_F(CodeGeneratorTest, BinaryNestedSubtraction)
{
    // 100 - (50 - 20)
    ast::BinaryExpr* inner = makeBinary(makeLiteral(50.0), makeLiteral(20.0), tok::TokenType::OP_MINUS);
    ast::BinaryExpr* outer = makeBinary(makeLiteral(100.0), inner, tok::TokenType::OP_MINUS);

    IR::Value result = codegen->eval(outer);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 70.0);
}

// BINARY EXPRESSION TESTS - COMPARISON

TEST_F(CodeGeneratorTest, BinaryEquality)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(42.0), makeLiteral(42.0), tok::TokenType::OP_EQ);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isBool());
    EXPECT_TRUE(*result.asBool());
}

TEST_F(CodeGeneratorTest, BinaryInequality)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_EQ);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isBool());
    EXPECT_FALSE(*result.asBool());
}

TEST_F(CodeGeneratorTest, BinaryGreaterThan)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(50.0), makeLiteral(30.0), tok::TokenType::OP_GT);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isBool());
    EXPECT_TRUE(*result.asBool());
}

TEST_F(CodeGeneratorTest, BinaryGreaterThanFalse)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(20.0), makeLiteral(30.0), tok::TokenType::OP_GT);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isBool());
    EXPECT_FALSE(*result.asBool());
}

TEST_F(CodeGeneratorTest, BinaryGreaterThanOrEqual)
{
    ast::BinaryExpr* expr1 = makeBinary(makeLiteral(50.0), makeLiteral(50.0), tok::TokenType::OP_GTE);
    IR::Value result1 = codegen->eval(expr1);

    EXPECT_TRUE(result1.isBool());
    EXPECT_TRUE(*result1.asBool());

    ast::BinaryExpr* expr2 = makeBinary(makeLiteral(51.0), makeLiteral(50.0), tok::TokenType::OP_GTE);
    IR::Value result2 = codegen->eval(expr2);

    EXPECT_TRUE(result2.isBool());
    EXPECT_TRUE(*result2.asBool());
}

TEST_F(CodeGeneratorTest, BinaryLessThan)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_LT);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isBool());
    EXPECT_TRUE(*result.asBool());
}

TEST_F(CodeGeneratorTest, BinaryLessThanOrEqual)
{
    ast::BinaryExpr* expr1 = makeBinary(makeLiteral(30.0), makeLiteral(30.0), tok::TokenType::OP_LTE);
    IR::Value result1 = codegen->eval(expr1);

    EXPECT_TRUE(result1.isBool());
    EXPECT_TRUE(*result1.asBool());

    ast::BinaryExpr* expr2 = makeBinary(makeLiteral(29.0), makeLiteral(30.0), tok::TokenType::OP_LTE);
    IR::Value result2 = codegen->eval(expr2);

    EXPECT_TRUE(result2.isBool());
    EXPECT_TRUE(*result2.asBool());
}

// UNARY EXPRESSION TESTS

TEST_F(CodeGeneratorTest, UnaryNegation)
{
    ast::UnaryExpr* expr = makeUnary(makeLiteral(42.0), tok::TokenType::OP_MINUS);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), -42.0);
}

TEST_F(CodeGeneratorTest, UnaryNegationNegative)
{
    ast::UnaryExpr* expr = makeUnary(makeLiteral(-10.0), tok::TokenType::OP_MINUS);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 10.0);
}

TEST_F(CodeGeneratorTest, UnaryLogicalNot)
{
    ast::UnaryExpr* expr = makeUnary(makeLiteral(true), tok::TokenType::KW_NOT);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isBool());
    EXPECT_FALSE(*result.asBool());
}

TEST_F(CodeGeneratorTest, UnaryLogicalNotFalse)
{
    ast::UnaryExpr* expr = makeUnary(makeLiteral(false), tok::TokenType::KW_NOT);
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isBool());
    EXPECT_TRUE(*result.asBool());
}

TEST_F(CodeGeneratorTest, UnaryDoubleNegation)
{
    ast::UnaryExpr* inner = makeUnary(makeLiteral(5.0), tok::TokenType::OP_MINUS);
    ast::UnaryExpr* outer = makeUnary(inner, tok::TokenType::OP_MINUS);

    IR::Value result = codegen->eval(outer);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 5.0);
}

// ASSIGNMENT EXPRESSION TESTS

TEST_F(CodeGeneratorTest, AssignmentSimple)
{
    ast::AssignmentExpr* expr = makeAssignmentExpr(ast::makeName("x"), makeLiteral(42.0));
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 42.0);

    // Verify it's stored in environment
    EXPECT_TRUE(env->exists(StringRef("x")));
    IR::Value stored = env->get(StringRef("x"));
    EXPECT_DOUBLE_EQ(*stored.asFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, AssignmentOverwrite)
{
    // First assignment
    ast::AssignmentExpr* expr1 = makeAssignmentExpr(ast::makeName("y"), makeLiteral(10.0));
    codegen->eval(expr1);

    // Second assignment (overwrite)
    ast::AssignmentExpr* expr2 = makeAssignmentExpr(ast::makeName("y"), makeLiteral(20.0));
    IR::Value result = codegen->eval(expr2);

    EXPECT_DOUBLE_EQ(*result.asFloat(), 20.0);

    // Verify updated value
    IR::Value stored = env->get(StringRef("y"));
    EXPECT_DOUBLE_EQ(*stored.asFloat(), 20.0);
}

TEST_F(CodeGeneratorTest, AssignmentExpression)
{
    // x = 10 + 20
    ast::BinaryExpr* value = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS);
    ast::AssignmentExpr* expr = makeAssignmentExpr(ast::makeName("z"), value);

    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(*result.asFloat(), 30.0);
}

/*
TEST_F(CodeGeneratorTest, AssignmentInvalidTarget)
{
    // Create an assignment with a non-name target (should throw)
    ast::LiteralExpr* invalidTarget = makeLiteral(42.0);
    ast::AssignmentExpr* expr = ast::makeAssignmentExpr(invalidTarget, makeLiteral(10.0));

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}
*/

// NAME EXPRESSION TESTS

TEST_F(CodeGeneratorTest, NameExpressionExists)
{
    // Define a variable first
    env->define(StringRef("myvar"), IR::Value(123.0));

    ast::NameExpr* expr = ast::makeName("myvar");
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 123.0);
}

TEST_F(CodeGeneratorTest, NameExpressionUndefined)
{
    ast::NameExpr* expr = ast::makeName("undefined_var");

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

TEST_F(CodeGeneratorTest, NameExpressionInExpression)
{
    // x = 10, then compute x + 5
    env->define(StringRef("x"), IR::Value(10.0));

    ast::BinaryExpr* expr = makeBinary(ast::makeName("x"), makeLiteral(5.0), tok::TokenType::OP_PLUS);
    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(*result.asFloat(), 15.0);
}

// LIST EXPRESSION TESTS

TEST_F(CodeGeneratorTest, ListEmpty)
{
    ast::ListExpr* expr = ast::makeList({});
    IR::Value result = codegen->eval(expr);

    // Empty list should return empty value or empty list
    EXPECT_TRUE(result.isList() /*|| !result.isValid()*/);
}

TEST_F(CodeGeneratorTest, ListSingleElement)
{
    ast::ListExpr* expr = ast::makeList({ makeLiteral(42.0) });

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isList());
    auto list = result.asList();
    EXPECT_EQ(list->size(), 1);
    EXPECT_DOUBLE_EQ(*(*list)[0].asFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, ListMultipleElements)
{
    ast::ListExpr* expr = ast::makeList({ makeLiteral(1.0), makeLiteral(2.0), makeLiteral(3.0), makeLiteral(4.0) });

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isList());
    auto list = result.asList();
    EXPECT_EQ(list->size(), 4);
    EXPECT_DOUBLE_EQ(*(*list)[0].asFloat(), 1.0);
    EXPECT_DOUBLE_EQ(*(*list)[1].asFloat(), 2.0);
    EXPECT_DOUBLE_EQ(*(*list)[2].asFloat(), 3.0);
    EXPECT_DOUBLE_EQ(*(*list)[3].asFloat(), 4.0);
}

TEST_F(CodeGeneratorTest, ListWithExpressions)
{
    // [10 + 5, 20 * 2]
    ast::ListExpr* expr = ast::makeList({ makeBinary(makeLiteral(10.0), makeLiteral(5.0), tok::TokenType::OP_PLUS),
        makeBinary(makeLiteral(20.0), makeLiteral(2.0), tok::TokenType::OP_STAR) });

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isList());
    auto list = result.asList();
    EXPECT_EQ(list->size(), 2);
    EXPECT_DOUBLE_EQ(*(*list)[0].asFloat(), 15.0);
    EXPECT_DOUBLE_EQ(*(*list)[1].asFloat(), 40.0);
}

TEST_F(CodeGeneratorTest, ListMixedTypes)
{
    ast::ListExpr* expr = ast::makeList({ makeLiteral(42.0),
        makeLiteral(true),
        makeLiteral(-10.5) });

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isList());
    auto list = result.asList();
    EXPECT_EQ(list->size(), 3);
    EXPECT_TRUE((*list)[0].isNumber());
    EXPECT_TRUE((*list)[1].isBool());
    EXPECT_TRUE((*list)[2].isNumber());
}

// CALL EXPRESSION TESTS

TEST_F(CodeGeneratorTest, CallUndefinedFunction)
{
    ast::CallExpr* expr = makeCall("undefinedFunc", {});

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

TEST_F(CodeGeneratorTest, CallInvalidCallee)
{
    // Create a call where callee is not a NameExpr
    ast::LiteralExpr* invalidCallee = makeLiteral(42.0);
    ast::CallExpr* expr = new ast::CallExpr(invalidCallee, {});

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

TEST_F(CodeGeneratorTest, CallUserFunctionNoArgs)
{
    // Define a simple function that returns a constant
    IR::Value::Function func;
    func.params = {};
    func.body = makeLiteral(100.0);
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("getHundred"), funcValue);

    ast::CallExpr* expr = makeCall("getHundred", {});
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 100.0);
}

TEST_F(CodeGeneratorTest, CallUserFunctionWithArgs)
{
    // Define a function that takes two parameters and adds them
    IR::Value::Function func;
    func.params = { StringRef("a"), StringRef("b") };
    func.body = ast::makeBinary(ast::makeName("a"), ast::makeName("b"), tok::TokenType::OP_PLUS);
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("add"), funcValue);

    ast::CallExpr* expr = makeCall("add", { makeLiteral(10.0), makeLiteral(20.0) });
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(*result.asFloat(), 30.0);
}

TEST_F(CodeGeneratorTest, CallUserFunctionWrongArgCount)
{
    // Define a function expecting 2 args
    IR::Value::Function func;
    func.params = { StringRef("x"), StringRef("y") };
    func.body = makeLiteral(0.0);
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("twoArgs"), funcValue);

    // Call with wrong number of arguments
    ast::CallExpr* expr = makeCall("twoArgs", { makeLiteral(10.0) });

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

TEST_F(CodeGeneratorTest, CallEagerEvaluation)
{
    // Verify arguments are evaluated before function call
    env->define(StringRef("x"), IR::Value(5.0));

    IR::Value::Function func;
    func.params = { StringRef("arg") };
    func.body = ast::makeName("arg");
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("identity"), funcValue);

    // Call with expression argument
    ast::BinaryExpr* arg = makeBinary(ast::makeName("x"), makeLiteral(2.0), tok::TokenType::OP_STAR);
    ast::CallExpr* expr = makeCall("identity", { arg });

    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(*result.asFloat(), 10.0);
}

// FUNCTION CALL ENVIRONMENT TESTS

TEST_F(CodeGeneratorTest, FunctionLexicalScoping)
{
    // Define outer variable
    env->define(StringRef("outer"), IR::Value(100.0));

    // Define function that uses outer variable
    IR::Value::Function func;
    func.params = {};
    func.body = ast::makeName("outer");
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("getOuter"), funcValue);

    // Call function
    ast::CallExpr* expr = makeCall("getOuter", {});
    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(*result.asFloat(), 100.0);
}

TEST_F(CodeGeneratorTest, FunctionParameterShadowing)
{
    // Define outer variable
    env->define(StringRef("x"), IR::Value(10.0));

    // Define function with parameter named 'x'
    IR::Value::Function func;
    func.params = { StringRef("x") };
    func.body = ast::makeName("x");
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("getX"), funcValue);

    // Call with different value
    ast::CallExpr* expr = makeCall("getX", { makeLiteral(99.0) });
    IR::Value result = codegen->eval(expr);

    // Should return parameter value, not outer variable
    EXPECT_DOUBLE_EQ(*result.asFloat(), 99.0);

    // Outer variable should be unchanged
    EXPECT_DOUBLE_EQ(*env->get(StringRef("x")).asFloat(), 10.0);
}

TEST_F(CodeGeneratorTest, FunctionEnvironmentRestore)
{
    // Define variable before function call
    env->define(StringRef("temp"), IR::Value(1.0));

    // Define function that modifies a parameter
    IR::Value::Function func;
    func.params = { StringRef("temp") };
    func.body = ast::makeName("temp");
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("useTemp"), funcValue);

    // Call function
    ast::CallExpr* expr = makeCall("useTemp", { makeLiteral(999.0) });
    codegen->eval(expr);

    // Original environment variable should be unchanged
    EXPECT_DOUBLE_EQ(*env->get(StringRef("temp")).asFloat(), 1.0);
}

TEST_F(CodeGeneratorTest, FunctionExceptionEnvironmentRestore)
{
    env->define(StringRef("orig"), IR::Value(50.0));

    // Define function that will throw (accessing undefined variable)
    IR::Value::Function func;
    func.params = {};
    func.body = ast::makeName("undefined_var");
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("throwFunc"), funcValue);

    ast::CallExpr* expr = makeCall("throwFunc", {});

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);

    // Environment should still have original variable
    EXPECT_TRUE(env->exists(StringRef("orig")));
}

// ERROR HANDLING TESTS

TEST_F(CodeGeneratorTest, InvalidExpressionKind)
{
    // Create expression with INVALID kind
    class InvalidExpr : public ast::Expr {
    public:
        InvalidExpr()
            : Expr()
        {
        }
    };

    InvalidExpr* expr = new InvalidExpr();

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

TEST_F(CodeGeneratorTest, UnknownBinaryOperator)
{
    // Create binary expression with unknown operator
    // Assuming there's a token type that's not handled
    ast::BinaryExpr* expr = makeBinary(
        makeLiteral(1.0),
        makeLiteral(2.0), static_cast<tok::TokenType>(9999) // Invalid operator
    );

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

TEST_F(CodeGeneratorTest, UnknownUnaryOperator)
{
    ast::UnaryExpr* expr = ast::makeUnary(
        makeLiteral(5.0), static_cast<tok::TokenType>(8888) // Invalid operator
    );

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

TEST_F(CodeGeneratorTest, StatementNodeType)
{
    // Create a mock statement node
    class TestStatement : public ast::ASTNode {
    public:
        NodeType getNodeType() const override { return NodeType::STATEMENT; }
    };

    TestStatement* stmt = new TestStatement();

    EXPECT_THROW(codegen->eval(stmt), std::runtime_error);
}

TEST_F(CodeGeneratorTest, UnknownNodeType)
{
    // Create node with unknown type
    class UnknownNode : public ast::ASTNode {
    public:
        NodeType getNodeType() const override
        {
            return static_cast<NodeType>(9999);
        }
    };

    UnknownNode* node = new UnknownNode();

    EXPECT_THROW(codegen->eval(node), std::runtime_error);
}

// COMPLEX EXPRESSION TESTS

TEST_F(CodeGeneratorTest, ComplexArithmeticExpression)
{
    // (10 + 20) * (30 - 15) / 5
    ast::BinaryExpr* add = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* sub = makeBinary(makeLiteral(30.0), makeLiteral(15.0), tok::TokenType::OP_MINUS);
    ast::BinaryExpr* mul = makeBinary(add, sub, tok::TokenType::OP_STAR);
    ast::BinaryExpr* div = makeBinary(mul, makeLiteral(5.0), tok::TokenType::OP_SLASH);

    IR::Value result = codegen->eval(div);

    EXPECT_DOUBLE_EQ(*result.asFloat(), 90.0);
}

TEST_F(CodeGeneratorTest, ComplexComparisonChain)
{
    // (10 < 20) AND (30 > 15)  -- represented as separate evaluations
    ast::BinaryExpr* lt = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_LT);
    ast::BinaryExpr* gt = makeBinary(makeLiteral(30.0), makeLiteral(15.0), tok::TokenType::OP_GT);

    IR::Value result1 = codegen->eval(lt);
    IR::Value result2 = codegen->eval(gt);

    EXPECT_TRUE(*result1.asBool());
    EXPECT_TRUE(*result2.asBool());
}

TEST_F(CodeGeneratorTest, ComplexWithVariables)
{
    // x = 10, y = 20, result = (x + y) * 2
    env->define(StringRef("x"), IR::Value(10.0));
    env->define(StringRef("y"), IR::Value(20.0));

    ast::BinaryExpr* add = ast::makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* mul = ast::makeBinary(add, makeLiteral(2.0), tok::TokenType::OP_STAR);
    ast::AssignmentExpr* assign = ast::makeAssignmentExpr(ast::makeName("result"), mul);

    IR::Value result = codegen->eval(assign);

    EXPECT_DOUBLE_EQ(*result.asFloat(), 60.0);
    EXPECT_DOUBLE_EQ(*env->get(StringRef("result")).asFloat(), 60.0);
}

TEST_F(CodeGeneratorTest, NestedFunctionCalls)
{
    // Define inner function
    IR::Value::Function innerFunc;
    innerFunc.params = { StringRef("n") };
    innerFunc.body = ast::makeBinary(ast::makeName("n"), makeLiteral(2.0), tok::TokenType::OP_STAR);
    innerFunc.closure = env.get();

    env->define(StringRef("double"), IR::Value::makeFunction(innerFunc));

    // Define outer function that calls inner
    IR::Value::Function outerFunc;
    outerFunc.params = { StringRef("m") };
    outerFunc.body = makeCall("double", { ast::makeName("m") });
    outerFunc.closure = env.get();

    env->define(StringRef("quadruple"), IR::Value::makeFunction(outerFunc));

    // Call outer function
    ast::CallExpr* expr = makeCall("quadruple", { makeLiteral(5.0) });
    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(*result.asFloat(), 10.0);
}

// EDGE CASES AND BOUNDARY TESTS

TEST_F(CodeGeneratorTest, VeryLargeNumbers)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(1e308), makeLiteral(1e308), tok::TokenType::OP_PLUS);

    IR::Value result = codegen->eval(expr);

    // May result in infinity
    EXPECT_TRUE(result.isNumber());
}

TEST_F(CodeGeneratorTest, VerySmallNumbers)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(1e-308), makeLiteral(1e-308), tok::TokenType::OP_STAR);

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
}

TEST_F(CodeGeneratorTest, ZeroComparisons)
{
    ast::BinaryExpr* eq = makeBinary(makeLiteral(0.0), makeLiteral(-0.0), tok::TokenType::OP_EQ);
    IR::Value result = codegen->eval(eq);

    EXPECT_TRUE(result.isBool());
    // 0.0 and -0.0 should be equal
}

TEST_F(CodeGeneratorTest, EmptyEnvironmentAccess)
{
    // Try to access variable in fresh environment
    ast::NameExpr* expr = ast::makeName("nonexistent");

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

TEST_F(CodeGeneratorTest, RecursiveExpression)
{
    // Deep nesting: ((((5 + 5) + 5) + 5) + 5)
    ast::Expr* expr = makeLiteral(5.0);
    for (int i = 0; i < 4; ++i)
        expr = makeBinary(expr, makeLiteral(5.0), tok::TokenType::OP_PLUS);

    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(*result.asFloat(), 25.0);
}
