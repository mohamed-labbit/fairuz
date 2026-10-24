//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "Parser/parser.hpp"
#include "Util/numeric_literals.hpp"
#include "parser_macros.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace fairuz::parser {
namespace {

lex::FileManager* checked_file(lex::FileManager* file)
{
    if (file == nullptr) diagnostic::panic(ErrorCode::INTERNAL_ERROR, "Parser requires a FileManager");
    return file;
}

Array<TokenPtr> const& checked_stream(Array<TokenPtr> const& tokens)
{
    if (tokens.empty()) diagnostic::panic(ErrorCode::INTERNAL_ERROR, "Parser requires a terminal ENDMARKER");
    for (size_t i = 0; i < tokens.size(); ++i) {
        auto* token = tokens[i];
        if (token == nullptr) diagnostic::panic(ErrorCode::INTERNAL_ERROR, "Parser received a null replay token");
        if (token->is(TT::ENDMARKER) != (i + 1 == tokens.size()))
            diagnostic::panic(ErrorCode::INTERNAL_ERROR, "ENDMARKER must occur exactly once, at the end");
        if (i != 0 && token->is(TT::BEGINMARKER))
            diagnostic::panic(ErrorCode::INTERNAL_ERROR, "BEGINMARKER is only valid at stream start");
    }
    return tokens;
}

} // namespace

ParseError::ParseError(StringRef const& message, u32 line, u32 column, StringRef context, Array<StringRef> suggestions)
    : std::runtime_error(std::string(message.data(), message.len()))
    , m_line(line)
    , m_column(column)
    , m_context(std::move(context))
    , m_suggestions(std::move(suggestions))
{
}

StringRef ParseError::format() const
{
    std::ostringstream out;
    out << "Line " << m_line << ':' << m_column << " - " << what() << '\n';
    if (!m_context.empty()) {
        out << "  | " << m_context << '\n' << "  | ";
        // Invalid/unknown column zero must not underflow an allocation size.
        // Bound padding by the supplied line; UTF-8 display is handled by the
        // structured diagnostic engine used by the parser itself.
        size_t padding = m_column == 0 ? 0 : size_t(m_column - 1);
        padding = std::min(padding, m_context.len());
        out << std::string(padding, ' ') << "^\n";
    }
    if (!m_suggestions.empty()) {
        out << "Suggestions:\n";
        for (auto const& suggestion : m_suggestions) out << "  - " << suggestion << '\n';
    }
    auto text = out.str();
    StringRef result(text.size(), '\0');
    if (!text.empty()) std::memcpy(result.data(), text.data(), text.size());
    return result;
}

Parser::Parser()
    : Parser(Array<TokenPtr> { make_token(TT::ENDMARKER, "", { }) }, nullptr)
{
}

Parser::Parser(lex::FileManager* file, BodyParsing bodies)
    : m_lexer(checked_file(file))
    , m_body_parsing(bodies)
{
    m_lexer.next();
    if (check(TT::BEGINMARKER)) advance();
}

Parser::Parser(Array<TokenPtr> const& tokens, diagnostic::SourcePtr source, BodyParsing bodies)
    : m_lexer(checked_stream(tokens), std::move(source))
    , m_body_parsing(bodies)
{
    if (check(TT::BEGINMARKER)) advance();
}

Parser::DepthScope::DepthScope(Parser& parser) noexcept
    : m_parser(parser.m_depth < MAX_PARSE_DEPTH ? &parser : nullptr)
{
    if (m_parser != nullptr) ++m_parser->m_depth;
}

Parser::DepthScope::~DepthScope()
{
    if (m_parser != nullptr) {
        assert(m_parser->m_depth != 0);
        --m_parser->m_depth;
    }
}

TokenPtr Parser::current_token() const
{
    auto* token = m_lexer.current();
    assert(token != nullptr);
    return token;
}

SourceLocation Parser::current_loc() const { return current_token()->location(); }
bool Parser::check(TT type) const { return current_token()->is(type); }
bool Parser::we_done() const { return check(TT::ENDMARKER); }

TokenPtr Parser::advance()
{
    if (we_done()) return current_token();
    m_recovery_boundary.reset();
    if (check(TT::INDENT)) ++m_indent_depth;
    else if (check(TT::DEDENT) && m_indent_depth != 0) --m_indent_depth;
    ++m_consumed;
    return m_lexer.next();
}

bool Parser::consume(TT type)
{
    if (!check(type)) return false;
    advance();
    return true;
}

Error Parser::fail(ErrorCode code, SourceLocation location)
{
    diagnostic::SourceScope source_scope(m_lexer.source());
    return report_error(code, location);
}

ErrorOr<TokenPtr> Parser::expect(TT type, ErrorCode error)
{
    if (!check(type)) return fail(error);
    auto* token = current_token();
    advance();
    return token;
}

bool Parser::at_statement_end() const { return check(TT::NEWLINE) || check(TT::DEDENT) || we_done(); }

ErrorOr<bool> Parser::expect_statement_end()
{
    if (!at_statement_end()) return fail(ErrorCode::UNEXPECTED_TOKEN);
    return true;
}

void Parser::skip_newlines()
{
    while (consume(TT::NEWLINE)) { }
}

// Recovery state is the indentation depth at statement entry, not the current
// token's column. A failed compound production may already have entered a
// suite. Consume that suite's remaining tokens, but leave the caller's DEDENT.
void Parser::synchronize(size_t statement_indent)
{
    // A missing suite after a newline can leave us on the next valid sibling.
    // Consume this one-shot marker even if no token is advanced, preventing a
    // repeated failure at the same cursor from disabling recovery indefinitely.
    auto boundary = std::exchange(m_recovery_boundary, std::nullopt);
    if (boundary == m_consumed && m_indent_depth <= statement_indent) return;
    while (!we_done()) {
        if (check(TT::DEDENT) && m_indent_depth <= statement_indent) return;
        if (check(TT::NEWLINE) && m_indent_depth <= statement_indent) {
            advance();
            skip_newlines();
            // A failed header may have an otherwise well-formed orphan suite.
            if (!check(TT::INDENT)) return;
            advance();
            while (!we_done() && m_indent_depth > statement_indent) advance();
            return;
        }
        bool closing_nested_suite = check(TT::DEDENT);
        advance();
        if (closing_nested_suite && m_indent_depth <= statement_indent) return;
    }
}

template<class T, class ReadItem>
ErrorOr<Array<T>> Parser::parse_delimited(TT close, ErrorCode missing_close, ReadItem&& read_item, bool* had_comma)
{
    if (had_comma != nullptr) *had_comma = false;
    Array<T> items;
    skip_newlines();
    if (consume(close)) return items;
    for (;;) {
        FA_PARSE_TRY(item, read_item());
        items.push(std::move(item));
        skip_newlines();
        if (!consume(TT::COMMA)) {
            FA_PARSE_REQUIRE(expect(close, missing_close));
            return items;
        }
        if (had_comma != nullptr) *had_comma = true;
        skip_newlines();
        if (consume(close)) return items;
    }
}

Array<AST::StmtPtr> Parser::parse_sequence(Sequence kind, Array<AST::ExprPtr>* members)
{
    assert((kind == Sequence::METHOD || kind == Sequence::CLASS) == (members != nullptr));
    Array<AST::StmtPtr> statements;
    while (!we_done() && !diagnostic::is_saturated()) {
        skip_newlines();
        if (we_done()) break;
        if (check(TT::DEDENT)) {
            if (kind != Sequence::PROGRAM) break;
            (void)fail(ErrorCode::UNEXPECTED_TOKEN);
            advance();
            continue;
        }
        size_t const before = m_consumed;
        size_t const indentation = m_indent_depth;
        auto statement = kind == Sequence::CLASS ? parse_class_method(*members)
            : kind == Sequence::METHOD           ? parse_class_method_statement(*members)
                                                 : parse_statement();
        if (statement.has_value()) {
            assert(statement.value() != nullptr);
            if (m_consumed == before)
                diagnostic::panic(ErrorCode::INTERNAL_ERROR, "Successful statement parser made no progress");
            statements.push(statement.value());
        } else if (!diagnostic::is_saturated()) {
            synchronize(indentation);
        }
    }
    return statements;
}

Array<AST::StmtPtr> Parser::parse_program()
{
    auto statements = parse_sequence(Sequence::PROGRAM);
    if (diagnostic::has_errors()) diagnostic::dump();
    return statements;
}

AST::AssignExpr* Parser::make_augmented(AST::ExprPtr target, AST::ExprPtr right, EK operation)
{
    assert(is_assignable(target) && right != nullptr);
    // Preserve Assign(target, Binary(op, target, rhs), augmented=true). Both
    // references share the arena-owned target; there is no ownership transfer
    // or recursive clone. The existing compiler captures member/index targets
    // once and reads only the binary RHS. Pointer identity is not AST semantics.
    auto* binary = m_builder.make_binary(operation, target, right, target->get_location());
    auto* assignment = m_builder.make_assignment_expr(target, binary, target->get_location());
    assignment->augmented = true;
    return assignment;
}

} // namespace fairuz::parser

#undef FA_PARSE_REQUIRE
#undef FA_PARSE_TRY
#undef FA_PARSE_JOIN
#undef FA_PARSE_JOIN_I
