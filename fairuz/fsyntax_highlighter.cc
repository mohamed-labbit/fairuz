#include "fsyntax_highlighter.hpp"

#include "fAST.hpp"
#include "fAST_printer.hpp"
#include "fctype.hpp"
#include "fdiagnostic.hpp"
#include "flexer.hpp"
#include "fparser.hpp"
#include "ftoken.hpp"
#include "futil.hpp"

#include <algorithm>
#include <unordered_map>

namespace fairuz::syntax {
namespace {

using ExprKind = AST::ExprKind;

struct Span {
    u64 offset { 0 };
    Token token;
    tok::TokenType lexical_type { tok::TokenType::INVALID };
    std::string text;
};

u32 utf16_width(u32 cp) { return cp > 0xffff ? 2 : 1; }

class Scanner {
public:
    explicit Scanner(StringRef const& source)
        : m_source(source)
    {
    }

    std::vector<Span> run()
    {
        while (m_offset < m_source.len()) {
            u64 start = m_offset;
            u32 start_line = m_line;
            u32 start_column = m_column;
            u32 cp = current();
            if (cp == '\n') {
                advance();
                m_line++;
                m_column = 0;
                continue;
            }
            if (cp == '#') {
                while (m_offset < m_source.len() && current() != '\n')
                    advance();
                add(start, start_line, start_column, "comment", tok::TokenType::INVALID);
                continue;
            }
            if (cp == '\'' || cp == '"') {
                u32 quote = cp;
                advance();
                bool escaped = false;
                while (m_offset < m_source.len()) {
                    u32 ch = current();
                    if (ch == '\n')
                        break;
                    advance();
                    if (!escaped && ch == quote)
                        break;
                    if (!escaped && ch == '\\')
                        escaped = true;
                    else
                        escaped = false;
                }
                add(start, start_line, start_column, "string", tok::TokenType::STRING);
                continue;
            }
            if (IS_IDENT_S(cp)) {
                advance();
                while (m_offset < m_source.len() && IS_IDENT_C(current()))
                    advance();
                StringRef word = m_source.slice(start, m_offset);
                auto keyword = tok::lookup_keyword(word);
                tok::TokenType kind = keyword.value_or(tok::TokenType::NAME);
                std::string type = "variable";
                if (kind == tok::TokenType::KW_TRUE || kind == tok::TokenType::KW_FALSE)
                    type = "boolean";
                else if (kind == tok::TokenType::KW_NIL)
                    type = "null";
                else if (keyword.has_value())
                    type = "keyword";
                add(start, start_line, start_column, type.c_str(), kind);
                continue;
            }
            if (IS_DIGIT(cp)) {
                advance();
                if (cp == '0' && m_offset < m_source.len()
                    && (current() == 'x' || current() == 'X'
                        || current() == 'o' || current() == 'O'
                        || current() == 'b' || current() == 'B')) {
                    advance();
                    while (m_offset < m_source.len()
                        && (IS_XDIGIT(current()) || current() == '_'))
                        advance();
                } else {
                    while (m_offset < m_source.len()
                        && (IS_DIGIT(current()) || current() == '_'))
                        advance();
                    if (m_offset < m_source.len() && current() == '.') {
                        advance();
                        while (m_offset < m_source.len()
                            && (IS_DIGIT(current()) || current() == '_'))
                            advance();
                    }
                }
                add(start, start_line, start_column, "number", tok::TokenType::INTEGER);
                continue;
            }
            if (is_operator(cp)) {
                advance();
                while (m_offset < m_source.len() && is_operator(current()))
                    advance();
                add(start, start_line, start_column, "operator", tok::TokenType::OP_PLUS);
                continue;
            }
            advance();
        }
        classify_imports();
        return m_spans;
    }

private:
    StringRef const& m_source;
    u64 m_offset { 0 };
    u32 m_line { 0 };
    u32 m_column { 0 };
    std::vector<Span> m_spans;

    u32 current() const
    {
        u64 bytes = 0;
        return util::decode_utf8_at(m_source, m_offset, &bytes);
    }

    void advance()
    {
        u64 bytes = 0;
        u32 cp = util::decode_utf8_at(m_source, m_offset, &bytes);
        m_offset += bytes;
        m_column += utf16_width(cp);
    }

    static bool is_operator(u32 cp)
    {
        return cp == '+' || cp == '-' || cp == '*' || cp == '/' || cp == '%'
            || cp == 0x066a || cp == '&' || cp == '|' || cp == '^' || cp == '~'
            || cp == '<' || cp == '>' || cp == '=' || cp == '!' || cp == ':';
    }

    void add(u64 offset, u32 line, u32 column, char const* type, tok::TokenType lexical)
    {
        Span span;
        span.offset = offset;
        span.token = { line, column, m_column - column, type, false };
        span.lexical_type = lexical;
        StringRef raw = m_source.slice(offset, m_offset);
        span.text.assign(raw.data(), raw.len());
        m_spans.push_back(std::move(span));
    }

    void classify_imports()
    {
        enum class State { None,
            FromModule,
            DirectModule,
            Member,
            Alias } state = State::None;
        bool from_import = false;
        Token* direct_binding = nullptr;
        u32 line = 0;
        for (Span& span : m_spans) {
            if (span.token.line != line) {
                line = span.token.line;
                state = State::None;
                from_import = false;
                direct_binding = nullptr;
            }
            switch (span.lexical_type) {
            case tok::TokenType::KW_FROM:
                from_import = true;
                state = State::FromModule;
                break;
            case tok::TokenType::KW_IMPORT:
                state = from_import ? State::Member : State::DirectModule;
                break;
            case tok::TokenType::KW_AS:
                state = State::Alias;
                break;
            case tok::TokenType::NAME:
                if (state == State::FromModule) {
                    span.token.type = "namespace";
                } else if (state == State::DirectModule) {
                    if (direct_binding)
                        direct_binding->declaration = false;
                    span.token.type = "namespace";
                    span.token.declaration = true;
                    direct_binding = &span.token;
                } else if (state == State::Member) {
                    span.token.type = "variable";
                    span.token.declaration = true;
                    state = State::None;
                } else if (state == State::Alias) {
                    if (direct_binding)
                        direct_binding->declaration = false;
                    span.token.type = from_import ? "variable" : "namespace";
                    span.token.declaration = true;
                    state = State::None;
                }
                break;
            default: break;
            }
        }
    }
};

class AstClassifier {
public:
    explicit AstClassifier(std::unordered_map<u64, Token*>& tokens)
        : m_tokens(tokens)
    {
    }

    void program(Array<AST::Stmt*> const& statements)
    {
        for (AST::Stmt* statement : statements)
            stmt(statement, false);
    }

private:
    std::unordered_map<u64, Token*>& m_tokens;

    void mark(AST::ASTNode const* node, char const* type, bool declaration = false)
    {
        if (!node)
            return;
        auto found = m_tokens.find(node->get_location().offset);
        if (found == m_tokens.end())
            return;
        found->second->type = type;
        found->second->declaration = declaration;
    }

    void target(AST::Expr const* expression)
    {
        if (!expression)
            return;
        if (AST::is_identifier(expression)) {
            mark(expression, "variable", true);
        } else if (expression->get_kind() == ExprKind::GET) {
            auto* get = AST::as_get(expression);
            expr(get->object);
            mark(get->member, "property", true);
        } else {
            expr(expression);
        }
    }

    void expr(AST::Expr const* expression)
    {
        if (!expression)
            return;
        switch (expression->get_kind()) {
        case ExprKind::OP_ADD:
        case ExprKind::OP_SUB:
        case ExprKind::OP_MUL:
        case ExprKind::OP_DIV:
        case ExprKind::OP_MOD:
        case ExprKind::OP_POW:
        case ExprKind::OP_EQ:
        case ExprKind::OP_NEQ:
        case ExprKind::OP_LT:
        case ExprKind::OP_GT:
        case ExprKind::OP_LTE:
        case ExprKind::OP_GTE:
        case ExprKind::OP_BITAND:
        case ExprKind::OP_BITOR:
        case ExprKind::OP_BITXOR:
        case ExprKind::OP_LSHIFT:
        case ExprKind::OP_RSHIFT:
        case ExprKind::OP_AND:
        case ExprKind::OP_OR: {
            auto* value = AST::as_binary(expression);
            expr(value->lhs);
            expr(value->rhs);
            break;
        }
        case ExprKind::OP_PLUS:
        case ExprKind::OP_NEG:
        case ExprKind::OP_BITNOT:
        case ExprKind::OP_NOT:
            expr(AST::as_unary(expression)->operand);
            break;
        case ExprKind::CALL: {
            auto* call = AST::as_call(expression);
            if (AST::is_identifier(call->callee))
                mark(call->callee, "function");
            else if (call->callee->get_kind() == ExprKind::GET) {
                auto* get = AST::as_get(call->callee);
                expr(get->object);
                mark(get->member, "method");
            } else
                expr(call->callee);
            for (AST::Expr* argument : call->args->elements)
                expr(argument);
            break;
        }
        case ExprKind::ASSIGNMENT: {
            auto* assignment = AST::as_assignment_expr(expression);
            target(assignment->target);
            expr(assignment->value);
            break;
        }
        case ExprKind::LIST:
            for (AST::Expr* item : AST::as_list(expression)->elements)
                expr(item);
            break;
        case ExprKind::DICT:
            for (auto const& item : AST::as_dict(expression)->get_content()) {
                expr(item.first);
                expr(item.second);
            }
            break;
        case ExprKind::INDEX_READ: {
            auto* index = AST::as_index(expression);
            expr(index->object);
            expr(index->index);
            break;
        }
        case ExprKind::GET: {
            auto* get = AST::as_get(expression);
            expr(get->object);
            mark(get->member, "property");
            break;
        }
        default: break;
        }
    }

    void function(AST::FunctionDef const* value, bool method)
    {
        mark(value->name, method ? "method" : "function", true);
        for (AST::Expr* parameter : value->params->elements)
            mark(parameter, "parameter", true);
        stmt(value->body, false);
    }

    void stmt(AST::Stmt const* statement, bool class_member)
    {
        if (!statement)
            return;
        switch (statement->get_kind()) {
        case AST::Stmt::Kind::BLOCK:
            for (AST::Stmt* child : AST::as_block(statement)->stmts)
                stmt(child, class_member);
            break;
        case AST::Stmt::Kind::EXPR: expr(AST::as_expr_stmt(statement)->expr); break;
        case AST::Stmt::Kind::IF: {
            auto* value = AST::as_if(statement);
            expr(value->condition);
            stmt(value->then_stmt, false);
            stmt(value->else_stmt, false);
            break;
        }
        case AST::Stmt::Kind::WHILE: {
            auto* value = AST::as_while(statement);
            expr(value->condition);
            stmt(value->body, false);
            break;
        }
        case AST::Stmt::Kind::FOR: {
            auto* value = AST::as_for(statement);
            mark(value->container, "variable", true);
            expr(value->iter);
            stmt(value->body, false);
            break;
        }
        case AST::Stmt::Kind::FUNC: function(AST::as_function_def(statement), class_member); break;
        case AST::Stmt::Kind::RETURN: expr(AST::as_return(statement)->value); break;
        case AST::Stmt::Kind::CLASS_DEF: {
            auto* value = AST::as_class_def(statement);
            mark(value->get_name(), "class", true);
            mark(value->get_parent(), "class");
            for (AST::Expr* member : value->get_members())
                target(member);
            for (AST::Stmt* method : value->get_methods())
                stmt(method, true);
            break;
        }
        default: break;
        }
    }
};

} // namespace

Result Highlighter::highlight(StringRef const& source)
{
    std::vector<Span> spans = Scanner(source).run();
    Result result;
    std::unordered_map<u64, Token*> by_offset;
    for (Span& span : spans)
        if (span.lexical_type == tok::TokenType::NAME)
            by_offset[span.offset] = &span.token;

    diagnostic::reset();
    lex::FileManager file;
    file.buffer() = source;
    diagnostic::set_source(&file);
    try {
        parser::Parser parser(&file);
        Array<AST::Stmt*> statements = parser.parse_program();
        result.ast_valid = !diagnostic::has_errors();
        AstClassifier(by_offset).program(statements);
    } catch (diagnostic::DiagnosticAbort const&) {
        result.ast_valid = false;
    }
    diagnostic::reset();

    result.tokens.reserve(spans.size());
    for (Span& span : spans)
        if (span.token.length > 0)
            result.tokens.push_back(std::move(span.token));
    std::sort(result.tokens.begin(), result.tokens.end(), [](Token const& a, Token const& b) {
        return a.line != b.line ? a.line < b.line : a.start < b.start;
    });
    return result;
}

} // namespace fairuz::syntax
