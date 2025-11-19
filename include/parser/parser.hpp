#pragma once


#include "../lex/lexer.hpp"
#include "../lex/token.hpp"
#include "ast/ast.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace mylang {
namespace parser {

// Advanced error with source context and suggestions
class ParseError: public std::runtime_error
{
   public:
    std::int32_t line, column;
    std::u16string context;
    std::vector<std::u16string> suggestions;

    ParseError(const std::u16string& msg,
      std::int32_t l,
      std::int32_t c,
      std::u16string ctx = u"",
      std::vector<std::u16string> sugg = {}) :
        std::runtime_error(utf8::utf16to8(msg)),
        line(l),
        column(c),
        context(std::move(ctx)),
        suggestions(std::move(sugg))
    {
    }

    std::u16string format() const;
};

// Parser with memoization for packrat parsing
class Parser
{
   private:
    struct Scope
    {
        std::unordered_set<std::u16string> variables;
        std::unordered_set<std::u16string> functions;
        Scope* parent{nullptr};

        bool isDefined(const std::u16string& name) const
        {
            if (variables.count(name) || functions.count(name))
            {
                return true;
            }

            return parent ? parent->isDefined(name) : false;
        }
    };

    // Error tracking for batch reporting
    struct ErrorInfo
    {
        ParseError error;
        std::size_t position{0};
    };

    // Packrat parsing memoization cache
    struct MemoEntry
    {
        ast::ExprPtr* result{nullptr};
        std::size_t endPos{0};
        bool failed{false};
    };

    struct OpInfo
    {
        std::int32_t precedence;
        bool rightAssoc;
        bool isComparison;
    };

    lex::Lexer lexer_;
    std::size_t current_{0};
    unsigned loopDepth_{0};
    unsigned functionDepth_{0};

    std::vector<lex::tok::Token> tokens_;
    std::size_t current{0};
    bool use_lexer{false};

    std::unordered_map<std::u16string, MemoEntry> memoCache_;

    // Symbol table for semantic analysis during parsing
    std::vector<std::unique_ptr<Scope>> scopeStack_;

    std::vector<ErrorInfo> errors_;

    const std::unordered_set<lex::tok::TokenType> syncTokens_ = {lex::tok::TokenType::KW_IF,
      lex::tok::TokenType::KW_WHILE, lex::tok::TokenType::KW_FOR, lex::tok::TokenType::KW_FN,
      lex::tok::TokenType::KW_RETURN, lex::tok::TokenType::DEDENT, lex::tok::TokenType::NEWLINE};

    const std::unordered_map<std::u16string, std::u16string> errorSuggestions_ = {{u"اذ", u"اذا"}, {u"طالم", u"طالما"},
      {u"لك", u"لكل"}, {u"ف", u"في"}, {u"عر", u"عرف"}, {u"صحي", u"صحيح"}, {u"خط", u"خطا"}, {u"عد", u"عدم"},
      {u"ليست", u"ليس"}, {u"او", u"او"}, {u"وا", u"و"}};
    ;

    // === lex::tok::Token Access & Navigation ===

    lex::tok::Token peek(std::size_t offset = 1);

    lex::tok::Token previous();

    lex::tok::Token advance();

    bool check(lex::tok::TokenType type);

    bool checkAny(std::initializer_list<lex::tok::TokenType> types);

    bool match(std::initializer_list<lex::tok::TokenType> types);

    lex::tok::Token consume(lex::tok::TokenType type, const std::u16string& msg);

    void skipNewlines();

    // Get source line for error context
    std::u16string getSourceLine(std::int32_t line);

    // Generate smart suggestions based on context
    std::vector<std::u16string> getSuggestions(lex::tok::TokenType expected);

    // Error recovery with Panic Mode
    void synchronize();

    // Record error without throwing
    void recordError(const ParseError& error);

    // === Scope Management ===

    void enterScope();

    void exitScope();

    void declareVariable(const std::u16string& name);

    void declareFunction(const std::u16string& name);

    bool isVariableDefined(const std::u16string& name) const;

    // === Memoization for Packrat Parsing ===

    std::u16string getMemoKey(const std::u16string& rule) const;

    template<typename T>
    std::optional<T> getMemo(const std::u16string& rule);

    template<typename T>
    void setMemo(const std::u16string& rule, T result);

    // === Operator Precedence & Properties ===

    OpInfo getOpInfo(lex::tok::TokenType type) const;

    bool isUnaryOp(lex::tok::TokenType type) const;

    bool isBinaryOp(lex::tok::TokenType type) const;

    bool isAugmentedAssign(lex::tok::TokenType type) const;

    bool isExpressionStart();

    // === Advanced Expression Parsing ===

   public:
    explicit Parser(std::ifstream* file) :
        lexer_(file),
        use_lexer(true)
    {
        enterScope();  // global
    }

    explicit Parser(std::vector<lex::tok::Token> toks) :
        tokens_(toks)
    {
        enterScope();
    }

    explicit Parser(const std::u16string source) :
        lexer_(source),
        use_lexer(true)
    {
        enterScope();
    }

    struct ParseStats
    {
        std::size_t tokenCount;
        std::size_t statementCount;
        std::size_t errorCount;
        std::size_t cacheHits;

        double parseTimeMs;
    };

    // Parse primary with speculative parsing for ambiguity
    ast::ExprPtr parsePrimary();

    // Parse postfix operations (calls, subscripts, attributes)
    ast::ExprPtr parsePostfixOps(ast::ExprPtr expr);

    // Parse parenthesized expression or tuple
    ast::ExprPtr parseParenthesizedExpr();

    // Parse list literal with comprehensions
    ast::ExprPtr parseListLiteral();

    // Parse dictionary literal
    ast::ExprPtr parseDictLiteral();

    // Parse lambda expression
    ast::ExprPtr parseLambda();

    // Pratt parser for expressions with left recursion handling
    ast::ExprPtr parseUnary();

    ast::ExprPtr parsePower();

    ast::ExprPtr parseBinaryExpr(std::int32_t minPrec);

    // Python-style comparison chaining: a < b < c == d
    ast::ExprPtr parseComparison();

    ast::ExprPtr parseTernary();

    ast::ExprPtr parseStarredExpr();

    ast::ExprPtr parseExpression();

    ast::ExprPtr parseAssignmentExpr();

    std::vector<ast::ExprPtr> parseExpressionList();

    // === Statement Parsing ===

    std::vector<ast::StmtPtr> parseBlock();

    ast::StmtPtr parseSimpleStmt();

    ast::StmtPtr parseCompoundStmt();

    ast::StmtPtr parseStatement();

    // === Main Parse Method with Advanced Error Recovery ===

    std::vector<ast::StmtPtr> parse();

    // === Utility Methods ===

    // Get parsing statistics

    // ParseStats getStats() const { return this->parse_stats_; }

    // Clear memoization cache (for memory management)
    void clearCache();
};

// Static initializations
// const std::unordered_set<lex::tok::TokenType> Parser::syncTokens_ =

// const std::unordered_map<std::u16string, std::u16string> Parser::errorSuggestions_
}
}