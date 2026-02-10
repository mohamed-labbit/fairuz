// parser rewrite
// recursive descent parser
//
// This header defines the recursive-descent parser for mylang.
// The parser consumes tokens produced by the lexer and builds
// an abstract syntax tree (AST) representing expressions and statements.

#pragma once

#include "../ast/ast.hpp"
#include "../input/file_manager.hpp"
#include "../lex/lexer.hpp"
#include "../lex/token.hpp"

#include <optional>
#include <sstream>

namespace mylang {
namespace parser {

/**
 * @brief Exception type representing a parse-time error.
 *
 * ParseError carries precise source location information (line and column),
 * optional source context, and a list of suggestions to aid error recovery
 * or diagnostics.
 */
class ParseError : public std::runtime_error {
public:
    std::int32_t Line_, Column_;         // Source location of the error
    StringRef Context_;                  // Source line where the error occurred
    std::vector<StringRef> Suggestions_; // Optional recovery suggestions

    /**
     * @brief Constructs a parse error.
     *
     * @param msg  Error message (UTF-16)
     * @param l    Line number
     * @param c    Column number
     * @param ctx  Optional source line context
     * @param sugg Optional list of suggestions
     */
    ParseError(StringRef const& msg, unsigned l, unsigned c, StringRef ctx = u"", std::vector<StringRef> sugg = {})
        : Line_(l)
        , Column_(c)
        , Context_(ctx)
        , Suggestions_(sugg)
        , std::runtime_error("")
    {
    }

    /**
     * @brief Formats the parse error into a user-readable message.
     *
     * The output includes:
     *  - Line and column
     *  - Error message
     *  - Source line with a caret pointing at the error location
     *  - Optional suggestions
     *
     * @return UTF-16 formatted error message
     */
    StringRef format() const
    {
        std::stringstream ss;
        ss << "Line " << Line_ << ":" << Column_ << " - " << what() << "\n";

        if (!Context_.empty()) {
            ss << "  | " << Context_ << "\n";
            ss << "  | " << std::string(Column_ - 1, ' ') << "^\n";
        }

        if (!Suggestions_.empty()) {
            ss << "Suggestions:\n";
            for (StringRef const& s : Suggestions_)
                ss << "  - " << s << "\n";
        }

        return StringRef::fromUtf8(ss.str());
    }
};

/**
 * @brief Recursive-descent parser for mylang.
 *
 * The Parser consumes tokens from the lexer and produces AST nodes.
 * It implements expression parsing using a combination of:
 *  - Recursive descent
 *  - Operator precedence climbing
 *  - Specialized routines for different grammar constructs
 */
class Parser {
public:
    /// @brief Constructs an empty parser
    explicit Parser() = default;

    /// @brief Constructs a parser bound to a file manager
    explicit Parser(input::FileManager* fm)
        : Lexer_(fm)
    {
        if (!fm)
            diagnostic::engine.panic("file_manager is NULL!");
        // Advance to the first real token
        Lexer_.next();
    }

    /// @brief Constructs a parser from a pre-existing token sequence
    explicit Parser(std::vector<tok::Token> seq, std::optional<SizeType> s = std::nullopt);

    std::vector<ast::Stmt*> parseProgram();

    ast::Stmt* parseStatement();

    ast::Stmt* parseExpressionStmt();

    ast::Stmt* parseIfStmt();

    ast::Stmt* parseWhileStmt();

    ast::Stmt* parseReturnStmt();

    ast::Stmt* parseFunctionDef();

    ast::ListExpr* parseParametersList();

    ast::BlockStmt* parseBlock();

    ast::Expr* parseParenthesizedExprContent();

    // Expression parsing entry points

    /// @brief does exactly what it says
    ast::ListExpr* parseFunctionArguments();

    ast::Expr* parseCallExpr(ast::Expr* callee);

    /// @brief Parses a primary expression
    ast::Expr* parsePrimary();

    /// @brief Parses a unary expression
    ast::Expr* parseUnary();

    /// @brief Parses a parenthesized expression
    ast::Expr* parseParenthesizedExpr();

    /// @brief Parses a general expression
    ast::Expr* parseExpression() { return parseAssignmentExpr(); }

    /// @brief Parses an assignment expression
    ast::Expr* parseAssignmentExpr();

    /// @brief Parses a list literal expression
    ast::Expr* parseListLiteral();

    /// @brief Parses a conditional (ternary-like) expression
    ast::Expr* parseConditionalExpr()
    {
        return parseLogicalExpr();
        /// TODO: Ternary?
    }

    /// @brief Parses logical expressions (AND / OR)
    ast::Expr* parseLogicalExpr() { return parseLogicalExprPrecedence(0); }

    /// @brief Parses logical expressions using precedence climbing
    ast::Expr* parseLogicalExprPrecedence(unsigned min_precedence);

    /// @brief Parses binary expressions using precedence climbing
    ast::Expr* parseBinaryExprPrecedence(unsigned min_precedence);

    /// @brief Parses comparison expressions
    ast::Expr* parseComparisonExpr();

    /// @brief Parses binary expressions
    ast::Expr* parseBinaryExpr() { return parseBinaryExprPrecedence(0); }

    /// @brief Parses unary expressions
    ast::Expr* parseUnaryExpr();

    /// @brief Parses primary expressions
    ast::Expr* parsePrimaryExpr();

    /// @brief Parses postfix expressions (calls, indexing, etc.)
    ast::Expr* parsePostfixExpr();

    /**
     * @brief Parses the entire input into a sequence of statements.
     *
     * @return Vector of parsed statement AST nodes
     */
    // std::vector<ast::Stmt*> parse();
    /// TODO: not sure if these should be private
    /// @brief check wether or not we reached the end of the file so not to bother lookin for stuff to parse
    bool weDone() const { return Lexer_.current()->is(tok::TokenType::ENDMARKER); }

    /// @brief Checks whether the current token is of the given type
    bool check(tok::TokenType type)
    {
        // if (weDone()) return false;
        return Lexer_.current()->is(type);
    }

    tok::Token const* currentToken() { return Lexer_.current(); }

    ast::Expr* parse() { return parseExpression(); }

    ast::BlockStmt* parseIndentedBlock();

private:
    lex::Lexer Lexer_; // Underlying lexer providing tokens

    /// @brief Peeks ahead in the token stream without consuming
    tok::Token const* peek(SizeType offset = 1) { return Lexer_.peek(offset); }

    /// @brief Advances and returns the next token
    tok::Token const* advance() { return Lexer_.next(); }

    /// @brief Matches and consumes a token if it is of the given type
    bool match(tok::TokenType const type);

    /**
     * @brief Consumes a token of the expected type or throws a ParseError.
     *
     * @param type Expected token type
     * @param msg  Error message if the token does not match
     */
    MYLANG_NODISCARD
    bool consume(tok::TokenType type, StringRef const& msg)
    {
        if (check(type)) {
            advance();
            return true;
        }
        // diagnostic::engine.emit(msg, diagnostic::DiagnosticEngine::Severity::ERROR);
        return false;
    }

    /// @brief Skips newline tokens during parsing
    void skipNewlines()
    {
        while (match(tok::TokenType::NEWLINE))
            ;
    }

    /// @brief Retrieves a source line for diagnostics
    StringRef getSourceLine(SizeType line) { return Lexer_.getSourceLine(line); }

    /// @brief Enters a new scope (currently a no-op)
    void enterScope() { }

    void synchronize();
};

} // namespace parser
} // namespace mylang
