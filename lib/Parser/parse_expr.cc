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

namespace fairuz::parser {

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

Parser::ParseExprResult Parser::parse_expression()
{
    FA_PARSE_TRY(expression, parse_binary_expr_precedence(0));
    if (is_assignment(current_token()->type())) return fail(ErrorCode::ASSIGNMENT_IN_EXPRESSION);
    return expression;
}

Parser::ParseExprResult Parser::parse_assignment_expr(bool allow_augmented)
{
    FA_PARSE_TRY(left, parse_binary_expr_precedence(0));
    return parse_assignment_tail(left, allow_augmented);
}

Parser::ParseExprResult Parser::parse_assignment_tail(AST::ExprPtr left, bool allow_augmented)
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
    for (auto it = targets.rbegin(); it != targets.rend(); ++it)
        left = m_builder.make_assignment_expr(*it, left, (*it)->get_location());
    return left;
}

Parser::ParseExprResult Parser::parse_binary_expr_precedence(u32 minimum)
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

Parser::ParseExprResult Parser::parse_unary_expr()
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

Parser::ParseExprResult Parser::parse_power_expr()
{
    FA_PARSE_TRY(base, parse_postfix_expr());
    if (!consume(TT::OP_POWER)) return base;
    // unary -> power -> unary makes -2**2 equal -(2**2), admits 2**-2,
    // and makes 2**3**4 equal 2**(3**4), with a guard on every recursive cycle.
    FA_PARSE_TRY(exponent, parse_unary_expr());
    return m_builder.make_binary(EK::OP_POW, base, exponent, base->get_location());
}

Parser::ParseExprResult Parser::parse_postfix_expr()
{
    FA_PARSE_TRY(expression, parse_primary_expr());
    for (;;) {
        if (consume(TT::LPAREN)) {
            FA_PARSE_TRY(arguments, parse_delimited<AST::ExprPtr>(TT::RPAREN, ErrorCode::EXPECTED_RPAREN_EXPR, [&] {
                return parse_expression();
            }));
            expression = m_builder.make_call(expression, std::move(arguments), expression->get_location());
        } else if (consume(TT::LBRACKET)) {
            FA_PARSE_TRY(index, parse_expression());
            FA_PARSE_REQUIRE(expect(TT::RBRACKET, ErrorCode::EXPECTED_RBRACKET));
            expression = m_builder.make_index(expression, index, expression->get_location());
        } else if (consume(TT::DOT)) {
            FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_MEMBER_NAME));
            expression = m_builder.make_get_expr(
                expression, m_builder.make_identifier(name->lexeme(), name->location()), expression->get_location());
        } else {
            return expression;
        }
    }
}

IntegerStatus integer_literal(fairuz::StringRef const& spelling, unsigned base, i64& result)
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
        else if (byte == 0xD9 && i < text.size() && static_cast<unsigned char>(text[i]) >= 0xA0
            && static_cast<unsigned char>(text[i]) <= 0xA9)
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
    result = negative
        ? (magnitude == (u64 { 1 } << 63) ? std::numeric_limits<i64>::min() : -static_cast<i64>(magnitude))
        : static_cast<i64>(magnitude);
    return IntegerStatus::SMALL;
}

Parser::ParseExprResult Parser::parse_primary_expr()
{
    TokenPtr const token = current_token();
    if (token->is_numeric()) {
        advance();
        if (token->is(TT::DECIMAL)) {
            diagnostic::SourceScope source_scope(m_lexer.source());
            f64 value = 0.0;
            if (!util::try_parse_float_literal(token->lexeme(), value))
                return fail(ErrorCode::INVALID_NUMBER_LITERAL, token->location());
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
        FA_PARSE_TRY(elements,
            parse_delimited<AST::ExprPtr>(
                TT::RPAREN, ErrorCode::EXPECTED_RPAREN_EXPR, [&] { return parse_expression(); }, &had_comma));
        if (elements.size() == 1 && !had_comma) return elements[0];
        return m_builder.make_list(std::move(elements), token->location());
    }
    case TT::LBRACKET: advance(); return parse_list_literal(token->location());
    case TT::LBRACE: advance(); return parse_dict_literal(token->location());
    case TT::ENDMARKER: return fail(ErrorCode::UNEXPECTED_EOF);
    default: return fail(ErrorCode::UNEXPECTED_TOKEN);
    }
}

Parser::ParseExprResult Parser::parse_list_literal(SourceLocation opening)
{
    FA_PARSE_TRY(elements,
        parse_delimited<AST::ExprPtr>(TT::RBRACKET, ErrorCode::EXPECTED_RBRACKET, [&] { return parse_expression(); }));
    return m_builder.make_list(std::move(elements), opening);
}

Parser::ParseExprResult Parser::parse_dict_literal(SourceLocation opening)
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

Parser::ParseExprResult Parser::parse() { return parse_assignment_expr(); }
Parser::ParseExprResult Parser::parse_list_literal() { return parse_list_literal(current_loc()); }
Parser::ParseExprResult Parser::parse_dict_literal() { return parse_dict_literal(current_loc()); }
Parser::ParseExprResult Parser::parse_conditional_expr() { return parse_binary_expr_precedence(0); }
Parser::ParseExprResult Parser::parse_logical_expr() { return parse_binary_expr_precedence(0); }
Parser::ParseExprResult Parser::parse_logical_expr_precedence(u32 p) { return parse_binary_expr_precedence(p); }
Parser::ParseExprResult Parser::parse_comparison_expr() { return parse_binary_expr_precedence(0); }
Parser::ParseExprResult Parser::parse_binary_expr() { return parse_binary_expr_precedence(0); }
Parser::ParseExprResult Parser::parse_member_access() { return parse_postfix_expr(); }

} // namespace fairuz
