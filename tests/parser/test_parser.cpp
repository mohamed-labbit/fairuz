
#include "../../include/lex/lexer.hpp"
#include "../../include/lex/token.hpp"
#include "../../include/parser/parser.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <vector>


// ============================================================================
// Test Fixtures
// ============================================================================


class ParserTest: public ::testing::Test
{
   protected:
    std::vector<mylang::lex::tok::Token> createTokens(std::initializer_list<mylang::lex::tok::TokenType> types)
    {
        mylang::lex::Lexer lexer;
        std::vector<mylang::lex::tok::Token> tokens;
        int line = 1, col = 1;
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
    EXPECT_EQ(err.suggestions.size(), 1);
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
// Token Navigation Tests
// ============================================================================
/*

TEST_F(ParserTest, PeekWithOffset) {
    auto tokens = createTokens({mylang::lex::tok::TokenType::NUMBER, mylang::lex::tok::TokenType::OP_PLUS, 
                                mylang::lex::tok::TokenType::NUMBER});
    Parser parser(tokens);
    
    // Test peeking without advancing
    auto tok1 = parser.peek(0);
    auto tok2 = parser.peek(1);
    
    EXPECT_EQ(tok1.type, mylang::lex::tok::TokenType::NUMBER);
    EXPECT_EQ(tok2.type, mylang::lex::tok::TokenType::OP_PLUS);
}

TEST_F(ParserTest, AdvanceToken) {
    auto tokens = createTokens({mylang::lex::tok::TokenType::NUMBER, mylang::lex::tok::TokenType::OP_PLUS});
    Parser parser(tokens);
    
    auto first = parser.advance();
    auto second = parser.advance();
    
    EXPECT_EQ(first.type, mylang::lex::tok::TokenType::NUMBER);
    EXPECT_EQ(second.type, mylang::lex::tok::TokenType::OP_PLUS);
}

TEST_F(ParserTest, CheckTokenType) {
    auto tokens = createTokens({mylang::lex::tok::TokenType::KW_IF, mylang::lex::tok::TokenType::LPAREN});
    Parser parser(tokens);
    
    EXPECT_TRUE(parser.check(mylang::lex::tok::TokenType::KW_IF));
    EXPECT_FALSE(parser.check(mylang::lex::tok::TokenType::KW_WHILE));
}

TEST_F(ParserTest, CheckAnyTokenTypes) {
    auto tokens = createTokens({mylang::lex::tok::TokenType::OP_PLUS});
    Parser parser(tokens);
    
    EXPECT_TRUE(parser.checkAny({mylang::lex::tok::TokenType::OP_PLUS, mylang::lex::tok::TokenType::MINUS, mylang::lex::tok::TokenType::STAR}));
    EXPECT_FALSE(parser.checkAny({mylang::lex::tok::TokenType::LPAREN, mylang::lex::tok::TokenType::RPAREN}));
}

TEST_F(ParserTest, MatchTokenTypes) {
    auto tokens = createTokens({mylang::lex::tok::TokenType::KW_IF, mylang::lex::tok::TokenType::LPAREN});
    Parser parser(tokens);
    
    EXPECT_TRUE(parser.match({mylang::lex::tok::TokenType::KW_IF, mylang::lex::tok::TokenType::KW_WHILE}));
    EXPECT_TRUE(parser.check(mylang::lex::tok::TokenType::LPAREN)); // Advanced past KW_IF
}
*/
// ============================================================================
// Primary Expression Tests
// ============================================================================

const std::string test_cases_dir = "/Users/mohamedrabbit/code/mylang/tests/parser/test_cases/";


TEST_F(ParserTest, ParseNumberLiteral)
{
    std::ifstream file(test_cases_dir + "number_literal.txt", std::ios::binary);
    mylang::parser::Parser parser(&file);
    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
    file.close();
    // Add type checking based on your AST implementation
}

TEST_F(ParserTest, ParseStringLiteral)
{
    std::ifstream file(test_cases_dir + "string_literal.txt", std::ios::binary);
    mylang::parser::Parser parser(&file);
    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseBooleanLiterals)
{
    std::ifstream file_true(test_cases_dir + "boolean_literal_true", std::ios::binary);
    mylang::parser::Parser parser1(&file_true);
    auto expr1 = parser1.parsePrimary();
    ASSERT_NE(expr1, nullptr);
    file_true.close();
    std::ifstream file_false(test_cases_dir + "boolean_literal_false", std::ios::binary);
    mylang::parser::Parser parser2(&file_false);
    auto expr2 = parser2.parsePrimary();
    ASSERT_NE(expr2, nullptr);
    file_false.close();
}

TEST_F(ParserTest, ParseIdentifier)
{
    std::ifstream file(test_cases_dir + "identifier.txt", std::ios::binary);
    mylang::parser::Parser parser(&file);
    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// Unary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseUnaryMinus)
{
    std::ifstream file(test_cases_dir + "unary_minus.txt", std::ios::binary);
    mylang::parser::Parser parser(&file);
    auto expr = parser.parseUnary();
    ASSERT_NE(expr, nullptr);
    file.close();
}

//
// write the files starting from here
//

TEST_F(ParserTest, ParseUnaryNot)
{
    std::ifstream file(test_cases_dir + "unary_not.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::KW_NOT),
      makeToken(mylang::lex::tok::TokenType::KW_TRUE), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseUnary();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseNestedUnary)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::OP_MINUS),
      makeToken(mylang::lex::tok::TokenType::OP_MINUS), makeToken(mylang::lex::tok::TokenType::NUMBER, u"10"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseUnary();
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// Binary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleAddition)
{
    std::ifstream file(test_cases_dir + "simple_addition.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::NUMBER, u"5"),
      makeToken(mylang::lex::tok::TokenType::OP_PLUS), makeToken(mylang::lex::tok::TokenType::NUMBER, u"3"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseExpression();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseMultiplication)
{
    std::ifstream file(test_cases_dir + "multiplication.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::NUMBER, u"4"),
      makeToken(mylang::lex::tok::TokenType::OP_STAR), makeToken(mylang::lex::tok::TokenType::NUMBER, u"2"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseExpression();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseOperatorPrecedence)
{
    std::ifstream file(test_cases_dir + "operator_precedence.txt", std::ios::binary);
    // 2 + 3 * 4 should parse as 2 + (3 * 4)
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::NUMBER, u"2"),
      makeToken(mylang::lex::tok::TokenType::OP_PLUS), makeToken(mylang::lex::tok::TokenType::NUMBER, u"3"),
      makeToken(mylang::lex::tok::TokenType::OP_STAR), makeToken(mylang::lex::tok::TokenType::NUMBER, u"4"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseExpression();
    ASSERT_NE(expr, nullptr);
    // Verify AST structure matches expected precedence
    file.close();
}

/*
TEST_F(ParserTest, ParsePowerOperator) {
    std::vector<mylang::lex::tok::Token> tokens = {
        makeToken(mylang::lex::tok::TokenType::NUMBER, u"2"),
        makeToken(mylang::lex::tok::TokenType::POWER),
        makeToken(mylang::lex::tok::TokenType::NUMBER, u"3"),
        makeToken(mylang::lex::tok::TokenType::ENDMARKER)
    };
    Parser parser(tokens);
    
    auto expr = parser.parsePower();
    ASSERT_NE(expr, nullptr);
}
*/

// ============================================================================
// Comparison Chaining Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleComparison)
{
    std::ifstream file(test_cases_dir + "simple_comparison.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::NUMBER, u"5"),
      makeToken(mylang::lex::tok::TokenType::OP_LT), makeToken(mylang::lex::tok::TokenType::NUMBER, u"10"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseComparison();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseChainedComparison)
{
    // a < b < c
    std::ifstream file(test_cases_dir + "chained_comparison.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"a"),
      makeToken(mylang::lex::tok::TokenType::OP_LT), makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"b"),
      makeToken(mylang::lex::tok::TokenType::OP_LT), makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"c"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseComparison();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseEqualityComparison)
{
    // 42 = x
    std::ifstream file(test_cases_dir + "equality_comparison.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::OP_EQ), makeToken(mylang::lex::tok::TokenType::NUMBER, u"42"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseComparison();
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// Ternary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseTernaryExpression)
{
    std::ifstream file(test_cases_dir + "ternary_expression.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::KW_IF), makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"condition"),
      makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"y"), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseTernary();
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// Parenthesized Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseParenthesizedExpr)
{
    std::ifstream file(test_cases_dir + "parenthesized_expr.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::LPAREN),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"5"), makeToken(mylang::lex::tok::TokenType::OP_PLUS),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"3"), makeToken(mylang::lex::tok::TokenType::RPAREN),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseParenthesizedExpr();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseEmptyTuple)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::LPAREN),
      makeToken(mylang::lex::tok::TokenType::RPAREN), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseParenthesizedExpr();
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// List Literal Tests
// ============================================================================

TEST_F(ParserTest, ParseEmptyList)
{
    std::ifstream file(test_cases_dir + "empty_list.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::LBRACKET),
      makeToken(mylang::lex::tok::TokenType::RBRACKET), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseListLiteral();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseListWithElements)
{
    std::ifstream file(test_cases_dir + "list_with_elements.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::LBRACKET),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"1"), makeToken(mylang::lex::tok::TokenType::COMMA),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"2"), makeToken(mylang::lex::tok::TokenType::COMMA),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"3"), makeToken(mylang::lex::tok::TokenType::RBRACKET),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseListLiteral();
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// Dictionary Literal Tests
// ============================================================================

TEST_F(ParserTest, ParseEmptyDict)
{
    std::ifstream file(test_cases_dir + "empty_dict.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::LBRACE),
      makeToken(mylang::lex::tok::TokenType::RBRACE), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseDictLiteral();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseDictWithKeyValue)
{
    std::ifstream file(test_cases_dir + "dict_with_key_value.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::LBRACE),
      makeToken(mylang::lex::tok::TokenType::STRING, u"key"), makeToken(mylang::lex::tok::TokenType::COLON),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"42"), makeToken(mylang::lex::tok::TokenType::RBRACE),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseDictLiteral();
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// Lambda Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleLambda)
{
    std::ifstream file(test_cases_dir + "simple_lambda.txt", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::COLON), makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::OP_STAR), makeToken(mylang::lex::tok::TokenType::NUMBER, u"2"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseLambda();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseLambdaWithMultipleParams)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::COMMA), makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"y"),
      makeToken(mylang::lex::tok::TokenType::COLON), makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::OP_PLUS), makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"y"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseLambda();
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// Postfix Operations Tests
// ============================================================================

TEST_F(ParserTest, ParseFunctionCall)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"func"),
      makeToken(mylang::lex::tok::TokenType::LPAREN), makeToken(mylang::lex::tok::TokenType::RPAREN),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parsePrimary();
    expr = parser.parsePostfixOps(std::move(expr));
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseFunctionCallWithArgs)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"func"),
      makeToken(mylang::lex::tok::TokenType::LPAREN), makeToken(mylang::lex::tok::TokenType::NUMBER, u"1"),
      makeToken(mylang::lex::tok::TokenType::COMMA), makeToken(mylang::lex::tok::TokenType::NUMBER, u"2"),
      makeToken(mylang::lex::tok::TokenType::RPAREN), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parsePrimary();
    expr = parser.parsePostfixOps(std::move(expr));
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseSubscript)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"list"),
      makeToken(mylang::lex::tok::TokenType::LBRACKET), makeToken(mylang::lex::tok::TokenType::NUMBER, u"0"),
      makeToken(mylang::lex::tok::TokenType::RBRACKET), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parsePrimary();
    expr = parser.parsePostfixOps(std::move(expr));
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseAttributeAccess)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"obj"),
      makeToken(mylang::lex::tok::TokenType::DOT), makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"property"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parsePrimary();
    expr = parser.parsePostfixOps(std::move(expr));
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// Assignment Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleAssignment)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::OP_EQ), makeToken(mylang::lex::tok::TokenType::NUMBER, u"42"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseAssignmentExpr();
    ASSERT_NE(expr, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseAugmentedAssignment)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::OP_PLUSEQ), makeToken(mylang::lex::tok::TokenType::NUMBER, u"5"),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto expr = parser.parseAssignmentExpr();
    ASSERT_NE(expr, nullptr);
    file.close();
}

// ============================================================================
// Statement Parsing Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleStatement)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::OP_EQ), makeToken(mylang::lex::tok::TokenType::NUMBER, u"10"),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto stmt = parser.parseSimpleStmt();
    ASSERT_NE(stmt, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseIfStatement)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::KW_IF),
      makeToken(mylang::lex::tok::TokenType::KW_TRUE), makeToken(mylang::lex::tok::TokenType::COLON),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::INDENT),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::DEDENT),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto stmt = parser.parseCompoundStmt();
    ASSERT_NE(stmt, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseWhileLoop)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::KW_WHILE),
      makeToken(mylang::lex::tok::TokenType::KW_TRUE), makeToken(mylang::lex::tok::TokenType::COLON),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::INDENT),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::DEDENT),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto stmt = parser.parseCompoundStmt();
    ASSERT_NE(stmt, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseForLoop)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::KW_FOR),
      makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"i"), makeToken(mylang::lex::tok::TokenType::KW_IN),
      makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"range"), makeToken(mylang::lex::tok::TokenType::LPAREN),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"10"), makeToken(mylang::lex::tok::TokenType::RPAREN),
      makeToken(mylang::lex::tok::TokenType::COLON), makeToken(mylang::lex::tok::TokenType::NEWLINE),
      makeToken(mylang::lex::tok::TokenType::INDENT), makeToken(mylang::lex::tok::TokenType::NEWLINE),
      makeToken(mylang::lex::tok::TokenType::DEDENT), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto stmt = parser.parseCompoundStmt();
    ASSERT_NE(stmt, nullptr);
    file.close();
}

TEST_F(ParserTest, ParseFunctionDefinition)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::KW_FN),
      makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"test"), makeToken(mylang::lex::tok::TokenType::LPAREN),
      makeToken(mylang::lex::tok::TokenType::RPAREN), makeToken(mylang::lex::tok::TokenType::COLON),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::INDENT),
      makeToken(mylang::lex::tok::TokenType::KW_RETURN), makeToken(mylang::lex::tok::TokenType::NUMBER, u"42"),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::DEDENT),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto stmt = parser.parseCompoundStmt();
    ASSERT_NE(stmt, nullptr);
    file.close();
}

// ============================================================================
// Block Parsing Tests
// ============================================================================

TEST_F(ParserTest, ParseEmptyBlock)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::INDENT),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::DEDENT),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto stmts = parser.parseBlock();
    EXPECT_FALSE(stmts.empty());
    file.close();
}

TEST_F(ParserTest, ParseMultiStatementBlock)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::INDENT),
      makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"), makeToken(mylang::lex::tok::TokenType::OP_EQ),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"1"), makeToken(mylang::lex::tok::TokenType::NEWLINE),
      makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"y"), makeToken(mylang::lex::tok::TokenType::OP_EQ),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"2"), makeToken(mylang::lex::tok::TokenType::NEWLINE),
      makeToken(mylang::lex::tok::TokenType::DEDENT), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    auto stmts = parser.parseBlock();
    EXPECT_GE(stmts.size(), 2);
    file.close();
}

// ============================================================================
// Scope Management Tests
// ============================================================================
/*
TEST_F(ParserTest, ScopeEnterExit) {
    auto tokens = createTokens({mylang::lex::tok::TokenType::ENDMARKER});
    Parser parser(tokens);
    
    parser.enterScope();
    parser.declareVariable(u"x");
    EXPECT_TRUE(parser.isVariableDefined(u"x"));
    
    parser.exitScope();
    EXPECT_FALSE(parser.isVariableDefined(u"x"));
}

TEST_F(ParserTest, NestedScopes) {
    auto tokens = createTokens({mylang::lex::tok::TokenType::ENDMARKER});
    Parser parser(tokens);
    
    parser.declareVariable(u"global");
    parser.enterScope();
    parser.declareVariable(u"local");
    
    EXPECT_TRUE(parser.isVariableDefined(u"global"));
    EXPECT_TRUE(parser.isVariableDefined(u"local"));
    
    parser.exitScope();
    EXPECT_TRUE(parser.isVariableDefined(u"global"));
    EXPECT_FALSE(parser.isVariableDefined(u"local"));
}

TEST_F(ParserTest, FunctionDeclaration) {
    auto tokens = createTokens({mylang::lex::tok::TokenType::ENDMARKER});
    Parser parser(tokens);
    
    parser.declareFunction(u"myFunc");
    EXPECT_TRUE(parser.isVariableDefined(u"myFunc"));
}
*/
// ============================================================================
// Error Handling Tests
// ============================================================================


TEST_F(ParserTest, UnexpectedTokenError)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {
      makeToken(mylang::lex::tok::TokenType::OP_PLUS), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    EXPECT_THROW({ parser.parseExpression(); }, std::exception);
    file.close();
}

TEST_F(ParserTest, MissingClosingParen)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::LPAREN),
      makeToken(mylang::lex::tok::TokenType::NUMBER, u"5"), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    EXPECT_THROW({ parser.parseParenthesizedExpr(); }, std::exception);
    file.close();
}

TEST_F(ParserTest, ErrorRecoveryWithSynchronization)
{
    std::ifstream file(test_cases_dir + "", std::ios::binary);
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::OP_PLUS),  // Missing right operand
      makeToken(mylang::lex::tok::TokenType::NEWLINE),
      makeToken(mylang::lex::tok::TokenType::KW_IF),  // Sync token
      makeToken(mylang::lex::tok::TokenType::KW_TRUE), makeToken(mylang::lex::tok::TokenType::COLON),
      makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);
    // Should recover and continue parsing
    auto stmts = parser.parse();
    // Should have captured some statements despite error
    file.close();
}

// ============================================================================
// Operator Tests
// ============================================================================
/*
TEST_F(ParserTest, IsUnaryOperator)
{
    auto tokens = createTokens({mylang::lex::tok::TokenType::ENDMARKER});
    mylang::parser::Parser parser(tokens);

    EXPECT_TRUE(parser.isUnaryOp(mylang::lex::tok::TokenType::OP_MINUS));
    EXPECT_TRUE(parser.isUnaryOp(mylang::lex::tok::TokenType::OP_PLUS));
    EXPECT_TRUE(parser.isUnaryOp(mylang::lex::tok::TokenType::KW_NOT));
    EXPECT_FALSE(parser.isUnaryOp(mylang::lex::tok::TokenType::OP_STAR));
}

TEST_F(ParserTest, IsBinaryOperator)
{
    auto tokens = createTokens({mylang::lex::tok::TokenType::ENDMARKER});
    mylang::parser::Parser parser(tokens);

    EXPECT_TRUE(parser.isBinaryOp(mylang::lex::tok::TokenType::OP_PLUS));
    EXPECT_TRUE(parser.isBinaryOp(mylang::lex::tok::TokenType::STAR));
    EXPECT_TRUE(parser.isBinaryOp(mylang::lex::tok::TokenType::LESS));
    EXPECT_FALSE(parser.isBinaryOp(mylang::lex::tok::TokenType::LPAREN));
}

TEST_F(ParserTest, IsAugmentedAssignment)
{
    auto tokens = createTokens({mylang::lex::tok::TokenType::ENDMARKER});
    mylang::parser::Parser parser(tokens);

    EXPECT_TRUE(parser.isAugmentedAssign(mylang::lex::tok::TokenType::PLUS_EQUAL));
    EXPECT_TRUE(parser.isAugmentedAssign(mylang::lex::tok::TokenType::MINUS_EQUAL));
    EXPECT_TRUE(parser.isAugmentedAssign(mylang::lex::tok::TokenType::STAR_EQUAL));
    EXPECT_FALSE(parser.isAugmentedAssign(mylang::lex::tok::TokenType::EQUAL));
}
  
TEST_F(ParserTest, OperatorPrecedence)
{
  auto tokens = createTokens({mylang::lex::tok::TokenType::ENDMARKER});
  mylang::parser::Parser parser(tokens);
  
  auto plusInfo = parser.getOpInfo(mylang::lex::tok::TokenType::OP_PLUS);
  auto starInfo = parser.getOpInfo(mylang::lex::tok::TokenType::OP_STAR);
  
  EXPECT_GT(starInfo.precedence, plusInfo.precedence);
}
*/

// ============================================================================
// Full Program Tests
// ============================================================================

TEST_F(ParserTest, ParseCompleteProgram)
{
    std::vector<mylang::lex::tok::Token> tokens = {makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"x"),
      makeToken(mylang::lex::tok::TokenType::OP_EQ), makeToken(mylang::lex::tok::TokenType::NUMBER, u"10"),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::IDENTIFIER, u"y"),
      makeToken(mylang::lex::tok::TokenType::OP_EQ), makeToken(mylang::lex::tok::TokenType::NUMBER, u"20"),
      makeToken(mylang::lex::tok::TokenType::NEWLINE), makeToken(mylang::lex::tok::TokenType::ENDMARKER)};
    mylang::parser::Parser parser(tokens);

    auto stmts = parser.parse();
    EXPECT_GE(stmts.size(), 2);
}