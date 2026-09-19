#include "../fairuz/fAST_printer.hpp"
#include "../fairuz/fparser.hpp"
#include "fAST.hpp"
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
    T* parse_and_cast(AST::ExprPtr& expr)
    {
        EXPECT_NE(expr, nullptr) << "Expression should not be null";
        if (!expr)
            return nullptr;

        T* casted = reinterpret_cast<T*>(expr);
        EXPECT_NE(casted, nullptr) << "Failed to cast to expected type";
        return casted;
    }

    template<typename T>
    T* as(AST::Stmt* node)
    {
        T* casted = dynamic_cast<T*>(node);
        EXPECT_NE(casted, nullptr);
        return casted;
    }

    template<typename T>
    T* as(AST::ExprPtr node)
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
    FileManager file_manager_0(parser_test_cases_dir() / "number_literal.fa");
    FileManager file_manager_1(parser_test_cases_dir() / "string_literal.fa");
    FileManager file_manager_2(parser_test_cases_dir() / "boolean_literal_true.fa");
    FileManager file_manager_3(parser_test_cases_dir() / "boolean_literal_false.fa");

    Parser parser_0(&file_manager_0);
    Parser parser_1(&file_manager_1);
    Parser parser_2(&file_manager_2);
    Parser parser_3(&file_manager_3);

    EXPECT_TRUE(AST::is_literal_int(parser_0.parse().value())) << "Should parse integer literal";
    EXPECT_TRUE(AST::is_literal_string(parser_1.parse().value())) << "Should parse string literal";
    EXPECT_TRUE(AST::is_literal_bool(parser_2.parse().value())) << "Should parse true bool literal";
    EXPECT_TRUE(AST::is_literal_bool(parser_3.parse().value())) << "Should parse false bool literal";
}

TEST_F(ParserTest, ParseNil)
{
    FileManager fm(parser_test_cases_dir() / "none_literal.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();
    EXPECT_TRUE(AST::is_nil(expr));
    if (test_config::print_ast)
        AST_Printer.print(AST::as_nil(expr));
}

TEST_F(ParserTest, ParseParenthesizedNumberLiteral)
{
    FileManager fm(parser_test_cases_dir() / "parenthesized_number.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();
    EXPECT_TRUE(AST::is_literal_int(expr));
    if (test_config::print_ast)
        AST_Printer.print(AST::as_literal_int(expr));
}

TEST_F(ParserTest, ParseIdentifier)
{
    FileManager fm(parser_test_cases_dir() / "identifier.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();
    AST::IdentifierExpr* name_fa_expr = AST::as_identifier(expr);

    if (test_config::print_ast)
        AST_Printer.print(name_fa_expr);

    ASSERT_NE(name_fa_expr, nullptr);
    EXPECT_EQ(name_fa_expr->spelling, "المتنبي");
}

TEST_F(ParserTest, ParseCallExpressionNoArgs)
{
    FileManager fm(parser_test_cases_dir() / "call_expression.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr);

    AST::CallExpr* call_fa_expr = as_call(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_fa_expr);

    ASSERT_NE(call_fa_expr, nullptr);
    ASSERT_NE(call_fa_expr->callee, nullptr);

    AST::IdentifierExpr* callee_name = as_identifier(call_fa_expr->callee);

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->spelling, "اطبع");
    EXPECT_TRUE(call_fa_expr->args.empty());
}

TEST_F(ParserTest, ParseCallExpressionWithOneArg)
{
    FileManager fm(parser_test_cases_dir() / "call_expression_with_one_argument.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr);

    AST::CallExpr* call_expr = as_call(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_expr);

    ASSERT_NE(call_expr, nullptr);
    ASSERT_NE(call_expr->callee, nullptr);

    AST::IdentifierExpr* callee_name = as_identifier(call_expr->callee);

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->spelling, "اطبع");

    auto const& args = call_expr->args;

    EXPECT_FALSE(args.empty());
    /// TODO: check for each argument and their order
}

TEST_F(ParserTest, ParseNestedCallExpression)
{
    // f(g(x))
    FileManager fm(parser_test_cases_dir() / "nested_call_expression.fa");
    Parser parser(&fm);

    AST::CallExpr* outer_call = as_call(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(outer_call);

    EXPECT_EQ(as_identifier(outer_call->callee)->spelling, "ا");

    AST::CallExpr* inner_call = as_call(outer_call->args[0]);

    EXPECT_EQ(as_identifier(inner_call->callee)->spelling, "ب");
    EXPECT_EQ(as_identifier(inner_call->args[0])->spelling, "د");
}

TEST_F(ParserTest, ParseSimpleAddition)
{
    FileManager fm(parser_test_cases_dir() / "simple_addition.fa");
    Parser parser(&fm);

    AST::BinaryExpr* bin = as_binary(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_kind(), AST::Expr::Kind::OP_ADD);
    EXPECT_EQ(as_identifier(bin->lhs)->spelling, "ا");
    EXPECT_EQ(as_identifier(bin->rhs)->spelling, "ب");
}

TEST_F(ParserTest, ParseSimpleMultiplication)
{
    FileManager fm(parser_test_cases_dir() / "simple_multiplication.fa");
    Parser parser(&fm);

    AST::BinaryExpr* bin = as_binary(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_kind(), AST::Expr::Kind::OP_MUL);
    EXPECT_EQ(as_identifier(bin->lhs)->spelling, "ا");
    EXPECT_EQ(as_identifier(bin->rhs)->spelling, "ب");
}

TEST_F(ParserTest, ParseSimpleSubtraction)
{
    FileManager fm(parser_test_cases_dir() / "simple_subtraction.fa");
    Parser parser(&fm);

    AST::BinaryExpr* bin = as_binary(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_kind(), AST::Expr::Kind::OP_SUB);

    AST::IdentifierExpr* lhs = as_identifier(bin->lhs);
    AST::IdentifierExpr* rhs = as_identifier(bin->rhs);

    EXPECT_EQ(lhs->spelling, "ا");
    EXPECT_EQ(rhs->spelling, "ب");
}

TEST_F(ParserTest, ParseSimpleDivision)
{
    FileManager fm(parser_test_cases_dir() / "simple_division.fa");
    Parser parser(&fm);

    AST::BinaryExpr* bin = as_binary(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->get_kind(), AST::Expr::Kind::OP_DIV);

    AST::IdentifierExpr* lhs = as_identifier(bin->lhs);
    AST::IdentifierExpr* rhs = as_identifier(bin->rhs);

    EXPECT_EQ(lhs->spelling, "ا");
    EXPECT_EQ(rhs->spelling, "ب");
}

TEST_F(ParserTest, ParseComplexExpression)
{
    // 2 + 3 * 4  →  2 + (3 * 4)
    FileManager fm(parser_test_cases_dir() / "complex_expression.fa");
    Parser parser(&fm);

    AST::BinaryExpr* root = as_binary(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(root);

    EXPECT_EQ(root->get_kind(), AST::Expr::Kind::OP_ADD);

    AST::IntLiteralExpr* left = as_literal_int(root->lhs);
    EXPECT_EQ(left->value, 2);

    AST::BinaryExpr* mul = as_binary(root->rhs);
    EXPECT_EQ(mul->get_kind(), AST::Expr::Kind::OP_MUL);

    EXPECT_EQ(as_literal_int(mul->lhs)->value, 3);
    EXPECT_EQ(as_literal_int(mul->rhs)->value, 4);
}

TEST_F(ParserTest, ParseNestedParentheses)
{
    // Test: ((2 + 3) * 4)
    FileManager fm(parser_test_cases_dir() / "nested_parens.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse nested parentheses expression";

    // Should be: AST::BinaryExpr((2 + 3), *, 4)
    AST::BinaryExpr* root = as_binary(expr);

    ASSERT_NE(root, nullptr) << "Root should be a AST::BinaryExpr";
    EXPECT_EQ(root->get_kind(), AST::Expr::Kind::OP_MUL);

    // Left should be (2 + 3)
    AST::BinaryExpr* left_add = as_binary(root->lhs);

    ASSERT_NE(left_add, nullptr) << "Left should be addition expression";
    EXPECT_EQ(left_add->get_kind(), AST::Expr::Kind::OP_ADD);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseLogicalExpression)
{
    // Test: a and b or c (should be (a and b) or c)
    FileManager fm(parser_test_cases_dir() / "logical_expression.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse logical expression";

    AST::BinaryExpr* root = as_binary(expr);

    ASSERT_NE(root, nullptr) << "Root should be AST::BinaryExpr";
    EXPECT_EQ(root->get_kind(), AST::Expr::Kind::OP_OR) << "Root should be OR (lower precedence)";

    // Left should be (a and b)
    AST::BinaryExpr* left_and = as_binary(root->lhs);

    ASSERT_NE(left_and, nullptr) << "Left should be AND expression";
    EXPECT_EQ(left_and->get_kind(), AST::Expr::Kind::OP_AND);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnaryChain)
{
    // Test: --x (f64 negation)
    FileManager fm(parser_test_cases_dir() / "unary_chain.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse unary chain";

    AST::UnaryExpr* outer = as_unary(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be AST::UnaryExpr";
    EXPECT_EQ(outer->get_kind(), AST::Expr::Kind::OP_NEG);

    AST::UnaryExpr* inner = as_unary(outer->operand);

    ASSERT_NE(inner, nullptr) << "Inner should be AST::UnaryExpr";
    EXPECT_EQ(inner->get_kind(), AST::Expr::Kind::OP_NEG);

    AST::IdentifierExpr* name = as_identifier(inner->operand);

    ASSERT_NE(name, nullptr) << "Innermost should be AST::IdentifierExpr";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseComplexFunctionCall)
{
    // func(a + b, c * d)
    FileManager fm(parser_test_cases_dir() / "complex_function_call.fa");
    Parser parser(&fm);

    AST::CallExpr* call = as_call(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(call);

    EXPECT_EQ(as_identifier(call->callee)->spelling, "علم");

    ASSERT_FALSE(call->args.empty());

    auto const& arg_list = call->args;
    ASSERT_EQ(arg_list.size(), 2);

    AST::BinaryExpr* arg1 = as_binary(arg_list[0]);
    EXPECT_EQ(arg1->get_kind(), AST::Expr::Kind::OP_ADD);
    EXPECT_EQ(as_identifier(arg1->lhs)->spelling, "ا");
    EXPECT_EQ(as_identifier(arg1->rhs)->spelling, "ب");

    AST::BinaryExpr* arg2 = as_binary(arg_list[1]);
    EXPECT_EQ(arg2->get_kind(), AST::Expr::Kind::OP_MUL);
    EXPECT_EQ(as_identifier(arg2->lhs)->spelling, "ت");
    EXPECT_EQ(as_identifier(arg2->rhs)->spelling, "ث");
}

TEST_F(ParserTest, ParseUnmatchedParenthesis)
{
    FileManager fm(parser_test_cases_dir() / "unmatched_paren.fa");
    Parser parser(&fm);
    auto expr = parser.parse();

    EXPECT_TRUE(expr.has_error()) << "Parser should detect unmatched parenthesis";
}

TEST_F(ParserTest, ParseExtraClosingParenthesis)
{
    FileManager fm(parser_test_cases_dir() / "extra_paren.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse the valid part";
    EXPECT_FALSE(parser.we_done()) << "Should have unparsed tokens remaining";
    EXPECT_TRUE(parser.check(tok::TokenType::RPAREN)) << "Remaining token should be RPAREN";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseInvalidOperatorSequence)
{
    FileManager fm(parser_test_cases_dir() / "invalid_operator_seq.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    if (expr != nullptr) {
        if (test_config::print_ast)
            AST_Printer.print(expr);
        AST::BinaryExpr* binary = as_binary(expr);
        if (binary != nullptr) {
            EXPECT_NE(binary->lhs, nullptr) << "Left operand should exist";
            EXPECT_NE(binary->rhs, nullptr) << "Right operand should exist";
        }
    }

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseSingleIdentifier)
{
    FileManager fm(parser_test_cases_dir() / "single_identifier.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse single identifier";

    AST::IdentifierExpr* name = as_identifier(expr);

    ASSERT_NE(name, nullptr) << "Should be AST::IdentifierExpr";
    EXPECT_EQ(name->spelling, "ا") << "Identifier value should be 'x'";
    EXPECT_TRUE(parser.we_done()) << "Should be at end after single identifier";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseVeryLongIdentifier)
{
    FileManager fm(parser_test_cases_dir() / "long_identifier.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse very long identifier";

    AST::IdentifierExpr* name = as_identifier(expr);

    ASSERT_NE(name, nullptr) << "Should be AST::IdentifierExpr";

    StringRef value = name->spelling;

    EXPECT_GT(value.len(), 100) << "Identifier should be very long";
    EXPECT_LT(value.len(), 10000) << "Identifier should have reasonable upper bound";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnicodeIdentifiers)
{
    FileManager fm(parser_test_cases_dir() / "unicode_identifiers.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse Unicode identifiers";

    AST::BinaryExpr* binary = as_binary(expr);

    ASSERT_NE(binary, nullptr) << "Should be AST::BinaryExpr";

    AST::IdentifierExpr* left = as_identifier(binary->lhs);
    AST::IdentifierExpr* right = as_identifier(binary->rhs);

    ASSERT_NE(left, nullptr) << "Left should be AST::IdentifierExpr";
    ASSERT_NE(right, nullptr) << "Right should be AST::IdentifierExpr";

    EXPECT_GT(left->spelling.len(), 0) << "Left identifier should not be empty";
    EXPECT_GT(right->spelling.len(), 0) << "Right identifier should not be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyList)
{
    FileManager fm(parser_test_cases_dir() / "empty_list.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse empty list";

    AST::ListExpr* list = dynamic_cast<AST::ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be AST::ListExpr";
    EXPECT_EQ(list->elements.size(), 0) << "List should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyTuple)
{
    FileManager fm(parser_test_cases_dir() / "empty_tuple.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse empty tuple";

    AST::ListExpr* tuple = dynamic_cast<AST::ListExpr*>(expr);

    ASSERT_NE(tuple, nullptr) << "Should be AST::ListExpr (representing tuple)";
    EXPECT_EQ(tuple->elements.size(), 0) << "Tuple should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseListWithTrailingComma)
{
    FileManager fm(parser_test_cases_dir() / "list_trailing_comma.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse list with trailing comma";

    AST::ListExpr* list = dynamic_cast<AST::ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be AST::ListExpr";
    EXPECT_EQ(list->elements.size(), 3) << "Should have 3 elements despite trailing comma";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseNestedLists)
{
    FileManager fm(parser_test_cases_dir() / "nested_lists.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse nested lists";

    AST::ListExpr* outer_list = dynamic_cast<AST::ListExpr*>(expr);

    ASSERT_NE(outer_list, nullptr) << "Should be AST::ListExpr";
    EXPECT_EQ(outer_list->elements.size(), 2) << "Outer list should have 2 elements";

    AST::ListExpr* inner1 = dynamic_cast<AST::ListExpr*>(outer_list->elements[0]);

    ASSERT_NE(inner1, nullptr) << "First element should be AST::ListExpr";
    EXPECT_EQ(inner1->elements.size(), 2) << "First inner list should have 2 elements";

    AST::ListExpr* inner2 = dynamic_cast<AST::ListExpr*>(outer_list->elements[1]);

    ASSERT_NE(inner2, nullptr) << "Second element should be AST::ListExpr";
    EXPECT_EQ(inner2->elements.size(), 2) << "Second inner list should have 2 elements";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseAssignment)
{
    FileManager fm(parser_test_cases_dir() / "assignment.fa");
    Parser parser(&fm);
    AST::ExprPtr node = parser.parse().value();
    ASSERT_NE(node, nullptr) << "Should parse assignment";

    AST::AssignExpr* assign = dynamic_cast<AST::AssignExpr*>(node);
    ASSERT_NE(assign, nullptr) << "Should be AssignExpr";

    AST::IdentifierExpr* target = as_identifier(assign->target);

    ASSERT_NE(target, nullptr) << "Assignment target should not be null";
    EXPECT_EQ(target->spelling, "ا") << "Target should be 'ا'";

    AST::IntLiteralExpr* value = as_literal_int(assign->value);

    EXPECT_EQ(value->value, 42);

    if (test_config::print_ast)
        AST_Printer.print(assign);
}

TEST_F(ParserTest, ParseChainedAssignment)
{
    FileManager fm(parser_test_cases_dir() / "chained_assignment.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse chained assignment";

    AST::AssignExpr* outer = dynamic_cast<AST::AssignExpr*>(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be AssignExpr";
    EXPECT_EQ(as_identifier(outer->target)->spelling, "ا");

    AST::AssignExpr* inner = dynamic_cast<AST::AssignExpr*>(outer->value);

    ASSERT_NE(inner, nullptr) << "Inner value should be AssignExpr";
    EXPECT_EQ(as_identifier(inner->target)->spelling, "ب");

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedAssignmentWithExpr)
{
    FileManager fm(parser_test_cases_dir() / "chained_assignment_with_expression.fa");
    Parser parser(&fm);

    AST::AssignExpr* outer = as_assignment_expr(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(outer);

    AST::AssignExpr* inner = as_assignment_expr(outer->value);
    AST::BinaryExpr* binary = as_binary(inner->value);

    EXPECT_EQ(as_identifier(outer->target)->spelling, "ا");
    EXPECT_EQ(as_identifier(inner->target)->spelling, "ب");
    EXPECT_EQ(binary->get_kind(), AST::Expr::Kind::OP_ADD);
    EXPECT_EQ(as_identifier(binary->lhs)->spelling, "م");
    EXPECT_EQ(as_identifier(binary->rhs)->spelling, "ل");
}

TEST_F(ParserTest, ParseDeeplyNestedExpression)
{
    FileManager fm(parser_test_cases_dir() / "deeply_nested.fa");
    Parser parser(&fm);
    AST::ExprPtr expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse deeply nested expression without stack overflow";

    if (test_config::print_ast) {
        AST_Printer.print(expr);
    }
}

TEST_F(ParserTest, ParseWhileLoop)
{
    FileManager fm(parser_test_cases_dir() / "while_loop.fa");
    Parser parser(&fm);

    AST::WhileStmt* while_stmt = as_while(parser.parse_while_stmt().value());

    if (test_config::print_ast)
        AST_Printer.print(while_stmt);

    AST::BinaryExpr* cond = as_binary(while_stmt->condition);
    AST::BlockStmt* block = as_block(while_stmt->body);
    AST::AssignExpr* assign = as_assignment_expr(as_expr_stmt(block->stmts[0])->expr);

    EXPECT_EQ(as_identifier(cond->lhs)->spelling, "شيء");
    EXPECT_TRUE(AST::as_literal_bool(cond->rhs)->value);
    EXPECT_EQ(cond->get_kind(), AST::Expr::Kind::OP_EQ);
    ASSERT_FALSE(block->stmts.empty());
    EXPECT_EQ(as_identifier(assign->target)->spelling, "بسبسمياو");
    EXPECT_FALSE(as_literal_bool(assign->value)->value);
}

TEST_F(ParserTest, ParseForLoop)
{
    FileManager fm(parser_test_cases_dir() / "for_loop.fa");
    Parser parser(&fm);

    AST::ForStmt* for_stmt = as_for(parser.parse_for_stmt().value());

    if (test_config::print_ast)
        AST_Printer.print(for_stmt);

    EXPECT_EQ(AST::as_identifier(for_stmt->container)->spelling, "عنصر");
    EXPECT_EQ(as_identifier(for_stmt->iter)->spelling, "عناصر");
    auto* block = as_block(for_stmt->body);
    ASSERT_EQ(block->stmts.size(), 1u);
    auto* expr_stmt = as_expr_stmt(block->stmts[0]);
    auto* assign = as_assignment_expr(expr_stmt->expr);
    EXPECT_EQ(as_identifier(assign->target)->spelling, "اجمع");
    EXPECT_EQ(as_identifier(assign->value)->spelling, "عنصر");
}

TEST_F(ParserTest, ParseBreakStatement)
{
    FileManager fm(parser_test_cases_dir() / "break_stmt.fa");
    Parser parser(&fm);

    AST::BreakStmt* break_stmt = as_break(parser.parse_break_stmt().value());
    ASSERT_NE(break_stmt, nullptr);
}

TEST_F(ParserTest, ParseContinueStatement)
{
    FileManager fm(parser_test_cases_dir() / "continue_stmt.fa");
    Parser parser(&fm);

    AST::ContinueStmt* continue_stmt = as_continue(parser.parse_continue_stmt().value());
    ASSERT_NE(continue_stmt, nullptr);
}

TEST_F(ParserTest, ParseComplexeIfStatement)
{
    FileManager fm(parser_test_cases_dir() / "complexe_if_statement.fa");
    Parser parser(&fm);

    AST::IfElseStmt* if_stmt = as_if(parser.parse_if_stmt().value());

    if (test_config::print_ast)
        AST_Printer.print(if_stmt);

    AST::BinaryExpr* cond = as_binary(if_stmt->condition);
    // the while statement is wrapped in a block inside the else clause
    AST::WhileStmt* while_stmt = as_while(as_block(if_stmt->then_stmt)->stmts[0]);
    AST::BinaryExpr* while_cond = as_binary(while_stmt->condition);
    AST::BlockStmt* block = as_block(while_stmt->body);
    AST::AssignExpr* assign = as_assignment_expr(as_expr_stmt(block->stmts[0])->expr);

    EXPECT_EQ(as_identifier(cond->lhs)->spelling, "شيء");
    EXPECT_TRUE(as_literal_bool(cond->rhs)->value);
    EXPECT_EQ(cond->get_kind(), AST::Expr::Kind::OP_NEQ);
    EXPECT_EQ(as_identifier(while_cond->lhs)->spelling, "شيء");
    EXPECT_TRUE(as_literal_bool(while_cond->rhs)->value);
    EXPECT_EQ(while_cond->get_kind(), AST::Expr::Kind::OP_EQ);
    ASSERT_FALSE(block->stmts.empty());
    EXPECT_EQ(as_identifier(assign->target)->spelling, "بسبسمياو");
    EXPECT_FALSE(as_literal_bool(assign->value)->value);
}

TEST_F(ParserTest, ParseAugmentedAssignmentPlus)
{
    // a += b -> a := a + b
    FileManager fm(parser_test_cases_dir() / "augmented_assign_plus.fa");
    Parser parser(&fm);

    auto assign_expr = as_assignment_expr(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->target;
    auto value_as_binary = as_binary(assign_expr->value);

    EXPECT_EQ(as_identifier(target)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->lhs)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->rhs)->spelling, "ب");
    EXPECT_EQ(value_as_binary->get_kind(), AST::Expr::Kind::OP_ADD);
}

TEST_F(ParserTest, ParseAugmentedAssignmentMinus)
{
    // a -= b -> a := a - b
    FileManager fm(parser_test_cases_dir() / "augmented_assign_minus.fa");
    Parser parser(&fm);

    auto assign_expr = as_assignment_expr(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->target;
    auto value_as_binary = as_binary(assign_expr->value);

    EXPECT_EQ(as_identifier(target)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->lhs)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->rhs)->spelling, "ب");
    EXPECT_EQ(value_as_binary->get_kind(), AST::Expr::Kind::OP_SUB);
}

TEST_F(ParserTest, ParseAugmentedAssignmentTimes)
{
    // a *= b -> a := a * b
    FileManager fm(parser_test_cases_dir() / "augmented_assign_times.fa");
    Parser parser(&fm);

    auto assign_expr = as_assignment_expr(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->target;
    auto value_as_binary = as_binary(assign_expr->value);

    EXPECT_EQ(as_identifier(target)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->lhs)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->rhs)->spelling, "ب");
    EXPECT_EQ(value_as_binary->get_kind(), AST::Expr::Kind::OP_MUL);
}

TEST_F(ParserTest, ParseAugmentedAssignmentDiv)
{
    // a /= b -> a := a / b
    FileManager fm(parser_test_cases_dir() / "augmented_assign_div.fa");
    Parser parser(&fm);

    auto assign_expr = as_assignment_expr(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->target;
    auto value_as_binary = as_binary(assign_expr->value);

    EXPECT_EQ(as_identifier(target)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->lhs)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->rhs)->spelling, "ب");
    EXPECT_EQ(value_as_binary->get_kind(), AST::Expr::Kind::OP_DIV);
}

TEST_F(ParserTest, ParseAugmentedAssignmentMod)
{
    // a %= b -> a := a % b
    FileManager fm(parser_test_cases_dir() / "augmented_assign_mod.fa");
    Parser parser(&fm);

    auto assign_expr = as_assignment_expr(parser.parse_assignment_expr().value());
    if (test_config::print_ast)
        AST_Printer.print(assign_expr);

    auto target = assign_expr->target;
    auto value_as_binary = as_binary(assign_expr->value);

    EXPECT_EQ(as_identifier(target)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->lhs)->spelling, "ا");
    EXPECT_EQ(as_identifier(value_as_binary->rhs)->spelling, "ب");
    EXPECT_EQ(value_as_binary->get_kind(), AST::Expr::Kind::OP_MOD);
}
