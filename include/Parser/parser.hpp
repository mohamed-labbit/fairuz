//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_PARSER_PARSER_HPP
#define FAIRUZ_PARSER_PARSER_HPP

#include "Diag/diagnostic.hpp"
#include "Macros.hpp"
#include "Parser/ast_builder.hpp"
#include "Parser/lexer.hpp"
#include "Support/error.hpp"
#include "Support/string.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>

namespace fairuz::parser {

// Compatibility exception. The parser reports syntax errors through ErrorOr.
// StringRef is Fairuz's arena-backed string, not std::string_view. Its storage,
// like token and AST storage, must outlive every reference to this object.
class ParseError : public std::runtime_error {
public:
    u32 m_line;
    u32 m_column;
    StringRef m_context;
    Array<StringRef> m_suggestions;

    ParseError(
        StringRef const& message, u32 line, u32 column, StringRef context = "", Array<StringRef> suggestions = { });
    StringRef format() const;
};

enum class BodyParsing { LAZY, EAGER };

// Parser contract
// ---------------
// 1. current_token() is non-null after construction. ENDMARKER is a stable
//    sentinel: consuming it cannot move the cursor. Replay streams contain
//    exactly one ENDMARKER, at the end, and no null token pointers.
// 2. A production starts at its first unconsumed token. On success it leaves
//    the first token outside that production. Expressions leave separators;
//    delimited productions consume their closer; suites consume their DEDENT.
// 3. A simple statement ends at NEWLINE, DEDENT, or ENDMARKER. It checks but
//    does not consume that terminator. Only sequence/suite owners skip lines.
// 4. Failure is not a transaction on the cursor or arena. Recovery belongs
//    to statement sequences and never consumes an enclosing suite's DEDENT.
//    Every sequence iteration advances, exits, or transfers that DEDENT to
//    its owner. Depth guards are restored on every return/exception path.
// 5. Successful AST pointers and required children are non-null. Optional
//    return values, else branches, and class parents may be null. Blocks may
//    contain successfully recovered statements: any recorded error invalidates
//    the program for execution. Check diagnostics even after a value result.
// 6. Assignment is admitted only at statement level. Plain assignment chains
//    associate right. Other infix operators associate left; power associates
//    right and admits unary operators on its right operand.
// 7. Lazy bodies retain tokens and their source snapshot, ending in a fresh
//    sentinel. Capturing a body establishes its boundary, not its grammatical
//    validity. Methods remain eager so that their fields can be discovered.
// 8. The arena outlives this parser, its token streams, and its ASTs. No parser
//    failure resets the shared arena. Allocation/lexer exceptions propagate;
//    after such an exception the caller must abandon the parser instance.
//    syntactic depth exhaustion is a recoverable ErrorOr error.
// 9. This object is not reentrant or thread-safe. Fairuz's diagnostic engine
//    and allocator are shared. Parser diagnostics use this stream's source.
class Parser {
public:
    // An empty parser is a valid, already-complete bounded stream.
    Parser();
    explicit Parser(lex::FileManager*, BodyParsing = BodyParsing::LAZY);
    explicit Parser(Array<TokenPtr> const&, diagnostic::SourcePtr, BodyParsing = BodyParsing::LAZY);

    Parser(Parser const&) = delete;
    Parser& operator=(Parser const&) = delete;
    Parser(Parser&&) = delete;
    Parser& operator=(Parser&&) = delete;

    Array<AST::StmtPtr> parse_program();
    ErrorOr<AST::StmtPtr> parse_statement();
    ErrorOr<AST::StmtPtr> parse_expression_stmt();
    ErrorOr<AST::StmtPtr> parse_if_stmt(u16 clause_column = 0);
    ErrorOr<AST::StmtPtr> parse_while_stmt();
    ErrorOr<AST::StmtPtr> parse_for_stmt();
    ErrorOr<AST::StmtPtr> parse_return_stmt();
    ErrorOr<AST::StmtPtr> parse_break_stmt();
    ErrorOr<AST::StmtPtr> parse_continue_stmt();
    ErrorOr<AST::StmtPtr> parse_function_def();
    ErrorOr<AST::StmtPtr> parse_function_body();
    ErrorOr<AST::StmtPtr> parse_block(Array<AST::ExprPtr>* members = nullptr);
    ErrorOr<AST::StmtPtr> parse_indented_block(Array<AST::ExprPtr>* members = nullptr);
    ErrorOr<AST::StmtPtr> parse_class_def();
    ErrorOr<AST::StmtPtr> parse_import_stmt();
    ErrorOr<AST::StmtPtr> parse_assert_stmt();
    ErrorOr<AST::StmtPtr> parse_class_method(Array<AST::ExprPtr>& members);
    ErrorOr<AST::StmtPtr> parse_class_method_statement(Array<AST::ExprPtr>& members);

    ErrorOr<AST::ExprPtr> parse_expression();
    ErrorOr<AST::ExprPtr> parse_assignment_expr(bool allow_augmented = true);
    ErrorOr<AST::ExprPtr> parse_binary_expr_precedence(u32 min_precedence);
    ErrorOr<AST::ExprPtr> parse_unary_expr();
    ErrorOr<AST::ExprPtr> parse_power_expr();
    ErrorOr<AST::ExprPtr> parse_postfix_expr();
    ErrorOr<AST::ExprPtr> parse_primary_expr();
    ErrorOr<Array<AST::ExprPtr>> parse_parameters_list();

    // Compatibility entry points. parse() permits statement-level assignment;
    // the expression aliases do not. The literal entry points below begin
    // AFTER '['/'{'; primary parsing additionally supplies the opener's location.
    ErrorOr<AST::ExprPtr> parse();
    ErrorOr<AST::ExprPtr> parse_list_literal();
    ErrorOr<AST::ExprPtr> parse_dict_literal();
    ErrorOr<AST::ExprPtr> parse_conditional_expr();
    ErrorOr<AST::ExprPtr> parse_logical_expr();
    ErrorOr<AST::ExprPtr> parse_logical_expr_precedence(u32 min_precedence);
    ErrorOr<AST::ExprPtr> parse_comparison_expr();
    ErrorOr<AST::ExprPtr> parse_binary_expr();
    ErrorOr<AST::ExprPtr> parse_member_access();

    bool we_done() const;
    bool check(tok::TokenType) const;
    TokenPtr current_token() const;
    SourceLocation current_loc() const;

private:
    lex::Lexer m_lexer;
    AST::ASTBuilder m_builder;
    BodyParsing m_body_parsing { BodyParsing::LAZY };
    size_t m_consumed { 0 };
    size_t m_indent_depth { 0 };
    std::optional<size_t> m_recovery_boundary;
    u32 m_depth { 0 };

    // Counts active unary/power/expression nesting and active suites, not all
    // helper calls. Standalone expressions permit 255 nested parentheses.
    // Every recursive grammar cycle passes a guard; precedence recursion is
    // independently bounded by the finite operator table. This is not a byte
    // guarantee for arbitrarily small host stacks. AST visitors need their own
    // limits for long, iteratively constructed assignment/infix/postfix chains.
    static constexpr u32 MAX_PARSE_DEPTH = 256;

    class DepthScope {
    public:
        explicit DepthScope(Parser&) noexcept;
        ~DepthScope();
        DepthScope(DepthScope const&) = delete;
        DepthScope& operator=(DepthScope const&) = delete;
        explicit operator bool() const noexcept { return m_parser != nullptr; }

    private:
        Parser* m_parser;
    };

    enum class Sequence { PROGRAM, BLOCK, METHOD, CLASS };
    Array<AST::StmtPtr> parse_sequence(Sequence, Array<AST::ExprPtr>* = nullptr);
    void synchronize(size_t statement_indent);
    void skip_newlines();
    TokenPtr advance();
    FAIRUZ_NODISCARD bool consume(tok::TokenType);
    ErrorOr<TokenPtr> expect(tok::TokenType, ErrorCode);
    ErrorOr<bool> expect_statement_end();
    bool at_statement_end() const;
    Error fail(ErrorCode, SourceLocation);
    Error fail(ErrorCode code) { return fail(code, current_loc()); }

    // The opening delimiter has already been consumed. This helper owns the
    // matching closer and permits an empty list and one trailing comma.
    template<class T, class ReadItem>
    ErrorOr<Array<T>> parse_delimited(tok::TokenType close, ErrorCode, ReadItem&&, bool* had_comma = nullptr);

    ErrorOr<AST::ExprPtr> parse_assignment_tail(AST::ExprPtr, bool allow_augmented);
    AST::AssignExpr* make_augmented(AST::ExprPtr, AST::ExprPtr, AST::ExprKind);
    ErrorOr<AST::ExprPtr> parse_list_literal(SourceLocation opening);
    ErrorOr<AST::ExprPtr> parse_dict_literal(SourceLocation opening);
    ErrorOr<AST::StmtPtr> defer_body(SourceLocation definition);
    void push_member_once(Array<AST::ExprPtr>&, AST::IdentifierExpr const*);
    void collect_this_field_assignment(Array<AST::ExprPtr>&, AST::StmtPtr);
};

} // namespace fairuz::parser

#endif // FAIRUZ_PARSER_PARSER_HPP
