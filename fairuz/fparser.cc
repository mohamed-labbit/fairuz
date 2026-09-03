//
// fparser.cc
//

#include "fparser.hpp"
#include "fdiagnostic.hpp"
#include "fmacros.hpp"
#include "futil.hpp"

namespace fairuz::parser {

// Macros

// Consume a token, early-return the error code if the token doesn't match.
#define Fa_VERIFY_TOKEN(expected, errc) \
    do {                                \
        if (UNLIKELY(!match(expected))) \
            return report_error(errc);  \
    } while (0)

// Propagate an error from an Fa_ErrorOr expression without unwrapping.
#define Fa_VERIFY_NODE(n)              \
    do {                               \
        if (UNLIKELY((n).has_error())) \
            return (n).error();        \
    } while (0)

// Token-pasting helpers for unique temporary names (standard C++, no GNU extension).
#define FA_CONCAT_(a, b) a##b
#define FA_CONCAT(a, b) FA_CONCAT_(a, b)

// Fa_TRY — replaces the GNU ({...}) statement-expression macros.
//
// Declares `var` as the unwrapped value of `expr` (which must return
// Fa_ErrorOr<T>).  Early-returns the error from the *enclosing function* if
// any.  Each expansion creates a uniquely named temporary via __LINE__, so
// multiple Fa_TRY calls in the same scope are safe as long as they appear on
// different source lines (which they always should).
#define Fa_TRY(var, expr)                                   \
    auto FA_CONCAT(fa_try_, __LINE__) = (expr);             \
    if (UNLIKELY(FA_CONCAT(fa_try_, __LINE__).has_error())) \
        return FA_CONCAT(fa_try_, __LINE__).error();        \
    auto var = std::move(FA_CONCAT(fa_try_, __LINE__)).value()

// Type aliases

using TokType = tok::Fa_TokenType;
using StmtPtr = AST::Fa_Stmt*;
using ExprPtr = AST::Fa_Expr*;
using TokenPtr = tok::Fa_Token const*;
using ParserCode = diagnostic::errc::parser::Code;
using SemaCode = diagnostic::errc::sema::Code;

// Shared between the parser (parse_class_method) and the semantic analyser
// (analyze_stmt CLASS_DEF).  Move to ast_constants.hpp if the two components
// are ever split into separate translation units.
static constexpr char kClassInstanceName[] = "__class$instance";

// File-local helpers

namespace {

// Maps an augmented-assignment operator token to the corresponding binary op.
// Replaces the fragile pointer-arithmetic enum indexing used previously.
AST::Fa_BinaryOp augmented_assign_to_binary_op(TokType t)
{
    switch (t) {
    case TokType::OP_PLUSEQ: return AST::Fa_BinaryOp::OP_ADD;
    case TokType::OP_MINUSEQ: return AST::Fa_BinaryOp::OP_SUB;
    case TokType::OP_STAREQ: return AST::Fa_BinaryOp::OP_MUL;
    case TokType::OP_SLASHEQ: return AST::Fa_BinaryOp::OP_DIV;
    case TokType::OP_PERCENTEQ: return AST::Fa_BinaryOp::OP_MOD;
    default: return AST::Fa_BinaryOp::INVALID;
    }
}

bool is_augmented_assign_tok(TokenPtr t)
{
    return t->is(TokType::OP_PLUSEQ)
        || t->is(TokType::OP_MINUSEQ)
        || t->is(TokType::OP_STAREQ)
        || t->is(TokType::OP_SLASHEQ)
        || t->is(TokType::OP_PERCENTEQ)
        || t->is(TokType::OP_ANDEQ)
        || t->is(TokType::OP_OREQ)
        || t->is(TokType::OP_XOREQ)
        || t->is(TokType::OP_LSHIFTEQ)
        || t->is(TokType::OP_RSHIFTEQ);
}

} // anonymous namespace

/// NOTE: These are logically a property of the token type and would be better
/// placed as methods on Fa_Token or in a token_ops.hpp utility header.

AST::Fa_BinaryOp to_binary_op(TokType const op)
{
    switch (op) {
    case TokType::OP_PLUS: return AST::Fa_BinaryOp::OP_ADD;
    case TokType::OP_MINUS: return AST::Fa_BinaryOp::OP_SUB;
    case TokType::OP_STAR: return AST::Fa_BinaryOp::OP_MUL;
    case TokType::OP_SLASH: return AST::Fa_BinaryOp::OP_DIV;
    case TokType::OP_PERCENT: return AST::Fa_BinaryOp::OP_MOD;
    case TokType::OP_POWER: return AST::Fa_BinaryOp::OP_POW;
    case TokType::OP_EQ: return AST::Fa_BinaryOp::OP_EQ;
    case TokType::OP_NEQ: return AST::Fa_BinaryOp::OP_NEQ;
    case TokType::OP_LT: return AST::Fa_BinaryOp::OP_LT;
    case TokType::OP_GT: return AST::Fa_BinaryOp::OP_GT;
    case TokType::OP_LTE: return AST::Fa_BinaryOp::OP_LTE;
    case TokType::OP_GTE: return AST::Fa_BinaryOp::OP_GTE;
    case TokType::OP_BITAND: return AST::Fa_BinaryOp::OP_BITAND;
    case TokType::OP_BITOR: return AST::Fa_BinaryOp::OP_BITOR;
    case TokType::OP_BITXOR: return AST::Fa_BinaryOp::OP_BITXOR;
    case TokType::OP_LSHIFT: return AST::Fa_BinaryOp::OP_LSHIFT;
    case TokType::OP_RSHIFT: return AST::Fa_BinaryOp::OP_RSHIFT;
    case TokType::OP_AND: return AST::Fa_BinaryOp::OP_AND;
    case TokType::OP_OR: return AST::Fa_BinaryOp::OP_OR;
    default: return AST::Fa_BinaryOp::INVALID;
    }
}

AST::Fa_UnaryOp to_unary_op(TokType const op)
{
    switch (op) {
    case TokType::OP_PLUS: return AST::Fa_UnaryOp::OP_PLUS;
    case TokType::OP_MINUS: return AST::Fa_UnaryOp::OP_NEG;
    case TokType::OP_BITNOT: return AST::Fa_UnaryOp::OP_BITNOT;
    case TokType::OP_NOT: return AST::Fa_UnaryOp::OP_NOT;
    default: return AST::Fa_UnaryOp::INVALID;
    }
}

// Fa_Parser — utilities

Fa_Error Fa_Parser::report_error(ParserCode err_code, diagnostic::Severity sv)
{
    auto* tok = current_token();
    Fa_SourceLocation loc = { tok->line(), tok->column(), static_cast<u16>(tok->lexeme().len()) };
    return fairuz::report_error(err_code, loc, sv);
}

bool Fa_Parser::we_done() const { return current_token()->is(TokType::ENDMARKER); }

bool Fa_Parser::check(TokType t) const { return current_token()->is(t); }

TokenPtr Fa_Parser::current_token() const { return m_lexer.current(); }

bool Fa_Parser::match(TokType const type)
{
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

void Fa_Parser::synchronize()
{
    while (!we_done()) {
        if (check(TokType::NEWLINE) || check(TokType::DEDENT)) {
            advance();
            return;
        }
        if (check(TokType::KW_IF) || check(TokType::KW_WHILE)
            || check(TokType::KW_FOR) || check(TokType::KW_RETURN)
            || check(TokType::KW_BREAK) || check(TokType::KW_CONTINUE)
            || check(TokType::KW_FN))
            return;
        advance();
    }
}

// Fa_Parser — top-level

Fa_Array<StmtPtr> Fa_Parser::parse_program()
{
    Fa_Array<StmtPtr> stmts;

    while (!we_done()) {
        skip_newlines();
        if (we_done())
            break;

        auto stmt = parse_statement();
        if (stmt.has_value()) {
            stmts.push(stmt.value());
        } else {
            if (diagnostic::is_saturated())
                break;
            synchronize();
            if (we_done())
                break;
        }
    }

    if (diagnostic::has_errors())
        diagnostic::dump();

    return stmts;
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_statement()
{
    skip_newlines();

    if (check(TokType::KW_IF))
        return parse_if_stmt();
    if (check(TokType::KW_WHILE))
        return parse_while_stmt();
    if (check(TokType::KW_FOR))
        return parse_for_stmt();
    if (check(TokType::KW_RETURN))
        return parse_return_stmt();
    if (check(TokType::KW_BREAK))
        return parse_break_stmt();
    if (check(TokType::KW_CONTINUE))
        return parse_continue_stmt();
    if (check(TokType::KW_FN))
        return parse_function_def();
    if (check(TokType::KW_CLASS))
        return parse_class_def();

    return parse_expression_stmt();
}

// Fa_Parser — statement parsers

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_return_stmt()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_RETURN, ParserCode::EXPECTED_RETURN);

    if (check(TokType::NEWLINE) || we_done())
        return AST::Fa_make_return(start->location());

    Fa_TRY(ret, parse_expression());
    return AST::Fa_make_return(start->location(), ret);
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_break_stmt()
{
    TokenPtr start = current_token();
    advance();
    return AST::Fa_make_break(start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_continue_stmt()
{
    TokenPtr start = current_token();
    advance();
    return AST::Fa_make_continue(start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_while_stmt()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_WHILE, ParserCode::EXPECTED_WHILE_KEYWORD);

    Fa_TRY(condition, parse_expression());
    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_WHILE);

    auto while_block = parse_indented_block();
    Fa_VERIFY_NODE(while_block);

    return Fa_make_while(condition, AS_BLOCK(while_block.value()), start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_for_stmt()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_FOR, ParserCode::UNEXPECTED_TOKEN);

    if (!check(TokType::IDENTIFIER))
        return report_error(ParserCode::EXPECTED_FOR_TARGET);

    auto* target = AST::Fa_make_name(current_token()->lexeme(), current_token()->location());
    advance();

    bool saw_in = false;
    if (!check(TokType::IDENTIFIER)) {
        advance();
        saw_in = true;
    }
    if (!saw_in)
        return report_error(ParserCode::EXPECTED_IN_KEYWORD);

    Fa_TRY(iter, parse_expression());
    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_FOR);

    auto body = parse_indented_block();
    Fa_VERIFY_NODE(body);

    return AST::Fa_make_for(target, iter, body.value(), start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_if_stmt()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_IF, ParserCode::EXPECTED_IF_KEYWORD);

    Fa_TRY(condition, parse_expression());
    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_IF);

    auto then_block = parse_indented_block();
    Fa_VERIFY_NODE(then_block);

    StmtPtr else_block = nullptr;
    skip_newlines();

    if (match(TokType::KW_ELSE)) {
        skip_newlines();
        if (check(TokType::KW_IF)) {
            // else-if: no colon between `else` and `if`.
            auto nested = parse_if_stmt();
            Fa_VERIFY_NODE(nested);
            else_block = nested.value();
        } else {
            Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_IF);
            auto else_stmt = parse_indented_block();
            Fa_VERIFY_NODE(else_stmt);
            else_block = else_stmt.value();
        }
    }

    return Fa_make_if(condition, AS_BLOCK(then_block.value()), start->location(), else_block);
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_expression_stmt()
{
    Fa_TRY(expr, parse_expression());
    return Fa_make_expr_stmt(expr, expr->get_location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_indented_block()
{
    TokenPtr start = current_token();
    match(TokType::NEWLINE);
    Fa_VERIFY_TOKEN(TokType::INDENT, ParserCode::EXPECTED_INDENT);

    Fa_Array<StmtPtr> stmts;

    if (match(TokType::DEDENT))
        return Fa_make_block(stmts, start->location());

    while (!check(TokType::DEDENT) && !we_done()) {
        skip_newlines();
        if (check(TokType::DEDENT))
            break;

        auto stmt = parse_statement();
        if (stmt.has_value()) {
            stmts.push(stmt.value());
        } else {
            synchronize();
            if (check(TokType::DEDENT) || we_done())
                break;
        }
    }

    if (check(TokType::ENDMARKER))
        return Fa_make_block(stmts, start->location());

    Fa_VERIFY_TOKEN(TokType::DEDENT, ParserCode::EXPECTED_DEDENT);
    return Fa_make_block(stmts, start->location());
}

// Fa_Parser — function and class parsers

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_function_def()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_FN, ParserCode::EXPECTED_FN_KEYWORD);

    if (!check(TokType::IDENTIFIER))
        return report_error(ParserCode::EXPECTED_FN_NAME);
    TokenPtr name_tok = current_token();
    advance();

    auto params = parse_parameters_list();
    Fa_VERIFY_NODE(params);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_FN);

    auto body = parse_indented_block();
    Fa_VERIFY_NODE(body);

    return Fa_make_function(
        AST::Fa_make_name(name_tok->lexeme(), name_tok->location()),
        AS_LIST(params.value()),
        AS_BLOCK(body.value()),
        start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_class_def()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_CLASS, ParserCode::EXPECTED_CLASS_KEYWORD);

    if (!check(TokType::IDENTIFIER))
        return report_error(ParserCode::EXPECTED_CLASS_NAME);

    TokenPtr name_tok = current_token();
    advance();
    ExprPtr class_name = AST::Fa_make_name(name_tok->lexeme(), name_tok->location());

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_CLASS);
    skip_newlines();
    Fa_VERIFY_TOKEN(TokType::INDENT, ParserCode::EXPECTED_INDENT);

    Fa_Array<ExprPtr> members = Fa_Array<ExprPtr>::with_capacity(4);
    Fa_Array<StmtPtr> methods = Fa_Array<StmtPtr>::with_capacity(4);

    while (!check(TokType::DEDENT) && !we_done()) {
        auto method = parse_class_method(members);
        if (method.has_value()) {
            methods.push(method.value());
        } else {
            if (diagnostic::is_saturated())
                break;
            synchronize();
            if (check(TokType::DEDENT) || we_done())
                break;
        }
    }

    if (check(TokType::ENDMARKER))
        return AST::Fa_make_class_def(class_name, members, methods, start->location());

    Fa_VERIFY_TOKEN(TokType::DEDENT, ParserCode::EXPECTED_DEDENT);
    return AST::Fa_make_class_def(class_name, members, methods, start->location());
}

bool same_name(AST::Fa_Expr const* e, Fa_StringRef const& n)
{
    return e != nullptr
        && e->get_kind() == AST::Fa_Expr::Kind::NAME
        && AS_CONST_NAME(e)->get_value() == n;
}

void push_member_once(Fa_Array<ExprPtr>& members, AST::Fa_NameExpr* name)
{
    for (auto* member : members) {
        if (same_name(member, name->get_value()))
            return;
    }
    members.push(AST::Fa_make_name(name->get_value(), name->get_location()));
}

void collect_this_field_assignment(Fa_Array<ExprPtr>& members, StmtPtr stmt)
{
    auto* expr_stmt = dynamic_cast<AST::Fa_ExprStmt*>(stmt);
    if (expr_stmt == nullptr)
        return;

    auto* assign = dynamic_cast<AST::Fa_AssignmentExpr*>(expr_stmt->get_expr());
    if (assign == nullptr)
        return;

    auto* get = dynamic_cast<AST::Fa_GetExpr*>(assign->get_target());
    if (get == nullptr || !same_name(get->get_object(), kClassInstanceName))
        return;

    auto* member = dynamic_cast<AST::Fa_NameExpr*>(get->get_member());
    if (member != nullptr)
        push_member_once(members, member);
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_class_method(Fa_Array<ExprPtr>& members)
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_FN, ParserCode::EXPECTED_FN_KEYWORD);

    TokenPtr name_tok = current_token();
    Fa_VERIFY_TOKEN(TokType::IDENTIFIER, ParserCode::EXPECTED_FN_NAME);

    auto params = parse_parameters_list();
    Fa_VERIFY_NODE(params);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_FN);
    skip_newlines();
    Fa_VERIFY_TOKEN(TokType::INDENT, ParserCode::EXPECTED_INDENT);

    Fa_Array<StmtPtr> stmts;

    while (!check(TokType::DEDENT) && !we_done()) {
        skip_newlines();
        if (check(TokType::DEDENT))
            break;

        if (match(TokType::DOT)) {
            // `.field = expr` member-initializer syntax inside a method body.
            if (!check(TokType::IDENTIFIER))
                return report_error(ParserCode::INVALID_ASSIGN_TARGET);

            TokenPtr member_tok = current_token();
            Fa_StringRef mname = member_tok->lexeme();
            advance();

            // Desugar `.field` to a GET expression (instance.field), not an
            // INDEX expression with a string key.  The compiler's fas
            // field-access path (compile_get_i / SET_FIELD) specifically looks
            // for Fa_GetExpr with a NAME member; an index form would silently
            // fall back to the slow dict-style path for every field access.
            ExprPtr target = AST::Fa_make_get_expr(
                AST::Fa_make_name(kClassInstanceName, member_tok->location()),
                AST::Fa_make_name(mname, member_tok->location()),
                member_tok->location());

            AST::Fa_AssignmentExpr* member_assign = nullptr;

            if (check(TokType::OP_ASSIGN)) {
                advance();
                Fa_TRY(rhs, parse_assignment_expr());
                member_assign = AST::Fa_make_assignment_expr(target, rhs, member_tok->location());
            } else if (is_augmented_assign_tok(current_token())) {
                TokenPtr op_tok = current_token();
                advance();
                Fa_TRY(rhs, parse_assignment_expr());
                AST::Fa_BinaryOp op = augmented_assign_to_binary_op(op_tok->type());
                // target->clone() reads the current field value (GET read);
                // `target` itself is the write target.
                auto* bin = AST::Fa_make_binary(
                    target->clone(), rhs, op, target->get_location());
                member_assign = AST::Fa_make_assignment_expr(target, bin, member_tok->location());
            } else {
                return report_error(ParserCode::INVALID_ASSIGN_TARGET);
            }

            push_member_once(members, AST::Fa_make_name(mname, member_tok->location()));
            stmts.push(AST::Fa_make_expr_stmt(member_assign, member_tok->location()));
            continue;
        }

        // Regular statement inside the method body.
        Fa_TRY(s, parse_statement());
        collect_this_field_assignment(members, s);
        stmts.push(s);
    }

    AST::Fa_BlockStmt* block = AST::Fa_make_block(
        stmts,
        stmts.empty() ? start->location() : stmts[0]->get_location());

    if (check(TokType::ENDMARKER))
        return AST::Fa_make_function(
            AST::Fa_make_name(name_tok->lexeme(), name_tok->location()),
            AS_LIST(params.value()), block, start->location());

    Fa_VERIFY_TOKEN(TokType::DEDENT, ParserCode::EXPECTED_DEDENT);
    return AST::Fa_make_function(
        AST::Fa_make_name(name_tok->lexeme(), name_tok->location()),
        AS_LIST(params.value()), block, start->location());
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_parameters_list()
{
    TokenPtr open = current_token();
    Fa_VERIFY_TOKEN(TokType::LPAREN, ParserCode::EXPECTED_LPAREN);

    Fa_Array<ExprPtr> params = Fa_Array<ExprPtr>::with_capacity(4);

    if (!check(TokType::RPAREN)) {
        do {
            skip_newlines();
            if (check(TokType::RPAREN))
                break;

            if (!check(TokType::IDENTIFIER))
                return report_error(ParserCode::EXPECTED_PARAM_NAME);

            TokenPtr param_tok = current_token();
            advance();
            params.push(AST::Fa_make_name(param_tok->lexeme(), param_tok->location()));
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RPAREN));
    }

    Fa_VERIFY_TOKEN(TokType::RPAREN, ParserCode::EXPECTED_RPAREN_EXPR);

    // For an empty parameter list, use the '(' location (not the token after ')').
    Fa_SourceLocation loc = params.empty() ? open->location() : params[0]->get_location();

    return Fa_make_list(params, loc);
}

// Fa_Parser — expression parsers

Fa_ErrorOr<ExprPtr> Fa_Parser::parse() { return parse_expression(); }

// parse_expression is the public entry point; it delegates to parse_assignment_expr.
Fa_ErrorOr<ExprPtr> Fa_Parser::parse_expression() { return parse_assignment_expr(); }

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_assignment_expr()
{
    // LHS goes through the full expression hierarchy (via parse_conditional_expr
    // → parse_binary_expr_precedence).  The Pratt parser stops at '=' and
    // augmented-assignment tokens, leaving them for this function to handle.
    Fa_TRY(lhs, parse_conditional_expr());

    if (check(TokType::OP_ASSIGN) || is_augmented_assign_tok(current_token())) {
        ExprPtr target = lhs;
        AST::Fa_Expr::Kind kind = target->get_kind();

        if (kind != AST::Fa_Expr::Kind::NAME && kind != AST::Fa_Expr::Kind::INDEX && kind != AST::Fa_Expr::Kind::GET)
            return report_error(ParserCode::INVALID_ASSIGN_TARGET);

        if (is_augmented_assign_tok(current_token())) {
            TokenPtr op_tok = current_token();
            advance();
            Fa_TRY(rhs, parse_assignment_expr());
            AST::Fa_BinaryOp op = augmented_assign_to_binary_op(op_tok->type());
            auto* bin = AST::Fa_make_binary(lhs->clone(), rhs, op, lhs->get_location());
            return Fa_make_assignment_expr(target, bin, target->get_location());
        }

        advance(); // consume '='
        Fa_TRY(rhs, parse_assignment_expr());
        return Fa_make_assignment_expr(target, rhs, target->get_location());
    }

    return lhs;
}

// Unified Pratt parser

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_binary_expr_precedence(unsigned int min_prec)
{
    Fa_TRY(lhs, parse_unary_expr());

    for (;;) {
        TokenPtr cur = current_token();
        // Stop at non-binary-ops and at plain assignment (handled by parse_assignment_expr).
        if (!cur->is_binary_op() || cur->is(TokType::OP_ASSIGN))
            break;

        unsigned int prec = cur->get_precedence();
        if (prec == tok::PREC_NONE || prec < min_prec)
            break;

        TokType op_type = cur->type();
        advance();

        // OP_POWER is right-associative: pass `prec` (not `prec+1`) so the
        // recursive call accepts another power op of the same precedence.
        // All other operators are left-associative: pass `prec+1`.
        unsigned int next_min = (op_type == TokType::OP_POWER) ? prec : prec + 1;
        Fa_TRY(rhs, parse_binary_expr_precedence(next_min));

        // FIX: assign to lhs and CONTINUE the loop — do not return here.
        // Returning inside the loop was the root cause of the left-associativity bug.
        lhs = Fa_make_binary(lhs, rhs, to_binary_op(op_type), lhs->get_location());
    }

    return lhs;
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_unary_expr()
{
    TokenPtr op_tok = current_token();
    if (op_tok->is_unary_op()) {
        TokType op = op_tok->type();
        advance();
        Fa_TRY(operand, parse_unary_expr());
        // FIX: use the operator token's location (op_tok), not the operand's.
        // `!a` should report the location at `!`, not at `a`.
        return Fa_make_unary(operand, to_unary_op(op), op_tok->location());
    }
    return parse_postfix_expr();
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_postfix_expr()
{
    Fa_TRY(base, parse_primary_expr());
    ExprPtr expr = base;

    for (;;) {
        // Function call: expr(args...)
        if (check(TokType::LPAREN)) {
            advance();
            Fa_Array<ExprPtr> args = Fa_Array<ExprPtr>::with_capacity(4);

            if (!check(TokType::RPAREN)) {
                do {
                    skip_newlines();
                    if (check(TokType::RPAREN))
                        break;
                    Fa_TRY(arg, parse_expression());
                    args.push(arg);
                    skip_newlines();
                } while (match(TokType::COMMA) && !check(TokType::RPAREN));
            }

            Fa_VERIFY_TOKEN(TokType::RPAREN, ParserCode::EXPECTED_RPAREN_EXPR);
            Fa_SourceLocation loc = (!args.empty() && args[0])
                ? args[0]->get_location()
                : Fa_SourceLocation { };
            expr = Fa_make_call(
                expr, Fa_make_list(std::move(args), loc),
                expr ? expr->get_location() : Fa_SourceLocation { });
            continue;
        }

        // Subscript: expr[index]
        if (match(TokType::LBRACKET)) {
            Fa_TRY(index, parse_expression());
            Fa_VERIFY_TOKEN(TokType::RBRACKET, ParserCode::EXPECTED_RBRACKET);
            expr = Fa_make_index(
                expr, index,
                expr ? expr->get_location() : Fa_SourceLocation { });
            continue;
        }

        if (match(TokType::DOT)) {
            if (!check(TokType::IDENTIFIER))
                return report_error(ParserCode::EXPECTED_MEMBER_NAME);
            TokenPtr member_tok = current_token();
            advance();
            expr = Fa_make_get_expr(
                expr,
                AST::Fa_make_name(
                    member_tok->lexeme(),
                    member_tok->location()),
                expr ? expr->get_location() : Fa_SourceLocation { });
            continue;
        }

        break;
    }

    return expr;
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_primary_expr()
{
    TokenPtr cur = current_token();

    // Numeric literals
    if (cur->is_numeric()) {
        advance();
        TokType tt = cur->type();

        if (tt == TokType::DECIMAL)
            return AST::Fa_make_literal_float(cur->lexeme().to_double(), cur->location());

        int base = 10;
        switch (tt) {
        case TokType::BINARY: base = 2; break;
        case TokType::OCTAL: base = 8; break;
        case TokType::INTEGER: base = 10; break;
        case TokType::HEX: base = 16; break;
        default: break;
        }
        return AST::Fa_make_literal_int(
            util::parse_integer_literal(cur->lexeme(), base),
            cur->location());
    }

    if (match(TokType::STRING))
        return AST::Fa_make_literal_string(cur->lexeme(), cur->location());

    if (check(TokType::KW_TRUE) || check(TokType::KW_FALSE)) {
        bool val = cur->is(TokType::KW_TRUE);
        advance();
        return AST::Fa_make_literal_bool(val, cur->location());
    }

    if (match(TokType::KW_NIL))
        return AST::Fa_make_literal_nil(cur->location());

    if (match(TokType::KW_THIS))
        return AST::Fa_make_name(kClassInstanceName, cur->location());

    if (match(TokType::IDENTIFIER))
        return AST::Fa_make_name(cur->lexeme(), cur->location());

    if (match(TokType::LPAREN)) {
        if (match(TokType::RPAREN))
            return Fa_make_list(Fa_Array<ExprPtr> { }, cur->location());
        Fa_TRY(inner, parse_expression());
        Fa_VERIFY_TOKEN(TokType::RPAREN, ParserCode::EXPECTED_RPAREN_EXPR);
        return inner;
    }

    if (match(TokType::LBRACKET))
        return parse_list_literal();
    if (match(TokType::LBRACE))
        return parse_dict_literal();

    if (we_done())
        return report_error(ParserCode::UNEXPECTED_EOF, diagnostic::Severity::FATAL);

    skip_newlines();
    return report_error(ParserCode::UNEXPECTED_TOKEN, diagnostic::Severity::FATAL);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_list_literal()
{
    TokenPtr start = current_token();
    Fa_Array<ExprPtr> elements = Fa_Array<ExprPtr>::with_capacity(4);

    if (!check(TokType::RBRACKET)) {
        do {
            skip_newlines();
            if (check(TokType::RBRACKET))
                break;
            Fa_TRY(elem, parse_expression());
            elements.push(elem);
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RBRACKET));
    }

    Fa_VERIFY_TOKEN(TokType::RBRACKET, ParserCode::EXPECTED_RBRACKET);

    Fa_SourceLocation loc = elements.empty()
        ? start->location()
        : elements[0]->get_location();
    return Fa_make_list(std::move(elements), loc);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_dict_literal()
{
    TokenPtr start = current_token();
    Fa_Array<std::pair<ExprPtr, ExprPtr>> content;

    if (!check(TokType::RBRACE)) {
        do {
            skip_newlines();
            if (check(TokType::RBRACE))
                break;
            Fa_TRY(key, parse_expression());
            Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_DICT);
            Fa_TRY(val, parse_expression());
            content.push({ key, val });
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RBRACE));
    }

    Fa_VERIFY_TOKEN(TokType::RBRACE, ParserCode::EXPECTED_RBRACE_EXPR);
    Fa_SourceLocation loc = content.empty() ? start->location() : content[0].first->get_location();
    return AST::Fa_make_dict(std::move(content), loc);
}

// Compatibility stubs
/// TODO: remove these declarations from parser.hpp, then delete the stubs.

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_conditional_expr()
{
    // Entry point for the unified Pratt parser.  Ternary operator will be
    // inserted here once implemented (see the existing TODO in the design).
    return parse_binary_expr_precedence(0);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_logical_expr()
{
    return parse_binary_expr_precedence(0);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_logical_expr_precedence(unsigned int p)
{
    return parse_binary_expr_precedence(p);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_comparison_expr()
{
    return parse_binary_expr_precedence(0);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_binary_expr()
{
    return parse_binary_expr_precedence(0);
}

} // namespace fairuz::parser
