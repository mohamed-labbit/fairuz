#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <vector>
#define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
#include <boost/stacktrace.hpp>

#include "../include/input/file_manager.hpp"
#include "../include/lex/lexer.hpp"
#include "../include/parser/ast/ast.hpp"
#include "../include/parser/ast/printer.hpp"
#include "../include/parser/parser.hpp"

using StringType = mylang::StringType;

namespace {
std::filesystem::path parser_test_cases_dir()
{
  static const auto dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
  return dir;
}

StringType load_source(const std::string& filename)
{
  auto filepath = parser_test_cases_dir() / filename;
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) ADD_FAILURE() << "Failed to open test file: " << filepath;
  std::string utf8_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return utf8::utf8to16(utf8_contents);
}

}  // anonymous namespace

// Test Fixture
class ParserTest: public ::testing::Test
{
 public:
  void SetUp() override
  {
    // GTEST_SKIP() << "Parser tests temporarily disabled while lexer source handling is stabilized.";
    ASSERT_TRUE(std::filesystem::exists(parser_test_cases_dir())) << "Test cases directory not found: " << parser_test_cases_dir();
  }

  std::vector<mylang::lex::tok::Token> createTokens(std::initializer_list<mylang::lex::tok::TokenType> types)
  {
    mylang::lex::Lexer lexer;
    std::vector<mylang::lex::tok::Token> tokens;
    tokens.emplace_back(lexer.make_token(mylang::lex::tok::TokenType::BEGINMARKER, u"", 1, 1));
    int line = 1, col = 2;

    for (auto type : types)
    {
      tokens.emplace_back(lexer.make_token(type, u"", line, col));
      col++;
    }

    tokens.emplace_back(lexer.make_token(mylang::lex::tok::TokenType::ENDMARKER, u"", line, col));
    return tokens;
  }

  mylang::lex::tok::Token makeToken(mylang::lex::tok::TokenType type, const StringType& value = u"", int line = 1, int col = 1)
  {
    mylang::lex::Lexer lexer;
    return lexer.make_token(type, value, line, col);
  }

  std::ifstream openTestFile(const std::string& filename)
  {
    auto filepath = parser_test_cases_dir() / filename;
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) ADD_FAILURE() << "Failed to open test file: " << filepath;
    return file;
  }

  template<typename T>
  T* parseAndCast(mylang::parser::Parser& parser, mylang::parser::ast::Expr*& expr)
  {
    EXPECT_NE(expr, nullptr) << "Expression should not be null";
    if (!expr) return nullptr;
    T* casted = reinterpret_cast<T*>(expr);
    EXPECT_NE(casted, nullptr) << "Failed to cast to expected type";
    return casted;
  }
};

mylang::parser::ast::ASTPrinter AST_Printer;

// Literal Expression Tests
TEST_F(ParserTest, ParseNumberLiteral)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "number_literal.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parsePrimaryExpr();
  mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
  AST_Printer.print(literal);
  ASSERT_NE(literal, nullptr);
  EXPECT_EQ(literal->getType(), mylang::parser::ast::LiteralExpr::Type::NUMBER);
}

TEST_F(ParserTest, ParseStringLiteral)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "string_literal.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parsePrimaryExpr();
  mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
  AST_Printer.print(literal);
  ASSERT_NE(literal, nullptr);
  std::cout << static_cast<int>(literal->getType()) << std::endl;
  EXPECT_EQ(static_cast<int>(literal->getType()), static_cast<int>(mylang::parser::ast::LiteralExpr::Type::STRING));
}

TEST_F(ParserTest, ParseBooleanLiteralTrue)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "boolean_literal_true.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parsePrimaryExpr();
  mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
  AST_Printer.print(literal);
  ASSERT_NE(literal, nullptr);
  EXPECT_EQ(literal->getType(), mylang::parser::ast::LiteralExpr::Type::BOOLEAN);
}

TEST_F(ParserTest, ParseBooleanLiteralFalse)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "boolean_literal_false.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parsePrimaryExpr();
  mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
  AST_Printer.print(literal);
  ASSERT_NE(literal, nullptr);
  EXPECT_EQ(literal->getType(), mylang::parser::ast::LiteralExpr::Type::BOOLEAN);
}

TEST_F(ParserTest, ParseNoneLiteral)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "none_literal.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parsePrimaryExpr();
  mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
  AST_Printer.print(literal);
  ASSERT_NE(literal, nullptr);
  EXPECT_EQ(literal->getType(), mylang::parser::ast::LiteralExpr::Type::NONE);
}

TEST_F(ParserTest, ParseParenthesizedNumberLiteral)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "parenthesized_number.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parsePrimaryExpr();
  mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
  AST_Printer.print(literal);
  ASSERT_NE(literal, nullptr);
  EXPECT_EQ(literal->getType(), mylang::parser::ast::LiteralExpr::Type::NUMBER);
}

// ============================================================================
// Name/Identifier Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseIdentifier)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "identifier.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parsePrimaryExpr();
  mylang::parser::ast::NameExpr* name_expr = parseAndCast<mylang::parser::ast::NameExpr>(parser, expr);
  AST_Printer.print(name_expr);
  ASSERT_NE(name_expr, nullptr);
  EXPECT_EQ(name_expr->getValue(), u"المتنبي");
}

/*
TEST_F(ParserTest, ParseSimpleIdentifier)
{
    // Test with a simple ASCII identifier when supported
    auto tokens = createTokens({mylang::lex::tok::TokenType::NAME});
    // ...
}
*/

// Call Expression Tests
TEST_F(ParserTest, ParseCallExpressionNoArgs)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "call_expression.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parsePrimaryExpr();
  ASSERT_NE(expr, nullptr);
  mylang::parser::ast::CallExpr* call_expr = parseAndCast<mylang::parser::ast::CallExpr>(parser, expr);
  AST_Printer.print(call_expr);
  ASSERT_NE(call_expr, nullptr);
  ASSERT_NE(call_expr->getCallee(), nullptr);
  mylang::parser::ast::NameExpr* callee_name = call_expr->getCallee();
  ASSERT_NE(callee_name, nullptr);
  EXPECT_EQ(callee_name->getValue(), u"اطبع");
  EXPECT_TRUE(call_expr->getArgs().empty());
}

TEST_F(ParserTest, ParseCallExpressionWithOneArg)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "call_expression_with_one_argument.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parsePrimaryExpr();
  ASSERT_NE(expr, nullptr);
  mylang::parser::ast::CallExpr* call_expr = parseAndCast<mylang::parser::ast::CallExpr>(parser, expr);
  AST_Printer.print(call_expr);
  ASSERT_NE(call_expr, nullptr);
  ASSERT_NE(call_expr->getCallee(), nullptr);
  mylang::parser::ast::NameExpr* callee_name = call_expr->getCallee();
  ASSERT_NE(callee_name, nullptr);
  EXPECT_EQ(callee_name->getValue(), u"اطبع");
  // const std::vector<mylang::parser::ast::Expr*>& args = call_expr->getArgs();
  // EXPECT_FALSE(args.empty());
  /// @todo check for each argument and their order
}

TEST_F(ParserTest, ParseNestedCallExpression)
{
  /// @todo: Add test for nested calls like f(g(x))
  GTEST_SKIP() << "Nested call test not yet implemented";
}

// ============================================================================
// Binary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleAddition)
{
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "simple_addition.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  EXPECT_NE(expr, nullptr) << "Should parse expression";
  mylang::parser::ast::BinaryExpr* add_expr = dynamic_cast<mylang::parser::ast::BinaryExpr*>(expr);
  AST_Printer.print(add_expr);
  EXPECT_NE(add_expr, nullptr) << "Should cast expression";
  EXPECT_NE(add_expr->getLeft(), nullptr) << "Should get left hand side";
  EXPECT_NE(add_expr->getRight(), nullptr) << "should get right hand side";
  mylang::parser::ast::Expr* left_expr = add_expr->getLeft();
  mylang::parser::ast::Expr* right_expr = add_expr->getRight();
  mylang::lex::tok::TokenType operator_value = add_expr->getOperator();
  mylang::parser::ast::NameExpr* left_name = dynamic_cast<mylang::parser::ast::NameExpr*>(left_expr);
  mylang::parser::ast::NameExpr* right_name = dynamic_cast<mylang::parser::ast::NameExpr*>(right_expr);
  EXPECT_NE(left_name, nullptr) << "Should parse left hand side of expression";
  EXPECT_NE(right_name, nullptr) << "Should parse right hand side of expression";
  StringType expected_left_name = u"ا";
  StringType expected_right_name = u"ب";
  StringType left_name_value = left_name->getValue();
  StringType right_name_value = right_name->getValue();
  mylang::lex::tok::TokenType expected_operator_value = mylang::lex::tok::TokenType::OP_PLUS;
  EXPECT_EQ(left_name_value, expected_left_name) << "left hand side not parsed correctly";
  EXPECT_EQ(right_name_value, expected_right_name) << "right hand side not parsed correctly";
  EXPECT_EQ(operator_value, expected_operator_value) << "binary operator not parsed correctly";
}

// ============================================================================
// Complex Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseComplexExpression)
{
  GTEST_SKIP() << "ParseComplexExpression: not checked yet";
  // Test operator precedence: 2 + 3 * 4 should be 2 + (3 * 4)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "complex_expression.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Failed to parse complex expression";
  // Should be: BinaryExpr(2, +, BinaryExpr(3, *, 4))
  mylang::parser::ast::BinaryExpr* root = dynamic_cast<mylang::parser::ast::BinaryExpr*>(expr);
  ASSERT_NE(root, nullptr) << "Root should be a BinaryExpr";
  EXPECT_EQ(root->getOperator(), mylang::lex::tok::TokenType::OP_PLUS) << "Root operator should be OP_PLUS";
  // Check left side: should be literal 2
  mylang::parser::ast::Expr* left_expr = root->getLeft();
  ASSERT_NE(left_expr, nullptr) << "Left expression is null";
  mylang::parser::ast::LiteralExpr* left_lit = dynamic_cast<mylang::parser::ast::LiteralExpr*>(left_expr);
  ASSERT_NE(left_lit, nullptr) << "Left should be a LiteralExpr";
  EXPECT_EQ(left_lit->getType(), mylang::parser::ast::LiteralExpr::Type::NUMBER);
  EXPECT_EQ(left_lit->getValue(), u"2");
  // Check right side: should be BinaryExpr(3, *, 4)
  mylang::parser::ast::Expr* right_expr = root->getRight();
  ASSERT_NE(right_expr, nullptr) << "Right expression is null";
  mylang::parser::ast::BinaryExpr* right_binary = dynamic_cast<mylang::parser::ast::BinaryExpr*>(right_expr);
  ASSERT_NE(right_binary, nullptr) << "Right should be a BinaryExpr for multiplication";
  EXPECT_EQ(right_binary->getOperator(), mylang::lex::tok::TokenType::OP_STAR) << "Right operator should be OP_STAR";
  // Check multiplication operands
  mylang::parser::ast::Expr* mult_left = right_binary->getLeft();
  mylang::parser::ast::Expr* mult_right = right_binary->getRight();
  ASSERT_NE(mult_left, nullptr) << "Multiplication left operand is null";
  ASSERT_NE(mult_right, nullptr) << "Multiplication right operand is null";
  mylang::parser::ast::LiteralExpr* mult_left_lit = dynamic_cast<mylang::parser::ast::LiteralExpr*>(mult_left);
  mylang::parser::ast::LiteralExpr* mult_right_lit = dynamic_cast<mylang::parser::ast::LiteralExpr*>(mult_right);
  ASSERT_NE(mult_left_lit, nullptr) << "Multiplication left should be LiteralExpr";
  ASSERT_NE(mult_right_lit, nullptr) << "Multiplication right should be LiteralExpr";
  EXPECT_EQ(mult_left_lit->getValue(), u"3");
  EXPECT_EQ(mult_right_lit->getValue(), u"4");
  AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseNestedParentheses)
{
  GTEST_SKIP() << "ParseNestedParentheses: not checked yet";
  // Test: ((2 + 3) * 4)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "nested_parens.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Failed to parse nested parentheses expression";
  // Should be: BinaryExpr((2 + 3), *, 4)
  mylang::parser::ast::BinaryExpr* root = dynamic_cast<mylang::parser::ast::BinaryExpr*>(expr);
  ASSERT_NE(root, nullptr) << "Root should be a BinaryExpr";
  EXPECT_EQ(root->getOperator(), mylang::lex::tok::TokenType::OP_STAR);
  // Left should be (2 + 3)
  mylang::parser::ast::BinaryExpr* left_add = dynamic_cast<mylang::parser::ast::BinaryExpr*>(root->getLeft());
  ASSERT_NE(left_add, nullptr) << "Left should be addition expression";
  EXPECT_EQ(left_add->getOperator(), mylang::lex::tok::TokenType::OP_PLUS);
  AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedComparison)
{
  GTEST_SKIP() << "ParseChainedComparison: not checked yet";
  // Test: a < b < c (should parse as (a < b) < c due to left associativity)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "chained_comparison.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Failed to parse chained comparison";
  mylang::parser::ast::BinaryExpr* root = dynamic_cast<mylang::parser::ast::BinaryExpr*>(expr);
  ASSERT_NE(root, nullptr) << "Root should be BinaryExpr";
  EXPECT_EQ(root->getOperator(), mylang::lex::tok::TokenType::OP_LT);
  // Left should be another comparison
  mylang::parser::ast::BinaryExpr* left_comp = dynamic_cast<mylang::parser::ast::BinaryExpr*>(root->getLeft());
  ASSERT_NE(left_comp, nullptr) << "Left should be comparison expression";
  EXPECT_EQ(left_comp->getOperator(), mylang::lex::tok::TokenType::OP_LT);
  AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseLogicalExpression)
{
  GTEST_SKIP() << "ParseLogicalExpression: not checked yet";
  // Test: a and b or c (should be (a and b) or c)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "logical_expression.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Failed to parse logical expression";
  mylang::parser::ast::BinaryExpr* root = dynamic_cast<mylang::parser::ast::BinaryExpr*>(expr);
  ASSERT_NE(root, nullptr) << "Root should be BinaryExpr";
  EXPECT_EQ(root->getOperator(), mylang::lex::tok::TokenType::KW_OR) << "Root should be OR (lower precedence)";
  // Left should be (a and b)
  mylang::parser::ast::BinaryExpr* left_and = dynamic_cast<mylang::parser::ast::BinaryExpr*>(root->getLeft());
  ASSERT_NE(left_and, nullptr) << "Left should be AND expression";
  EXPECT_EQ(left_and->getOperator(), mylang::lex::tok::TokenType::KW_AND);
  AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnaryChain)
{
  GTEST_SKIP() << "ParseUnaryChain: not checked yet";
  // Test: --x (double negation)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "unary_chain.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Failed to parse unary chain";
  mylang::parser::ast::UnaryExpr* outer = dynamic_cast<mylang::parser::ast::UnaryExpr*>(expr);
  ASSERT_NE(outer, nullptr) << "Outer should be UnaryExpr";
  EXPECT_EQ(outer->getOperator(), mylang::lex::tok::TokenType::OP_MINUS);
  mylang::parser::ast::UnaryExpr* inner = dynamic_cast<mylang::parser::ast::UnaryExpr*>(outer->getOperand());
  ASSERT_NE(inner, nullptr) << "Inner should be UnaryExpr";
  EXPECT_EQ(inner->getOperator(), mylang::lex::tok::TokenType::OP_MINUS);
  mylang::parser::ast::NameExpr* name = dynamic_cast<mylang::parser::ast::NameExpr*>(inner->getOperand());
  ASSERT_NE(name, nullptr) << "Innermost should be NameExpr";
  AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseComplexFunctionCall)
{
  GTEST_SKIP() << "ParseComplexFunctionCall: not checked yet";
  // Test: func(a + b, c * d)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "complex_function_call.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Failed to parse complex function call";
  mylang::parser::ast::CallExpr* call = dynamic_cast<mylang::parser::ast::CallExpr*>(expr);
  ASSERT_NE(call, nullptr) << "Should be CallExpr";
  mylang::parser::ast::NameExpr* callee = call->getCallee();
  ASSERT_NE(callee, nullptr) << "Callee should not be null";
  EXPECT_EQ(callee->getValue(), u"func");
  mylang::parser::ast::ListExpr* args = call->getArgsAsListExpr();
  ASSERT_NE(args, nullptr) << "Args should not be null";
  const auto& arg_list = args->getElements();
  ASSERT_EQ(arg_list.size(), 2) << "Should have exactly 2 arguments";
  // First arg should be (a + b)
  mylang::parser::ast::BinaryExpr* arg1 = dynamic_cast<mylang::parser::ast::BinaryExpr*>(arg_list[0]);
  ASSERT_NE(arg1, nullptr) << "First arg should be BinaryExpr";
  EXPECT_EQ(arg1->getOperator(), mylang::lex::tok::TokenType::OP_PLUS);
  // Second arg should be (c * d)
  mylang::parser::ast::BinaryExpr* arg2 = dynamic_cast<mylang::parser::ast::BinaryExpr*>(arg_list[1]);
  ASSERT_NE(arg2, nullptr) << "Second arg should be BinaryExpr";
  EXPECT_EQ(arg2->getOperator(), mylang::lex::tok::TokenType::OP_STAR);
  AST_Printer.print(expr);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ParserTest, ParseInvalidSyntaxThrows)
{
  GTEST_SKIP() << "ParseInvalidSyntaxThrows: not checked yet";
  // Test: + + (invalid: operator without operands)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "invalid_syntax.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  // Parser should return nullptr or handle gracefully
  // Depending on implementation, might return nullptr or throw
  EXPECT_EQ(expr, nullptr) << "Parser should return nullptr for invalid syntax";
}

TEST_F(ParserTest, ParseMissingOperand)
{
  GTEST_SKIP() << "ParseMissingOperand: not checked yet";
  // Test: a + (missing right operand)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "missing_operand.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  // Should handle gracefully
  if (expr != nullptr)
  {
    // If it parsed something, verify it's not complete
    mylang::parser::ast::BinaryExpr* binary = dynamic_cast<mylang::parser::ast::BinaryExpr*>(expr);
    if (binary != nullptr)
      // Right side might be null or invalid
      EXPECT_TRUE(binary->getRight() == nullptr || dynamic_cast<mylang::parser::ast::NameExpr*>(binary->getRight()) == nullptr)
        << "Parser should not create valid expression with missing operand";
  }
}

TEST_F(ParserTest, ParseUnmatchedParenthesis)
{
  GTEST_SKIP() << "ParseUnmatchedParenthesis: not checked yet";
  // Test: (a + b (missing closing paren)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "unmatched_paren.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  // Should fail to parse or return incomplete expression
  EXPECT_TRUE(expr == nullptr || !parser.weDone()) << "Parser should detect unmatched parenthesis";
}

TEST_F(ParserTest, ParseExtraClosingParenthesis)
{
  GTEST_SKIP() << "ParseExtraClosingParenthesis: not checked yet";
  // Test: a + b) (extra closing paren)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "extra_paren.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse the valid part";
  // Verify there's still a ')' token left
  EXPECT_FALSE(parser.weDone()) << "Should have unparsed tokens remaining";
  EXPECT_TRUE(parser.check(mylang::lex::tok::TokenType::RPAREN)) << "Remaining token should be RPAREN";
}

TEST_F(ParserTest, ParseUnexpectedEOF)
{
  GTEST_SKIP() << "ParseUnexpectedEOF: not checked yet";
  // Test: a + b + (EOF in the middle of expression)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "unexpected_eof.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  // Parser should return what it can parse or nullptr
  if (expr != nullptr)
  {
    mylang::parser::ast::BinaryExpr* binary = dynamic_cast<mylang::parser::ast::BinaryExpr*>(expr);
    if (binary != nullptr)
      // If it's a binary expression, right side might be incomplete
      EXPECT_TRUE(binary->getRight() == nullptr) << "Right side should be null due to unexpected EOF";
  }
}

TEST_F(ParserTest, ParseInvalidOperatorSequence)
{
  GTEST_SKIP() << "ParseInvalidOperatorSequence: not checked yet";
  // Test: a ++ b (invalid operator sequence)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "invalid_operator_seq.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  // Should handle gracefully - might parse as a + (+b) or fail
  if (expr != nullptr)
  {
    AST_Printer.print(expr);
    // Verify structure makes sense even if semantically wrong
    mylang::parser::ast::BinaryExpr* binary = dynamic_cast<mylang::parser::ast::BinaryExpr*>(expr);
    if (binary != nullptr)
    {
      EXPECT_NE(binary->getLeft(), nullptr) << "Left operand should exist";
      EXPECT_NE(binary->getRight(), nullptr) << "Right operand should exist";
    }
  }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ParserTest, ParseEmptyInput)
{
  GTEST_SKIP() << "ParseEmptyInput: not checked yet";
  // Test: completely empty file
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "empty_input.txt");
  mylang::parser::Parser parser(&file_manager);
  EXPECT_TRUE(parser.weDone()) << "Parser should recognize empty input immediately";
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  EXPECT_EQ(expr, nullptr) << "Should return nullptr for empty input";
}

TEST_F(ParserTest, ParseWhitespaceOnly)
{
  GTEST_SKIP() << "ParseWhitespaceOnly: not checked yet";
  // Test: file with only whitespace and newlines
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "whitespace_only.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  EXPECT_EQ(expr, nullptr) << "Should return nullptr for whitespace-only input";
  EXPECT_TRUE(parser.weDone()) << "Should reach end marker after whitespace";
}

TEST_F(ParserTest, ParseSingleIdentifier)
{
  GTEST_SKIP() << "ParseSingleIdentifier: not checked yet";
  // Test: just "x"
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "single_identifier.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse single identifier";
  mylang::parser::ast::NameExpr* name = dynamic_cast<mylang::parser::ast::NameExpr*>(expr);
  ASSERT_NE(name, nullptr) << "Should be NameExpr";
  EXPECT_EQ(name->getValue(), u"x") << "Identifier value should be 'x'";
  EXPECT_TRUE(parser.weDone()) << "Should be at end after single identifier";
}

TEST_F(ParserTest, ParseVeryLongIdentifier)
{
  GTEST_SKIP() << "ParseVeryLongIdentifier: not checked yet";
  // Test: extremely long identifier (1000+ characters)
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "long_identifier.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse very long identifier";
  mylang::parser::ast::NameExpr* name = dynamic_cast<mylang::parser::ast::NameExpr*>(expr);
  ASSERT_NE(name, nullptr) << "Should be NameExpr";
  StringType value = name->getValue();
  EXPECT_GT(value.length(), 100) << "Identifier should be very long";
  EXPECT_LT(value.length(), 10000) << "Identifier should have reasonable upper bound";
  // Verify it's all valid identifier characters
  for (auto ch : value) EXPECT_TRUE(std::isalnum(ch) || ch == U'_' || ch > 127) << "All characters should be valid identifier chars";
}

TEST_F(ParserTest, ParseUnicodeIdentifiers)
{
  GTEST_SKIP() << "ParseUnicodeIdentifiers: not checked yet";
  // Test: Arabic identifiers like in SimpleAddition test
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "unicode_identifiers.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse Unicode identifiers";
  mylang::parser::ast::BinaryExpr* binary = dynamic_cast<mylang::parser::ast::BinaryExpr*>(expr);
  ASSERT_NE(binary, nullptr) << "Should be BinaryExpr";
  mylang::parser::ast::NameExpr* left = dynamic_cast<mylang::parser::ast::NameExpr*>(binary->getLeft());
  mylang::parser::ast::NameExpr* right = dynamic_cast<mylang::parser::ast::NameExpr*>(binary->getRight());
  ASSERT_NE(left, nullptr) << "Left should be NameExpr";
  ASSERT_NE(right, nullptr) << "Right should be NameExpr";
  // Verify Unicode preservation
  EXPECT_GT(left->getValue().length(), 0) << "Left identifier should not be empty";
  EXPECT_GT(right->getValue().length(), 0) << "Right identifier should not be empty";
  AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyList)
{
  GTEST_SKIP() << "ParseEmptyList: not checked yet";
  // Test: []
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "empty_list.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse empty list";
  mylang::parser::ast::ListExpr* list = dynamic_cast<mylang::parser::ast::ListExpr*>(expr);
  ASSERT_NE(list, nullptr) << "Should be ListExpr";
  EXPECT_EQ(list->getElements().size(), 0) << "List should be empty";
}

TEST_F(ParserTest, ParseEmptyTuple)
{
  GTEST_SKIP() << "ParseEmptyTuple: not checked yet";
  // Test: ()
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "empty_tuple.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse empty tuple";
  mylang::parser::ast::ListExpr* tuple = dynamic_cast<mylang::parser::ast::ListExpr*>(expr);
  ASSERT_NE(tuple, nullptr) << "Should be ListExpr (representing tuple)";
  EXPECT_EQ(tuple->getElements().size(), 0) << "Tuple should be empty";
}

TEST_F(ParserTest, ParseListWithTrailingComma)
{
  GTEST_SKIP() << "ParseListWithTrailingComma: not checked yet";
  // Test: [1, 2, 3,]
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "list_trailing_comma.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse list with trailing comma";
  mylang::parser::ast::ListExpr* list = dynamic_cast<mylang::parser::ast::ListExpr*>(expr);
  ASSERT_NE(list, nullptr) << "Should be ListExpr";
  EXPECT_EQ(list->getElements().size(), 3) << "Should have 3 elements despite trailing comma";
}

TEST_F(ParserTest, ParseNestedLists)
{
  GTEST_SKIP() << "ParseNestedLists: not checked yet";
  // Test: [[1, 2], [3, 4]]
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "nested_lists.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse nested lists";
  mylang::parser::ast::ListExpr* outer_list = dynamic_cast<mylang::parser::ast::ListExpr*>(expr);
  ASSERT_NE(outer_list, nullptr) << "Should be ListExpr";
  EXPECT_EQ(outer_list->getElements().size(), 2) << "Outer list should have 2 elements";
  // Check first inner list
  mylang::parser::ast::ListExpr* inner1 = dynamic_cast<mylang::parser::ast::ListExpr*>(outer_list->getElements()[0]);
  ASSERT_NE(inner1, nullptr) << "First element should be ListExpr";
  EXPECT_EQ(inner1->getElements().size(), 2) << "First inner list should have 2 elements";
  // Check second inner list
  mylang::parser::ast::ListExpr* inner2 = dynamic_cast<mylang::parser::ast::ListExpr*>(outer_list->getElements()[1]);
  ASSERT_NE(inner2, nullptr) << "Second element should be ListExpr";
  EXPECT_EQ(inner2->getElements().size(), 2) << "Second inner list should have 2 elements";
  AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseAssignment)
{
  GTEST_SKIP() << "ParseAssignment: not checked yet";
  // Test: x = 42
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "assignment.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse assignment";
  mylang::parser::ast::AssignmentExpr* assign = dynamic_cast<mylang::parser::ast::AssignmentExpr*>(expr);
  ASSERT_NE(assign, nullptr) << "Should be AssignmentExpr";
  mylang::parser::ast::NameExpr* target = assign->getTarget();
  ASSERT_NE(target, nullptr) << "Assignment target should not be null";
  EXPECT_EQ(target->getValue(), u"x") << "Target should be 'x'";
  mylang::parser::ast::LiteralExpr* value = dynamic_cast<mylang::parser::ast::LiteralExpr*>(assign->getValue());
  ASSERT_NE(value, nullptr) << "Value should be LiteralExpr";
  EXPECT_EQ(value->getValue(), u"42");
  AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedAssignment)
{
  GTEST_SKIP() << "ParseChainedAssignment: not checked yet";
  // Test: x = y = 5 (should be right associative: x = (y = 5))
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "chained_assignment.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse chained assignment";
  mylang::parser::ast::AssignmentExpr* outer = dynamic_cast<mylang::parser::ast::AssignmentExpr*>(expr);
  ASSERT_NE(outer, nullptr) << "Outer should be AssignmentExpr";
  EXPECT_EQ(outer->getTarget()->getValue(), u"x");
  mylang::parser::ast::AssignmentExpr* inner = dynamic_cast<mylang::parser::ast::AssignmentExpr*>(outer->getValue());
  ASSERT_NE(inner, nullptr) << "Inner value should be AssignmentExpr";
  EXPECT_EQ(inner->getTarget()->getValue(), u"y");
  AST_Printer.print(expr);
}

// ============================================================================
// Performance Tests (Optional)
// ============================================================================

TEST_F(ParserTest, DISABLED_ParseLargeFile)
{
  GTEST_SKIP() << "DISABLED_ParseLargeFile: not checked yet";
  // Disabled by default, enable for performance testing
  // Test: file with thousands of expressions
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "large_file.txt");
  mylang::parser::Parser parser(&file_manager);
  auto start = std::chrono::high_resolution_clock::now();
  int expr_count = 0;
  while (!parser.weDone())
  {
    mylang::parser::ast::Expr* expr = parser.parseExpression();
    if (expr != nullptr)
      expr_count++;
    else
      break;
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "Parsed " << expr_count << " expressions in " << duration.count() << "ms" << std::endl;
  EXPECT_GT(expr_count, 1000) << "Should parse many expressions";
  EXPECT_LT(duration.count(), 5000) << "Should complete in reasonable time";
}

TEST_F(ParserTest, DISABLED_ParseDeeplyNestedExpression)
{
  GTEST_SKIP() << "DISABLED_ParseDeeplyNestedExpression: not checked yet";
  // Test: ((((((((((x))))))))))  - 100+ levels deep
  mylang::input::FileManager file_manager(parser_test_cases_dir() / "deeply_nested.txt");
  mylang::parser::Parser parser(&file_manager);
  mylang::parser::ast::Expr* expr = parser.parseExpression();
  ASSERT_NE(expr, nullptr) << "Should parse deeply nested expression without stack overflow";
  // Unwrap to find the innermost expression
  int depth = 0;
  mylang::parser::ast::Expr* current = expr;
  while (current != nullptr && depth < 200)
  {
    // In a real implementation, parenthesized expressions might be transparent
    // or wrapped in some node type
    depth++;
    mylang::parser::ast::NameExpr* name = dynamic_cast<mylang::parser::ast::NameExpr*>(current);
    if (name != nullptr)
    {
      EXPECT_EQ(name->getValue(), u"x") << "Innermost should be 'x'";
      break;
    }
    // Try to unwrap if there's a wrapping node
    break;
  }
  EXPECT_GT(depth, 50) << "Should have significant nesting depth";
  AST_Printer.print(expr);
}