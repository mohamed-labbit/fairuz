/// lexer.cc

#include "../include/lexer.hpp"
#include "../include/ctype.hpp"
#include "../include/util.hpp"
#include "token.hpp"

#include <fstream>

#define CONSUME_BASE_DIGITS(valid_fa_expr, token_type, err_code, detail) \
    do {                                                                 \
        number += util::encode_utf8_str(m_next);                         \
        m_current = m_source_manager.next_char();                        \
        bool any = false;                                                \
        for (;;) {                                                       \
            if (m_current == '_') {                                      \
                m_current = m_source_manager.next_char();                \
                continue;                                                \
            }                                                            \
            if (!(valid_fa_expr))                                        \
                break;                                                   \
            number += util::encode_utf8_str(m_current);                  \
            m_current = m_source_manager.next_char();                    \
            any = true;                                                  \
        }                                                                \
        if (!any)                                                        \
            diagnostic::panic(err_code, detail);                         \
        return finish(token_type, number, m_line, col);                  \
    } while (0)

#define OCTAL_DIGIT(c)                                                                                   \
    ((c) >= '0' && (c) <= '7'                                                                            \
            ? true                                                                                       \
            : (IS_DIGIT(c)                                                                               \
                      ? (diagnostic::panic(diagnostic::errc::m_lexer::Code::INVALID_OCTAL_DIGIT), false) \
                      : false))

#define BINARY_DIGIT(c)                                                                                   \
    ((c) == '0' || (c) == '1'                                                                             \
            ? true                                                                                        \
            : (IS_DIGIT(c)                                                                                \
                      ? (diagnostic::panic(diagnostic::errc::m_lexer::Code::INVALID_BINARY_DIGIT), false) \
                      : false))

#define FINISH(tt, str, m_line, col)                                    \
    ({                                                                  \
        const tok::Fa_Token* _token = MAKE_TOKEN(tt, str, m_line, col); \
        store(_token);                                                  \
        m_tok_stream.back();                                            \
    })

namespace fairuz::lex {

namespace fs = std::filesystem;

Fa_FileManager::Fa_FileManager(std::string const& filepath)
    : m_file_path(filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in)
        diagnostic::panic(diagnostic::errc::m_lexer::Code::FILE_NOT_OPEN, filepath);

    std::string content { std::istreambuf_iterator<char> { in },
        std::istreambuf_iterator<char> { } };
    m_input_buffer = Fa_StringRef(content.data());
    m_input_buffer.trim_whitespace(false, true);
    m_last_known_write_time = fs::last_write_time(filepath);
}

Fa_StringRef Fa_FileManager::load(std::string const& filepath, bool replace)
{
    if (filepath.empty())
        return "";

    std::ifstream in(filepath, std::ios::binary);
    Fa_StringRef ret = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()).data();
    if (!in)
        diagnostic::panic(diagnostic::errc::m_lexer::Code::FILE_NOT_OPEN, filepath);

    if (replace || m_file_path.empty()) {
        m_input_buffer = ret;
        m_file_path = filepath;
        m_last_known_write_time = fs::last_write_time(filepath);
    }

    return ret;
}

void Fa_SourceManager::reset()
{
    m_context.m_line = 1;
    m_context.m_column = 1;
    m_context.m_offset = 0;
    m_current = 0;

    while (!m_unget_stack.empty())
        m_unget_stack.pop();

    if (m_file_manager && m_file_manager->buffer().len() > 0) {
        u64 bytes = 0;
        u32 cp = util::decode_utf8_at(m_file_manager->buffer(), 0, &bytes);
        m_current = cp;
        m_current_bytes = bytes;
    }
}

u32 Fa_SourceManager::peek_char()
{
    if (!m_unget_stack.empty())
        return m_unget_stack.top().ch;

    Context saved = m_context;
    u32 saved_current = m_current;
    u32 cp = next_char();

    if (cp != 0) {
        unget(cp);
    } else {
        m_context = saved;
        m_current = saved_current;
    }

    return cp;
}

u32 Fa_SourceManager::current_char() const
{
    if (m_context.m_offset >= m_file_manager->buffer().len())
        return 0;

    u64 bytes = 0;
    return util::decode_utf8_at(m_file_manager->buffer(), m_context.m_offset, &bytes);
}

void Fa_SourceManager::consume_char()
{
    if (m_context.m_offset >= m_file_manager->buffer().len())
        return;

    u64 bytes = 0;
    u32 cp = util::decode_utf8_at(m_file_manager->buffer(), m_context.m_offset, &bytes);
    advance(cp, bytes);
}

u32 Fa_SourceManager::next_char()
{
    consume_char();
    return current_char();
}

void Fa_SourceManager::unget(u32 const cp)
{
    PushbackEntry e = {
        .ch = cp,
        .ctx = m_context,
        .bytes = m_current_bytes
    };

    rewind_position_(cp, e.bytes);
    m_unget_stack.push(e);
}

void Fa_SourceManager::advance(u32 const cp, u64 const bytes)
{
    m_context.m_offset += bytes;

    if (cp == '\n') {
        m_context.m_line += 1;
        m_context.m_column = 1;
    } else {
        m_context.m_column += 1;
    }
}

void Fa_SourceManager::rewind_position_(u32 const cp, u64 const bytes)
{
    if (m_context.m_offset < bytes)
        throw std::runtime_error("Fa_SourceManager: attempted to rewind past beginning of file");

    m_context.m_offset -= bytes;

    if (cp == '\n') {
        m_context.m_line = (m_context.m_line > 1) ? (m_context.m_line - 1) : 1;
        m_context.m_column = calculate_column_at_offset(m_context.m_offset);
    } else {
        m_context.m_column = (m_context.m_column > 1) ? (m_context.m_column - 1) : 1;
    }
}

u32 Fa_SourceManager::calculate_column_at_offset(u64 const target_offset) const
{
    Fa_StringRef const& buf = m_file_manager->buffer();

    u64 line_start = target_offset;
    while (line_start > 0) {
        if (buf.data()[line_start - 1] == '\n')
            break;

        line_start -= 1;
    }

    u32 m_column = 1;
    u64 pos = line_start;

    while (pos < target_offset) {
        u64 bytes = 0;
        util::decode_utf8_at(buf, pos, &bytes);
        pos += bytes;
        m_column += 1;
    }

    return m_column;
}

Fa_Lexer::Fa_Lexer(Fa_FileManager* fm)
    : m_source_manager(fm)
    , m_tok_index(0)
    , m_indent_size(4)
    , m_indent_level(0)
    , m_at_bol(true)
{
    m_tok_stream = Fa_Array<fairuz::tok::Fa_Token const*>::with_capacity(1024);
    m_indent_stack = Fa_Array<unsigned int>::with_capacity(8);
    m_alt_indent_stack = Fa_Array<unsigned int>::with_capacity(8);
    m_indent_stack.push(0);
    m_alt_indent_stack.push(0);

    util::configure_locale();
}

Fa_Lexer::Fa_Lexer(Fa_Array<tok::Fa_Token const*>& seq /*, size_t const s*/)
    : m_tok_stream(seq)
    , m_tok_index(0)
    , m_indent_size(4)
    , m_indent_level(0)
    , m_at_bol(true)
{
    m_indent_stack.push(0);
    m_alt_indent_stack.push(0);
    util::configure_locale();
}

tok::Fa_Token const* Fa_Lexer::lex_token()
{
    auto finish = [this](tok::Fa_TokenType tt, Fa_StringRef str, size_t m_line, size_t col) {
        tok::Fa_Token const* ret = MAKE_TOKEN(tt, str, m_line, col);
        store(ret);
        return m_tok_stream.back();
    };

    if (m_tok_stream.empty())
        return FINISH(tok::Fa_TokenType::BEGINMARKER, "", 1, 1);

    auto next_line = [this](u32 m_line, u32 col) {
        if (!m_at_bol)
            return;

        m_at_bol = false;

        unsigned int m_size = 0;
        unsigned int alt_size = 0;
        unsigned int cont_line_col = 0;

        u32 m_current = m_source_manager.current_char();

        // count leading whitespace
        for (;;) {
            if (m_current == ' ') {
                m_source_manager.consume_char();
                m_size += 1;
                alt_size += 1;
            } else if (m_current == '\t') {
                m_source_manager.consume_char();
                m_size = (m_size / m_indent_size + 1) * m_indent_size;
                alt_size = (alt_size / TABWIDTH + 1) * TABWIDTH;
            } else if (m_current == '\f') {
                // form-feed resets the column counter — handled explicitly,
                // NOT via ml_is_space, because it has special indent semantics.
                m_source_manager.consume_char();
                m_size = alt_size = 0;
            } else if (m_current == '\\') {
                m_source_manager.consume_char();
                if (!cont_line_col)
                    cont_line_col = m_size;
            } else {
                break;
            }

            m_current = m_source_manager.current_char();
        }

        // blank
        if (m_current == '#' || m_current == '\n' || m_current == '\r')
            return;
        if (m_current == 0)
            return;

        if (cont_line_col)
            m_size = alt_size = cont_line_col;

        if (m_size == m_indent_stack.back()) {
            if (alt_size != m_alt_indent_stack.back())
                diagnostic::panic(diagnostic::errc::m_lexer::Code::INCONSISTENT_INDENTATION);

        } else if (m_size > m_indent_stack.back()) {
            if (m_indent_level + 1 > MAX_ALLOWED_INDENT)
                diagnostic::panic(diagnostic::errc::m_lexer::Code::TOO_MANY_INDENT_LEVELS);
            if (alt_size <= m_alt_indent_stack.back())
                diagnostic::panic(diagnostic::errc::m_lexer::Code::MIXED_INDENTATION);

            m_indent_level += 1;
            m_indent_stack.push(m_size);
            m_alt_indent_stack.push(alt_size);
            store(MAKE_TOKEN(tok::Fa_TokenType::INDENT, "", m_line, col));

        } else {
            unsigned int dedent_count = 0;
            while (m_indent_level > 0 && m_size < m_indent_stack.back()) {
                m_indent_level -= 1;
                m_indent_stack.pop();
                m_alt_indent_stack.pop();
                dedent_count += 1;
            }

            if (m_size != m_indent_stack.back())
                diagnostic::panic(diagnostic::errc::m_lexer::Code::INVALID_UNINDENT);
            if (alt_size != m_alt_indent_stack.back())
                diagnostic::panic(diagnostic::errc::m_lexer::Code::INCONSISTENT_INDENTATION);

            for (unsigned int i = 0; i < dedent_count; i += 1)
                store(MAKE_TOKEN(tok::Fa_TokenType::DEDENT, "", m_line, col));
        }
    };

    for (;;) {
        u32 m_line = m_source_manager.get_line_number();
        u32 col = m_source_manager.get_column_number();
        u32 m_current = m_source_manager.current_char();

        if (m_current == 0)
            break;

        if (m_current == '\n') {
            m_source_manager.consume_char();
            m_at_bol = true;
            tok::Fa_Token const* ret = MAKE_TOKEN(tok::Fa_TokenType::NEWLINE, util::encode_utf8_str('\n'), m_line, col);
            store(ret);
            next_line(m_line, col);
            return ret;
        }

        if (m_current == ' ' || m_current == '\t' || m_current == '\r') {
            m_source_manager.consume_char();
            continue;
        }

        if (m_current == '#') {
            while (m_current != '\n' && m_current != 0)
                m_current = m_source_manager.next_char();

            continue;
        }

        if (m_current == '\'' || m_current == '"') {
            Fa_StringRef string_literal;
            u32 quote = m_current;
            m_current = m_source_manager.next_char();

            while (m_current != '\n' && m_current != 0 && m_current != quote) {
                if (m_current == '\\') {
                    m_current = m_source_manager.next_char();
                    switch (m_current) {
                    case 'n':
                        string_literal += '\n';
                        break;
                    case 't':
                        string_literal += '\t';
                        break;
                    case 'r':
                        string_literal += '\r';
                        break;
                    case '\\':
                        string_literal += '\\';
                        break;
                    case '\'':
                        string_literal += '\'';
                        break;
                    case '"':
                        string_literal += '"';
                        break;
                    case '0':
                        string_literal += '\0';
                        break;
                    default:
                        string_literal += '\\';
                        string_literal += util::encode_utf8_str(m_current);
                        break;
                    }
                    m_source_manager.consume_char();
                } else {
                    string_literal += util::encode_utf8_str(m_current);
                    m_current = m_source_manager.next_char();
                }
            }

            if (m_current != quote)
                return finish(tok::Fa_TokenType::INVALID, string_literal, m_line, col);

            m_source_manager.consume_char(); // closing quote
            return FINISH(tok::Fa_TokenType::STRING, string_literal, m_line, col);
        }

        if (m_current == ',' || m_current == '.' || m_current == u'،' || m_current == u'٬') {
            m_source_manager.consume_char();
            return finish((m_current == '.') ? tok::Fa_TokenType::DOT : tok::Fa_TokenType::COMMA, util::encode_utf8_str(m_current), m_line, col);
        }

        if (m_current == '{' || m_current == '}' || m_current == '[' || m_current == ']' || m_current == '(' || m_current == ')' || m_current == ':') {
            Fa_StringRef symbol;
            symbol += util::encode_utf8_str(m_current);
            m_source_manager.consume_char();

            tok::Fa_TokenType tt;
            switch (m_current) {
            case '{':
                tt = tok::Fa_TokenType::LBRACE;
                break;
            case '}':
                tt = tok::Fa_TokenType::RBRACE;
                break;
            case '[':
                tt = tok::Fa_TokenType::LBRACKET;
                break;
            case ']':
                tt = tok::Fa_TokenType::RBRACKET;
                break;
            case '(':
                tt = tok::Fa_TokenType::LPAREN;
                break;
            case ')':
                tt = tok::Fa_TokenType::RPAREN;
                break;
            case ':': {
                if (m_source_manager.current_char() == '=') {
                    symbol += util::encode_utf8_str('=');
                    m_source_manager.consume_char();
                    tt = tok::Fa_TokenType::OP_ASSIGN;
                } else {
                    tt = tok::Fa_TokenType::COLON;
                }
                break;
            }
            default:
                tt = tok::Fa_TokenType::INVALID;
                break;
            }
            return FINISH(tt, symbol, m_line, col);
        }

        if (m_current == '=' || m_current == '<' || m_current == '>'
            || m_current == '!' || m_current == '+' || m_current == '-'
            || m_current == '|' || m_current == '&' || m_current == '*'
            || m_current == '/' || m_current == '%' || m_current == u'٪'
            || m_current == '^' || m_current == '~') {

            Fa_StringRef operator_str;
            operator_str += util::encode_utf8_str(m_current);
            m_source_manager.consume_char();

            u32 m_next = m_source_manager.current_char();
            if (m_next != 0) {
                Fa_StringRef two = operator_str + util::encode_utf8_str(m_next);
                if (tok::lookup_operator(two)) {
                    operator_str = two;
                    m_source_manager.consume_char();
                }
            }

            if (auto m_type = tok::lookup_operator(operator_str))
                return FINISH(*m_type, operator_str, m_line, col);

            return FINISH(tok::Fa_TokenType::INVALID, operator_str, m_line, col);
        }

        if (IS_DIGIT(m_current)) {
            Fa_StringRef number;
            number += util::encode_utf8_str(m_current);
            u32 m_next = m_source_manager.next_char();

            // numeric base prefix: 0x / 0o / 0b
            if (m_current == '0') {
                if (m_next == 'x' || m_next == 'X')
                    CONSUME_BASE_DIGITS(IS_XDIGIT(m_current), tok::Fa_TokenType::HEX, diagnostic::errc::m_lexer::Code::INVALID_BASE_LITERAL, "no digits after 0x");
                if (m_next == 'o' || m_next == 'O')
                    CONSUME_BASE_DIGITS(OCTAL_DIGIT(m_current), tok::Fa_TokenType::OCTAL, diagnostic::errc::m_lexer::Code::INVALID_BASE_LITERAL, "no digits after 0o");
                if (m_next == 'b' || m_next == 'B')
                    CONSUME_BASE_DIGITS(BINARY_DIGIT(m_current), tok::Fa_TokenType::BINARY, diagnostic::errc::m_lexer::Code::INVALID_BASE_LITERAL, "no digits after 0b");
            }

            m_current = m_source_manager.current_char();
            for (;;) {
                if (m_current == '_') {
                    m_current = m_source_manager.next_char();
                    continue;
                }

                if (!IS_DIGIT(m_current))
                    break;

                number += util::encode_utf8_str(m_current);
                m_current = m_source_manager.next_char();
            }

            if (m_current == '.') {
                number += util::encode_utf8_str(m_current);
                m_current = m_source_manager.next_char();
                for (;;) {
                    if (m_current == '_') {
                        m_current = m_source_manager.next_char();
                        continue;
                    }

                    if (!IS_DIGIT(m_current))
                        break;

                    number += util::encode_utf8_str(m_current);
                    m_current = m_source_manager.next_char();
                }
                return FINISH(tok::Fa_TokenType::DECIMAL, number, m_line, col);
            }

            return FINISH(tok::Fa_TokenType::INTEGER, number, m_line, col);
        }

        if (IS_ARDIGIT(m_current)) {
            Fa_StringRef digits;
            while (IS_ARDIGIT(m_current)) {
                digits += util::encode_utf8_str(m_current);
                m_current = m_source_manager.next_char();
            }

            return FINISH(tok::Fa_TokenType::INTEGER, digits, m_line, col);
        }

        if (IS_IDENT_S(m_current)) {
            Fa_StringRef identifier;
            while (IS_IDENT_C(m_current)) {
                identifier += util::encode_utf8_str(m_current);
                m_current = m_source_manager.next_char();
            }

            tok::Fa_TokenType tt = tok::Fa_TokenType::IDENTIFIER;
            if (auto m_type = tok::lookup_keyword(identifier))
                tt = *m_type;

            return FINISH(tt, identifier, m_line, col);
        }

        Fa_StringRef source_line = get_line_at(m_line);
        std::string snippet = source_line.empty() ? std::string() : std::string(source_line.data(), source_line.len());
        diagnostic::report(diagnostic::Severity::ERROR, m_line, col, diagnostic::errc::m_lexer::Code::INVALID_CHARACTER, snippet);
        diagnostic::panic(diagnostic::errc::m_lexer::Code::INVALID_CHARACTER, "U+" + [](u32 cp) {char buf[8]; std::snprintf(buf, sizeof(buf), "%04X", cp); return std::string(buf); }(m_current));
    }

    if (!m_tok_stream.empty() && m_tok_stream.back()->type() == tok::Fa_TokenType::ENDMARKER)
        return m_tok_stream.back();

    u32 last_line = m_source_manager.get_line_number();
    u32 last_col = m_source_manager.get_column_number();

    while (m_indent_level > 0) {
        m_indent_level -= 1;
        m_indent_stack.pop();
        m_alt_indent_stack.pop();
        store(MAKE_TOKEN(tok::Fa_TokenType::DEDENT, "", last_line, last_col));
    }

    return FINISH(tok::Fa_TokenType::ENDMARKER, "", last_line, last_col);
}

tok::Fa_Token const* Fa_Lexer::m_next()
{
    if (m_tok_index + 1 < m_tok_stream.size()) {
        m_tok_index += 1;
        return m_tok_stream[m_tok_index];
    }

    while (true) {
        lex_token();

        if (!m_tok_stream.empty() && m_tok_stream.back()->type() == tok::Fa_TokenType::ENDMARKER)
            break;
        if (m_tok_index + 1 < m_tok_stream.size())
            break;
    }

    if (m_tok_index + 1 < m_tok_stream.size())
        m_tok_index += 1;

    return m_tok_stream[m_tok_index];
}

tok::Fa_Token const* Fa_Lexer::m_current() const
{
    if (m_tok_index < m_tok_stream.size())
        return m_tok_stream[m_tok_index];
    if (!m_tok_stream.empty())
        return m_tok_stream.back();

    return nullptr;
}

tok::Fa_Token const* Fa_Lexer::peek(size_t n)
{
    while (m_tok_index + n >= m_tok_stream.size()) {
        if (!m_tok_stream.empty() && m_tok_stream.back()->type() == tok::Fa_TokenType::ENDMARKER)
            return m_tok_stream.back();

        lex_token();
    }

    return m_tok_stream[m_tok_index + n];
}

void Fa_Lexer::store(tok::Fa_Token const* tok) { m_tok_stream.push(tok); }

Fa_Array<tok::Fa_Token const*> Fa_Lexer::tokenize()
{
    while (m_next()->type() != tok::Fa_TokenType::ENDMARKER)
        ;

    return this->m_tok_stream;
}

} // namespace fairuz::lex
