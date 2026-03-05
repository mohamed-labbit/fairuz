#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <vector>

#include "../../include/ast.hpp"
#include "../../include/ast_printer.hpp"
#include "../../include/lex/file_manager.hpp"
#include "../../include/lex/lexer.hpp"
#include "../../include/parser/parser.hpp"
#include "../test_config.h"

using namespace mylang;
using namespace mylang::ast;
using namespace mylang::parser;
using namespace mylang::lex;

namespace {

std::filesystem::path parser_test_cases_dir()
{
    static auto const dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
    return dir;
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
    T* parseAndCast(Parser& parser, Expr*& expr)
    {
        EXPECT_NE(expr, nullptr) << "Expression should not be null";
        if (!expr)
            return nullptr;

        T* casted = reinterpret_cast<T*>(expr);
        EXPECT_NE(casted, nullptr) << "Failed to cast to expected type";
        return casted;
    }

    template<typename T>
    T* as(Stmt* node)
    {
        T* casted = dynamic_cast<T*>(node);
        EXPECT_NE(casted, nullptr);
        return casted;
    }

    template<typename T>
    T* as(Expr* node)
    {
        T* casted = dynamic_cast<T*>(node);
        EXPECT_NE(casted, nullptr);
        return casted;
    }
};

inline ASTPrinter AST_Printer;

// Literal Expression Tests

TEST_F(ParserTest, ParseLiteral)
{
    FileManager file_manager_0(parser_test_cases_dir() / "number_literal.txt");
    FileManager file_manager_1(parser_test_cases_dir() / "string_literal.txt");
    FileManager file_manager_2(parser_test_cases_dir() / "boolean_literal_true.txt");
    FileManager file_manager_3(parser_test_cases_dir() / "boolean_literal_false.txt");

    Parser parser_0(&file_manager_0);
    Parser parser_1(&file_manager_1);
    Parser parser_2(&file_manager_2);
    Parser parser_3(&file_manager_3);

    EXPECT_EQ(dynamic_cast<LiteralExpr*>(parser_0.parse())->getType(), LiteralExpr::Type::INTEGER) << "Should parse integer literal";
    EXPECT_EQ(dynamic_cast<LiteralExpr*>(parser_1.parse())->getType(), LiteralExpr::Type::STRING) << "Should parse string literal";
    EXPECT_EQ(dynamic_cast<LiteralExpr*>(parser_2.parse())->getType(), LiteralExpr::Type::BOOLEAN) << "Should parse true bool literal";
    EXPECT_EQ(dynamic_cast<LiteralExpr*>(parser_3.parse())->getType(), LiteralExpr::Type::BOOLEAN) << "Should parse false bool literal";
}

TEST_F(ParserTest, ParseNoneLiteral)
{
    GTEST_SKIP() << "Not supported yet";
    FileManager file_manager(parser_test_cases_dir() / "none_literal.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();
    LiteralExpr* literal = dynamic_cast<LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->getType(), LiteralExpr::Type::NIL);
}

TEST_F(ParserTest, ParseParenthesizedNumberLiteral)
{
    // (x)
    FileManager file_manager(parser_test_cases_dir() / "parenthesized_number.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();
    LiteralExpr* literal = dynamic_cast<LiteralExpr*>(expr);

    if (test_config::print_ast)
        AST_Printer.print(literal);

    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->getType(), LiteralExpr::Type::INTEGER);
}

// Name/Identifier Expression Tests

TEST_F(ParserTest, ParseIdentifier)
{
    FileManager file_manager(parser_test_cases_dir() / "identifier.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();
    NameExpr* name_expr = dynamic_cast<NameExpr*>(expr);

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
  auto tokens = createTokens({BinaryOp::NAME});
  // ...
}
*/

// Call Expression Tests
TEST_F(ParserTest, ParseCallExpressionNoArgs)
{
    FileManager file_manager(parser_test_cases_dir() / "call_expression.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr);

    CallExpr* call_expr = dynamic_cast<CallExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_expr);

    ASSERT_NE(call_expr, nullptr);
    ASSERT_NE(call_expr->getCallee(), nullptr);

    NameExpr* callee_name = dynamic_cast<NameExpr*>(call_expr->getCallee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->getValue(), "اطبع");
    EXPECT_TRUE(call_expr->getArgs().empty());
}

TEST_F(ParserTest, ParseCallExpressionWithOneArg)
{
    FileManager file_manager(parser_test_cases_dir() / "call_expression_with_one_argument.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr);

    CallExpr* call_expr = dynamic_cast<CallExpr*>(expr);
    if (test_config::print_ast)
        AST_Printer.print(call_expr);

    ASSERT_NE(call_expr, nullptr);
    ASSERT_NE(call_expr->getCallee(), nullptr);

    NameExpr* callee_name = dynamic_cast<NameExpr*>(call_expr->getCallee());

    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->getValue(), "اطبع");

    std::vector<Expr*> const& args = call_expr->getArgs();

    EXPECT_FALSE(args.empty());
    /// TODO: check for each argument and their order
}

TEST_F(ParserTest, ParseNestedCallExpression)
{
    // f(g(x))
    FileManager file_manager(parser_test_cases_dir() / "nested_call_expression.txt");
    Parser parser(&file_manager);

    CallExpr* outerCall = as<CallExpr>(parser.parse());

    if (test_config::print_ast)
        AST_Printer.print(outerCall);

    EXPECT_EQ(as<NameExpr>(outerCall->getCallee())->getValue(), "ا");

    CallExpr* innerCall = as<CallExpr>(outerCall->getArgsAsListExpr()->getElements()[0]);

    EXPECT_EQ(as<NameExpr>(innerCall->getCallee())->getValue(), "ب");
    EXPECT_EQ(as<NameExpr>(innerCall->getArgsAsListExpr()->getElements()[0])->getValue(), "د");
}

// Binary Expression Tests

TEST_F(ParserTest, ParseSimpleAddition)
{
    FileManager file_manager(parser_test_cases_dir() / "simple_addition.txt");
    Parser parser(&file_manager);

    BinaryExpr* bin = as<BinaryExpr>(parser.parse());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->getOperator(), BinaryOp::OP_ADD);
    EXPECT_EQ(as<NameExpr>(bin->getLeft())->getValue(), "ا");
    EXPECT_EQ(as<NameExpr>(bin->getRight())->getValue(), "ب");
}

TEST_F(ParserTest, ParseSimpleMultiplication)
{
    FileManager file_manager(parser_test_cases_dir() / "simple_multiplication.txt");
    Parser parser(&file_manager);

    BinaryExpr* bin = as<BinaryExpr>(parser.parse());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->getOperator(), BinaryOp::OP_MUL);
    EXPECT_EQ(as<NameExpr>(bin->getLeft())->getValue(), "ا");
    EXPECT_EQ(as<NameExpr>(bin->getRight())->getValue(), "ب");
}

TEST_F(ParserTest, ParseSimpleSubtraction)
{
    FileManager file_manager(parser_test_cases_dir() / "simple_subtraction.txt");
    Parser parser(&file_manager);

    BinaryExpr* bin = as<BinaryExpr>(parser.parse());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->getOperator(), BinaryOp::OP_SUB);

    NameExpr* lhs = as<NameExpr>(bin->getLeft());
    NameExpr* rhs = as<NameExpr>(bin->getRight());

    EXPECT_EQ(lhs->getValue(), "ا");
    EXPECT_EQ(rhs->getValue(), "ب");
}

TEST_F(ParserTest, ParseSimpleDivision)
{
    FileManager file_manager(parser_test_cases_dir() / "simple_division.txt");
    Parser parser(&file_manager);

    BinaryExpr* bin = as<BinaryExpr>(parser.parse());

    if (test_config::print_ast)
        AST_Printer.print(bin);

    EXPECT_EQ(bin->getOperator(), BinaryOp::OP_DIV);

    NameExpr* lhs = as<NameExpr>(bin->getLeft());
    NameExpr* rhs = as<NameExpr>(bin->getRight());

    EXPECT_EQ(lhs->getValue(), "ا");
    EXPECT_EQ(rhs->getValue(), "ب");
}

// Complex Expression Tests

TEST_F(ParserTest, ParseComplexExpression)
{
    // 2 + 3 * 4  →  2 + (3 * 4)
    FileManager file_manager(parser_test_cases_dir() / "complex_expression.txt");
    Parser parser(&file_manager);

    BinaryExpr* root = as<BinaryExpr>(parser.parse());

    if (test_config::print_ast)
        AST_Printer.print(root);

    EXPECT_EQ(root->getOperator(), BinaryOp::OP_ADD);

    LiteralExpr* left = as<LiteralExpr>(root->getLeft());
    EXPECT_EQ(left->getType(), LiteralExpr::Type::INTEGER);
    EXPECT_EQ(left->toNumber(), 2);

    BinaryExpr* mul = as<BinaryExpr>(root->getRight());
    EXPECT_EQ(mul->getOperator(), BinaryOp::OP_MUL);

    EXPECT_EQ(as<LiteralExpr>(mul->getLeft())->toNumber(), 3);
    EXPECT_EQ(as<LiteralExpr>(mul->getRight())->toNumber(), 4);
}

TEST_F(ParserTest, ParseNestedParentheses)
{
    // Test: ((2 + 3) * 4)
    FileManager file_manager(parser_test_cases_dir() / "nested_parens.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse nested parentheses expression";

    // Should be: BinaryExpr((2 + 3), *, 4)
    BinaryExpr* root = dynamic_cast<BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be a BinaryExpr";
    EXPECT_EQ(root->getOperator(), BinaryOp::OP_MUL);

    // Left should be (2 + 3)
    BinaryExpr* left_add = dynamic_cast<BinaryExpr*>(root->getLeft());

    ASSERT_NE(left_add, nullptr) << "Left should be addition expression";
    EXPECT_EQ(left_add->getOperator(), BinaryOp::OP_ADD);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedComparison)
{
    GTEST_SKIP() << "It isn't trivial if this feature should be in the lang at all";
    // Test: a < b < c (should parse as (a < b) < c due to left associativity)
    FileManager file_manager(parser_test_cases_dir() / "chained_comparison.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse chained comparison";

    BinaryExpr* root = dynamic_cast<BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be BinaryExpr";
    EXPECT_EQ(root->getOperator(), BinaryOp::OP_LT);

    // Left should be another comparison
    BinaryExpr* left_comp = dynamic_cast<BinaryExpr*>(root->getLeft());

    ASSERT_NE(left_comp, nullptr) << "Left should be comparison expression";
    EXPECT_EQ(left_comp->getOperator(), BinaryOp::OP_LT);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseLogicalExpression)
{
    // Test: a and b or c (should be (a and b) or c)
    FileManager file_manager(parser_test_cases_dir() / "logical_expression.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse logical expression";

    BinaryExpr* root = dynamic_cast<BinaryExpr*>(expr);

    ASSERT_NE(root, nullptr) << "Root should be BinaryExpr";
    EXPECT_EQ(root->getOperator(), BinaryOp::OP_OR) << "Root should be OR (lower precedence)";

    // Left should be (a and b)
    BinaryExpr* left_and = dynamic_cast<BinaryExpr*>(root->getLeft());

    ASSERT_NE(left_and, nullptr) << "Left should be AND expression";
    EXPECT_EQ(left_and->getOperator(), BinaryOp::OP_AND);

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnaryChain)
{
    // Test: --x (double negation)
    FileManager file_manager(parser_test_cases_dir() / "unary_chain.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Failed to parse unary chain";

    UnaryExpr* outer = dynamic_cast<UnaryExpr*>(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be UnaryExpr";
    EXPECT_EQ(outer->getOperator(), UnaryOp::OP_NEG);

    UnaryExpr* inner = dynamic_cast<UnaryExpr*>(outer->getOperand());

    ASSERT_NE(inner, nullptr) << "Inner should be UnaryExpr";
    EXPECT_EQ(inner->getOperator(), UnaryOp::OP_NEG);

    NameExpr* name = dynamic_cast<NameExpr*>(inner->getOperand());

    ASSERT_NE(name, nullptr) << "Innermost should be NameExpr";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseComplexFunctionCall)
{
    // func(a + b, c * d)
    FileManager file_manager(parser_test_cases_dir() / "complex_function_call.txt");
    Parser parser(&file_manager);

    CallExpr* call = as<CallExpr>(parser.parse());

    if (test_config::print_ast)
        AST_Printer.print(call);

    EXPECT_EQ(as<NameExpr>(call->getCallee())->getValue(), "علم");

    ListExpr* args = call->getArgsAsListExpr();
    ASSERT_NE(args, nullptr);
    ASSERT_FALSE(args->isEmpty());

    auto const& arg_list = args->getElements();
    ASSERT_EQ(arg_list.size(), 2);

    BinaryExpr* arg1 = as<BinaryExpr>(arg_list[0]);
    EXPECT_EQ(arg1->getOperator(), BinaryOp::OP_ADD);
    EXPECT_EQ(as<NameExpr>(arg1->getLeft())->getValue(), "ا");
    EXPECT_EQ(as<NameExpr>(arg1->getRight())->getValue(), "ب");

    BinaryExpr* arg2 = as<BinaryExpr>(arg_list[1]);
    EXPECT_EQ(arg2->getOperator(), BinaryOp::OP_MUL);
    EXPECT_EQ(as<NameExpr>(arg2->getLeft())->getValue(), "ت");
    EXPECT_EQ(as<NameExpr>(arg2->getRight())->getValue(), "ث");
}

// Error Handling Tests
TEST_F(ParserTest, ParseInvalidSyntaxThrows)
{
    // GTEST_SKIP() << "ParseInvalidSyntaxThrows: not checked yet";
    // Test: + + (invalid: operator without operands)
    FileManager file_manager(parser_test_cases_dir() / "invalid_syntax.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

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
    FileManager file_manager(parser_test_cases_dir() / "missing_operand.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    // Should handle gracefully
    if (expr != nullptr) {
        // If it parsed something, verify it's not complete
        BinaryExpr* binary = dynamic_cast<BinaryExpr*>(expr);
        if (binary != nullptr) // Right side might be null or invalid
            EXPECT_TRUE(binary->getRight() == nullptr || dynamic_cast<NameExpr*>(binary->getRight()) == nullptr)
                << "Parser should not create valid expression with missing operand";
    }

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseUnmatchedParenthesis)
{
    // GTEST_SKIP() << "ParseUnmatchedParenthesis: not checked yet";
    // Test: (a + b (missing closing paren)
    FileManager file_manager(parser_test_cases_dir() / "unmatched_paren.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    // Should fail to parse or return incomplete expression
    EXPECT_TRUE(expr == nullptr) << "Parser should detect unmatched parenthesis";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseExtraClosingParenthesis)
{
    // GTEST_SKIP() << "ParseExtraClosingParenthesis: not checked yet";
    // Test: a + b) (extra closing paren)
    FileManager file_manager(parser_test_cases_dir() / "extra_paren.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

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
    FileManager file_manager(parser_test_cases_dir() / "unexpected_eof.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    // Parser should return what it can parse or nullptr
    if (expr != nullptr) {
        BinaryExpr* binary = dynamic_cast<BinaryExpr*>(expr);
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
    FileManager file_manager(parser_test_cases_dir() / "invalid_operator_seq.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    // Should handle gracefully - might parse as a + (+b) or fail
    if (expr != nullptr) {
        if (test_config::print_ast)
            AST_Printer.print(expr);
        // Verify structure makes sense even if semantically wrong
        BinaryExpr* binary = dynamic_cast<BinaryExpr*>(expr);
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
    FileManager file_manager(parser_test_cases_dir() / "empty_input.txt");
    Parser parser(&file_manager);

    EXPECT_TRUE(parser.weDone()) << "Parser should recognize empty input immediately";

    Expr* expr = parser.parse();

    EXPECT_EQ(expr, nullptr) << "Should return nullptr for empty input";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseWhitespaceOnly)
{
    // GTEST_SKIP() << "ParseWhitespaceOnly: not checked yet";
    // Test: file with only whitespace and newlines
    FileManager file_manager(parser_test_cases_dir() / "whitespace_only.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    EXPECT_EQ(expr, nullptr) << "Should return nullptr for whitespace-only input";

    // EXPECT_TRUE(parser.weDone()) << "Should reach end marker after whitespace";
    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseSingleIdentifier)
{
    // Test: just "x"
    FileManager file_manager(parser_test_cases_dir() / "single_identifier.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse single identifier";

    NameExpr* name = dynamic_cast<NameExpr*>(expr);

    ASSERT_NE(name, nullptr) << "Should be NameExpr";
    EXPECT_EQ(name->getValue(), "ا") << "Identifier value should be 'x'";
    EXPECT_TRUE(parser.weDone()) << "Should be at end after single identifier";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseVeryLongIdentifier)
{
    // Test: extremely long identifier (1000+ characters)
    FileManager file_manager(parser_test_cases_dir() / "long_identifier.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse very long identifier";

    NameExpr* name = dynamic_cast<NameExpr*>(expr);

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
    FileManager file_manager(parser_test_cases_dir() / "unicode_identifiers.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse Unicode identifiers";

    BinaryExpr* binary = dynamic_cast<BinaryExpr*>(expr);

    ASSERT_NE(binary, nullptr) << "Should be BinaryExpr";

    NameExpr* left = dynamic_cast<NameExpr*>(binary->getLeft());
    NameExpr* right = dynamic_cast<NameExpr*>(binary->getRight());

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
    FileManager file_manager(parser_test_cases_dir() / "empty_list.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse empty list";

    ListExpr* list = dynamic_cast<ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be ListExpr";
    EXPECT_EQ(list->getElements().size(), 0) << "List should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseEmptyTuple)
{
    // Test: ()
    FileManager file_manager(parser_test_cases_dir() / "empty_tuple.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse empty tuple";

    ListExpr* tuple = dynamic_cast<ListExpr*>(expr);

    ASSERT_NE(tuple, nullptr) << "Should be ListExpr (representing tuple)";
    EXPECT_EQ(tuple->getElements().size(), 0) << "Tuple should be empty";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseListWithTrailingComma)
{
    // GTEST_SKIP() << "ParseListWithTrailingComma: not checked yet";
    // Test: [1, 2, 3,]
    FileManager file_manager(parser_test_cases_dir() / "list_trailing_comma.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse list with trailing comma";

    ListExpr* list = dynamic_cast<ListExpr*>(expr);

    ASSERT_NE(list, nullptr) << "Should be ListExpr";
    EXPECT_EQ(list->getElements().size(), 3) << "Should have 3 elements despite trailing comma";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseNestedLists)
{
    // GTEST_SKIP() << "ParseNestedLists: not checked yet";
    // Test: [[1, 2], [3, 4]]
    FileManager file_manager(parser_test_cases_dir() / "nested_lists.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse nested lists";

    ListExpr* outer_list = dynamic_cast<ListExpr*>(expr);

    ASSERT_NE(outer_list, nullptr) << "Should be ListExpr";
    EXPECT_EQ(outer_list->getElements().size(), 2) << "Outer list should have 2 elements";

    // Check first inner list
    ListExpr* inner1 = dynamic_cast<ListExpr*>(outer_list->getElements()[0]);

    ASSERT_NE(inner1, nullptr) << "First element should be ListExpr";
    EXPECT_EQ(inner1->getElements().size(), 2) << "First inner list should have 2 elements";

    // Check second inner list
    ListExpr* inner2 = dynamic_cast<ListExpr*>(outer_list->getElements()[1]);

    ASSERT_NE(inner2, nullptr) << "Second element should be ListExpr";
    EXPECT_EQ(inner2->getElements().size(), 2) << "Second inner list should have 2 elements";

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseAssignment)
{
    // Test: x := 42
    FileManager file_manager(parser_test_cases_dir() / "assignment.txt");
    Parser parser(&file_manager);
    Expr* node = parser.parse();
    ASSERT_NE(node, nullptr) << "Should parse assignment";

    AssignmentExpr* assign = dynamic_cast<AssignmentExpr*>(node);
    ASSERT_NE(assign, nullptr) << "Should be AssignmentExpr";

    NameExpr* target = dynamic_cast<NameExpr*>(assign->getTarget());

    ASSERT_NE(target, nullptr) << "Assignment target should not be null";
    EXPECT_EQ(target->getValue(), "ا") << "Target should be 'ا'";

    LiteralExpr* value = dynamic_cast<LiteralExpr*>(assign->getValue());

    ASSERT_NE(value, nullptr) << "Value should be LiteralExpr";
    EXPECT_EQ(value->toNumber(), 42);

    if (test_config::print_ast)
        AST_Printer.print(assign);
}

TEST_F(ParserTest, ParseChainedAssignment)
{
    // Test: x := y := 5 (should be right associative: x := (y := 5))
    FileManager file_manager(parser_test_cases_dir() / "chained_assignment.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse chained assignment";

    AssignmentExpr* outer = dynamic_cast<AssignmentExpr*>(expr);

    ASSERT_NE(outer, nullptr) << "Outer should be AssignmentExpr";
    EXPECT_EQ(outer->getTarget()->getValue(), "ا");

    AssignmentExpr* inner = dynamic_cast<AssignmentExpr*>(outer->getValue());

    ASSERT_NE(inner, nullptr) << "Inner value should be AssignmentExpr";
    EXPECT_EQ(inner->getTarget()->getValue(), "ب");

    if (test_config::print_ast)
        AST_Printer.print(expr);
}

TEST_F(ParserTest, ParseChainedAssignmentWithExpr)
{
    // x := y := (a + b)
    FileManager file_manager(parser_test_cases_dir() / "chained_assignment_with_expression.txt");
    Parser parser(&file_manager);

    AssignmentExpr* outer = as<AssignmentExpr>(parser.parse());

    if (test_config::print_ast)
        AST_Printer.print(outer);

    AssignmentExpr* inner = as<AssignmentExpr>(outer->getValue());
    BinaryExpr* binary = as<BinaryExpr>(inner->getValue());

    EXPECT_EQ(outer->getTarget()->getValue(), "ا");
    EXPECT_EQ(inner->getTarget()->getValue(), "ب");
    EXPECT_EQ(binary->getOperator(), BinaryOp::OP_ADD);
    EXPECT_EQ(as<NameExpr>(binary->getLeft())->getValue(), "م");
    EXPECT_EQ(as<NameExpr>(binary->getRight())->getValue(), "ل");
}

// Performance Tests (Optional)

TEST_F(ParserTest, DISABLED_ParseLargeFile)
{
    GTEST_SKIP() << "DISABLED_ParseLargeFile: not checked yet";
    // Disabled by default, enable for performance testing
    // Test: file with thousands of expressions
    FileManager file_manager(parser_test_cases_dir() / "large_file.txt");
    Parser parser(&file_manager);
    auto start = std::chrono::high_resolution_clock::now();
    int expr_count = 0;

    while (!parser.weDone()) {
        Expr* expr = parser.parse();
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
    FileManager file_manager(parser_test_cases_dir() / "deeply_nested.txt");
    Parser parser(&file_manager);
    Expr* expr = parser.parse();

    ASSERT_NE(expr, nullptr) << "Should parse deeply nested expression without stack overflow";

    // Unwrap to find the innermost expression
    /*
    int                        depth   = 0;
    Expr* current = expr;
    while (current != nullptr && depth < 200)
    {
      // In a real implementation, parenthesized expressions might be transparent
      // or wrapped in some node type
      depth++;
      NameExpr* name = dynamic_cast<NameExpr*>(current);
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
    FileManager file_manager(parser_test_cases_dir() / "while_loop.txt");
    Parser parser(&file_manager);

    WhileStmt* while_stmt = as<WhileStmt>(parser.parseWhileStmt());

    if (test_config::print_ast)
        AST_Printer.print(while_stmt);

    BinaryExpr* cond = as<BinaryExpr>(while_stmt->getCondition());
    BlockStmt* block = as<BlockStmt>(while_stmt->getBlock());
    AssignmentExpr* assign = as<AssignmentExpr>(as<ExprStmt>(block->getStatements()[0])->getExpr());

    EXPECT_EQ(as<NameExpr>(cond->getLeft())->getValue(), "شيء");
    EXPECT_TRUE(as<LiteralExpr>(cond->getRight())->getBool());
    EXPECT_EQ(cond->getOperator(), BinaryOp::OP_EQ);
    ASSERT_FALSE(block->getStatements().empty());
    EXPECT_EQ(as<NameExpr>(assign->getTarget())->getValue(), "بسبسمياو");
    EXPECT_FALSE(as<LiteralExpr>(assign->getValue())->getBool());
}

TEST_F(ParserTest, ParseComplexeIfStatement)
{
    FileManager file_manager(parser_test_cases_dir() / "complexe_if_statement.txt");
    Parser parser(&file_manager);

    IfStmt* if_stmt = as<IfStmt>(parser.parseIfStmt());

    if (test_config::print_ast)
        AST_Printer.print(if_stmt);

    BinaryExpr* cond = as<BinaryExpr>(if_stmt->getCondition());
    WhileStmt* while_stmt = as<WhileStmt>(if_stmt->getThenBlock()->getStatements()[0]);
    BinaryExpr* while_cond = as<BinaryExpr>(while_stmt->getCondition());
    BlockStmt* block = as<BlockStmt>(while_stmt->getBlock());
    AssignmentExpr* assign = as<AssignmentExpr>(as<ExprStmt>(block->getStatements()[0])->getExpr());

    EXPECT_EQ(as<NameExpr>(cond->getLeft())->getValue(), "شيء");
    EXPECT_TRUE(as<LiteralExpr>(cond->getRight())->getBool());
    EXPECT_EQ(cond->getOperator(), BinaryOp::OP_NEQ);
    EXPECT_EQ(as<NameExpr>(while_cond->getLeft())->getValue(), "شيء");
    EXPECT_TRUE(as<LiteralExpr>(while_cond->getRight())->getBool());
    EXPECT_EQ(while_cond->getOperator(), BinaryOp::OP_EQ);
    ASSERT_FALSE(block->getStatements().empty());
    EXPECT_EQ(as<NameExpr>(assign->getTarget())->getValue(), "بسبسمياو");
    EXPECT_FALSE(as<LiteralExpr>(assign->getValue())->getBool());
}
