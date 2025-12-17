#include "../../include/parser/parser_.hpp"


namespace mylang {
namespace parser {

// constructors
Parser::Parser(input::FileManager* file_manager) :
    lex_(file_manager)
{
    // ...
}

ast::ExprPtr Parser::parseUnary()
{
    auto type = peek();

    if (isUnaryOp(type))
    {
        auto op = advance();
        auto self = parseUnary();
        return std::make_unique<ast::UnaryExpr>(std::move(self), op.lexeme());
    }

    return parsePrimary();
}

ast::ExprPtr Parser::parsePrimary()
{
    ast::ExprPtr ret;

    // number literal
    if (match(lex::tok::TokenType::NUMBER))
    {
        auto tok = advance();
        ret = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::NUMBER, tok.lexeme()));
    }
    // string literal
    else if (match(lex::tok::TokenType::STRING))
    {
        auto tok = advance();
        ret = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::STRING, tok.lexeme()));
    }
    // keyword true
    else if (match(lex::tok::TokenType::KW_TRUE))
    {
        auto tok = advance();
        ret = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::BOOLEAN, tok.lexeme()));
    }
    // keyword false
    else if (match(lex::tok::TokenType::KW_FALSE))
    {
        auto tok = advance();
        ret = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::BOOLEAN, tok.lexeme()));
    }
    // keyword none
    else if (match(lex::tok::TokenType::KW_NONE))
    {
        auto tok = advance();
        ret = std::make_unique<ast::LiteralExpr>(ast::LiteralExpr(ast::LiteralExpr::Type::NONE, tok.lexeme()));
    }
    // parenthesized expression
    /// @note parsing tuples isn't implemented yet
    else if (match(lex::tok::TokenType::LPAREN))
    {
        ret = parseParenthesizedExpr();
    }
    // list
    else if (match(lex::tok::TokenType::LPAREN))
    {
        ret = parseListLiteral();
    }
    // ... 'dict'
    else
    {
        throw ParseError(u"Expected expression, got '" + peek().lexeme() + u"'", peek().line(), peek().column(),
          getSourceLine(peek().line()));
    }

    // TODO : add caching
    return ret;
}

ast::ExprPtr Parser::parseParenthesizedExpr()
{
    if (check(lex::tok::TokenType::RPAREN))
    {
        advance();
        return std::make_unique<ast::ListExpr>(std::vector<ast::ExprPtr>());
    }

    ast::ExprPtr expr = parseExpression();

    // check for tuple
    // ...

    consume(lex::tok::TokenType::RPAREN, u"Expected ')' after expression");
    return expr;
}

ast::ExprPtr Parser::parseExpression()
{
    // TODO : starred
    // ...
    return parseAssignmentExpr();
}

ast::ExprPtr Parser::parseAssignmentExpr()
{
    // TODO
    // ...
    return ast::ExprPtr();
}

ast::ExprPtr Parser::parseListLiteral()
{
    std::vector<ast::ExprPtr> elements;
    if (!check(lex::tok::TokenType::RBRACKET))
    {
        // ... 'list comprehension'

        while (match(lex::tok::TokenType::COMMA))
        {
            skipNewlines();
            if (check(lex::tok::TokenType::RBRACKET))
                break;

            // ... 'unpacking'

            elements.push_back(parseExpression());
            skipNewlines();
        }
    }

    // TODO : hell find a better way to report errors
    consume(lex::tok::TokenType::RBRACKET, u"Expected ']' after list elements");
    return std::make_unique<ast::ListExpr>(std::move(elements));
}

// helpers
bool Parser::isUnaryOp(lex::tok::Token tok) const
{
    return tok.type() == lex::tok::TokenType::OP_MINUS || tok.type() == lex::tok::TokenType::OP_BITNOT;
}

lex::tok::Token Parser::peek(std::size_t offset)
{
    if (!offset)
        return lex_.current();
    return lex_.peek(offset);
}

lex::tok::Token Parser::advance() { return lex_.next(); }

bool Parser::match(const lex::tok::TokenType type)
{
    if (check(type))
    {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(lex::tok::TokenType type) { return peek().is(type); }

lex::tok::Token Parser::consume(lex::tok::TokenType type, const std::u16string& msg)
{
    if (check(type))
        return advance();
    // TODO  : error ...
    // throw ParseError(msg, peek().line(), peek().column(), context, suggestions);
    // Placeholder to suppress warnings
    return lex::tok::Token();
}

void Parser::skipNewlines()
{
    while (match(lex::tok::TokenType::NEWLINE))
        ;
}

std::u16string Parser::getSourceLine(std::size_t line)
{
    // this would retrieve the actual source line
    // simplified for now
    // TODO : use the file manager
    return peek().lexeme();
}

}
}