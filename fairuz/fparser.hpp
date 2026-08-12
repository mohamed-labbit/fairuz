#ifndef PARSER_HPP
#define PARSER_HPP

#include "fAST.hpp"
#include "ferror.hpp"
#include "flexer.hpp"
#include "fstring.hpp"
#include "ftable.hpp"

#include <unordered_set>

namespace fairuz::parser {

class Fa_ParseError : public std::runtime_error {
public:
    i32 m_line;
    i32 m_column;
    Fa_StringRef m_context;
    Fa_Array<Fa_StringRef> m_suggestions;

    Fa_ParseError(Fa_StringRef const& msg, unsigned int l, unsigned int c, Fa_StringRef ctx = "", Fa_Array<Fa_StringRef> sugg = { })
        : m_line(l)
        , m_column(c)
        , m_context(ctx)
        , m_suggestions(sugg)
        , std::runtime_error(msg.data())
    {
    }

    Fa_StringRef format() const
    {
        std::stringstream ss;
        ss << "Line " << m_line << ":" << m_column << " - " << what() << "\n";

        if (!m_context.empty()) {
            ss << "  | " << m_context << "\n";
            ss << "  | " << std::string(m_column - 1, ' ') << "^\n";
        }

        if (!m_suggestions.empty()) {
            ss << "Suggestions:\n";
            for (Fa_StringRef const& s : m_suggestions)
                ss << "  - " << s << "\n";
        }

        return Fa_StringRef(ss.str().data());
    }
}; // class ParseError

class Fa_Parser {
public:
    explicit Fa_Parser() = default;

    explicit Fa_Parser(lex::Fa_FileManager* fm)
        : m_lexer(fm)
    {
        if (fm == nullptr)
            diagnostic::panic(diagnostic::errc::general::Code::INTERNAL_ERROR, "parser received a null Fa_FileManager");

        m_lexer.m_next();
        if (current_token() != nullptr && current_token()->type() == tok::Fa_TokenType::BEGINMARKER)
            m_lexer.m_next();
    }

    explicit Fa_Parser(Fa_Array<tok::Fa_Token> seq, std::optional<size_t> s = std::nullopt);

    Fa_Array<AST::Fa_Stmt*> parse_program();

    Fa_ErrorOr<AST::Fa_Stmt*> parse_statement();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_expression_stmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_if_stmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_while_stmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_for_stmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_return_stmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_break_stmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_continue_stmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_function_def();
    Fa_ErrorOr<AST::Fa_Expr*> parse_expression();
    Fa_ErrorOr<AST::Fa_Expr*> parse_assignment_expr();
    Fa_ErrorOr<AST::Fa_Expr*> parse_list_literal();
    Fa_ErrorOr<AST::Fa_Expr*> parse_dict_literal();
    Fa_ErrorOr<AST::Fa_Expr*> parse_conditional_expr();
    Fa_ErrorOr<AST::Fa_Expr*> parse_logical_expr();
    Fa_ErrorOr<AST::Fa_Expr*> parse_logical_expr_precedence(unsigned int min_precedence);
    Fa_ErrorOr<AST::Fa_Expr*> parse_binary_expr_precedence(unsigned int min_precedence);
    Fa_ErrorOr<AST::Fa_Expr*> parse_comparison_expr();
    Fa_ErrorOr<AST::Fa_Expr*> parse_binary_expr();
    Fa_ErrorOr<AST::Fa_Expr*> parse_unary_expr();
    Fa_ErrorOr<AST::Fa_Expr*> parse_primary_expr();
    Fa_ErrorOr<AST::Fa_Expr*> parse_postfix_expr();
    Fa_ErrorOr<AST::Fa_Expr*> parse();
    Fa_ErrorOr<AST::Fa_Expr*> parse_parameters_list();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_indented_block();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_class_def();
    Fa_ErrorOr<AST::Fa_Stmt*> parse_class_method(Fa_Array<AST::Fa_Expr*>& members);
    Fa_ErrorOr<AST::Fa_Expr*> parse_member_access();

    bool we_done() const;

    bool check(tok::Fa_TokenType type) const;

    tok::Fa_Token const* current_token() const;

private:
    lex::Fa_Lexer m_lexer;

    tok::Fa_Token const* peek(size_t offset = 1) { return m_lexer.peek(offset); }
    tok::Fa_Token const* advance() { return m_lexer.m_next(); }

    bool match(tok::Fa_TokenType const type);

    [[nodiscard]]
    bool consume(tok::Fa_TokenType type)
    {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    Fa_Error report_error(diagnostic::errc::parser::Code err_code, diagnostic::Severity sv = diagnostic::Severity::ERROR);

    void skip_newlines()
    {
        while (match(tok::Fa_TokenType::NEWLINE))
            ;
    }

    void synchronize();
}; // class Fa_Parser

} // namespace fairuz::parser

#endif // PARSER_HPP
