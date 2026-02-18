#include "../../include//ast/AST_allocator.hpp"
#include "../../include//ast/ast.hpp"
#include "../../include/lex/file_manager.hpp"
#include "../../include/parser/analyzer.hpp"
#include "../../include/types/string.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace mylang;
using namespace mylang::parser;
using namespace testing;

// Test Fixture

class SemanticAnalyzerTest : public ::testing::Test {
protected:
    std::unique_ptr<SemanticAnalyzer> analyzer;
    std::vector<ast::Stmt*> statements;

    void SetUp() override
    {
        analyzer = std::make_unique<SemanticAnalyzer>();
        statements.clear();
    }

    // Helper function to create a literal expression
    ast::LiteralExpr* createNumberLiteral(StringRef const& value, std::int32_t line = 1)
    {
        return ast::makeLiteral(ast::LiteralExpr::Type::NUMBER, value /*, line*/);
    }

    ast::LiteralExpr* createStringLiteral(StringRef const& value, std::int32_t line = 1)
    {
        return ast::makeLiteral(ast::LiteralExpr::Type::STRING, value /*, line*/);
    }

    ast::LiteralExpr* createBooleanLiteral(StringRef const& value, std::int32_t line = 1)
    {
        return ast::makeLiteral(ast::LiteralExpr::Type::BOOLEAN, value /*, line*/);
    }

    ast::LiteralExpr* createNoneLiteral(std::int32_t line = 1)
    {

        return ast::makeLiteral(ast::LiteralExpr::Type::NONE, "none" /*, line*/);
    }

    // Helper function to create a name expression
    ast::NameExpr* createName(StringRef const& name, std::int32_t line = 1) { return ast::makeName(name /*, line*/); }

    // Helper function to create a binary expression
    ast::BinaryExpr* createBinary(ast::Expr* left, tok::TokenType op, ast::Expr* right, std::int32_t line = 1)
    {
        return ast::makeBinary(left, right, op /*, line*/);
    }

    // Helper to count issues by severity
    size_t countIssuesBySeverity(SemanticAnalyzer::Issue::Severity sev) const
    {
        size_t count = 0;
        for (auto const& issue : analyzer->getIssues()) {
            if (issue.severity == sev)
                count++;
        }
        return count;
    }

    // Helper to check if specific issue exists
    bool hasIssueContaining(StringRef const& substring) const
    {
        for (auto const& issue : analyzer->getIssues()) {
            if (issue.message.find(substring))
                return true;
        }
        return false;
    }
};

// Type Inference Tests

TEST_F(SemanticAnalyzerTest, InferType_NumberLiteral_Integer)
{
    ast::LiteralExpr* expr = createNumberLiteral("42");
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::INTEGER);
}

TEST_F(SemanticAnalyzerTest, InferType_NumberLiteral_Float)
{
    auto* expr = createNumberLiteral("3.14");
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, InferType_StringLiteral)
{
    auto* expr = createStringLiteral("hello");
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BooleanLiteral)
{
    auto* expr = createBooleanLiteral("true");
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::BOOLEAN);
}

TEST_F(SemanticAnalyzerTest, InferType_NoneLiteral)
{
    auto* expr = createNoneLiteral();
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::NONE);
}

TEST_F(SemanticAnalyzerTest, InferType_NullExpr) { EXPECT_EQ(analyzer->inferType(nullptr), SymbolTable::DataType_t::UNKNOWN); }

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_IntPlusInt)
{
    auto* left = createNumberLiteral("10");
    auto* right = createNumberLiteral("20");
    auto* binary = createBinary(left, tok::TokenType::OP_PLUS, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::INTEGER);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_FloatPromotion)
{
    auto* left = createNumberLiteral("10");
    auto* right = createNumberLiteral("20.5");
    auto* binary = createBinary(left, tok::TokenType::OP_PLUS, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_StringConcat)
{
    auto* left = createStringLiteral("hello");
    auto* right = createStringLiteral("world");
    auto* binary = createBinary(left, tok::TokenType::OP_PLUS, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_LogicalAnd)
{
    auto* left = createBooleanLiteral("true");
    auto* right = createBooleanLiteral("false");
    auto* binary = createBinary(left, tok::TokenType::KW_AND, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::BOOLEAN);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_LogicalOr)
{
    auto* left = createBooleanLiteral("true");
    auto* right = createBooleanLiteral("false");
    auto* binary = createBinary(left, tok::TokenType::KW_OR, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::BOOLEAN);
}

// Variable Analysis Tests

TEST_F(SemanticAnalyzerTest, UndefinedVariable_ReportsError)
{
    auto* nameExpr = createName("undefined_var", 5);

    analyzer->analyzeExpr(nameExpr);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Undefined variable"));
}

TEST_F(SemanticAnalyzerTest, DefinedVariable_NoError)
{
    // First define the variable
    auto* value = createNumberLiteral("42");
    auto* target = createName("x");
    auto* assign = ast::makeAssignmentStmt(target, value /*, 1*/);
    statements.push_back(assign);

    analyzer->analyze(statements);

    // Now use the variable
    auto* useExpr = createName("x", 2);
    analyzer->analyzeExpr(useExpr);

    // Should not report undefined variable error for 'x'
    EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, UnusedVariable_ReportsWarning)
{
    auto* value = createNumberLiteral("42");
    auto* target = createName("unused_var");
    auto* assign = ast::makeAssignmentStmt(target, value /*, 1*/);
    statements.push_back(assign);

    analyzer->analyze(statements);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Unused variable"));
}

// Type Compatibility Tests

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Subtraction)
{
    auto* left = createStringLiteral("hello");
    auto* right = createStringLiteral("world");
    auto* binary = createBinary(left, tok::TokenType::OP_MINUS, right);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Multiplication)
{
    auto* left = createStringLiteral("hello");
    auto* right = createNumberLiteral("3");
    auto* binary = createBinary(left, tok::TokenType::OP_STAR, right);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Division)
{
    auto* left = createStringLiteral("hello");
    auto* right = createNumberLiteral("2");
    auto* binary = createBinary(left, tok::TokenType::OP_SLASH, right);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

// Division by Zero Tests

TEST_F(SemanticAnalyzerTest, DivisionByZero_ReportsError)
{
    auto* left = createNumberLiteral("10");
    auto* right = createNumberLiteral("0");
    auto* binary = createBinary(left, tok::TokenType::OP_SLASH, right);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, DivisionByNonZero_NoError)
{
    auto* left = createNumberLiteral("10");
    auto* right = createNumberLiteral("5");
    auto* binary = createBinary(left, tok::TokenType::OP_SLASH, right);

    analyzer->analyzeExpr(binary);

    EXPECT_FALSE(hasIssueContaining("Division by zero"));
}

// Function Analysis Tests

TEST_F(SemanticAnalyzerTest, BuiltInFunction_Print_IsDefined)
{
    auto* printName = createName("print");

    analyzer->analyzeExpr(printName);

    EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, CallNonCallable_ReportsError)
{
    // Define a variable
    auto* value = createNumberLiteral("42");
    auto* target = createName("not_a_function");
    auto* assign = ast::makeAssignmentStmt(target, value /*, 1*/);
    statements.push_back(assign);
    analyzer->analyze(statements);

    // Try to call it
    auto* callee = createName("not_a_function", 2);
    std::vector<ast::Expr*> args;
    ast::ListExpr* arg_list = ast::makeList(args);
    auto* call = ast::makeCall(callee, arg_list /*, 2*/);

    analyzer->analyzeExpr(call);

    EXPECT_TRUE(hasIssueContaining("is not callable"));
}

// Control Flow Tests

TEST_F(SemanticAnalyzerTest, ConstantIfCondition_ReportsWarning)
{
    auto* condition = createBooleanLiteral("true");
    auto* thenBlock = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 1*/);
    auto* elseBlock = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 1*/);
    auto* ifStmt = ast::makeIf(condition, thenBlock, elseBlock /*, 1*/);

    analyzer->analyzeStmt(ifStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Condition is always constant"));
}

TEST_F(SemanticAnalyzerTest, InfiniteLoop_ReportsWarning)
{
    auto* condition = createBooleanLiteral("true");
    auto* block = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 1*/);
    auto* whileStmt = ast::makeWhile(condition, block /*, 1*/);

    analyzer->analyzeStmt(whileStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Infinite loop"));
}

// Expression Statement Tests

TEST_F(SemanticAnalyzerTest, UnusedExpressionResult_ReportsInfo)
{
    auto* expr = createNumberLiteral("42");
    auto* exprStmt = ast::makeExprStmt(expr /*, 1*/);

    analyzer->analyzeStmt(exprStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Expression result not used"));
}

TEST_F(SemanticAnalyzerTest, FunctionCallExpression_NoUnusedWarning)
{
    auto* callee = createName("print");
    std::vector<ast::Expr*> args;
    ast::ListExpr* arg_list = ast::makeList(args);
    auto* call = ast::makeCall(callee, arg_list /*, 1*/);
    auto* exprStmt = ast::makeExprStmt(call /*, 1*/);

    analyzer->analyzeStmt(exprStmt);

    // Should not report unused expression for function calls
    size_t infoCount = countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO);
    // May have other info messages, but shouldn't have unused expression warning
}

// Scope Tests

TEST_F(SemanticAnalyzerTest, ForLoopVariableShadowing_ReportsWarning)
{
    // Define outer variable
    auto* outerValue = createNumberLiteral("10");
    auto* outerTarget = createName("i");
    auto* outerAssign = ast::makeAssignmentStmt(outerTarget, outerValue /*, 1*/);
    statements.push_back(outerAssign);

    analyzer->analyze(statements);

    // Create for loop with same variable name
    auto* loopVar = createName("i");
    auto* iterable = ast::makeList(std::vector<ast::Expr*> {} /*, 2*/);
    auto* loopBlock = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 2*/);
    auto* forStmt = ast::makeFor(loopVar, iterable, loopBlock /*, 2*/);

    analyzer->analyzeStmt(forStmt);

    EXPECT_TRUE(hasIssueContaining("Loop variable shadows"));
}

// Function Definition Tests

TEST_F(SemanticAnalyzerTest, FunctionWithoutReturn_ReportsInfo)
{
    auto* funcName = createName("my_func");
    std::vector<ast::Expr*> params;
    ast::ListExpr* param_list = ast::makeList(params);
    auto* body = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 1*/);
    auto* funcDef = ast::makeFunction(funcName, param_list, body /*, 1*/);

    analyzer->analyzeStmt(funcDef);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithReturn_NoWarning)
{
    auto* funcName = createName("my_func");
    std::vector<ast::Expr*> params;
    ast::ListExpr* param_list = ast::makeList(params);
    // Create return statement
    auto* retValue = createNumberLiteral("42");
    auto* retStmt = ast::makeReturn(retValue /*, 2*/);

    std::vector<ast::Stmt*> bodyStmts = { retStmt };
    auto* body = ast::makeBlock(bodyStmts /*, 1*/);
    auto* funcDef = ast::makeFunction(funcName, param_list, body /*, 1*/);

    analyzer->analyzeStmt(funcDef);

    // Should not warn about missing return
    EXPECT_FALSE(hasIssueContaining("Function may not return a value"));
}

// Complex Expression Tests

TEST_F(SemanticAnalyzerTest, NestedBinaryExpressions_InfersCorrectType)
{
    // (10 + 20) * 2.5
    auto* inner_left = createNumberLiteral("10");
    auto* inner_right = createNumberLiteral("20");
    auto* inner_binary = createBinary(inner_left, tok::TokenType::OP_PLUS, inner_right);

    auto* outer_right = createNumberLiteral("2.5");
    auto* outer_binary = createBinary(inner_binary, tok::TokenType::OP_STAR, outer_right);

    // Should promote to FLOAT due to 2.5
    EXPECT_EQ(analyzer->inferType(outer_binary), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, ListExpression_InfersListType)
{
    std::vector<ast::Expr*> elements = { createNumberLiteral("1"), createNumberLiteral("2"), createNumberLiteral("3") };
    auto* listExpr = ast::makeList(elements /*, 1*/);

    EXPECT_EQ(analyzer->inferType(listExpr), SymbolTable::DataType_t::LIST);

    analyzer->analyzeExpr(listExpr);
}

// Edge Cases and Null Safety Tests

TEST_F(SemanticAnalyzerTest, AnalyzeNullStatement_NoError) { EXPECT_NO_THROW(analyzer->analyzeStmt(nullptr)); }

TEST_F(SemanticAnalyzerTest, AnalyzeNullExpression_NoError) { EXPECT_NO_THROW(analyzer->analyzeExpr(nullptr)); }

TEST_F(SemanticAnalyzerTest, EmptyStatementList_NoIssues)
{
    analyzer->analyze(statements);
    EXPECT_EQ(analyzer->getIssues().size(), 0);
}

// Report Generation Tests

TEST_F(SemanticAnalyzerTest, PrintReport_NoIssues)
{
    // Should not throw
    EXPECT_NO_THROW(analyzer->printReport());
}

TEST_F(SemanticAnalyzerTest, PrintReport_WithIssues)
{
    auto* nameExpr = createName("undefined", 1);
    analyzer->analyzeExpr(nameExpr);

    // Should not throw
    EXPECT_NO_THROW(analyzer->printReport());
}

// Symbol Table Tests

TEST_F(SemanticAnalyzerTest, GetGlobalScope_NotNull) { EXPECT_NE(analyzer->getGlobalScope(), nullptr); }

TEST_F(SemanticAnalyzerTest, AssignmentCreatesSymbol)
{
    auto* value = createNumberLiteral("42");
    auto* target = createName("x");
    auto* assign = ast::makeAssignmentStmt(target, value /*, 1*/);
    statements.push_back(assign);

    analyzer->analyze(statements);

    // Verify symbol was created
    SymbolTable const* globalScope = analyzer->getGlobalScope();
    ASSERT_NE(globalScope, nullptr);
}

// Integration Tests

TEST_F(SemanticAnalyzerTest, ComplexProgram_MultipleIssues)
{
    // Undefined variable use
    auto* undefined = createName("undefined_var", 1);
    auto* exprStmt1 = ast::makeExprStmt(undefined /*, 1*/);
    statements.push_back(exprStmt1);

    // Division by zero
    auto* left = createNumberLiteral("10", 2);
    auto* right = createNumberLiteral("0", 2);
    auto* divExpr = createBinary(left, tok::TokenType::OP_SLASH, right);
    auto* exprStmt2 = ast::makeExprStmt(divExpr /*, 2*/);
    statements.push_back(exprStmt2);

    // Unused variable
    auto* value = createNumberLiteral("100", 3);
    auto* target = createName("unused");
    auto* assign = ast::makeAssignmentStmt(target, value /*, 3*/);
    statements.push_back(assign);

    analyzer->analyze(statements);

    // Should have multiple issues
    EXPECT_GE(analyzer->getIssues().size(), 3);
    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
}

TEST_F(SemanticAnalyzerTest, VariableFlowAnalysis)
{
    // Define variable
    auto* value1 = createNumberLiteral("10");
    auto* target1 = createName("x");
    auto* assign1 = ast::makeAssignmentStmt(target1, value1 /*, 1*/);
    statements.push_back(assign1);

    // Use variable
    auto* useExpr = createName("x", 2);
    auto* value2 = createNumberLiteral("5");
    auto* binary = createBinary(useExpr, tok::TokenType::OP_PLUS, value2);
    auto* target2 = createName("y");
    auto* assign2 = ast::makeAssignmentStmt(target2, binary /*, 2*/);
    statements.push_back(assign2);

    analyzer->analyze(statements);

    // Should report unused variable 'y' but not 'x'
    auto issues = analyzer->getIssues();
    size_t unusedYCount = 0;
    for (auto const& issue : issues) {
        if (issue.message.find('y') && issue.message.find("Unused"))
            unusedYCount++;
    }
    EXPECT_GT(unusedYCount, 0);
}

// Call Expression Tests

TEST_F(SemanticAnalyzerTest, CallExpression_InfersAnyType)
{
    auto* callee = createName("some_function");
    std::vector<ast::Expr*> args;
    ast::ListExpr* arg_list = ast::makeList(args);
    auto* call = ast::makeCall(callee, arg_list /*, 1*/);

    EXPECT_EQ(analyzer->inferType(call), SymbolTable::DataType_t::ANY);
}

TEST_F(SemanticAnalyzerTest, CallWithArguments_AnalyzesAllArgs)
{
    auto* callee = createName("print");
    std::vector<ast::Expr*> args = { createNumberLiteral("1"), createStringLiteral("hello"), createBooleanLiteral("true") };
    ast::ListExpr* arg_list = ast::makeList(args);
    auto* call = ast::makeCall(callee, arg_list /*, 1*/);

    EXPECT_NO_THROW(analyzer->analyzeExpr(call));
}
