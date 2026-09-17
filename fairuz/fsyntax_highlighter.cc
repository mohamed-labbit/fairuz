#include "fsyntax_highlighter.hpp"

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

    void target(AST::Expr* expression)
    {
        if (!expression)
            return;
        if (expression->get_kind() == AST::Expr::Kind::NAME) {
            mark(expression, "variable", true);
        } else if (expression->get_kind() == AST::Expr::Kind::GET) {
            auto* get = static_cast<AST::GetExpr*>(expression);
            expr(get->get_object());
            mark(get->get_member(), "property", true);
        } else {
            expr(expression);
        }
    }

    void expr(AST::Expr* expression)
    {
        if (!expression)
            return;
        switch (expression->get_kind()) {
        case AST::Expr::Kind::BINARY: {
            auto* value = static_cast<AST::BinaryExpr*>(expression);
            expr(value->get_left());
            expr(value->get_right());
            break;
        }
        case AST::Expr::Kind::UNARY:
            expr(static_cast<AST::UnaryExpr*>(expression)->get_operand());
            break;
        case AST::Expr::Kind::CALL: {
            auto* call = static_cast<AST::CallExpr*>(expression);
            if (call->get_callee()->get_kind() == AST::Expr::Kind::NAME)
                mark(call->get_callee(), "function");
            else if (call->get_callee()->get_kind() == AST::Expr::Kind::GET) {
                auto* get = static_cast<AST::GetExpr*>(call->get_callee());
                expr(get->get_object());
                mark(get->get_member(), "method");
            } else
                expr(call->get_callee());
            for (AST::Expr* argument : call->get_args())
                expr(argument);
            break;
        }
        case AST::Expr::Kind::ASSIGNMENT: {
            auto* assignment = static_cast<AST::AssignmentExpr*>(expression);
            target(assignment->get_target());
            expr(assignment->get_value());
            break;
        }
        case AST::Expr::Kind::LIST:
            for (AST::Expr* item : static_cast<AST::ListExpr*>(expression)->get_elements())
                expr(item);
            break;
        case AST::Expr::Kind::DICT:
            for (auto const& item : static_cast<AST::DictExpr*>(expression)->get_content()) {
                expr(item.first);
                expr(item.second);
            }
            break;
        case AST::Expr::Kind::INDEX_READ: {
            auto* index = static_cast<AST::IndexExpr*>(expression);
            expr(index->get_object());
            expr(index->get_index());
            break;
        }
        case AST::Expr::Kind::GET: {
            auto* get = static_cast<AST::GetExpr*>(expression);
            expr(get->get_object());
            mark(get->get_member(), "property");
            break;
        }
        default: break;
        }
    }

    void function(AST::FunctionDef* value, bool method)
    {
        mark(value->get_name(), method ? "method" : "function", true);
        for (AST::Expr* parameter : value->get_parameters())
            mark(parameter, "parameter", true);
        stmt(value->get_body(), false);
    }

    void stmt(AST::Stmt* statement, bool class_member)
    {
        if (!statement)
            return;
        switch (statement->get_kind()) {
        case AST::Stmt::Kind::BLOCK:
            for (AST::Stmt* child : static_cast<AST::BlockStmt*>(statement)->get_statements())
                stmt(child, class_member);
            break;
        case AST::Stmt::Kind::EXPR: expr(static_cast<AST::ExprStmt*>(statement)->get_expr()); break;
        case AST::Stmt::Kind::ASSIGNMENT: {
            auto* assignment = static_cast<AST::AssignmentStmt*>(statement);
            target(assignment->get_target());
            expr(assignment->get_expr()->get_value());
            break;
        }
        case AST::Stmt::Kind::IF: {
            auto* value = static_cast<AST::IfStmt*>(statement);
            expr(value->get_condition());
            stmt(value->get_then(), false);
            stmt(value->get_else(), false);
            break;
        }
        case AST::Stmt::Kind::WHILE: {
            auto* value = static_cast<AST::WhileStmt*>(statement);
            expr(value->get_condition());
            stmt(value->get_body(), false);
            break;
        }
        case AST::Stmt::Kind::FOR: {
            auto* value = static_cast<AST::ForStmt*>(statement);
            mark(value->get_target(), "variable", true);
            expr(value->get_iter());
            stmt(value->get_body(), false);
            break;
        }
        case AST::Stmt::Kind::FUNC: function(static_cast<AST::FunctionDef*>(statement), class_member); break;
        case AST::Stmt::Kind::RETURN: expr(static_cast<AST::ReturnStmt*>(statement)->get_value()); break;
        case AST::Stmt::Kind::CLASS_DEF: {
            auto* value = static_cast<AST::ClassDef*>(statement);
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
