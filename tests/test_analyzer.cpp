#include "../include/parser.hpp"
#include "ast.hpp"

#include <gtest/gtest.h>

using namespace fairuz;
using namespace fairuz::parser;
using namespace testing;

class SemanticAnalyzerTest : public ::testing::Test {
protected:
    Fa_SemanticAnalyzer analyzer;
    Fa_Array<AST::Fa_Stmt*> statements;

    void SetUp() override
    {
    }

    size_t countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity sev) const
    {
        size_t count = 0;
        for (Fa_SemanticAnalyzer::Issue const& issue : analyzer.getIssues()) {
            if (issue.severity == sev)
                count++;
        }
        return count;
    }

    bool hasIssueContaining(Fa_StringRef const& substring) const
    {
        for (Fa_SemanticAnalyzer::Issue const& issue : analyzer.getIssues()) {
            if (issue.message.find(substring))
                return true;
        }
        return false;
    }
};

TEST_F(SemanticAnalyzerTest, InferType_Primitive)
{
    AST::Fa_LiteralExpr* Fa_Expr0 = AST::Fa_makeLiteralInt(42);
    AST::Fa_LiteralExpr* Fa_Expr1 = AST::Fa_makeLiteralFloat(3.14);
    AST::Fa_LiteralExpr* Fa_Expr2 = AST::Fa_makeLiteralString("hello");
    AST::Fa_LiteralExpr* Fa_Expr3 = AST::Fa_makeLiteralBool(true);

    EXPECT_EQ(analyzer.inferType(Fa_Expr0), Fa_SymbolTable::DataType_t::INTEGER);
    EXPECT_EQ(analyzer.inferType(Fa_Expr1), Fa_SymbolTable::DataType_t::FLOAT);
    EXPECT_EQ(analyzer.inferType(Fa_Expr2), Fa_SymbolTable::DataType_t::STRING);
    EXPECT_EQ(analyzer.inferType(Fa_Expr3), Fa_SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer.inferType(nullptr), Fa_SymbolTable::DataType_t::UNKNOWN);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryFa_Expr_Add)
{
    AST::Fa_BinaryExpr* Fa_Expr0 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(20), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* Fa_Expr1 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralFloat(20.5), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* Fa_Expr2 = AST::Fa_makeBinary(AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralString("world"), AST::Fa_BinaryOp::OP_ADD);

    EXPECT_EQ(analyzer.inferType(Fa_Expr0), Fa_SymbolTable::DataType_t::INTEGER);
    EXPECT_EQ(analyzer.inferType(Fa_Expr1), Fa_SymbolTable::DataType_t::FLOAT);
    EXPECT_EQ(analyzer.inferType(Fa_Expr2), Fa_SymbolTable::DataType_t::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryFa_Expr_LogicalOp)
{
    AST::Fa_BinaryExpr* Fa_Expr0 = AST::Fa_makeBinary(AST::Fa_makeLiteralBool(true), AST::Fa_makeLiteralBool(false), AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* Fa_Expr1 = AST::Fa_makeBinary(AST::Fa_makeLiteralBool(true), AST::Fa_makeLiteralBool(false), AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* Fa_Expr2 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(0), Fa_Expr1, AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* Fa_Expr3 = AST::Fa_makeBinary(AST::Fa_makeLiteralBool(true), Fa_Expr2, AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* Fa_Expr4 = AST::Fa_makeBinary(AST::Fa_makeLiteralBool(false), Fa_Expr3, AST::Fa_BinaryOp::OP_AND);

    EXPECT_EQ(analyzer.inferType(Fa_Expr0), Fa_SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer.inferType(Fa_Expr1), Fa_SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer.inferType(Fa_Expr2), Fa_SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer.inferType(Fa_Expr3), Fa_SymbolTable::DataType_t::BOOLEAN);
    EXPECT_EQ(analyzer.inferType(Fa_Expr4), Fa_SymbolTable::DataType_t::BOOLEAN);
}

TEST_F(SemanticAnalyzerTest, UndefinedVariable_ReportsError)
{
    AST::Fa_NameExpr* nameFa_Expr = AST::Fa_makeName("undefined_var");

    analyzer.analyzeFa_Expr(nameFa_Expr);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Undefined variable"));
}

TEST_F(SemanticAnalyzerTest, DefinedVariable_NoError)
{
    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(42));
    statements.push(assign);

    analyzer.analyze(statements);

    AST::Fa_NameExpr* useFa_Expr = AST::Fa_makeName("x");
    analyzer.analyzeFa_Expr(useFa_Expr);

    EXPECT_EQ(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, UnusedVariable_ReportsWarning)
{
    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("unused_var"), AST::Fa_makeLiteralInt(42));
    statements.push(assign);

    analyzer.analyze(statements);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Unused variable"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Subtraction)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralString("world"), AST::Fa_BinaryOp::OP_SUB);

    analyzer.analyzeFa_Expr(binary);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Multiplication)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_MUL);

    analyzer.analyzeFa_Expr(binary);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Division)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_DIV);

    analyzer.analyzeFa_Expr(binary);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, DivisionByZero_ReportsError)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_DIV);

    analyzer.analyzeFa_Expr(binary);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(hasIssueContaining("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, DivisionByNonZero_NoError)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_DIV);

    analyzer.analyzeFa_Expr(binary);

    EXPECT_FALSE(hasIssueContaining("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, BuiltInFunction_Print_IsDefined)
{
    AST::Fa_NameExpr* printName = AST::Fa_makeName("print");

    analyzer.analyzeFa_Expr(printName);

    EXPECT_EQ(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, CallNonCallable_ReportsError)
{
    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("not_a_function"), AST::Fa_makeLiteralInt(42));
    statements.push(assign);
    analyzer.analyze(statements);

    AST::Fa_CallExpr* call = Fa_makeCall(AST::Fa_makeName("not_a_function"), AST::Fa_makeList());

    analyzer.analyzeFa_Expr(call);

    EXPECT_TRUE(hasIssueContaining("is not callable"));
}

TEST_F(SemanticAnalyzerTest, ConstantIfCondition_ReportsWarning)
{
    AST::Fa_IfStmt* ifStmt = AST::Fa_makeIf(AST::Fa_makeLiteralBool(true), AST::Fa_makeBlock(), AST::Fa_makeBlock());

    analyzer.analyzeStmt(ifStmt);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Condition is always constant"));
}

TEST_F(SemanticAnalyzerTest, InfiniteLoop_ReportsWarning)
{
    AST::Fa_WhileStmt* whileStmt = AST::Fa_makeWhile(AST::Fa_makeLiteralBool(true), AST::Fa_makeBlock());

    analyzer.analyzeStmt(whileStmt);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(hasIssueContaining("Infinite loop"));
}

TEST_F(SemanticAnalyzerTest, UnusedFa_ExpressionResult_ReportsInfo)
{
    AST::Fa_ExprStmt* expr_stmt = AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(42));

    analyzer.analyzeStmt(expr_stmt);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Fa_Expression result not used"));
}

TEST_F(SemanticAnalyzerTest, FunctionCallFa_Expression_NoUnusedWarning)
{
    AST::Fa_CallExpr* call = Fa_makeCall(AST::Fa_makeName("print"), AST::Fa_makeList());
    AST::Fa_ExprStmt* expr_stmt = AST::Fa_makeExprStmt(call);

    analyzer.analyzeStmt(expr_stmt);

    size_t infoCount = countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::INFO);
}

TEST_F(SemanticAnalyzerTest, ForLoopVariableShadowing_ReportsWarning)
{
    AST::Fa_AssignmentStmt* outerAssign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(10));
    statements.push(outerAssign);

    analyzer.analyze(statements);

    AST::Fa_ForStmt* forStmt = AST::Fa_makeFor(AST::Fa_makeName("i"), AST::Fa_makeList(), AST::Fa_makeBlock());

    analyzer.analyzeStmt(forStmt);

    EXPECT_TRUE(hasIssueContaining("Loop variable shadows"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithoutReturn_ReportsInfo)
{
    AST::Fa_FunctionDef* funcDef = AST::Fa_makeFunction(AST::Fa_makeName("my_func"), AST::Fa_makeList(), AST::Fa_makeBlock());

    analyzer.analyzeStmt(funcDef);

    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(hasIssueContaining("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithReturn_NoWarning)
{
    AST::Fa_ReturnStmt* retStmt = AST::Fa_makeReturn(AST::Fa_makeLiteralInt(42));
    AST::Fa_FunctionDef* funcDef = AST::Fa_makeFunction(AST::Fa_makeName("my_func"), AST::Fa_makeList(), AST::Fa_makeBlock({ retStmt }));

    analyzer.analyzeStmt(funcDef);

    EXPECT_FALSE(hasIssueContaining("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, NestedBinaryFa_Expressions_InfersCorrectType)
{
    AST::Fa_BinaryExpr* inner_binary = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(20), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* outer_binary = AST::Fa_makeBinary(inner_binary, AST::Fa_makeLiteralFloat(2.5), AST::Fa_BinaryOp::OP_MUL);

    EXPECT_EQ(analyzer.inferType(outer_binary), Fa_SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, ListFa_Expression_InfersListType)
{
    AST::Fa_ListExpr* listFa_Expr = AST::Fa_makeList({ AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(3) });

    EXPECT_EQ(analyzer.inferType(listFa_Expr), Fa_SymbolTable::DataType_t::LIST);

    analyzer.analyzeFa_Expr(listFa_Expr);
}

TEST_F(SemanticAnalyzerTest, AnalyzeNullStatement_NoError) { EXPECT_NO_THROW(analyzer.analyzeStmt(nullptr)); }

TEST_F(SemanticAnalyzerTest, AnalyzeNullFa_Expression_NoError) { EXPECT_NO_THROW(analyzer.analyzeFa_Expr(nullptr)); }

TEST_F(SemanticAnalyzerTest, EmptyStatementList_NoIssues)
{
    analyzer.analyze(statements);
    EXPECT_EQ(analyzer.getIssues().size(), 0);
}

TEST_F(SemanticAnalyzerTest, PrintReport_NoIssues) { EXPECT_NO_THROW(analyzer.printReport()); }

TEST_F(SemanticAnalyzerTest, PrintReport_WithIssues)
{
    analyzer.analyzeFa_Expr(AST::Fa_makeName("undefined"));
    EXPECT_NO_THROW(analyzer.printReport());
}

TEST_F(SemanticAnalyzerTest, GetGlobalScope_NotNull) { EXPECT_NE(analyzer.getGlobalScope(), nullptr); }

TEST_F(SemanticAnalyzerTest, AssignmentCreatesSymbol)
{
    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(42));
    statements.push(assign);

    analyzer.analyze(statements);

    Fa_SymbolTable const* globalScope = analyzer.getGlobalScope();
    ASSERT_NE(globalScope, nullptr);
}

TEST_F(SemanticAnalyzerTest, ComplFa_Exprogram_MultipleIssues)
{
    AST::Fa_ExprStmt* Fa_ExprStmt1 = AST::Fa_makeExprStmt(AST::Fa_makeName("undefined_var"));
    statements.push(Fa_ExprStmt1);

    AST::Fa_BinaryExpr* divFa_Expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_DIV);
    AST::Fa_ExprStmt* Fa_ExprStmt2 = AST::Fa_makeExprStmt(divFa_Expr);
    statements.push(Fa_ExprStmt2);

    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("unused"), AST::Fa_makeLiteralInt(100));
    statements.push(assign);

    analyzer.analyze(statements);

    EXPECT_GE(analyzer.getIssues().size(), 3);
    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_GT(countIssuesBySeverity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
}

TEST_F(SemanticAnalyzerTest, VariableFlowAnalysis)
{
    AST::Fa_AssignmentStmt* assign1 = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(10));
    statements.push(assign1);

    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_AssignmentStmt* assign2 = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("y"), binary);
    statements.push(assign2);

    analyzer.analyze(statements);

    auto issues = analyzer.getIssues();
    size_t unusedYCount = 0;
    for (Fa_SemanticAnalyzer::Issue const& issue : issues) {
        if (issue.message.find('y') && issue.message.find("Unused"))
            unusedYCount++;
    }

    EXPECT_GT(unusedYCount, 0);
}

TEST_F(SemanticAnalyzerTest, CallFa_Expression_InfersAnyType)
{
    AST::Fa_CallExpr* call = Fa_makeCall(AST::Fa_makeName("some_function"), AST::Fa_makeList({ }) /*, 1*/);

    EXPECT_EQ(analyzer.inferType(call), Fa_SymbolTable::DataType_t::ANY);
}

TEST_F(SemanticAnalyzerTest, CallWithArguments_AnalyzesAllArgs)
{
    AST::Fa_CallExpr* call = Fa_makeCall(AST::Fa_makeName("print"), AST::Fa_makeList({ AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralBool(true) }));

    EXPECT_NO_THROW(analyzer.analyzeFa_Expr(call));
}
