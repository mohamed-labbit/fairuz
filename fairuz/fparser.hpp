#ifndef FA_PARSER_HPP
#define FA_PARSER_HPP

#include "fAST.hpp"
#include "fdiagnostic.hpp"
#include "ferror.hpp"
#include "flexer.hpp"
#include "fmacros.hpp"
#include "fstring.hpp"

namespace fairuz::parser {

class ParseError : public std::runtime_error {
public:
    i32 m_line;
    i32 m_column;
    StringRef m_context;
    Array<StringRef> m_suggestions;

    ParseError(StringRef const& msg, u32 l, u32 c, StringRef ctx = "", Array<StringRef> sugg = { })
        : std::runtime_error(msg.data())
        , m_line(l)
        , m_column(c)
        , m_context(ctx)
        , m_suggestions(sugg)
    {
    }

    StringRef format() const
    {
        std::stringstream ss;
        ss << "Line " << m_line << ":" << m_column << " - " << what() << "\n";

        if (!m_context.empty()) {
            ss << "  | " << m_context << "\n";
            ss << "  | " << std::string(m_column - 1, ' ') << "^\n";
        }

        if (!m_suggestions.empty()) {
            ss << "Suggestions:\n";
            for (StringRef const& s : m_suggestions)
                ss << "  - " << s << "\n";
        }

        return StringRef(ss.str().data());
    }
}; // class ParseError

class Parser {
public:
    explicit Parser() = default;

    explicit Parser(lex::FileManager* fm)
        : m_lexer(fm)
    {
        if (fm == nullptr)
            diagnostic::panic(ErrorCode::INTERNAL_ERROR, "parser received a null FileManager");

        m_lexer.next();
        if (current_token() != nullptr && current_token()->type() == tok::TokenType::BEGINMARKER)
            m_lexer.next();
    }

    explicit Parser(Array<tok::Token> seq, std::optional<size_t> s = std::nullopt);

    Array<AST::Stmt*> parse_program();

    ErrorOr<AST::Stmt*> parse_statement();
    ErrorOr<AST::Stmt*> parse_expression_stmt();
    ErrorOr<AST::Stmt*> parse_if_stmt();
    ErrorOr<AST::Stmt*> parse_while_stmt();
    ErrorOr<AST::Stmt*> parse_for_stmt();
    ErrorOr<AST::Stmt*> parse_return_stmt();
    ErrorOr<AST::Stmt*> parse_break_stmt();
    ErrorOr<AST::Stmt*> parse_continue_stmt();
    ErrorOr<AST::Stmt*> parse_function_def();
    ErrorOr<AST::Expr*> parse_expression();
    ErrorOr<AST::Expr*> parse_assignment_expr();
    ErrorOr<AST::Expr*> parse_list_literal();
    ErrorOr<AST::Expr*> parse_dict_literal();
    ErrorOr<AST::Expr*> parse_conditional_expr();
    ErrorOr<AST::Expr*> parse_logical_expr();
    ErrorOr<AST::Expr*> parse_logical_expr_precedence(u32 min_precedence);
    ErrorOr<AST::Expr*> parse_binary_expr_precedence(u32 min_precedence);
    ErrorOr<AST::Expr*> parse_comparison_expr();
    ErrorOr<AST::Expr*> parse_binary_expr();
    ErrorOr<AST::Expr*> parse_unary_expr();
    ErrorOr<AST::Expr*> parse_primary_expr();
    ErrorOr<AST::Expr*> parse_postfix_expr();
    ErrorOr<AST::Expr*> parse();
    ErrorOr<AST::Expr*> parse_parameters_list();
    ErrorOr<AST::Stmt*> parse_indented_block();
    ErrorOr<AST::Stmt*> parse_class_def();
    ErrorOr<AST::Stmt*> parse_import_stmt();
    ErrorOr<AST::Stmt*> parse_assert_stmt();
    ErrorOr<AST::Stmt*> parse_class_method(Array<AST::Expr*>& members);
    ErrorOr<AST::Expr*> parse_member_access();

    bool we_done() const;

    bool check(tok::TokenType type) const;

    TokenPtr current_token() const;
    SourceLocation current_loc() const { return current_token()->location(); }

private:
    lex::Lexer m_lexer;
    u32 m_nesting_level { 0 };
    /// keeping a stack of open parentheses
    /// using bool for minimal memory use
    std::vector<bool> m_parenths;

    // Each syntactic level currently traverses several mutually-recursive
    // parser helpers. Keep a bounded implementation-depth budget large
    // enough for 255 source nesting levels while still protecting the C++
    // stack from hostile input.
    static constexpr u32 MAX_NESTING_LEVEL = 1280;

    struct NestingLevel {
        u32* p { nullptr };
        NestingLevel(u32* c, SourceLocation loc)
            : p(c)
        {
            assert(p != nullptr);
            if (*p >= MAX_NESTING_LEVEL)
                diagnostic::report(diagnostic::Severity::FATAL, loc, ErrorCode::EXCEEDED_MAX_NESTING_LIMIT);
            (*p)++;
        }
        ~NestingLevel()
        {
            if (p != nullptr)
                (*p)--;
        }
    };

    TokenPtr peek(size_t offset = 1) { return m_lexer.peek(offset); }
    TokenPtr advance() { return m_lexer.next(); }

    bool match(tok::TokenType const type);

    [[nodiscard]]
    bool consume(tok::TokenType type)
    {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    void skip_newlines()
    {
        while (match(tok::TokenType::NEWLINE))
            ;
    }

    void synchronize();
}; // class Parser

} // namespace fairuz::parser

#endif // FA_PARSER_HPP
