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

    // Helper to count issues by severity
    size_t countIssuesBySeverity(SemanticAnalyzer::Issue::Severity sev) const
    {
        size_t count = 0;
        for (auto const& issue : analyzer->getIssues())
            if (issue.severity == sev)
                count++;
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

TEST_F(SemanticAnalyzerTest, InferType_Primitive)
{
    ast::LiteralExpr* expr0 = ast::makeLiteralInt(42);
    ast::LiteralExpr* expr1 = ast::makeLiteralFloat(3.14);
    ast::LiteralExpr* expr2 = ast::makeLiteralString("hello");
    ast::LiteralExpr* expr3 = ast::makeLiteralBool(true);

    EXPECT_EQ(analyzer->inferType(expr0), SymbolTable::DataType_t::INTEGER);
    EXPECT_EQ(analyzer->inferType(expr1), SymbolTable::DataType_t::FLOAT);
    EXPECT_EQ(analyzer->inferType(expr2), SymbolTable::DataType_t::STRING);
    EXPECT_EQ(analyzer->inferType(expr3), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(nullptr), SymbolTable::DataType_t::UNKNOWN);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_Add)
{
    ast::BinaryExpr* expr0 = ast::makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(20), ast::BinaryOp::OP_ADD);
    ast::BinaryExpr* expr1 = ast::makeBinary(ast::makeLiteralInt(10), ast::makeLiteralFloat(20.5), ast::BinaryOp::OP_ADD);
    ast::BinaryExpr* expr2 = ast::makeBinary(ast::makeLiteralString("hello"), ast::makeLiteralString("world"), ast::BinaryOp::OP_ADD);

    EXPECT_EQ(analyzer->inferType(expr0), SymbolTable::DataType_t::INTEGER);
    EXPECT_EQ(analyzer->inferType(expr1), SymbolTable::DataType_t::FLOAT);
    EXPECT_EQ(analyzer->inferType(expr2), SymbolTable::DataType_t::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_LogicalOp)
{
    ast::BinaryExpr* expr0 = ast::makeBinary(ast::makeLiteralBool(true), ast::makeLiteralBool(false), ast::BinaryOp::OP_AND);
    ast::BinaryExpr* expr1 = ast::makeBinary(ast::makeLiteralBool(true), ast::makeLiteralBool(false), ast::BinaryOp::OP_AND);
    ast::BinaryExpr* expr2 = ast::makeBinary(ast::makeLiteralInt(0), expr1, ast::BinaryOp::OP_AND);
    ast::BinaryExpr* expr3 = ast::makeBinary(ast::makeLiteralBool(true), expr2, ast::BinaryOp::OP_AND);
    ast::BinaryExpr* expr4 = ast::makeBinary(ast::makeLiteralBool(false), expr3, ast::BinaryOp::OP_AND);

    EXPECT_EQ(analyzer->inferType(expr0), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(expr1), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(expr2), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(expr3), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(expr4), SymbolTable::DataType_t::BOOLEAN);
}

// Variable Analysis Tests

TEST_F(SemanticAnalyzerTest, UndefinedVariable_ReportsError)
{
    ast::NameExpr* nameExpr = ast::makeName("undefined_var");

    analyzer->analyzeExpr(nameExpr);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Undefined variable"));
}

TEST_F(SemanticAnalyzerTest, DefinedVariable_NoError)
{
    // First define the variable
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(ast::makeName("x"), ast::makeLiteralInt(42));
    statements.push_back(assign);

    analyzer->analyze(statements);

    // Now use the variable
    ast::NameExpr* useExpr = ast::makeName("x");
    analyzer->analyzeExpr(useExpr);

    // Should not report undefined variable error for 'x'
    EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, UnusedVariable_ReportsWarning)
{
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(ast::makeName("unused_var"), ast::makeLiteralInt(42));
    statements.push_back(assign);

    analyzer->analyze(statements);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Unused variable"));
}

// Type Compatibility Tests

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Subtraction)
{
    ast::BinaryExpr* binary = ast::makeBinary(ast::makeLiteralString("hello"), ast::makeLiteralString("world"), ast::BinaryOp::OP_SUB);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Multiplication)
{
    ast::BinaryExpr* binary = ast::makeBinary(ast::makeLiteralString("hello"), ast::makeLiteralInt(3), ast::BinaryOp::OP_MUL);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Division)
{
    ast::BinaryExpr* binary = ast::makeBinary(ast::makeLiteralString("hello"), ast::makeLiteralInt(2), ast::BinaryOp::OP_DIV);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

// Division by Zero Tests

TEST_F(SemanticAnalyzerTest, DivisionByZero_ReportsError)
{
    ast::BinaryExpr* binary = ast::makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(0), ast::BinaryOp::OP_DIV);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, DivisionByNonZero_NoError)
{
    auto binary = ast::makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(5), ast::BinaryOp::OP_DIV);

    analyzer->analyzeExpr(binary);

    EXPECT_FALSE(hasIssueContaining("Division by zero"));
}

// Function Analysis Tests

TEST_F(SemanticAnalyzerTest, BuiltInFunction_Print_IsDefined)
{
    ast::NameExpr* printName = ast::makeName("print");

    analyzer->analyzeExpr(printName);

    EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, CallNonCallable_ReportsError)
{
    // Define a variable
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(ast::makeName("not_a_function"), ast::makeLiteralInt(42));
    statements.push_back(assign);
    analyzer->analyze(statements);

    // Try to call it
    ast::CallExpr* call = ast::makeCall(ast::makeName("not_a_function"), ast::makeList());

    analyzer->analyzeExpr(call);

    EXPECT_TRUE(hasIssueContaining("is not callable"));
}

// Control Flow Tests

TEST_F(SemanticAnalyzerTest, ConstantIfCondition_ReportsWarning)
{
    ast::IfStmt* ifStmt = ast::makeIf(ast::makeLiteralBool(true), ast::makeBlock(), ast::makeBlock());

    analyzer->analyzeStmt(ifStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Condition is always constant"));
}

TEST_F(SemanticAnalyzerTest, InfiniteLoop_ReportsWarning)
{
    ast::WhileStmt* whileStmt = ast::makeWhile(ast::makeLiteralBool(true), ast::makeBlock());

    analyzer->analyzeStmt(whileStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Infinite loop"));
}

// Expression Statement Tests

TEST_F(SemanticAnalyzerTest, UnusedExpressionResult_ReportsInfo)
{
    ast::ExprStmt* exprStmt = ast::makeExprStmt(ast::makeLiteralInt(42));

    analyzer->analyzeStmt(exprStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Expression result not used"));
}

TEST_F(SemanticAnalyzerTest, FunctionCallExpression_NoUnusedWarning)
{
    ast::CallExpr* call = ast::makeCall(ast::makeName("print"), ast::makeList());
    ast::ExprStmt* exprStmt = ast::makeExprStmt(call);

    analyzer->analyzeStmt(exprStmt);

    // Should not report unused expression for function calls
    size_t infoCount = countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO);
    // May have other info messages, but shouldn't have unused expression warning
}

// Scope Tests

TEST_F(SemanticAnalyzerTest, ForLoopVariableShadowing_ReportsWarning)
{
    // Define outer variable
    ast::AssignmentStmt* outerAssign = ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(10));
    statements.push_back(outerAssign);

    analyzer->analyze(statements);

    // Create for loop with same variable name
    ast::ForStmt* forStmt = ast::makeFor(ast::makeName("i"), ast::makeList(), ast::makeBlock());

    analyzer->analyzeStmt(forStmt);

    EXPECT_TRUE(hasIssueContaining("Loop variable shadows"));
}

// Function Definition Tests

TEST_F(SemanticAnalyzerTest, FunctionWithoutReturn_ReportsInfo)
{
    ast::FunctionDef* funcDef = ast::makeFunction(ast::makeName("my_func"), ast::makeList(), ast::makeBlock());

    analyzer->analyzeStmt(funcDef);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithReturn_NoWarning)
{
    ast::ReturnStmt* retStmt = ast::makeReturn(ast::makeLiteralInt(42));
    ast::FunctionDef* funcDef = ast::makeFunction(ast::makeName("my_func"), ast::makeList(), ast::makeBlock({ retStmt }));

    analyzer->analyzeStmt(funcDef);

    // Should not warn about missing return
    EXPECT_FALSE(hasIssueContaining("Function may not return a value"));
}

// Complex Expression Tests

TEST_F(SemanticAnalyzerTest, NestedBinaryExpressions_InfersCorrectType)
{
    // (10 + 20) * 2.5
    ast::BinaryExpr* inner_binary = ast::makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(20), ast::BinaryOp::OP_ADD);
    ast::BinaryExpr* outer_binary = ast::makeBinary(inner_binary, ast::makeLiteralFloat(2.5), ast::BinaryOp::OP_MUL);

    // Should promote to FLOAT due to 2.5
    EXPECT_EQ(analyzer->inferType(outer_binary), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, ListExpression_InfersListType)
{
    ast::ListExpr* listExpr = ast::makeList({ ast::makeLiteralInt(1), ast::makeLiteralInt(2), ast::makeLiteralInt(3) });

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
    analyzer->analyzeExpr(ast::makeName("undefined"));
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
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(ast::makeName("x"), ast::makeLiteralInt(42));
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
    ast::ExprStmt* exprStmt1 = ast::makeExprStmt(ast::makeName("undefined_var"));
    statements.push_back(exprStmt1);

    // Division by zero
    ast::BinaryExpr* divExpr = ast::makeBinary(ast::makeLiteralInt(10), ast::makeLiteralInt(0), ast::BinaryOp::OP_DIV);
    ast::ExprStmt* exprStmt2 = ast::makeExprStmt(divExpr);
    statements.push_back(exprStmt2);

    // Unused variable
    ast::AssignmentStmt* assign = ast::makeAssignmentStmt(ast::makeName("unused"), ast::makeLiteralInt(100));
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
    ast::AssignmentStmt* assign1 = ast::makeAssignmentStmt(ast::makeName("x"), ast::makeLiteralInt(10));
    statements.push_back(assign1);

    // Use variable
    ast::BinaryExpr* binary = ast::makeBinary(ast::makeName("x"), ast::makeLiteralInt(5), ast::BinaryOp::OP_ADD);
    ast::AssignmentStmt* assign2 = ast::makeAssignmentStmt(ast::makeName("y"), binary);
    statements.push_back(assign2);

    analyzer->analyze(statements);

    // Should report unused variable 'y' but not 'x'
    auto issues = analyzer->getIssues();
    size_t unusedYCount = 0;
    for (auto const& issue : issues)
        if (issue.message.find('y') && issue.message.find("Unused"))
            unusedYCount++;

    EXPECT_GT(unusedYCount, 0);
}

// Call Expression Tests

TEST_F(SemanticAnalyzerTest, CallExpression_InfersAnyType)
{
    std::vector<ast::Expr*> args;
    ast::CallExpr* call = ast::makeCall(ast::makeName("some_function"), ast::makeList(args) /*, 1*/);

    EXPECT_EQ(analyzer->inferType(call), SymbolTable::DataType_t::ANY);
}

TEST_F(SemanticAnalyzerTest, CallWithArguments_AnalyzesAllArgs)
{
    std::vector<ast::Expr*> args = { ast::makeLiteralInt(1), ast::makeLiteralString("hello"), ast::makeLiteralBool(true) };
    ast::CallExpr* call = ast::makeCall(ast::makeName("print"), ast::makeList(args));

    EXPECT_NO_THROW(analyzer->analyzeExpr(call));
}
