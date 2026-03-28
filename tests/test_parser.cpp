#include "../include/ast_printer.hpp"
#include "../include/parser.hpp"
#include "test_config.h"

#include <gtest/gtest.h>

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
    T* parseAndCast(Fa_Parser& parser, AST::Fa_Expr*& expr)
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
        if (diagnostic::hasErrors() || diagnostic::warningCount() > 0)
            diagnostic::dump();
    }
};

inline AST::ASTPrinter AST_Printer;

TEST_F(ParserTest, ParseLiteral)
{
    Fa_FileManager file_manager_0(parser_test_cases_dir() / "number_literal.txt");
    Fa_FileManager file_manager_1(parser_test_cases_dir() / "string_literal.txt");
    Fa_FileManager file_manager_2(parser_test_cases_dir() / "boolean_literal_true.txt");
    Fa_FileManager file_manager_3(parser_test_cases_dir() / "boolean_literal_false.txt");

    Fa_Parser parser_0(&file_manager_0);
    Fa_Parser parser_1(&file_manager_1);
    Fa_Parser parser_2(&file_manager_2);
    Fa_Parser parser_3(&file_manager_3);

    EXPECT_EQ(dynamic_cast<AST::Fa_LiteralExpr*>(parser_0.parse().value())->getType(), AST::Fa_LiteralExpr::Type::INTEGER) << "Should parse integer literal";
    EXPECT_EQ(dynamic_cast<AST::Fa_LiteralExpr*>(parser_1.parse().value())->getType(), AST::Fa_LiteralExpr::Type::STRING) << "Should parse string literal";
    EXPECT_EQ(dynamic_cast<AST::Fa_LiteralExpr*>(parser_2.parse().value())->getType(), AST::Fa_LiteralExpr::Type::BOOLEAN) << "Should parse true bool literal";
    EXPECT_EQ(dynamic_cast<AST::Fa_LiteralExpr*>(parser_3.parse().value())->getType(), AST::Fa_LiteralExpr::Type::BOOLEAN) << "Should parse false bool literal";
}

TEST_F(ParserTest, ParseNoneLiteral)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "none_literal.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();
    AST::Fa_LiteralExpr* literal = dynamic_cast<AST::Fa_LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->getType(), AST::Fa_LiteralExpr::Type::NIL);
}

TEST_F(ParserTest, ParseParenthesizedNumberLiteral)
{
    // (x)
    Fa_FileManager file_manager(parser_test_cases_dir() / "parenthesized_number.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();
    AST::Fa_LiteralExpr* literal = dynamic_cast<AST::Fa_LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->getType(), AST::Fa_LiteralExpr::Type::INTEGER);
}

TEST_F(ParserTest, ParseIdentifier)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "identifier.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();
    AST::Fa_NameExpr* name_Fa_Expr = dynamic_cast<AST::Fa_NameExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(name_Fa_Expr);

    ASSERT_NE(name_Fa_Expr, nullptr);
    EXPECT_EQ(name_Fa_Expr->getValue(), "المتنبي");
}

TEST_F(ParserTest, ParseCallExpressionNoArgs)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "call_expression.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr);

    AST::Fa_CallExpr* call_Fa_Expr = dynamic_cast<AST::Fa_CallExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_Fa_Expr);

    ASSERT_NE(call_Fa_Expr, nullptr);
    ASSERT_NE(call_Fa_Expr->getCallee(), nullptr);

    AST::Fa_NameExpr* callee_name = dynamic_cast<AST::Fa_NameExpr*>(call_Fa_Expr->getCallee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->getValue(), "اطبع");
    EXPECT_TRUE(call_Fa_Expr->getArgs().empty());
}

TEST_F(ParserTest, ParseCallExpressionWithOneArg)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "call_expression_with_one_argument.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr);

    AST::Fa_CallExpr* call_expr = dynamic_cast<AST::Fa_CallExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_expr);

    ASSERT_NE(call_expr, nullptr);
    ASSERT_NE(call_expr->getCallee(), nullptr);

    AST::Fa_NameExpr* callee_name = dynamic_cast<AST::Fa_NameExpr*>(call_expr->getCallee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->getValue(), "اطبع");

    auto const& args = call_expr->getArgs();

    EXPECT_FALSE(args.empty());
    /// TODO: check for each argument and their order
}

TEST_F(ParserTest, ParseNestedCallExpression)
{
    // f(g(x))
    Fa_FileManager file_manager(parser_test_cases_dir() / "nested_call_expression.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_CallExpr* outerCall = as<AST::Fa_CallExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(outerCall);

    EXPECT_EQ(as<AST::Fa_NameExpr>(outerCall->getCallee())->getValue(), "ا");

    AST::Fa_CallExpr* innerCall = as<AST::Fa_CallExpr>(outerCall->getArgsAsListExpr()->getElements()[0]);

    EXPECT_EQ(as<AST::Fa_NameExpr>(innerCall->getCallee())->getValue(), "ب");
    EXPECT_EQ(as<AST::Fa_NameExpr>(innerCall->getArgsAsListExpr()->getElements()[0])->getValue(), "د");
}

TEST_F(ParserTest, ParseSimpleAddition)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "simple_addition.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_BinaryExpr* bin = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->getOperator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(as<AST::Fa_NameExpr>(bin->getLeft())->getValue(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(bin->getRight())->getValue(), "ب");
}

TEST_F(ParserTest, ParseSimpleMultiplication)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "simple_multiplication.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_BinaryExpr* bin = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->getOperator(), AST::Fa_BinaryOp::OP_MUL);
    EXPECT_EQ(as<AST::Fa_NameExpr>(bin->getLeft())->getValue(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(bin->getRight())->getValue(), "ب");
}

TEST_F(ParserTest, ParseSimpleSubtraction)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "simple_subtraction.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_BinaryExpr* bin = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->getOperator(), AST::Fa_BinaryOp::OP_SUB);

    AST::Fa_NameExpr* lhs = as<AST::Fa_NameExpr>(bin->getLeft());
    AST::Fa_NameExpr* rhs = as<AST::Fa_NameExpr>(bin->getRight());

    EXPECT_EQ(lhs->getValue(), "ا");
    EXPECT_EQ(rhs->getValue(), "ب");
}

TEST_F(ParserTest, ParseSimpleDivision)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "simple_division.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_BinaryExpr* bin = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->getOperator(), AST::Fa_BinaryOp::OP_DIV);

    AST::Fa_NameExpr* lhs = as<AST::Fa_NameExpr>(bin->getLeft());
    AST::Fa_NameExpr* rhs = as<AST::Fa_NameExpr>(bin->getRight());

    EXPECT_EQ(lhs->getValue(), "ا");
    EXPECT_EQ(rhs->getValue(), "ب");
}

TEST_F(ParserTest, ParseComplexExpression)
{
    // 2 + 3 * 4  →  2 + (3 * 4)
    Fa_FileManager file_manager(parser_test_cases_dir() / "complex_expression.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_BinaryExpr* root = as<AST::Fa_BinaryExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(root);

    EXPECT_EQ(root->getOperator(), AST::Fa_BinaryOp::OP_ADD);

    AST::Fa_LiteralExpr* left = as<AST::Fa_LiteralExpr>(root->getLeft());
    EXPECT_EQ(left->getType(), AST::Fa_LiteralExpr::Type::INTEGER);
    EXPECT_EQ(left->toNumber(), 2);

    AST::Fa_BinaryExpr* mul = as<AST::Fa_BinaryExpr>(root->getRight());
    EXPECT_EQ(mul->getOperator(), AST::Fa_BinaryOp::OP_MUL);

    EXPECT_EQ(as<AST::Fa_LiteralExpr>(mul->getLeft())->toNumber(), 3);
    EXPECT_EQ(as<AST::Fa_LiteralExpr>(mul->getRight())->toNumber(), 4);
}

TEST_F(ParserTest, ParseNestedParentheses)
{
    // Test: ((2 + 3) * 4)
    Fa_FileManager file_manager(parser_test_cases_dir() / "nested_parens.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse nested parentheses expression";

    // Should be: AST::Fa_BinaryExpr((2 + 3), *, 4)
    AST::Fa_BinaryExpr* root = dynamic_cast<AST::Fa_BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be a AST::Fa_BinaryExpr";
    EXPECT_EQ(root->getOperator(), AST::Fa_BinaryOp::OP_MUL);

    // Left should be (2 + 3)
    AST::Fa_BinaryExpr* left_add = dynamic_cast<AST::Fa_BinaryExpr*>(root->getLeft());

    ASSERT_NE(left_add, nullptr) << "Left should be addition expression";
    EXPECT_EQ(left_add->getOperator(), AST::Fa_BinaryOp::OP_ADD);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedComparison)
{
    GTEST_SKIP() << "It isn't trivial if this feature should be in the lang at all";
    // Test: a < b < c (should parse as (a < b) < c due to left associativity)
    Fa_FileManager file_manager(parser_test_cases_dir() / "chained_comparison.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse chained comparison";

    AST::Fa_BinaryExpr* root = dynamic_cast<AST::Fa_BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be AST::Fa_BinaryExpr";
    EXPECT_EQ(root->getOperator(), AST::Fa_BinaryOp::OP_LT);

    // Left should be another comparison
    AST::Fa_BinaryExpr* left_comp = dynamic_cast<AST::Fa_BinaryExpr*>(root->getLeft());

    ASSERT_NE(left_comp, nullptr) << "Left should be comparison expression";
    EXPECT_EQ(left_comp->getOperator(), AST::Fa_BinaryOp::OP_LT);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseLogicalExpression)
{
    // Test: a and b or c (should be (a and b) or c)
    Fa_FileManager file_manager(parser_test_cases_dir() / "logical_expression.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse logical expression";

    AST::Fa_BinaryExpr* root = dynamic_cast<AST::Fa_BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be AST::Fa_BinaryExpr";
    EXPECT_EQ(root->getOperator(), AST::Fa_BinaryOp::OP_OR) << "Root should be OR (lower precedence)";

    // Left should be (a and b)
    AST::Fa_BinaryExpr* left_and = dynamic_cast<AST::Fa_BinaryExpr*>(root->getLeft());

    ASSERT_NE(left_and, nullptr) << "Left should be AND expression";
    EXPECT_EQ(left_and->getOperator(), AST::Fa_BinaryOp::OP_AND);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnaryChain)
{
    // Test: --x (f64 negation)
    Fa_FileManager file_manager(parser_test_cases_dir() / "unary_chain.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Failed to parse unary chain";

    AST::Fa_UnaryExpr* outer = dynamic_cast<AST::Fa_UnaryExpr*>(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be AST::Fa_UnaryExpr";
    EXPECT_EQ(outer->getOperator(), AST::Fa_UnaryOp::OP_NEG);

    AST::Fa_UnaryExpr* inner = dynamic_cast<AST::Fa_UnaryExpr*>(outer->getOperand());

    ASSERT_NE(inner, nullptr) << "Inner should be AST::Fa_UnaryExpr";
    EXPECT_EQ(inner->getOperator(), AST::Fa_UnaryOp::OP_NEG);

    AST::Fa_NameExpr* name = dynamic_cast<AST::Fa_NameExpr*>(inner->getOperand());

    ASSERT_NE(name, nullptr) << "Innermost should be AST::Fa_NameExpr";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseComplexFunctionCall)
{
    // func(a + b, c * d)
    Fa_FileManager file_manager(parser_test_cases_dir() / "complex_function_call.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_CallExpr* call = as<AST::Fa_CallExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(call);

    EXPECT_EQ(as<AST::Fa_NameExpr>(call->getCallee())->getValue(), "علم");

    AST::Fa_ListExpr* args = call->getArgsAsListExpr();
    ASSERT_NE(args, nullptr);
    ASSERT_FALSE(args->isEmpty());

    auto const& arg_list = args->getElements();
    ASSERT_EQ(arg_list.size(), 2);

    AST::Fa_BinaryExpr* arg1 = as<AST::Fa_BinaryExpr>(arg_list[0]);
    EXPECT_EQ(arg1->getOperator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(as<AST::Fa_NameExpr>(arg1->getLeft())->getValue(), "ا");
    EXPECT_EQ(as<AST::Fa_NameExpr>(arg1->getRight())->getValue(), "ب");

    AST::Fa_BinaryExpr* arg2 = as<AST::Fa_BinaryExpr>(arg_list[1]);
    EXPECT_EQ(arg2->getOperator(), AST::Fa_BinaryOp::OP_MUL);
    EXPECT_EQ(as<AST::Fa_NameExpr>(arg2->getLeft())->getValue(), "ت");
    EXPECT_EQ(as<AST::Fa_NameExpr>(arg2->getRight())->getValue(), "ث");
}

TEST_F(ParserTest, ParseInvalidSyntaxThrows)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "invalid_syntax.txt");
    Fa_Parser parser(&file_manager);
    auto expr = parser.parse();

    EXPECT_TRUE(expr.hasError()) << "Fa_Parser should return error for invalid syntax";
}

TEST_F(ParserTest, ParseMissingOperand)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "missing_operand.txt");
    Fa_Parser parser(&file_manager);
    auto expr = parser.parse();

    EXPECT_TRUE(expr.hasError()) << "Fa_Parser should not create valid expression with missing operand";
}

TEST_F(ParserTest, ParseUnmatchedParenthesis)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "unmatched_paren.txt");
    Fa_Parser parser(&file_manager);
    auto expr = parser.parse();

    EXPECT_TRUE(expr.hasError()) << "Fa_Parser should detect unmatched parenthesis";
}

TEST_F(ParserTest, ParseExtraClosingParenthesis)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "extra_paren.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse the valid part";
    EXPECT_FALSE(parser.weDone()) << "Should have unparsed tokens remaining";
    EXPECT_TRUE(parser.check(tok::Fa_TokenType::RPAREN)) << "Remaining token should be RPAREN";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnexpectedEOF)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "unexpected_eof.txt");
    Fa_Parser parser(&file_manager);
    auto expr = parser.parse();

    EXPECT_TRUE(expr.hasError());
}

TEST_F(ParserTest, ParseInvalidOperatorSequence)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "invalid_operator_seq.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    if (expr != nullptr) {
        if (test_config::print_ast)
            AST_Printer.print(expr);
        AST::Fa_BinaryExpr* binary = dynamic_cast<AST::Fa_BinaryExpr*>(expr);
        if (binary != nullptr) {
            EXPECT_NE(binary->getLeft(), nullptr) << "Left operand should exist";
            EXPECT_NE(binary->getRight(), nullptr) << "Right operand should exist";
        }
    }

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyInput)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "empty_input.txt");
    Fa_Parser parser(&file_manager);

    EXPECT_TRUE(parser.weDone()) << "Fa_Parser should recognize empty input immediately";

    auto expr = parser.parse();

    EXPECT_TRUE(expr.hasError());
}

TEST_F(ParserTest, ParseWhitespaceOnly)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "whitespace_only.txt");
    Fa_Parser parser(&file_manager);
    auto expr = parser.parse();

    EXPECT_TRUE(expr.hasError());
}

TEST_F(ParserTest, ParseSingleIdentifier)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "single_identifier.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse single identifier";

    AST::Fa_NameExpr* name = dynamic_cast<AST::Fa_NameExpr*>(expr);

    ASSERT_NE(name, nullptr) << "Should be AST::Fa_NameExpr";
    EXPECT_EQ(name->getValue(), "ا") << "Identifier value should be 'x'";
    EXPECT_TRUE(parser.weDone()) << "Should be at end after single identifier";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseVeryLongIdentifier)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "long_identifier.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse very long identifier";

    AST::Fa_NameExpr* name = dynamic_cast<AST::Fa_NameExpr*>(expr);

    ASSERT_NE(name, nullptr) << "Should be AST::Fa_NameExpr";

    Fa_StringRef value = name->getValue();

    EXPECT_GT(value.len(), 100) << "Identifier should be very long";
    EXPECT_LT(value.len(), 10000) << "Identifier should have reasonable upper bound";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnicodeIdentifiers)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "unicode_identifiers.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse Unicode identifiers";

    AST::Fa_BinaryExpr* binary = dynamic_cast<AST::Fa_BinaryExpr*>(expr);

    ASSERT_NE(binary, nullptr) << "Should be AST::Fa_BinaryExpr";

    AST::Fa_NameExpr* left = dynamic_cast<AST::Fa_NameExpr*>(binary->getLeft());
    AST::Fa_NameExpr* right = dynamic_cast<AST::Fa_NameExpr*>(binary->getRight());

    ASSERT_NE(left, nullptr) << "Left should be AST::Fa_NameExpr";
    ASSERT_NE(right, nullptr) << "Right should be AST::Fa_NameExpr";

    EXPECT_GT(left->getValue().len(), 0) << "Left identifier should not be empty";
    EXPECT_GT(right->getValue().len(), 0) << "Right identifier should not be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyList)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "empty_list.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse empty list";

    AST::Fa_ListExpr* list = dynamic_cast<AST::Fa_ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be AST::Fa_ListExpr";
    EXPECT_EQ(list->getElements().size(), 0) << "List should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyTuple)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "empty_tuple.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse empty tuple";

    AST::Fa_ListExpr* tuple = dynamic_cast<AST::Fa_ListExpr*>(expr);

    ASSERT_NE(tuple, nullptr) << "Should be AST::Fa_ListExpr (representing tuple)";
    EXPECT_EQ(tuple->getElements().size(), 0) << "Tuple should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseListWithTrailingComma)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "list_trailing_comma.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse list with trailing comma";

    AST::Fa_ListExpr* list = dynamic_cast<AST::Fa_ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be AST::Fa_ListExpr";
    EXPECT_EQ(list->getElements().size(), 3) << "Should have 3 elements despite trailing comma";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseNestedLists)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "nested_lists.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse nested lists";

    AST::Fa_ListExpr* outer_list = dynamic_cast<AST::Fa_ListExpr*>(expr);

    ASSERT_NE(outer_list, nullptr) << "Should be AST::Fa_ListExpr";
    EXPECT_EQ(outer_list->getElements().size(), 2) << "Outer list should have 2 elements";

    AST::Fa_ListExpr* inner1 = dynamic_cast<AST::Fa_ListExpr*>(outer_list->getElements()[0]);

    ASSERT_NE(inner1, nullptr) << "First element should be AST::Fa_ListExpr";
    EXPECT_EQ(inner1->getElements().size(), 2) << "First inner list should have 2 elements";

    AST::Fa_ListExpr* inner2 = dynamic_cast<AST::Fa_ListExpr*>(outer_list->getElements()[1]);

    ASSERT_NE(inner2, nullptr) << "Second element should be AST::Fa_ListExpr";
    EXPECT_EQ(inner2->getElements().size(), 2) << "Second inner list should have 2 elements";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseAssignment)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "assignment.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* node = parser.parse().value();
    ASSERT_NE(node, nullptr) << "Should parse assignment";

    AST::Fa_AssignmentExpr* assign = dynamic_cast<AST::Fa_AssignmentExpr*>(node);
    ASSERT_NE(assign, nullptr) << "Should be Fa_AssignmentExpr";

    AST::Fa_NameExpr* target = dynamic_cast<AST::Fa_NameExpr*>(assign->getTarget());

    ASSERT_NE(target, nullptr) << "Assignment target should not be null";
    EXPECT_EQ(target->getValue(), "ا") << "Target should be 'ا'";

    AST::Fa_LiteralExpr* value = dynamic_cast<AST::Fa_LiteralExpr*>(assign->getValue());

    ASSERT_NE(value, nullptr) << "Fa_Value should be AST::Fa_LiteralExpr";
    EXPECT_EQ(value->toNumber(), 42);

    if (test_config::print_ast)
        AST_Printer.print(assign);
}

TEST_F(ParserTest, ParseChainedAssignment)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "chained_assignment.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse chained assignment";

    AST::Fa_AssignmentExpr* outer = dynamic_cast<AST::Fa_AssignmentExpr*>(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be Fa_AssignmentExpr";
    EXPECT_EQ(static_cast<AST::Fa_NameExpr*>(outer->getTarget())->getValue(), "ا");

    AST::Fa_AssignmentExpr* inner = dynamic_cast<AST::Fa_AssignmentExpr*>(outer->getValue());

    ASSERT_NE(inner, nullptr) << "Inner value should be Fa_AssignmentExpr";
    EXPECT_EQ(static_cast<AST::Fa_NameExpr*>(inner->getTarget())->getValue(), "ب");

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedAssignmentWithExpr)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "chained_assignment_with_expression.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_AssignmentExpr* outer = as<AST::Fa_AssignmentExpr>(parser.parse().value());

    if (test_config::print_ast)
        AST_Printer.print(outer);

    AST::Fa_AssignmentExpr* inner = as<AST::Fa_AssignmentExpr>(outer->getValue());
    AST::Fa_BinaryExpr* binary = as<AST::Fa_BinaryExpr>(inner->getValue());

    EXPECT_EQ(static_cast<AST::Fa_NameExpr*>(outer->getTarget())->getValue(), "ا");
    EXPECT_EQ(static_cast<AST::Fa_NameExpr*>(inner->getTarget())->getValue(), "ب");
    EXPECT_EQ(binary->getOperator(), AST::Fa_BinaryOp::OP_ADD);
    EXPECT_EQ(as<AST::Fa_NameExpr>(binary->getLeft())->getValue(), "م");
    EXPECT_EQ(as<AST::Fa_NameExpr>(binary->getRight())->getValue(), "ل");
}

TEST_F(ParserTest, DISABLED_ParseLargeFile)
{
    GTEST_SKIP() << "DISABLED_ParseLargeFile: not checked yet";
    Fa_FileManager file_manager(parser_test_cases_dir() / "large_file.txt");
    Fa_Parser parser(&file_manager);
    auto start = std::chrono::high_resolution_clock::now();
    int Fa_Expr_count = 0;

    while (!parser.weDone()) {
        AST::Fa_Expr* expr = parser.parse().value();
        if (expr != nullptr)
            Fa_Expr_count++;
        else
            break;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Parsed " << Fa_Expr_count << " expressions in " << duration.count() << "ms" << std::endl;

    EXPECT_GT(Fa_Expr_count, 1000) << "Should parse many expressions";
    EXPECT_LT(duration.count(), 5000) << "Should complete in reasonable time";
}

TEST_F(ParserTest, ParseDeeplyNestedExpression)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "deeply_nested.txt");
    Fa_Parser parser(&file_manager);
    AST::Fa_Expr* expr = parser.parse().value();

    ASSERT_NE(expr, nullptr) << "Should parse deeply nested expression without stack overflow";

    if (test_config::print_ast) {
        AST_Printer.print(expr);
    }
}

TEST_F(ParserTest, ParseWhileLoop)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "while_loop.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_WhileStmt* while_stmt = as<AST::Fa_WhileStmt>(parser.parseWhileStmt().value());

    if (test_config::print_ast)
        AST_Printer.print(while_stmt);

    AST::Fa_BinaryExpr* cond = as<AST::Fa_BinaryExpr>(while_stmt->getCondition());
    AST::Fa_BlockStmt* block = as<AST::Fa_BlockStmt>(while_stmt->getBody());
    AST::Fa_AssignmentExpr* assign = as<AST::Fa_AssignmentExpr>(as<AST::Fa_ExprStmt>(block->getStatements()[0])->getExpr());

    EXPECT_EQ(as<AST::Fa_NameExpr>(cond->getLeft())->getValue(), "شيء");
    EXPECT_TRUE(as<AST::Fa_LiteralExpr>(cond->getRight())->getBool());
    EXPECT_EQ(cond->getOperator(), AST::Fa_BinaryOp::OP_EQ);
    ASSERT_FALSE(block->getStatements().empty());
    EXPECT_EQ(as<AST::Fa_NameExpr>(assign->getTarget())->getValue(), "بسبسمياو");
    EXPECT_FALSE(as<AST::Fa_LiteralExpr>(assign->getValue())->getBool());
}

TEST_F(ParserTest, ParseComplexeIfStatement)
{
    Fa_FileManager file_manager(parser_test_cases_dir() / "complexe_if_statement.txt");
    Fa_Parser parser(&file_manager);

    AST::Fa_IfStmt* if_stmt = as<AST::Fa_IfStmt>(parser.parseIfStmt().value());

    if (test_config::print_ast)
        AST_Printer.print(if_stmt);

    AST::Fa_BinaryExpr* cond = as<AST::Fa_BinaryExpr>(if_stmt->getCondition());
    // the while statement is wrapped in a block inside the else clause
    AST::Fa_WhileStmt* while_stmt = as<AST::Fa_WhileStmt>(dynamic_cast<AST::Fa_BlockStmt*>(if_stmt->getThen())->getStatements()[0]);
    AST::Fa_BinaryExpr* while_cond = as<AST::Fa_BinaryExpr>(while_stmt->getCondition());
    AST::Fa_BlockStmt* block = as<AST::Fa_BlockStmt>(while_stmt->getBody());
    AST::Fa_AssignmentExpr* assign = as<AST::Fa_AssignmentExpr>(as<AST::Fa_ExprStmt>(block->getStatements()[0])->getExpr());

    EXPECT_EQ(as<AST::Fa_NameExpr>(cond->getLeft())->getValue(), "شيء");
    EXPECT_TRUE(as<AST::Fa_LiteralExpr>(cond->getRight())->getBool());
    EXPECT_EQ(cond->getOperator(), AST::Fa_BinaryOp::OP_NEQ);
    EXPECT_EQ(as<AST::Fa_NameExpr>(while_cond->getLeft())->getValue(), "شيء");
    EXPECT_TRUE(as<AST::Fa_LiteralExpr>(while_cond->getRight())->getBool());
    EXPECT_EQ(while_cond->getOperator(), AST::Fa_BinaryOp::OP_EQ);
    ASSERT_FALSE(block->getStatements().empty());
    EXPECT_EQ(as<AST::Fa_NameExpr>(assign->getTarget())->getValue(), "بسبسمياو");
    EXPECT_FALSE(as<AST::Fa_LiteralExpr>(assign->getValue())->getBool());
}
