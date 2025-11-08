#pragma once

#include "lex/lexer.h"
#include "lex/token.h"
#include "parser/ast.h"


namespace mylang {
namespace parser {


class Parser
{
   private:
    lex::Lexer lexer_;
    lex::tok::Token current_token_;
    bool had_error_;

   public:
    explicit Parser(const std::string& filename) :
        lexer_(filename),
        current_token_(),
        had_error_(false)
    {
        advance();  // load tokenizer
    }

    std::unique_ptr<ast::ASTNode> parse() { return parse_program(); }

    bool has_errors() const { return had_error_; }

   private:
    void advance() { current_token_ = lexer_.next(); }

    lex::tok::Token peek() { return lexer_.peek(); }

    const lex::tok::Token& current() const { return current_token_; }

    bool done() const { return current_token_.type() == lex::tok::TokenType::END_OF_FILE; }

    bool isKind(lex::tok::TokenType type) const
    {
        if (done())
            return false;
        return current_token_.type() == type;
    }

    bool match(lex::tok::TokenType type)
    {
        if (isKind(type))
        {
            advance();
            return true;
        }
        return false;
    }

    template<typename... Args>
    bool match(lex::tok::TokenType first, Args... rest)
    {
        if (match(first))
            return true;
        return match(rest...);
    }

    lex::tok::Token consume(lex::tok::TokenType type, const std::string& error_msg)
    {
        if (isKind(type))
        {
            lex::tok::Token tok = current_token_;
            advance();
            return tok;
        }
        // report_error(error_msg);
        return current_token_;
    }

    std::unique_ptr<ast::ASTNode> parse_program();
    std::unique_ptr<ast::ASTNode> parse_statement();
    std::unique_ptr<ast::ASTNode> parse_expression();
    std::unique_ptr<ast::ASTNode> parse_assignment();
    std::unique_ptr<ast::ASTNode> parse_term();
    std::unique_ptr<ast::ASTNode> parse_factor();
    std::unique_ptr<ast::ASTNode> parse_primary();
    std::unique_ptr<ast::ASTNode> parse_block();

    void sync();
};
}
}
