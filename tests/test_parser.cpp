#include "../fairuz/ast_printer.hpp"
#include "../fairuz/parser.hpp"
#include "test_config.h"

#include <fstream>
#include <gtest/gtest.h>
#include <iostream>

using namespace fairuz;
using namespace fairuz::parser;
using namespace fairuz::lex;

namespace {

std::filesystem::path parser_test_cases_dir()
{
    static auto const dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
    return dir;
}

} // namespace

class ParserTest : public ::testing::Test {
public:
    void SetUp() override
    {
        diagnostic::reset();
        ASSERT_TRUE(std::filesystem::exists(parser_test_cases_dir())) << "Test cases directory not found: " << parser_test_cases_dir();
    }

    template<typename T>
    T* parse_and_cast(AST::Fa_Expr*& expr)
    {
        EXPECT_NE(expr, nullptr) << "Expression should not be null";
        if (!expr)
            return nullptr;

        T* casted = reinterpret_cast<T*>(expr);
        EXPECT_NE(casted, nullptr) << "Failed to cast to expected type";
        return casted;
    }

    template<typename T>
    T* as(AST::Fa_Stmt* node)
    {
        T* casted = dynamic_cast<T*>(node);
        EXPECT_NE(casted, nullptr);
        return casted;
    }

    template<typename T>
    T* as(AST::Fa_Expr* node)
    {
        T* casted = dynamic_cast<T*>(node);
        EXPECT_NE(casted, nullptr);
        return casted;
    }

    void TearDown() override
    {
        if (diagnostic::has_errors() || diagnostic::m_warning_count() > 0)
            diagnostic::dump();
    }
};

inline AST::ASTPrinter AST_Printer;

TEST_F(ParserTest, ParseLiteral)
{
    Fa_FileManager file_manager_0(parser_test_cases_dir() / "number_literal.fa");
    Fa_FileManager file_manager_1(parser_test_cases_dir() / "string_literal.fa");
    Fa_FileManager file_manager_2(parser_test_cases_dir() / "boolean_literal_true.fa");
    Fa_FileManager file_manager_3(parser_test_cases_dir() / "boolean_literal_false.fa");

    Fa_Parser parser_0(&file_manager_0);
    Fa_Parser parser_1(&file_manager_1);
    Fa_Parser parser_2(&file_manager_2);
    Fa_Parser parser_3(&file_manager_3);

    EXPECT_EQ(dynamic_cast<AST::Fa_LiteralExpr*>(parser_0.parse().value())->get_type(), AST::Fa_LiteralExpr::Type::INTEGER) << "Should parse integer literal";
    EXPECT_EQ(dynamic_cast<AST::Fa_LiteralExpr*>(parser_1.parse().value())->get_type(), AST::Fa_LiteralExpr::Type::STRING) << "Should parse string literal";
    EXPECT_EQ(dynamic_cast<AST::Fa_LiteralExpr*>(parser_2.parse().value())->get_type(), AST::Fa_LiteralExpr::Type::BOOLEAN) << "Should parse true bool literal";
    EXPECT_EQ(dynamic_cast<AST::Fa_LiteralExpr*>(parser_3.parse().value())->get_type(), AST::Fa_LiteralExpr::Type::BOOLEAN) << "Should parse false bool literal";
}

TEST_F(ParserTest, ParseNoneLiteral)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "none_literal.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();
    AST::Fa_LiteralExpr* literal = dynamic_cast<AST::Fa_LiteralExpr*>(m_expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->get_type(), AST::Fa_LiteralExpr::Type::NIL);
}

TEST_F(ParserTest, ParseParenthesizedNumberLiteral)
{
    // (x)
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "parenthesized_number.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();
    AST::Fa_LiteralExpr* literal = dynamic_cast<AST::Fa_LiteralExpr*>(m_expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->get_type(), AST::Fa_LiteralExpr::Type::INTEGER);
}

TEST_F(ParserTest, ParseIdentifier)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "identifier.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();
    AST::Fa_NameExpr* name_fa_expr = dynamic_cast<AST::Fa_NameExpr*>(m_expr);

    if (test_config::print_ast)
        AST_Printer.print(name_fa_expr);

    ASSERT_NE(name_fa_expr, nullptr);
    EXPECT_EQ(name_fa_expr->get_value(), "المتنبي");
}

TEST_F(ParserTest, ParseCallExpressionNoArgs)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "call_expression.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr);

    AST::Fa_CallExpr* call_fa_expr = dynamic_cast<AST::Fa_CallExpr*>(m_expr);
    if (test_config::print_ast)
        AST_Printer.print(call_fa_expr);

    ASSERT_NE(call_fa_expr, nullptr);
    ASSERT_NE(call_fa_expr->get_callee(), nullptr);

    AST::Fa_NameExpr* callee_name = dynamic_cast<AST::Fa_NameExpr*>(call_fa_expr->get_callee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->get_value(), "اطبع");
    EXPECT_TRUE(call_fa_expr->get_args().empty());
}

TEST_F(ParserTest, ParseCallExpressionWithOneArg)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "call_expression_with_one_argument.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr);

    AST::Fa_CallExpr* call_expr = dynamic_cast<AST::Fa_CallExpr*>(m_expr);
    if (test_config::print_ast)
        AST_Printer.print(call_expr);

    ASSERT_NE(call_expr, nullptr);
    ASSERT_NE(call_expr->get_callee(), nullptr);

    AST::Fa_NameExpr* callee_name = dynamic_cast<AST::Fa_NameExpr*>(call_expr->get_callee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->get_value(), "اطبع");

    auto const& m_args = call_expr->get_args();

    EXPECT_FALSE(m_args.empty());
    /// TODO: check for each argument and their order
}

TEST_F(ParserTest, ParseNestedCallExpression)
{
    // f(g(x))
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "nested_call_expression.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_CallExpr* outer_call = as<AST::Fa_CallExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(outer_call);

    EXPECT_EQ(as<AST::Fa_NameExpr>(outer_call->get_callee())->get_value(), "ا");

    AST::Fa_CallExpr* inner_call = as<AST::Fa_CallExpr>(outer_call->get_args_as_list_expr()->get_elements()[0]);

    EXPECT_EQ(as<AST::Fa_NameExpr>(inner_call->get_callee())->get_value(), "ب");
    EXPECT_EQ(as<AST::Fa_NameExpr>(inner_call->get_args_as_list_expr()->get_elements()[0])->get_value(), "د");
}

TEST_F(ParserTest, ParseSimpleAddition)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "simple_addition.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_BinaryExpr* bin = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_operator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(as<AST::Fa_NameExpr>(bin->get_left())->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(bin->get_right())->get_value(), "ب");
}

TEST_F(ParserTest, ParseSimpleMultiplication)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "simple_multiplication.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_BinaryExpr* bin = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_operator(), AST::Fa_BinaryOp::OP_MUL);
    EXPECT_EQ(as<AST::Fa_NameExpr>(bin->get_left())->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(bin->get_right())->get_value(), "ب");
}

TEST_F(ParserTest, ParseSimpleSubtraction)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "simple_subtraction.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_BinaryExpr* bin = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_operator(), AST::Fa_BinaryOp::OP_SUB);

    AST::Fa_NameExpr* lhs = as<AST::Fa_NameExpr>(bin->get_left());
    AST::Fa_NameExpr* rhs = as<AST::Fa_NameExpr>(bin->get_right());

    EXPECT_EQ(lhs->get_value(), "ا");
    EXPECT_EQ(rhs->get_value(), "ب");
}

TEST_F(ParserTest, ParseSimpleDivision)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "simple_division.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_BinaryExpr* bin = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_operator(), AST::Fa_BinaryOp::OP_DIV);

    AST::Fa_NameExpr* lhs = as<AST::Fa_NameExpr>(bin->get_left());
    AST::Fa_NameExpr* rhs = as<AST::Fa_NameExpr>(bin->get_right());

    EXPECT_EQ(lhs->get_value(), "ا");
    EXPECT_EQ(rhs->get_value(), "ب");
}

TEST_F(ParserTest, ParseComplexExpression)
{
    // 2 + 3 * 4  →  2 + (3 * 4)
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "complex_expression.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_BinaryExpr* root = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(root);

    EXPECT_EQ(root->get_operator(), AST::Fa_BinaryOp::OP_ADD);

    AST::Fa_LiteralExpr* m_left = as<AST::Fa_LiteralExpr>(root->get_left());
    EXPECT_EQ(m_left->get_type(), AST::Fa_LiteralExpr::Type::INTEGER);
    EXPECT_EQ(m_left->is_number(), 2);

    AST::Fa_BinaryExpr* mul = as<AST::Fa_BinaryExpr>(root->get_right());
    EXPECT_EQ(mul->get_operator(), AST::Fa_BinaryOp::OP_MUL);

    EXPECT_EQ(as<AST::Fa_LiteralExpr>(mul->get_left())->is_number(), 3);
    EXPECT_EQ(as<AST::Fa_LiteralExpr>(mul->get_right())->is_number(), 4);
}

TEST_F(ParserTest, ParseNestedParentheses)
{
    // Test: ((2 + 3) * 4)
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "nested_parens.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Failed to parse nested parentheses expression";

    // Should be: AST::Fa_BinaryExpr((2 + 3), *, 4)
    AST::Fa_BinaryExpr* root = dynamic_cast<AST::Fa_BinaryExpr*>(m_expr);

    ASSERT_NE(root, nullptr) << "Root should be a AST::Fa_BinaryExpr";
    EXPECT_EQ(root->get_operator(), AST::Fa_BinaryOp::OP_MUL);

    // Left should be (2 + 3)
    AST::Fa_BinaryExpr* left_add = dynamic_cast<AST::Fa_BinaryExpr*>(root->get_left());

    ASSERT_NE(left_add, nullptr) << "Left should be addition expression";
    EXPECT_EQ(left_add->get_operator(), AST::Fa_BinaryOp::OP_ADD);

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseChainedComparison)
{
    GTEST_SKIP() << "It isn't trivial if this feature should be in the lang at all";
    // Test: a < b < c (should parse as (a < b) < c due to left associativity)
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "chained_comparison.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Failed to parse chained comparison";

    AST::Fa_BinaryExpr* root = dynamic_cast<AST::Fa_BinaryExpr*>(m_expr);

    ASSERT_NE(root, nullptr) << "Root should be AST::Fa_BinaryExpr";
    EXPECT_EQ(root->get_operator(), AST::Fa_BinaryOp::OP_LT);

    // Left should be another comparison
    AST::Fa_BinaryExpr* left_comp = dynamic_cast<AST::Fa_BinaryExpr*>(root->get_left());

    ASSERT_NE(left_comp, nullptr) << "Left should be comparison expression";
    EXPECT_EQ(left_comp->get_operator(), AST::Fa_BinaryOp::OP_LT);

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseLogicalExpression)
{
    // Test: a and b or c (should be (a and b) or c)
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "logical_expression.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Failed to parse logical expression";

    AST::Fa_BinaryExpr* root = dynamic_cast<AST::Fa_BinaryExpr*>(m_expr);

    ASSERT_NE(root, nullptr) << "Root should be AST::Fa_BinaryExpr";
    EXPECT_EQ(root->get_operator(), AST::Fa_BinaryOp::OP_OR) << "Root should be OR (lower precedence)";

    // Left should be (a and b)
    AST::Fa_BinaryExpr* left_and = dynamic_cast<AST::Fa_BinaryExpr*>(root->get_left());

    ASSERT_NE(left_and, nullptr) << "Left should be AND expression";
    EXPECT_EQ(left_and->get_operator(), AST::Fa_BinaryOp::OP_AND);

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseUnaryChain)
{
    // Test: --x (f64 negation)
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "unary_chain.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Failed to parse unary chain";

    AST::Fa_UnaryExpr* outer = dynamic_cast<AST::Fa_UnaryExpr*>(m_expr);

    ASSERT_NE(outer, nullptr) << "Outer should be AST::Fa_UnaryExpr";
    EXPECT_EQ(outer->get_operator(), AST::Fa_UnaryOp::OP_NEG);

    AST::Fa_UnaryExpr* inner = dynamic_cast<AST::Fa_UnaryExpr*>(outer->get_operand());

    ASSERT_NE(inner, nullptr) << "Inner should be AST::Fa_UnaryExpr";
    EXPECT_EQ(inner->get_operator(), AST::Fa_UnaryOp::OP_NEG);

    AST::Fa_NameExpr* m_name = dynamic_cast<AST::Fa_NameExpr*>(inner->get_operand());

    ASSERT_NE(m_name, nullptr) << "Innermost should be AST::Fa_NameExpr";

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseComplexFunctionCall)
{
    // func(a + b, c * d)
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "complex_function_call.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_CallExpr* call = as<AST::Fa_CallExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(call);

    EXPECT_EQ(as<AST::Fa_NameExpr>(call->get_callee())->get_value(), "علم");

    AST::Fa_ListExpr* m_args = call->get_args_as_list_expr();
    ASSERT_NE(m_args, nullptr);
    ASSERT_FALSE(m_args->is_empty());

    auto const& arg_list = m_args->get_elements();
    ASSERT_EQ(arg_list.size(), 2);

    AST::Fa_BinaryExpr* arg1 = as<AST::Fa_BinaryExpr>(arg_list[0]);
    EXPECT_EQ(arg1->get_operator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(as<AST::Fa_NameExpr>(arg1->get_left())->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(arg1->get_right())->get_value(), "ب");

    AST::Fa_BinaryExpr* arg2 = as<AST::Fa_BinaryExpr>(arg_list[1]);
    EXPECT_EQ(arg2->get_operator(), AST::Fa_BinaryOp::OP_MUL);
    EXPECT_EQ(as<AST::Fa_NameExpr>(arg2->get_left())->get_value(), "ت");
    EXPECT_EQ(as<AST::Fa_NameExpr>(arg2->get_right())->get_value(), "ث");
}

TEST_F(ParserTest, ParseInvalidSyntaxThrows)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "invalid_syntax.fa");
    Fa_Parser parser(&m_file_manager);
    auto m_expr = parser.parse();

    EXPECT_TRUE(m_expr.has_error()) << "Fa_Parser should return error for invalid syntax";
}

TEST_F(ParserTest, ParseMissingOperand)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "missing_operand.fa");
    Fa_Parser parser(&m_file_manager);
    auto m_expr = parser.parse();

    EXPECT_TRUE(m_expr.has_error()) << "Fa_Parser should not create valid expression with missing operand";
}

TEST_F(ParserTest, ParseUnmatchedParenthesis)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "unmatched_paren.fa");
    Fa_Parser parser(&m_file_manager);
    auto m_expr = parser.parse();

    EXPECT_TRUE(m_expr.has_error()) << "Fa_Parser should detect unmatched parenthesis";
}

TEST_F(ParserTest, ParseExtraClosingParenthesis)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "extra_paren.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse the valid part";
    EXPECT_FALSE(parser.we_done()) << "Should have unparsed tokens remaining";
    EXPECT_TRUE(parser.check(tok::Fa_TokenType::RPAREN)) << "Remaining token should be RPAREN";

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseUnexpectedEOF)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "unexpected_eof.fa");
    Fa_Parser parser(&m_file_manager);
    auto m_expr = parser.parse();

    EXPECT_TRUE(m_expr.has_error());
}

TEST_F(ParserTest, ParseInvalidOperatorSequence)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "invalid_operator_seq.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    if (m_expr != nullptr) {
        if (test_config::print_ast)
            AST_Printer.print(m_expr);
        AST::Fa_BinaryExpr* binary = dynamic_cast<AST::Fa_BinaryExpr*>(m_expr);
        if (binary != nullptr) {
            EXPECT_NE(binary->get_left(), nullptr) << "Left operand should exist";
            EXPECT_NE(binary->get_right(), nullptr) << "Right operand should exist";
        }
    }

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseEmptyInput)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "empty_input.fa");
    Fa_Parser parser(&m_file_manager);

    EXPECT_TRUE(parser.we_done()) << "Fa_Parser should recognize empty input immediately";

    auto m_expr = parser.parse();

    EXPECT_TRUE(m_expr.has_error());
}

TEST_F(ParserTest, ParseWhitespaceOnly)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "whitespace_only.fa");
    Fa_Parser parser(&m_file_manager);
    auto m_expr = parser.parse();

    EXPECT_TRUE(m_expr.has_error());
}

TEST_F(ParserTest, ParseSingleIdentifier)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "single_identifier.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse single identifier";

    AST::Fa_NameExpr* m_name = dynamic_cast<AST::Fa_NameExpr*>(m_expr);

    ASSERT_NE(m_name, nullptr) << "Should be AST::Fa_NameExpr";
    EXPECT_EQ(m_name->get_value(), "ا") << "Identifier value should be 'x'";
    EXPECT_TRUE(parser.we_done()) << "Should be at end after single identifier";

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseVeryLongIdentifier)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "long_identifier.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse very long identifier";

    AST::Fa_NameExpr* m_name = dynamic_cast<AST::Fa_NameExpr*>(m_expr);

    ASSERT_NE(m_name, nullptr) << "Should be AST::Fa_NameExpr";

    Fa_StringRef m_value = m_name->get_value();

    EXPECT_GT(m_value.len(), 100) << "Identifier should be very long";
    EXPECT_LT(m_value.len(), 10000) << "Identifier should have reasonable upper bound";

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseUnicodeIdentifiers)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "unicode_identifiers.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse Unicode identifiers";

    AST::Fa_BinaryExpr* binary = dynamic_cast<AST::Fa_BinaryExpr*>(m_expr);

    ASSERT_NE(binary, nullptr) << "Should be AST::Fa_BinaryExpr";

    AST::Fa_NameExpr* m_left = dynamic_cast<AST::Fa_NameExpr*>(binary->get_left());
    AST::Fa_NameExpr* m_right = dynamic_cast<AST::Fa_NameExpr*>(binary->get_right());

    ASSERT_NE(m_left, nullptr) << "Left should be AST::Fa_NameExpr";
    ASSERT_NE(m_right, nullptr) << "Right should be AST::Fa_NameExpr";

    EXPECT_GT(m_left->get_value().len(), 0) << "Left identifier should not be empty";
    EXPECT_GT(m_right->get_value().len(), 0) << "Right identifier should not be empty";

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseEmptyList)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "empty_list.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse empty list";

    AST::Fa_ListExpr* list = dynamic_cast<AST::Fa_ListExpr*>(m_expr);

    ASSERT_NE(list, nullptr) << "Should be AST::Fa_ListExpr";
    EXPECT_EQ(list->get_elements().size(), 0) << "List should be empty";

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseEmptyTuple)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "empty_tuple.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse empty tuple";

    AST::Fa_ListExpr* tuple = dynamic_cast<AST::Fa_ListExpr*>(m_expr);

    ASSERT_NE(tuple, nullptr) << "Should be AST::Fa_ListExpr (representing tuple)";
    EXPECT_EQ(tuple->get_elements().size(), 0) << "Tuple should be empty";

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseListWithTrailingComma)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "list_trailing_comma.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse list with trailing comma";

    AST::Fa_ListExpr* list = dynamic_cast<AST::Fa_ListExpr*>(m_expr);

    ASSERT_NE(list, nullptr) << "Should be AST::Fa_ListExpr";
    EXPECT_EQ(list->get_elements().size(), 3) << "Should have 3 elements despite trailing comma";

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseNestedLists)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "nested_lists.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse nested lists";

    AST::Fa_ListExpr* outer_list = dynamic_cast<AST::Fa_ListExpr*>(m_expr);

    ASSERT_NE(outer_list, nullptr) << "Should be AST::Fa_ListExpr";
    EXPECT_EQ(outer_list->get_elements().size(), 2) << "Outer list should have 2 elements";

    AST::Fa_ListExpr* inner1 = dynamic_cast<AST::Fa_ListExpr*>(outer_list->get_elements()[0]);

    ASSERT_NE(inner1, nullptr) << "First element should be AST::Fa_ListExpr";
    EXPECT_EQ(inner1->get_elements().size(), 2) << "First inner list should have 2 elements";

    AST::Fa_ListExpr* inner2 = dynamic_cast<AST::Fa_ListExpr*>(outer_list->get_elements()[1]);

    ASSERT_NE(inner2, nullptr) << "Second element should be AST::Fa_ListExpr";
    EXPECT_EQ(inner2->get_elements().size(), 2) << "Second inner list should have 2 elements";

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseAssignment)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "assignment.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* node = parser.parse().value();
    ASSERT_NE(node, nullptr) << "Should parse assignment";

    AST::Fa_AssignmentExpr* assign = dynamic_cast<AST::Fa_AssignmentExpr*>(node);
    ASSERT_NE(assign, nullptr) << "Should be Fa_AssignmentExpr";

    AST::Fa_NameExpr* m_target = dynamic_cast<AST::Fa_NameExpr*>(assign->get_target());

    ASSERT_NE(m_target, nullptr) << "Assignment target should not be null";
    EXPECT_EQ(m_target->get_value(), "ا") << "Target should be 'ا'";

    AST::Fa_LiteralExpr* m_value = dynamic_cast<AST::Fa_LiteralExpr*>(assign->get_value());

    ASSERT_NE(m_value, nullptr) << "Fa_Value should be AST::Fa_LiteralExpr";
    EXPECT_EQ(m_value->is_number(), 42);

    if (test_config::print_ast)
        AST_Printer.print(assign);
}

TEST_F(ParserTest, ParseChainedAssignment)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "chained_assignment.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse chained assignment";

    AST::Fa_AssignmentExpr* outer = dynamic_cast<AST::Fa_AssignmentExpr*>(m_expr);

    ASSERT_NE(outer, nullptr) << "Outer should be Fa_AssignmentExpr";
    EXPECT_EQ(static_cast<AST::Fa_NameExpr*>(outer->get_target())->get_value(), "ا");

    AST::Fa_AssignmentExpr* inner = dynamic_cast<AST::Fa_AssignmentExpr*>(outer->get_value());

    ASSERT_NE(inner, nullptr) << "Inner value should be Fa_AssignmentExpr";
    EXPECT_EQ(static_cast<AST::Fa_NameExpr*>(inner->get_target())->get_value(), "ب");

    if (test_config::print_ast)
        AST_Printer.print(m_expr);
}

TEST_F(ParserTest, ParseChainedAssignmentWithExpr)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "chained_assignment_with_expression.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_AssignmentExpr* outer = as<AST::Fa_AssignmentExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(outer);

    AST::Fa_AssignmentExpr* inner = as<AST::Fa_AssignmentExpr>(outer->get_value());
    AST::Fa_BinaryExpr* binary = as<AST::Fa_BinaryExpr>(inner->get_value());

    EXPECT_EQ(static_cast<AST::Fa_NameExpr*>(outer->get_target())->get_value(), "ا");
    EXPECT_EQ(static_cast<AST::Fa_NameExpr*>(inner->get_target())->get_value(), "ب");
    EXPECT_EQ(binary->get_operator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(as<AST::Fa_NameExpr>(binary->get_left())->get_value(), "م");
    EXPECT_EQ(as<AST::Fa_NameExpr>(binary->get_right())->get_value(), "ل");
}

TEST_F(ParserTest, DISABLED_ParseLargeFile)
{
    GTEST_SKIP() << "DISABLED_ParseLargeFile: not checked yet";
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "large_file.fa");
    Fa_Parser parser(&m_file_manager);
    auto start = std::chrono::high_resolution_clock::now();
    int Fa_Expr_count = 0;

    while (!parser.we_done()) {
        AST::Fa_Expr* m_expr = parser.parse().value();
        if (m_expr != nullptr)
            Fa_Expr_count += 1;
        else
            break;
    }

    auto m_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - start);
    std::cout << "Parsed " << Fa_Expr_count << " expressions in " << duration.count() << "ms" << std::endl;

    EXPECT_GT(Fa_Expr_count, 1000) << "Should parse many expressions";
    EXPECT_LT(duration.count(), 5000) << "Should complete in reasonable time";
}

TEST_F(ParserTest, ParseDeeplyNestedExpression)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "deeply_nested.fa");
    Fa_Parser parser(&m_file_manager);
    AST::Fa_Expr* m_expr = parser.parse().value();

    ASSERT_NE(m_expr, nullptr) << "Should parse deeply nested expression without stack overflow";

    if (test_config::print_ast) {
        AST_Printer.print(m_expr);
    }
}

TEST_F(ParserTest, ParseWhileLoop)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "while_loop.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_WhileStmt* while_stmt = as<AST::Fa_WhileStmt>(parser.parse_while_stmt().value());

    if (test_config::print_ast)
        AST_Printer.print(while_stmt);

    AST::Fa_BinaryExpr* cond = as<AST::Fa_BinaryExpr>(while_stmt->get_condition());
    AST::Fa_BlockStmt* block = as<AST::Fa_BlockStmt>(while_stmt->get_body());
    AST::Fa_AssignmentExpr* assign = as<AST::Fa_AssignmentExpr>(as<AST::Fa_ExprStmt>(block->get_statements()[0])->get_expr());

    EXPECT_EQ(as<AST::Fa_NameExpr>(cond->get_left())->get_value(), "شيء");
    EXPECT_TRUE(as<AST::Fa_LiteralExpr>(cond->get_right())->get_bool());
    EXPECT_EQ(cond->get_operator(), AST::Fa_BinaryOp::OP_EQ);
    ASSERT_FALSE(block->get_statements().empty());
    EXPECT_EQ(as<AST::Fa_NameExpr>(assign->get_target())->get_value(), "بسبسمياو");
    EXPECT_FALSE(as<AST::Fa_LiteralExpr>(assign->get_value())->get_bool());
}

TEST_F(ParserTest, ParseForLoop)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "for_loop.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_ForStmt* for_stmt = as<AST::Fa_ForStmt>(parser.parse_for_stmt().value());

    if (test_config::print_ast)
        AST_Printer.print(for_stmt);

    EXPECT_EQ(for_stmt->get_target()->get_value(), "عنصر");
    EXPECT_EQ(as<AST::Fa_NameExpr>(for_stmt->get_iter())->get_value(), "عناصر");
    auto* block = as<AST::Fa_BlockStmt>(for_stmt->get_body());
    ASSERT_EQ(block->get_statements().size(), 1u);
    auto* expr_stmt = as<AST::Fa_ExprStmt>(block->get_statements()[0]);
    auto* assign = as<AST::Fa_AssignmentExpr>(expr_stmt->get_expr());
    EXPECT_EQ(as<AST::Fa_NameExpr>(assign->get_target())->get_value(), "اجمع");
    EXPECT_EQ(as<AST::Fa_NameExpr>(assign->get_value())->get_value(), "عنصر");
}

TEST_F(ParserTest, ParseBreakStatement)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "break_stmt.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_BreakStmt* break_stmt = as<AST::Fa_BreakStmt>(parser.parse_break_stmt().value());
    ASSERT_NE(break_stmt, nullptr);
}

TEST_F(ParserTest, ParseContinueStatement)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "continue_stmt.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_ContinueStmt* continue_stmt = as<AST::Fa_ContinueStmt>(parser.parse_continue_stmt().value());
    ASSERT_NE(continue_stmt, nullptr);
}

TEST_F(ParserTest, ParseComplexeIfStatement)
{
    Fa_FileManager m_file_manager(parser_test_cases_dir() / "complexe_if_statement.fa");
    Fa_Parser parser(&m_file_manager);

    AST::Fa_IfStmt* if_stmt = as<AST::Fa_IfStmt>(parser.parse_if_stmt().value());

    if (test_config::print_ast)
        AST_Printer.print(if_stmt);

    AST::Fa_BinaryExpr* cond = as<AST::Fa_BinaryExpr>(if_stmt->get_condition());
    // the while statement is wrapped in a block inside the else clause
    AST::Fa_WhileStmt* while_stmt = as<AST::Fa_WhileStmt>(dynamic_cast<AST::Fa_BlockStmt*>(if_stmt->get_then())->get_statements()[0]);
    AST::Fa_BinaryExpr* while_cond = as<AST::Fa_BinaryExpr>(while_stmt->get_condition());
    AST::Fa_BlockStmt* block = as<AST::Fa_BlockStmt>(while_stmt->get_body());
    AST::Fa_AssignmentExpr* assign = as<AST::Fa_AssignmentExpr>(as<AST::Fa_ExprStmt>(block->get_statements()[0])->get_expr());

    EXPECT_EQ(as<AST::Fa_NameExpr>(cond->get_left())->get_value(), "شيء");
    EXPECT_TRUE(as<AST::Fa_LiteralExpr>(cond->get_right())->get_bool());
    EXPECT_EQ(cond->get_operator(), AST::Fa_BinaryOp::OP_NEQ);
    EXPECT_EQ(as<AST::Fa_NameExpr>(while_cond->get_left())->get_value(), "شيء");
    EXPECT_TRUE(as<AST::Fa_LiteralExpr>(while_cond->get_right())->get_bool());
    EXPECT_EQ(while_cond->get_operator(), AST::Fa_BinaryOp::OP_EQ);
    ASSERT_FALSE(block->get_statements().empty());
    EXPECT_EQ(as<AST::Fa_NameExpr>(assign->get_target())->get_value(), "بسبسمياو");
    EXPECT_FALSE(as<AST::Fa_LiteralExpr>(assign->get_value())->get_bool());
}

TEST_F(ParserTest, ParseAugmentedAssignmentPlus)
{
    // a += b -> a := a + b
    Fa_FileManager file_manager(parser_test_cases_dir() / "augmented_assign_plus.fa");
    Fa_Parser parser(&file_manager);

    auto assign_expr = as<AST::Fa_AssignmentExpr>(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = as<AST::Fa_BinaryExpr>(assign_expr->get_value());

    EXPECT_EQ(as<AST::Fa_NameExpr>(target)->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_ADD);
}

TEST_F(ParserTest, ParseAugmentedAssignmentMinus)
{
    // a -= b -> a := a - b
    Fa_FileManager file_manager(parser_test_cases_dir() / "augmented_assign_minus.fa");
    Fa_Parser parser(&file_manager);

    auto assign_expr = as<AST::Fa_AssignmentExpr>(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = as<AST::Fa_BinaryExpr>(assign_expr->get_value());

    EXPECT_EQ(as<AST::Fa_NameExpr>(target)->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_SUB);
}

TEST_F(ParserTest, ParseAugmentedAssignmentTimes)
{
    // a *= b -> a := a * b
    Fa_FileManager file_manager(parser_test_cases_dir() / "augmented_assign_times.fa");
    Fa_Parser parser(&file_manager);

    auto assign_expr = as<AST::Fa_AssignmentExpr>(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = as<AST::Fa_BinaryExpr>(assign_expr->get_value());

    EXPECT_EQ(as<AST::Fa_NameExpr>(target)->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_MUL);
}

TEST_F(ParserTest, ParseAugmentedAssignmentDiv)
{
    // a /= b -> a := a / b
    Fa_FileManager file_manager(parser_test_cases_dir() / "augmented_assign_div.fa");
    Fa_Parser parser(&file_manager);

    auto assign_expr = as<AST::Fa_AssignmentExpr>(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = as<AST::Fa_BinaryExpr>(assign_expr->get_value());

    EXPECT_EQ(as<AST::Fa_NameExpr>(target)->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_DIV);
}

TEST_F(ParserTest, ParseAugmentedAssignmentMod)
{
    // a %= b -> a := a % b
    Fa_FileManager file_manager(parser_test_cases_dir() / "augmented_assign_mod.fa");
    Fa_Parser parser(&file_manager);

    auto assign_expr = as<AST::Fa_AssignmentExpr>(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = as<AST::Fa_BinaryExpr>(assign_expr->get_value());

    EXPECT_EQ(as<AST::Fa_NameExpr>(target)->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_MOD);
}

TEST_F(ParserTest, ParseClassMethodPreservesMetadataAndMemberInit)
{
    auto path = std::filesystem::temp_directory_path() / "fairuz_class_parser_test.fa";
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.good());
        out << std::string("نوع Point:\n"
                           "    دالة make(x, y):\n"
                           "        .x := x\n"
                           "        .y := y\n");
    }

    Fa_FileManager fm(path);
    Fa_Parser parser(&fm);
    Fa_Array<AST::Fa_Stmt*> program = parser.parse_program();

    ASSERT_EQ(program.size(), 1u);

    auto* class_def = as<AST::Fa_ClassDef>(program[0]);
    ASSERT_NE(class_def, nullptr);

    auto* class_name = as<AST::Fa_NameExpr>(class_def->get_name());
    EXPECT_EQ(class_name->get_value(), "Point");

    ASSERT_EQ(class_def->get_members().size(), 2u);
    EXPECT_EQ(as<AST::Fa_NameExpr>(class_def->get_members()[0])->get_value(), "x");
    EXPECT_EQ(as<AST::Fa_NameExpr>(class_def->get_members()[1])->get_value(), "y");

    ASSERT_EQ(class_def->get_methods().size(), 1u);
    auto* method = as<AST::Fa_FunctionDef>(class_def->get_methods()[0]);
    EXPECT_EQ(method->get_name()->get_value(), "make");
    ASSERT_EQ(method->get_parameters().size(), 2u);
    EXPECT_EQ(as<AST::Fa_NameExpr>(method->get_parameters()[0])->get_value(), "x");
    EXPECT_EQ(as<AST::Fa_NameExpr>(method->get_parameters()[1])->get_value(), "y");

    auto* body = as<AST::Fa_BlockStmt>(method->get_body());
    ASSERT_EQ(body->get_statements().size(), 2u);

    auto* first_stmt = as<AST::Fa_ExprStmt>(body->get_statements()[0]);
    auto* first_assign = as<AST::Fa_AssignmentExpr>(first_stmt->get_expr());
    auto* first_target = as<AST::Fa_IndexExpr>(first_assign->get_target());

    EXPECT_EQ(as<AST::Fa_NameExpr>(first_target->get_object())->get_value(), "__class$instance");
    EXPECT_EQ(as<AST::Fa_LiteralExpr>(first_target->get_index())->get_str(), "x");
    EXPECT_EQ(as<AST::Fa_NameExpr>(first_assign->get_value())->get_value(), "x");
}
