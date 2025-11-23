#include "../../include/lex/lexer.hpp"
#include "../../include/lex/token.hpp"
#include "../../include/parser/ast/ast.hpp"
#include "../../include/parser/parser.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace {
std::filesystem::path parser_test_cases_dir()
{
    static const auto dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
    return dir;
}
}

class ParserTest: public ::testing::Test
{
   protected:
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

    mylang::lex::tok::Token makeToken(
      mylang::lex::tok::TokenType type, const std::u16string& value = u"", int line = 1, int col = 1)
    {
        mylang::lex::Lexer lexer;
        return lexer.make_token(type, value, line, col);
    }
};

// ============================================================================
// ParseError Tests
// ============================================================================

TEST(ParseErrorTest, BasicConstruction)
{
    mylang::parser::ParseError err(u"Test error", 10, 5, u"context line", {u"suggestion1"});
    EXPECT_EQ(err.line, 10);
    EXPECT_EQ(err.column, 5);
    EXPECT_EQ(err.context, u"context line");
    ASSERT_EQ(err.suggestions.size(), 1);
    EXPECT_EQ(err.suggestions[0], u"suggestion1");
}

TEST(ParseErrorTest, FormatMethod)
{
    mylang::parser::ParseError err(u"Syntax error", 5, 10);
    std::u16string formatted = err.format();
    EXPECT_FALSE(formatted.empty());
}

TEST(ParseErrorTest, MultipleSuggestions)
{
    std::vector<std::u16string> suggestions = {u"اذا", u"طالما", u"لكل"};
    mylang::parser::ParseError err(u"Unknown keyword", 1, 1, u"", suggestions);
    EXPECT_EQ(err.suggestions.size(), 3);
}

// ============================================================================
// Primary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseNumberLiteral)
{
    std::ifstream file(parser_test_cases_dir() / "number_literal.txt", std::ios::binary);
    mylang::parser::Parser parser(&file);
    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
    auto* literal = dynamic_cast<mylang::parser::ast::LiteralExpr*>(expr.get());
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->literalType, mylang::parser::ast::LiteralExpr::Type::NUMBER);
}

TEST_F(ParserTest, ParseStringLiteral)
{
    std::ifstream file(parser_test_cases_dir() / "string_literal.txt", std::ios::binary);
    mylang::parser::Parser parser(&file);
    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
    auto* literal = dynamic_cast<mylang::parser::ast::LiteralExpr*>(expr.get());
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->literalType, mylang::parser::ast::LiteralExpr::Type::STRING);
}

TEST_F(ParserTest, ParseBooleanLiterals)
{
    std::ifstream file_true(parser_test_cases_dir() / "boolean_literal_true.txt", std::ios::binary);
    mylang::parser::Parser parser_true(&file_true);
    auto expr_true = parser_true.parsePrimary();
    ASSERT_NE(expr_true, nullptr);
    auto* literal_true = dynamic_cast<mylang::parser::ast::LiteralExpr*>(expr_true.get());
    ASSERT_NE(literal_true, nullptr);
    EXPECT_EQ(literal_true->literalType, mylang::parser::ast::LiteralExpr::Type::BOOLEAN);
    file_true.close();

    std::ifstream file_false(parser_test_cases_dir() / "boolean_literal_false.txt", std::ios::binary);
    mylang::parser::Parser parser_false(&file_false);
    auto expr_false = parser_false.parsePrimary();
    ASSERT_NE(expr_false, nullptr);
    auto* literal_false = dynamic_cast<mylang::parser::ast::LiteralExpr*>(expr_false.get());
    ASSERT_NE(literal_false, nullptr);
    EXPECT_EQ(literal_false->literalType, mylang::parser::ast::LiteralExpr::Type::BOOLEAN);
}

TEST_F(ParserTest, ParseNoneLiteral)
{
    std::ifstream file(parser_test_cases_dir() / "none_literal.txt", std::ios::binary);
    mylang::parser::Parser parser(&file);
    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
    auto* literal = dynamic_cast<mylang::parser::ast::LiteralExpr*>(expr.get());
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->literalType, mylang::parser::ast::LiteralExpr::Type::NONE);
}

TEST_F(ParserTest, ParseParenthesizedNumberLiteral)
{
    std::ifstream file(parser_test_cases_dir() / "parenthesized_number.txt", std::ios::binary);
    mylang::parser::Parser parser(&file);
    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
    auto* literal = dynamic_cast<mylang::parser::ast::LiteralExpr*>(expr.get());
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->literalType, mylang::parser::ast::LiteralExpr::Type::NUMBER);
}

TEST_F(ParserTest, ParseIdentifierFromTokens)
{
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::BEGINMARKER),
      makeToken(mylang::lex::tok::TokenType::NAME, u"المتنبي"), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
    auto* name_expr = dynamic_cast<mylang::parser::ast::NameExpr*>(expr.get());
    ASSERT_NE(name_expr, nullptr);
    EXPECT_EQ(name_expr->name, u"المتنبي");
}

TEST_F(ParserTest, ParseCallExpressionFromTokens)
{
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::BEGINMARKER),
      makeToken(mylang::lex::tok::TokenType::NAME, u"اطبع"), makeToken(mylang::lex::tok::TokenType::LPAREN),
      makeToken(mylang::lex::tok::TokenType::RPAREN), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);

    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);

    auto* call_expr = dynamic_cast<mylang::parser::ast::CallExpr*>(expr.get());
    ASSERT_NE(call_expr, nullptr);
    auto* callee_name = dynamic_cast<mylang::parser::ast::NameExpr*>(call_expr->callee.get());
    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->name, u"اطبع");
    EXPECT_TRUE(call_expr->args.empty());
}

// ============================================================================
// Operator Tests
// ============================================================================

TEST_F(ParserTest, IsUnaryOperator)
{
    auto tokens = createTokens({});
    mylang::parser::Parser parser(tokens);

    EXPECT_TRUE(parser.isUnaryOp(mylang::lex::tok::TokenType::OP_MINUS));
    EXPECT_TRUE(parser.isUnaryOp(mylang::lex::tok::TokenType::OP_PLUS));
    EXPECT_TRUE(parser.isUnaryOp(mylang::lex::tok::TokenType::OP_BITNOT));
    EXPECT_TRUE(parser.isUnaryOp(mylang::lex::tok::TokenType::KW_NOT));
    EXPECT_FALSE(parser.isUnaryOp(mylang::lex::tok::TokenType::OP_STAR));
}

TEST_F(ParserTest, IsBinaryOperator)
{
    auto tokens = createTokens({});
    mylang::parser::Parser parser(tokens);

    EXPECT_TRUE(parser.isBinaryOp(mylang::lex::tok::TokenType::OP_PLUS));
    EXPECT_TRUE(parser.isBinaryOp(mylang::lex::tok::TokenType::OP_STAR));
    EXPECT_TRUE(parser.isBinaryOp(mylang::lex::tok::TokenType::OP_LTE));
    EXPECT_FALSE(parser.isBinaryOp(mylang::lex::tok::TokenType::LPAREN));
}

TEST_F(ParserTest, IsAugmentedAssignment)
{
    auto tokens = createTokens({});
    mylang::parser::Parser parser(tokens);

    EXPECT_TRUE(parser.isAugmentedAssign(mylang::lex::tok::TokenType::OP_PLUSEQ));
    EXPECT_TRUE(parser.isAugmentedAssign(mylang::lex::tok::TokenType::OP_MINUSEQ));
    EXPECT_TRUE(parser.isAugmentedAssign(mylang::lex::tok::TokenType::OP_STAREQ));
    EXPECT_FALSE(parser.isAugmentedAssign(mylang::lex::tok::TokenType::OP_EQ));
}
  
TEST_F(ParserTest, OperatorPrecedence)
{
    auto tokens = createTokens({mylang::lex::tok::TokenType::ENDMARKER});
    mylang::parser::Parser parser(tokens);

    auto plusInfo = parser.getOpInfo(mylang::lex::tok::TokenType::OP_PLUS);
    auto starInfo = parser.getOpInfo(mylang::lex::tok::TokenType::STAR);

    EXPECT_GT(starInfo.precedence, plusInfo.precedence);
}
*/

// ============================================================================
// Scope Management
// ============================================================================

TEST_F(ParserTest, ScopeTracksVariablesAcrossBlocks)
{
    auto tokens = createTokens({});
    mylang::parser::Parser parser(tokens);

    parser.declareVariable(u"global");
    EXPECT_TRUE(parser.isVariableDefined(u"global"));

    parser.enterScope();
    parser.declareVariable(u"local");
    EXPECT_TRUE(parser.isVariableDefined(u"local"));

    parser.exitScope();
    EXPECT_TRUE(parser.isVariableDefined(u"global"));
    EXPECT_FALSE(parser.isVariableDefined(u"local"));
}
