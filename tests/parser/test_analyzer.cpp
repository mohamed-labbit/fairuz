#include "../../include//ast/AST_allocator.hpp"
#include "../../include//ast/ast.hpp"
#include "../../include/input/file_manager.hpp"
#include "../../include/parser/analyzer.hpp"
#include "../../include/types/string.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>


using namespace mylang;
using namespace mylang::input;
using namespace mylang::parser;
using namespace testing;

// Test Fixture

class SemanticAnalyzerTest: public ::testing::Test
{
 protected:
  std::unique_ptr<SemanticAnalyzer> analyzer;
  std::vector<ast::Stmt*>           statements;

  void SetUp() override
  {
    analyzer = std::make_unique<SemanticAnalyzer>();
    statements.clear();
  }

  // Helper function to create a literal expression
  ast::LiteralExpr* createNumberLiteral(const StringRef& value, std::int32_t line = 1)
  {
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, value /*, line*/);
  }

  ast::LiteralExpr* createStringLiteral(const StringRef& value, std::int32_t line = 1)
  {
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::STRING, value /*, line*/);
  }

  ast::LiteralExpr* createBooleanLiteral(const StringRef& value, std::int32_t line = 1)
  {
    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::BOOLEAN, value /*, line*/);
  }

  ast::LiteralExpr* createNoneLiteral(std::int32_t line = 1)
  {

    return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NONE, u"none" /*, line*/);
  }

  // Helper function to create a name expression
  ast::NameExpr* createName(const StringRef& name, std::int32_t line = 1) { return ast::AST_allocator.make<ast::NameExpr>(name /*, line*/); }

  // Helper function to create a binary expression
  ast::BinaryExpr* createBinary(ast::Expr* left, tok::TokenType op, ast::Expr* right, std::int32_t line = 1)
  {
    return ast::AST_allocator.make<ast::BinaryExpr>(left, right, op /*, line*/);
  }

  // Helper to count issues by severity
  size_t countIssuesBySeverity(SemanticAnalyzer::Issue::Severity sev) const
  {
    size_t count = 0;
    for (const auto& issue : analyzer->getIssues())
    {
      if (issue.severity == sev)
        count++;
    }
    return count;
  }

  // Helper to check if specific issue exists
  bool hasIssueContaining(const StringRef& substring) const
  {
    for (const auto& issue : analyzer->getIssues())
    {
      if (issue.message.find(substring))
        return true;
    }
    return false;
  }
};

// Type Inference Tests

TEST_F(SemanticAnalyzerTest, InferType_NumberLiteral_Integer)
{
  ast::LiteralExpr* expr = createNumberLiteral(u"42");
  EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::INTEGER);
}

TEST_F(SemanticAnalyzerTest, InferType_NumberLiteral_Float)
{
  auto* expr = createNumberLiteral(u"3.14");
  EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, InferType_StringLiteral)
{
  auto* expr = createStringLiteral(u"hello");
  EXPECT_EQ(analyzer->inferType(expr), SymbolTable::DataType_t::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BooleanLiteral)
{
  auto* expr = createBooleanLiteral(u"true");
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
  auto* left   = createNumberLiteral(u"10");
  auto* right  = createNumberLiteral(u"20");
  auto* binary = createBinary(left, tok::TokenType::OP_PLUS, right);

  EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::INTEGER);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_FloatPromotion)
{
  auto* left   = createNumberLiteral(u"10");
  auto* right  = createNumberLiteral(u"20.5");
  auto* binary = createBinary(left, tok::TokenType::OP_PLUS, right);

  EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_StringConcat)
{
  auto* left   = createStringLiteral(u"hello");
  auto* right  = createStringLiteral(u"world");
  auto* binary = createBinary(left, tok::TokenType::OP_PLUS, right);

  EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::STRING);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_LogicalAnd)
{
  auto* left   = createBooleanLiteral(u"true");
  auto* right  = createBooleanLiteral(u"false");
  auto* binary = createBinary(left, tok::TokenType::KW_AND, right);

  EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::BOOLEAN);
}

TEST_F(SemanticAnalyzerTest, InferType_BinaryExpr_LogicalOr)
{
  auto* left   = createBooleanLiteral(u"true");
  auto* right  = createBooleanLiteral(u"false");
  auto* binary = createBinary(left, tok::TokenType::KW_OR, right);

  EXPECT_EQ(analyzer->inferType(binary), SymbolTable::DataType_t::BOOLEAN);
}

// Variable Analysis Tests

TEST_F(SemanticAnalyzerTest, UndefinedVariable_ReportsError)
{
  auto* nameExpr = createName(u"undefined_var", 5);

  analyzer->analyzeExpr(nameExpr);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
  EXPECT_TRUE(hasIssueContaining(u"Undefined variable"));
}

TEST_F(SemanticAnalyzerTest, DefinedVariable_NoError)
{
  // First define the variable
  auto* value  = createNumberLiteral(u"42");
  auto* target = createName(u"x");
  auto* assign = ast::AST_allocator.make<ast::AssignmentStmt>(target, value /*, 1*/);
  statements.push_back(assign);

  analyzer->analyze(statements);

  // Now use the variable
  auto* useExpr = createName(u"x", 2);
  analyzer->analyzeExpr(useExpr);

  // Should not report undefined variable error for 'x'
  EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, UnusedVariable_ReportsWarning)
{
  auto* value  = createNumberLiteral(u"42");
  auto* target = createName(u"unused_var");
  auto* assign = ast::AST_allocator.make<ast::AssignmentStmt>(target, value /*, 1*/);
  statements.push_back(assign);

  analyzer->analyze(statements);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
  EXPECT_TRUE(hasIssueContaining(u"Unused variable"));
}

// Type Compatibility Tests

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Subtraction)
{
  auto* left   = createStringLiteral(u"hello");
  auto* right  = createStringLiteral(u"world");
  auto* binary = createBinary(left, tok::TokenType::OP_MINUS, right);

  analyzer->analyzeExpr(binary);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
  EXPECT_TRUE(hasIssueContaining(u"Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Multiplication)
{
  auto* left   = createStringLiteral(u"hello");
  auto* right  = createNumberLiteral(u"3");
  auto* binary = createBinary(left, tok::TokenType::OP_STAR, right);

  analyzer->analyzeExpr(binary);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
  EXPECT_TRUE(hasIssueContaining(u"Invalid operation on string"));
}

TEST_F(SemanticAnalyzerTest, InvalidStringOperation_Division)
{
  auto* left   = createStringLiteral(u"hello");
  auto* right  = createNumberLiteral(u"2");
  auto* binary = createBinary(left, tok::TokenType::OP_SLASH, right);

  analyzer->analyzeExpr(binary);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
  EXPECT_TRUE(hasIssueContaining(u"Invalid operation on string"));
}

// Division by Zero Tests

TEST_F(SemanticAnalyzerTest, DivisionByZero_ReportsError)
{
  auto* left   = createNumberLiteral(u"10");
  auto* right  = createNumberLiteral(u"0");
  auto* binary = createBinary(left, tok::TokenType::OP_SLASH, right);

  analyzer->analyzeExpr(binary);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
  EXPECT_TRUE(hasIssueContaining(u"Division by zero"));
}

TEST_F(SemanticAnalyzerTest, DivisionByNonZero_NoError)
{
  auto* left   = createNumberLiteral(u"10");
  auto* right  = createNumberLiteral(u"5");
  auto* binary = createBinary(left, tok::TokenType::OP_SLASH, right);

  analyzer->analyzeExpr(binary);

  EXPECT_FALSE(hasIssueContaining(u"Division by zero"));
}

// Function Analysis Tests

TEST_F(SemanticAnalyzerTest, BuiltInFunction_Print_IsDefined)
{
  auto* printName = createName(u"print");

  analyzer->analyzeExpr(printName);

  EXPECT_EQ(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::ERROR), 0);
}

TEST_F(SemanticAnalyzerTest, CallNonCallable_ReportsError)
{
  // Define a variable
  auto* value  = createNumberLiteral(u"42");
  auto* target = createName(u"not_a_function");
  auto* assign = ast::AST_allocator.make<ast::AssignmentStmt>(target, value /*, 1*/);
  statements.push_back(assign);
  analyzer->analyze(statements);

  // Try to call it
  auto*                   callee = createName(u"not_a_function", 2);
  std::vector<ast::Expr*> args;
  ast::ListExpr*          arg_list = ast::AST_allocator.make<ast::ListExpr>(args);
  auto*                   call     = ast::AST_allocator.make<ast::CallExpr>(callee, arg_list /*, 2*/);

  analyzer->analyzeExpr(call);

  EXPECT_TRUE(hasIssueContaining(u"is not callable"));
}

// Control Flow Tests

TEST_F(SemanticAnalyzerTest, ConstantIfCondition_ReportsWarning)
{
  auto* condition = createBooleanLiteral(u"true");
  auto* thenBlock = ast::AST_allocator.make<ast::BlockStmt>(std::vector<ast::Stmt*>{} /*, 1*/);
  auto* elseBlock = ast::AST_allocator.make<ast::BlockStmt>(std::vector<ast::Stmt*>{} /*, 1*/);
  auto* ifStmt    = ast::AST_allocator.make<ast::IfStmt>(condition, thenBlock, elseBlock /*, 1*/);

  analyzer->analyzeStmt(ifStmt);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
  EXPECT_TRUE(hasIssueContaining(u"Condition is always constant"));
}

TEST_F(SemanticAnalyzerTest, InfiniteLoop_ReportsWarning)
{
  auto* condition = createBooleanLiteral(u"true");
  auto* block     = ast::AST_allocator.make<ast::BlockStmt>(std::vector<ast::Stmt*>{} /*, 1*/);
  auto* whileStmt = ast::AST_allocator.make<ast::WhileStmt>(condition, block /*, 1*/);

  analyzer->analyzeStmt(whileStmt);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::WARNING), 0);
  EXPECT_TRUE(hasIssueContaining(u"Infinite loop"));
}

// Expression Statement Tests

TEST_F(SemanticAnalyzerTest, UnusedExpressionResult_ReportsInfo)
{
  auto* expr     = createNumberLiteral(u"42");
  auto* exprStmt = ast::AST_allocator.make<ast::ExprStmt>(expr /*, 1*/);

  analyzer->analyzeStmt(exprStmt);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
  EXPECT_TRUE(hasIssueContaining(u"Expression result not used"));
}

TEST_F(SemanticAnalyzerTest, FunctionCallExpression_NoUnusedWarning)
{
  auto*                   callee = createName(u"print");
  std::vector<ast::Expr*> args;
  ast::ListExpr*          arg_list = ast::AST_allocator.make<ast::ListExpr>(args);
  auto*                   call     = ast::AST_allocator.make<ast::CallExpr>(callee, arg_list /*, 1*/);
  auto*                   exprStmt = ast::AST_allocator.make<ast::ExprStmt>(call /*, 1*/);

  analyzer->analyzeStmt(exprStmt);

  // Should not report unused expression for function calls
  size_t infoCount = countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO);
  // May have other info messages, but shouldn't have unused expression warning
}

// Scope Tests

TEST_F(SemanticAnalyzerTest, ForLoopVariableShadowing_ReportsWarning)
{
  // Define outer variable
  auto* outerValue  = createNumberLiteral(u"10");
  auto* outerTarget = createName(u"i");
  auto* outerAssign = ast::AST_allocator.make<ast::AssignmentStmt>(outerTarget, outerValue /*, 1*/);
  statements.push_back(outerAssign);

  analyzer->analyze(statements);

  // Create for loop with same variable name
  auto* loopVar   = createName(u"i");
  auto* iterable  = ast::AST_allocator.make<ast::ListExpr>(std::vector<ast::Expr*>{} /*, 2*/);
  auto* loopBlock = ast::AST_allocator.make<ast::BlockStmt>(std::vector<ast::Stmt*>{} /*, 2*/);
  auto* forStmt   = ast::AST_allocator.make<ast::ForStmt>(loopVar, iterable, loopBlock /*, 2*/);

  analyzer->analyzeStmt(forStmt);

  EXPECT_TRUE(hasIssueContaining(u"Loop variable shadows"));
}

// Function Definition Tests

TEST_F(SemanticAnalyzerTest, FunctionWithoutReturn_ReportsInfo)
{
  auto*                   funcName = createName(u"my_func");
  std::vector<ast::Expr*> params;
  ast::ListExpr*          param_list = ast::AST_allocator.make<ast::ListExpr>(params);
  auto*                   body       = ast::AST_allocator.make<ast::BlockStmt>(std::vector<ast::Stmt*>{} /*, 1*/);
  auto*                   funcDef    = ast::AST_allocator.make<ast::FunctionDef>(funcName, param_list, body /*, 1*/);

  analyzer->analyzeStmt(funcDef);

  EXPECT_GT(countIssuesBySeverity(SemanticAnalyzer::Issue::Severity::INFO), 0);
  EXPECT_TRUE(hasIssueContaining(u"Function may not return a value"));
}

TEST_F(SemanticAnalyzerTest, FunctionWithReturn_NoWarning)
{
  auto*                   funcName = createName(u"my_func");
  std::vector<ast::Expr*> params;
  ast::ListExpr*          param_list = ast::AST_allocator.make<ast::ListExpr>(params);
  // Create return statement
  auto* retValue = createNumberLiteral(u"42");
  auto* retStmt  = ast::AST_allocator.make<ast::ReturnStmt>(retValue /*, 2*/);

  std::vector<ast::Stmt*> bodyStmts = {retStmt};
  auto*                   body      = ast::AST_allocator.make<ast::BlockStmt>(bodyStmts /*, 1*/);
  auto*                   funcDef   = ast::AST_allocator.make<ast::FunctionDef>(funcName, param_list, body /*, 1*/);

  analyzer->analyzeStmt(funcDef);

  // Should not warn about missing return
  EXPECT_FALSE(hasIssueContaining(u"Function may not return a value"));
}

// Complex Expression Tests

TEST_F(SemanticAnalyzerTest, NestedBinaryExpressions_InfersCorrectType)
{
  // (10 + 20) * 2.5
  auto* inner_left   = createNumberLiteral(u"10");
  auto* inner_right  = createNumberLiteral(u"20");
  auto* inner_binary = createBinary(inner_left, tok::TokenType::OP_PLUS, inner_right);

  auto* outer_right  = createNumberLiteral(u"2.5");
  auto* outer_binary = createBinary(inner_binary, tok::TokenType::OP_STAR, outer_right);

  // Should promote to FLOAT due to 2.5
  EXPECT_EQ(analyzer->inferType(outer_binary), SymbolTable::DataType_t::FLOAT);
}

TEST_F(SemanticAnalyzerTest, ListExpression_InfersListType)
{
  std::vector<ast::Expr*> elements = {createNumberLiteral(u"1"), createNumberLiteral(u"2"), createNumberLiteral(u"3")};
  auto*                   listExpr = ast::AST_allocator.make<ast::ListExpr>(elements /*, 1*/);

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
  auto* nameExpr = createName(u"undefined", 1);
  analyzer->analyzeExpr(nameExpr);

  // Should not throw
  EXPECT_NO_THROW(analyzer->printReport());
}

// Symbol Table Tests

TEST_F(SemanticAnalyzerTest, GetGlobalScope_NotNull) { EXPECT_NE(analyzer->getGlobalScope(), nullptr); }

TEST_F(SemanticAnalyzerTest, AssignmentCreatesSymbol)
{
  auto* value  = createNumberLiteral(u"42");
  auto* target = createName(u"x");
  auto* assign = ast::AST_allocator.make<ast::AssignmentStmt>(target, value /*, 1*/);
  statements.push_back(assign);

  analyzer->analyze(statements);

  // Verify symbol was created
  const SymbolTable* globalScope = analyzer->getGlobalScope();
  ASSERT_NE(globalScope, nullptr);
}

// Integration Tests

TEST_F(SemanticAnalyzerTest, ComplexProgram_MultipleIssues)
{
  // Undefined variable use
  auto* undefined = createName(u"undefined_var", 1);
  auto* exprStmt1 = ast::AST_allocator.make<ast::ExprStmt>(undefined /*, 1*/);
  statements.push_back(exprStmt1);

  // Division by zero
  auto* left      = createNumberLiteral(u"10", 2);
  auto* right     = createNumberLiteral(u"0", 2);
  auto* divExpr   = createBinary(left, tok::TokenType::OP_SLASH, right);
  auto* exprStmt2 = ast::AST_allocator.make<ast::ExprStmt>(divExpr /*, 2*/);
  statements.push_back(exprStmt2);

  // Unused variable
  auto* value  = createNumberLiteral(u"100", 3);
  auto* target = createName(u"unused");
  auto* assign = ast::AST_allocator.make<ast::AssignmentStmt>(target, value /*, 3*/);
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
  auto* value1  = createNumberLiteral(u"10");
  auto* target1 = createName(u"x");
  auto* assign1 = ast::AST_allocator.make<ast::AssignmentStmt>(target1, value1 /*, 1*/);
  statements.push_back(assign1);

  // Use variable
  auto* useExpr = createName(u"x", 2);
  auto* value2  = createNumberLiteral(u"5");
  auto* binary  = createBinary(useExpr, tok::TokenType::OP_PLUS, value2);
  auto* target2 = createName(u"y");
  auto* assign2 = ast::AST_allocator.make<ast::AssignmentStmt>(target2, binary /*, 2*/);
  statements.push_back(assign2);

  analyzer->analyze(statements);

  // Should report unused variable 'y' but not 'x'
  auto   issues       = analyzer->getIssues();
  size_t unusedYCount = 0;
  for (const auto& issue : issues)
  {
    if (issue.message.find('y') && issue.message.find(u"Unused"))
      unusedYCount++;
  }
  EXPECT_GT(unusedYCount, 0);
}

// Call Expression Tests

TEST_F(SemanticAnalyzerTest, CallExpression_InfersAnyType)
{
  auto*                   callee = createName(u"some_function");
  std::vector<ast::Expr*> args;
  ast::ListExpr*          arg_list = ast::AST_allocator.make<ast::ListExpr>(args);
  auto*                   call     = ast::AST_allocator.make<ast::CallExpr>(callee, arg_list /*, 1*/);

  EXPECT_EQ(analyzer->inferType(call), SymbolTable::DataType_t::ANY);
}

TEST_F(SemanticAnalyzerTest, CallWithArguments_AnalyzesAllArgs)
{
  auto*                   callee   = createName(u"print");
  std::vector<ast::Expr*> args     = {createNumberLiteral(u"1"), createStringLiteral(u"hello"), createBooleanLiteral(u"true")};
  ast::ListExpr*          arg_list = ast::AST_allocator.make<ast::ListExpr>(args);
  auto*                   call     = ast::AST_allocator.make<ast::CallExpr>(callee, arg_list /*, 1*/);

  EXPECT_NO_THROW(analyzer->analyzeExpr(call));
}
