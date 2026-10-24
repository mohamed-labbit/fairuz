//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "Parser/parser.hpp"
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

Parser::ParseStmtResult Parser::parse_statement()
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

Parser::ParseStmtResult Parser::parse_expression_stmt()
{
    FA_PARSE_TRY(expression, parse_assignment_expr());
    FA_PARSE_REQUIRE(expect_statement_end());
    return m_builder.make_expr_stmt(expression, expression->get_location());
}

Parser::ParseStmtResult Parser::parse_return_stmt()
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

Parser::ParseStmtResult Parser::parse_break_stmt()
{
    FA_PARSE_TRY(start, expect(TT::KW_BREAK, ErrorCode::UNEXPECTED_TOKEN));
    FA_PARSE_REQUIRE(expect_statement_end());
    return m_builder.make_break(start->location());
}

Parser::ParseStmtResult Parser::parse_continue_stmt()
{
    FA_PARSE_TRY(start, expect(TT::KW_CONTINUE, ErrorCode::UNEXPECTED_TOKEN));
    FA_PARSE_REQUIRE(expect_statement_end());
    return m_builder.make_continue(start->location());
}

Parser::ParseStmtResult Parser::parse_assert_stmt()
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

Parser::ParseStmtResult Parser::parse_while_stmt()
{
    FA_PARSE_TRY(start, expect(TT::KW_WHILE, ErrorCode::EXPECTED_WHILE_KEYWORD));
    FA_PARSE_TRY(condition, parse_expression());
    FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_WHILE));
    FA_PARSE_TRY(body, parse_block());
    return m_builder.make_while(condition, body, start->location());
}

Parser::ParseStmtResult Parser::parse_for_stmt()
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

Parser::ParseStmtResult Parser::parse_if_stmt(u16 clause_column)
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
    for (auto it = arms.rbegin(); it != arms.rend(); ++it)
        tail = m_builder.make_if(it->condition, it->body, it->location, tail);
    assert(tail != nullptr);
    return tail;
}

Parser::ParseStmtResult Parser::parse_block(Array<AST::ExprPtr>* members)
{
    if (check(TT::NEWLINE) || check(TT::INDENT)) return parse_indented_block(members);
    DepthScope depth(*this);
    if (!depth) return fail(ErrorCode::EXCEEDED_MAX_NESTING_LIMIT);
    if (check(TT::DEDENT) || we_done()) return fail(ErrorCode::EXPECTED_INDENT);
    FA_PARSE_TRY(statement, members ? parse_class_method_statement(*members) : parse_statement());
    return m_builder.make_block({ statement }, statement->get_location());
}

Parser::ParseStmtResult Parser::parse_indented_block(Array<AST::ExprPtr>* members)
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
    auto statements = parse_sequence(members ? Sequence::METHOD : Sequence::BLOCK, members);
    // Saturation is a terminal state for this parse. Preserve the partial AST
    // without issuing a misleading missing-DEDENT diagnostic at the stop point.
    if (!diagnostic::is_saturated()) FA_PARSE_REQUIRE(expect(TT::DEDENT, ErrorCode::EXPECTED_DEDENT));
    return m_builder.make_block(std::move(statements), start);
}

ErrorOr<Array<AST::ExprPtr>> Parser::parse_parameters_list()
{
    FA_PARSE_REQUIRE(expect(TT::LPAREN, ErrorCode::EXPECTED_LPAREN));
    return parse_delimited<AST::ExprPtr>(TT::RPAREN, ErrorCode::EXPECTED_RPAREN_EXPR, [&]() -> Parser::ParseExprResult {
        FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_PARAM_NAME));
        return m_builder.make_identifier(name->lexeme(), name->location());
    });
}

Parser::ParseStmtResult Parser::parse_function_def()
{
    FA_PARSE_TRY(start, expect(TT::KW_FN, ErrorCode::EXPECTED_FN_KEYWORD));
    FA_PARSE_TRY(name, expect(TT::IDENTIFIER, ErrorCode::EXPECTED_FN_NAME));
    FA_PARSE_TRY(parameters, parse_parameters_list());
    FA_PARSE_REQUIRE(expect(TT::COLON, ErrorCode::EXPECTED_COLON_FN));
    FA_PARSE_TRY(body, m_body_parsing == BodyParsing::LAZY ? defer_body(start->location()) : parse_block());
    return m_builder.make_function(
        m_builder.make_identifier(name->lexeme(), name->location()), std::move(parameters), body, start->location());
}

Parser::ParseStmtResult Parser::parse_function_body()
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
Parser::ParseStmtResult Parser::defer_body(SourceLocation definition)
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
    if (source && start.offset <= end.offset && end.offset <= source->text.size())
        text = m_lexer.source_slice(start.offset, end.offset);
    return m_builder.make_function_stub(std::move(tokens), std::move(source), std::move(text), start);
}

Parser::ParseStmtResult Parser::parse_class_def()
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
        methods = parse_sequence(Sequence::CLASS, &members);
        if (!diagnostic::is_saturated()) FA_PARSE_REQUIRE(expect(TT::DEDENT, ErrorCode::EXPECTED_DEDENT));
    } else {
        FA_PARSE_TRY(method, parse_class_method(members));
        methods.push(method);
    }
    return m_builder.make_class_def(class_name, parent, std::move(members), std::move(methods), start->location());
}

Parser::ParseStmtResult Parser::parse_class_method(Array<AST::ExprPtr>& members)
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
    return m_builder.make_function(
        m_builder.make_identifier(spelling, name->location()), std::move(parameters), body, start->location());
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

Parser::ParseStmtResult Parser::parse_class_method_statement(Array<AST::ExprPtr>& members)
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

Parser::ParseStmtResult Parser::parse_import_stmt()
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

} // namespace fairuz::parser
