//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "Parser/parser.hpp"
#include "Util/numeric_literals.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

// Each expression is evaluated once. These macros propagate existing errors;
// they never emit a second diagnostic for the same failure. FA_PARSE_TRY is a
// declaration and must be used as a standalone statement in a braced scope.
#define FA_PARSE_JOIN_I(a, b) a##b
#define FA_PARSE_JOIN(a, b) FA_PARSE_JOIN_I(a, b)
#define FA_PARSE_TRY(name, expression)                                                                             \
    auto FA_PARSE_JOIN(parse_result_, __LINE__) = (expression);                                                    \
    if (FA_PARSE_JOIN(parse_result_, __LINE__).has_error()) return FA_PARSE_JOIN(parse_result_, __LINE__).error(); \
    auto name = std::move(FA_PARSE_JOIN(parse_result_, __LINE__)).value()
#define FA_PARSE_REQUIRE(expression)                                                 \
    do {                                                                             \
        auto parse_required_result = (expression);                                   \
        if (parse_required_result.has_error()) return parse_required_result.error(); \
    } while (false)

namespace fairuz::parser {
namespace {

using TT = tok::TokenType;
using EK = AST::ExprKind;

// This spelling is part of the existing parser/compiler interface.
constexpr char class_instance_name[] = "__class$instance";

struct Infix {
    EK kind;
    u32 precedence;
};

// Only left-associative operators belong here. Assignment and exponentiation
// have separate productions. Precedence constants match token.hpp.
std::optional<Infix> infix_operator(TT type)
{
    switch (type) {
    case TT::OP_OR: return Infix { EK::OP_OR, tok::PREC_OR };
    case TT::OP_AND: return Infix { EK::OP_AND, tok::PREC_AND };
    case TT::OP_BITOR: return Infix { EK::OP_BITOR, tok::PREC_BITOR };
    case TT::OP_BITXOR: return Infix { EK::OP_BITXOR, tok::PREC_BITXOR };
    case TT::OP_BITAND: return Infix { EK::OP_BITAND, tok::PREC_BITAND };
    case TT::OP_EQ: return Infix { EK::OP_EQ, tok::PREC_EQ };
    case TT::OP_NEQ: return Infix { EK::OP_NEQ, tok::PREC_EQ };
    case TT::OP_LT: return Infix { EK::OP_LT, tok::PREC_CMP };
    case TT::OP_GT: return Infix { EK::OP_GT, tok::PREC_CMP };
    case TT::OP_LTE: return Infix { EK::OP_LTE, tok::PREC_CMP };
    case TT::OP_GTE: return Infix { EK::OP_GTE, tok::PREC_CMP };
    case TT::OP_LSHIFT: return Infix { EK::OP_LSHIFT, tok::PREC_SHIFT };
    case TT::OP_RSHIFT: return Infix { EK::OP_RSHIFT, tok::PREC_SHIFT };
    case TT::OP_PLUS: return Infix { EK::OP_ADD, tok::PREC_BINARY };
    case TT::OP_MINUS: return Infix { EK::OP_SUB, tok::PREC_BINARY };
    case TT::OP_STAR: return Infix { EK::OP_MUL, tok::PREC_FACTOR };
    case TT::OP_SLASH: return Infix { EK::OP_DIV, tok::PREC_FACTOR };
    case TT::OP_PERCENT: return Infix { EK::OP_MOD, tok::PREC_FACTOR };
    default: return std::nullopt;
    }
}

std::optional<EK> unary_operator(TT type)
{
    switch (type) {
    case TT::OP_PLUS: return EK::OP_PLUS;
    case TT::OP_MINUS: return EK::OP_NEG;
    case TT::OP_BITNOT: return EK::OP_BITNOT;
    case TT::OP_NOT: return EK::OP_NOT;
    default: return std::nullopt;
    }
}

std::optional<EK> augmented_operator(TT type)
{
    switch (type) {
    case TT::OP_PLUSEQ: return EK::OP_ADD;
    case TT::OP_MINUSEQ: return EK::OP_SUB;
    case TT::OP_STAREQ: return EK::OP_MUL;
    case TT::OP_SLASHEQ: return EK::OP_DIV;
    case TT::OP_PERCENTEQ: return EK::OP_MOD;
    case TT::OP_ANDEQ: return EK::OP_BITAND;
    case TT::OP_OREQ: return EK::OP_BITOR;
    case TT::OP_XOREQ: return EK::OP_BITXOR;
    case TT::OP_LSHIFTEQ: return EK::OP_LSHIFT;
    case TT::OP_RSHIFTEQ: return EK::OP_RSHIFT;
    default: return std::nullopt;
    }
}

bool is_assignment(TT type) { return type == TT::OP_ASSIGN || augmented_operator(type).has_value(); }

bool is_assignable(AST::ExprPtr expr)
{
    assert(expr != nullptr);
    return AST::is_identifier(expr) || AST::is_index(expr) || AST::is_get(expr);
}

bool same_name(AST::ExprPtr expr, StringRef const& name)
{
    return expr != nullptr && AST::is_identifier(expr) && AST::as_identifier(expr)->spelling == name;
}

bool starts_compound(TT type) { return type == TT::KW_IF || type == TT::KW_WHILE || type == TT::KW_FOR || type == TT::KW_FN || type == TT::KW_CLASS; }

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
        if (i != 0 && token->is(TT::BEGINMARKER)) diagnostic::panic(ErrorCode::INTERNAL_ERROR, "BEGINMARKER is only valid at stream start");
    }
    return tokens;
}

// Distinguish overflow from invalid spelling, including invalid bytes AFTER an
// overflow. The utility's bool result cannot make that distinction. This also
// keeps malformed replay tokens on the parser's recoverable error path.
enum class IntegerStatus {
    INVALID,
    SMALL,
    LARGE,
};

IntegerStatus integer_literal(StringRef const& spelling, unsigned base, i64& result)
{
    std::string_view text(spelling.data(), spelling.len());
    if (text.empty()) return IntegerStatus::INVALID;
    bool negative = text.front() == '-';
    size_t i = negative ? 1 : 0;
    if (i + 1 < text.size() && text[i] == '0') {
        char prefix = text[i + 1];
        unsigned prefix_base = (prefix == 'x' || prefix == 'X') ? 16
            : (prefix == 'o' || prefix == 'O')                  ? 8
            : (prefix == 'b' || prefix == 'B')                  ? 2
                                                                : 0;
        if (prefix_base != 0) {
            if (prefix_base != base) return IntegerStatus::INVALID;
            i += 2;
        }
    }
    u64 magnitude = 0;
    u64 const limit = negative ? (u64 { 1 } << 63) : u64(std::numeric_limits<i64>::max());
    bool overflow = false;
    bool saw_digit = false;
    while (i < text.size()) {
        unsigned char byte = static_cast<unsigned char>(text[i++]);
        if (byte == '_' || byte == '\'') continue;
        unsigned digit;
        if (byte >= '0' && byte <= '9') digit = byte - '0';
        else if (byte >= 'a' && byte <= 'f') digit = byte - 'a' + 10;
        else if (byte >= 'A' && byte <= 'F') digit = byte - 'A' + 10;
        else if (byte == 0xD9 && i < text.size() && static_cast<unsigned char>(text[i]) >= 0xA0 && static_cast<unsigned char>(text[i]) <= 0xA9)
            digit = static_cast<unsigned char>(text[i++]) - 0xA0;
        else return IntegerStatus::INVALID;
        if (digit >= base) return IntegerStatus::INVALID;
        saw_digit = true;
        if (!overflow) {
            if (magnitude > (limit - digit) / base) overflow = true;
            else magnitude = magnitude * base + digit;
        }
    }
    if (!saw_digit) return IntegerStatus::INVALID;
    if (overflow) return IntegerStatus::LARGE;
    result
        = negative ? (magnitude == (u64 { 1 } << 63) ? std::numeric_limits<i64>::min() : -static_cast<i64>(magnitude)) : static_cast<i64>(magnitude);
    return IntegerStatus::SMALL;
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

Array<AST::StmtPtr> Parser::parse_sequence(Sequence kind, Array<AST::ExprPtr>* members)
{
    assert((kind == Sequence::Method || kind == Sequence::Class) == (members != nullptr));
    Array<AST::StmtPtr> statements;
    while (!we_done() && !diagnostic::is_saturated()) {
        skip_newlines();
        if (we_done()) break;
        if (check(TT::DEDENT)) {
            if (kind != Sequence::Program) break;
            (void)fail(ErrorCode::UNEXPECTED_TOKEN);
            advance();
            continue;
        }
        size_t const before = m_consumed;
        size_t const indentation = m_indent_depth;
        auto statement = kind == Sequence::Class ? parse_class_method(*members)
            : kind == Sequence::Method           ? parse_class_method_statement(*members)
                                                 : parse_statement();
        if (statement.has_value()) {
            assert(statement.value() != nullptr);
            if (m_consumed == before) diagnostic::panic(ErrorCode::INTERNAL_ERROR, "Successful statement parser made no progress");
            statements.push(statement.value());
        } else if (!diagnostic::is_saturated()) {
            synchronize(indentation);
        }
    }
    return statements;
}

Array<AST::StmtPtr> Parser::parse_program()
{
    auto statements = parse_sequence(Sequence::Program);
    if (diagnostic::has_errors()) diagnostic::dump();
    return statements;
}

ErrorOr<AST::StmtPtr> Parser::parse_statement()
{
    skip_newlines();
    switch (current_token()->type()) {
    case TT::KW_IF: return parse_if_stmt();
    case TT::KW_WHILE: return parse_while_stmt();
    case TT::KW_FOR: return parse_for_stmt();
    case TT::KW_RETURN: return parse_return_stmt();
    case TT::KW_BREAK: return parse_break_stmt();
    case TT::KW_CONTINUE: return parse_continue_stmt();
    case TT::KW_FN: return parse_function_def();
    case TT::KW_CLASS: return parse_class_def();
    case TT::KW_IMPORT:
    case TT::KW_FROM: return parse_import_stmt();
    case TT::KW_ASSERT: return parse_assert_stmt();
    default: return parse_expression_stmt();
    }
}

ErrorOr<AST::StmtPtr> Parser::parse_expression_stmt()
{
    FA_PARSE_TRY(expression, parse_assignment_expr());
    FA_PARSE_REQUIRE(expect_statement_end());
    return m_builder.make_expr_stmt(expression, expression->get_location());
}

ErrorOr<AST::StmtPtr> Parser::parse_return_stmt()
{
    FA_PARSE_TRY(start, expect(TT::KW_RETURN, ErrorCode::EXPECTED_RETURN));
    AST::ExprPtr value = nullptr;
    if (!at_statement_end()) {
        FA_PARSE_TRY(expression, parse_expression());
        value = expression;
    }
    FA_PARSE_REQUIRE(expect_statement_end());
    return m_builder.make_return(start->location(), value);
}

ErrorOr<AST::StmtPtr> Parser::parse_break_stmt()
{
    FA_PARSE_TRY(start, expect(TT::KW_BREAK, ErrorCode::UNEXPECTED_TOKEN));
    FA_PARSE_REQUIRE(expect_statement_end());
    return m_builder.make_break(start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_continue_stmt()
{
    FA_PARSE_TRY(start, expect(TT::KW_CONTINUE, ErrorCode::UNEXPECTED_TOKEN));
    FA_PARSE_REQUIRE(expect_statement_end());
    return m_builder.make_continue(start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_assert_stmt()
{
    FA_PARSE_TRY(start, expect(TT::KW_ASSERT, ErrorCode::UNEXPECTED_TOKEN));
    FA_PARSE_TRY(condition, parse_expression());
    Array<AST::ExprPtr> arguments { condition };
    if (consume(TT::COMMA)) {
        FA_PARSE_TRY(message, parse_expression());
        arguments.push(message);
    }
    FA_PARSE_REQUIRE(expect_statement_end());
    auto* callee = m_builder.make_identifier("تاكد", start->location());
    auto* call = m_builder.make_call(callee, std::move(arguments), start->location());
    return m_builder.make_expr_stmt(call, start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_while_stmt()
{
    FA_PARSE_TRY(start, expect(TT::KW_WHILE, ErrorCode::EXPECTED_WHILE_KEYWORD));
    FA_PARSE_TRY(condition, parse_expression());
    FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_WHILE));
    FA_PARSE_TRY(body, parse_block());
    return m_builder.make_while(condition, body, start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_for_stmt()
{
    FA_PARSE_TRY(start, expect(TT::KW_FOR, ErrorCode::UNEXPECTED_TOKEN));
    FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_FOR_TARGET));
    auto* target = m_builder.make_identifier(name->lexeme(), name->location());
    FA_PARSE_REQUIRE(expect(TT::KW_IN, ErrorCode::EXPECTED_IN_KEYWORD));
    FA_PARSE_TRY(iterable, parse_expression());
    FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_FOR));
    FA_PARSE_TRY(body, parse_block());
    return m_builder.make_for(target, iterable, body, start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_if_stmt(u16 clause_column)
{
    struct Arm {
        AST::ExprPtr condition;
        AST::StmtPtr body;
        SourceLocation location;
    };
    std::vector<Arm> arms;
    if (clause_column == 0) clause_column = current_loc().column;
    AST::StmtPtr tail = nullptr;
    for (;;) {
        FA_PARSE_TRY(start, expect(TT::KW_IF, ErrorCode::EXPECTED_IF_KEYWORD));
        FA_PARSE_TRY(condition, parse_expression());
        FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_IF));
        FA_PARSE_TRY(body, parse_block());
        arms.push_back({ condition, body, start->location() });
        skip_newlines();
        // An inline nested if cannot steal an enclosing, aligned else.
        if (!check(TT::KW_ELSE) || current_loc().column != clause_column) break;
        advance();
        skip_newlines();
        if (check(TT::KW_IF)) continue;
        FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_IF));
        FA_PARSE_TRY(otherwise, parse_block());
        tail = otherwise;
        break;
    }
    for (auto it = arms.rbegin(); it != arms.rend(); ++it) tail = m_builder.make_if(it->condition, it->body, it->location, tail);
    assert(tail != nullptr);
    return tail;
}

ErrorOr<AST::StmtPtr> Parser::parse_block(Array<AST::ExprPtr>* members)
{
    if (check(TT::NEWLINE) || check(TT::INDENT)) return parse_indented_block(members);
    DepthScope depth(*this);
    if (!depth) return fail(ErrorCode::EXCEEDED_MAX_NESTING_LIMIT);
    if (check(TT::DEDENT) || we_done()) return fail(ErrorCode::EXPECTED_INDENT);
    FA_PARSE_TRY(statement, members ? parse_class_method_statement(*members) : parse_statement());
    return m_builder.make_block({ statement }, statement->get_location());
}

ErrorOr<AST::StmtPtr> Parser::parse_indented_block(Array<AST::ExprPtr>* members)
{
    DepthScope depth(*this);
    if (!depth) return fail(ErrorCode::EXCEEDED_MAX_NESTING_LIMIT);
    SourceLocation const start = current_loc();
    skip_newlines();
    if (!check(TT::INDENT)) {
        m_recovery_boundary = m_consumed;
        return fail(ErrorCode::EXPECTED_INDENT);
    }
    FA_PARSE_REQUIRE(expect(TT::INDENT, ErrorCode::EXPECTED_INDENT));
    auto statements = parse_sequence(members ? Sequence::Method : Sequence::Block, members);
    // Saturation is a terminal state for this parse. Preserve the partial AST
    // without issuing a misleading missing-DEDENT diagnostic at the stop point.
    if (!diagnostic::is_saturated()) FA_PARSE_REQUIRE(expect(TT::DEDENT, ErrorCode::EXPECTED_DEDENT));
    return m_builder.make_block(std::move(statements), start);
}

template<class T, class ReadItem> ErrorOr<Array<T>> Parser::parse_delimited(TT close, ErrorCode missing_close, ReadItem&& read_item, bool* had_comma)
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

ErrorOr<Array<AST::ExprPtr>> Parser::parse_parameters_list()
{
    FA_PARSE_REQUIRE(expect(TT::LPAREN, ErrorCode::EXPECTED_LPAREN));
    return parse_delimited<AST::ExprPtr>(TT::RPAREN, ErrorCode::EXPECTED_RPAREN_EXPR, [&]() -> ErrorOr<AST::ExprPtr> {
        FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_PARAM_NAME));
        return m_builder.make_identifier(name->lexeme(), name->location());
    });
}

ErrorOr<AST::StmtPtr> Parser::parse_function_def()
{
    FA_PARSE_TRY(start, expect(TT::KW_FN, ErrorCode::EXPECTED_FN_KEYWORD));
    FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_FN_NAME));
    FA_PARSE_TRY(parameters, parse_parameters_list());
    FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_FN));
    FA_PARSE_TRY(body, m_body_parsing == BodyParsing::Lazy ? defer_body(start->location()) : parse_block());
    return m_builder.make_function(m_builder.make_identifier(name->lexeme(), name->location()), std::move(parameters), body, start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_function_body()
{
    FA_PARSE_TRY(body, parse_block());
    skip_newlines();
    FA_PARSE_REQUIRE(expect(TT::ENDMARKER, ErrorCode::UNEXPECTED_TOKEN));
    return body;
}

// The lexer has already removed newlines inside (), [], and {}. Consequently
// only logical NEWLINE/INDENT/DEDENT tokens participate in capture. There are
// three states: an indented suite; an inline simple statement; and an inline
// compound statement whose continuation can include a suite or aligned else.
// The enclosing DEDENT is never copied. A captured suite's own DEDENT is copied.
// This scan is iterative and performs no AST construction or body validation.
ErrorOr<AST::StmtPtr> Parser::defer_body(SourceLocation definition)
{
    SourceLocation const start = current_loc();
    Array<TokenPtr> tokens;
    auto take = [&] {
        assert(!we_done());
        tokens.push(current_token());
        advance();
    };
    bool const indented = check(TT::NEWLINE) || check(TT::INDENT);
    bool const compound = starts_compound(current_token()->type());
    if (indented) {
        while (check(TT::NEWLINE)) take();
        if (!check(TT::INDENT)) {
            m_recovery_boundary = m_consumed;
            return fail(ErrorCode::EXPECTED_INDENT);
        }
    } else if (check(TT::DEDENT) || we_done()) {
        return fail(ErrorCode::EXPECTED_INDENT);
    }

    // Only if headers on the initial inline level can own continuations at
    // that level. Else-if retains its original if's column, just as eager
    // parsing does. Merely being to the right of the function is insufficient.
    std::vector<u16> inline_if_columns;
    bool after_else = false;
    auto owns_else = [&] {
        if (!check(TT::KW_ELSE) || current_loc().column <= definition.column) return false;
        for (u16 column : inline_if_columns) {
            if (column == current_loc().column) return true;
        }
        return false;
    };
    size_t depth = 0;
    while (!we_done()) {
        if (!indented && depth == 0) {
            if (check(TT::KW_IF) && !after_else) inline_if_columns.push_back(current_loc().column);
            if (!check(TT::NEWLINE)) after_else = check(TT::KW_ELSE);
        }
        if (check(TT::INDENT)) {
            ++depth;
        } else if (check(TT::DEDENT)) {
            if (depth == 0) break;
            --depth;
            take();
            if (depth == 0 && (indented || !owns_else())) break;
            continue;
        } else if (check(TT::NEWLINE) && depth == 0) {
            take();
            if (!compound) break;
            while (check(TT::NEWLINE)) take();
            if (!check(TT::INDENT) && !owns_else()) break;
            continue;
        }
        take();
    }
    if (depth != 0) return fail(ErrorCode::EXPECTED_DEDENT);
    SourceLocation const end = current_loc();
    tokens.push(make_token(TT::ENDMARKER, "", end));
    auto source = m_lexer.source();
    StringRef text;
    if (source && start.offset <= end.offset && end.offset <= source->text.size()) text = m_lexer.source_slice(start.offset, end.offset);
    return m_builder.make_function_stub(std::move(tokens), std::move(source), std::move(text), start);
}

ErrorOr<AST::StmtPtr> Parser::parse_class_def()
{
    DepthScope depth(*this);
    if (!depth) return fail(ErrorCode::EXCEEDED_MAX_NESTING_LIMIT);
    FA_PARSE_TRY(start, expect(TT::KW_CLASS, ErrorCode::EXPECTED_CLASS_KEYWORD));
    FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_CLASS_NAME));
    auto* class_name = m_builder.make_identifier(name->lexeme(), name->location());
    AST::ExprPtr parent = nullptr;
    if (consume(TT::LPAREN)) {
        FA_PARSE_TRY(parent_name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_CLASS_NAME));
        parent = m_builder.make_identifier(parent_name->lexeme(), parent_name->location());
        FA_PARSE_REQUIRE(expect(TT::RPAREN, ErrorCode::EXPECTED_RPAREN_CLASS));
    }
    FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_CLASS));
    Array<AST::ExprPtr> members;
    Array<AST::StmtPtr> methods;
    if (check(TT::NEWLINE) || check(TT::INDENT)) {
        skip_newlines();
        if (!check(TT::INDENT)) {
            m_recovery_boundary = m_consumed;
            return fail(ErrorCode::EXPECTED_INDENT);
        }
        FA_PARSE_REQUIRE(expect(TT::INDENT, ErrorCode::EXPECTED_INDENT));
        methods = parse_sequence(Sequence::Class, &members);
        if (!diagnostic::is_saturated()) FA_PARSE_REQUIRE(expect(TT::DEDENT, ErrorCode::EXPECTED_DEDENT));
    } else {
        FA_PARSE_TRY(method, parse_class_method(members));
        methods.push(method);
    }
    return m_builder.make_class_def(class_name, parent, std::move(members), std::move(methods), start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_class_method(Array<AST::ExprPtr>& members)
{
    FA_PARSE_TRY(start, expect(TT::KW_FN, ErrorCode::EXPECTED_FN_KEYWORD));
    FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_FN_NAME));
    StringRef spelling = name->lexeme();
    if (spelling == "عملية") {
        switch (current_token()->type()) {
        case TT::OP_PLUS:
        case TT::OP_MINUS:
        case TT::OP_STAR:
        case TT::OP_SLASH:
        case TT::OP_PERCENT:
            spelling += current_token()->lexeme();
            advance();
            break;
        default: break;
        }
    }
    FA_PARSE_TRY(parameters, parse_parameters_list());
    FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_FN));

    // Field discovery is committed only for a successfully parsed method.
    // Failed/recovered methods cannot leave phantom fields in their class.
    Array<AST::ExprPtr> discovered;
    auto const errors_before = diagnostic::error_count();
    FA_PARSE_TRY(body, parse_block(&discovered));
    if (diagnostic::error_count() == errors_before) {
        for (auto* member : discovered) push_member_once(members, AST::as_identifier(member));
    }
    return m_builder.make_function(m_builder.make_identifier(spelling, name->location()), std::move(parameters), body, start->location());
}

void Parser::push_member_once(Array<AST::ExprPtr>& members, AST::IdentifierExpr const* name)
{
    assert(name != nullptr);
    for (auto* member : members) {
        if (same_name(member, name->spelling)) return;
    }
    members.push(m_builder.make_identifier(name->spelling, name->get_location()));
}

// Walk statements in source order without recursing through long else-if
// chains. Definitions introduce another scope and are deliberately not visited.
void Parser::collect_this_field_assignment(Array<AST::ExprPtr>& members, AST::StmtPtr root)
{
    std::vector<AST::StmtPtr> work;
    if (root != nullptr) work.push_back(root);
    while (!work.empty()) {
        auto* statement = work.back();
        work.pop_back();
        if (AST::is_block(statement)) {
            auto const& statements = AST::as_block(statement)->stmts;
            for (size_t i = statements.size(); i != 0; --i) work.push_back(statements[i - 1]);
        } else if (AST::is_if(statement)) {
            auto* branch = AST::as_if(statement);
            if (branch->else_stmt != nullptr) work.push_back(branch->else_stmt);
            work.push_back(branch->then_stmt);
        } else if (AST::is_for(statement)) {
            work.push_back(AST::as_for(statement)->body);
        } else if (AST::is_while(statement)) {
            work.push_back(AST::as_while(statement)->body);
        } else if (AST::is_expr(statement)) {
            auto* expression = AST::as_expr_stmt(statement)->expr;
            // Every target in a plain assignment chain can introduce a field.
            while (AST::is_assignment(expression)) {
                auto* assignment = AST::as_assignment_expr(expression);
                if (AST::is_get(assignment->target)) {
                    auto* target = AST::as_get(assignment->target);
                    if (same_name(target->object, class_instance_name)) push_member_once(members, target->member);
                }
                expression = assignment->value;
            }
        }
    }
}

ErrorOr<AST::StmtPtr> Parser::parse_class_method_statement(Array<AST::ExprPtr>& members)
{
    skip_newlines();
    if (!consume(TT::DOT)) {
        auto const errors_before = diagnostic::error_count();
        FA_PARSE_TRY(statement, parse_statement());
        if (diagnostic::error_count() == errors_before) collect_this_field_assignment(members, statement);
        return statement;
    }
    FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::INVALID_ASSIGN_TARGET));
    auto* target = m_builder.make_get_expr(m_builder.make_identifier(class_instance_name, name->location()),
        m_builder.make_identifier(name->lexeme(), name->location()), name->location());
    if (!is_assignment(current_token()->type())) return fail(ErrorCode::INVALID_ASSIGN_TARGET);
    FA_PARSE_TRY(assignment, parse_assignment_tail(target, true));
    FA_PARSE_REQUIRE(expect_statement_end());
    auto* statement = m_builder.make_expr_stmt(assignment, name->location());
    collect_this_field_assignment(members, statement);
    return statement;
}

ErrorOr<AST::StmtPtr> Parser::parse_import_stmt()
{
    SourceLocation const start = current_loc();
    bool const from = consume(TT::KW_FROM);
    if (!from) FA_PARSE_REQUIRE(expect(TT::KW_IMPORT, ErrorCode::EXPECTED_IMPORT_KEYWORD));
    FA_PARSE_TRY(first, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_MODULE_NAME));
    TokenPtr last_component = first;
    std::string module_text(first->lexeme().data(), first->lexeme().len());
    while (consume(TT::DOT)) {
        FA_PARSE_TRY(component, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_MODULE_NAME));
        module_text += '.';
        module_text.append(component->lexeme().data(), component->lexeme().len());
        last_component = component;
    }
    StringRef module(module_text.c_str());
    Array<StringRef> names;
    Array<StringRef> aliases;
    if (from) {
        FA_PARSE_REQUIRE(expect(TT::KW_IMPORT, ErrorCode::EXPECTED_IMPORT_KEYWORD));
        using Name = std::pair<StringRef, StringRef>;
        auto read_name = [&]() -> ErrorOr<Name> {
            FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_IMPORT_NAME));
            StringRef alias = name->lexeme();
            if (consume(TT::KW_AS)) {
                FA_PARSE_TRY(renamed, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_ALIAS_NAME));
                alias = renamed->lexeme();
            }
            return Name { name->lexeme(), std::move(alias) };
        };
        if (consume(TT::LPAREN)) {
            skip_newlines();
            // Empty imports are not part of the existing grammar.
            if (check(TT::RPAREN)) return fail(ErrorCode::EXPECTED_IMPORT_NAME);
            FA_PARSE_TRY(imports, parse_delimited<Name>(TT::RPAREN, ErrorCode::EXPECTED_RPAREN_EXPR, read_name));
            for (auto& imported : imports) {
                names.push(std::move(imported.first));
                aliases.push(std::move(imported.second));
            }
        } else {
            do {
                // Bare imports must not consume the following logical line.
                FA_PARSE_TRY(imported, read_name());
                names.push(std::move(imported.first));
                aliases.push(std::move(imported.second));
            } while (consume(TT::COMMA));
        }
    } else {
        StringRef alias = last_component->lexeme();
        if (consume(TT::KW_AS)) {
            FA_PARSE_TRY(renamed, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_ALIAS_NAME));
            alias = renamed->lexeme();
        }
        aliases.push(std::move(alias));
    }
    FA_PARSE_REQUIRE(expect_statement_end());
    assert(from ? names.size() == aliases.size() : names.empty() && aliases.size() == 1);
    return m_builder.make_import(std::move(module), std::move(names), std::move(aliases), start);
}

ErrorOr<AST::ExprPtr> Parser::parse_expression()
{
    FA_PARSE_TRY(expression, parse_binary_expr_precedence(0));
    if (is_assignment(current_token()->type())) return fail(ErrorCode::ASSIGNMENT_IN_EXPRESSION);
    return expression;
}

ErrorOr<AST::ExprPtr> Parser::parse_assignment_expr(bool allow_augmented)
{
    FA_PARSE_TRY(left, parse_binary_expr_precedence(0));
    return parse_assignment_tail(left, allow_augmented);
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

ErrorOr<AST::ExprPtr> Parser::parse_assignment_tail(AST::ExprPtr left, bool allow_augmented)
{
    std::vector<AST::ExprPtr> targets;
    while (is_assignment(current_token()->type())) {
        if (!is_assignable(left)) return fail(ErrorCode::INVALID_ASSIGN_TARGET);
        if (auto operation = augmented_operator(current_token()->type())) {
            if (!allow_augmented || !targets.empty()) return fail(ErrorCode::ASSIGNMENT_IN_EXPRESSION);
            advance();
            FA_PARSE_TRY(right, parse_expression());
            return make_augmented(left, right, *operation);
        }
        targets.push_back(left);
        advance();
        FA_PARSE_TRY(right, parse_binary_expr_precedence(0));
        left = right;
    }
    // Right fold gives a := (b := value) without C++ recursion proportional
    // to the number of assignment operators in the source.
    for (auto it = targets.rbegin(); it != targets.rend(); ++it) left = m_builder.make_assignment_expr(*it, left, (*it)->get_location());
    return left;
}

ErrorOr<AST::ExprPtr> Parser::parse_binary_expr_precedence(u32 minimum)
{
    FA_PARSE_TRY(left, parse_unary_expr());
    for (;;) {
        auto operation = infix_operator(current_token()->type());
        if (!operation || operation->precedence < minimum) return left;
        advance();
        // The recursive threshold strictly increases. Equal precedence is
        // folded by this loop, so an arbitrarily long flat chain uses bounded
        // parser stack space. Comparisons retain Fairuz's binary-tree meaning.
        FA_PARSE_TRY(right, parse_binary_expr_precedence(operation->precedence + 1));
        left = m_builder.make_binary(operation->kind, left, right, left->get_location());
    }
}

ErrorOr<AST::ExprPtr> Parser::parse_unary_expr()
{
    DepthScope depth(*this);
    if (!depth) return fail(ErrorCode::EXCEEDED_MAX_NESTING_LIMIT);
    TokenPtr const token = current_token();
    if (auto operation = unary_operator(token->type())) {
        advance();
        FA_PARSE_TRY(operand, parse_unary_expr());
        return m_builder.make_unary(*operation, operand, token->location());
    }
    return parse_power_expr();
}

ErrorOr<AST::ExprPtr> Parser::parse_power_expr()
{
    FA_PARSE_TRY(base, parse_postfix_expr());
    if (!consume(TT::OP_POWER)) return base;
    // unary -> power -> unary makes -2**2 equal -(2**2), admits 2**-2,
    // and makes 2**3**4 equal 2**(3**4), with a guard on every recursive cycle.
    FA_PARSE_TRY(exponent, parse_unary_expr());
    return m_builder.make_binary(EK::OP_POW, base, exponent, base->get_location());
}

ErrorOr<AST::ExprPtr> Parser::parse_postfix_expr()
{
    FA_PARSE_TRY(expression, parse_primary_expr());
    for (;;) {
        if (consume(TT::LPAREN)) {
            FA_PARSE_TRY(arguments, parse_delimited<AST::ExprPtr>(TT::RPAREN, ErrorCode::EXPECTED_RPAREN_EXPR, [&] { return parse_expression(); }));
            expression = m_builder.make_call(expression, std::move(arguments), expression->get_location());
        } else if (consume(TT::LBRACKET)) {
            FA_PARSE_TRY(index, parse_expression());
            FA_PARSE_REQUIRE(expect(TT::RBRACKET, ErrorCode::EXPECTED_RBRACKET));
            expression = m_builder.make_index(expression, index, expression->get_location());
        } else if (consume(TT::DOT)) {
            FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_MEMBER_NAME));
            expression = m_builder.make_get_expr(expression, m_builder.make_identifier(name->lexeme(), name->location()), expression->get_location());
        } else {
            return expression;
        }
    }
}

ErrorOr<AST::ExprPtr> Parser::parse_primary_expr()
{
    TokenPtr const token = current_token();
    if (token->is_numeric()) {
        advance();
        if (token->is(TT::DECIMAL)) {
            diagnostic::SourceScope source_scope(m_lexer.source());
            f64 value = 0.0;
            if (!util::try_parse_float_literal(token->lexeme(), value)) return fail(ErrorCode::INVALID_NUMBER_LITERAL, token->location());
            return m_builder.make_literal_float(value, token->location());
        }
        unsigned base = token->is(TT::HEX) ? 16 : token->is(TT::OCTAL) ? 8 : token->is(TT::BINARY) ? 2 : 10;
        i64 value = 0;
        auto status = integer_literal(token->lexeme(), base, value);
        if (status == IntegerStatus::INVALID) return fail(ErrorCode::INVALID_NUMBER_LITERAL, token->location());
        auto* literal = m_builder.make_literal_int(value, token->location());
        if (status == IntegerStatus::LARGE) {
            literal->large_literal = token->lexeme();
            literal->literal_base = static_cast<int>(base);
        }
        return literal;
    }
    switch (token->type()) {
    case TT::STRING: advance(); return m_builder.make_literal_string(token->lexeme(), token->location());
    case TT::KW_TRUE:
    case TT::KW_FALSE: advance(); return m_builder.make_literal_bool(token->is(TT::KW_TRUE), token->location());
    case TT::KW_NIL: advance(); return m_builder.make_nil(token->location());
    case TT::KW_THIS: advance(); return m_builder.make_identifier(class_instance_name, token->location());
    case TT::IDENTIFIER: advance(); return m_builder.make_identifier(token->lexeme(), token->location());
    case TT::LPAREN: {
        advance();
        bool had_comma = false;
        FA_PARSE_TRY(
            elements, parse_delimited<AST::ExprPtr>(TT::RPAREN, ErrorCode::EXPECTED_RPAREN_EXPR, [&] { return parse_expression(); }, &had_comma));
        if (elements.size() == 1 && !had_comma) return elements[0];
        return m_builder.make_list(std::move(elements), token->location());
    }
    case TT::LBRACKET: advance(); return parse_list_literal(token->location());
    case TT::LBRACE: advance(); return parse_dict_literal(token->location());
    case TT::ENDMARKER: return fail(ErrorCode::UNEXPECTED_EOF);
    default: return fail(ErrorCode::UNEXPECTED_TOKEN);
    }
}

ErrorOr<AST::ExprPtr> Parser::parse_list_literal(SourceLocation opening)
{
    FA_PARSE_TRY(elements, parse_delimited<AST::ExprPtr>(TT::RBRACKET, ErrorCode::EXPECTED_RBRACKET, [&] { return parse_expression(); }));
    return m_builder.make_list(std::move(elements), opening);
}

ErrorOr<AST::ExprPtr> Parser::parse_dict_literal(SourceLocation opening)
{
    using Entry = std::pair<AST::ExprPtr, AST::ExprPtr>;
    FA_PARSE_TRY(entries, parse_delimited<Entry>(TT::RBRACE, ErrorCode::EXPECTED_RBRACE_EXPR, [&]() -> ErrorOr<Entry> {
        FA_PARSE_TRY(key, parse_expression());
        FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_DICT));
        FA_PARSE_TRY(value, parse_expression());
        return Entry { key, value };
    }));
    return m_builder.make_dict(std::move(entries), opening);
}

ErrorOr<AST::ExprPtr> Parser::parse() { return parse_assignment_expr(); }
ErrorOr<AST::ExprPtr> Parser::parse_list_literal() { return parse_list_literal(current_loc()); }
ErrorOr<AST::ExprPtr> Parser::parse_dict_literal() { return parse_dict_literal(current_loc()); }
ErrorOr<AST::ExprPtr> Parser::parse_conditional_expr() { return parse_binary_expr_precedence(0); }
ErrorOr<AST::ExprPtr> Parser::parse_logical_expr() { return parse_binary_expr_precedence(0); }
ErrorOr<AST::ExprPtr> Parser::parse_logical_expr_precedence(u32 p) { return parse_binary_expr_precedence(p); }
ErrorOr<AST::ExprPtr> Parser::parse_comparison_expr() { return parse_binary_expr_precedence(0); }
ErrorOr<AST::ExprPtr> Parser::parse_binary_expr() { return parse_binary_expr_precedence(0); }
ErrorOr<AST::ExprPtr> Parser::parse_member_access() { return parse_postfix_expr(); }

} // namespace fairuz::parser

#undef FA_PARSE_REQUIRE
#undef FA_PARSE_TRY
#undef FA_PARSE_JOIN
#undef FA_PARSE_JOIN_I
