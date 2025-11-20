
#include "../../include/lex/lexer.hpp"
#include "../../include/lex/token.hpp"
#include "../../include/parser/parser.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace mylang::parser;
using namespace mylang::lex::tok;
using namespace mylang::lex;
using namespace mylang::parser::ast;

// ============================================================================
// Test Fixtures
// ============================================================================

class ParserTest: public ::testing::Test
{
   protected:
    std::vector<Token> createTokens(std::initializer_list<TokenType> types)
    {
        Lexer lexer;
        std::vector<Token> tokens;
        int line = 1, col = 1;
        for (auto type : types)
        {
            tokens.emplace_back(lexer.make_token(type, u"", line, col));
            col++;
        }
        tokens.emplace_back(lexer.make_token(TokenType::ENDMARKER, u"", line, col));
        return tokens;
    }

    Token makeToken(TokenType type, const std::u16string& value = u"", int line = 1, int col = 1)
    {
        Lexer lexer;
        return lexer.make_token(type, value, line, col);
    }
};

// ============================================================================
// ParseError Tests
// ============================================================================

TEST(ParseErrorTest, BasicConstruction)
{
    ParseError err(u"Test error", 10, 5, u"context line", {u"suggestion1"});

    EXPECT_EQ(err.line, 10);
    EXPECT_EQ(err.column, 5);
    EXPECT_EQ(err.context, u"context line");
    EXPECT_EQ(err.suggestions.size(), 1);
    EXPECT_EQ(err.suggestions[0], u"suggestion1");
}

TEST(ParseErrorTest, FormatMethod)
{
    ParseError err(u"Syntax error", 5, 10);
    std::u16string formatted = err.format();
    EXPECT_FALSE(formatted.empty());
}

TEST(ParseErrorTest, MultipleSuggestions)
{
    std::vector<std::u16string> suggestions = {u"اذا", u"طالما", u"لكل"};
    ParseError err(u"Unknown keyword", 1, 1, u"", suggestions);

    EXPECT_EQ(err.suggestions.size(), 3);
}

// ============================================================================
// Token Navigation Tests
// ============================================================================
/*

TEST_F(ParserTest, PeekWithOffset) {
    auto tokens = createTokens({TokenType::NUMBER, TokenType::OP_PLUS, 
                                TokenType::NUMBER});
    Parser parser(tokens);
    
    // Test peeking without advancing
    auto tok1 = parser.peek(0);
    auto tok2 = parser.peek(1);
    
    EXPECT_EQ(tok1.type, TokenType::NUMBER);
    EXPECT_EQ(tok2.type, TokenType::OP_PLUS);
}

TEST_F(ParserTest, AdvanceToken) {
    auto tokens = createTokens({TokenType::NUMBER, TokenType::OP_PLUS});
    Parser parser(tokens);
    
    auto first = parser.advance();
    auto second = parser.advance();
    
    EXPECT_EQ(first.type, TokenType::NUMBER);
    EXPECT_EQ(second.type, TokenType::OP_PLUS);
}

TEST_F(ParserTest, CheckTokenType) {
    auto tokens = createTokens({TokenType::KW_IF, TokenType::LPAREN});
    Parser parser(tokens);
    
    EXPECT_TRUE(parser.check(TokenType::KW_IF));
    EXPECT_FALSE(parser.check(TokenType::KW_WHILE));
}

TEST_F(ParserTest, CheckAnyTokenTypes) {
    auto tokens = createTokens({TokenType::OP_PLUS});
    Parser parser(tokens);
    
    EXPECT_TRUE(parser.checkAny({TokenType::OP_PLUS, TokenType::MINUS, TokenType::STAR}));
    EXPECT_FALSE(parser.checkAny({TokenType::LPAREN, TokenType::RPAREN}));
}

TEST_F(ParserTest, MatchTokenTypes) {
    auto tokens = createTokens({TokenType::KW_IF, TokenType::LPAREN});
    Parser parser(tokens);
    
    EXPECT_TRUE(parser.match({TokenType::KW_IF, TokenType::KW_WHILE}));
    EXPECT_TRUE(parser.check(TokenType::LPAREN)); // Advanced past KW_IF
}
*/
// ============================================================================
// Primary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseNumberLiteral)
{
    std::vector<Token> tokens = {makeToken(TokenType::NUMBER, u"42"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
    // Add type checking based on your AST implementation
}

TEST_F(ParserTest, ParseStringLiteral)
{
    std::vector<Token> tokens = {makeToken(TokenType::STRING, u"hello"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseBooleanLiterals)
{
    std::vector<Token> tokens1 = {makeToken(TokenType::KW_TRUE), makeToken(TokenType::ENDMARKER)};
    Parser parser1(tokens1);
    auto expr1 = parser1.parsePrimary();
    ASSERT_NE(expr1, nullptr);

    std::vector<Token> tokens2 = {makeToken(TokenType::KW_FALSE), makeToken(TokenType::ENDMARKER)};
    Parser parser2(tokens2);
    auto expr2 = parser2.parsePrimary();
    ASSERT_NE(expr2, nullptr);
}

TEST_F(ParserTest, ParseIdentifier)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"variable"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parsePrimary();
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// Unary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseUnaryMinus)
{
    std::vector<Token> tokens = {
      makeToken(TokenType::OP_MINUS), makeToken(TokenType::NUMBER, u"5"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseUnary();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseUnaryNot)
{
    std::vector<Token> tokens = {
      makeToken(TokenType::KW_NOT), makeToken(TokenType::KW_TRUE), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseUnary();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseNestedUnary)
{
    std::vector<Token> tokens = {makeToken(TokenType::OP_MINUS), makeToken(TokenType::OP_MINUS),
      makeToken(TokenType::NUMBER, u"10"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseUnary();
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// Binary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleAddition)
{
    std::vector<Token> tokens = {makeToken(TokenType::NUMBER, u"5"), makeToken(TokenType::OP_PLUS),
      makeToken(TokenType::NUMBER, u"3"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseExpression();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseMultiplication)
{
    std::vector<Token> tokens = {makeToken(TokenType::NUMBER, u"4"), makeToken(TokenType::OP_STAR),
      makeToken(TokenType::NUMBER, u"2"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseExpression();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseOperatorPrecedence)
{
    // 2 + 3 * 4 should parse as 2 + (3 * 4)
    std::vector<Token> tokens = {makeToken(TokenType::NUMBER, u"2"), makeToken(TokenType::OP_PLUS),
      makeToken(TokenType::NUMBER, u"3"), makeToken(TokenType::OP_STAR), makeToken(TokenType::NUMBER, u"4"),
      makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseExpression();
    ASSERT_NE(expr, nullptr);
    // Verify AST structure matches expected precedence
}

/*
TEST_F(ParserTest, ParsePowerOperator) {
    std::vector<Token> tokens = {
        makeToken(TokenType::NUMBER, u"2"),
        makeToken(TokenType::POWER),
        makeToken(TokenType::NUMBER, u"3"),
        makeToken(TokenType::ENDMARKER)
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
    std::vector<Token> tokens = {makeToken(TokenType::NUMBER, u"5"), makeToken(TokenType::OP_LT),
      makeToken(TokenType::NUMBER, u"10"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseComparison();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseChainedComparison)
{
    // a < b < c
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"a"), makeToken(TokenType::OP_LT),
      makeToken(TokenType::IDENTIFIER, u"b"), makeToken(TokenType::OP_LT), makeToken(TokenType::IDENTIFIER, u"c"),
      makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseComparison();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseEqualityComparison)
{
    // 42 = x
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"x"), makeToken(TokenType::OP_EQ),
      makeToken(TokenType::NUMBER, u"42"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseComparison();
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// Ternary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseTernaryExpression)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"x"), makeToken(TokenType::KW_IF),
      makeToken(TokenType::IDENTIFIER, u"condition"), makeToken(TokenType::IDENTIFIER, u"y"),
      makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseTernary();
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// Parenthesized Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseParenthesizedExpr)
{
    std::vector<Token> tokens = {makeToken(TokenType::LPAREN), makeToken(TokenType::NUMBER, u"5"),
      makeToken(TokenType::OP_PLUS), makeToken(TokenType::NUMBER, u"3"), makeToken(TokenType::RPAREN),
      makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseParenthesizedExpr();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseEmptyTuple)
{
    std::vector<Token> tokens = {
      makeToken(TokenType::LPAREN), makeToken(TokenType::RPAREN), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseParenthesizedExpr();
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// List Literal Tests
// ============================================================================

TEST_F(ParserTest, ParseEmptyList)
{
    std::vector<Token> tokens = {
      makeToken(TokenType::LBRACKET), makeToken(TokenType::RBRACKET), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseListLiteral();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseListWithElements)
{
    std::vector<Token> tokens = {makeToken(TokenType::LBRACKET), makeToken(TokenType::NUMBER, u"1"),
      makeToken(TokenType::COMMA), makeToken(TokenType::NUMBER, u"2"), makeToken(TokenType::COMMA),
      makeToken(TokenType::NUMBER, u"3"), makeToken(TokenType::RBRACKET), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseListLiteral();
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// Dictionary Literal Tests
// ============================================================================

TEST_F(ParserTest, ParseEmptyDict)
{
    std::vector<Token> tokens = {
      makeToken(TokenType::LBRACE), makeToken(TokenType::RBRACE), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseDictLiteral();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseDictWithKeyValue)
{
    std::vector<Token> tokens = {makeToken(TokenType::LBRACE), makeToken(TokenType::STRING, u"key"),
      makeToken(TokenType::COLON), makeToken(TokenType::NUMBER, u"42"), makeToken(TokenType::RBRACE),
      makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseDictLiteral();
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// Lambda Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleLambda)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"x"), makeToken(TokenType::COLON),
      makeToken(TokenType::IDENTIFIER, u"x"), makeToken(TokenType::OP_STAR), makeToken(TokenType::NUMBER, u"2"),
      makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseLambda();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseLambdaWithMultipleParams)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"x"), makeToken(TokenType::COMMA),
      makeToken(TokenType::IDENTIFIER, u"y"), makeToken(TokenType::COLON), makeToken(TokenType::IDENTIFIER, u"x"),
      makeToken(TokenType::OP_PLUS), makeToken(TokenType::IDENTIFIER, u"y"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseLambda();
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// Postfix Operations Tests
// ============================================================================

TEST_F(ParserTest, ParseFunctionCall)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"func"), makeToken(TokenType::LPAREN),
      makeToken(TokenType::RPAREN), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parsePrimary();
    expr = parser.parsePostfixOps(std::move(expr));
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseFunctionCallWithArgs)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"func"), makeToken(TokenType::LPAREN),
      makeToken(TokenType::NUMBER, u"1"), makeToken(TokenType::COMMA), makeToken(TokenType::NUMBER, u"2"),
      makeToken(TokenType::RPAREN), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parsePrimary();
    expr = parser.parsePostfixOps(std::move(expr));
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseSubscript)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"list"), makeToken(TokenType::LBRACKET),
      makeToken(TokenType::NUMBER, u"0"), makeToken(TokenType::RBRACKET), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parsePrimary();
    expr = parser.parsePostfixOps(std::move(expr));
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseAttributeAccess)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"obj"), makeToken(TokenType::DOT),
      makeToken(TokenType::IDENTIFIER, u"property"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parsePrimary();
    expr = parser.parsePostfixOps(std::move(expr));
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// Assignment Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleAssignment)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"x"), makeToken(TokenType::OP_EQ),
      makeToken(TokenType::NUMBER, u"42"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseAssignmentExpr();
    ASSERT_NE(expr, nullptr);
}

TEST_F(ParserTest, ParseAugmentedAssignment)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"x"), makeToken(TokenType::OP_PLUSEQ),
      makeToken(TokenType::NUMBER, u"5"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto expr = parser.parseAssignmentExpr();
    ASSERT_NE(expr, nullptr);
}

// ============================================================================
// Statement Parsing Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleStatement)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"x"), makeToken(TokenType::OP_EQ),
      makeToken(TokenType::NUMBER, u"10"), makeToken(TokenType::NEWLINE), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto stmt = parser.parseSimpleStmt();
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserTest, ParseIfStatement)
{
    std::vector<Token> tokens = {makeToken(TokenType::KW_IF), makeToken(TokenType::KW_TRUE),
      makeToken(TokenType::COLON), makeToken(TokenType::NEWLINE), makeToken(TokenType::INDENT),
      makeToken(TokenType::NEWLINE), makeToken(TokenType::DEDENT), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto stmt = parser.parseCompoundStmt();
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserTest, ParseWhileLoop)
{
    std::vector<Token> tokens = {makeToken(TokenType::KW_WHILE), makeToken(TokenType::KW_TRUE),
      makeToken(TokenType::COLON), makeToken(TokenType::NEWLINE), makeToken(TokenType::INDENT),
      makeToken(TokenType::NEWLINE), makeToken(TokenType::DEDENT), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto stmt = parser.parseCompoundStmt();
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserTest, ParseForLoop)
{
    std::vector<Token> tokens = {makeToken(TokenType::KW_FOR), makeToken(TokenType::IDENTIFIER, u"i"),
      makeToken(TokenType::KW_IN), makeToken(TokenType::IDENTIFIER, u"range"), makeToken(TokenType::LPAREN),
      makeToken(TokenType::NUMBER, u"10"), makeToken(TokenType::RPAREN), makeToken(TokenType::COLON),
      makeToken(TokenType::NEWLINE), makeToken(TokenType::INDENT), makeToken(TokenType::NEWLINE),
      makeToken(TokenType::DEDENT), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto stmt = parser.parseCompoundStmt();
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserTest, ParseFunctionDefinition)
{
    std::vector<Token> tokens = {makeToken(TokenType::KW_FN), makeToken(TokenType::IDENTIFIER, u"test"),
      makeToken(TokenType::LPAREN), makeToken(TokenType::RPAREN), makeToken(TokenType::COLON),
      makeToken(TokenType::NEWLINE), makeToken(TokenType::INDENT), makeToken(TokenType::KW_RETURN),
      makeToken(TokenType::NUMBER, u"42"), makeToken(TokenType::NEWLINE), makeToken(TokenType::DEDENT),
      makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto stmt = parser.parseCompoundStmt();
    ASSERT_NE(stmt, nullptr);
}

// ============================================================================
// Block Parsing Tests
// ============================================================================

TEST_F(ParserTest, ParseEmptyBlock)
{
    std::vector<Token> tokens = {makeToken(TokenType::INDENT), makeToken(TokenType::NEWLINE),
      makeToken(TokenType::DEDENT), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto stmts = parser.parseBlock();
    EXPECT_FALSE(stmts.empty());
}

TEST_F(ParserTest, ParseMultiStatementBlock)
{
    std::vector<Token> tokens = {makeToken(TokenType::INDENT), makeToken(TokenType::IDENTIFIER, u"x"),
      makeToken(TokenType::OP_EQ), makeToken(TokenType::NUMBER, u"1"), makeToken(TokenType::NEWLINE),
      makeToken(TokenType::IDENTIFIER, u"y"), makeToken(TokenType::OP_EQ), makeToken(TokenType::NUMBER, u"2"),
      makeToken(TokenType::NEWLINE), makeToken(TokenType::DEDENT), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    auto stmts = parser.parseBlock();
    EXPECT_GE(stmts.size(), 2);
}

// ============================================================================
// Scope Management Tests
// ============================================================================
/*
TEST_F(ParserTest, ScopeEnterExit) {
    auto tokens = createTokens({TokenType::ENDMARKER});
    Parser parser(tokens);
    
    parser.enterScope();
    parser.declareVariable(u"x");
    EXPECT_TRUE(parser.isVariableDefined(u"x"));
    
    parser.exitScope();
    EXPECT_FALSE(parser.isVariableDefined(u"x"));
}

TEST_F(ParserTest, NestedScopes) {
    auto tokens = createTokens({TokenType::ENDMARKER});
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
    auto tokens = createTokens({TokenType::ENDMARKER});
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
    std::vector<Token> tokens = {makeToken(TokenType::OP_PLUS), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    EXPECT_THROW({ parser.parseExpression(); }, std::exception);
}

TEST_F(ParserTest, MissingClosingParen)
{
    std::vector<Token> tokens = {
      makeToken(TokenType::LPAREN), makeToken(TokenType::NUMBER, u"5"), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    EXPECT_THROW({ parser.parseParenthesizedExpr(); }, std::exception);
}

TEST_F(ParserTest, ErrorRecoveryWithSynchronization)
{
    std::vector<Token> tokens = {makeToken(TokenType::IDENTIFIER, u"x"),
      makeToken(TokenType::OP_PLUS),  // Missing right operand
      makeToken(TokenType::NEWLINE),
      makeToken(TokenType::KW_IF),  // Sync token
      makeToken(TokenType::KW_TRUE), makeToken(TokenType::COLON), makeToken(TokenType::ENDMARKER)};
    Parser parser(tokens);

    // Should recover and continue parsing
    auto stmts = parser.parse();
    // Should have captured some statements despite error
}

// ============================================================================
// Operator Tests
// ============================================================================
/*
TEST_F(ParserTest, IsUnaryOperator) {
    auto tokens = createTokens({TokenType::ENDMARKER});
    Parser parser(tokens);
    
    EXPECT_TRUE(parser.isUnaryOp(TokenType::OP_MINUS));
    EXPECT_TRUE(parser.isUnaryOp(TokenType::OP_PLUS));
    EXPECT_TRUE(parser.isUnaryOp(TokenType::KW_NOT));
    EXPECT_FALSE(parser.isUnaryOp(TokenType::OP_STAR));
}

TEST_F(ParserTest, IsBinaryOperator) {
    auto tokens = createTokens({TokenType::ENDMARKER});
    Parser parser(tokens);
    
    EXPECT_TRUE(parser.isBinaryOp(TokenType::OP_PLUS));
    EXPECT_TRUE(parser.isBinaryOp(TokenType::STAR));
    EXPECT_TRUE(parser.isBinaryOp(TokenType::LESS));
    EXPECT_FALSE(parser.isBinaryOp(TokenType::LPAREN));
}

TEST_F(ParserTest, IsAugmentedAssignment) {
    auto tokens = createTokens({TokenType::ENDMARKER});
    Parser parser(tokens);
    
    EXPECT_TRUE(parser.isAugmentedAssign(TokenType::PLUS_EQUAL));
    EXPECT_TRUE(parser.isAugmentedAssign(TokenType::MINUS_EQUAL));
    EXPECT_TRUE(parser.isAugmentedAssign(TokenType::STAR_EQUAL));
    EXPECT_FALSE(parser.isAugmentedAssign(TokenType::EQUAL));
}

TEST_F(ParserTest, OperatorPrecedence) {
    auto tokens = createTokens({TokenType::ENDMARKER});
    Parser parser(tokens);
    
    auto plusInfo = parser.getOpInfo(TokenType::OP_PLUS);
    auto starInfo = parser.getOpInfo(TokenType::STAR);
    
    EXPECT_GT(starInfo.precedence, plusInfo.precedence);
}

// ============================================================================
// Full Program Tests
// ============================================================================

TEST_F(ParserTest, ParseCompleteProgram) {
    std::vector<Token> tokens = {
        makeToken(TokenType::IDENTIFIER, u"x"),
        makeToken(TokenType::OP_EQ),
        makeToken(TokenType::NUMBER, u"10"),
        makeToken(TokenType::NEWLINE),
        makeToken(TokenType::IDENTIFIER, u"y"),
        makeToken(TokenType::OP_EQ),
        makeToken(TokenType::NUMBER, u"20"),
        makeToken(TokenType::NEWLINE),
        makeToken(TokenType::ENDMARKER)
    };
    Parser parser(tokens);
    
    auto stmts = parser.parse();
    EXPECT_GE(stmts.size(), 2);
}
*/