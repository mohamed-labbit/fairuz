//
// fparser.cc
//

#include "fparser.hpp"
#include "fAST.hpp"
#include "farray.hpp"
#include "fdiagnostic.hpp"
#include "ferror.hpp"
#include "flexer.hpp"
#include "fmacros.hpp"
#include "ftoken.hpp"
#include "futil.hpp"

namespace fairuz::parser {

// Macros

/// Consume a token, early-return the error code if the token doesn't match.
#define VERIFY_TOKEN(expected, errc)                  \
    do {                                              \
        if (UNLIKELY(!match(expected)))               \
            return report_error(errc, current_loc()); \
    } while (0)

/// Checking is different, it doesn't consume the current token
/// but force checking it to an expected type, return error if false.
#define CHECK_TOKEN(expected, errc)                   \
    do {                                              \
        if (UNLIKELY(!check(expected)))               \
            return report_error(errc, current_loc()); \
    } while (0)

// Propagate an error from an ErrorOr expression without unwrapping.
#define VERIFY_NODE(n)                 \
    do {                               \
        if (UNLIKELY((n).has_error())) \
            return (n).error();        \
    } while (0)

// Token-pasting helpers for unique temporary names (standard C++, no GNU extension).
#define FA_CONCAT_(a, b) a##b
#define FA_CONCAT(a, b) FA_CONCAT_(a, b)

// TRY — replaces the GNU ({...}) statement-expression macros.
//
// Declares `var` as the unwrapped value of `expr` (which must return
// ErrorOr<T>).  Early-returns the error from the *enclosing function* if
// any.  Each expansion creates a uniquely named temporary via __LINE__, so
// multiple TRY calls in the same scope are safe as long as they appear on
// different source lines (which they always should).
#define TRY(var, expr)                                      \
    auto FA_CONCAT(fa_try_, __LINE__) = (expr);             \
    if (UNLIKELY(FA_CONCAT(fa_try_, __LINE__).has_error())) \
        return FA_CONCAT(fa_try_, __LINE__).error();        \
    auto var = std::move(FA_CONCAT(fa_try_, __LINE__)).value()

// Type aliases

using TokType = tok::TokenType;

// Shared between the parser (parse_class_method) and the semantic analyser
// (analyze_stmt CLASS_DEF).  Move to ast_constants.hpp if the two components
// are ever split into separate translation units.
static constexpr char kClassInstanceName[] = "__class$instance";

// File-local helpers

namespace {

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
/// placed as methods on Token or in a token_ops.hpp utility header.

AST::ExprKind to_op(TokType const op, bool is_unary)
{
    switch (op) {
    case TokType::OP_PLUS: return is_unary ? AST::ExprKind::OP_PLUS : AST::ExprKind::OP_ADD;
    case TokType::OP_MINUS: return is_unary ? AST::ExprKind::OP_NEG : AST::ExprKind::OP_SUB;
    case TokType::OP_BITNOT: return AST::ExprKind::OP_BITNOT;
    case TokType::OP_NOT: return AST::ExprKind::OP_NOT;
    case TokType::OP_STAR: return AST::ExprKind::OP_MUL;
    case TokType::OP_SLASH: return AST::ExprKind::OP_DIV;
    case TokType::OP_PERCENT: return AST::ExprKind::OP_MOD;
    case TokType::OP_POWER: return AST::ExprKind::OP_POW;
    case TokType::OP_EQ: return AST::ExprKind::OP_EQ;
    case TokType::OP_NEQ: return AST::ExprKind::OP_NEQ;
    case TokType::OP_LT: return AST::ExprKind::OP_LT;
    case TokType::OP_GT: return AST::ExprKind::OP_GT;
    case TokType::OP_LTE: return AST::ExprKind::OP_LTE;
    case TokType::OP_GTE: return AST::ExprKind::OP_GTE;
    case TokType::OP_BITAND: return AST::ExprKind::OP_BITAND;
    case TokType::OP_BITOR: return AST::ExprKind::OP_BITOR;
    case TokType::OP_BITXOR: return AST::ExprKind::OP_BITXOR;
    case TokType::OP_LSHIFT: return AST::ExprKind::OP_LSHIFT;
    case TokType::OP_RSHIFT: return AST::ExprKind::OP_RSHIFT;
    case TokType::OP_AND: return AST::ExprKind::OP_AND;
    case TokType::OP_OR: return AST::ExprKind::OP_OR;
    case TokType::OP_PLUSEQ: return AST::ExprKind::OP_ADD;
    case TokType::OP_MINUSEQ: return AST::ExprKind::OP_SUB;
    case TokType::OP_STAREQ: return AST::ExprKind::OP_MUL;
    case TokType::OP_SLASHEQ: return AST::ExprKind::OP_DIV;
    case TokType::OP_PERCENTEQ: return AST::ExprKind::OP_MOD;
    case TokType::OP_ANDEQ: return AST::ExprKind::OP_BITAND;
    case TokType::OP_OREQ: return AST::ExprKind::OP_BITOR;
    case TokType::OP_XOREQ: return AST::ExprKind::OP_BITXOR;
    case TokType::OP_LSHIFTEQ: return AST::ExprKind::OP_LSHIFT;
    case TokType::OP_RSHIFTEQ: return AST::ExprKind::OP_RSHIFT;
    default: return AST::ExprKind::INVALID;
    }
}

// Parser — utilities

bool Parser::we_done() const { return current_token()->is(TokType::ENDMARKER); }

bool Parser::check(TokType t) const { return current_token()->is(t); }

TokenPtr Parser::current_token() const { return m_lexer.current(); }

bool Parser::match(TokType const type)
{
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

void Parser::synchronize()
{
    while (!we_done()) {
        if (check(TokType::DEDENT))
            return;

        if (check(TokType::NEWLINE)) {
            advance();
            // A malformed header can leave an orphaned suite. Skip the suite
            // as a unit instead of reporting one error per INDENT/body/DEDENT.
            if (check(TokType::INDENT)) {
                unsigned depth = 1;
                advance();
                while (depth && !we_done()) {
                    if (check(TokType::INDENT))
                        depth++;
                    if (check(TokType::DEDENT))
                        depth--;
                    advance();
                }
            }
            return;
        }

        advance();
    }
}
// Parser — top-level

Array<AST::StmtPtr> Parser::parse_program()
{
    Array<AST::StmtPtr> stmts;

    while (!we_done() && !diagnostic::is_saturated()) {
        skip_newlines();
        // No enclosing block owns a dedent at top level during recovery.
        if (check(TokType::DEDENT)) {
            advance();
            continue;
        }
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

ErrorOr<AST::StmtPtr> Parser::parse_statement()
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
    if (check(TokType::KW_IMPORT) || check(TokType::KW_FROM))
        return parse_import_stmt();
    if (check(TokType::KW_ASSERT))
        return parse_assert_stmt();

    return parse_expression_stmt();
}

// Parser — statement parsers

ErrorOr<AST::StmtPtr> Parser::parse_return_stmt()
{
    TokenPtr start = current_token();
    VERIFY_TOKEN(TokType::KW_RETURN, ErrorCode::EXPECTED_RETURN);

    if (check(TokType::NEWLINE) || we_done())
        return AST::make_return(start->location());

    TRY(ret, parse_expression());
    return AST::make_return(start->location(), ret);
}

ErrorOr<AST::StmtPtr> Parser::parse_break_stmt()
{
    TokenPtr start = current_token();
    advance();
    return AST::make_break(start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_continue_stmt()
{
    TokenPtr start = current_token();
    advance();
    return AST::make_continue(start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_while_stmt()
{
    TokenPtr start = current_token();
    VERIFY_TOKEN(TokType::KW_WHILE, ErrorCode::EXPECTED_WHILE_KEYWORD);

    TRY(condition, parse_expression());
    VERIFY_TOKEN(TokType::COLON, ErrorCode::EXPECTED_COLON_WHILE);

    auto while_block = parse_indented_block();
    VERIFY_NODE(while_block);

    return make_while(condition, as_block(while_block.value()), start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_for_stmt()
{
    TokenPtr start = current_token();
    VERIFY_TOKEN(TokType::KW_FOR, ErrorCode::UNEXPECTED_TOKEN);

    if (!check(TokType::IDENTIFIER))
        return report_error(ErrorCode::EXPECTED_FOR_TARGET, current_loc());

    auto* target = AST::make_identifier(current_token()->lexeme(), current_token()->location());
    advance();

    /// check 'in' after target
    VERIFY_TOKEN(TokType::KW_IN, ErrorCode::EXPECTED_IN_KEYWORD);

    TRY(iter, parse_expression());
    VERIFY_TOKEN(TokType::COLON, ErrorCode::EXPECTED_COLON_FOR);

    auto body = parse_indented_block();
    VERIFY_NODE(body);

    return AST::make_for(target, iter, body.value(), start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_if_stmt()
{
    TokenPtr start = current_token();
    VERIFY_TOKEN(TokType::KW_IF, ErrorCode::EXPECTED_IF_KEYWORD);

    TRY(condition, parse_expression());
    VERIFY_TOKEN(TokType::COLON, ErrorCode::EXPECTED_COLON_IF);

    auto then_block = parse_indented_block();
    VERIFY_NODE(then_block);

    AST::StmtPtr else_block = nullptr;
    skip_newlines();

    if (match(TokType::KW_ELSE)) {
        skip_newlines();
        if (check(TokType::KW_IF)) {
            // else-if: no colon between `else` and `if`.
            auto nested = parse_if_stmt();
            VERIFY_NODE(nested);
            else_block = nested.value();
        } else {
            VERIFY_TOKEN(TokType::COLON, ErrorCode::EXPECTED_COLON_IF);
            auto else_stmt = parse_indented_block();
            VERIFY_NODE(else_stmt);
            else_block = else_stmt.value();
        }
    }

    return make_if(condition, as_block(then_block.value()), start->location(), else_block);
}

ErrorOr<AST::StmtPtr> Parser::parse_expression_stmt()
{
    TRY(expr, parse_assignment_expr());
    if (UNLIKELY(!(check(TokType::NEWLINE) || check(TokType::DEDENT) || check(TokType::ENDMARKER))))
        return report_error(ErrorCode::UNEXPECTED_TOKEN, current_loc());
    return make_expr_stmt(expr, expr->get_location());
}

ErrorOr<AST::StmtPtr> Parser::parse_indented_block()
{
    TokenPtr start = current_token();
    skip_newlines();
    VERIFY_TOKEN(TokType::INDENT, ErrorCode::EXPECTED_INDENT);

    Array<AST::StmtPtr> stmts;

    if (match(TokType::DEDENT))
        return make_block(stmts, start->location());

    while (!check(TokType::DEDENT) && !we_done() && !diagnostic::is_saturated()) {
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

    if (check(TokType::ENDMARKER) || diagnostic::is_saturated())
        return make_block(stmts, start->location());

    VERIFY_TOKEN(TokType::DEDENT, ErrorCode::EXPECTED_DEDENT);
    return make_block(stmts, start->location());
}

// Parser — function and class parsers

ErrorOr<AST::StmtPtr> Parser::parse_function_def()
{
    TokenPtr start = current_token();
    VERIFY_TOKEN(TokType::KW_FN, ErrorCode::EXPECTED_FN_KEYWORD);

    if (!check(TokType::IDENTIFIER))
        return report_error(ErrorCode::EXPECTED_FN_NAME, current_loc());
    TokenPtr name_tok = current_token();
    advance();

    auto params = parse_parameters_list();
    VERIFY_NODE(params);

    VERIFY_TOKEN(TokType::COLON, ErrorCode::EXPECTED_COLON_FN);

    auto body = parse_indented_block();
    VERIFY_NODE(body);

    return make_function(
        AST::make_identifier(name_tok->lexeme(), name_tok->location()),
        { params.value() },
        as_block(body.value()),
        start->location());
}

/// Parse a class def of form 'class foo(Optional)', emit an error if not.
/// Members are parsed as an array of AST::ExprPtr.
/// Methods are parsed as an array of AST::StmtPtr.
/// Calling 'parse_class_method' to parse every defined method,
ErrorOr<AST::StmtPtr> Parser::parse_class_def()
{
    TokenPtr start = current_token();

    /// Discard 'class' keyword, and force check class name is specified
    VERIFY_TOKEN(TokType::KW_CLASS, ErrorCode::EXPECTED_CLASS_KEYWORD);
    CHECK_TOKEN(TokType::IDENTIFIER, ErrorCode::EXPECTED_CLASS_NAME);

    AST::ExprPtr class_name = AST::make_identifier(
        current_token()->lexeme(), current_loc());
    advance();

    /// allow for specifying parent class in the form of 'name(Parent)'
    AST::ExprPtr parent = nullptr;
    if (consume(TokType::LPAREN)) {
        if (!check(TokType::IDENTIFIER))
            return report_error(ErrorCode::EXPECTED_CLASS_NAME, current_loc());
        parent = AST::make_identifier(current_token()->lexeme(), current_loc());
        advance();
        VERIFY_TOKEN(TokType::RPAREN, ErrorCode::EXPECTED_RPAREN_CLASS);
    }

    VERIFY_TOKEN(TokType::COLON, ErrorCode::EXPECTED_COLON_CLASS);
    skip_newlines();
    VERIFY_TOKEN(TokType::INDENT, ErrorCode::EXPECTED_INDENT);

    /// Collect members and / or methods
    /// member values must be defined inside 'init' method
    /// member def outside is not supported as of now
    Array<AST::ExprPtr> members = Array<AST::ExprPtr>::with_capacity(4);
    Array<AST::StmtPtr> methods = Array<AST::StmtPtr>::with_capacity(4);

    while (!check(TokType::DEDENT) && !we_done()) {
        auto method = parse_class_method(members);
        if (method.has_value()) {
            methods.push(method.value());
        } else {
            /// Error recovery after method parsing
            /// keeps parsing until error LIMIT is reached
            if (diagnostic::is_saturated())
                break;
            synchronize();
            if (check(TokType::DEDENT) || we_done())
                break;
        }
    }

    if (check(TokType::ENDMARKER))
        return AST::make_class_def(class_name, parent, members, methods, start->location());

    VERIFY_TOKEN(TokType::DEDENT, ErrorCode::EXPECTED_DEDENT);
    return AST::make_class_def(class_name, parent, members, methods, start->location());
}

/// TODO: Make it parse multiple imported names inside parentheses
/// if we have a lot of imported names from one module, then we either
/// repeat the import stmt or write one very long line we have to scroll
/// horizontally through, some standard library modules do that now, especially
/// in test files, fix it as well once this is implemented.
ErrorOr<AST::StmtPtr> Parser::parse_import_stmt()
{
    TokenPtr start = current_token();
    bool const from_import = consume(TokType::KW_FROM);

    if (!from_import)
        VERIFY_TOKEN(TokType::KW_IMPORT, ErrorCode::EXPECTED_IMPORT_KEYWORD);

    if (!check(TokType::IDENTIFIER))
        return report_error(ErrorCode::EXPECTED_MODULE_NAME, current_loc());

    StringRef module = current_token()->lexeme();
    advance();

    while (consume(TokType::DOT)) {
        if (!check(TokType::IDENTIFIER))
            return report_error(ErrorCode::EXPECTED_MODULE_NAME, current_loc());
        module = module + "." + current_token()->lexeme();
        advance();
    }

    Array<StringRef> names;
    Array<StringRef> aliases;

    if (from_import) {
        VERIFY_TOKEN(TokType::KW_IMPORT, ErrorCode::EXPECTED_IMPORT_KEYWORD);

        do {
            if (!check(TokType::IDENTIFIER))
                return report_error(ErrorCode::EXPECTED_IMPORT_NAME, current_loc());

            StringRef name = current_token()->lexeme();
            advance();
            // Without `as`, the imported name is also its local name.
            StringRef alias = name;

            if (consume(TokType::KW_AS)) {
                if (!check(TokType::IDENTIFIER))
                    return report_error(ErrorCode::EXPECTED_ALIAS_NAME, current_loc());
                alias = current_token()->lexeme();
                advance();
            }

            names.push(name);
            aliases.push(alias);
        } while (consume(TokType::COMMA));
    } else {
        // By default, `import foo.bar` binds the final component: `bar`.
        std::string_view const module_view(module.data(), module.len());
        size_t const last_dot = module_view.find_last_of('.');
        StringRef alias = last_dot == std::string_view::npos ? module : module.slice(last_dot + 1, module.len());

        if (consume(TokType::KW_AS)) {
            if (!check(TokType::IDENTIFIER))
                return report_error(ErrorCode::EXPECTED_ALIAS_NAME, current_loc());
            alias = current_token()->lexeme();
            advance();
        }

        aliases.push(alias);
    }

    return AST::make_import(module, names, aliases, start->location());
}

ErrorOr<AST::StmtPtr> Parser::parse_assert_stmt()
{
    TokenPtr start = current_token();
    advance();
    TRY(condition, parse_expression());
    Array<AST::ExprPtr> args;
    args.push(condition);
    if (consume(TokType::COMMA)) {
        TRY(message, parse_expression());
        args.push(message);
    }
    auto* callee = AST::make_identifier("تاكد", start->location());
    auto* call = AST::make_call(callee, args, start->location());
    return AST::make_expr_stmt(call, start->location());
}

bool same_name(AST::ExprPtr e, StringRef const& n)
{
    return e != nullptr
        && AST::is_identifier(e)
        && AST::as_identifier(e)->spelling == n;
}

void push_member_once(Array<AST::ExprPtr>& members, AST::IdentifierExpr const* name)
{
    for (auto* member : members) {
        if (same_name(member, name->spelling))
            return;
    }
    members.push(AST::make_identifier(name->spelling, name->get_location()));
}

void collect_this_field_assignment(Array<AST::ExprPtr>& members, AST::StmtPtr stmt)
{
    if (stmt == nullptr)
        return;

    if (AST::is_block(stmt)) {
        for (auto* s : as_block(stmt)->stmts)
            collect_this_field_assignment(members, s);
        return;
    }

    if (AST::is_if(stmt)) {
        collect_this_field_assignment(members, as_if(stmt)->then_stmt);
        collect_this_field_assignment(members, as_if(stmt)->else_stmt);
        return;
    }

    if (AST::is_for(stmt)) {
        collect_this_field_assignment(members, as_for(stmt)->body);
        return;
    }

    if (AST::is_while(stmt)) {
        collect_this_field_assignment(members, as_while(stmt)->body);
        return;
    }

    if (!AST::is_expr(stmt))
        return;

    auto* expr = as_expr_stmt(stmt)->expr;

    if (AST::is_assignment(expr)) {
        auto* assign = as_assignment_expr(expr);
        auto* t = assign->target;
        if (!AST::is_get(t))
            return;
        auto* get = as_get(t);
        if (!same_name(get->object, kClassInstanceName))
            return; /// not of the form this.foo
        auto* mem = get->member;
        if (AST::is_identifier(mem)) {
            push_member_once(members, AST::as_identifier(mem));
            return;
        }
    }
}

ErrorOr<AST::StmtPtr> Parser::parse_class_method(Array<AST::ExprPtr>& members)
{
    TokenPtr start = current_token();
    VERIFY_TOKEN(TokType::KW_FN, ErrorCode::EXPECTED_FN_KEYWORD);

    TokenPtr name_tok = current_token();
    VERIFY_TOKEN(TokType::IDENTIFIER, ErrorCode::EXPECTED_FN_NAME);

    auto fn_name = name_tok->lexeme();
    auto cur = current_token()->lexeme();
    if (cur == "+" || cur == "-" || cur == "*" || cur == "/" || cur == "%" || cur == "٪") {
        if (fn_name == "عملية") {
            fn_name += cur;
            advance();
        }
        /// NOTE: do not raise an error here, parse_parameters_list() will take care of it
    }

    auto params = parse_parameters_list();
    VERIFY_NODE(params);

    VERIFY_TOKEN(TokType::COLON, ErrorCode::EXPECTED_COLON_FN);
    skip_newlines();
    VERIFY_TOKEN(TokType::INDENT, ErrorCode::EXPECTED_INDENT);

    Array<AST::StmtPtr> stmts;

    while (!check(TokType::DEDENT) && !we_done()) {
        skip_newlines();
        if (check(TokType::DEDENT))
            break;

        if (match(TokType::DOT)) {
            // `.field = expr` member-initializer syntax inside a method body.
            if (!check(TokType::IDENTIFIER))
                return report_error(ErrorCode::INVALID_ASSIGN_TARGET, current_loc());

            TokenPtr member_tok = current_token();
            StringRef mname = member_tok->lexeme();
            advance();

            // Desugar `.field` to a GET expression (instance.field), not an
            // INDEX_READ expression with a string key.  The compiler's fas
            // field-access path (compile_get_i / SET_FIELD) specifically looks
            // for GetExpr with a NAME member; an index form would silently
            // fall back to the slow dict-style path for every field access.
            AST::ExprPtr target = AST::make_get_expr(
                AST::make_identifier(kClassInstanceName, member_tok->location()),
                AST::make_identifier(mname, member_tok->location()),
                member_tok->location());

            AST::AssignExpr* member_assign = nullptr;

            if (check(TokType::OP_ASSIGN)) {
                advance();
                TRY(rhs, parse_assignment_expr(false));
                member_assign = AST::make_assignment_expr(target, rhs, member_tok->location());
            } else if (is_augmented_assign_tok(current_token())) {
                TokenPtr op_tok = current_token();
                advance();
                TRY(rhs, parse_expression());
                AST::ExprKind op = to_op(op_tok->type(), false);
                // target->clone() reads the current field value (GET read);
                // `target` itself is the write target.
                auto* bin = AST::make_binary(op, target->clone(), rhs, target->get_location());
                member_assign = AST::make_assignment_expr(target, bin, member_tok->location());
                member_assign->augmented = true;
            } else {
                return report_error(ErrorCode::INVALID_ASSIGN_TARGET, current_loc());
            }

            push_member_once(members, AST::make_identifier(mname, member_tok->location()));
            stmts.push(AST::make_expr_stmt(member_assign, member_tok->location()));
            continue;
        }

        // Regular statement inside the method body.
        TRY(s, parse_statement());
        collect_this_field_assignment(members, s);
        stmts.push(s);
    }

    AST::BlockStmt* block = AST::make_block(
        stmts,
        stmts.empty() ? start->location() : stmts[0]->get_location());

    if (check(TokType::ENDMARKER))
        return AST::make_function(
            AST::make_identifier(fn_name, name_tok->location()),
            { params.value() }, block, start->location());

    VERIFY_TOKEN(TokType::DEDENT, ErrorCode::EXPECTED_DEDENT);
    return AST::make_function(
        AST::make_identifier(fn_name, name_tok->location()),
        params.value().empty() ? Array<AST::ExprPtr> { } : params.value(), block, start->location());
}

ErrorOr<Array<AST::ExprPtr>> Parser::parse_parameters_list()
{
    VERIFY_TOKEN(TokType::LPAREN, ErrorCode::EXPECTED_LPAREN);
    Array<AST::ExprPtr> params = Array<AST::ExprPtr>::with_capacity(4);

    if (!check(TokType::RPAREN)) {
        do {
            skip_newlines();
            if (check(TokType::RPAREN))
                break;

            if (!check(TokType::IDENTIFIER))
                return report_error(ErrorCode::EXPECTED_PARAM_NAME, current_loc());

            TokenPtr param_tok = current_token();
            advance();
            params.push(AST::make_identifier(param_tok->lexeme(), param_tok->location()));
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RPAREN));
    }
    VERIFY_TOKEN(TokType::RPAREN, ErrorCode::EXPECTED_RPAREN_EXPR);
    return params;
}

// Parser — expression parsers

// Compatibility entry point for a single expression or assignment statement.
ErrorOr<AST::ExprPtr> Parser::parse() { return parse_assignment_expr(); }

ErrorOr<AST::ExprPtr> Parser::parse_expression()
{
    NestingLevel n { &m_nesting_level, current_loc() };
    TRY(expression, parse_conditional_expr());
    if (check(TokType::OP_ASSIGN) || is_augmented_assign_tok(current_token()))
        return report_error(ErrorCode::ASSIGNMENT_IN_EXPRESSION, current_loc());
    return expression;
}

ErrorOr<AST::ExprPtr> Parser::parse_assignment_expr(bool allow_augmented)
{
    NestingLevel n { &m_nesting_level, current_loc() };
    // LHS goes through the full expression hierarchy (via parse_conditional_expr
    // → parse_binary_expr_precedence).  The Pratt parser stops at ':=' and
    // augmented-assignment tokens, leaving them for this statement-level path.
    TRY(lhs, parse_conditional_expr());

    if (check(TokType::OP_ASSIGN) || is_augmented_assign_tok(current_token())) {
        AST::ExprPtr target = lhs;

        if (!AST::is_identifier(target) && !AST::is_index(target) && !AST::is_get(target))
            return report_error(ErrorCode::INVALID_ASSIGN_TARGET, current_loc());

        if (is_augmented_assign_tok(current_token())) {
            if (!allow_augmented)
                return report_error(ErrorCode::ASSIGNMENT_IN_EXPRESSION, current_loc());
            TokenPtr op_tok = current_token();
            advance();
            TRY(rhs, parse_expression());
            AST::ExprKind op = to_op(op_tok->type(), false);
            auto* bin = AST::make_binary(op, lhs->clone(), rhs, lhs->get_location());
            auto* assignment = make_assignment_expr(target, bin, target->get_location());
            assignment->augmented = true;
            return assignment;
        }

        advance(); // consume ':='; permit only bare plain-assignment chains
        TRY(rhs, parse_assignment_expr(false));
        return make_assignment_expr(target, rhs, target->get_location());
    }

    return lhs;
}

// Unified Pratt parser

ErrorOr<AST::ExprPtr> Parser::parse_binary_expr_precedence(u32 min_prec)
{
    NestingLevel n(&m_nesting_level, current_loc());
    TRY(lhs, parse_unary_expr());

    for (;;) {
        TokenPtr cur = current_token();
        // Stop at non-binary-ops and at plain assignment (handled by parse_assignment_expr).
        if (!cur->is_binary_op() || cur->is(TokType::OP_ASSIGN) || is_augmented_assign_tok(cur))
            break;

        u32 prec = cur->get_precedence();
        if (prec == tok::PREC_NONE || prec < min_prec)
            break;

        TokType op_type = cur->type();
        advance();

        // OP_POWER is right-associative: pass `prec` (not `prec+1`) so the
        // recursive call accepts another power op of the same precedence.
        // All other operators are left-associative: pass `prec+1`.
        u32 next_min = (op_type == TokType::OP_POWER) ? prec : prec + 1;
        TRY(rhs, parse_binary_expr_precedence(next_min));

        // FIX: assign to lhs and CONTINUE the loop — do not return here.
        // Returning inside the loop was the root cause of the left-associativity bug.
        lhs = make_binary(to_op(op_type, false), lhs, rhs, lhs->get_location());
    }

    return lhs;
}

ErrorOr<AST::ExprPtr> Parser::parse_unary_expr()
{
    NestingLevel n(&m_nesting_level, current_loc());
    TokenPtr op_tok = current_token();
    if (op_tok->is_unary_op()) {
        TokType op = op_tok->type();
        advance();
        TRY(operand, parse_unary_expr());
        // FIX: use the operator token's location (op_tok), not the operand's.
        // `!a` should report the location at `!`, not at `a`.
        return make_unary(to_op(op, true), operand, op_tok->location());
    }
    return parse_postfix_expr();
}

ErrorOr<AST::ExprPtr> Parser::parse_postfix_expr()
{
    NestingLevel n(&m_nesting_level, current_loc());
    TRY(base, parse_primary_expr());
    AST::ExprPtr expr = base;

    for (;;) {
        // Function call: expr(args...)
        if (check(TokType::LPAREN)) {
            advance();
            Array<AST::ExprPtr> args = Array<AST::ExprPtr>::with_capacity(4);

            if (!check(TokType::RPAREN)) {
                do {
                    skip_newlines();
                    if (check(TokType::RPAREN))
                        break;
                    TRY(arg, parse_expression());
                    args.push(arg);
                    skip_newlines();
                } while (match(TokType::COMMA) && !check(TokType::RPAREN));
            }

            VERIFY_TOKEN(TokType::RPAREN, ErrorCode::EXPECTED_RPAREN_EXPR);
            expr = make_call(
                expr, args,
                expr ? expr->get_location() : SourceLocation { });
            continue;
        }

        // Subscript: expr[index]
        if (match(TokType::LBRACKET)) {
            TRY(index, parse_expression());
            VERIFY_TOKEN(TokType::RBRACKET, ErrorCode::EXPECTED_RBRACKET);
            expr = make_index(
                expr, index,
                expr ? expr->get_location() : SourceLocation { });
            continue;
        }

        if (match(TokType::DOT)) {
            if (!check(TokType::IDENTIFIER))
                return report_error(ErrorCode::EXPECTED_MEMBER_NAME, current_loc());
            TokenPtr member_tok = current_token();
            advance();
            expr = make_get_expr(
                expr,
                AST::make_identifier(
                    member_tok->lexeme(),
                    member_tok->location()),
                expr ? expr->get_location() : SourceLocation { });
            continue;
        }

        break;
    }

    return expr;
}

ErrorOr<AST::ExprPtr> Parser::parse_primary_expr()
{
    NestingLevel n(&m_nesting_level, current_loc());
    TokenPtr cur = current_token();

    // Numeric literals
    if (cur->is_numeric()) {
        advance();
        TokType tt = cur->type();

        if (tt == TokType::DECIMAL)
            return AST::make_literal_float(cur->lexeme().to_double(), cur->location());

        int base = 10;
        switch (tt) {
        case TokType::BINARY: base = 2; break;
        case TokType::OCTAL: base = 8; break;
        case TokType::INTEGER: base = 10; break;
        case TokType::HEX: base = 16; break;
        default: break;
        }
        return AST::make_literal_int(
            util::parse_integer_literal(cur->lexeme(), base),
            cur->location());
    }

    if (match(TokType::STRING))
        return AST::make_literal_string(cur->lexeme(), cur->location());

    if (check(TokType::KW_TRUE) || check(TokType::KW_FALSE)) {
        bool val = cur->is(TokType::KW_TRUE);
        advance();
        return AST::make_literal_bool(val, cur->location());
    }

    if (match(TokType::KW_NIL))
        return AST::make_nil(cur->location());

    if (match(TokType::KW_THIS))
        return AST::make_identifier(kClassInstanceName, cur->location());

    if (match(TokType::IDENTIFIER))
        return AST::make_identifier(cur->lexeme(), cur->location());

    if (match(TokType::LPAREN)) {
        m_parenths.push_back(true);

        if (match(TokType::RPAREN)) {
            m_parenths.pop_back();
            return make_list(Array<AST::ExprPtr> { }, cur->location());
        }

        Array<AST::ExprPtr> elements { };
        bool trailing_comma = false;

        do {
            if (check(TokType::RPAREN))
                break;

            auto inner = parse_expression();
            VERIFY_NODE(inner);
            elements.push(inner.value());

            if (!match(TokType::COMMA)) {
                trailing_comma = false;
                break;
            }
            trailing_comma = true;
        } while (true);

        if (!match(TokType::RPAREN)) {
            m_parenths.pop_back();
            return report_error(ErrorCode::EXPECTED_RPAREN_EXPR, current_loc());
        }
        m_parenths.pop_back();
        if (elements.size() == 1 && !trailing_comma)
            return elements[0];
        return make_list(std::move(elements), cur->location());
    }

    if (match(TokType::LBRACKET))
        return parse_list_literal();
    if (match(TokType::LBRACE))
        return parse_dict_literal();

    if (we_done())
        return report_error(ErrorCode::UNEXPECTED_EOF, current_loc());

    return report_error(ErrorCode::UNEXPECTED_TOKEN, current_loc());
}

ErrorOr<AST::ExprPtr> Parser::parse_list_literal()
{
    NestingLevel n(&m_nesting_level, current_loc());
    TokenPtr start = current_token();
    Array<AST::ExprPtr> elements = Array<AST::ExprPtr>::with_capacity(4);

    if (!check(TokType::RBRACKET)) {
        do {
            skip_newlines();
            if (check(TokType::RBRACKET))
                break;
            TRY(elem, parse_expression());
            elements.push(elem);
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RBRACKET));
    }

    VERIFY_TOKEN(TokType::RBRACKET, ErrorCode::EXPECTED_RBRACKET);
    return make_list(std::move(elements), start->location());
}

ErrorOr<AST::ExprPtr> Parser::parse_dict_literal()
{
    NestingLevel n(&m_nesting_level, current_loc());
    TokenPtr start = current_token();
    Array<std::pair<AST::ExprPtr, AST::ExprPtr>> content;

    if (!check(TokType::RBRACE)) {
        do {
            skip_newlines();
            if (check(TokType::RBRACE))
                break;
            TRY(key, parse_expression());
            VERIFY_TOKEN(TokType::COLON, ErrorCode::EXPECTED_COLON_DICT);
            TRY(val, parse_expression());
            content.push({ key, val });
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RBRACE));
    }

    VERIFY_TOKEN(TokType::RBRACE, ErrorCode::EXPECTED_RBRACE_EXPR);
    return AST::make_dict(std::move(content), start->location());
}

// Compatibility stubs
/// TODO: remove these declarations from parser.hpp, then delete the stubs.

ErrorOr<AST::ExprPtr> Parser::parse_conditional_expr()
{
    // Entry point for the unified Pratt parser.  Ternary operator will be
    // inserted here once implemented (see the existing TODO in the design).
    return parse_binary_expr_precedence(0);
}

ErrorOr<AST::ExprPtr> Parser::parse_logical_expr()
{
    return parse_binary_expr_precedence(0);
}

ErrorOr<AST::ExprPtr> Parser::parse_logical_expr_precedence(u32 p)
{
    return parse_binary_expr_precedence(p);
}

ErrorOr<AST::ExprPtr> Parser::parse_comparison_expr()
{
    return parse_binary_expr_precedence(0);
}

ErrorOr<AST::ExprPtr> Parser::parse_binary_expr()
{
    return parse_binary_expr_precedence(0);
}

} // namespace fairuz::parser
