#include "../../include/IR/codegen.hpp"
#include "../../include/IR/env.hpp"
#include "../../include/IR/value.hpp"
#include "../../include/ast/ast.hpp"
#include "../../include/ast/expr.hpp"
#include "../../include/ast/printer.hpp"
#include "../../include/lex/file_manager.hpp"
#include "../../include/lex/lexer.hpp"
#include "../../include/lex/token.hpp"
#include "../../include/parser/parser.hpp"
#include "../test_config.h"
#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>

using namespace mylang;

inline ast::ASTPrinter AST_Printer;

std::filesystem::path test_cases_dir()
{
    static auto const dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
    return dir;
}

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

    IR::Value eval(std::string const& filename)
    {
        lex::FileManager file_manager(test_cases_dir() / filename);
        parser::Parser parser(&file_manager);

        std::vector<ast::Stmt*> stmts = parser.parseProgram();

        if (test_config::print_ast) {
            for (ast::Stmt const* s : stmts)
                AST_Printer.print(s);
        }

        IR::Value result;
        for (ast::Stmt const* stmt : stmts)
            result = codegen->eval(stmt);

        return result;
    }
};

// LITERAL EXPRESSION TESTS

TEST_F(CodeGeneratorTest, EvalLiteralInteger)
{
    ast::LiteralExpr* literal = makeLiteral(42.0);
    IR::Value result = codegen->eval(literal);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, EvalLiteralZero)
{
    ast::LiteralExpr* literal = makeLiteral(0.0);

    IR::Value result = codegen->eval(literal);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 0.0);
}

TEST_F(CodeGeneratorTest, EvalLiteralNegative)
{
    ast::LiteralExpr* literal = makeLiteral(-100.5);

    IR::Value result = codegen->eval(literal);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), -100.5);
}

TEST_F(CodeGeneratorTest, EvalLiteralBoolean)
{
    ast::LiteralExpr* literal = makeLiteral(true);

    IR::Value result = codegen->eval(literal);

    EXPECT_TRUE(result.isBool());
    EXPECT_TRUE(result.toBool());
}

TEST_F(CodeGeneratorTest, EvalNullNode)
{
    IR::Value result = codegen->eval(nullptr);
    EXPECT_TRUE(result.isNone() /**/);
}

// BINARY EXPRESSION TESTS - ARITHMETIC

TEST_F(CodeGeneratorTest, BinaryAddition)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS);

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 30.0);
}

TEST_F(CodeGeneratorTest, BinarySubtraction)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(50.0), makeLiteral(30.0), tok::TokenType::OP_MINUS);

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 20.0);
}

TEST_F(CodeGeneratorTest, BinaryMultiplication)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(6.0), makeLiteral(7.0), tok::TokenType::OP_STAR);

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, BinaryDivision)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(100.0), makeLiteral(4.0), tok::TokenType::OP_SLASH);

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 25.0);
}

TEST_F(CodeGeneratorTest, BinaryDivisionByZero)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), makeLiteral(0.0), tok::TokenType::OP_SLASH);

    // Division by zero should be handled in IR::Value class
    // This test verifies it doesn't crash
    IR::Value result = codegen->eval(expr);
    /// TODO: implement inf and nan into IR::Value
}

TEST_F(CodeGeneratorTest, BinaryChainedOperations)
{
    // (10 + 20) * 3
    ast::BinaryExpr* inner = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS);
    ast::BinaryExpr* outer = makeBinary(inner, makeLiteral(3.0), tok::TokenType::OP_STAR);

    IR::Value result = codegen->eval(outer);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 90.0);
}

TEST_F(CodeGeneratorTest, BinaryNestedSubtraction)
{
    // 100 - (50 - 20)
    ast::BinaryExpr* inner = makeBinary(makeLiteral(50.0), makeLiteral(20.0), tok::TokenType::OP_MINUS);
    ast::BinaryExpr* outer = makeBinary(makeLiteral(100.0), inner, tok::TokenType::OP_MINUS);

    IR::Value result = codegen->eval(outer);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 70.0);
}

// BINARY EXPRESSION TESTS - COMPARISON

TEST_F(CodeGeneratorTest, BinaryEquality)
{
    ast::BinaryExpr* expr = makeBinary(makeLiteral(42.0), makeLiteral(42.0), tok::TokenType::OP_EQ);

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isBool());
    EXPECT_TRUE(result.toBool());
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
    EXPECT_DOUBLE_EQ(result.toFloat(), -42.0);
}

TEST_F(CodeGeneratorTest, UnaryNegationNegative)
{
    ast::UnaryExpr* expr = makeUnary(makeLiteral(-10.0), tok::TokenType::OP_MINUS);

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 10.0);
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
    EXPECT_DOUBLE_EQ(result.toFloat(), 5.0);
}

// ASSIGNMENT EXPRESSION TESTS

TEST_F(CodeGeneratorTest, AssignmentSimple)
{
    ast::AssignmentExpr* expr = makeAssignmentExpr(ast::makeName("x"), makeLiteral(42.0));

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 42.0);

    // Verify it's stored in environment
    EXPECT_TRUE(env->exists(StringRef("x")));

    IR::Value stored = env->get(StringRef("x"));

    EXPECT_DOUBLE_EQ(stored.toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, AssignmentOverwrite)
{
    // First assignment
    ast::AssignmentExpr* expr1 = makeAssignmentExpr(ast::makeName("y"), makeLiteral(10.0));

    codegen->eval(expr1);

    // Second assignment (overwrite)
    ast::AssignmentExpr* expr2 = makeAssignmentExpr(ast::makeName("y"), makeLiteral(20.0));

    IR::Value result = codegen->eval(expr2);

    EXPECT_DOUBLE_EQ(result.toFloat(), 20.0);

    // Verify updated value
    IR::Value stored = env->get(StringRef("y"));
    EXPECT_DOUBLE_EQ(stored.toFloat(), 20.0);
}

TEST_F(CodeGeneratorTest, AssignmentExpression)
{
    // x = 10 + 20
    ast::BinaryExpr* value = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS);
    ast::AssignmentExpr* expr = makeAssignmentExpr(ast::makeName("z"), value);

    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(result.toFloat(), 30.0);
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
    EXPECT_DOUBLE_EQ(result.toFloat(), 123.0);
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

    EXPECT_DOUBLE_EQ(result.toFloat(), 15.0);
}

// LIST EXPRESSION TESTS

TEST_F(CodeGeneratorTest, ListEmpty)
{
    ast::ListExpr* expr = ast::makeList({});

    IR::Value result = codegen->eval(expr);

    // Empty list should return empty value or empty list
    EXPECT_TRUE(result.isList() || result.isNone() /**/);
}

TEST_F(CodeGeneratorTest, ListSingleElement)
{
    ast::ListExpr* expr = ast::makeList({ makeLiteral(42.0) });

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isList());

    std::vector<IR::Value>* list = result.asList();

    EXPECT_EQ(list->size(), 1);
    EXPECT_DOUBLE_EQ((*list)[0].toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, ListMultipleElements)
{
    ast::ListExpr* expr = ast::makeList({ makeLiteral(1.0), makeLiteral(2.0), makeLiteral(3.0), makeLiteral(4.0) });

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isList());

    auto list = result.asList();

    EXPECT_EQ(list->size(), 4);
    EXPECT_DOUBLE_EQ((*list)[0].toFloat(), 1.0);
    EXPECT_DOUBLE_EQ((*list)[1].toFloat(), 2.0);
    EXPECT_DOUBLE_EQ((*list)[2].toFloat(), 3.0);
    EXPECT_DOUBLE_EQ((*list)[3].toFloat(), 4.0);
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
    EXPECT_DOUBLE_EQ((*list)[0].toFloat(), 15.0);
    EXPECT_DOUBLE_EQ((*list)[1].toFloat(), 40.0);
}

TEST_F(CodeGeneratorTest, ListMixedTypes)
{
    ast::ListExpr* expr = ast::makeList({ makeLiteral(42.0), makeLiteral(true), makeLiteral(-10.5) });

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

    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNone());
}

TEST_F(CodeGeneratorTest, CallInvalidCallee)
{
    // Create a call where callee is not a NameExpr
    ast::LiteralExpr* invalidCallee = makeLiteral(42.0);
    ast::CallExpr* expr = ast::makeCall(invalidCallee, {});

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
    EXPECT_DOUBLE_EQ(result.toFloat(), 100.0);
}

TEST_F(CodeGeneratorTest, CallUserFunctionWithArgs)
{
    // Define a function that takes two parameters and adds them
    IR::Value::Function func;
    func.params = IR::makeValue(std::vector<IR::Value> { StringRef("a"), StringRef("b") });
    func.body = ast::makeBinary(ast::makeName("a"), ast::makeName("b"), tok::TokenType::OP_PLUS);
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("add"), funcValue);

    ast::CallExpr* expr = makeCall("add", { makeLiteral(10.0), makeLiteral(20.0) });
    IR::Value result = codegen->eval(expr);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 30.0);
}

TEST_F(CodeGeneratorTest, CallUserFunctionWrongArgCount)
{
    // Define a function expecting 2 args
    IR::Value::Function func;
    func.params = IR::makeValue(std::vector<IR::Value> { StringRef("x"), StringRef("y") });
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
    func.params = IR::makeValue({ StringRef("arg") });
    func.body = ast::makeName("arg");
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("identity"), funcValue);

    // Call with expression argument
    ast::BinaryExpr* arg = makeBinary(ast::makeName("x"), makeLiteral(2.0), tok::TokenType::OP_STAR);
    ast::CallExpr* expr = makeCall("identity", { arg });

    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(result.toFloat(), 10.0);
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

    EXPECT_DOUBLE_EQ(result.toFloat(), 100.0);
}

TEST_F(CodeGeneratorTest, FunctionParameterShadowing)
{
    // Define outer variable
    env->define(StringRef("x"), IR::Value(10.0));

    // Define function with parameter named 'x'
    IR::Value::Function func;
    func.params = IR::makeValue({ StringRef("x") });
    func.body = ast::makeName("x");
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("getX"), funcValue);

    // Call with different value
    ast::CallExpr* expr = makeCall("getX", { makeLiteral(99.0) });
    IR::Value result = codegen->eval(expr);

    // Should return parameter value, not outer variable
    EXPECT_DOUBLE_EQ(result.toFloat(), 99.0);

    // Outer variable should be unchanged
    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 10.0);
}

TEST_F(CodeGeneratorTest, FunctionEnvironmentRestore)
{
    // Define variable before function call
    env->define(StringRef("temp"), IR::Value(1.0));

    // Define function that modifies a parameter
    IR::Value::Function func;
    func.params = IR::makeValue({ StringRef("temp") });
    func.body = ast::makeName("temp");
    func.closure = env.get();

    IR::Value funcValue = IR::Value::makeFunction(func);
    env->define(StringRef("useTemp"), funcValue);

    // Call function
    ast::CallExpr* expr = makeCall("useTemp", { makeLiteral(999.0) });
    codegen->eval(expr);

    // Original environment variable should be unchanged
    EXPECT_DOUBLE_EQ(env->get(StringRef("temp")).toFloat(), 1.0);
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
    
    delete expr;
}

TEST_F(CodeGeneratorTest, UnknownBinaryOperator)
{
    // Create binary expression with unknown operator
    // Assuming there's a token type that's not handled
    ast::BinaryExpr* expr = makeBinary(makeLiteral(1.0), makeLiteral(2.0), static_cast<tok::TokenType>(9999));

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

TEST_F(CodeGeneratorTest, UnknownUnaryOperator)
{
    ast::UnaryExpr* expr = ast::makeUnary(makeLiteral(5.0), static_cast<tok::TokenType>(8888));

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
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

    delete node;
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

    EXPECT_DOUBLE_EQ(result.toFloat(), 90.0);
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

    EXPECT_DOUBLE_EQ(result.toFloat(), 60.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("result")).toFloat(), 60.0);
}

TEST_F(CodeGeneratorTest, NestedFunctionCalls)
{
    // Define inner function
    IR::Value::Function innerFunc;
    innerFunc.params = IR::makeValue({ StringRef("n") });
    innerFunc.body = ast::makeBinary(ast::makeName("n"), makeLiteral(2.0), tok::TokenType::OP_STAR);
    innerFunc.closure = env.get();

    env->define(StringRef("double"), IR::Value::makeFunction(innerFunc));

    // Define outer function that calls inner
    IR::Value::Function outerFunc;
    outerFunc.params = IR::makeValue({ StringRef("m") });
    outerFunc.body = makeCall("double", { ast::makeName("m") });
    outerFunc.closure = env.get();

    env->define(StringRef("quadruple"), IR::Value::makeFunction(outerFunc));

    // Call outer function
    ast::CallExpr* expr = makeCall("quadruple", { makeLiteral(5.0) });
    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(result.toFloat(), 10.0);
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

TEST_F(CodeGeneratorTest, RecursiveExpression)
{
    // Deep nesting: ((((5 + 5) + 5) + 5) + 5)
    ast::Expr* expr = makeLiteral(5.0);
    for (int i = 0; i < 4; ++i)
        expr = makeBinary(expr, makeLiteral(5.0), tok::TokenType::OP_PLUS);

    IR::Value result = codegen->eval(expr);

    EXPECT_DOUBLE_EQ(result.toFloat(), 25.0);
}

TEST_F(CodeGeneratorTest, AssignmentStmtDefineNew)
{
    // x = 42
    ast::AssignmentStmt* stmt = ast::makeAssignmentStmt(ast::makeName("x"), makeLiteral(42.0));
    IR::Value result = codegen->eval(stmt);

    // Assignment statement should return void/null
    EXPECT_TRUE(result.isNone() /**/);

    // Variable should be defined in environment
    EXPECT_TRUE(env->exists(StringRef("x")));
    IR::Value stored = env->get(StringRef("x"));
    EXPECT_TRUE(stored.isNumber());
    EXPECT_DOUBLE_EQ(stored.toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, AssignmentStmtUpdateExisting)
{
    // Define x first
    env->define(StringRef("x"), IR::Value::makeFloat(10.0));

    // x = 99
    ast::AssignmentStmt* stmt = makeAssignmentStmt(ast::makeName("x"), makeLiteral(99.0));
    codegen->eval(stmt);

    // Variable should be updated
    IR::Value stored = env->get(StringRef("x"));
    EXPECT_DOUBLE_EQ(stored.toFloat(), 99.0);
}

TEST_F(CodeGeneratorTest, AssignmentStmtWithExpression)
{
    // y = 10 + 20
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS);
    ast::AssignmentStmt* stmt = makeAssignmentStmt(ast::makeName("y"), expr);

    codegen->eval(stmt);

    IR::Value stored = env->get(StringRef("y"));
    EXPECT_DOUBLE_EQ(stored.toFloat(), 30.0);
}

TEST_F(CodeGeneratorTest, AssignmentStmtChained)
{
    // a = 5
    ast::AssignmentStmt* stmt1 = makeAssignmentStmt(ast::makeName("a"), makeLiteral(5.0));
    codegen->eval(stmt1);

    // b = a * 2
    ast::BinaryExpr* expr = makeBinary(ast::makeName("a"), makeLiteral(2.0), tok::TokenType::OP_STAR);
    ast::AssignmentStmt* stmt2 = makeAssignmentStmt(ast::makeName("b"), expr);
    codegen->eval(stmt2);

    EXPECT_DOUBLE_EQ(env->get(StringRef("a")).toFloat(), 5.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("b")).toFloat(), 10.0);
}

// STATEMENT TESTS - EXPRESSION STATEMENTS

TEST_F(CodeGeneratorTest, ExprStmtSimple)
{
    // 42; (expression as statement)
    ast::ExprStmt* stmt = makeExprStmt(makeLiteral(42.0));
    IR::Value result = codegen->eval(stmt);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, ExprStmtBinary)
{
    // 10 + 20;
    ast::BinaryExpr* expr = makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS);
    ast::ExprStmt* stmt = makeExprStmt(expr);

    IR::Value result = codegen->eval(stmt);

    EXPECT_DOUBLE_EQ(result.toFloat(), 30.0);
}

TEST_F(CodeGeneratorTest, ExprStmtFunctionCall)
{
    // Define a simple function that returns 100
    IR::Value::Function func;
    func.name = StringRef("getHundred");
    func.params = nullptr;
    func.body = makeExprStmt(makeLiteral(100.0));
    func.closure = env.get();

    env->define(StringRef("getHundred"), IR::Value::makeFunction(func));

    // getHundred();
    ast::CallExpr* call = makeCall("getHundred", {});
    ast::ExprStmt* stmt = makeExprStmt(call);

    IR::Value result = codegen->eval(stmt);

    EXPECT_DOUBLE_EQ(result.toFloat(), 100.0);
}

// STATEMENT TESTS - BLOCK STATEMENTS

TEST_F(CodeGeneratorTest, BlockStmtEmpty)
{
    // { }
    ast::BlockStmt* block = ast::makeBlock({});
    IR::Value result = codegen->eval(block);

    // Empty block returns empty list or void
    EXPECT_TRUE(result.isList() || result.isNone());
}

TEST_F(CodeGeneratorTest, BlockStmtSingle)
{
    // { 42; }
    ast::BlockStmt* block = ast::makeBlock({ makeExprStmt(makeLiteral(42.0)) });

    IR::Value result = codegen->eval(block);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, BlockStmtMultiple)
{
    // { 10; 20; 30; }
    ast::BlockStmt* block = ast::makeBlock({ makeExprStmt(makeLiteral(10.0)), makeExprStmt(makeLiteral(20.0)), makeExprStmt(makeLiteral(30.0)) });

    IR::Value result = codegen->eval(block);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 30.0);
}

TEST_F(CodeGeneratorTest, BlockStmtWithAssignments)
{
    // { x = 5; y = 10; x + y; }
    ast::BlockStmt* block = ast::makeBlock(
        { makeAssignmentStmt(ast::makeName("x"), makeLiteral(5.0)), makeAssignmentStmt(ast::makeName("y"), makeLiteral(10.0)),
            ast::makeExprStmt(ast::makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS)) });

    IR::Value result = codegen->eval(block);

    // Last statement result should be 15
    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 15.0);

    // Variables should exist in environment
    EXPECT_TRUE(env->exists(StringRef("x")));
    EXPECT_TRUE(env->exists(StringRef("y")));
}

// STATEMENT TESTS - IF STATEMENTS

TEST_F(CodeGeneratorTest, IfStmtTrueCondition_01)
{
    // if (true) { 42; }
    ast::IfStmt* ifStmt = ast::makeIf(makeLiteral(true), ast::makeBlock({ makeExprStmt(makeLiteral(42.0)) }));

    IR::Value result = codegen->eval(ifStmt);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, IfStmtFalseCondition)
{
    // if (false) { 42; }
    ast::IfStmt* ifStmt = makeIf(makeLiteral(false), ast::makeBlock({ makeExprStmt(makeLiteral(42.0)) }));

    IR::Value result = codegen->eval(ifStmt);

    // Should return void/none when condition is false and no else
    EXPECT_TRUE(result.isNone());
}

TEST_F(CodeGeneratorTest, IfStmtTrueWithElse)
{
    // if (true) { 10; } else { 20; }
    ast::IfStmt* ifStmt = makeIf(
        makeLiteral(true), ast::makeBlock({ makeExprStmt(makeLiteral(10.0)) }), ast::makeBlock({ makeExprStmt(makeLiteral(20.0)) }));

    IR::Value result = codegen->eval(ifStmt);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 10.0);
}

TEST_F(CodeGeneratorTest, IfStmtFalseWithElse)
{
    // if (false) { 10; } else { 20; }
    ast::IfStmt* ifStmt = makeIf(
        makeLiteral(false), ast::makeBlock({ makeExprStmt(makeLiteral(10.0)) }), ast::makeBlock({ makeExprStmt(makeLiteral(20.0)) }));

    IR::Value result = codegen->eval(ifStmt);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 20.0);
}

TEST_F(CodeGeneratorTest, IfStmtComparisonCondition_01)
{
    // x = 10; if (x > 5) { 100; } else { 200; }
    env->define(StringRef("x"), IR::Value::makeFloat(10.0));

    ast::IfStmt* ifStmt = makeIf(
        makeBinary(ast::makeName("x"), makeLiteral(5.0), tok::TokenType::OP_GT),
        ast::makeBlock({ makeExprStmt(makeLiteral(100.0)) }), ast::makeBlock({ makeExprStmt(makeLiteral(200.0)) }));

    IR::Value result = codegen->eval(ifStmt);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 100.0);
}

TEST_F(CodeGeneratorTest, IfStmtNestedIf)
{
    // if (true) { if (true) { 42; } }
    ast::IfStmt* innerIf = makeIf(makeLiteral(true), ast::makeBlock({ makeExprStmt(makeLiteral(42.0)) }));
    ast::IfStmt* outerIf = makeIf(makeLiteral(true), ast::makeBlock({ innerIf }));

    IR::Value result = codegen->eval(outerIf);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, IfStmtWithAssignment)
{
    // if (true) { x = 42; }
    ast::IfStmt* ifStmt = makeIf(makeLiteral(true), ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(42.0)) }));

    codegen->eval(ifStmt);

    EXPECT_TRUE(env->exists(StringRef("x")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 42.0);
}

// STATEMENT TESTS - WHILE STATEMENTS

TEST_F(CodeGeneratorTest, WhileStmtFalseCondition)
{
    // counter = 0; while (false) { counter = counter + 1; }
    env->define(StringRef("counter"), IR::Value::makeFloat(0.0));

    ast::WhileStmt* whileStmt = makeWhile(
        makeLiteral(false),
        ast::makeBlock({ makeAssignmentStmt(ast::makeName("counter"),
            makeBinary(ast::makeName("counter"), makeLiteral(1.0), tok::TokenType::OP_PLUS)) }));

    IR::Value result = codegen->eval(whileStmt);

    // Counter should still be 0
    EXPECT_DOUBLE_EQ(env->get(StringRef("counter")).toFloat(), 0.0);
}

TEST_F(CodeGeneratorTest, WhileStmtAccumulator)
{
    // sum = 0
    // i = 1
    // while (i <= 5)
    //     sum = sum + i
    //     i = i + 1

    eval("test_while_stmt_accumulator.txt");

    // Sum should be 1+2+3+4+5 = 15
    EXPECT_DOUBLE_EQ(env->get(StringRef("ا")).toFloat(), 15.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("ب")).toFloat(), 6.0);
}

TEST_F(CodeGeneratorTest, WhileStmtReturnValue)
{
    // x = 1
    // while (x < 3)
    //      x = x + 1

    IR::Value result = eval("test_while.txt");

    // Result should be the last block execution result
    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toInt(), 3);
}

// STATEMENT TESTS - FUNCTION DEFINITIONS

TEST_F(CodeGeneratorTest, FunctionDefNoParams)
{
    // function getFortyTwo() { 42; }
    ast::FunctionDef* funcDef = ast::makeFunction(ast::makeName("getFortyTwo"), ast::makeList({}), ast::makeBlock({ makeExprStmt(makeLiteral(42.0)) }));

    IR::Value result = codegen->eval(funcDef);

    // Function definition returns void
    EXPECT_TRUE(result.isNone() /**/);

    // Function should be defined in environment
    EXPECT_TRUE(env->exists(StringRef("getFortyTwo")));
    IR::Value funcValue = env->get(StringRef("getFortyTwo"));
    EXPECT_TRUE(funcValue.isFunction());
}

TEST_F(CodeGeneratorTest, FunctionDefWithSingleParam)
{
    // function double(x) { x * 2; }
    ast::FunctionDef* funcDef = ast::makeFunction(
        ast::makeName("double"),
        ast::makeList({ ast::makeName("x") }),
        ast::makeBlock({ makeExprStmt(makeBinary(ast::makeName("x"), makeLiteral(2.0), tok::TokenType::OP_STAR)) }));

    codegen->eval(funcDef);

    EXPECT_TRUE(env->exists(StringRef("double")));
    IR::Value funcValue = env->get(StringRef("double"));
    EXPECT_TRUE(funcValue.isFunction());
}

TEST_F(CodeGeneratorTest, FunctionDefWithMultipleParams)
{
    // function add(a, b) { a + b; }
    ast::FunctionDef* funcDef = ast::makeFunction(
        ast::makeName("add"),
        ast::makeList({ ast::makeName("a"), ast::makeName("b") }),
        ast::makeBlock({ ast::makeExprStmt(ast::makeBinary(ast::makeName("a"), ast::makeName("b"), tok::TokenType::OP_PLUS)) }));

    codegen->eval(funcDef);

    EXPECT_TRUE(env->exists(StringRef("add")));
}

TEST_F(CodeGeneratorTest, FunctionDefAndCall)
{
    // function square(n) { n * n; }
    // square(5)
    ast::FunctionDef* funcDef = ast::makeFunction(
        ast::makeName("square"),
        ast::makeList({ ast::makeName("n") }),
        ast::makeBlock({ ast::makeExprStmt(ast::makeBinary(ast::makeName("n"), ast::makeName("n"), tok::TokenType::OP_STAR)) }));

    codegen->eval(funcDef);

    // Call the function
    ast::CallExpr* call = ast::makeCall(ast::makeName("square"), ast::makeList({ makeLiteral(5.0) }));
    IR::Value result = codegen->eval(call);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 25.0);
}

TEST_F(CodeGeneratorTest, FunctionDefMultipleStatements)
{
    // function compute(x)
    //      y = x * 2
    //      y = y + 10
    // compute(5)

    IR::Value result = eval("test_function_def_multiple_statements.txt");

    EXPECT_TRUE(result.isNumber());
    // Result should be (5 * 2) + 10 = 20
    EXPECT_DOUBLE_EQ(result.toFloat(), 20.0);
}

TEST_F(CodeGeneratorTest, FunctionDefClosureCapture)
{
    // outer = 100; function getOuter() { outer; }
    env->define(StringRef("outer"), IR::Value::makeFloat(100.0));

    ast::FunctionDef* funcDef = ast::makeFunction(
        ast::makeName("getOuter"), ast::makeList({}), ast::makeBlock({ ast::makeExprStmt(ast::makeName("outer")) }));

    codegen->eval(funcDef);

    ast::CallExpr* call = makeCall("getOuter", {});
    IR::Value result = codegen->eval(call);

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 100.0);
}

// STATEMENT TESTS - INTEGRATION SCENARIOS

TEST_F(CodeGeneratorTest, IntegrationFibonacciIterative)
{
    // function fib(n) {
    //   if (n <= 1) { n; }
    //   else {
    //     a = 0; b = 1; i = 2;
    //     while (i <= n) {
    //       temp = a + b;
    //       a = b;
    //       b = temp;
    //       i = i + 1;
    //     }
    //     b;
    //   }
    // }

    ast::FunctionDef* funcDef = ast::makeFunction(
        ast::makeName("fib"), ast::makeList({ ast::makeName("n") }),
        ast::makeBlock({ makeIf(
            ast::makeBinary(ast::makeName("n"), makeLiteral(1.0), tok::TokenType::OP_LTE),
            ast::makeBlock({ ast::makeExprStmt(ast::makeName("n")) }),
            ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("a"), makeLiteral(0.0)),
                ast::makeAssignmentStmt(ast::makeName("b"), makeLiteral(1.0)),
                ast::makeAssignmentStmt(ast::makeName("i"), makeLiteral(2.0)),
                ast::makeWhile(ast::makeBinary(ast::makeName("i"), ast::makeName("n"), tok::TokenType::OP_LTE),
                    ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("temp"),
                                         ast::makeBinary(ast::makeName("a"), ast::makeName("b"), tok::TokenType::OP_PLUS)),
                        ast::makeAssignmentStmt(ast::makeName("a"), ast::makeName("b")),
                        ast::makeAssignmentStmt(ast::makeName("b"), ast::makeName("temp")),
                        ast::makeAssignmentStmt(ast::makeName("i"), ast::makeBinary(ast::makeName("i"), makeLiteral(1.0), tok::TokenType::OP_PLUS)) })),
                ast::makeExprStmt(ast::makeName("b")) })) }));

    codegen->eval(funcDef);

    // Test fib(6) = 8
    ast::CallExpr* call = ast::makeCall(ast::makeName("fib"), ast::makeList({ makeLiteral(6.0) }));
    IR::Value result = codegen->eval(call);

    // The result should be 8 (fibonacci of 6)
    EXPECT_EQ(result.toFloat(), 8.0);
}

TEST_F(CodeGeneratorTest, IntegrationFactorial)
{
    // function factorial(n)
    //   result = 1
    //   i = 1
    //   while (i <= n)
    //     result = result * i
    //     i = i + 1
    //   return result

    IR::Value result = eval("test_integration_factorial.txt");

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 120.0);
}

TEST_F(CodeGeneratorTest, IntegrationNestedFunctions)
{
    /// NOTE: it's here for convenienve
    /// do not know if we want something like this in the language
    // function outer(x)
    //   function inner(y)
    //       x + y; 
    //   inner(10);

    ast::FunctionDef* innerFunc = ast::makeFunction(
        ast::makeName("inner"),
        ast::makeList({ ast::makeName("y") }),
        ast::makeBlock({ ast::makeExprStmt(ast::makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_PLUS)) }));

    ast::FunctionDef* outerFunc = ast::makeFunction(
        ast::makeName("outer"),
        ast::makeList({ ast::makeName("x") }),
        ast::makeBlock({ innerFunc,
            ast::makeExprStmt(ast::makeCall(ast::makeName("inner"), ast::makeList({ makeLiteral(10.0) }))) }));

    codegen->eval(outerFunc);

    ast::CallExpr* call = makeCall("outer", { makeLiteral(5.0) });
    IR::Value result = codegen->eval(call);

    // Should get x (5) + y (10) = 15
    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toFloat(), 15.0);
}

TEST_F(CodeGeneratorTest, ZeroComparisons)
{
    ast::BinaryExpr* eq = makeBinary(makeLiteral(0.0), makeLiteral(-0.0), tok::TokenType::OP_EQ);
    IR::Value result = codegen->eval(eq);

    EXPECT_TRUE(result.isBool());
    // 0.0 and -0.0 should be equal
    EXPECT_TRUE(*result.asBool());
}

TEST_F(CodeGeneratorTest, EmptyEnvironmentAccess)
{
    // Try to access variable in fresh environment
    ast::NameExpr* expr = ast::makeName("nonexistent");

    EXPECT_THROW(codegen->eval(expr), std::runtime_error);
}

// STATEMENT TESTS

// ASSIGNMENT STATEMENT TESTS

TEST_F(CodeGeneratorTest, AssignmentStmtSimple)
{
    // x = 42
    ast::AssignmentStmt* stmt = makeAssignmentStmt(ast::makeName("x"), makeLiteral(42.0));
    IR::Value result = codegen->eval(stmt);

    // Assignment returns void/empty value
    EXPECT_TRUE(result.isNone());

    // Variable should be defined
    EXPECT_TRUE(env->exists(StringRef("x")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 42.0);
}

TEST_F(CodeGeneratorTest, AssignmentStmtExpression)
{
    // y = 10 + 20
    ast::AssignmentStmt* stmt = makeAssignmentStmt(ast::makeName("y"),
        makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS));

    codegen->eval(stmt);

    EXPECT_TRUE(env->exists(StringRef("y")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("y")).toFloat(), 30.0);
}

TEST_F(CodeGeneratorTest, AssignmentStmtReassign)
{
    // First assignment
    env->define(StringRef("z"), IR::Value::makeFloat(5.0));

    // Reassignment: z = 100
    ast::AssignmentStmt* stmt = makeAssignmentStmt(ast::makeName("z"), makeLiteral(100.0));
    codegen->eval(stmt);

    EXPECT_DOUBLE_EQ(env->get(StringRef("z")).toFloat(), 100.0);
}

TEST_F(CodeGeneratorTest, AssignmentStmtMultipleVariables)
{
    // a = 1, b = 2, c = 3
    ast::AssignmentStmt* stmt1 = makeAssignmentStmt(ast::makeName("a"), makeLiteral(1.0));
    ast::AssignmentStmt* stmt2 = makeAssignmentStmt(ast::makeName("b"), makeLiteral(2.0));
    ast::AssignmentStmt* stmt3 = makeAssignmentStmt(ast::makeName("c"), makeLiteral(3.0));

    codegen->eval(stmt1);
    codegen->eval(stmt2);
    codegen->eval(stmt3);

    EXPECT_DOUBLE_EQ(env->get(StringRef("a")).toFloat(), 1.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("b")).toFloat(), 2.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("c")).toFloat(), 3.0);
}

TEST_F(CodeGeneratorTest, AssignmentStmtWithVariableReference)
{
    // x = 10
    env->define(StringRef("x"), IR::Value::makeFloat(10.0));

    // y = x + 5
    ast::AssignmentStmt* stmt = makeAssignmentStmt(ast::makeName("y"),
        makeBinary(ast::makeName("x"), makeLiteral(5.0), tok::TokenType::OP_PLUS));

    codegen->eval(stmt);

    EXPECT_DOUBLE_EQ(env->get(StringRef("y")).toFloat(), 15.0);
}

// EXPRESSION STATEMENT TESTS

TEST_F(CodeGeneratorTest, ExprStmtBinaryExpression)
{
    // Expression statement: 10 + 20
    ast::ExprStmt* stmt = makeExprStmt(
        makeBinary(makeLiteral(10.0), makeLiteral(20.0), tok::TokenType::OP_PLUS));

    IR::Value result = codegen->eval(stmt);

    EXPECT_DOUBLE_EQ(result.toFloat(), 30.0);
}

TEST_F(CodeGeneratorTest, ExprStmtWithSideEffects)
{
    // x = 5 (as expression statement)
    ast::ExprStmt* stmt = ast::makeExprStmt(ast::makeAssignmentExpr(ast::makeName("x"), makeLiteral(5.0)));

    IR::Value result = codegen->eval(stmt);

    // Assignment returns the value
    EXPECT_DOUBLE_EQ(result.toFloat(), 5.0);

    // And defines the variable
    EXPECT_TRUE(env->exists(StringRef("x")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 5.0);
}

// BLOCK STATEMENT TESTS

TEST_F(CodeGeneratorTest, BlockStmtSingleStatement)
{
    // { x = 10 }
    ast::BlockStmt* block = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(10.0)) });

    IR::Value result = codegen->eval(block);

    // Returns list of values from statements
    EXPECT_TRUE(result.isNone()); // assignments return none with side effects

    // Variable should be defined
    EXPECT_TRUE(env->exists(StringRef("x")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 10.0);
}

TEST_F(CodeGeneratorTest, BlockStmtMultipleStatements)
{
    // { a = 1; b = 2; c = 3; }
    ast::BlockStmt* block = ast::makeBlock({ makeAssignmentStmt(ast::makeName("a"), makeLiteral(1.0)),
        makeAssignmentStmt(ast::makeName("b"), makeLiteral(2.0)),
        makeAssignmentStmt(ast::makeName("c"), makeLiteral(3.0)) });

    IR::Value result = codegen->eval(block);

    EXPECT_TRUE(result.isNone());

    EXPECT_DOUBLE_EQ(env->get(StringRef("a")).toFloat(), 1.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("b")).toFloat(), 2.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("c")).toFloat(), 3.0);
}

TEST_F(CodeGeneratorTest, BlockStmtWithExpressions)
{
    // { 10; 20; 30; }
    ast::BlockStmt* block = ast::makeBlock({ makeExprStmt(makeLiteral(10.0)),
        makeExprStmt(makeLiteral(20.0)),
        makeExprStmt(makeLiteral(30.0)) });

    IR::Value result = codegen->eval(block);

    EXPECT_TRUE(result.isNumber());
    EXPECT_EQ(result.toFloat(), 30);
}

TEST_F(CodeGeneratorTest, BlockStmtMixedStatements)
{
    // { x = 5; x + 10; y = x * 2; }
    ast::BlockStmt* block = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(5.0)),
        makeExprStmt(makeBinary(ast::makeName("x"), makeLiteral(10.0), tok::TokenType::OP_PLUS)),
        makeAssignmentStmt(ast::makeName("y"), makeBinary(ast::makeName("x"), makeLiteral(2.0), tok::TokenType::OP_STAR)) });

    codegen->eval(block);

    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 5.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("y")).toFloat(), 10.0);
}

// IF STATEMENT TESTS

TEST_F(CodeGeneratorTest, IfStmtTrueCondition)
{
    // if (true) { x = 10 }
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(10.0)) });
    ast::IfStmt* ifStmt = makeIf(makeLiteral(true), thenBlock);

    codegen->eval(ifStmt);

    EXPECT_TRUE(env->exists(StringRef("x")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 10.0);
}

TEST_F(CodeGeneratorTest, IfStmtFalseCondition_01)
{
    // if (false) { x = 10 }
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(10.0)) });
    ast::IfStmt* ifStmt = makeIf(makeLiteral(false), thenBlock);

    codegen->eval(ifStmt);

    // x should not be defined
    EXPECT_FALSE(env->exists(StringRef("x")));
}

TEST_F(CodeGeneratorTest, IfElseStmtTrueCondition)
{
    // if (true) { x = 10 } else { x = 20 }
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(10.0)) });
    ast::BlockStmt* elseBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(20.0)) });
    ast::IfStmt* ifStmt = makeIf(makeLiteral(true), thenBlock, elseBlock);

    codegen->eval(ifStmt);

    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 10.0);
}

TEST_F(CodeGeneratorTest, IfElseStmtFalseCondition)
{
    // if (false) { x = 10 } else { x = 20 }
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(10.0)) });
    ast::BlockStmt* elseBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(20.0)) });
    ast::IfStmt* ifStmt = makeIf(makeLiteral(false), thenBlock, elseBlock);

    codegen->eval(ifStmt);

    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 20.0);
}

TEST_F(CodeGeneratorTest, IfStmtComparisonCondition)
{
    // if (5 > 3) { result = 1 }
    ast::BinaryExpr* condition = makeBinary(makeLiteral(5.0), makeLiteral(3.0), tok::TokenType::OP_GT);
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("result"), makeLiteral(1.0)) });
    ast::IfStmt* ifStmt = makeIf(condition, thenBlock);

    codegen->eval(ifStmt);

    EXPECT_TRUE(env->exists(StringRef("result")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("result")).toFloat(), 1.0);
}

TEST_F(CodeGeneratorTest, IfStmtVariableCondition)
{
    // x = true; if (x) { y = 5 }
    env->define(StringRef("x"), IR::Value::makeBool(true));

    ast::BlockStmt* thenBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("y"), makeLiteral(5.0)) });
    ast::IfStmt* ifStmt = makeIf(ast::makeName("x"), thenBlock);

    codegen->eval(ifStmt);

    EXPECT_TRUE(env->exists(StringRef("y")));
}

TEST_F(CodeGeneratorTest, IfStmtNestedIf_01)
{
    // if (true) { if (true) { x = 10 } }
    ast::BlockStmt* innerThen = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(10.0)) });
    ast::IfStmt* innerIf = makeIf(makeLiteral(true), innerThen);

    ast::BlockStmt* outerThen = ast::makeBlock({ innerIf });
    ast::IfStmt* outerIf = makeIf(makeLiteral(true), outerThen);

    codegen->eval(outerIf);

    EXPECT_TRUE(env->exists(StringRef("x")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 10.0);
}

TEST_F(CodeGeneratorTest, IfStmtReturnValue)
{
    // if (true) { 42 } should return 42
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeExprStmt(makeLiteral(42.0)) });
    ast::IfStmt* ifStmt = makeIf(makeLiteral(true), thenBlock);

    IR::Value result = codegen->eval(ifStmt);

    // Result should be 42 from the block
    EXPECT_TRUE(result.isNumber());
    EXPECT_EQ(result.toInt(), 42);
}

// WHILE STATEMENT TESTS

TEST_F(CodeGeneratorTest, WhileStmtSimpleLoop)
{
    // i = 0; while (i < 3) { i = i + 1 }
    env->define(StringRef("i"), IR::Value::makeFloat(0.0));

    ast::BinaryExpr* condition = makeBinary(ast::makeName("i"), makeLiteral(3.0), tok::TokenType::OP_LT);
    ast::BlockStmt* body = ast::makeBlock({ makeAssignmentStmt(ast::makeName("i"), makeBinary(ast::makeName("i"), makeLiteral(1.0), tok::TokenType::OP_PLUS)) });

    ast::WhileStmt* whileStmt = makeWhile(condition, body);
    codegen->eval(whileStmt);

    EXPECT_DOUBLE_EQ(env->get(StringRef("i")).toFloat(), 3.0);
}

TEST_F(CodeGeneratorTest, WhileStmtFalseConditionNeverExecutes)
{
    // while (false) { x = 10 }
    ast::BlockStmt* body = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(10.0)) });
    ast::WhileStmt* whileStmt = makeWhile(makeLiteral(false), body);

    codegen->eval(whileStmt);

    EXPECT_FALSE(env->exists(StringRef("x")));
}

TEST_F(CodeGeneratorTest, WhileStmtCounterAccumulation)
{
    // sum = 0; i = 1; while (i <= 5) { sum = sum + i; i = i + 1 }
    env->define(StringRef("sum"), IR::Value::makeFloat(0.0));
    env->define(StringRef("i"), IR::Value::makeFloat(1.0));

    ast::BinaryExpr* condition = makeBinary(ast::makeName("i"), makeLiteral(5.0), tok::TokenType::OP_LTE);
    ast::BlockStmt* body = ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("sum"), ast::makeBinary(ast::makeName("sum"), ast::makeName("i"), tok::TokenType::OP_PLUS)),
        makeAssignmentStmt(ast::makeName("i"), makeBinary(ast::makeName("i"), makeLiteral(1.0), tok::TokenType::OP_PLUS)) });

    ast::WhileStmt* whileStmt = makeWhile(condition, body);
    codegen->eval(whileStmt);

    // sum should be 1+2+3+4+5 = 15
    EXPECT_DOUBLE_EQ(env->get(StringRef("sum")).toFloat(), 15.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("i")).toFloat(), 6.0);
}

TEST_F(CodeGeneratorTest, WhileStmtWithComplexCondition)
{
    // x = 10; while (x > 0 && x < 20) { x = x - 1 }
    env->define(StringRef("x"), IR::Value::makeFloat(10.0));

    ast::BinaryExpr* condition = makeBinary(ast::makeName("x"), makeLiteral(0.0), tok::TokenType::OP_GT);
    ast::BlockStmt* body = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), ast::makeBinary(ast::makeName("x"), makeLiteral(1.0), tok::TokenType::OP_MINUS)) });

    ast::WhileStmt* whileStmt = makeWhile(condition, body);
    codegen->eval(whileStmt);

    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 0.0);
}

TEST_F(CodeGeneratorTest, WhileStmtNestedLoop)
{
    // i = 0; 
    // while (i < 3) 
    //      j = 0
    //      while (j < 2) 
    //          j = j + 1  
    //      i = i + 1 

    eval("test_while_stmt_nested_loop.txt");

    EXPECT_DOUBLE_EQ(env->get(StringRef("ا")).toFloat(), 3.0);
    EXPECT_DOUBLE_EQ(env->get(StringRef("م")).toFloat(), 2.0);
}

// FUNCTION DEFINITION STATEMENT TESTS

TEST_F(CodeGeneratorTest, FuncDefSimpleNoParams)
{
    // function getValue() { return 42 }
    /// NOTE: Since return is not fully implemented, we use expression
    ast::BlockStmt* body = ast::makeBlock({ makeExprStmt(makeLiteral(42.0)) });

    // Create parameter list (empty)
    std::vector<ast::Expr*> params = {};

    ast::FunctionDef* funcDef = ast::makeFunction(
        ast::makeName("getValue"),
        ast::makeList(params),
        body);

    codegen->eval(funcDef);

    // Function should be defined in environment
    EXPECT_TRUE(env->exists(StringRef("getValue")));
    IR::Value funcValue = env->get(StringRef("getValue"));
    EXPECT_TRUE(funcValue.isFunction());
}

TEST_F(CodeGeneratorTest, FuncDefWithParameters)
{
    // function add(a, b) { return a + b }

    eval("test_func_def_with_parameters.txt");

    EXPECT_TRUE(env->exists(StringRef("جمع")));
}

TEST_F(CodeGeneratorTest, FuncDefAndCall)
{
    // function double(x) { return x * 2 }
    // double(5)

    IR::Value result = eval("test_func_def_and_call.txt");

    EXPECT_TRUE(result.isNumber()); // Body returns block result
    EXPECT_EQ(result.toInt(), 10);
}

TEST_F(CodeGeneratorTest, FuncDefWithSideEffects)
{
    // function setX() { x = 100 }
    ast::AssignmentStmt* x = ast::makeAssignmentStmt(ast::makeName("x"), makeLiteral(0.0));
    codegen->eval(x); // declare x

    ast::BlockStmt* body = ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("x"), makeLiteral(100.0)) });

    std::vector<ast::Expr*> params = {};

    ast::FunctionDef* funcDef = ast::makeFunction(ast::makeName("setX"), ast::makeList(params), body);

    codegen->eval(funcDef); // define function

    // Call it
    ast::CallExpr* call = makeCall("setX", {});
    codegen->eval(call); // set x to 100

    // x should now be defined
    EXPECT_TRUE(env->exists(StringRef("x")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("x")).toFloat(), 100.0);
}

TEST_F(CodeGeneratorTest, FuncDefClosure)
{
    // outer = 10
    // function getOuter() { return outer }
    env->define(StringRef("outer"), IR::Value::makeFloat(10.0));

    ast::BlockStmt* body = ast::makeBlock({ ast::makeExprStmt(ast::makeName("outer")) });

    std::vector<ast::Expr*> params = {};

    ast::FunctionDef* funcDef = ast::makeFunction(
        ast::makeName("getOuter"),
        ast::makeList(params),
        body);

    codegen->eval(funcDef);

    // Function should capture the environment
    IR::Value funcValue = env->get(StringRef("getOuter"));
    EXPECT_TRUE(funcValue.isFunction());
}

TEST_F(CodeGeneratorTest, FuncDefMultipleFunctions)
{
    // function f1() { return 1 }
    // function f2() { return 2 }
    ast::BlockStmt* body1 = ast::makeBlock({ makeExprStmt(makeLiteral(1.0)) });
    ast::BlockStmt* body2 = ast::makeBlock({ makeExprStmt(makeLiteral(2.0)) });

    ast::FunctionDef* func1 = ast::makeFunction(ast::makeName("f1"), {}, body1);
    ast::FunctionDef* func2 = ast::makeFunction(ast::makeName("f2"), {}, body2);

    codegen->eval(func1);
    codegen->eval(func2);

    EXPECT_TRUE(env->exists(StringRef("f1")));
    EXPECT_TRUE(env->exists(StringRef("f2")));
}

// INTEGRATED STATEMENT TESTS

TEST_F(CodeGeneratorTest, IntegratedIfWithinWhile)
{
    // sum = 0
    // i = 0
    // while (i < 10)
    //   if (i % 2 == 0)
    //     sum = sum + i
    //   i = i + 1

    eval("test_integrated_if_within_while.txt");

    // sum should be 0+2+4+6+8 = 20
    EXPECT_DOUBLE_EQ(env->get(StringRef("جمع")).toFloat(), 20.0);
}

TEST_F(CodeGeneratorTest, IntegratedFunctionWithIfElse)
{
    // function abs(x)
    //   if (x < 0)
    //      return -x
    //   else
    //      return x

    ast::BinaryExpr* condition = makeBinary(ast::makeName("x"), makeLiteral(0.0), tok::TokenType::OP_LT);
    ast::BlockStmt* thenBlock = ast::makeBlock({ ast::makeExprStmt(ast::makeUnary(ast::makeName("x"), tok::TokenType::OP_MINUS)) });
    ast::BlockStmt* elseBlock = ast::makeBlock({ ast::makeExprStmt(ast::makeName("x")) });

    ast::IfStmt* ifStmt = makeIf(condition, thenBlock, elseBlock);
    ast::BlockStmt* funcBody = ast::makeBlock({ ifStmt });

    std::vector<ast::Expr*> params = { ast::makeName("x") };
    ast::FunctionDef* funcDef = ast::makeFunction(ast::makeName("abs"), ast::makeList(params), funcBody);

    codegen->eval(funcDef);

    // Test with negative number
    ast::CallExpr* call1 = makeCall("abs", { makeLiteral(-5.0) });
    IR::Value result1 = codegen->eval(call1);
    EXPECT_TRUE(result1.isNumber()); // Returns block result
    EXPECT_EQ(result1.toInt(), 5);

    // Test with positive number
    ast::CallExpr* call2 = makeCall("abs", { makeLiteral(5.0) });
    IR::Value result2 = codegen->eval(call2);
    EXPECT_TRUE(result2.isNumber());
    EXPECT_EQ(result2.toInt(), 5);
}

TEST_F(CodeGeneratorTest, IntegratedBlockWithMultipleStatementTypes)
{
    //   x = 10
    //   y = 20
    //   if (x < y)
    //      result = 1
    //   else
    //      result = 0
    //   result
    ast::BinaryExpr* condition = makeBinary(ast::makeName("x"), ast::makeName("y"), tok::TokenType::OP_LT);
    ast::BlockStmt* thenBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("result"), makeLiteral(1.0)) });
    ast::BlockStmt* elseBlock = ast::makeBlock({ makeAssignmentStmt(ast::makeName("result"), makeLiteral(0.0)) });
    ast::IfStmt* ifStmt = makeIf(condition, thenBlock, elseBlock);

    ast::BlockStmt* block = ast::makeBlock({ makeAssignmentStmt(ast::makeName("x"), makeLiteral(10.0)),
        makeAssignmentStmt(ast::makeName("y"), makeLiteral(20.0)), ifStmt, ast::makeExprStmt(ast::makeName("result")) });

    codegen->eval(block);

    EXPECT_TRUE(env->exists(StringRef("result")));
    EXPECT_DOUBLE_EQ(env->get(StringRef("result")).toFloat(), 1.0);
}
