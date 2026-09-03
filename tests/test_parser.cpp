#include "../fairuz/fAST_printer.hpp"
#include "../fairuz/fparser.hpp"
#include "test_config.h"

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
        if (diagnostic::has_errors() || diagnostic::warning_count() > 0)
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

    EXPECT_EQ(AS_LITERAL(parser_0.parse().value())->get_type(), AST::Fa_LiteralExpr::Type::INTEGER) << "Should parse integer literal";
    EXPECT_EQ(AS_LITERAL(parser_1.parse().value())->get_type(), AST::Fa_LiteralExpr::Type::STRING) << "Should parse string literal";
    EXPECT_EQ(AS_LITERAL(parser_2.parse().value())->get_type(), AST::Fa_LiteralExpr::Type::BOOLEAN) << "Should parse true bool literal";
    EXPECT_EQ(AS_LITERAL(parser_3.parse().value())->get_type(), AST::Fa_LiteralExpr::Type::BOOLEAN) << "Should parse false bool literal";
}

TEST_F(ParserTest, ParseNoneLiteral)
{
    Fa_FileManager fm(parser_test_cases_dir() / "none_literal.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();
    AST::Fa_LiteralExpr* literal = AS_LITERAL(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->get_type(), AST::Fa_LiteralExpr::Type::NIL);
}

TEST_F(ParserTest, ParseParenthesizedNumberLiteral)
{
    Fa_FileManager fm(parser_test_cases_dir() / "parenthesized_number.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();
    AST::Fa_LiteralExpr* literal = AS_LITERAL(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->get_type(), AST::Fa_LiteralExpr::Type::INTEGER);
}

TEST_F(ParserTest, ParseIdentifier)
{
    Fa_FileManager fm(parser_test_cases_dir() / "identifier.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();
    AST::Fa_NameExpr* name_fa_expr = AS_NAME(expr);

    if (test_config::print_ast)
        AST_Printer.print(name_fa_expr);

    ASSERT_NE(name_fa_expr, nullptr);
    EXPECT_EQ(name_fa_expr->get_value(), "المتنبي");
}

TEST_F(ParserTest, ParseCallExpressionNoArgs)
{
    Fa_FileManager fm(parser_test_cases_dir() / "call_expression.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr);

    AST::Fa_CallExpr* call_fa_expr = AS_CALL(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_fa_expr);

    ASSERT_NE(call_fa_expr, nullptr);
    ASSERT_NE(call_fa_expr->get_callee(), nullptr);

    AST::Fa_NameExpr* callee_name = AS_NAME(call_fa_expr->get_callee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->get_value(), "اطبع");
    EXPECT_TRUE(call_fa_expr->get_args().empty());
}

TEST_F(ParserTest, ParseCallExpressionWithOneArg)
{
    Fa_FileManager fm(parser_test_cases_dir() / "call_expression_with_one_argument.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr);

    AST::Fa_CallExpr* call_expr = AS_CALL(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_expr);

    ASSERT_NE(call_expr, nullptr);
    ASSERT_NE(call_expr->get_callee(), nullptr);

    AST::Fa_NameExpr* callee_name = AS_NAME(call_expr->get_callee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->get_value(), "اطبع");

    auto const& args = call_expr->get_args();

    EXPECT_FALSE(args.empty());
    /// TODO: check for each argument and their order
}

TEST_F(ParserTest, ParseNestedCallExpression)
{
    // f(g(x))
    Fa_FileManager fm(parser_test_cases_dir() / "nested_call_expression.fa");
    Fa_Parser parser(&fm);

    AST::Fa_CallExpr* outer_call = AS_CALL(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(outer_call);

    EXPECT_EQ(AS_NAME(outer_call->get_callee())->get_value(), "ا");

    AST::Fa_CallExpr* inner_call = AS_CALL(outer_call->get_args_as_list_expr()->get_elements()[0]);

    EXPECT_EQ(AS_NAME(inner_call->get_callee())->get_value(), "ب");
    EXPECT_EQ(AS_NAME(inner_call->get_args_as_list_expr()->get_elements()[0])->get_value(), "د");
}

TEST_F(ParserTest, ParseSimpleAddition)
{
    Fa_FileManager fm(parser_test_cases_dir() / "simple_addition.fa");
    Fa_Parser parser(&fm);

    AST::Fa_BinaryExpr* bin = AS_BINARY(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_operator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(AS_NAME(bin->get_left())->get_value(), "ا");
    EXPECT_EQ(AS_NAME(bin->get_right())->get_value(), "ب");
}

TEST_F(ParserTest, ParseSimpleMultiplication)
{
    Fa_FileManager fm(parser_test_cases_dir() / "simple_multiplication.fa");
    Fa_Parser parser(&fm);

    AST::Fa_BinaryExpr* bin = AS_BINARY(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_operator(), AST::Fa_BinaryOp::OP_MUL);
    EXPECT_EQ(AS_NAME(bin->get_left())->get_value(), "ا");
    EXPECT_EQ(AS_NAME(bin->get_right())->get_value(), "ب");
}

TEST_F(ParserTest, ParseSimpleSubtraction)
{
    Fa_FileManager fm(parser_test_cases_dir() / "simple_subtraction.fa");
    Fa_Parser parser(&fm);

    AST::Fa_BinaryExpr* bin = AS_BINARY(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_operator(), AST::Fa_BinaryOp::OP_SUB);

    AST::Fa_NameExpr* lhs = AS_NAME(bin->get_left());
    AST::Fa_NameExpr* rhs = AS_NAME(bin->get_right());

    EXPECT_EQ(lhs->get_value(), "ا");
    EXPECT_EQ(rhs->get_value(), "ب");
}

TEST_F(ParserTest, ParseSimpleDivision)
{
    Fa_FileManager fm(parser_test_cases_dir() / "simple_division.fa");
    Fa_Parser parser(&fm);

    AST::Fa_BinaryExpr* bin = AS_BINARY(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_operator(), AST::Fa_BinaryOp::OP_DIV);

    AST::Fa_NameExpr* lhs = AS_NAME(bin->get_left());
    AST::Fa_NameExpr* rhs = AS_NAME(bin->get_right());

    EXPECT_EQ(lhs->get_value(), "ا");
    EXPECT_EQ(rhs->get_value(), "ب");
}

TEST_F(ParserTest, ParseComplexExpression)
{
    // 2 + 3 * 4  →  2 + (3 * 4)
    Fa_FileManager fm(parser_test_cases_dir() / "complex_expression.fa");
    Fa_Parser parser(&fm);

    AST::Fa_BinaryExpr* root = AS_BINARY(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(root);

    EXPECT_EQ(root->get_operator(), AST::Fa_BinaryOp::OP_ADD);

    AST::Fa_LiteralExpr* left = AS_LITERAL(root->get_left());
    EXPECT_EQ(left->get_type(), AST::Fa_LiteralExpr::Type::INTEGER);
    EXPECT_EQ(left->as_number(), 2);

    AST::Fa_BinaryExpr* mul = AS_BINARY(root->get_right());
    EXPECT_EQ(mul->get_operator(), AST::Fa_BinaryOp::OP_MUL);

    EXPECT_EQ(AS_LITERAL(mul->get_left())->as_number(), 3);
    EXPECT_EQ(AS_LITERAL(mul->get_right())->as_number(), 4);
}

TEST_F(ParserTest, ParseNestedParentheses)
{
    // Test: ((2 + 3) * 4)
    Fa_FileManager fm(parser_test_cases_dir() / "nested_parens.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse nested parentheses expression";

    // Should be: AST::Fa_BinaryExpr((2 + 3), *, 4)
    AST::Fa_BinaryExpr* root = AS_BINARY(expr);

    ASSERT_NE(root, nullptr) << "Root should be a AST::Fa_BinaryExpr";
    EXPECT_EQ(root->get_operator(), AST::Fa_BinaryOp::OP_MUL);

    // Left should be (2 + 3)
    AST::Fa_BinaryExpr* left_add = AS_BINARY(root->get_left());

    ASSERT_NE(left_add, nullptr) << "Left should be addition expression";
    EXPECT_EQ(left_add->get_operator(), AST::Fa_BinaryOp::OP_ADD);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseLogicalExpression)
{
    // Test: a and b or c (should be (a and b) or c)
    Fa_FileManager fm(parser_test_cases_dir() / "logical_expression.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse logical expression";

    AST::Fa_BinaryExpr* root = AS_BINARY(expr);

    ASSERT_NE(root, nullptr) << "Root should be AST::Fa_BinaryExpr";
    EXPECT_EQ(root->get_operator(), AST::Fa_BinaryOp::OP_OR) << "Root should be OR (lower precedence)";

    // Left should be (a and b)
    AST::Fa_BinaryExpr* left_and = AS_BINARY(root->get_left());

    ASSERT_NE(left_and, nullptr) << "Left should be AND expression";
    EXPECT_EQ(left_and->get_operator(), AST::Fa_BinaryOp::OP_AND);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnaryChain)
{
    // Test: --x (f64 negation)
    Fa_FileManager fm(parser_test_cases_dir() / "unary_chain.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse unary chain";

    AST::Fa_UnaryExpr* outer = AS_UNARY(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be AST::Fa_UnaryExpr";
    EXPECT_EQ(outer->get_operator(), AST::Fa_UnaryOp::OP_NEG);

    AST::Fa_UnaryExpr* inner = AS_UNARY(outer->get_operand());

    ASSERT_NE(inner, nullptr) << "Inner should be AST::Fa_UnaryExpr";
    EXPECT_EQ(inner->get_operator(), AST::Fa_UnaryOp::OP_NEG);

    AST::Fa_NameExpr* name = AS_NAME(inner->get_operand());

    ASSERT_NE(name, nullptr) << "Innermost should be AST::Fa_NameExpr";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseComplexFunctionCall)
{
    // func(a + b, c * d)
    Fa_FileManager fm(parser_test_cases_dir() / "complex_function_call.fa");
    Fa_Parser parser(&fm);

    AST::Fa_CallExpr* call = AS_CALL(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(call);

    EXPECT_EQ(AS_NAME(call->get_callee())->get_value(), "علم");

    AST::Fa_ListExpr* args = call->get_args_as_list_expr();
    ASSERT_NE(args, nullptr);
    ASSERT_FALSE(args->is_empty());

    auto const& arg_list = args->get_elements();
    ASSERT_EQ(arg_list.size(), 2);

    AST::Fa_BinaryExpr* arg1 = AS_BINARY(arg_list[0]);
    EXPECT_EQ(arg1->get_operator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(AS_NAME(arg1->get_left())->get_value(), "ا");
    EXPECT_EQ(AS_NAME(arg1->get_right())->get_value(), "ب");

    AST::Fa_BinaryExpr* arg2 = AS_BINARY(arg_list[1]);
    EXPECT_EQ(arg2->get_operator(), AST::Fa_BinaryOp::OP_MUL);
    EXPECT_EQ(AS_NAME(arg2->get_left())->get_value(), "ت");
    EXPECT_EQ(AS_NAME(arg2->get_right())->get_value(), "ث");
}

TEST_F(ParserTest, ParseUnmatchedParenthesis)
{
    Fa_FileManager fm(parser_test_cases_dir() / "unmatched_paren.fa");
    Fa_Parser parser(&fm);
    auto expr = parser.parse();

    EXPECT_TRUE(expr.has_error()) << "Fa_Parser should detect unmatched parenthesis";
}

TEST_F(ParserTest, ParseExtraClosingParenthesis)
{
    Fa_FileManager fm(parser_test_cases_dir() / "extra_paren.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse the valid part";
    EXPECT_FALSE(parser.we_done()) << "Should have unparsed tokens remaining";
    EXPECT_TRUE(parser.check(tok::Fa_TokenType::RPAREN)) << "Remaining token should be RPAREN";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseInvalidOperatorSequence)
{
    Fa_FileManager fm(parser_test_cases_dir() / "invalid_operator_seq.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    if (expr != nullptr) {
        if (test_config::print_ast)
            AST_Printer.print(expr);
        AST::Fa_BinaryExpr* binary = AS_BINARY(expr);
        if (binary != nullptr) {
            EXPECT_NE(binary->get_left(), nullptr) << "Left operand should exist";
            EXPECT_NE(binary->get_right(), nullptr) << "Right operand should exist";
        }
    }

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseSingleIdentifier)
{
    Fa_FileManager fm(parser_test_cases_dir() / "single_identifier.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse single identifier";

    AST::Fa_NameExpr* name = AS_NAME(expr);

    ASSERT_NE(name, nullptr) << "Should be AST::Fa_NameExpr";
    EXPECT_EQ(name->get_value(), "ا") << "Identifier value should be 'x'";
    EXPECT_TRUE(parser.we_done()) << "Should be at end after single identifier";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseVeryLongIdentifier)
{
    Fa_FileManager fm(parser_test_cases_dir() / "long_identifier.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse very long identifier";

    AST::Fa_NameExpr* name = AS_NAME(expr);

    ASSERT_NE(name, nullptr) << "Should be AST::Fa_NameExpr";

    Fa_StringRef value = name->get_value();

    EXPECT_GT(value.len(), 100) << "Identifier should be very long";
    EXPECT_LT(value.len(), 10000) << "Identifier should have reasonable upper bound";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnicodeIdentifiers)
{
    Fa_FileManager fm(parser_test_cases_dir() / "unicode_identifiers.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse Unicode identifiers";

    AST::Fa_BinaryExpr* binary = AS_BINARY(expr);

    ASSERT_NE(binary, nullptr) << "Should be AST::Fa_BinaryExpr";

    AST::Fa_NameExpr* left = AS_NAME(binary->get_left());
    AST::Fa_NameExpr* right = AS_NAME(binary->get_right());

    ASSERT_NE(left, nullptr) << "Left should be AST::Fa_NameExpr";
    ASSERT_NE(right, nullptr) << "Right should be AST::Fa_NameExpr";

    EXPECT_GT(left->get_value().len(), 0) << "Left identifier should not be empty";
    EXPECT_GT(right->get_value().len(), 0) << "Right identifier should not be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyList)
{
    Fa_FileManager fm(parser_test_cases_dir() / "empty_list.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse empty list";

    AST::Fa_ListExpr* list = dynamic_cast<AST::Fa_ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be AST::Fa_ListExpr";
    EXPECT_EQ(list->get_elements().size(), 0) << "List should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyTuple)
{
    Fa_FileManager fm(parser_test_cases_dir() / "empty_tuple.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse empty tuple";

    AST::Fa_ListExpr* tuple = dynamic_cast<AST::Fa_ListExpr*>(expr);

    ASSERT_NE(tuple, nullptr) << "Should be AST::Fa_ListExpr (representing tuple)";
    EXPECT_EQ(tuple->get_elements().size(), 0) << "Tuple should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseListWithTrailingComma)
{
    Fa_FileManager fm(parser_test_cases_dir() / "list_trailing_comma.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse list with trailing comma";

    AST::Fa_ListExpr* list = dynamic_cast<AST::Fa_ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be AST::Fa_ListExpr";
    EXPECT_EQ(list->get_elements().size(), 3) << "Should have 3 elements despite trailing comma";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseNestedLists)
{
    Fa_FileManager fm(parser_test_cases_dir() / "nested_lists.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse nested lists";

    AST::Fa_ListExpr* outer_list = dynamic_cast<AST::Fa_ListExpr*>(expr);

    ASSERT_NE(outer_list, nullptr) << "Should be AST::Fa_ListExpr";
    EXPECT_EQ(outer_list->get_elements().size(), 2) << "Outer list should have 2 elements";

    AST::Fa_ListExpr* inner1 = dynamic_cast<AST::Fa_ListExpr*>(outer_list->get_elements()[0]);

    ASSERT_NE(inner1, nullptr) << "First element should be AST::Fa_ListExpr";
    EXPECT_EQ(inner1->get_elements().size(), 2) << "First inner list should have 2 elements";

    AST::Fa_ListExpr* inner2 = dynamic_cast<AST::Fa_ListExpr*>(outer_list->get_elements()[1]);

    ASSERT_NE(inner2, nullptr) << "Second element should be AST::Fa_ListExpr";
    EXPECT_EQ(inner2->get_elements().size(), 2) << "Second inner list should have 2 elements";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseAssignment)
{
    Fa_FileManager fm(parser_test_cases_dir() / "assignment.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* node = parser.parse().value();
    ASSERT_NE(node, nullptr) << "Should parse assignment";

    AST::Fa_AssignmentExpr* assign = dynamic_cast<AST::Fa_AssignmentExpr*>(node);
    ASSERT_NE(assign, nullptr) << "Should be Fa_AssignmentExpr";

    AST::Fa_NameExpr* target = AS_NAME(assign->get_target());

    ASSERT_NE(target, nullptr) << "Assignment target should not be null";
    EXPECT_EQ(target->get_value(), "ا") << "Target should be 'ا'";

    AST::Fa_LiteralExpr* value = AS_LITERAL(assign->get_value());

    ASSERT_NE(value, nullptr) << "Fa_Value should be AST::Fa_LiteralExpr";
    EXPECT_EQ(value->as_number(), 42);

    if (test_config::print_ast)
        AST_Printer.print(assign);
}

TEST_F(ParserTest, ParseChainedAssignment)
{
    Fa_FileManager fm(parser_test_cases_dir() / "chained_assignment.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse chained assignment";

    AST::Fa_AssignmentExpr* outer = dynamic_cast<AST::Fa_AssignmentExpr*>(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be Fa_AssignmentExpr";
    EXPECT_EQ(AS_NAME(outer->get_target())->get_value(), "ا");

    AST::Fa_AssignmentExpr* inner = dynamic_cast<AST::Fa_AssignmentExpr*>(outer->get_value());

    ASSERT_NE(inner, nullptr) << "Inner value should be Fa_AssignmentExpr";
    EXPECT_EQ(AS_NAME(inner->get_target())->get_value(), "ب");

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedAssignmentWithExpr)
{
    Fa_FileManager fm(parser_test_cases_dir() / "chained_assignment_with_expression.fa");
    Fa_Parser parser(&fm);

    AST::Fa_AssignmentExpr* outer = AS_ASSIGNMENT_EXPR(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(outer);

    AST::Fa_AssignmentExpr* inner = AS_ASSIGNMENT_EXPR(outer->get_value());
    AST::Fa_BinaryExpr* binary = AS_BINARY(inner->get_value());

    EXPECT_EQ(AS_NAME(outer->get_target())->get_value(), "ا");
    EXPECT_EQ(AS_NAME(inner->get_target())->get_value(), "ب");
    EXPECT_EQ(binary->get_operator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(AS_NAME(binary->get_left())->get_value(), "م");
    EXPECT_EQ(AS_NAME(binary->get_right())->get_value(), "ل");
}

TEST_F(ParserTest, ParseDeeplyNestedExpression)
{
    Fa_FileManager fm(parser_test_cases_dir() / "deeply_nested.fa");
    Fa_Parser parser(&fm);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse deeply nested expression without stack overflow";

    if (test_config::print_ast) {
        AST_Printer.print(expr);
    }
}

TEST_F(ParserTest, ParseWhileLoop)
{
    Fa_FileManager fm(parser_test_cases_dir() / "while_loop.fa");
    Fa_Parser parser(&fm);

    AST::Fa_WhileStmt* while_stmt = AS_WHILE(parser.parse_while_stmt().value());

    if (test_config::print_ast)
        AST_Printer.print(while_stmt);

    AST::Fa_BinaryExpr* cond = AS_BINARY(while_stmt->get_condition());
    AST::Fa_BlockStmt* block = AS_BLOCK(while_stmt->get_body());
    AST::Fa_AssignmentExpr* assign = AS_ASSIGNMENT_EXPR(AS_EXPR_STMT(block->get_statements()[0])->get_expr());

    EXPECT_EQ(AS_NAME(cond->get_left())->get_value(), "شيء");
    EXPECT_TRUE(AS_LITERAL(cond->get_right())->get_bool());
    EXPECT_EQ(cond->get_operator(), AST::Fa_BinaryOp::OP_EQ);
    ASSERT_FALSE(block->get_statements().empty());
    EXPECT_EQ(AS_NAME(assign->get_target())->get_value(), "بسبسمياو");
    EXPECT_FALSE(AS_LITERAL(assign->get_value())->get_bool());
}

TEST_F(ParserTest, ParseForLoop)
{
    Fa_FileManager fm(parser_test_cases_dir() / "for_loop.fa");
    Fa_Parser parser(&fm);

    AST::Fa_ForStmt* for_stmt = AS_FOR(parser.parse_for_stmt().value());

    if (test_config::print_ast)
        AST_Printer.print(for_stmt);

    EXPECT_EQ(for_stmt->get_target()->get_value(), "عنصر");
    EXPECT_EQ(AS_NAME(for_stmt->get_iter())->get_value(), "عناصر");
    auto* block = AS_BLOCK(for_stmt->get_body());
    ASSERT_EQ(block->get_statements().size(), 1u);
    auto* expr_stmt = AS_EXPR_STMT(block->get_statements()[0]);
    auto* assign = AS_ASSIGNMENT_EXPR(expr_stmt->get_expr());
    EXPECT_EQ(AS_NAME(assign->get_target())->get_value(), "اجمع");
    EXPECT_EQ(AS_NAME(assign->get_value())->get_value(), "عنصر");
}

TEST_F(ParserTest, ParseBreakStatement)
{
    Fa_FileManager fm(parser_test_cases_dir() / "break_stmt.fa");
    Fa_Parser parser(&fm);

    AST::Fa_BreakStmt* break_stmt = AS_BREAK(parser.parse_break_stmt().value());
    ASSERT_NE(break_stmt, nullptr);
}

TEST_F(ParserTest, ParseContinueStatement)
{
    Fa_FileManager fm(parser_test_cases_dir() / "continue_stmt.fa");
    Fa_Parser parser(&fm);

    AST::Fa_ContinueStmt* continue_stmt = AS_CONTINUE(parser.parse_continue_stmt().value());
    ASSERT_NE(continue_stmt, nullptr);
}

TEST_F(ParserTest, ParseComplexeIfStatement)
{
    Fa_FileManager fm(parser_test_cases_dir() / "complexe_if_statement.fa");
    Fa_Parser parser(&fm);

    AST::Fa_IfStmt* if_stmt = AS_IF(parser.parse_if_stmt().value());

    if (test_config::print_ast)
        AST_Printer.print(if_stmt);

    AST::Fa_BinaryExpr* cond = AS_BINARY(if_stmt->get_condition());
    // the while statement is wrapped in a block inside the else clause
    AST::Fa_WhileStmt* while_stmt = AS_WHILE(AS_BLOCK(if_stmt->get_then())->get_statements()[0]);
    AST::Fa_BinaryExpr* while_cond = AS_BINARY(while_stmt->get_condition());
    AST::Fa_BlockStmt* block = AS_BLOCK(while_stmt->get_body());
    AST::Fa_AssignmentExpr* assign = AS_ASSIGNMENT_EXPR(AS_EXPR_STMT(block->get_statements()[0])->get_expr());

    EXPECT_EQ(AS_NAME(cond->get_left())->get_value(), "شيء");
    EXPECT_TRUE(AS_LITERAL(cond->get_right())->get_bool());
    EXPECT_EQ(cond->get_operator(), AST::Fa_BinaryOp::OP_NEQ);
    EXPECT_EQ(AS_NAME(while_cond->get_left())->get_value(), "شيء");
    EXPECT_TRUE(AS_LITERAL(while_cond->get_right())->get_bool());
    EXPECT_EQ(while_cond->get_operator(), AST::Fa_BinaryOp::OP_EQ);
    ASSERT_FALSE(block->get_statements().empty());
    EXPECT_EQ(AS_NAME(assign->get_target())->get_value(), "بسبسمياو");
    EXPECT_FALSE(AS_LITERAL(assign->get_value())->get_bool());
}

TEST_F(ParserTest, ParseAugmentedAssignmentPlus)
{
    // a += b -> a := a + b
    Fa_FileManager fm(parser_test_cases_dir() / "augmented_assign_plus.fa");
    Fa_Parser parser(&fm);

    auto assign_expr = AS_ASSIGNMENT_EXPR(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = AS_BINARY(assign_expr->get_value());

    EXPECT_EQ(AS_NAME(target)->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_ADD);
}

TEST_F(ParserTest, ParseAugmentedAssignmentMinus)
{
    // a -= b -> a := a - b
    Fa_FileManager fm(parser_test_cases_dir() / "augmented_assign_minus.fa");
    Fa_Parser parser(&fm);

    auto assign_expr = AS_ASSIGNMENT_EXPR(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = AS_BINARY(assign_expr->get_value());

    EXPECT_EQ(AS_NAME(target)->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_SUB);
}

TEST_F(ParserTest, ParseAugmentedAssignmentTimes)
{
    // a *= b -> a := a * b
    Fa_FileManager fm(parser_test_cases_dir() / "augmented_assign_times.fa");
    Fa_Parser parser(&fm);

    auto assign_expr = AS_ASSIGNMENT_EXPR(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = AS_BINARY(assign_expr->get_value());

    EXPECT_EQ(AS_NAME(target)->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_MUL);
}

TEST_F(ParserTest, ParseAugmentedAssignmentDiv)
{
    // a /= b -> a := a / b
    Fa_FileManager fm(parser_test_cases_dir() / "augmented_assign_div.fa");
    Fa_Parser parser(&fm);

    auto assign_expr = AS_ASSIGNMENT_EXPR(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = AS_BINARY(assign_expr->get_value());

    EXPECT_EQ(AS_NAME(target)->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_DIV);
}

TEST_F(ParserTest, ParseAugmentedAssignmentMod)
{
    // a %= b -> a := a % b
    Fa_FileManager fm(parser_test_cases_dir() / "augmented_assign_mod.fa");
    Fa_Parser parser(&fm);

    auto assign_expr = AS_ASSIGNMENT_EXPR(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->get_target();
    auto value_as_binary = AS_BINARY(assign_expr->get_value());

    EXPECT_EQ(AS_NAME(target)->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_left())->get_value(), "ا");
    EXPECT_EQ(AS_NAME(value_as_binary->get_right())->get_value(), "ب");
    EXPECT_EQ(value_as_binary->get_operator(), AST::Fa_BinaryOp::OP_MOD);
}
