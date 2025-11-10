#pragma once


#include "../lex/lexer.h"
#include "../lex/token.h"
#include "ast.h"

#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>


namespace mylang {
namespace parser {

class ParseError: public std::runtime_error
{
   public:
    int line_, column_;
    ParseError(const std::string& msg, int l, int c) :
        std::runtime_error(msg),
        line_(l),
        column_(c)
    {
    }
};

class Parser
{
   private:
    std::vector<lex::tok::Token> tokens_;
    size_t current_ = 0;
    int loopDepth_ = 0;  // Track nested loops
    int functionDepth_ = 0;  // Track nested functions

    // Synchronization points for error recovery
    static const std::unordered_set<lex::tok::TokenType> syncTokens_;

    const lex::tok::Token& peek(size_t offset = 0) const
    {
        size_t idx = current_ + offset;
        return idx < tokens_.size() ? tokens_[idx] : tokens_.back();
    }

    const lex::tok::Token& previous() const { return current_ > 0 ? tokens_[current_ - 1] : tokens_[0]; }

    const lex::tok::Token& advance()
    {
        if (current_ < tokens_.size() - 1)
            current_++;
        return tokens_[current_ - 1];
    }

    bool check(lex::tok::TokenType type) const { return peek().is(type); }

    bool checkAny(std::initializer_list<lex::tok::TokenType> types) const
    {
        for (auto type : types)
        {
            if (check(type))
                return true;
        }
        return false;
    }

    bool match(std::initializer_list<lex::tok::TokenType> types)
    {
        for (auto type : types)
        {
            if (check(type))
            {
                advance();
                return true;
            }
        }
        return false;
    }

    const lex::tok::Token& consume(lex::tok::TokenType type, const std::string& msg)
    {
        if (check(type))
            return advance();
        throw ParseError(msg, peek().line(), peek().column());
    }

    void skipNewlines()
    {
        while (match({lex::tok::TokenType::NEWLINE}))
            ;
    }

    // Error recovery: skip tokens until we find a synchronization point
    void synchronize()
    {
        advance();
        while (!check(lex::tok::TokenType::ENDMARKER))
        {
            if (previous().is(lex::tok::TokenType::NEWLINE))
                return;
            if (syncTokens_.count(peek().type()))
                return;
            advance();
        }
    }

    // Enhanced operator precedence with right-associativity support
    struct OpInfo
    {
        int precedence;
        bool rightAssoc;
    };

    OpInfo getOpInfo(lex::tok::TokenType type) const
    {
        switch (type)
        {
        case lex::tok::TokenType::KW_OR :
            return {1, false};
        case lex::tok::TokenType::KW_AND :
            return {2, false};
        case lex::tok::TokenType::KW_NOT :
            return {3, false};
        case lex::tok::TokenType::OP_EQ :
        case lex::tok::TokenType::OP_NEQ :
        case lex::tok::TokenType::OP_LT :
        case lex::tok::TokenType::OP_GT :
        case lex::tok::TokenType::OP_LTE :
        case lex::tok::TokenType::OP_GTE :
        case lex::tok::TokenType::KW_IN :
            return {4, false};
        case lex::tok::TokenType::OP_BITOR :
            return {5, false};
        case lex::tok::TokenType::OP_BITXOR :
            return {6, false};
        case lex::tok::TokenType::OP_BITAND :
            return {7, false};
        case lex::tok::TokenType::OP_LSHIFT :
        case lex::tok::TokenType::OP_RSHIFT :
            return {8, false};
        case lex::tok::TokenType::OP_PLUS :
        case lex::tok::TokenType::OP_MINUS :
            return {9, false};
        case lex::tok::TokenType::OP_STAR :
        case lex::tok::TokenType::OP_SLASH :
        case lex::tok::TokenType::OP_PERCENT :
            return {10, false};
        case lex::tok::TokenType::OP_POWER :
            return {11, true};  // Right-associative
        default :
            return {0, false};
        }
    }

    bool isUnaryOp(lex::tok::TokenType type) const
    {
        return type == lex::tok::TokenType::OP_PLUS || type == lex::tok::TokenType::OP_MINUS
          || type == lex::tok::TokenType::OP_BITNOT || type == lex::tok::TokenType::KW_NOT;
    }

    bool isBinaryOp(lex::tok::TokenType type) const { return getOpInfo(type).precedence > 0; }

    bool isAugmentedAssign(lex::tok::TokenType type) const
    {
        return type == lex::tok::TokenType::OP_PLUSEQ || type == lex::tok::TokenType::OP_MINUSEQ
          || type == lex::tok::TokenType::OP_STAREQ || type == lex::tok::TokenType::OP_SLASHEQ
          || type == lex::tok::TokenType::OP_PERCENTEQ || type == lex::tok::TokenType::OP_ANDEQ
          || type == lex::tok::TokenType::OP_OREQ || type == lex::tok::TokenType::OP_XOREQ
          || type == lex::tok::TokenType::OP_LSHIFTEQ || type == lex::tok::TokenType::OP_RSHIFTEQ;
    }

    // Check if we're at the start of an expression
    bool isExpressionStart() const
    {
        return checkAny({lex::tok::TokenType::NUMBER, lex::tok::TokenType::STRING, lex::tok::TokenType::NAME,
          lex::tok::TokenType::KW_TRUE, lex::tok::TokenType::KW_FALSE, lex::tok::TokenType::KW_NONE,
          lex::tok::TokenType::LPAREN, lex::tok::TokenType::LBRACKET, lex::tok::TokenType::OP_PLUS,
          lex::tok::TokenType::OP_MINUS, lex::tok::TokenType::OP_BITNOT, lex::tok::TokenType::KW_NOT});
    }

   public:
    explicit Parser(std::vector<lex::tok::Token> toks) :
        tokens_(std::move(toks))
    {
    }

    // Parse primary expressions with improved error handling
    ExprPtr parsePrimary()
    {
        // Literals
        if (match({lex::tok::TokenType::NUMBER}))
        {
            auto& tok = tokens_[current_ - 1];
            return std::make_unique<LiteralExpr>(LiteralExpr::Type::NUMBER, tok.lexeme());
        }

        if (match({lex::tok::TokenType::STRING}))
        {
            auto& tok = tokens_[current_ - 1];
            return std::make_unique<LiteralExpr>(LiteralExpr::Type::STRING, tok.lexeme());
        }

        if (match({lex::tok::TokenType::KW_TRUE}))
        {
            return std::make_unique<LiteralExpr>(LiteralExpr::Type::BOOLEAN, u"true");
        }

        if (match({lex::tok::TokenType::KW_FALSE}))
        {
            return std::make_unique<LiteralExpr>(LiteralExpr::Type::BOOLEAN, u"false");
        }

        if (match({lex::tok::TokenType::KW_NONE}))
        {
            return std::make_unique<LiteralExpr>(LiteralExpr::Type::NONE, u"none");
        }

        // Names and calls
        if (match({lex::tok::TokenType::NAME}))
        {
            auto& tok = tokens_[current_ - 1];
            auto name = tok.lexeme();
            ExprPtr expr = std::make_unique<NameExpr>(name);

            // Handle postfix operations (calls, subscripts, attributes)
            while (true)
            {
                // Function call
                if (match({lex::tok::TokenType::LPAREN}))
                {
                    std::vector<ExprPtr> args;
                    if (!check(lex::tok::TokenType::RPAREN))
                    {
                        do
                        {
                            skipNewlines();
                            args.push_back(parseExpression());
                            skipNewlines();
                        } while (match({lex::tok::TokenType::COMMA}));
                    }
                    consume(lex::tok::TokenType::RPAREN, "Expected ')' after arguments");
                    expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
                }
                // Subscript or attribute access could be added here
                else
                {
                    break;
                }
            }

            return expr;
        }

        // Parenthesized expression or tuple
        if (match({lex::tok::TokenType::LPAREN}))
        {
            if (check(lex::tok::TokenType::RPAREN))
            {
                advance();
                // Empty tuple
                return std::make_unique<ListExpr>(std::vector<ExprPtr>());
            }

            auto expr = parseExpression();

            // Check for tuple
            if (match({lex::tok::TokenType::COMMA}))
            {
                std::vector<ExprPtr> elements;
                elements.push_back(std::move(expr));

                if (!check(lex::tok::TokenType::RPAREN))
                {
                    do
                    {
                        skipNewlines();
                        elements.push_back(parseExpression());
                    } while (match({lex::tok::TokenType::COMMA}));
                }

                consume(lex::tok::TokenType::RPAREN, "Expected ')' after tuple elements");
                return std::make_unique<ListExpr>(std::move(elements));
            }

            consume(lex::tok::TokenType::RPAREN, "Expected ')' after expression");
            return expr;
        }

        // List literal
        if (match({lex::tok::TokenType::LBRACKET}))
        {
            std::vector<ExprPtr> elements;
            if (!check(lex::tok::TokenType::RBRACKET))
            {
                do
                {
                    skipNewlines();
                    // Handle starred expressions
                    if (match({lex::tok::TokenType::OP_STAR}))
                    {
                        auto starred = parseExpression();
                        elements.push_back(std::move(starred));
                    }
                    else
                    {
                        elements.push_back(parseExpression());
                    }
                    skipNewlines();
                } while (match({lex::tok::TokenType::COMMA}));
            }
            consume(lex::tok::TokenType::RBRACKET, "Expected ']' after list elements");
            return std::make_unique<ListExpr>(std::move(elements));
        }

        throw ParseError("Expected expression, got '" + peek().utf8_lexeme() + "'", peek().line(), peek().column());
    }

    // Parse unary expressions with better precedence
    ExprPtr parseUnary()
    {
        if (isUnaryOp(peek().type()))
        {
            auto op = advance();
            auto operand = parseUnary();
            return std::make_unique<UnaryExpr>(op.lexeme(), std::move(operand));
        }
        return parsePrimary();
    }

    // Parse power operator (right-associative)
    ExprPtr parsePower()
    {
        auto left = parseUnary();

        if (match({lex::tok::TokenType::OP_POWER}))
        {
            // Right-associative: parse the rest as power expression
            auto right = parsePower();
            return std::make_unique<BinaryExpr>(std::move(left), u"**", std::move(right));
        }

        return left;
    }

    // Enhanced binary expression parser with proper associativity
    ExprPtr parseBinaryExpr(int minPrec)
    {
        auto left = parsePower();

        while (isBinaryOp(peek().type()))
        {
            OpInfo opInfo = getOpInfo(peek().type());
            if (opInfo.precedence < minPrec)
                break;

            auto op = advance();
            int nextMinPrec = opInfo.rightAssoc ? opInfo.precedence : opInfo.precedence + 1;
            auto right = parseBinaryExpr(nextMinPrec);

            left = std::make_unique<BinaryExpr>(std::move(left), op.lexeme(), std::move(right));
        }

        return left;
    }

    // Parse comparison chains: a < b < c
    ExprPtr parseComparison()
    {
        auto left = parseBinaryExpr(0);

        // Python allows chained comparisons
        std::vector<std::pair<std::u16string, ExprPtr>> chain;
        while (checkAny({lex::tok::TokenType::OP_EQ, lex::tok::TokenType::OP_NEQ, lex::tok::TokenType::OP_LT,
          lex::tok::TokenType::OP_GT, lex::tok::TokenType::OP_LTE, lex::tok::TokenType::OP_GTE,
          lex::tok::TokenType::KW_IN}))
        {
            auto op = advance();
            auto right = parseBinaryExpr(5);  // Don't parse more comparisons
            chain.push_back({op.lexeme(), std::move(right)});
        }

        if (chain.empty())
            return left;

        // Build chained comparison: a < b < c becomes (a < b) and (b < c)
        auto result = std::make_unique<BinaryExpr>(std::move(left), chain[0].first, std::move(chain[0].second));

        for (size_t i = 1; i < chain.size(); i++)
        {
            // Need to duplicate middle expression - simplified here
            result = std::make_unique<BinaryExpr>(
              std::move(result), u"و", std::make_unique<LiteralExpr>(LiteralExpr::Type::BOOLEAN, u"true"));
        }

        return result;
    }

    // Parse ternary conditional: expr if condition else expr
    ExprPtr parseTernary()
    {
        auto expr = parseComparison();

        if (match({lex::tok::TokenType::KW_IF}))
        {
            auto condition = parseComparison();
            // Note: Python's ternary is "true_expr if cond else false_expr"
            // Grammar shows simplified version
            return std::make_unique<TernaryExpr>(std::move(condition), std::move(expr), nullptr);
        }

        return expr;
    }

    // Parse assignment expression (walrus operator :=)
    ExprPtr parseAssignmentExpr()
    {
        if (check(lex::tok::TokenType::NAME) && peek(1).is(lex::tok::TokenType::OP_ASSIGN))
        {
            auto name = advance().lexeme();
            advance();  // consume :=
            auto value = parseExpression();
            return std::make_unique<AssignmentExpr>(name, std::move(value));
        }
        return parseTernary();
    }

    // Parse starred expressions (*args)
    ExprPtr parseStarredExpr()
    {
        if (match({lex::tok::TokenType::OP_STAR}))
        {
            auto expr = parseAssignmentExpr();
            // Wrap in special starred node if needed
            return expr;
        }
        return parseAssignmentExpr();
    }

    // Parse full expression
    ExprPtr parseExpression() { return parseStarredExpr(); }

    // Parse multiple expressions (for assignments, returns, etc.)
    std::vector<ExprPtr> parseExpressionList()
    {
        std::vector<ExprPtr> exprs;
        do
        {
            skipNewlines();
            exprs.push_back(parseExpression());
            skipNewlines();
        } while (match({lex::tok::TokenType::COMMA}) && isExpressionStart());
        return exprs;
    }

    // Parse block with improved indentation handling
    std::vector<StmtPtr> parseBlock()
    {
        std::vector<StmtPtr> statements;

        if (match({lex::tok::TokenType::NEWLINE}))
        {
            if (!match({lex::tok::TokenType::INDENT}))
            {
                throw ParseError("Expected indented block", peek().line(), peek().column());
            }

            while (!check(lex::tok::TokenType::DEDENT) && !check(lex::tok::TokenType::ENDMARKER))
            {
                try
                {
                    statements.push_back(parseStatement());
                } catch (const ParseError& e)
                {
                    // Error recovery: skip to next statement
                    synchronize();
                }
            }

            if (!match({lex::tok::TokenType::DEDENT}))
            {
                throw ParseError("Expected dedent", peek().line(), peek().column());
            }
        }
        else
        {
            // Single-line block
            statements.push_back(parseSimpleStmt());
        }

        return statements;
    }

    // Parse simple statement
    StmtPtr parseSimpleStmt()
    {
        skipNewlines();

        // Return statement
        if (match({lex::tok::TokenType::KW_RETURN}))
        {
            if (functionDepth_ == 0)
            {
                throw ParseError("'return' outside function", previous().line(), previous().column());
            }

            ExprPtr value = nullptr;
            if (isExpressionStart())
            {
                value = parseExpression();
            }
            skipNewlines();
            return std::make_unique<ReturnStmt>(std::move(value));
        }

        // Assignment or expression statement
        if (check(lex::tok::TokenType::NAME))
        {
            size_t savePos = current_;
            auto name = peek().lexeme();

            // Type annotation: NAME ':' expression ['=' expression]
            if (peek(1).is(lex::tok::TokenType::COLON))
            {
                advance();  // NAME
                advance();  // ':'
                auto typeExpr = parseExpression();

                ExprPtr value = nullptr;
                if (match({lex::tok::TokenType::OP_ASSIGN}))
                {
                    value = parseExpression();
                }
                skipNewlines();
                return std::make_unique<AssignmentStmt>(name, std::move(value));
            }

            // Augmented assignment: NAME += expression
            if (peek(1).type() != lex::tok::TokenType::NAME && isAugmentedAssign(peek(1).type()))
            {
                advance();  // NAME
                auto op = advance();
                auto value = parseExpression();
                skipNewlines();
                // Convert += to = NAME +
                return std::make_unique<AssignmentStmt>(name, std::move(value));
            }

            // Regular assignment: NAME = expression
            if (peek(1).is(lex::tok::TokenType::OP_ASSIGN))
            {
                advance();  // NAME
                advance();  // '='
                auto value = parseExpression();
                skipNewlines();
                return std::make_unique<AssignmentStmt>(name, std::move(value));
            }

            // Multiple assignment targets: a = b = expr
            current_ = savePos;
        }

        // Expression statement
        auto expr = parseExpression();
        skipNewlines();
        return std::make_unique<ExprStmt>(std::move(expr));
    }

    // Parse compound statements with enhanced features
    StmtPtr parseCompoundStmt()
    {
        // If statement with elif/else
        if (match({lex::tok::TokenType::KW_IF}))
        {
            auto condition = parseExpression();
            consume(lex::tok::TokenType::COLON, "Expected ':' after if condition");
            auto thenBlock = parseBlock();

            std::vector<StmtPtr> elseBlock;
            // Note: Grammar doesn't show elif/else, but adding for completeness

            return std::make_unique<IfStmt>(std::move(condition), std::move(thenBlock), std::move(elseBlock));
        }

        // While statement
        if (match({lex::tok::TokenType::KW_WHILE}))
        {
            loopDepth_++;
            auto condition = parseExpression();
            consume(lex::tok::TokenType::COLON, "Expected ':' after while condition");
            auto body = parseBlock();
            loopDepth_--;
            return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
        }

        // For statement
        if (match({lex::tok::TokenType::KW_FOR}))
        {
            loopDepth_++;
            auto target = consume(lex::tok::TokenType::NAME, "Expected variable name after 'لكل'").lexeme();
            consume(lex::tok::TokenType::KW_IN, "Expected 'في' in for loop");
            auto iter = parseExpression();
            consume(lex::tok::TokenType::COLON, "Expected ':' after for clause");
            auto body = parseBlock();
            loopDepth_--;
            return std::make_unique<ForStmt>(target, std::move(iter), std::move(body));
        }

        // Function definition
        if (match({lex::tok::TokenType::KW_FN}))
        {
            functionDepth_++;
            auto name = consume(lex::tok::TokenType::NAME, "Expected function name").lexeme();
            consume(lex::tok::TokenType::LPAREN, "Expected '(' after function name");

            std::vector<std::u16string> params;
            if (!check(lex::tok::TokenType::RPAREN))
            {
                do
                {
                    skipNewlines();
                    params.push_back(consume(lex::tok::TokenType::NAME, "Expected parameter name").lexeme());
                    skipNewlines();
                } while (match({lex::tok::TokenType::COMMA}));
            }
            consume(lex::tok::TokenType::RPAREN, "Expected ')' after parameters");

            // Optional return type annotation
            if (match({lex::tok::TokenType::ARROW}))
            {
                parseExpression();  // Parse but don't store
            }

            consume(lex::tok::TokenType::COLON, "Expected ':' after function signature");
            auto body = parseBlock();
            functionDepth_--;

            return std::make_unique<FunctionDef>(name, std::move(params), std::move(body));
        }

        throw ParseError("Expected compound statement", peek().line(), peek().column());
    }

    // Parse any statement with error recovery
    StmtPtr parseStatement()
    {
        skipNewlines();

        if (checkAny({lex::tok::TokenType::KW_IF, lex::tok::TokenType::KW_WHILE, lex::tok::TokenType::KW_FOR,
              lex::tok::TokenType::KW_FN}))
        {
            return parseCompoundStmt();
        }

        return parseSimpleStmt();
    }

    // Parse entire program with comprehensive error handling
    std::vector<StmtPtr> parse()
    {
        std::vector<StmtPtr> statements;
        std::vector<std::string> errors;

        while (!check(lex::tok::TokenType::ENDMARKER))
        {
            try
            {
                skipNewlines();
                if (check(lex::tok::TokenType::ENDMARKER))
                    break;
                statements.push_back(parseStatement());
            } catch (const ParseError& e)
            {
                errors.push_back("Line " + std::to_string(e.line_) + ": " + e.what());
                synchronize();
            }
        }

        if (!errors.empty())
        {
            std::stringstream ss;
            ss << "Parse errors:\n";
            for (const auto& err : errors)
            {
                ss << "  " << err << "\n";
            }
            throw std::runtime_error(ss.str());
        }

        return statements;
    }
};

// Synchronization tokens for error recovery
const std::unordered_set<lex::tok::TokenType> syncTokens = {lex::tok::TokenType::KW_IF, lex::tok::TokenType::KW_WHILE,
  lex::tok::TokenType::KW_FOR, lex::tok::TokenType::KW_FN, lex::tok::TokenType::KW_RETURN,
  lex::tok::TokenType::KW_CONTINUE, lex::tok::TokenType::DEDENT};

}  // parser
}  // mylang