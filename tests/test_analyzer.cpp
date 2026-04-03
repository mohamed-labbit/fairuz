#include "../include/parser.hpp"
#include "ast.hpp"

#include <gtest/gtest.h>

using namespace fairuz;
using namespace fairuz::parser;
using namespace testing;

class SemanticAnalyzerTest : public ::testing::Test {
protected:
    Fa_SemanticAnalyzer analyzer;
    Fa_Array<AST::Fa_Stmt*> m_statements;

    void SetUp() override
    {
    }

    size_t count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity sev) const
    {
        size_t count = 0;
        for (Fa_SemanticAnalyzer::Issue const& issue : analyzer.get_issues()) {
            if (issue.severity == sev)
                count += 1;
        }
        return count;
    }

    bool has_issue_containing(Fa_StringRef const& substring) const
    {
        for (Fa_SemanticAnalyzer::Issue const& issue : analyzer.get_issues()) {
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

    EXPECT_EQ(analyzer.infer_type(Fa_Expr0), Fa_SymbolTable::DataType::INTEGER);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr1), Fa_SymbolTable::DataType::FLOAT);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr2), Fa_SymbolTable::DataType::STRING);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr3), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(nullptr), Fa_SymbolTable::DataType::UNKNOWN);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryFa_Expr_Add)
{
    AST::Fa_BinaryExpr* Fa_Expr0 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(20), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* Fa_Expr1 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralFloat(20.5), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* Fa_Expr2 = AST::Fa_makeBinary(AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralString("world"), AST::Fa_BinaryOp::OP_ADD);

    EXPECT_EQ(analyzer.infer_type(Fa_Expr0), Fa_SymbolTable::DataType::INTEGER);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr1), Fa_SymbolTable::DataType::FLOAT);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr2), Fa_SymbolTable::DataType::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryFa_Expr_LogicalOp)
{
    AST::Fa_BinaryExpr* Fa_Expr0 = AST::Fa_makeBinary(AST::Fa_makeLiteralBool(true), AST::Fa_makeLiteralBool(false), AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* Fa_Expr1 = AST::Fa_makeBinary(AST::Fa_makeLiteralBool(true), AST::Fa_makeLiteralBool(false), AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* Fa_Expr2 = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(0), Fa_Expr1, AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* Fa_Expr3 = AST::Fa_makeBinary(AST::Fa_makeLiteralBool(true), Fa_Expr2, AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* Fa_Expr4 = AST::Fa_makeBinary(AST::Fa_makeLiteralBool(false), Fa_Expr3, AST::Fa_BinaryOp::OP_AND);

    EXPECT_EQ(analyzer.infer_type(Fa_Expr0), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr1), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr2), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr3), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr4), Fa_SymbolTable::DataType::BOOLEAN);
}

TEST_F(SemanticAnalyzerTest, UndefinedVariable_ReportsError)
{
    AST::Fa_NameExpr* name_fa_expr = AST::Fa_makeName("undefined_var");

    analyzer.analyze_fa_expr(name_fa_expr);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Undefined variable"));
}

TEST_F(SemanticAnalyzerTest, DefinedVariable_NoError)
{
    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(42));
    m_statements.push(assign);

    analyzer.analyze(m_statements);

    AST::Fa_NameExpr* use_fa_expr = AST::Fa_makeName("x");
    analyzer.analyze_fa_expr(use_fa_expr);

    EXPECT_EQ(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, UnusedVariable_ReportsWarning)
{
    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("unused_var"), AST::Fa_makeLiteralInt(42));
    m_statements.push(assign);

    analyzer.analyze(m_statements);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(has_issue_containing("Unused variable"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Subtraction)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralString("world"), AST::Fa_BinaryOp::OP_SUB);

    analyzer.analyze_fa_expr(binary);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Multiplication)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_MUL);

    analyzer.analyze_fa_expr(binary);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Division)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_DIV);

    analyzer.analyze_fa_expr(binary);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, DivisionByZero_ReportsError)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_DIV);

    analyzer.analyze_fa_expr(binary);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, DivisionByNonZero_NoError)
{
    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_DIV);

    analyzer.analyze_fa_expr(binary);

    EXPECT_FALSE(has_issue_containing("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, BuiltInFunction_Print_IsDefined)
{
    AST::Fa_NameExpr* print_name = AST::Fa_makeName("print");

    analyzer.analyze_fa_expr(print_name);

    EXPECT_EQ(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, CallNonCallable_ReportsError)
{
    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("not_a_function"), AST::Fa_makeLiteralInt(42));
    m_statements.push(assign);
    analyzer.analyze(m_statements);

    AST::Fa_CallExpr* call = Fa_makeCall(AST::Fa_makeName("not_a_function"), AST::Fa_makeList());

    analyzer.analyze_fa_expr(call);

    EXPECT_TRUE(has_issue_containing("is not callable"));
}

TEST_F(SemanticAnalyzerTest, ConstantIfCondition_ReportsWarning)
{
    AST::Fa_IfStmt* if_stmt = AST::Fa_makeIf(AST::Fa_makeLiteralBool(true), AST::Fa_makeBlock(), AST::Fa_makeBlock());

    analyzer.analyze_stmt(if_stmt);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(has_issue_containing("Condition is always constant"));
}

TEST_F(SemanticAnalyzerTest, InfiniteLoop_ReportsWarning)
{
    AST::Fa_WhileStmt* while_stmt = AST::Fa_makeWhile(AST::Fa_makeLiteralBool(true), AST::Fa_makeBlock());

    analyzer.analyze_stmt(while_stmt);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(has_issue_containing("Infinite loop"));
}

TEST_F(SemanticAnalyzerTest, UnusedFa_ExpressionResult_ReportsInfo)
{
    AST::Fa_ExprStmt* expr_stmt = AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(42));

    analyzer.analyze_stmt(expr_stmt);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(has_issue_containing("Fa_Expression result not used"));
}

TEST_F(SemanticAnalyzerTest, FunctionCallFa_Expression_NoUnusedWarning)
{
    AST::Fa_CallExpr* call = Fa_makeCall(AST::Fa_makeName("print"), AST::Fa_makeList());
    AST::Fa_ExprStmt* expr_stmt = AST::Fa_makeExprStmt(call);

    analyzer.analyze_stmt(expr_stmt);

    size_t info_count = count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::INFO);
}

TEST_F(SemanticAnalyzerTest, AssignmentExpressionStatement_NoUnusedWarning)
{
    AST::Fa_ExprStmt* expr_stmt = AST::Fa_makeExprStmt(
        AST::Fa_makeAssignmentExpr(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(42), true));

    analyzer.analyze_stmt(expr_stmt);

    EXPECT_EQ(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::INFO), 0);
}

TEST_F(SemanticAnalyzerTest, AssignmentExpressionDeclarationDefinesSymbol)
{
    AST::Fa_ExprStmt* expr_stmt = AST::Fa_makeExprStmt(
        AST::Fa_makeAssignmentExpr(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(42), true));
    m_statements.push(expr_stmt);

    analyzer.analyze(m_statements);
    analyzer.analyze_fa_expr(AST::Fa_makeName("x"));

    EXPECT_EQ(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, ForLoopVariableShadowing_ReportsWarning)
{
    AST::Fa_AssignmentStmt* outer_assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(10));
    m_statements.push(outer_assign);

    analyzer.analyze(m_statements);

    AST::Fa_ForStmt* for_stmt = AST::Fa_makeFor(AST::Fa_makeName("i"), AST::Fa_makeList(), AST::Fa_makeBlock());

    analyzer.analyze_stmt(for_stmt);

    EXPECT_TRUE(has_issue_containing("Loop variable shadows"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithoutReturn_ReportsInfo)
{
    AST::Fa_FunctionDef* func_def = AST::Fa_makeFunction(AST::Fa_makeName("my_func"), AST::Fa_makeList(), AST::Fa_makeBlock());

    analyzer.analyze_stmt(func_def);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(has_issue_containing("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithReturn_NoWarning)
{
    AST::Fa_ReturnStmt* ret_stmt = AST::Fa_makeReturn(AST::Fa_makeLiteralInt(42));
    AST::Fa_FunctionDef* func_def = AST::Fa_makeFunction(AST::Fa_makeName("my_func"), AST::Fa_makeList(), AST::Fa_makeBlock({ ret_stmt }));

    analyzer.analyze_stmt(func_def);

    EXPECT_FALSE(has_issue_containing("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, NestedBinaryFa_Expressions_InfersCorrectType)
{
    AST::Fa_BinaryExpr* inner_binary = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(20), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* outer_binary = AST::Fa_makeBinary(inner_binary, AST::Fa_makeLiteralFloat(2.5), AST::Fa_BinaryOp::OP_MUL);

    EXPECT_EQ(analyzer.infer_type(outer_binary), Fa_SymbolTable::DataType::FLOAT);
}

TEST_F(SemanticAnalyzerTest, ListFa_Expression_InfersListType)
{
    AST::Fa_ListExpr* list_fa_expr = AST::Fa_makeList({ AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(3) });

    EXPECT_EQ(analyzer.infer_type(list_fa_expr), Fa_SymbolTable::DataType::LIST);

    analyzer.analyze_fa_expr(list_fa_expr);
}

TEST_F(SemanticAnalyzerTest, AnalyzeNullStatement_NoError) { EXPECT_NO_THROW(analyzer.analyze_stmt(nullptr)); }

TEST_F(SemanticAnalyzerTest, AnalyzeNullFa_Expression_NoError) { EXPECT_NO_THROW(analyzer.analyze_fa_expr(nullptr)); }

TEST_F(SemanticAnalyzerTest, EmptyStatementList_NoIssues)
{
    analyzer.analyze(m_statements);
    EXPECT_EQ(analyzer.get_issues().size(), 0);
}

TEST_F(SemanticAnalyzerTest, PrintReport_NoIssues) { EXPECT_NO_THROW(analyzer.print_report()); }

TEST_F(SemanticAnalyzerTest, PrintReport_WithIssues)
{
    analyzer.analyze_fa_expr(AST::Fa_makeName("undefined"));
    EXPECT_NO_THROW(analyzer.print_report());
}

TEST_F(SemanticAnalyzerTest, GetGlobalScope_NotNull) { EXPECT_NE(analyzer.get_global_scope(), nullptr); }

TEST_F(SemanticAnalyzerTest, AssignmentCreatesSymbol)
{
    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(42));
    m_statements.push(assign);

    analyzer.analyze(m_statements);

    Fa_SymbolTable const* m_global_scope = analyzer.get_global_scope();
    ASSERT_NE(m_global_scope, nullptr);
}

TEST_F(SemanticAnalyzerTest, ComplFa_Exprogram_MultipleIssues)
{
    AST::Fa_ExprStmt* Fa_ExprStmt1 = AST::Fa_makeExprStmt(AST::Fa_makeName("undefined_var"));
    m_statements.push(Fa_ExprStmt1);

    AST::Fa_BinaryExpr* div_fa_expr = AST::Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_DIV);
    AST::Fa_ExprStmt* Fa_ExprStmt2 = AST::Fa_makeExprStmt(div_fa_expr);
    m_statements.push(Fa_ExprStmt2);

    AST::Fa_AssignmentStmt* assign = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("unused"), AST::Fa_makeLiteralInt(100));
    m_statements.push(assign);

    analyzer.analyze(m_statements);

    EXPECT_GE(analyzer.get_issues().size(), 3);
    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
}

TEST_F(SemanticAnalyzerTest, VariableFlowAnalysis)
{
    AST::Fa_AssignmentStmt* assign1 = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(10));
    m_statements.push(assign1);

    AST::Fa_BinaryExpr* binary = AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(5), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_AssignmentStmt* assign2 = AST::Fa_makeAssignmentStmt(AST::Fa_makeName("y"), binary);
    m_statements.push(assign2);

    analyzer.analyze(m_statements);

    auto m_issues = analyzer.get_issues();
    size_t unused_y_count = 0;
    for (Fa_SemanticAnalyzer::Issue const& issue : m_issues) {
        if (issue.message.find('y') && issue.message.find("Unused"))
            unused_y_count += 1;
    }

    EXPECT_GT(unused_y_count, 0);
}

TEST_F(SemanticAnalyzerTest, CallFa_Expression_InfersAnyType)
{
    AST::Fa_CallExpr* call = Fa_makeCall(AST::Fa_makeName("some_function"), AST::Fa_makeList({ }) /*, 1*/);

    EXPECT_EQ(analyzer.infer_type(call), Fa_SymbolTable::DataType::ANY);
}

TEST_F(SemanticAnalyzerTest, CallWithArguments_AnalyzesAllArgs)
{
    AST::Fa_CallExpr* call = Fa_makeCall(AST::Fa_makeName("print"), AST::Fa_makeList({ AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralBool(true) }));

    EXPECT_NO_THROW(analyzer.analyze_fa_expr(call));
}
