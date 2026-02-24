#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <vector>

#include "../../include//ast/ast.hpp"
#include "../../include//ast/printer.hpp"
#include "../../include/lex/file_manager.hpp"
#include "../../include/lex/lexer.hpp"
#include "../../include/parser/parser.hpp"
#include "../test_config.h"

using namespace mylang;
using StringRef = StringRef;

namespace {

std::filesystem::path parser_test_cases_dir()
{
    static auto const dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
    return dir;
}

StringRef load_source(std::string const& filename)
{
    auto filepath = parser_test_cases_dir() / filename;
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        ADD_FAILURE() << "Failed to open test file: " << filepath;

    std::string utf8_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return StringRef(utf8_contents.data());
}

} // anonymous namespace

// Test Fixture
class ParserTest : public ::testing::Test {
public:
    void SetUp() override
    {
        // GTEST_SKIP() << "Parser tests temporarily disabled while lexer source handling is stabilized.";
        ASSERT_TRUE(std::filesystem::exists(parser_test_cases_dir())) << "Test cases directory not found: " << parser_test_cases_dir();
    }

    template<typename T>
    T* parseAndCast(parser::Parser& parser, ast::Expr*& expr)
    {
        EXPECT_NE(expr, nullptr) << "Expression should not be null";
        if (!expr)
            return nullptr;

        T* casted = reinterpret_cast<T*>(expr);
        EXPECT_NE(casted, nullptr) << "Failed to cast to expected type";
        return casted;
    }
};

inline ast::ASTPrinter AST_Printer;

// Literal Expression Tests
TEST_F(ParserTest, ParseNumberLiteral)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "number_literal.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();
    ast::LiteralExpr* literal = dynamic_cast<ast::LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->getType(), ast::LiteralExpr::Type::INTEGER);
}

TEST_F(ParserTest, ParseStringLiteral)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "string_literal.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();
    ast::LiteralExpr* literal = dynamic_cast<ast::LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    std::cout << static_cast<int>(literal->getType()) << std::endl;
    EXPECT_EQ(static_cast<int>(literal->getType()), static_cast<int>(ast::LiteralExpr::Type::STRING));
}

TEST_F(ParserTest, ParseBooleanLiteralTrue)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "boolean_literal_true.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();
    ast::LiteralExpr* literal = dynamic_cast<ast::LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->getType(), ast::LiteralExpr::Type::BOOLEAN);
}

TEST_F(ParserTest, ParseBooleanLiteralFalse)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "boolean_literal_false.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();
    ast::LiteralExpr* literal = dynamic_cast<ast::LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->getType(), ast::LiteralExpr::Type::BOOLEAN);
}

TEST_F(ParserTest, ParseNoneLiteral)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "none_literal.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();
    ast::LiteralExpr* literal = dynamic_cast<ast::LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->getType(), ast::LiteralExpr::Type::NONE);
}

TEST_F(ParserTest, ParseParenthesizedNumberLiteral)
{
    // (x)
    lex::FileManager file_manager(parser_test_cases_dir() / "parenthesized_number.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();
    ast::LiteralExpr* literal = dynamic_cast<ast::LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->getType(), ast::LiteralExpr::Type::INTEGER);
}

// Name/Identifier Expression Tests

TEST_F(ParserTest, ParseIdentifier)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "identifier.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();
    ast::NameExpr* name_expr = dynamic_cast<ast::NameExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(name_expr);

    ASSERT_NE(name_expr, nullptr);
    EXPECT_EQ(name_expr->getValue(), "المتنبي");
}

/*
TEST_F(ParserTest, ParseSimpleIdentifier)
{
  GTEST_SKIP() << "not supported";
  // Test with a simple ASCII identifier when supported
  auto tokens = createTokens({tok::TokenType::NAME});
  // ...
}
*/

// Call Expression Tests
TEST_F(ParserTest, ParseCallExpressionNoArgs)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "call_expression.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr);

    ast::CallExpr* call_expr = dynamic_cast<ast::CallExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_expr);

    ASSERT_NE(call_expr, nullptr);
    ASSERT_NE(call_expr->getCallee(), nullptr);

    ast::NameExpr* callee_name = dynamic_cast<ast::NameExpr*>(call_expr->getCallee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->getValue(), "اطبع");
    EXPECT_TRUE(call_expr->getArgs().empty());
}

TEST_F(ParserTest, ParseCallExpressionWithOneArg)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "call_expression_with_one_argument.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr);

    ast::CallExpr* call_expr = dynamic_cast<ast::CallExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_expr);

    ASSERT_NE(call_expr, nullptr);
    ASSERT_NE(call_expr->getCallee(), nullptr);

    ast::NameExpr* callee_name = dynamic_cast<ast::NameExpr*>(call_expr->getCallee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->getValue(), "اطبع");

    std::vector<ast::Expr*> const& args = call_expr->getArgs();

    EXPECT_FALSE(args.empty());
    /// TODO: check for each argument and their order
}

TEST_F(ParserTest, ParseNestedCallExpression)
{
    /// nested call f(g(x))
    lex::FileManager file_manager(parser_test_cases_dir() / "nested_call_expression.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    // AST_Printer.print(expr);
    EXPECT_NE(expr, nullptr) << "Should parse expression";

    ast::CallExpr* call_expr = dynamic_cast<ast::CallExpr*>(expr);

    EXPECT_NE(call_expr, nullptr);
    if (test_config::print_ast)
        AST_Printer.print(call_expr);

    ast::Expr* callee = call_expr->getCallee();
    ast::ListExpr* args = call_expr->getArgsAsListExpr();

    EXPECT_NE(callee, nullptr) << "Callee should not be null";
    EXPECT_NE(args, nullptr) << "Arguments list should not be null";

    StringRef callee_name = dynamic_cast<ast::NameExpr*>(callee)->getValue();
    StringRef expected_callee_name = "ا";
    ast::CallExpr* nested_call_expr = dynamic_cast<ast::CallExpr*>(args->getElements()[0]);

    EXPECT_NE(nested_call_expr, nullptr);

    ast::Expr* nested_callee = nested_call_expr->getCallee();
    ast::ListExpr* nested_args = nested_call_expr->getArgsAsListExpr();

    EXPECT_NE(nested_callee, nullptr);
    EXPECT_NE(nested_args, nullptr);

    ast::NameExpr* nested_callee_name_expr = dynamic_cast<ast::NameExpr*>(nested_callee);

    EXPECT_NE(nested_callee_name_expr, nullptr);

    StringRef nested_callee_name = nested_callee_name_expr->getValue();
    StringRef expected_nested_callee_name = "ب";
    ast::Expr* only_arg_expr = (*nested_args)[0];
    ast::NameExpr* arg_name_expr = dynamic_cast<ast::NameExpr*>(only_arg_expr);

    EXPECT_NE(arg_name_expr, nullptr);

    StringRef arg_name = arg_name_expr->getValue();
    StringRef expected_arg_name = "د";

    EXPECT_EQ(callee_name, expected_callee_name);
    EXPECT_EQ(nested_callee_name, expected_nested_callee_name);
    EXPECT_EQ(arg_name, expected_arg_name);
}

// Binary Expression Tests

TEST_F(ParserTest, ParseSimpleAddition)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "simple_addition.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    EXPECT_NE(expr, nullptr) << "Should parse expression";

    ast::BinaryExpr* add_expr = dynamic_cast<ast::BinaryExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(add_expr);

    EXPECT_NE(add_expr, nullptr) << "Should cast expression";
    EXPECT_NE(add_expr->getLeft(), nullptr) << "Should get left hand side";
    EXPECT_NE(add_expr->getRight(), nullptr) << "should get right hand side";

    ast::Expr* left_expr = add_expr->getLeft();
    ast::Expr* right_expr = add_expr->getRight();
    tok::TokenType operator_value = add_expr->getOperator();
    ast::NameExpr* left_name = dynamic_cast<ast::NameExpr*>(left_expr);
    ast::NameExpr* right_name = dynamic_cast<ast::NameExpr*>(right_expr);

    EXPECT_NE(left_name, nullptr) << "Should parse left hand side of expression";
    EXPECT_NE(right_name, nullptr) << "Should parse right hand side of expression";

    StringRef expected_left_name = "ا";
    StringRef expected_right_name = "ب";
    StringRef left_name_value = left_name->getValue();
    StringRef right_name_value = right_name->getValue();
    tok::TokenType expected_operator_value = tok::TokenType::OP_PLUS;

    EXPECT_EQ(left_name_value, expected_left_name) << "left hand side not parsed correctly";
    EXPECT_EQ(right_name_value, expected_right_name) << "right hand side not parsed correctly";
    EXPECT_EQ(operator_value, expected_operator_value) << "binary operator not parsed correctly";
}

TEST_F(ParserTest, ParseSimpleMultiplication)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "simple_multiplication.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    EXPECT_NE(expr, nullptr) << "Should parse expression";

    ast::BinaryExpr* add_expr = dynamic_cast<ast::BinaryExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(add_expr);

    EXPECT_NE(add_expr, nullptr) << "Should cast expression";
    EXPECT_NE(add_expr->getLeft(), nullptr) << "Should get left hand side";
    EXPECT_NE(add_expr->getRight(), nullptr) << "should get right hand side";

    ast::Expr* left_expr = add_expr->getLeft();
    ast::Expr* right_expr = add_expr->getRight();
    tok::TokenType operator_value = add_expr->getOperator();
    ast::NameExpr* left_name = dynamic_cast<ast::NameExpr*>(left_expr);
    ast::NameExpr* right_name = dynamic_cast<ast::NameExpr*>(right_expr);

    EXPECT_NE(left_name, nullptr) << "Should parse left hand side of expression";
    EXPECT_NE(right_name, nullptr) << "Should parse right hand side of expression";

    StringRef expected_left_name = "ا";
    StringRef expected_right_name = "ب";
    StringRef left_name_value = left_name->getValue();
    StringRef right_name_value = right_name->getValue();
    tok::TokenType expected_operator_value = tok::TokenType::OP_STAR;

    EXPECT_EQ(left_name_value, expected_left_name) << "left hand side not parsed correctly";
    EXPECT_EQ(right_name_value, expected_right_name) << "right hand side not parsed correctly";
    EXPECT_EQ(operator_value, expected_operator_value) << "binary operator not parsed correctly";
}

TEST_F(ParserTest, ParseSimpleSubtraction)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "simple_subtraction.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    EXPECT_NE(expr, nullptr) << "Should parse expression";

    ast::BinaryExpr* add_expr = dynamic_cast<ast::BinaryExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(add_expr);

    EXPECT_NE(add_expr, nullptr) << "Should cast expression";
    EXPECT_NE(add_expr->getLeft(), nullptr) << "Should get left hand side";
    EXPECT_NE(add_expr->getRight(), nullptr) << "should get right hand side";

    ast::Expr* left_expr = add_expr->getLeft();
    ast::Expr* right_expr = add_expr->getRight();
    tok::TokenType operator_value = add_expr->getOperator();
    ast::NameExpr* left_name = dynamic_cast<ast::NameExpr*>(left_expr);
    ast::NameExpr* right_name = dynamic_cast<ast::NameExpr*>(right_expr);

    EXPECT_NE(left_name, nullptr) << "Should parse left hand side of expression";
    EXPECT_NE(right_name, nullptr) << "Should parse right hand side of expression";

    StringRef expected_left_name = "ا";
    StringRef expected_right_name = "ب";
    StringRef left_name_value = left_name->getValue();
    StringRef right_name_value = right_name->getValue();
    tok::TokenType expected_operator_value = tok::TokenType::OP_MINUS;

    EXPECT_EQ(left_name_value, expected_left_name) << "left hand side not parsed correctly";
    EXPECT_EQ(right_name_value, expected_right_name) << "right hand side not parsed correctly";
    EXPECT_EQ(operator_value, expected_operator_value) << "binary operator not parsed correctly";
}

TEST_F(ParserTest, ParseSimpleDivision)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "simple_division.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    EXPECT_NE(expr, nullptr) << "Should parse expression";

    ast::BinaryExpr* add_expr = dynamic_cast<ast::BinaryExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(add_expr);

    EXPECT_NE(add_expr, nullptr) << "Should cast expression";
    EXPECT_NE(add_expr->getLeft(), nullptr) << "Should get left hand side";
    EXPECT_NE(add_expr->getRight(), nullptr) << "should get right hand side";

    ast::Expr* left_expr = add_expr->getLeft();
    ast::Expr* right_expr = add_expr->getRight();
    tok::TokenType operator_value = add_expr->getOperator();
    ast::NameExpr* left_name = dynamic_cast<ast::NameExpr*>(left_expr);
    ast::NameExpr* right_name = dynamic_cast<ast::NameExpr*>(right_expr);

    EXPECT_NE(left_name, nullptr) << "Should parse left hand side of expression";
    EXPECT_NE(right_name, nullptr) << "Should parse right hand side of expression";

    StringRef expected_left_name = "ا";
    StringRef expected_right_name = "ب";
    StringRef left_name_value = left_name->getValue();
    StringRef right_name_value = right_name->getValue();
    tok::TokenType expected_operator_value = tok::TokenType::OP_SLASH;

    EXPECT_EQ(left_name_value, expected_left_name) << "left hand side not parsed correctly";
    EXPECT_EQ(right_name_value, expected_right_name) << "right hand side not parsed correctly";
    EXPECT_EQ(operator_value, expected_operator_value) << "binary operator not parsed correctly";
}

// Complex Expression Tests

TEST_F(ParserTest, ParseComplexExpression)
{
    // Test operator precedence: 2 + 3 * 4 should be 2 + (3 * 4)
    lex::FileManager file_manager(parser_test_cases_dir() / "complex_expression.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse complex expression";

    // Should be: BinaryExpr(2, +, BinaryExpr(3, *, 4))
    ast::BinaryExpr* root = dynamic_cast<ast::BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be a BinaryExpr";
    EXPECT_EQ(root->getOperator(), tok::TokenType::OP_PLUS) << "Root operator should be OP_PLUS";

    // Check left side: should be literal 2
    ast::Expr* left_expr = root->getLeft();

    ASSERT_NE(left_expr, nullptr) << "Left expression is null";

    ast::LiteralExpr* left_lit = dynamic_cast<ast::LiteralExpr*>(left_expr);

    ASSERT_NE(left_lit, nullptr) << "Left should be a LiteralExpr";
    EXPECT_EQ(left_lit->getType(), ast::LiteralExpr::Type::INTEGER);
    EXPECT_EQ(left_lit->getValue(), "2");

    // Check right side: should be BinaryExpr(3, *, 4)
    ast::Expr* right_expr = root->getRight();

    ASSERT_NE(right_expr, nullptr) << "Right expression is null";

    ast::BinaryExpr* right_binary = dynamic_cast<ast::BinaryExpr*>(right_expr);

    ASSERT_NE(right_binary, nullptr) << "Right should be a BinaryExpr for multiplication";
    EXPECT_EQ(right_binary->getOperator(), tok::TokenType::OP_STAR) << "Right operator should be OP_STAR";

    // Check multiplication operands
    ast::Expr* mult_left = right_binary->getLeft();
    ast::Expr* mult_right = right_binary->getRight();

    ASSERT_NE(mult_left, nullptr) << "Multiplication left operand is null";
    ASSERT_NE(mult_right, nullptr) << "Multiplication right operand is null";

    ast::LiteralExpr* mult_left_lit = dynamic_cast<ast::LiteralExpr*>(mult_left);
    ast::LiteralExpr* mult_right_lit = dynamic_cast<ast::LiteralExpr*>(mult_right);

    ASSERT_NE(mult_left_lit, nullptr) << "Multiplication left should be LiteralExpr";
    ASSERT_NE(mult_right_lit, nullptr) << "Multiplication right should be LiteralExpr";
    EXPECT_EQ(mult_left_lit->getValue(), "3");
    EXPECT_EQ(mult_right_lit->getValue(), "4");

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseNestedParentheses)
{
    // Test: ((2 + 3) * 4)
    lex::FileManager file_manager(parser_test_cases_dir() / "nested_parens.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse nested parentheses expression";

    // Should be: BinaryExpr((2 + 3), *, 4)
    ast::BinaryExpr* root = dynamic_cast<ast::BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be a BinaryExpr";
    EXPECT_EQ(root->getOperator(), tok::TokenType::OP_STAR);

    // Left should be (2 + 3)
    ast::BinaryExpr* left_add = dynamic_cast<ast::BinaryExpr*>(root->getLeft());

    ASSERT_NE(left_add, nullptr) << "Left should be addition expression";
    EXPECT_EQ(left_add->getOperator(), tok::TokenType::OP_PLUS);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedComparison)
{
    GTEST_SKIP() << "It isn't trivial if this feature should be in the lang at all";
    // Test: a < b < c (should parse as (a < b) < c due to left associativity)
    lex::FileManager file_manager(parser_test_cases_dir() / "chained_comparison.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse chained comparison";

    ast::BinaryExpr* root = dynamic_cast<ast::BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be BinaryExpr";
    EXPECT_EQ(root->getOperator(), tok::TokenType::OP_LT);

    // Left should be another comparison
    ast::BinaryExpr* left_comp = dynamic_cast<ast::BinaryExpr*>(root->getLeft());

    ASSERT_NE(left_comp, nullptr) << "Left should be comparison expression";
    EXPECT_EQ(left_comp->getOperator(), tok::TokenType::OP_LT);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseLogicalExpression)
{
    // Test: a and b or c (should be (a and b) or c)
    lex::FileManager file_manager(parser_test_cases_dir() / "logical_expression.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse logical expression";

    ast::BinaryExpr* root = dynamic_cast<ast::BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be BinaryExpr";
    EXPECT_EQ(root->getOperator(), tok::TokenType::KW_OR) << "Root should be OR (lower precedence)";

    // Left should be (a and b)
    ast::BinaryExpr* left_and = dynamic_cast<ast::BinaryExpr*>(root->getLeft());

    ASSERT_NE(left_and, nullptr) << "Left should be AND expression";
    EXPECT_EQ(left_and->getOperator(), tok::TokenType::KW_AND);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnaryChain)
{
    // Test: --x (double negation)
    lex::FileManager file_manager(parser_test_cases_dir() / "unary_chain.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse unary chain";

    ast::UnaryExpr* outer = dynamic_cast<ast::UnaryExpr*>(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be UnaryExpr";
    EXPECT_EQ(outer->getOperator(), tok::TokenType::OP_MINUS);

    ast::UnaryExpr* inner = dynamic_cast<ast::UnaryExpr*>(outer->getOperand());

    ASSERT_NE(inner, nullptr) << "Inner should be UnaryExpr";
    EXPECT_EQ(inner->getOperator(), tok::TokenType::OP_MINUS);

    ast::NameExpr* name = dynamic_cast<ast::NameExpr*>(inner->getOperand());

    ASSERT_NE(name, nullptr) << "Innermost should be NameExpr";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseComplexFunctionCall)
{
    // Test: func(a + b, c * d)
    lex::FileManager file_manager(parser_test_cases_dir() / "complex_function_call.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse complex function call";

    ast::CallExpr* call = dynamic_cast<ast::CallExpr*>(expr);

    ASSERT_NE(call, nullptr) << "Should be CallExpr";

    ast::NameExpr* callee = dynamic_cast<ast::NameExpr*>(call->getCallee());

    ASSERT_NE(callee, nullptr) << "Callee should not be null";
    EXPECT_EQ(callee->getValue(), "علم");

    ast::ListExpr* args = call->getArgsAsListExpr();

    ASSERT_NE(args, nullptr) << "Args should not be null";
    ASSERT_FALSE(args->isEmpty());

    std::vector<ast::Expr*> const& arg_list = args->getElements();

    ASSERT_EQ(arg_list.size(), 2) << "Should have exactly 2 arguments";

    // First arg should be (a + b)
    ast::BinaryExpr* arg1 = dynamic_cast<ast::BinaryExpr*>(arg_list[0]);

    ASSERT_NE(arg1, nullptr) << "First arg should be BinaryExpr";
    EXPECT_EQ(arg1->getOperator(), tok::TokenType::OP_PLUS);

    ast::Expr* first_lhs = arg1->getLeft();
    ast::Expr* first_rhs = arg1->getRight();

    ASSERT_NE(first_lhs, nullptr);
    ASSERT_NE(first_rhs, nullptr);

    ast::NameExpr* first_lhs_expr = dynamic_cast<ast::NameExpr*>(first_lhs);
    ast::NameExpr* first_rhs_expr = dynamic_cast<ast::NameExpr*>(first_rhs);

    StringRef first_lhs_name = first_lhs_expr->getValue();
    StringRef first_rhs_name = first_rhs_expr->getValue();

    EXPECT_EQ(first_lhs_name, "ا");
    EXPECT_EQ(first_rhs_name, "ب");

    // Second arg should be (c * d)
    ast::BinaryExpr* arg2 = dynamic_cast<ast::BinaryExpr*>(arg_list[1]);

    ASSERT_NE(arg2, nullptr) << "Second arg should be BinaryExpr";
    EXPECT_EQ(arg2->getOperator(), tok::TokenType::OP_STAR);

    ast::Expr* second_lhs = arg2->getLeft();
    ast::Expr* second_rhs = arg2->getRight();

    ASSERT_NE(second_lhs, nullptr);
    ASSERT_NE(second_rhs, nullptr);

    ast::NameExpr* second_lhs_expr = dynamic_cast<ast::NameExpr*>(second_lhs);
    ast::NameExpr* second_rhs_expr = dynamic_cast<ast::NameExpr*>(second_rhs);

    StringRef second_lhs_name = second_lhs_expr->getValue();
    StringRef second_rhs_name = second_rhs_expr->getValue();

    EXPECT_EQ(second_lhs_name, "ت");
    EXPECT_EQ(second_rhs_name, "ث");
    EXPECT_EQ(arg2->getOperator(), tok::TokenType::OP_STAR);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

// Error Handling Tests
TEST_F(ParserTest, ParseInvalidSyntaxThrows)
{
    // GTEST_SKIP() << "ParseInvalidSyntaxThrows: not checked yet";
    // Test: + + (invalid: operator without operands)
    lex::FileManager file_manager(parser_test_cases_dir() / "invalid_syntax.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    // Parser should return nullptr or handle gracefully
    // Depending on implementation, might return nullptr or throw
    EXPECT_EQ(expr, nullptr) << "Parser should return nullptr for invalid syntax";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseMissingOperand)
{
    // GTEST_SKIP() << "ParseMissingOperand: not checked yet";
    // Test: a + (missing right operand)
    lex::FileManager file_manager(parser_test_cases_dir() / "missing_operand.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    // Should handle gracefully
    if (expr != nullptr) {
        // If it parsed something, verify it's not complete
        ast::BinaryExpr* binary = dynamic_cast<ast::BinaryExpr*>(expr);
        if (binary != nullptr) // Right side might be null or invalid
            EXPECT_TRUE(binary->getRight() == nullptr || dynamic_cast<ast::NameExpr*>(binary->getRight()) == nullptr)
                << "Parser should not create valid expression with missing operand";
    }

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnmatchedParenthesis)
{
    // GTEST_SKIP() << "ParseUnmatchedParenthesis: not checked yet";
    // Test: (a + b (missing closing paren)
    lex::FileManager file_manager(parser_test_cases_dir() / "unmatched_paren.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    // Should fail to parse or return incomplete expression
    EXPECT_TRUE(expr == nullptr) << "Parser should detect unmatched parenthesis";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseExtraClosingParenthesis)
{
    // GTEST_SKIP() << "ParseExtraClosingParenthesis: not checked yet";
    // Test: a + b) (extra closing paren)
    lex::FileManager file_manager(parser_test_cases_dir() / "extra_paren.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse the valid part";
    EXPECT_FALSE(parser.weDone()) << "Should have unparsed tokens remaining";
    EXPECT_TRUE(parser.check(tok::TokenType::RPAREN)) << "Remaining token should be RPAREN"; // Verify there's still a ')' token left

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnexpectedEOF)
{
    // GTEST_SKIP() << "ParseUnexpectedEOF: not checked yet";
    // Test: a + b + (EOF in the middle of expression)
    lex::FileManager file_manager(parser_test_cases_dir() / "unexpected_eof.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    // Parser should return what it can parse or nullptr
    if (expr != nullptr) {
        ast::BinaryExpr* binary = dynamic_cast<ast::BinaryExpr*>(expr);
        if (binary != nullptr) // If it's a binary expression, right side might be incomplete
            EXPECT_TRUE(binary->getRight() == nullptr) << "Right side should be null due to unexpected EOF";
    }

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseInvalidOperatorSequence)
{
    // GTEST_SKIP() << "ParseInvalidOperatorSequence: not checked yet";
    // Test: a ++ b (invalid operator sequence)
    lex::FileManager file_manager(parser_test_cases_dir() / "invalid_operator_seq.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    // Should handle gracefully - might parse as a + (+b) or fail
    if (expr != nullptr) {
        if (test_config::print_ast)
            AST_Printer.print(expr);
        // Verify structure makes sense even if semantically wrong
        ast::BinaryExpr* binary = dynamic_cast<ast::BinaryExpr*>(expr);
        if (binary != nullptr) {
            EXPECT_NE(binary->getLeft(), nullptr) << "Left operand should exist";
            EXPECT_NE(binary->getRight(), nullptr) << "Right operand should exist";
        }
    }

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

// Edge Cases

TEST_F(ParserTest, ParseEmptyInput)
{
    // GTEST_SKIP() << "ParseEmptyInput: not checked yet";
    // Test: completely empty file
    lex::FileManager file_manager(parser_test_cases_dir() / "empty_input.txt");
    parser::Parser parser(&file_manager);

    EXPECT_TRUE(parser.weDone()) << "Parser should recognize empty input immediately";

    ast::Expr* expr = parser.parse();

    EXPECT_EQ(expr, nullptr) << "Should return nullptr for empty input";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseWhitespaceOnly)
{
    // GTEST_SKIP() << "ParseWhitespaceOnly: not checked yet";
    // Test: file with only whitespace and newlines
    lex::FileManager file_manager(parser_test_cases_dir() / "whitespace_only.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    EXPECT_EQ(expr, nullptr) << "Should return nullptr for whitespace-only input";

    // EXPECT_TRUE(parser.weDone()) << "Should reach end marker after whitespace";
    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseSingleIdentifier)
{
    // Test: just "x"
    lex::FileManager file_manager(parser_test_cases_dir() / "single_identifier.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse single identifier";

    ast::NameExpr* name = dynamic_cast<ast::NameExpr*>(expr);

    ASSERT_NE(name, nullptr) << "Should be NameExpr";
    EXPECT_EQ(name->getValue(), "ا") << "Identifier value should be 'x'";
    EXPECT_TRUE(parser.weDone()) << "Should be at end after single identifier";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseVeryLongIdentifier)
{
    // Test: extremely long identifier (1000+ characters)
    lex::FileManager file_manager(parser_test_cases_dir() / "long_identifier.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse very long identifier";

    ast::NameExpr* name = dynamic_cast<ast::NameExpr*>(expr);

    ASSERT_NE(name, nullptr) << "Should be NameExpr";

    StringRef value = name->getValue();

    EXPECT_GT(value.len(), 100) << "Identifier should be very long";
    EXPECT_LT(value.len(), 10000) << "Identifier should have reasonable upper bound";

    // Verify it's all valid identifier characters

    // for (size_t i = 0; i < value.len() && value[i]; ++i)
    // EXPECT_TRUE(::isalnum(util::encode_utf8_str(value[i])[0] || value[i] == '_' || value[i] > 127)) << "All characters should be valid identifier chars";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnicodeIdentifiers)
{
    // Test: Arabic identifiers like in SimpleAddition test
    lex::FileManager file_manager(parser_test_cases_dir() / "unicode_identifiers.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse Unicode identifiers";

    ast::BinaryExpr* binary = dynamic_cast<ast::BinaryExpr*>(expr);

    ASSERT_NE(binary, nullptr) << "Should be BinaryExpr";

    ast::NameExpr* left = dynamic_cast<ast::NameExpr*>(binary->getLeft());
    ast::NameExpr* right = dynamic_cast<ast::NameExpr*>(binary->getRight());

    ASSERT_NE(left, nullptr) << "Left should be NameExpr";
    ASSERT_NE(right, nullptr) << "Right should be NameExpr";

    // Verify Unicode preservation

    EXPECT_GT(left->getValue().len(), 0) << "Left identifier should not be empty";
    EXPECT_GT(right->getValue().len(), 0) << "Right identifier should not be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyList)
{
    // Test: []
    lex::FileManager file_manager(parser_test_cases_dir() / "empty_list.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse empty list";

    ast::ListExpr* list = dynamic_cast<ast::ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be ListExpr";
    EXPECT_EQ(list->getElements().size(), 0) << "List should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyTuple)
{
    // Test: ()
    lex::FileManager file_manager(parser_test_cases_dir() / "empty_tuple.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse empty tuple";

    ast::ListExpr* tuple = dynamic_cast<ast::ListExpr*>(expr);

    ASSERT_NE(tuple, nullptr) << "Should be ListExpr (representing tuple)";
    EXPECT_EQ(tuple->getElements().size(), 0) << "Tuple should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseListWithTrailingComma)
{
    // GTEST_SKIP() << "ParseListWithTrailingComma: not checked yet";
    // Test: [1, 2, 3,]
    lex::FileManager file_manager(parser_test_cases_dir() / "list_trailing_comma.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse list with trailing comma";

    ast::ListExpr* list = dynamic_cast<ast::ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be ListExpr";
    EXPECT_EQ(list->getElements().size(), 3) << "Should have 3 elements despite trailing comma";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseNestedLists)
{
    // GTEST_SKIP() << "ParseNestedLists: not checked yet";
    // Test: [[1, 2], [3, 4]]
    lex::FileManager file_manager(parser_test_cases_dir() / "nested_lists.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse nested lists";

    ast::ListExpr* outer_list = dynamic_cast<ast::ListExpr*>(expr);

    ASSERT_NE(outer_list, nullptr) << "Should be ListExpr";
    EXPECT_EQ(outer_list->getElements().size(), 2) << "Outer list should have 2 elements";

    // Check first inner list
    ast::ListExpr* inner1 = dynamic_cast<ast::ListExpr*>(outer_list->getElements()[0]);

    ASSERT_NE(inner1, nullptr) << "First element should be ListExpr";
    EXPECT_EQ(inner1->getElements().size(), 2) << "First inner list should have 2 elements";

    // Check second inner list
    ast::ListExpr* inner2 = dynamic_cast<ast::ListExpr*>(outer_list->getElements()[1]);

    ASSERT_NE(inner2, nullptr) << "Second element should be ListExpr";
    EXPECT_EQ(inner2->getElements().size(), 2) << "Second inner list should have 2 elements";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseAssignment)
{
    // Test: x := 42
    lex::FileManager file_manager(parser_test_cases_dir() / "assignment.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* node = parser.parse();
    ASSERT_NE(node, nullptr) << "Should parse assignment";

    ast::AssignmentExpr* assign = dynamic_cast<ast::AssignmentExpr*>(node);
    ASSERT_NE(assign, nullptr) << "Should be AssignmentExpr";

    ast::NameExpr* target = dynamic_cast<ast::NameExpr*>(assign->getTarget());

    ASSERT_NE(target, nullptr) << "Assignment target should not be null";
    EXPECT_EQ(target->getValue(), "ا") << "Target should be 'ا'";

    ast::LiteralExpr* value = dynamic_cast<ast::LiteralExpr*>(assign->getValue());

    ASSERT_NE(value, nullptr) << "Value should be LiteralExpr";
    EXPECT_EQ(value->getValue(), "42");

    if (test_config::print_ast)
        AST_Printer.print(assign);
}

TEST_F(ParserTest, ParseChainedAssignment)
{
    // Test: x := y := 5 (should be right associative: x := (y := 5))
    lex::FileManager file_manager(parser_test_cases_dir() / "chained_assignment.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse chained assignment";

    ast::AssignmentExpr* outer = dynamic_cast<ast::AssignmentExpr*>(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be AssignmentExpr";
    EXPECT_EQ(outer->getTarget()->getValue(), "ا");

    ast::AssignmentExpr* inner = dynamic_cast<ast::AssignmentExpr*>(outer->getValue());

    ASSERT_NE(inner, nullptr) << "Inner value should be AssignmentExpr";
    EXPECT_EQ(inner->getTarget()->getValue(), "ب");

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedAssignmentWithExpr)
{
    // Test: x := y := (a + b)
    // This should parse as: x := (y := (a + b))
    // Due to right-associativity of assignment
    lex::FileManager file_manager(parser_test_cases_dir() / "chained_assignment_with_expression.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse chained assignment";

    if (test_config::print_ast)
        AST_Printer.print(expr); // <-- DEBUG

    ast::AssignmentExpr* outer = dynamic_cast<ast::AssignmentExpr*>(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be AssignmentExpr";
    EXPECT_EQ(outer->getTarget()->getValue(), "ا") << "Outer target should be 'ا'";

    // Inner assignment: y = (a + b)
    ast::AssignmentExpr* inner = dynamic_cast<ast::AssignmentExpr*>(outer->getValue());

    ASSERT_NE(inner, nullptr) << "Inner value should be AssignmentExpr";
    EXPECT_EQ(inner->getTarget()->getValue(), "ب") << "Inner target should be 'ب'";

    // Binary expression: a + b
    ast::BinaryExpr* value = dynamic_cast<ast::BinaryExpr*>(inner->getValue());

    ASSERT_NE(value, nullptr) << "Should cast to BinaryExpr";
    ASSERT_NE(value->getLeft(), nullptr) << "Should get left hand side";
    ASSERT_NE(value->getRight(), nullptr) << "Should get right hand side";

    ast::Expr* left_expr = value->getLeft();
    ast::Expr* right_expr = value->getRight();
    tok::TokenType operator_value = value->getOperator();
    ast::NameExpr* left_name = dynamic_cast<ast::NameExpr*>(left_expr);
    ast::NameExpr* right_name = dynamic_cast<ast::NameExpr*>(right_expr);

    ASSERT_NE(left_name, nullptr) << "Should parse left hand side of expression";
    ASSERT_NE(right_name, nullptr) << "Should parse right hand side of expression";

    StringRef expected_left_name = "م";
    StringRef expected_right_name = "ل";
    StringRef left_name_value = left_name->getValue();
    StringRef right_name_value = right_name->getValue();
    tok::TokenType expected_operator_value = tok::TokenType::OP_PLUS;

    EXPECT_EQ(left_name_value, expected_left_name) << "Left hand side not parsed correctly";
    EXPECT_EQ(right_name_value, expected_right_name) << "Right hand side not parsed correctly";
    EXPECT_EQ(operator_value, expected_operator_value) << "Binary operator not parsed correctly";
}

// Performance Tests (Optional)

TEST_F(ParserTest, DISABLED_ParseLargeFile)
{
    GTEST_SKIP() << "DISABLED_ParseLargeFile: not checked yet";
    // Disabled by default, enable for performance testing
    // Test: file with thousands of expressions
    lex::FileManager file_manager(parser_test_cases_dir() / "large_file.txt");
    parser::Parser parser(&file_manager);
    auto start = std::chrono::high_resolution_clock::now();
    int expr_count = 0;

    while (!parser.weDone()) {
        ast::Expr* expr = parser.parse();
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

TEST_F(ParserTest, ParseDeeplyNestedExpression)
{
    // GTEST_SKIP() << "DISABLED_ParseDeeplyNestedExpression: not checked yet";
    // Test: ((((((((((x))))))))))  - 100+ levels deep
    lex::FileManager file_manager(parser_test_cases_dir() / "deeply_nested.txt");
    parser::Parser parser(&file_manager);
    ast::Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse deeply nested expression without stack overflow";

    // Unwrap to find the innermost expression
    /*
    int                        depth   = 0;
    ast::Expr* current = expr;
    while (current != nullptr && depth < 200)
    {
      // In a real implementation, parenthesized expressions might be transparent
      // or wrapped in some node type
      depth++;
      ast::NameExpr* name = dynamic_cast<ast::NameExpr*>(current);
      if (name != nullptr)
      {
        EXPECT_EQ(name->getValue(), "x") << "Innermost should be 'x'";
        break;
      }
      // Try to unwrap if there's a wrapping node
      break;
    }
    EXPECT_GT(depth, 50) << "Should have significant nesting depth";
    */

    if (test_config::print_ast) {
        AST_Printer.print(expr);
    }
}

TEST_F(ParserTest, ParseWhileLoop)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "while_loop.txt");
    parser::Parser parser(&file_manager);
    ast::Stmt* stmt = parser.parseWhileStmt();
    ASSERT_NE(stmt, nullptr) << "Should parse while loop statement";

    if (test_config::print_ast) {
        AST_Printer.print(stmt);
    }

    ast::WhileStmt* while_stmt = dynamic_cast<ast::WhileStmt*>(stmt);
    ASSERT_NE(while_stmt, nullptr) << "Should parse while loop statement";

    // Check condition
    ast::BinaryExpr* condition_expr = dynamic_cast<ast::BinaryExpr*>(while_stmt->getCondition());
    ASSERT_NE(condition_expr, nullptr) << "Should parse while loop condition expression";

    // Check condition operands: left is Name, right is Literal
    ast::NameExpr* left = dynamic_cast<ast::NameExpr*>(condition_expr->getLeft());
    ast::LiteralExpr* right = dynamic_cast<ast::LiteralExpr*>(condition_expr->getRight());

    ASSERT_NE(left, nullptr) << "Left operand should be a NameExpr";
    ASSERT_NE(right, nullptr) << "Right operand should be a LiteralExpr";
    EXPECT_EQ(left->getValue(), "شيء");
    EXPECT_EQ(right->getValue(), "صحيح");
    EXPECT_EQ(condition_expr->getOperator(), tok::TokenType::OP_EQ);

    // Check body (using getBody() instead of getBlock())
    ast::BlockStmt* block = dynamic_cast<ast::BlockStmt*>(while_stmt->getBlock());
    ASSERT_NE(block, nullptr) << "Should parse while loop body block";

    // Check block has statements
    ASSERT_GT(block->getStatements().size(), 0) << "Block should have at least one statement";

    // Check assignment statement (wrapped in ExprStmt)
    ast::ExprStmt* expr_stmt = dynamic_cast<ast::ExprStmt*>(block->getStatements()[0]);
    ASSERT_NE(expr_stmt, nullptr) << "First statement should be an expression statement";

    ast::AssignmentExpr* assign_expr = dynamic_cast<ast::AssignmentExpr*>(expr_stmt->getExpr());
    ASSERT_NE(assign_expr, nullptr) << "Expression should be an assignment";

    // Check assignment: target is Name, value is Literal
    ast::NameExpr* target = dynamic_cast<ast::NameExpr*>(assign_expr->getTarget());
    ast::LiteralExpr* value = dynamic_cast<ast::LiteralExpr*>(assign_expr->getValue());

    ASSERT_NE(target, nullptr) << "Assignment target should be a NameExpr";
    ASSERT_NE(value, nullptr) << "Assignment value should be a LiteralExpr";
    EXPECT_EQ(target->getValue(), "بسبسمياو");
    EXPECT_EQ(value->getValue(), "خطا");
}

TEST_F(ParserTest, ParseComplexeIfStatement)
{
    lex::FileManager file_manager(parser_test_cases_dir() / "complexe_if_statement.txt");
    parser::Parser parser(&file_manager);

    ast::Stmt* stmt = parser.parseIfStmt();
    ASSERT_NE(stmt, nullptr) << "Should parse if statement";

    if (test_config::print_ast) {
        AST_Printer.print(stmt);
    }

    ast::IfStmt* if_stmt = dynamic_cast<ast::IfStmt*>(stmt);
    ASSERT_NE(if_stmt, nullptr) << "Conversion to if statement failed";

    ast::BinaryExpr* first_condition_expr = dynamic_cast<ast::BinaryExpr*>(if_stmt->getCondition());
    ASSERT_NE(first_condition_expr, nullptr);

    EXPECT_EQ(dynamic_cast<ast::NameExpr*>(first_condition_expr->getLeft())->getValue(), "شيء");
    EXPECT_EQ(dynamic_cast<ast::LiteralExpr*>(first_condition_expr->getRight())->getValue(), "صحيح");
    EXPECT_EQ(first_condition_expr->getOperator(), tok::TokenType::OP_NEQ);

    ast::WhileStmt* while_stmt = dynamic_cast<ast::WhileStmt*>(if_stmt->getThenBlock()->getStatements()[0]);
    ASSERT_NE(while_stmt, nullptr) << "Should parse while loop statement";

    // Check condition
    ast::BinaryExpr* condition_expr = dynamic_cast<ast::BinaryExpr*>(while_stmt->getCondition());
    ASSERT_NE(condition_expr, nullptr) << "Should parse while loop condition expression";

    // Check condition operands: left is Name, right is Literal
    ast::NameExpr* left = dynamic_cast<ast::NameExpr*>(condition_expr->getLeft());
    ast::LiteralExpr* right = dynamic_cast<ast::LiteralExpr*>(condition_expr->getRight());

    ASSERT_NE(left, nullptr) << "Left operand should be a NameExpr";
    ASSERT_NE(right, nullptr) << "Right operand should be a LiteralExpr";
    EXPECT_EQ(left->getValue(), "شيء");
    EXPECT_EQ(right->getValue(), "صحيح");
    EXPECT_EQ(condition_expr->getOperator(), tok::TokenType::OP_EQ);

    // Check body (using getBody() instead of getBlock())
    ast::BlockStmt* block = dynamic_cast<ast::BlockStmt*>(while_stmt->getBlock());
    ASSERT_NE(block, nullptr) << "Should parse while loop body block";

    // Check block has statements
    ASSERT_GT(block->getStatements().size(), 0) << "Block should have at least one statement";

    // Check assignment statement (wrapped in ExprStmt)
    ast::ExprStmt* expr_stmt = dynamic_cast<ast::ExprStmt*>(block->getStatements()[0]);
    ASSERT_NE(expr_stmt, nullptr) << "First statement should be an expression statement";

    ast::AssignmentExpr* assign_expr = dynamic_cast<ast::AssignmentExpr*>(expr_stmt->getExpr());
    ASSERT_NE(assign_expr, nullptr) << "Expression should be an assignment";

    // Check assignment: target is Name, value is Literal
    ast::NameExpr* target = dynamic_cast<ast::NameExpr*>(assign_expr->getTarget());
    ast::LiteralExpr* value = dynamic_cast<ast::LiteralExpr*>(assign_expr->getValue());

    ASSERT_NE(target, nullptr) << "Assignment target should be a NameExpr";
    ASSERT_NE(value, nullptr) << "Assignment value should be a LiteralExpr";
    EXPECT_EQ(target->getValue(), "بسبسمياو");
    EXPECT_EQ(value->getValue(), "خطا");
}
