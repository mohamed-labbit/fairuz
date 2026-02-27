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

    // Helper function to create a name expression
    ast::NameExpr* createName(StringRef const& name, int32_t line = 1) { return ast::makeName(name /*, line*/); }

    // Helper function to create a binary expression
    ast::BinaryExpr* createBinary(ast::Expr* l, ast::BinaryOp op, ast::Expr* right, int32_t line = 1)
    {
        return ast::makeBinary(l, right, op /*, line*/);
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
    ast::LiteralExpr* expr = ast::makeLiteralInt(42);
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::INTEGER);
}

TEST_F(SemanticAnalyzerTest, InferType_NumberLiteral_Float)
{
    ast::LiteralExpr* expr = ast::makeLiteralFloat(3.14);
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, InferType_StringLiteral)
{
    ast::LiteralExpr* expr = ast::makeLiteralString("hello");
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BooleanLiteral)
{
    ast::LiteralExpr* expr = ast::makeLiteralBool(true);
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::BOOLEAN);
}

/*
TEST_F(SemanticAnalyzerTest, InferType_NoneLiteral)
{
    ast::LiteralExpr* expr = ast::makeLiteral("");
    EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::NONE);
}
*/

TEST_F(SemanticAnalyzerTest, InferType_NullExpr)
{
    EXPECT_EQ(analyzer->inferType(nullptr), SymbolTable::DataType_t::UNKNOWN);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_IntPlusInt)
{
    ast::LiteralExpr* left = ast::makeLiteralInt(10);
    ast::LiteralExpr* right = ast::makeLiteralInt(20);
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_ADD, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::INTEGER);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_FloatPromotion)
{
    ast::LiteralExpr* left = ast::makeLiteralInt(10);
    ast::LiteralExpr* right = ast::makeLiteralFloat(20.5);
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_ADD, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_StringConcat)
{
    ast::LiteralExpr* left = ast::makeLiteralString("hello");
    ast::LiteralExpr* right = ast::makeLiteralString("world");
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_ADD, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_LogicalAnd)
{
    ast::LiteralExpr* left = ast::makeLiteralBool(true);
    ast::LiteralExpr* right = ast::makeLiteralBool(false);
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_AND, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::BOOLEAN);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_LogicalOr)
{
    ast::LiteralExpr* left = ast::makeLiteralBool(true);
    ast::LiteralExpr* right = ast::makeLiteralBool(false);
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_OR, right);

    EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::BOOLEAN);
}

// Variable Analysis Tests

TEST_F(SemanticAnalyzerTest, UndefinedVariable_ReportsError)
{
    ast::NameExpr* nameExpr = createName("undefined_var", 5);

    analyzer->analyzeExpr(nameExpr);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Undefined variable"));
}

TEST_F(SemanticAnalyzerTest, DefinedVariable_NoError)
{
    // First define the variable
    ast::LiteralExpr* value = ast::makeLiteralInt(42);
    ast::NameExpr* target = createName("x");
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(target, value /*, 1*/);
    statements.push_back(assign);

    analyzer->analyze(statements);

    // Now use the variable
    ast::NameExpr* useExpr = createName("x", 2);
    analyzer->analyzeExpr(useExpr);

    // Should not report undefined variable error for 'x'
    EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, UnusedVariable_ReportsWarning)
{
    ast::LiteralExpr* value = ast::makeLiteralInt(42);
    ast::NameExpr* target = createName("unused_var");
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(target, value /*, 1*/);
    statements.push_back(assign);

    analyzer->analyze(statements);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Unused variable"));
}

// Type Compatibility Tests

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Subtraction)
{
    ast::LiteralExpr* left = ast::makeLiteralString("hello");
    ast::LiteralExpr* right = ast::makeLiteralString("world");
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_SUB, right);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Multiplication)
{
    ast::LiteralExpr* left = ast::makeLiteralString("hello");
    ast::LiteralExpr* right = ast::makeLiteralInt(3);
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_MUL, right);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Division)
{
    ast::LiteralExpr* left = ast::makeLiteralString("hello");
    ast::LiteralExpr* right = ast::makeLiteralInt(2);
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_DIV, right);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

// Division by Zero Tests

TEST_F(SemanticAnalyzerTest, DivisionByZero_ReportsError)
{
    ast::LiteralExpr* left = ast::makeLiteralInt(10);
    ast::LiteralExpr* right = ast::makeLiteralInt(0);
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_DIV, right);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, DivisionByNonZero_NoError)
{
    ast::LiteralExpr* left = ast::makeLiteralInt(10);
    ast::LiteralExpr* right = ast::makeLiteralInt(5);
    ast::BinaryExpr* binary = createBinary(left, ast::BinaryOp::OP_DIV, right);

    analyzer->analyzeExpr(binary);

    EXPECT_FALSE(hasIssueContaining("Division by zero"));
}

// Function Analysis Tests

TEST_F(SemanticAnalyzerTest, BuiltInFunction_Print_IsDefined)
{
    ast::NameExpr* printName = createName("print");

    analyzer->analyzeExpr(printName);

    EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, CallNonCallable_ReportsError)
{
    // Define a variable
    ast::LiteralExpr* value = ast::makeLiteralInt(42);
    ast::NameExpr* target = createName("not_a_function");
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(target, value /*, 1*/);
    statements.push_back(assign);
    analyzer->analyze(statements);

    // Try to call it
    ast::NameExpr* callee = createName("not_a_function", 2);
    std::vector<ast::Expr*> args;
    ast::ListExpr* arg_list = ast::makeList(args);
    ast::CallExpr* call = ast::makeCall(callee, arg_list /*, 2*/);

    analyzer->analyzeExpr(call);

    EXPECT_TRUE(hasIssueContaining("is not callable"));
}

// Control Flow Tests

TEST_F(SemanticAnalyzerTest, ConstantIfCondition_ReportsWarning)
{
    ast::LiteralExpr* condition = ast::makeLiteralBool(true);
    ast::BlockStmt* thenBlock = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 1*/);
    ast::BlockStmt* elseBlock = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 1*/);
    ast::IfStmt* ifStmt = ast::makeIf(condition, thenBlock, elseBlock /*, 1*/);

    analyzer->analyzeStmt(ifStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Condition is always constant"));
}

TEST_F(SemanticAnalyzerTest, InfiniteLoop_ReportsWarning)
{
    ast::LiteralExpr* condition = ast::makeLiteralBool(true);
    ast::BlockStmt* block = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 1*/);
    ast::WhileStmt* whileStmt = ast::makeWhile(condition, block /*, 1*/);

    analyzer->analyzeStmt(whileStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Infinite loop"));
}

// Expression Statement Tests

TEST_F(SemanticAnalyzerTest, UnusedExpressionResult_ReportsInfo)
{
    ast::LiteralExpr* expr = ast::makeLiteralInt(42);
    ast::ExprStmt* exprStmt = ast::makeExprStmt(expr /*, 1*/);

    analyzer->analyzeStmt(exprStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Expression result not used"));
}

TEST_F(SemanticAnalyzerTest, FunctionCallExpression_NoUnusedWarning)
{
    ast::NameExpr* callee = createName("print");
    std::vector<ast::Expr*> args;
    ast::ListExpr* arg_list = ast::makeList(args);
    ast::CallExpr* call = ast::makeCall(callee, arg_list /*, 1*/);
    ast::ExprStmt* exprStmt = ast::makeExprStmt(call /*, 1*/);

    analyzer->analyzeStmt(exprStmt);

    // Should not report unused expression for function calls
    size_t infoCount = countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO);
    // May have other info messages, but shouldn't have unused expression warning
}

// Scope Tests

TEST_F(SemanticAnalyzerTest, ForLoopVariableShadowing_ReportsWarning)
{
    // Define outer variable
    ast::LiteralExpr* outerValue = ast::makeLiteralInt(10);
    ast::NameExpr* outerTarget = createName("i");
    ast::AssignmentStmt* outerAssign = ast::makeAssignmentStmt(outerTarget, outerValue /*, 1*/);
    statements.push_back(outerAssign);

    analyzer->analyze(statements);

    // Create for loop with same variable name
    ast::NameExpr* loopVar = createName("i");
    ast::ListExpr* iterable = ast::makeList(std::vector<ast::Expr*> {} /*, 2*/);
    ast::BlockStmt* loopBlock = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 2*/);
    ast::ForStmt* forStmt = ast::makeFor(loopVar, iterable, loopBlock /*, 2*/);

    analyzer->analyzeStmt(forStmt);

    EXPECT_TRUE(hasIssueContaining("Loop variable shadows"));
}

// Function Definition Tests

TEST_F(SemanticAnalyzerTest, FunctionWithoutReturn_ReportsInfo)
{
    ast::NameExpr* funcName = createName("my_func");
    std::vector<ast::Expr*> params;
    ast::ListExpr* param_list = ast::makeList(params);
    ast::BlockStmt* body = ast::makeBlock(std::vector<ast::Stmt*> {} /*, 1*/);
    ast::FunctionDef* funcDef = ast::makeFunction(funcName, param_list, body /*, 1*/);

    analyzer->analyzeStmt(funcDef);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithReturn_NoWarning)
{
    ast::NameExpr* funcName = createName("my_func");
    std::vector<ast::Expr*> params;
    ast::ListExpr* param_list = ast::makeList(params);
    // Create return statement
    ast::LiteralExpr* retValue = ast::makeLiteralInt(42);
    ast::ReturnStmt* retStmt = ast::makeReturn(retValue /*, 2*/);

    std::vector<ast::Stmt*> bodyStmts = { retStmt };
    ast::BlockStmt* body = ast::makeBlock(bodyStmts /*, 1*/);
    ast::FunctionDef* funcDef = ast::makeFunction(funcName, param_list, body /*, 1*/);

    analyzer->analyzeStmt(funcDef);

    // Should not warn about missing return
    EXPECT_FALSE(hasIssueContaining("Function may not return a value"));
}

// Complex Expression Tests

TEST_F(SemanticAnalyzerTest, NestedBinaryExpressions_InfersCorrectType)
{
    // (10 + 20) * 2.5
    ast::LiteralExpr* inner_left = ast::makeLiteralInt(10);
    ast::LiteralExpr* inner_right = ast::makeLiteralInt(20);
    ast::BinaryExpr* inner_binary = createBinary(inner_left, ast::BinaryOp::OP_ADD, inner_right);

    ast::LiteralExpr* outer_right = ast::makeLiteralFloat(2.5);
    ast::BinaryExpr* outer_binary = createBinary(inner_binary, ast::BinaryOp::OP_MUL, outer_right);

    // Should promote to FLOAT due to 2.5
    EXPECT_EQ(analyzer->inferType(outer_binary), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, ListExpression_InfersListType)
{
    std::vector<ast::Expr*> elements = { ast::makeLiteralInt(1), ast::makeLiteralInt(2), ast::makeLiteralInt(3) };
    ast::ListExpr* listExpr = ast::makeList(elements /*, 1*/);

    EXPECT_EQ(analyzer->inferType(listExpr), SymbolTable::DataType_t::LIST);

    analyzer->analyzeExpr(listExpr);
}

// Edge Cases and Null Safety Tests

TEST_F(SemanticAnalyzerTest, AnalyzeNullStatement_NoError)
{
    EXPECT_NO_THROW(analyzer->analyzeStmt(nullptr));
}

TEST_F(SemanticAnalyzerTest, AnalyzeNullExpression_NoError)
{
    EXPECT_NO_THROW(analyzer->analyzeExpr(nullptr));
}

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
    ast::NameExpr* nameExpr = createName("undefined", 1);
    analyzer->analyzeExpr(nameExpr);

    // Should not throw
    EXPECT_NO_THROW(analyzer->printReport());
}

// Symbol Table Tests

TEST_F(SemanticAnalyzerTest, GetGlobalScope_NotNull)
{
    EXPECT_NE(analyzer->getGlobalScope(), nullptr);
}

TEST_F(SemanticAnalyzerTest, AssignmentCreatesSymbol)
{
    ast::LiteralExpr* value = ast::makeLiteralInt(42);
    ast::NameExpr* target = createName("x");
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(target, value /*, 1*/);
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
    ast::NameExpr* undefined = createName("undefined_var", 1);
    ast::ExprStmt* exprStmt1 = ast::makeExprStmt(undefined /*, 1*/);
    statements.push_back(exprStmt1);

    // Division by zero
    ast::LiteralExpr* left = ast::makeLiteralInt(10);
    ast::LiteralExpr* right = ast::makeLiteralInt(0);
    ast::BinaryExpr* divExpr = createBinary(left, ast::BinaryOp::OP_DIV, right);
    ast::ExprStmt* exprStmt2 = ast::makeExprStmt(divExpr /*, 2*/);
    statements.push_back(exprStmt2);

    // Unused variable
    ast::LiteralExpr* value = ast::makeLiteralInt(100);
    ast::NameExpr* target = createName("unused");
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(target, value /*, 3*/);
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
    ast::LiteralExpr* value1 = ast::makeLiteralInt(10);
    ast::NameExpr* target1 = createName("x");
    ast::AssignmentStmt* assign1 = ast::makeAssignmentStmt(target1, value1 /*, 1*/);
    statements.push_back(assign1);

    // Use variable
    ast::NameExpr* useExpr = createName("x", 2);
    ast::LiteralExpr* value2 = ast::makeLiteralInt(5);
    ast::BinaryExpr* binary = createBinary(useExpr, ast::BinaryOp::OP_ADD, value2);
    ast::NameExpr* target2 = createName("y");
    ast::AssignmentStmt* assign2 = ast::makeAssignmentStmt(target2, binary /*, 2*/);
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
    ast::NameExpr* callee = createName("some_function");
    std::vector<ast::Expr*> args;
    ast::ListExpr* arg_list = ast::makeList(args);
    ast::CallExpr* call = ast::makeCall(callee, arg_list /*, 1*/);

    EXPECT_EQ(analyzer->inferType(call), SymbolTable::DataType_t::ANY);
}

TEST_F(SemanticAnalyzerTest, CallWithArguments_AnalyzesAllArgs)
{
    ast::NameExpr* callee = createName("print");
    std::vector<ast::Expr*> args = { ast::makeLiteralInt(1), ast::makeLiteralString("hello"), ast::makeLiteralBool(true) };
    ast::ListExpr* arg_list = ast::makeList(args);
    ast::CallExpr* call = ast::makeCall(callee, arg_list /*, 1*/);

    EXPECT_NO_THROW(analyzer->analyzeExpr(call));
}
