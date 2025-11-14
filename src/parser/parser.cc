#include "../../include/parser/parser.hpp"


namespace mylang {
namespace parser {

std::u16string ParseError::format() const
{
    std::stringstream ss;
    ss << "Line " << line << ":" << column << " - " << what() << "\n";
    if (!context.empty())
    {
        ss << "  | " << utf8::utf16to8(context) << "\n";
        ss << "  | " << std::string(column - 1, ' ') << "^\n";
    }
    if (!suggestions.empty())
    {
        ss << "Suggestions:\n";
        for (const auto& s : suggestions)
        {
            ss << "  - " << utf8::utf16to8(s) << "\n";
        }
    }
    return utf8::utf8to16(ss.str());
}

// TODO : watch out for overflow and underflow
lex::tok::Token Parser::peek(std::size_t offset)
{
    if (use_lexer)
    {
        return lexer_.peek(offset);
    }
    return tokens_[current_ + 1];
}

lex::tok::Token Parser::previous()
{
    if (use_lexer)
    {
        return lexer_.prev();
    }
    return tokens_[current_ - 1];
}

lex::tok::Token Parser::advance()
{
    if (use_lexer)
    {
        return lexer_.next();
    }
    return tokens_[current_++];
}

bool Parser::check(lex::tok::TokenType type) { return peek().is(type); }

bool Parser::checkAny(std::initializer_list<lex::tok::TokenType> types)
{
    for (auto type : types)
    {
        if (check(type))
        {
            return true;
        }
    }
    return false;
}

bool Parser::match(std::initializer_list<lex::tok::TokenType> types)
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

lex::tok::Token Parser::consume(lex::tok::TokenType type, const std::u16string& msg)
{
    if (check(type))
    {
        return advance();
    }

    // Generate helpful error with context
    std::u16string context = getSourceLine(peek().line());
    std::vector<std::u16string> suggestions = getSuggestions(type);

    throw ParseError(msg, peek().line(), peek().column(), context, suggestions);
}

void Parser::skipNewlines()
{
    while (match({lex::tok::TokenType::NEWLINE}))
    {
        ;
    }
}

// Get source line for error context
std::u16string Parser::getSourceLine(int line)
{
    // This would retrieve the actual source line
    // Simplified for now
    return peek().lexeme();
}

// Generate smart suggestions based on context
std::vector<std::u16string> Parser::getSuggestions(lex::tok::TokenType expected)
{
    std::vector<std::u16string> suggs;

    if (expected == lex::tok::TokenType::COLON)
    {
        suggs.push_back(u"Did you forget ':' after the statement?");
    }
    else if (expected == lex::tok::TokenType::RPAREN)
    {
        suggs.push_back(u"Check for matching parentheses");
    }
    else if (expected == lex::tok::TokenType::NAME)
    {
        suggs.push_back(u"Expected an identifier");
        if (check(lex::tok::TokenType::NUMBER))
        {
            suggs.push_back(u"Identifiers cannot start with numbers");
        }
    }

    // Check for similar tokens
    auto current = peek().lexeme();
    auto it = errorSuggestions_.find(current);
    if (it != errorSuggestions_.end())
    {
        suggs.push_back(u"Did you mean '" + it->second + u"'?");
    }

    return suggs;
}

// Error recovery with Panic Mode
void Parser::synchronize()
{
    advance();
    while (!check(lex::tok::TokenType::ENDMARKER))
    {
        if (previous().is(lex::tok::TokenType::NEWLINE))
        {
            return;
        }
        if (previous().is(lex::tok::TokenType::SEMICOLON))
        {
            return;
        }
        if (syncTokens_.count(peek().type()))
        {
            return;
        }
        advance();
    }
}

// Record error without throwing
void Parser::recordError(const ParseError& error) { errors_.push_back({error}); }

// === Scope Management ===

void Parser::enterScope()
{
    auto scope = std::make_unique<Scope>();
    if (!scopeStack_.empty())
    {
        scope->parent = scopeStack_.back().get();
    }
    scopeStack_.push_back(std::move(scope));
}

void Parser::exitScope()
{
    if (!scopeStack_.empty())
    {
        scopeStack_.pop_back();
    }
}

void Parser::declareVariable(const std::u16string& name)
{
    if (!scopeStack_.empty())
    {
        scopeStack_.back()->variables_.insert(name);
    }
}

void Parser::declareFunction(const std::u16string& name)
{
    if (!scopeStack_.empty())
    {
        scopeStack_.back()->functions_.insert(name);
    }
}

bool Parser::isVariableDefined(const std::u16string& name) const
{
    return !scopeStack_.empty() && scopeStack_.back()->isDefined(name);
}

// === Memoization for Packrat Parsing ===

std::u16string Parser::getMemoKey(const std::u16string& rule) const
{
    return rule + u"_" + utf8::utf8to16(std::to_string(current_));
}

template<typename T>
std::optional<T> Parser::getMemo(const std::u16string& rule)
{
    auto key = getMemoKey(rule);
    auto it = memoCache_.find(key);
    if (it != memoCache_.end() && !it->second.failed_)
    {
        current_ = it->second.endPos_;
        return std::move(*reinterpret_cast<std::optional<T>*>(&it->second.result_));
    }
    return std::nullopt;
}

template<typename T>
void Parser::setMemo(const std::u16string& rule, T result)
{
    auto key = getMemoKey(rule);
    MemoEntry entry;
    entry.endPos_ = current_;
    entry.failed_ = false;
    memoCache_[key] = entry;
}

typename Parser::OpInfo Parser::getOpInfo(lex::tok::TokenType type) const
{
    switch (type)
    {
    case lex::tok::TokenType::KW_OR :
        return {1, false, false};
    case lex::tok::TokenType::KW_AND :
        return {2, false, false};
    case lex::tok::TokenType::KW_NOT :
        return {3, false, false};
    case lex::tok::TokenType::OP_EQ :
    case lex::tok::TokenType::OP_NEQ :
    case lex::tok::TokenType::OP_LT :
    case lex::tok::TokenType::OP_GT :
    case lex::tok::TokenType::OP_LTE :
    case lex::tok::TokenType::OP_GTE :
    case lex::tok::TokenType::KW_IN :
        return {4, false, true};
    case lex::tok::TokenType::OP_BITOR :
        return {5, false, false};
    case lex::tok::TokenType::OP_BITXOR :
        return {6, false, false};
    case lex::tok::TokenType::OP_BITAND :
        return {7, false, false};
    case lex::tok::TokenType::OP_LSHIFT :
    case lex::tok::TokenType::OP_RSHIFT :
        return {8, false, false};
    case lex::tok::TokenType::OP_PLUS :
    case lex::tok::TokenType::OP_MINUS :
        return {9, false, false};
    case lex::tok::TokenType::OP_STAR :
    case lex::tok::TokenType::OP_SLASH :
    case lex::tok::TokenType::OP_PERCENT :
        return {10, false, false};
    case lex::tok::TokenType::OP_POWER :
        return {11, true, false};
    default :
        return {0, false, false};
    }
}

bool Parser::isUnaryOp(lex::tok::TokenType type) const
{
    return type == lex::tok::TokenType::OP_PLUS || type == lex::tok::TokenType::OP_MINUS
      || type == lex::tok::TokenType::OP_BITNOT || type == lex::tok::TokenType::KW_NOT;
}

bool Parser::isBinaryOp(lex::tok::TokenType type) const { return getOpInfo(type).precedence > 0; }

bool Parser::isAugmentedAssign(lex::tok::TokenType type) const
{
    return type == lex::tok::TokenType::OP_PLUSEQ || type == lex::tok::TokenType::OP_MINUSEQ
      || type == lex::tok::TokenType::OP_STAREQ || type == lex::tok::TokenType::OP_SLASHEQ
      || type == lex::tok::TokenType::OP_PERCENTEQ || type == lex::tok::TokenType::OP_ANDEQ
      || type == lex::tok::TokenType::OP_OREQ || type == lex::tok::TokenType::OP_XOREQ
      || type == lex::tok::TokenType::OP_LSHIFTEQ || type == lex::tok::TokenType::OP_RSHIFTEQ;
}

bool Parser::isExpressionStart()
{
    return checkAny({lex::tok::TokenType::NUMBER, lex::tok::TokenType::STRING, lex::tok::TokenType::NAME,
      lex::tok::TokenType::KW_TRUE, lex::tok::TokenType::KW_FALSE, lex::tok::TokenType::KW_NONE,
      lex::tok::TokenType::LPAREN, lex::tok::TokenType::LBRACKET, lex::tok::TokenType::LBRACE,
      lex::tok::TokenType::OP_PLUS, lex::tok::TokenType::OP_MINUS, lex::tok::TokenType::OP_BITNOT,
      lex::tok::TokenType::KW_NOT});
}

// Parse primary with speculative parsing for ambiguity
ast::ExprPtr Parser::parsePrimary()
{
    // Try memoized result first
    if (auto cached = getMemo<ast::ExprPtr>(u"primary"))
    {
        return std::move(*cached);
    }

    size_t startPos = current_;
    ast::ExprPtr result;

    try
    {
        // Literals
        if (match({lex::tok::TokenType::NUMBER}))
        {
            auto tok = lexer_.next();
            result = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, tok.lexeme());
        }
        else if (match({lex::tok::TokenType::STRING}))
        {
            auto tok = lexer_.next();
            result = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::STRING, tok.lexeme());
        }
        else if (match({lex::tok::TokenType::KW_TRUE}))
        {
            result = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::BOOLEAN, u"true");
        }
        else if (match({lex::tok::TokenType::KW_FALSE}))
        {
            result = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::BOOLEAN, u"false");
        }
        else if (match({lex::tok::TokenType::KW_NONE}))
        {
            result = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::NONE, u"none");
        }
        // Names with postfix operations
        else if (match({lex::tok::TokenType::NAME}))
        {
            auto tok = lexer_.next();
            auto name = tok.lexeme();

            // Semantic check: undefined variable warning (non-fatal)
            if (!isVariableDefined(name))
            {
                // Warning: possibly undefined variable
            }

            result = std::make_unique<ast::NameExpr>(name);

            // Chain postfix operations
            result = parsePostfixOps(std::move(result));
        }
        // Parenthesized expression or tuple
        else if (match({lex::tok::TokenType::LPAREN}))
        {
            result = parseParenthesizedExpr();
        }
        // List literal
        else if (match({lex::tok::TokenType::LBRACKET}))
        {
            result = parseListLiteral();
        }
        // Dictionary literal
        else if (match({lex::tok::TokenType::LBRACE}))
        {
            result = parseDictLiteral();
        }
        // Lambda expression (if grammar supports)
        else if (check(lex::tok::TokenType::NAME) && peek().lexeme() == u"lambda")
        {
            result = parseLambda();
        }
        else
        {
            throw ParseError(u"Expected expression, got '" + peek().lexeme() + u"'", peek().line(), peek().column(),
              getSourceLine(peek().line()));
        }

        setMemo(u"primary", &result);
        return result;

    } catch (const ParseError& e)
    {
        current_ = startPos;  // Backtrack on error
        throw;
    }
}

// Parse postfix operations (calls, subscripts, attributes)
ast::ExprPtr Parser::parsePostfixOps(ast::ExprPtr expr)
{
    while (true)
    {
        // Function call
        if (match({lex::tok::TokenType::LPAREN}))
        {
            std::vector<ast::ExprPtr> args;
            std::unordered_map<std::u16string, ast::ExprPtr> kwargs;

            if (!check(lex::tok::TokenType::RPAREN))
            {
                // Parse positional and keyword arguments
                do
                {
                    skipNewlines();

                    // Check for keyword argument
                    if (check(lex::tok::TokenType::NAME) && peek(1).is(lex::tok::TokenType::OP_ASSIGN))
                    {
                        std::u16string key = advance().lexeme();
                        advance();  // '='
                        kwargs[key] = parseExpression();
                    }
                    else if (match({lex::tok::TokenType::OP_STAR}))
                    {
                        // *args unpacking
                        args.push_back(parseExpression());
                    }
                    else if (match({lex::tok::TokenType::OP_POWER}))
                    {
                        // **kwargs unpacking
                        auto kwexpr = parseExpression();
                    }
                    else
                    {
                        args.push_back(parseExpression());
                    }
                    skipNewlines();
                } while (match({lex::tok::TokenType::COMMA}));
            }
            consume(lex::tok::TokenType::RPAREN, u"Expected ')' after arguments");
            expr = std::make_unique<ast::CallExpr>(std::move(expr), std::move(args));
        }
        // Subscript access
        else if (match({lex::tok::TokenType::LBRACKET}))
        {
            auto index = parseExpression();

            // Handle slicing: [start:stop:step]
            if (match({lex::tok::TokenType::COLON}))
            {
                auto stop = isExpressionStart() ? parseExpression() : nullptr;
                ast::ExprPtr step = nullptr;
                if (match({lex::tok::TokenType::COLON}))
                {
                    step = isExpressionStart() ? parseExpression() : nullptr;
                }
                // Create slice expression
            }

            consume(lex::tok::TokenType::RBRACKET, u"Expected ']' after subscript");
            // expr = subscript node
        }
        // Attribute access
        else if (match({lex::tok::TokenType::DOT}))
        {
            std::u16string attr = consume(lex::tok::TokenType::NAME, u"Expected attribute name after '.'").lexeme();
            // expr = attribute node
        }
        else
        {
            break;
        }
    }
    return expr;
}

// Parse parenthesized expression or tuple
ast::ExprPtr Parser::parseParenthesizedExpr()
{
    if (check(lex::tok::TokenType::RPAREN))
    {
        advance();
        return std::make_unique<ast::ListExpr>(std::vector<ast::ExprPtr>());
    }

    auto expr = parseExpression();

    // Check for tuple
    if (match({lex::tok::TokenType::COMMA}))
    {
        std::vector<ast::ExprPtr> elements;
        elements.push_back(std::move(expr));

        if (!check(lex::tok::TokenType::RPAREN))
        {
            do
            {
                skipNewlines();
                if (check(lex::tok::TokenType::RPAREN))
                {
                    break;
                }
                elements.push_back(parseExpression());
            } while (match({lex::tok::TokenType::COMMA}));
        }

        consume(lex::tok::TokenType::RPAREN, u"Expected ')' after tuple elements");
        return std::make_unique<ast::ListExpr>(std::move(elements));
    }

    consume(lex::tok::TokenType::RPAREN, u"Expected ')' after expression");
    return expr;
}

// Parse list literal with comprehensions
ast::ExprPtr Parser::parseListLiteral()
{
    std::vector<ast::ExprPtr> elements;

    if (!check(lex::tok::TokenType::RBRACKET))
    {
        auto firstExpr = parseExpression();

        // Check for list comprehension: [expr for x in iter]
        if (check(lex::tok::TokenType::KW_FOR))
        {
            // Parse comprehension
            consume(lex::tok::TokenType::KW_FOR, u"");
            auto target = consume(lex::tok::TokenType::NAME, u"Expected variable").lexeme();
            consume(lex::tok::TokenType::KW_IN, u"Expected 'في'");
            auto iter = parseExpression();

            // Optional conditions: if condition
            std::vector<ast::ExprPtr> conditions;
            while (match({lex::tok::TokenType::KW_IF}))
            {
                conditions.push_back(parseExpression());
            }

            consume(lex::tok::TokenType::RBRACKET, u"Expected ']'");
            // Return comprehension node
            return std::make_unique<ast::ListExpr>(std::vector<ast::ExprPtr>());
        }

        elements.push_back(std::move(firstExpr));

        while (match({lex::tok::TokenType::COMMA}))
        {
            skipNewlines();
            if (check(lex::tok::TokenType::RBRACKET))
            {
                break;
            }

            if (match({lex::tok::TokenType::OP_STAR}))
            {
                // Unpacking: [1, 2, *others]
                elements.push_back(parseExpression());
            }
            else
            {
                elements.push_back(parseExpression());
            }
            skipNewlines();
        }
    }

    consume(lex::tok::TokenType::RBRACKET, u"Expected ']' after list elements");
    return std::make_unique<ast::ListExpr>(std::move(elements));
}

// Parse dictionary literal
ast::ExprPtr Parser::parseDictLiteral()
{
    std::vector<std::pair<ast::ExprPtr, ast::ExprPtr>> pairs;

    if (!check(lex::tok::TokenType::RBRACE))
    {
        do
        {
            skipNewlines();
            if (check(lex::tok::TokenType::RBRACE))
            {
                break;
            }

            // Dictionary unpacking: {**other}
            if (match({lex::tok::TokenType::OP_POWER}))
            {
                auto expr = parseExpression();
                {
                    continue;
                }
            }

            auto key = parseExpression();
            consume(lex::tok::TokenType::COLON, u"Expected ':' after dictionary key");
            auto value = parseExpression();
            pairs.emplace_back(std::move(key), std::move(value));

            skipNewlines();
        } while (match({lex::tok::TokenType::COMMA}));
    }

    consume(lex::tok::TokenType::RBRACE, u"Expected '}' after dictionary");
    // Return dict node
    return std::make_unique<ast::ListExpr>(std::vector<ast::ExprPtr>());
}

// Parse lambda expression
ast::ExprPtr Parser::parseLambda()
{
    consume(lex::tok::TokenType::NAME, u"");  // lambda keyword

    std::vector<std::u16string> params;
    if (!check(lex::tok::TokenType::COLON))
    {
        do
        {
            params.push_back(consume(lex::tok::TokenType::NAME, u"Expected parameter").lexeme());
        } while (match({lex::tok::TokenType::COMMA}));
    }

    consume(lex::tok::TokenType::COLON, u"Expected ':' in lambda");
    auto body = parseExpression();

    // Return lambda node
    return body;
}

// Pratt parser for expressions with left recursion handling
ast::ExprPtr Parser::parseUnary()
{
    if (isUnaryOp(peek().type()))
    {
        auto op = advance();
        auto operand = parseUnary();
        return std::make_unique<ast::UnaryExpr>(op.lexeme(), std::move(operand));
    }
    return parsePrimary();
}

ast::ExprPtr Parser::parsePower()
{
    auto left = parseUnary();

    if (match({lex::tok::TokenType::OP_POWER}))
    {
        auto right = parsePower();
        return std::make_unique<ast::BinaryExpr>(std::move(left), u"**", std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseBinaryExpr(int minPrec)
{
    auto left = parsePower();

    while (isBinaryOp(peek().type()))
    {
        OpInfo opInfo = getOpInfo(peek().type());

        if (opInfo.precedence < minPrec)
        {
            break;
        }

        if (opInfo.isComparison)
        {
            break;  // Handle separately
        }

        auto op = advance();
        int nextMinPrec = opInfo.rightAssoc ? opInfo.precedence : opInfo.precedence + 1;
        auto right = parseBinaryExpr(nextMinPrec);

        left = std::make_unique<ast::BinaryExpr>(std::move(left), op.lexeme(), std::move(right));
    }

    return left;
}

// Python-style comparison chaining: a < b < c == d
ast::ExprPtr Parser::parseComparison()
{
    auto left = parseBinaryExpr(0);

    std::vector<std::pair<std::u16string, ast::ExprPtr>> chain;
    while (true)
    {
        OpInfo opInfo = getOpInfo(peek().type());
        if (!opInfo.isComparison)
        {
            break;
        }

        auto op = advance();
        auto right = parseBinaryExpr(5);
        chain.push_back({op.lexeme(), std::move(right)});
    }

    if (chain.empty())
    {
        return left;
    }

    // Build: (a < b) and (b < c) and (c == d)
    auto prevRight = std::move(chain[0].second);
    auto result = std::make_unique<ast::BinaryExpr>(std::move(left), chain[0].first,
      std::make_unique<ast::NameExpr>(u"__tmp"));  // Would need proper temp

    for (size_t i = 1; i < chain.size(); i++)
    {
        // Simplified: proper implementation needs temporary variables
        result = std::make_unique<ast::BinaryExpr>(
          std::move(result), u"و", std::make_unique<ast::LiteralExpr>(ast::LiteralExpr::Type::BOOLEAN, u"true"));
    }

    return result;
}

ast::ExprPtr Parser::parseTernary()
{
    auto expr = parseComparison();

    if (match({lex::tok::TokenType::KW_IF}))
    {
        auto condition = parseComparison();
        // Python: true_expr if cond else false_expr
        // Simplified grammar version
        return std::make_unique<ast::TernaryExpr>(std::move(condition), std::move(expr), nullptr);
    }

    return expr;
}

ast::ExprPtr Parser::parseStarredExpr()
{
    if (match({lex::tok::TokenType::OP_STAR}))
    {
        auto expr = parseAssignmentExpr();
        return expr;  // Wrap in starred node
    }
    return parseAssignmentExpr();
}

ast::ExprPtr Parser::parseExpression() { return parseStarredExpr(); }

std::vector<ast::ExprPtr> Parser::parseExpressionList()
{
    std::vector<ast::ExprPtr> exprs;
    do
    {
        skipNewlines();
        exprs.push_back(parseExpression());
        skipNewlines();
    } while (match({lex::tok::TokenType::COMMA}) && isExpressionStart());
    return exprs;
}

// === Statement Parsing ===

std::vector<ast::StmtPtr> Parser::parseBlock()
{
    std::vector<ast::StmtPtr> statements;

    if (match({lex::tok::TokenType::NEWLINE}))
    {
        if (!match({lex::tok::TokenType::INDENT}))
        {
            throw ParseError(u"Expected indented block", peek().line(), peek().column(), getSourceLine(peek().line()));
        }

        while (!check(lex::tok::TokenType::DEDENT) && !check(lex::tok::TokenType::ENDMARKER))
        {
            try
            {
                statements.push_back(parseStatement());
            } catch (const ParseError& e)
            {
                recordError(e);
                synchronize();
            }
        }

        if (!match({lex::tok::TokenType::DEDENT}))
        {
            throw ParseError(u"Expected dedent", peek().line(), peek().column(), getSourceLine(peek().line()));
        }
    }
    else
    {
        statements.push_back(parseSimpleStmt());
    }

    return statements;
}

ast::StmtPtr Parser::parseSimpleStmt()
{
    skipNewlines();

    if (match({lex::tok::TokenType::KW_RETURN}))
    {
        if (functionDepth_ == 0)
        {
            throw ParseError(u"'return' outside function", previous().line(), previous().column(),
              getSourceLine(previous().line()), {u"Move this inside a function"});
        }

        ast::ExprPtr value = nullptr;
        if (isExpressionStart())
        {
            value = parseExpression();
        }
        skipNewlines();
        return std::make_unique<ast::ReturnStmt>(std::move(value));
    }

    // Complex assignment handling
    if (check(lex::tok::TokenType::NAME))
    {
        size_t savePos = current_;
        std::u16string name = peek().lexeme();

        // Type annotation
        if (peek(1).is(lex::tok::TokenType::COLON))
        {
            advance();
            advance();
            auto typeExpr = parseExpression();

            ast::ExprPtr value = nullptr;
            if (match({lex::tok::TokenType::OP_ASSIGN}))
            {
                value = parseExpression();
                declareVariable(name);
            }
            skipNewlines();
            return std::make_unique<ast::AssignmentStmt>(name, std::move(value));
        }

        // Augmented assignment
        if (isAugmentedAssign(peek(1).type()))
        {
            advance();
            auto op = advance();
            auto value = parseExpression();
            skipNewlines();

            if (!isVariableDefined(name))
            {
                recordError(ParseError(u"Variable '" + name + u"' used before assignment", previous().line(),
                  previous().column(), getSourceLine(previous().line()), {u"Initialize the variable first"}));
            }

            return std::make_unique<ast::AssignmentStmt>(name, std::move(value));
        }

        // Regular assignment
        if (peek(1).is(lex::tok::TokenType::OP_ASSIGN))
        {
            advance();
            advance();
            auto value = parseExpression();
            declareVariable(name);
            skipNewlines();
            return std::make_unique<ast::AssignmentStmt>(name, std::move(value));
        }

        current_ = savePos;
    }

    auto expr = parseExpression();
    skipNewlines();
    return std::make_unique<ast::ExprStmt>(std::move(expr));
}

ast::StmtPtr Parser::parseCompoundStmt()
{
    if (match({lex::tok::TokenType::KW_IF}))
    {
        auto condition = parseExpression();
        consume(lex::tok::TokenType::COLON, u"Expected ':' after if condition");
        auto thenBlock = parseBlock();

        std::vector<ast::StmtPtr> elseBlock;

        return std::make_unique<ast::IfStmt>(std::move(condition), std::move(thenBlock), std::move(elseBlock));
    }

    if (match({lex::tok::TokenType::KW_WHILE}))
    {
        loopDepth_++;
        auto condition = parseExpression();
        consume(lex::tok::TokenType::COLON, u"Expected ':' after while condition");
        auto body = parseBlock();
        loopDepth_--;
        return std::make_unique<ast::WhileStmt>(std::move(condition), std::move(body));
    }

    if (match({lex::tok::TokenType::KW_FOR}))
    {
        loopDepth_++;
        std::u16string target = consume(lex::tok::TokenType::NAME, u"Expected variable name after 'لكل'").lexeme();
        consume(lex::tok::TokenType::KW_IN, u"Expected 'في' in for loop");
        auto iter = parseExpression();
        consume(lex::tok::TokenType::COLON, u"Expected ':' after for clause");

        enterScope();
        declareVariable(target);
        auto body = parseBlock();
        exitScope();

        loopDepth_--;
        return std::make_unique<ast::ForStmt>(target, std::move(iter), std::move(body));
    }

    if (match({lex::tok::TokenType::KW_FN}))
    {
        functionDepth_++;
        std::u16string name = consume(lex::tok::TokenType::NAME, u"Expected function name").lexeme();

        declareFunction(name);
        consume(lex::tok::TokenType::LPAREN, u"Expected '(' after function name");

        enterScope();
        std::vector<std::u16string> params;
        if (!check(lex::tok::TokenType::RPAREN))
        {
            do
            {
                skipNewlines();

                // Handle default parameters: param=value
                std::u16string paramName = consume(lex::tok::TokenType::NAME, u"Expected parameter name").lexeme();

                if (match({lex::tok::TokenType::OP_ASSIGN}))
                {
                    auto defaultVal = parseExpression();
                    // Store default value
                }

                params.push_back(paramName);
                declareVariable(paramName);
                skipNewlines();
            } while (match({lex::tok::TokenType::COMMA}));
        }
        consume(lex::tok::TokenType::RPAREN, u"Expected ')' after parameters");

        if (match({lex::tok::TokenType::ARROW}))
        {
            parseExpression();
        }

        consume(lex::tok::TokenType::COLON, u"Expected ':' after function signature");
        auto body = parseBlock();
        exitScope();

        functionDepth_--;

        return std::make_unique<ast::FunctionDef>(name, std::move(params), std::move(body));
    }

    throw ParseError(u"Expected compound statement", peek().line(), peek().column(), getSourceLine(peek().line()));
}

ast::StmtPtr Parser::parseStatement()
{
    skipNewlines();

    if (checkAny({lex::tok::TokenType::KW_IF, lex::tok::TokenType::KW_WHILE, lex::tok::TokenType::KW_FOR,
          lex::tok::TokenType::KW_FN}))
    {
        return parseCompoundStmt();
    }

    return parseSimpleStmt();
}

// === Main Parse Method with Advanced Error Recovery ===

std::vector<ast::StmtPtr> Parser::parse()
{
    std::vector<ast::StmtPtr> statements;

    while (!check(lex::tok::TokenType::ENDMARKER))
    {
        try
        {
            skipNewlines();
            if (check(lex::tok::TokenType::ENDMARKER))
            {
                break;
            }
            statements.push_back(parseStatement());
        } catch (const ParseError& e)
        {
            recordError(e);
            synchronize();
        }
    }

    // Report all accumulated errors
    if (!errors_.empty())
    {
        std::stringstream ss;
        ss << "Parse completed with " << errors_.size() << " error(s):\n\n";
        for (const auto& errInfo : errors_)
        {
            ss << utf8::utf16to8(errInfo.error_.format()) << "\n";
        }
        throw std::runtime_error(ss.str());
    }

    return statements;
}

void Parser::clearCache() { memoCache_.clear(); }

}
}