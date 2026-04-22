#include "../fairuz/ast.hpp"
#include "../fairuz/parser.hpp"
#include "../fairuz/string.hpp"
#include "test_common.h"

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
    AST::Fa_LiteralExpr* Fa_Expr0 = lit_int(42);
    AST::Fa_LiteralExpr* Fa_Expr1 = lit_flt(3.14);
    AST::Fa_LiteralExpr* Fa_Expr2 = lit_str("hello");
    AST::Fa_LiteralExpr* Fa_Expr3 = lit_bool(true);

    EXPECT_EQ(analyzer.infer_type(Fa_Expr0), Fa_SymbolTable::DataType::INTEGER);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr1), Fa_SymbolTable::DataType::FLOAT);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr2), Fa_SymbolTable::DataType::STRING);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr3), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(nullptr), Fa_SymbolTable::DataType::UNKNOWN);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryFa_Expr_Add)
{
    AST::Fa_BinaryExpr* Fa_Expr0 = binary(lit_int(10), lit_int(20), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* Fa_Expr1 = binary(lit_int(10), lit_flt(20.5), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* Fa_Expr2 = binary(lit_str("hello"), lit_str("world"), AST::Fa_BinaryOp::OP_ADD);

    EXPECT_EQ(analyzer.infer_type(Fa_Expr0), Fa_SymbolTable::DataType::INTEGER);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr1), Fa_SymbolTable::DataType::FLOAT);
    EXPECT_EQ(analyzer.infer_type(Fa_Expr2), Fa_SymbolTable::DataType::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryFa_Expr_LogicalOp)
{
    AST::Fa_BinaryExpr* expr0 = binary(lit_bool(true), lit_bool(false), AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* expr1 = binary(lit_bool(true), lit_bool(false), AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* expr2 = binary(lit_int(0), expr1, AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* expr3 = binary(lit_bool(true), expr2, AST::Fa_BinaryOp::OP_AND);
    AST::Fa_BinaryExpr* expr4 = binary(lit_bool(false), expr3, AST::Fa_BinaryOp::OP_AND);

    EXPECT_EQ(analyzer.infer_type(expr0), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(expr1), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(expr2), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(expr3), Fa_SymbolTable::DataType::BOOLEAN);
    EXPECT_EQ(analyzer.infer_type(expr4), Fa_SymbolTable::DataType::BOOLEAN);
}

TEST_F(SemanticAnalyzerTest, UndefinedVariable_ReportsError)
{
    analyzer.analyze_expr(name_expr("undefined_var"));

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Undefined variable"));
}

TEST_F(SemanticAnalyzerTest, DefinedVariable_NoError)
{
    m_statements.push(assign_stmt(name_expr("x"), lit_int(42)));
    analyzer.analyze(m_statements);
    analyzer.analyze_expr(name_expr("x"));

    EXPECT_EQ(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, UnusedVariable_ReportsWarning)
{
    m_statements.push(assign_stmt(name_expr("unused_var"), lit_int(42)));

    analyzer.analyze(m_statements);

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(has_issue_containing("Unused variable"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Subtraction)
{
    analyzer.analyze_expr(binary(lit_str("hello"), lit_str("world"), AST::Fa_BinaryOp::OP_SUB));

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Multiplication)
{
    analyzer.analyze_expr(binary(lit_str("hello"), lit_int(3), AST::Fa_BinaryOp::OP_MUL));

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Division)
{
    analyzer.analyze_expr(binary(lit_str("hello"), lit_int(2), AST::Fa_BinaryOp::OP_DIV));

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, DivisionByZero_ReportsError)
{
    analyzer.analyze_expr(binary(lit_int(10), lit_int(0), AST::Fa_BinaryOp::OP_DIV));

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_TRUE(has_issue_containing("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, DivisionByNonZero_NoError)
{
    analyzer.analyze_expr(binary(lit_int(10), lit_int(5), AST::Fa_BinaryOp::OP_DIV));

    EXPECT_FALSE(has_issue_containing("Division by zero"));
}

TEST_F(SemanticAnalyzerTest, BuiltInFunction_Print_IsDefined)
{
    analyzer.analyze_expr(name_expr("print"));

    EXPECT_EQ(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, CallNonCallable_ReportsError)
{
    m_statements.push(assign_stmt(name_expr("not_a_function"), lit_int(42)));
    analyzer.analyze(m_statements);
    analyzer.analyze_expr(call_expr(name_expr("not_a_function"), list_expr()));

    EXPECT_TRUE(has_issue_containing("is not callable"));
}

TEST_F(SemanticAnalyzerTest, ConstantIfCondition_ReportsWarning)
{
    analyzer.analyze_stmt(if_stmt(lit_bool(true), blk({ }), blk({ })));

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(has_issue_containing("Condition is always constant"));
}

TEST_F(SemanticAnalyzerTest, InfiniteLoop_ReportsWarning)
{
    analyzer.analyze_stmt(while_stmt(lit_bool(true), blk({ })));

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
    EXPECT_TRUE(has_issue_containing("Infinite loop"));
}

TEST_F(SemanticAnalyzerTest, UnusedFa_ExpressionResult_ReportsInfo)
{
    analyzer.analyze_stmt(expr_stmt(lit_int(42)));

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(has_issue_containing("Fa_Expression result not used"));
}

TEST_F(SemanticAnalyzerTest, FunctionCallFa_Expression_NoUnusedWarning)
{
    analyzer.analyze_stmt(expr_stmt(call_expr(name_expr("print"), list_expr())));

    EXPECT_EQ(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::INFO), 0);
}

TEST_F(SemanticAnalyzerTest, AssignmentExpressionStatement_NoUnusedWarning)
{
    analyzer.analyze_stmt(expr_stmt(assign_expr(name_expr("x"), lit_int(42))));

    EXPECT_EQ(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::INFO), 0);
}

TEST_F(SemanticAnalyzerTest, AssignmentExpressionDeclarationDefinesSymbol)
{
    m_statements.push(expr_stmt(assign_expr(name_expr("x"), lit_int(42))));
    analyzer.analyze(m_statements);
    analyzer.analyze_expr(name_expr("x"));

    EXPECT_EQ(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, ForLoopVariableShadowing_ReportsWarning)
{
    m_statements.push(expr_stmt(assign_expr(name_expr("i"), lit_int(10))));
    analyzer.analyze(m_statements);
    analyzer.analyze_stmt(for_stmt(name_expr("i"), list_expr(), blk({ })));

    EXPECT_TRUE(has_issue_containing("Loop variable shadows"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithoutReturn_ReportsInfo)
{
    analyzer.analyze_stmt(func_def(name_expr("my_func"), list_expr(), blk({ })));

    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::INFO), 0);
    EXPECT_TRUE(has_issue_containing("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithReturn_NoWarning)
{
    analyzer.analyze_stmt(func_def(name_expr("my_func"), list_expr(), blk({ return_stmt(lit_int(42)) })));

    EXPECT_FALSE(has_issue_containing("Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, NestedBinaryFa_Expressions_InfersCorrectType)
{
    AST::Fa_BinaryExpr* inner_binary = binary(lit_int(10), lit_int(20), AST::Fa_BinaryOp::OP_ADD);
    AST::Fa_BinaryExpr* outer_binary = binary(inner_binary, lit_flt(2.5), AST::Fa_BinaryOp::OP_MUL);

    EXPECT_EQ(analyzer.infer_type(outer_binary), Fa_SymbolTable::DataType::FLOAT);
}

TEST_F(SemanticAnalyzerTest, ListFa_Expression_InfersListType)
{
    AST::Fa_ListExpr* list = list_expr({ lit_int(1), lit_int(2), lit_int(3) });

    EXPECT_EQ(analyzer.infer_type(list), Fa_SymbolTable::DataType::LIST);

    analyzer.analyze_expr(list);
}

TEST_F(SemanticAnalyzerTest, AnalyzeNullStatement_NoError) { EXPECT_NO_THROW(analyzer.analyze_stmt(nullptr)); }

TEST_F(SemanticAnalyzerTest, AnalyzeNullFa_Expression_NoError) { EXPECT_NO_THROW(analyzer.analyze_expr(nullptr)); }

TEST_F(SemanticAnalyzerTest, EmptyStatementList_NoIssues)
{
    analyzer.analyze(m_statements);
    EXPECT_EQ(analyzer.get_issues().size(), 0);
}

TEST_F(SemanticAnalyzerTest, PrintReport_NoIssues) { EXPECT_NO_THROW(analyzer.print_report()); }

TEST_F(SemanticAnalyzerTest, PrintReport_WithIssues)
{
    analyzer.analyze_expr(name_expr("undefined"));
    EXPECT_NO_THROW(analyzer.print_report());
}

TEST_F(SemanticAnalyzerTest, GetGlobalScope_NotNull) { EXPECT_NE(analyzer.get_global_scope(), nullptr); }

TEST_F(SemanticAnalyzerTest, AssignmentCreatesSymbol)
{
    m_statements.push(expr_stmt(assign_expr(name_expr("x"), lit_int(42))));
    analyzer.analyze(m_statements);

    ASSERT_NE(analyzer.get_global_scope(), nullptr);
}

TEST_F(SemanticAnalyzerTest, ComplFa_Exprogram_MultipleIssues)
{
    m_statements.push(expr_stmt(name_expr("undefined_var")));
    m_statements.push(expr_stmt(binary(lit_int(10), lit_int(0), AST::Fa_BinaryOp::OP_DIV)));
    m_statements.push(expr_stmt(assign_expr(name_expr("unused"), lit_int(100))));

    analyzer.analyze(m_statements);

    EXPECT_GE(analyzer.get_issues().size(), 3);
    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::ERROR), 0);
    EXPECT_GT(count_issues_by_severity(Fa_SemanticAnalyzer::Issue::Severity::WARNING), 0);
}

TEST_F(SemanticAnalyzerTest, VariableFlowAnalysis)
{
    m_statements.push(assign_stmt(name_expr("x"), lit_int(10)));
    m_statements.push(expr_stmt(assign_expr(name_expr("y"), binary(name_expr("x"), lit_int(5), AST::Fa_BinaryOp::OP_ADD))));

    analyzer.analyze(m_statements);

    auto issues = analyzer.get_issues();
    size_t unused_y_count = 0;
    for (Fa_SemanticAnalyzer::Issue const& issue : issues) {
        if (issue.message.find('y') && issue.message.find("Unused"))
            unused_y_count += 1;
    }

    EXPECT_GT(unused_y_count, 0);
}

TEST_F(SemanticAnalyzerTest, CallFa_Expression_InfersAnyType)
{
    AST::Fa_CallExpr* call = call_expr(name_expr("some_function"), list_expr());

    EXPECT_EQ(analyzer.infer_type(call), Fa_SymbolTable::DataType::ANY);
}

TEST_F(SemanticAnalyzerTest, CallWithArguments_AnalyzesAllArgs)
{
    AST::Fa_CallExpr* call = call_expr(name_expr("print"), list_expr({ lit_int(1), lit_str("hello"), lit_bool(true) }));

    EXPECT_NO_THROW(analyzer.analyze_expr(call));
}
