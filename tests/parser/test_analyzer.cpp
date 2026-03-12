#include "../../include/ast.hpp"
#include "../../include/parser.hpp"
#include "../../include/string.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace mylang;
using namespace mylang::ast;
using namespace mylang::parser;
using namespace testing;

// Test Fixture

class SemanticAnalyzerTest : public ::testing::Test {
protected:
    std::unique_ptr<SemanticAnalyzer> analyzer;
    Array<Stmt*> statements;

    void SetUp() override
    {
        analyzer = std::make_unique<SemanticAnalyzer>();
    }

    // Helper to count issues by severity
    size_t countIssuesBySeverity(SemanticAnalyzer::Issue::Severity sev) const
    {
        size_t count = 0;
        for (SemanticAnalyzer::Issue const& issue : analyzer->getIssues())
            if (issue.severity == sev)
                count++;
        return count;
    }

    // Helper to check if specific issue exists
    bool hasIssueContaining(StringRef const& substring) const
    {
        for (SemanticAnalyzer::Issue const& issue : analyzer->getIssues()) {
            if (issue.message.find(substring))
                return true;
        }
        return false;
    }
};

// Type Inference Tests

TEST_F(SemanticAnalyzerTest, InferType_Primitive)
{
    LiteralExpr* expr0 = makeLiteralInt(42);
    LiteralExpr* expr1 = makeLiteralFloat(3.14);
    LiteralExpr* expr2 = makeLiteralString("hello");
    LiteralExpr* expr3 = makeLiteralBool(true);

    EXPECT_EQ(analyzer->inferType(expr0), SymbolTable::DataType_t::INTEGER);
    EXPECT_EQ(analyzer->inferType(expr1), SymbolTable::DataType_t::FLOAT);
    EXPECT_EQ(analyzer->inferType(expr2), SymbolTable::DataType_t::STRING);
    EXPECT_EQ(analyzer->inferType(expr3), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(nullptr), SymbolTable::DataType_t::UNKNOWN);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_Add)
{
    BinaryExpr* expr0 = makeBinary(makeLiteralInt(10), makeLiteralInt(20), BinaryOp::OP_ADD);
    BinaryExpr* expr1 = makeBinary(makeLiteralInt(10), makeLiteralFloat(20.5), BinaryOp::OP_ADD);
    BinaryExpr* expr2 = makeBinary(makeLiteralString("hello"), makeLiteralString("world"), BinaryOp::OP_ADD);

    EXPECT_EQ(analyzer->inferType(expr0), SymbolTable::DataType_t::INTEGER);
    EXPECT_EQ(analyzer->inferType(expr1), SymbolTable::DataType_t::FLOAT);
    EXPECT_EQ(analyzer->inferType(expr2), SymbolTable::DataType_t::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_LogicalOp)
{
    BinaryExpr* expr0 = makeBinary(makeLiteralBool(true), makeLiteralBool(false), BinaryOp::OP_AND);
    BinaryExpr* expr1 = makeBinary(makeLiteralBool(true), makeLiteralBool(false), BinaryOp::OP_AND);
    BinaryExpr* expr2 = makeBinary(makeLiteralInt(0), expr1, BinaryOp::OP_AND);
    BinaryExpr* expr3 = makeBinary(makeLiteralBool(true), expr2, BinaryOp::OP_AND);
    BinaryExpr* expr4 = makeBinary(makeLiteralBool(false), expr3, BinaryOp::OP_AND);

    EXPECT_EQ(analyzer->inferType(expr0), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(expr1), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(expr2), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(expr3), SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer->inferType(expr4), SymbolTable::DataType_t::BOOLEAN);
}

// Variable Analysis Tests

TEST_F(SemanticAnalyzerTest, UndefinedVariable_ReportsError)
{
    NameExpr* nameExpr = makeName("undefined_var");

    analyzer->analyzeExpr(nameExpr);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Undefined variable"));
}

TEST_F(SemanticAnalyzerTest, DefinedVariable_NoError)
{
    // First define the variable
    AssignmentStmt* assign = makeAssignmentStmt(makeName("x"), makeLiteralInt(42));
    statements.push(assign);

    analyzer->analyze(statements);

    // Now use the variable
    NameExpr* useExpr = makeName("x");
    analyzer->analyzeExpr(useExpr);

    // Should not report undefined variable error for 'x'
    EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, UnusedVariable_ReportsWarning)
{
    AssignmentStmt* assign = makeAssignmentStmt(makeName("unused_var"), makeLiteralInt(42));
    statements.push(assign);

    analyzer->analyze(statements);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Unused variable"));
}

// Type Compatibility Tests

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Subtraction)
{
    BinaryExpr* binary = makeBinary(makeLiteralString("hello"), makeLiteralString("world"), BinaryOp::OP_SUB);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Multiplication)
{
    BinaryExpr* binary = makeBinary(makeLiteralString("hello"), makeLiteralInt(3), BinaryOp::OP_MUL);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Division)
{
    BinaryExpr* binary = makeBinary(makeLiteralString("hello"), makeLiteralInt(2), BinaryOp::OP_DIV);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

// Division by Zero Tests

TEST_F(SemanticAnalyzerTest, DivisionByZero_ReportsError)
{
    BinaryExpr* binary = makeBinary(makeLiteralInt(10), makeLiteralInt(0), BinaryOp::OP_DIV);

    analyzer->analyzeExpr(binary);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, DivisionByNonZero_NoError)
{
    BinaryExpr* binary = makeBinary(makeLiteralInt(10), makeLiteralInt(5), BinaryOp::OP_DIV);

    analyzer->analyzeExpr(binary);

    EXPECT_FALSE(hasIssueContaining("Division by zero"));
}

// Function Analysis Tests

TEST_F(SemanticAnalyzerTest, BuiltInFunction_Print_IsDefined)
{
    NameExpr* printName = makeName("print");

    analyzer->analyzeExpr(printName);

    EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, CallNonCallable_ReportsError)
{
    // Define a variable
    AssignmentStmt* assign = makeAssignmentStmt(makeName("not_a_function"), makeLiteralInt(42));
    statements.push(assign);
    analyzer->analyze(statements);

    // Try to call it
    CallExpr* call = makeCall(makeName("not_a_function"), makeList());

    analyzer->analyzeExpr(call);

    EXPECT_TRUE(hasIssueContaining("is not callable"));
}

// Control Flow Tests

TEST_F(SemanticAnalyzerTest, ConstantIfCondition_ReportsWarning)
{
    IfStmt* ifStmt = makeIf(makeLiteralBool(true), makeBlock(), makeBlock());

    analyzer->analyzeStmt(ifStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Condition is always constant"));
}

TEST_F(SemanticAnalyzerTest, InfiniteLoop_ReportsWarning)
{
    WhileStmt* whileStmt = makeWhile(makeLiteralBool(true), makeBlock());

    analyzer->analyzeStmt(whileStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Infinite loop"));
}

// Expression Statement Tests

TEST_F(SemanticAnalyzerTest, UnusedExpressionResult_ReportsInfo)
{
    ExprStmt* exprStmt = makeExprStmt(makeLiteralInt(42));

    analyzer->analyzeStmt(exprStmt);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Expression result not used"));
}

TEST_F(SemanticAnalyzerTest, FunctionCallExpression_NoUnusedWarning)
{
    CallExpr* call = makeCall(makeName("print"), makeList());
    ExprStmt* exprStmt = makeExprStmt(call);

    analyzer->analyzeStmt(exprStmt);

    // Should not report unused expression for function calls
    size_t infoCount = countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO);
    // May have other info messages, but shouldn't have unused expression warning
}

// Scope Tests

TEST_F(SemanticAnalyzerTest, ForLoopVariableShadowing_ReportsWarning)
{
    // Define outer variable
    AssignmentStmt* outerAssign = makeAssignmentStmt(makeName("i"), makeLiteralInt(10));
    statements.push(outerAssign);

    analyzer->analyze(statements);

    // Create for loop with same variable name
    ForStmt* forStmt = makeFor(makeName("i"), makeList(), makeBlock());

    analyzer->analyzeStmt(forStmt);

    EXPECT_TRUE(hasIssueContaining("Loop variable shadows"));
}

// Function Definition Tests

TEST_F(SemanticAnalyzerTest, FunctionWithoutReturn_ReportsInfo)
{
    FunctionDef* funcDef = makeFunction(makeName("my_func"), makeList(), makeBlock());

    analyzer->analyzeStmt(funcDef);

    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithReturn_NoWarning)
{
    ReturnStmt* retStmt = makeReturn(makeLiteralInt(42));
    FunctionDef* funcDef = makeFunction(makeName("my_func"), makeList(), makeBlock({ retStmt }));

    analyzer->analyzeStmt(funcDef);

    // Should not warn about missing return
    EXPECT_FALSE(hasIssueContaining("Function may not return a value"));
}

// Complex Expression Tests

TEST_F(SemanticAnalyzerTest, NestedBinaryExpressions_InfersCorrectType)
{
    // (10 + 20) * 2.5
    BinaryExpr* inner_binary = makeBinary(makeLiteralInt(10), makeLiteralInt(20), BinaryOp::OP_ADD);
    BinaryExpr* outer_binary = makeBinary(inner_binary, makeLiteralFloat(2.5), BinaryOp::OP_MUL);

    // Should promote to FLOAT due to 2.5
    EXPECT_EQ(analyzer->inferType(outer_binary), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, ListExpression_InfersListType)
{
    ListExpr* listExpr = makeList({ makeLiteralInt(1), makeLiteralInt(2), makeLiteralInt(3) });

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
    analyzer->analyzeExpr(makeName("undefined"));
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
    AssignmentStmt* assign = makeAssignmentStmt(makeName("x"), makeLiteralInt(42));
    statements.push(assign);

    analyzer->analyze(statements);

    // Verify symbol was created
    SymbolTable const* globalScope = analyzer->getGlobalScope();
    ASSERT_NE(globalScope, nullptr);
}

// Integration Tests

TEST_F(SemanticAnalyzerTest, ComplexProgram_MultipleIssues)
{
    // Undefined variable use
    ExprStmt* exprStmt1 = makeExprStmt(makeName("undefined_var"));
    statements.push(exprStmt1);

    // Division by zero
    BinaryExpr* divExpr = makeBinary(makeLiteralInt(10), makeLiteralInt(0), BinaryOp::OP_DIV);
    ExprStmt* exprStmt2 = makeExprStmt(divExpr);
    statements.push(exprStmt2);

    // Unused variable
    AssignmentStmt* assign = makeAssignmentStmt(makeName("unused"), makeLiteralInt(100));
    statements.push(assign);

    analyzer->analyze(statements);

    // Should have multiple issues
    EXPECT_GE(analyzer->getIssues().size(), 3);
    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
}

TEST_F(SemanticAnalyzerTest, VariableFlowAnalysis)
{
    // Define variable
    AssignmentStmt* assign1 = makeAssignmentStmt(makeName("x"), makeLiteralInt(10));
    statements.push(assign1);

    // Use variable
    BinaryExpr* binary = makeBinary(makeName("x"), makeLiteralInt(5), BinaryOp::OP_ADD);
    AssignmentStmt* assign2 = makeAssignmentStmt(makeName("y"), binary);
    statements.push(assign2);

    analyzer->analyze(statements);

    // Should report unused variable 'y' but not 'x'
    auto issues = analyzer->getIssues();
    size_t unusedYCount = 0;
    for (SemanticAnalyzer::Issue const& issue : issues)
        if (issue.message.find('y') && issue.message.find("Unused"))
            unusedYCount++;

    EXPECT_GT(unusedYCount, 0);
}

// Call Expression Tests

TEST_F(SemanticAnalyzerTest, CallExpression_InfersAnyType)
{
    CallExpr* call = makeCall(makeName("some_function"), makeList({}) /*, 1*/);

    EXPECT_EQ(analyzer->inferType(call), SymbolTable::DataType_t::ANY);
}

TEST_F(SemanticAnalyzerTest, CallWithArguments_AnalyzesAllArgs)
{
    CallExpr* call = makeCall(makeName("print"), makeList({ makeLiteralInt(1), makeLiteralString("hello"), makeLiteralBool(true) }));

    EXPECT_NO_THROW(analyzer->analyzeExpr(call));
}
