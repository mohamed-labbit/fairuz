#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
#include <boost/stacktrace.hpp>

#include "../include/input/file_manager.hpp"
#include "../include/lex/lexer.hpp"
#include "../include/parser/ast/ast_.hpp"
#include "../include/parser/parser_.hpp"


namespace {
std::filesystem::path parser_test_cases_dir()
{
    static const auto dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
    return dir;
}

std::u16string load_source(const std::string& filename)
{
    auto filepath = parser_test_cases_dir() / filename;
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        ADD_FAILURE() << "Failed to open test file: " << filepath;
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
        ASSERT_TRUE(std::filesystem::exists(parser_test_cases_dir()))
          << "Test cases directory not found: " << parser_test_cases_dir();
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

    mylang::lex::tok::Token makeToken(
      mylang::lex::tok::TokenType type, const std::u16string& value = u"", int line = 1, int col = 1)
    {
        mylang::lex::Lexer lexer;
        return lexer.make_token(type, value, line, col);
    }

    std::ifstream openTestFile(const std::string& filename)
    {
        auto filepath = parser_test_cases_dir() / filename;
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open())
            ADD_FAILURE() << "Failed to open test file: " << filepath;
        return file;
    }

    template<typename T>
    T* parseAndCast(mylang::parser::Parser& parser, mylang::parser::ast::ExprPtr& expr)
    {
        EXPECT_NE(expr, nullptr) << "Expression should not be null";
        if (!expr)
            return nullptr;
        T* casted = static_cast<T*>(expr.get());
        EXPECT_NE(casted, nullptr) << "Failed to cast to expected type";
        return casted;
    }
};

// ParseError Tests
class ParseErrorTest: public ::testing::Test
{
};

TEST_F(ParseErrorTest, BasicConstruction)
{
    mylang::parser::ParseError err(u"Test error", 10, 5, u"context line", {u"suggestion1"});

    EXPECT_EQ(err.line, 10);
    EXPECT_EQ(err.column, 5);
    EXPECT_EQ(err.context, u"context line");
    ASSERT_EQ(err.suggestions.size(), 1);
    EXPECT_EQ(err.suggestions[0], u"suggestion1");
}

TEST_F(ParseErrorTest, ConstructionWithoutContext)
{
    mylang::parser::ParseError err(u"Simple error", 1, 1);

    EXPECT_EQ(err.line, 1);
    EXPECT_EQ(err.column, 1);
    EXPECT_TRUE(err.context.empty());
    EXPECT_TRUE(err.suggestions.empty());
}

TEST_F(ParseErrorTest, FormatMethodNotEmpty)
{
    mylang::parser::ParseError err(u"Syntax error", 5, 10);
    std::u16string formatted = err.format();

    EXPECT_FALSE(formatted.empty());
    // verify format contains line/column info
}

TEST_F(ParseErrorTest, FormatWithContext)
{
    mylang::parser::ParseError err(u"Unexpected token", 3, 7, u"x = 5 + ");
    std::u16string formatted = err.format();

    EXPECT_FALSE(formatted.empty());
    // verify context appears in formatted message
}

TEST_F(ParseErrorTest, MultipleSuggestions)
{
    std::vector<std::u16string> suggestions = {u"اذا", u"طالما", u"لكل"};
    mylang::parser::ParseError err(u"Unknown keyword", 1, 1, u"", suggestions);

    ASSERT_EQ(err.suggestions.size(), 3);
    EXPECT_EQ(err.suggestions[0], u"اذا");
    EXPECT_EQ(err.suggestions[1], u"طالما");
    EXPECT_EQ(err.suggestions[2], u"لكل");
}

TEST_F(ParseErrorTest, EmptySuggestions)
{
    mylang::parser::ParseError err(u"Generic error", 1, 1);
    EXPECT_TRUE(err.suggestions.empty());
}

TEST_F(ParseErrorTest, LargeLineAndColumnNumbers)
{
    mylang::parser::ParseError err(u"Error at end", 9999, 250);

    EXPECT_EQ(err.line, 9999);
    EXPECT_EQ(err.column, 250);
}

// Literal Expression Tests
TEST_F(ParserTest, ParseNumberLiteral)
{
    mylang::input::FileManager file_manager(parser_test_cases_dir() / "number_literal.txt");
    std::cout << "-- DEBUG : file_manager initialized successfully!" << std::endl;
    mylang::parser::Parser parser(&file_manager);
    // std::cout << boost::stacktrace::stacktrace();
    std::cout << "-- DEBUG : parser initialized successfully!" << std::endl;
    mylang::parser::ast::ExprPtr expr = parser.parsePrimary();
    mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->type, mylang::parser::ast::LiteralExpr::Type::NUMBER);
}

#if 0
TEST_F(ParserTest, ParseStringLiteral)
{
    mylang::input::FileManager file_manager(parser_test_cases_dir() / "string_literal.txt");
    mylang::parser::Parser parser(&file_manager);
    mylang::parser::ast::ExprPtr expr = parser.parsePrimary();
    mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->litType, mylang::parser::ast::LiteralExpr::Type::STRING);
}

TEST_F(ParserTest, ParseBooleanLiteralTrue)
{
    mylang::input::FileManager file_manager(parser_test_cases_dir() / "boolean_literal_true.txt");
    mylang::parser::Parser parser(&file_manager);
    mylang::parser::ast::ExprPtr expr = parser.parsePrimary();
    mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->litType, mylang::parser::ast::LiteralExpr::Type::BOOLEAN);
}

TEST_F(ParserTest, ParseBooleanLiteralFalse)
{
    mylang::input::FileManager file_manager(parser_test_cases_dir() / "boolean_literal_false.txt");
    mylang::parser::Parser parser(&file_manager);
    mylang::parser::ast::ExprPtr expr = parser.parsePrimary();
    mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->litType, mylang::parser::ast::LiteralExpr::Type::BOOLEAN);
}

TEST_F(ParserTest, ParseNoneLiteral)
{
    mylang::input::FileManager file_manager(parser_test_cases_dir() / "none_literal.txt");
    mylang::parser::Parser parser(&file_manager);
    mylang::parser::ast::ExprPtr expr = parser.parsePrimary();
    mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->litType, mylang::parser::ast::LiteralExpr::Type::NONE);
}

TEST_F(ParserTest, ParseParenthesizedNumberLiteral)
{
    mylang::input::FileManager file_manager(parser_test_cases_dir() / "parenthesized_number.txt");
    mylang::parser::Parser parser(&file_manager);
    mylang::parser::ast::ExprPtr expr = parser.parsePrimary();
    mylang::parser::ast::LiteralExpr* literal = parseAndCast<mylang::parser::ast::LiteralExpr>(parser, expr);
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->litType, mylang::parser::ast::LiteralExpr::Type::NUMBER);
}

// ============================================================================
// Name/Identifier Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseIdentifier)
{
    mylang::input::FileManager file_manager(parser_test_cases_dir() / "indentifier.txt");
    mylang::parser::Parser parser(&file_manager);
    mylang::parser::ast::ExprPtr expr = parser.parsePrimary();
    mylang::parser::ast::NameExpr* name_expr = parseAndCast<mylang::parser::ast::NameExpr>(parser, expr);
    ASSERT_NE(name_expr, nullptr);
    EXPECT_EQ(name_expr->name, u"المتنبي");
}

TEST_F(ParserTest, ParseSimpleIdentifier)
{
    // Test with a simple ASCII identifier when supported
    auto tokens = createTokens({mylang::lex::tok::TokenType::NAME});
    // ...
}

// Call Expression Tests
TEST_F(ParserTest, ParseCallExpressionNoArgs)
{
    mylang::input::FileManager file_manager(parser_test_cases_dir() / "call_expression.txt");
    mylang::parser::Parser parser(&file_manager);
    mylang::parser::ast::ExprPtr expr = parser.parsePrimary();
    mylang::parser::ast::CallExpr* call_expr = parseAndCast<mylang::parser::ast::CallExpr>(parser, expr);
    ASSERT_NE(call_expr, nullptr);
    mylang::parser::ast::NameExpr* callee_name = dynamic_cast<mylang::parser::ast::NameExpr*>(call_expr->callee.get());
    ASSERT_NE(callee_name, nullptr);
    EXPECT_EQ(callee_name->name, u"اطبع");
    EXPECT_TRUE(call_expr->args.empty());
}

TEST_F(ParserTest, ParseCallExpressionWithArgs)
{
    // TODO: Add test case file with arguments
    // Expected: function_name(arg1, arg2, arg3)
    GTEST_SKIP() << "Test file with arguments not yet created";
}

TEST_F(ParserTest, ParseNestedCallExpression)
{
    // TODO: Add test for nested calls like f(g(x))
    GTEST_SKIP() << "Nested call test not yet implemented";
}

// ============================================================================
// Binary Expression Tests
// ============================================================================

TEST_F(ParserTest, ParseSimpleAddition)
{
    // TODO: Add test for binary operations
    GTEST_SKIP() << "Binary operation tests not yet implemented";
}

TEST_F(ParserTest, ParseComplexExpression)
{
    // TODO: Test operator precedence: 2 + 3 * 4
    GTEST_SKIP() << "Complex expression tests not yet implemented";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ParserTest, ParseInvalidSyntaxThrows)
{
    // TODO: Verify parser throws on invalid syntax
    GTEST_SKIP() << "Error handling tests not yet implemented";
}

TEST_F(ParserTest, ParseUnmatchedParenthesis)
{
    // TODO: Test error on unmatched parenthesis
    GTEST_SKIP() << "Parenthesis matching tests not yet implemented";
}

TEST_F(ParserTest, ParseUnexpectedEOF)
{
    // TODO: Test handling of unexpected end of file
    GTEST_SKIP() << "EOF handling tests not yet implemented";
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ParserTest, ParseEmptyInput)
{
    // TODO: Test parser behavior on empty input
    GTEST_SKIP() << "Empty input test not yet implemented";
}

TEST_F(ParserTest, ParseWhitespaceOnly)
{
    // TODO: Test parser with whitespace-only input
    GTEST_SKIP() << "Whitespace handling test not yet implemented";
}

TEST_F(ParserTest, ParseVeryLongIdentifier)
{
    // TODO: Test with extremely long identifier names
    GTEST_SKIP() << "Long identifier test not yet implemented";
}

// ============================================================================
// Performance Tests (Optional)
// ============================================================================

TEST_F(ParserTest, DISABLED_ParseLargeFile)
{
    // Disabled by default, enable for performance testing
    GTEST_SKIP() << "Performance test - run manually";
}
#endif  // Parser tests disabled pending stabilization