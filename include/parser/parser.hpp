#pragma once

#include "../ast/ast.hpp"
#include "../lex/file_manager.hpp"
#include "../lex/lexer.hpp"
#include "../lex/token.hpp"

#include <optional>
#include <sstream>

namespace mylang {
namespace parser {

class ParseError : public std::runtime_error {
public:
    std::int32_t Line_, Column_;
    StringRef Context_;
    std::vector<StringRef> Suggestions_;

    ParseError(StringRef const& msg, unsigned int l, unsigned int c, StringRef ctx = "", std::vector<StringRef> sugg = {})
        : Line_(l)
        , Column_(c)
        , Context_(ctx)
        , Suggestions_(sugg)
        , std::runtime_error("")
    {
    }

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

        return StringRef(ss.str().data());
    }
};

class Parser {
public:
    explicit Parser() = default;

    explicit Parser(lex::FileManager* fm)
        : Lexer_(fm)
    {
        if (!fm)
            diagnostic::engine.panic("file_manager is NULL!");

        Lexer_.next();
    }

    explicit Parser(std::vector<tok::Token> seq, std::optional<std::size_t> s = std::nullopt);

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

    ast::ListExpr* parseFunctionArguments();

    ast::Expr* parseCallExpr(ast::Expr* callee);

    ast::Expr* parsePrimary();

    ast::Expr* parseUnary();

    ast::Expr* parseParenthesizedExpr();

    ast::Expr* parseExpression()
    {
        return parseAssignmentExpr();
    }

    ast::Expr* parseAssignmentExpr();

    ast::Expr* parseListLiteral();

    ast::Expr* parseConditionalExpr()
    {
        return parseLogicalExpr();
        /// TODO: Ternary?
    }

    ast::Expr* parseLogicalExpr()
    {
        return parseLogicalExprPrecedence(0);
    }

    ast::Expr* parseLogicalExprPrecedence(unsigned int min_precedence);

    ast::Expr* parseBinaryExprPrecedence(unsigned int min_precedence);

    ast::Expr* parseComparisonExpr();

    ast::Expr* parseBinaryExpr()
    {
        return parseBinaryExprPrecedence(0);
    }

    ast::Expr* parseUnaryExpr();

    ast::Expr* parsePrimaryExpr();

    ast::Expr* parsePostfixExpr();

    /// TODO: not sure if these should be private
    bool weDone() const
    {
        return Lexer_.current()->is(tok::TokenType::ENDMARKER);
    }

    bool check(tok::TokenType type)
    {
        return Lexer_.current()->is(type);
    }

    tok::Token const* currentToken()
    {
        return Lexer_.current();
    }

    ast::Expr* parse()
    {
        return parseExpression();
    }

    ast::BlockStmt* parseIndentedBlock();

private:
    lex::Lexer Lexer_;

    tok::Token const* peek(std::size_t offset = 1)
    {
        return Lexer_.peek(offset);
    }

    tok::Token const* advance()
    {
        return Lexer_.next();
    }

    bool match(tok::TokenType const type);

    [[nodiscard]]
    bool consume(tok::TokenType type, StringRef const& msg)
    {
        if (check(type)) {
            advance();
            return true;
        }
        // diagnostic::engine.emit(msg, diagnostic::DiagnosticEngine::Severity::ERROR);
        return false;
    }

    void skipNewlines()
    {
        while (match(tok::TokenType::NEWLINE))
            ;
    }

    StringRef getSourceLine(std::size_t line)
    {
        return Lexer_.getSourceLine(line);
    }

    void enterScope() { }

    void synchronize();
};

} // namespace parser
} // namespace mylang
