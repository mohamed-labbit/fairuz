#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"
#include "error.hpp"
#include "lexer.hpp"
#include "table.hpp"

#include <unordered_set>

namespace fairuz::parser {

class Fa_SymbolTable {
public:
    enum class SymbolType {
        VARIABLE,
        FUNCTION,
        CLASS,
        MODULE,
        UNKNOWN
    }; // enum SymbolType

    enum class DataType {
        INTEGER,
        FLOAT,
        STRING,
        BOOLEAN,
        LIST,
        DICT,
        TUPLE,
        NONE,
        FUNCTION,
        ANY,
        UNKNOWN
    }; // enum DataType

    struct Symbol {
        Fa_StringRef name { "" };
        SymbolType symbol_type;
        DataType data_type;
        bool is_used { false };
        Fa_SourceLocation definition_loc;
        Fa_Array<i32> usage_lines;
        Fa_Array<DataType> param_types;
        DataType return_type = DataType::UNKNOWN;
        std::unordered_set<DataType> possible_types;
    }; // struct Symbol

    Fa_SymbolTable* m_parent = nullptr;

private:
    Fa_HashTable<Fa_StringRef, Symbol, Fa_StringRefHash, Fa_StringRefEqual> m_symbols;
    Fa_Array<Fa_SymbolTable*> m_children;
    unsigned int m_scope_level { 0 };

public:
    explicit Fa_SymbolTable(Fa_SymbolTable* p = nullptr, i32 level = 0);

    void define(Fa_StringRef const& m_name, Symbol symbol);
    Symbol* lookup(Fa_StringRef const& m_name);
    Symbol* lookup_local(Fa_StringRef const& m_name);
    bool is_defined(Fa_StringRef const& m_name) const;
    void mark_used(Fa_StringRef const& m_name, i32 m_line);
    Fa_SymbolTable* create_child();
    Fa_Array<Symbol*> get_unused_symbols();
    Fa_HashTable<Fa_StringRef, Symbol, Fa_StringRefHash, Fa_StringRefEqual> const& get_symbols() const;
}; // class Fa_SymbolTable

class Fa_SemanticAnalyzer {
public:
    struct Issue {
        enum class Severity {
            ERROR,
            WARNING,
            INFO
        }; // enum Severity

        Severity severity { Severity::ERROR };
        u16 code { 0 };
        Fa_StringRef message;
        Fa_SourceLocation loc;
        Fa_StringRef suggestion;
    }; // struct Issue

private:
    Fa_SymbolTable* m_current_scope;
    Fa_SymbolTable* m_global_scope;
    Fa_Array<Issue> m_issues;

public:
    Fa_SemanticAnalyzer();
    Fa_SymbolTable::DataType infer_type(AST::Fa_Expr* m_expr);
    void report_issue(Issue::Severity sev, diagnostic::errc::sema::Code code, Fa_StringRef msg, Fa_SourceLocation loc, Fa_StringRef const& sugg = "");
    void analyze_expr(AST::Fa_Expr* m_expr);
    void analyze_stmt(AST::Fa_Stmt* stmt);
    void analyze(Fa_Array<AST::Fa_Stmt*> const& m_statements);
    Fa_Array<Issue> const& get_issues() const;
    Fa_SymbolTable const* get_global_scope() const;
    Fa_SymbolTable const* get_current_scope() const;
    void print_report() const;
}; // class Fa_SemanticAnalyzer

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

    bool we_done() const;
    bool check(tok::Fa_TokenType m_type);

    tok::Fa_Token const* current_token() const;

private:
    lex::Fa_Lexer m_lexer;
    Fa_SemanticAnalyzer m_sema;

    tok::Fa_Token const* peek(size_t m_offset = 1) { return m_lexer.peek(m_offset); }
    tok::Fa_Token const* advance() { return m_lexer.m_next(); }

    bool match(tok::Fa_TokenType const m_type);

    [[nodiscard]]
    bool consume(tok::Fa_TokenType m_type)
    {
        if (check(m_type)) {
            advance();
            return true;
        }
        return false;
    }

    Fa_Error report_error(diagnostic::errc::parser::Code err_code);

    void skip_newlines()
    {
        while (match(tok::Fa_TokenType::NEWLINE))
            ;
    }

    void synchronize();
}; // class Fa_Parser

} // namespace fairuz::parser

#endif // PARSER_HPP
