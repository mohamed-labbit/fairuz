/// lexer.cc

#include "lexer.hpp"
#include "ctype.hpp"
#include "util.hpp"

#include <fstream>

#define CONSUME_BASE_DIGITS(valid_fa_expr, token_type, err_code, detail)                                     \
    do {                                                                                                     \
        current = m_source_manager.next_char();                                                              \
        bool any = false;                                                                                    \
        for (;;) {                                                                                           \
            if (current == '_') {                                                                            \
                current = m_source_manager.next_char();                                                      \
                continue;                                                                                    \
            }                                                                                                \
            if (!(valid_fa_expr))                                                                            \
                break;                                                                                       \
            current = m_source_manager.next_char();                                                          \
            any = true;                                                                                      \
        }                                                                                                    \
        if (!any)                                                                                            \
            diagnostic::panic(err_code, detail);                                                             \
        Fa_StringRef number = m_source_manager.source_slice(start_byte, m_source_manager.get_file_offset()); \
        return finish(token_type, number, src_loc);                                                          \
    } while (0)

#define OCTAL_DIGIT(c)                                                             \
    ((c) >= '0' && (c) <= '7'                                                      \
            ? true                                                                 \
            : (IS_DIGIT(c)                                                         \
                      ? (diagnostic::panic(ErrorCode::INVALID_OCTAL_DIGIT), false) \
                      : false))

#define BINARY_DIGIT(c)                                                             \
    ((c) == '0' || (c) == '1'                                                       \
            ? true                                                                  \
            : (IS_DIGIT(c)                                                          \
                      ? (diagnostic::panic(ErrorCode::INVALID_BINARY_DIGIT), false) \
                      : false))

namespace fairuz::lex {

using ErrorCode = diagnostic::errc::lexer::Code;

namespace fs = std::filesystem;

Fa_FileManager::Fa_FileManager(std::string const& filepath)
    : m_file_path(filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in)
        diagnostic::panic(ErrorCode::FILE_NOT_OPEN, filepath);

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
        diagnostic::panic(ErrorCode::FILE_NOT_OPEN, filepath);

    if (replace || m_file_path.empty()) {
        m_input_buffer = ret;
        m_file_path = filepath;
        m_last_known_write_time = fs::last_write_time(filepath);
    }

    return ret;
}

void Fa_SourceManager::refresh_current_()
{
    if (!m_file_manager || m_context.offset >= m_file_manager->buffer().len()) {
        m_current = 0;
        m_current_bytes = 0;
        return;
    }

    m_current = util::decode_utf8_at(
        m_file_manager->buffer(),
        m_context.offset,
        &m_current_bytes);
}

void Fa_SourceManager::reset()
{
    m_context.line = 1;
    m_context.column = 1;
    m_context.offset = 0;

    while (!m_unget_stack.empty())
        m_unget_stack.pop();

    refresh_current_();
}

u32 Fa_SourceManager::peek_char()
{
    Fa_SourceLocation saved_ctx = m_context;
    u32 saved_current = m_current;
    u64 saved_bytes = m_current_bytes;

    consume_char();
    u32 cp = m_current;

    m_context = saved_ctx;
    m_current = saved_current;
    m_current_bytes = saved_bytes;

    return cp;
}

u32 Fa_SourceManager::current_char() const
{
    return m_current;
}

void Fa_SourceManager::consume_char()
{
    if (m_context.offset >= m_file_manager->buffer().len()) {
        m_current = 0;
        m_current_bytes = 0;
        return;
    }

    advance(m_current, m_current_bytes);
    refresh_current_();
}

u32 Fa_SourceManager::next_char()
{
    consume_char();
    return m_current;
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
    m_context.offset += bytes;

    if (cp == '\n') {
        m_context.line += 1;
        m_context.column = 1;
    } else {
        m_context.column += 1;
    }
}

void Fa_SourceManager::rewind_position_(u32 const cp, u64 const bytes)
{
    if (m_context.offset < bytes)
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR, "Fa_SourceManager: attempted to rewind past beginning of file", diagnostic::Severity::FATAL);

    m_context.offset -= bytes;

    if (cp == '\n') {
        m_context.line = (m_context.line > 1) ? (m_context.line - 1) : 1;
        m_context.column = calculate_column_at_offset(m_context.offset);
    } else {
        m_context.column = (m_context.column > 1) ? (m_context.column - 1) : 1;
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

    u32 column = 1;
    u64 pos = line_start;

    while (pos < target_offset) {
        u64 bytes = 0;
        util::decode_utf8_at(buf, pos, &bytes);
        pos += bytes;
        column += 1;
    }

    return column;
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
}

Fa_Lexer::Fa_Lexer(Fa_Array<tok::Fa_Token const*>& seq)
    : m_tok_stream(seq)
    , m_tok_index(0)
    , m_indent_size(4)
    , m_indent_level(0)
    , m_at_bol(true)
{
    m_indent_stack.push(0);
    m_alt_indent_stack.push(0);
}

tok::Fa_Token const* Fa_Lexer::lex_token()
{
    auto finish = [this](tok::Fa_TokenType tt, Fa_StringRef str, Fa_SourceLocation src_loc) {
        tok::Fa_Token const* ret = make_token(tt, str, src_loc);
        store(ret);
        return m_tok_stream.back();
    };

    if (m_tok_stream.empty())
        return finish(tok::Fa_TokenType::BEGINMARKER, "", { });

    auto next_line = [this](Fa_SourceLocation src_loc) {
        if (!m_at_bol)
            return;

        m_at_bol = false;

        unsigned int size = 0;
        unsigned int alt_size = 0;
        unsigned int cont_line_col = 0;

        u32 current = m_source_manager.current_char();

        // count leading whitespace
        for (;;) {
            if (current == ' ') {
                m_source_manager.consume_char();
                size += 1;
                alt_size += 1;
            } else if (current == '\t') {
                m_source_manager.consume_char();
                size = (size / m_indent_size + 1) * m_indent_size;
                alt_size = (alt_size / TABWIDTH + 1) * TABWIDTH;
            } else if (current == '\f') {
                // form-feed resets the column counter — handled explicitly,
                // NOT via ml_is_space, because it has special indent semantics.
                m_source_manager.consume_char();
                size = alt_size = 0;
            } else if (current == '\\') {
                m_source_manager.consume_char();
                if (!cont_line_col)
                    cont_line_col = size;
            } else {
                break;
            }

            current = m_source_manager.current_char();
        }

        // blank
        if (current == '#' || current == '\n' || current == '\r')
            return;
        if (current == 0)
            return;

        if (cont_line_col)
            size = alt_size = cont_line_col;

        if (size == m_indent_stack.back()) {
            if (alt_size != m_alt_indent_stack.back())
                diagnostic::panic(ErrorCode::INCONSISTENT_INDENTATION);

        } else if (size > m_indent_stack.back()) {
            if (m_indent_level + 1 > MAX_ALLOWED_INDENT)
                diagnostic::panic(ErrorCode::TOO_MANY_INDENT_LEVELS);
            if (alt_size <= m_alt_indent_stack.back())
                diagnostic::panic(ErrorCode::MIXED_INDENTATION);

            m_indent_level += 1;
            m_indent_stack.push(size);
            m_alt_indent_stack.push(alt_size);
            store(make_token(tok::Fa_TokenType::INDENT, "", src_loc));

        } else {
            unsigned int dedent_count = 0;
            while (m_indent_level > 0 && size < m_indent_stack.back()) {
                m_indent_level -= 1;
                m_indent_stack.pop();
                m_alt_indent_stack.pop();
                dedent_count += 1;
            }

            if (size != m_indent_stack.back())
                diagnostic::panic(ErrorCode::INVALID_UNINDENT);
            if (alt_size != m_alt_indent_stack.back())
                diagnostic::panic(ErrorCode::INCONSISTENT_INDENTATION);

            for (unsigned int i = 0; i < dedent_count; i += 1)
                store(make_token(tok::Fa_TokenType::DEDENT, "", src_loc));
        }
    };

    for (;;) {
        Fa_SourceLocation src_loc = m_source_manager.get_source_location();
        u32 current = m_source_manager.current_char();

        if (current == 0)
            break;

        if (current == '\n') {
            u32 const start_byte = m_source_manager.get_file_offset();
            m_source_manager.consume_char();
            m_at_bol = true;
            Fa_StringRef endl_str = m_source_manager.source_slice(start_byte, start_byte + 1);
            tok::Fa_Token const* ret = make_token(tok::Fa_TokenType::NEWLINE, endl_str, src_loc);
            store(ret);
            next_line(src_loc);
            return ret;
        }

        if (current == ' ' || current == '\t' || current == '\r') {
            m_source_manager.consume_char();
            continue;
        }

        if (current == '#') {
            while (current != '\n' && current != 0)
                current = m_source_manager.next_char();

            continue;
        }

        if (current == '\'' || current == '"') {
            u32 quote = current;
            current = m_source_manager.next_char();
            u32 const start_byte = m_source_manager.get_file_offset();

            while (current != '\n' && current != 0 && current != quote)
                current = m_source_manager.next_char();

            if (current != quote) {
                Fa_StringRef str_lit = m_source_manager.source_slice(start_byte, m_source_manager.get_file_offset());
                return finish(tok::Fa_TokenType::INVALID, str_lit, src_loc);
            }

            Fa_StringRef str_lit = m_source_manager.source_slice(start_byte, m_source_manager.get_file_offset());
            m_source_manager.consume_char(); // closing quote
            return finish(tok::Fa_TokenType::STRING, str_lit, src_loc);
        }

        if (current == ',' || current == '.' || current == u'،' || current == u'٬') {
            u32 const start_byte = m_source_manager.get_file_offset();
            m_source_manager.consume_char();
            Fa_StringRef comma = m_source_manager.source_slice(start_byte, m_source_manager.get_file_offset());
            return finish((current == '.') ? tok::Fa_TokenType::DOT : tok::Fa_TokenType::COMMA, util::encode_utf8_str(current), src_loc);
        }

        if (current == '{' || current == '}' || current == '[' || current == ']' || current == '(' || current == ')' || current == ':') {
            u32 const start_byte = m_source_manager.get_file_offset();
            m_source_manager.consume_char();

            tok::Fa_TokenType tt;
            switch (current) {
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

            Fa_StringRef symbol = m_source_manager.source_slice(start_byte, m_source_manager.get_file_offset());
            return finish(tt, symbol, src_loc);
        }

        if (current == '=' || current == '<' || current == '>' || current == '!' || current == '+' || current == '-' || current == '|'
            || current == '&' || current == '*' || current == '/' || current == '%' || current == u'٪' || current == '^' || current == '~') {

            u32 const start_byte = m_source_manager.get_file_offset();
            m_source_manager.consume_char();

            u32 next = m_source_manager.current_char();
            if (next != 0) {
                Fa_StringRef two = m_source_manager.source_slice(start_byte, m_source_manager.get_file_offset() + util::utf8_codepoint_size(next));
                if (auto type = tok::lookup_operator(two)) {
                    m_source_manager.consume_char();
                    return finish(*type, two, src_loc);
                }
            }

            Fa_StringRef operator_str = m_source_manager.source_slice(start_byte, m_source_manager.get_file_offset());

            if (auto type = tok::lookup_operator(operator_str))
                return finish(*type, operator_str, src_loc);

            return finish(tok::Fa_TokenType::INVALID, operator_str, src_loc);
        }

        if (IS_DIGIT(current)) {
            u32 const start_byte = m_source_manager.get_file_offset();
            u32 next = m_source_manager.next_char();

            // numeric base prefix: 0x / 0o / 0b
            if (current == '0') {
                if (next == 'x' || next == 'X')
                    CONSUME_BASE_DIGITS(IS_XDIGIT(current), tok::Fa_TokenType::HEX, ErrorCode::INVALID_BASE_LITERAL, "no digits after 0x");
                if (next == 'o' || next == 'O')
                    CONSUME_BASE_DIGITS(OCTAL_DIGIT(current), tok::Fa_TokenType::OCTAL, ErrorCode::INVALID_BASE_LITERAL, "no digits after 0o");
                if (next == 'b' || next == 'B')
                    CONSUME_BASE_DIGITS(BINARY_DIGIT(current), tok::Fa_TokenType::BINARY, ErrorCode::INVALID_BASE_LITERAL, "no digits after 0b");
            }

            current = m_source_manager.current_char();
            for (;;) {
                if (current == '_') {
                    current = m_source_manager.next_char();
                    continue;
                }

                if (!IS_DIGIT(current))
                    break;

                current = m_source_manager.next_char();
            }

            if (current == '.') {
                current = m_source_manager.next_char();

                for (;;) {
                    if (current == '_') {
                        current = m_source_manager.next_char();
                        continue;
                    }

                    if (!IS_DIGIT(current))
                        break;

                    current = m_source_manager.next_char();
                }

                Fa_StringRef number = m_source_manager.source_slice(start_byte, m_source_manager.get_file_offset());
                return finish(tok::Fa_TokenType::DECIMAL, number, src_loc);
            }

            Fa_StringRef number = m_source_manager.source_slice(start_byte, m_source_manager.get_file_offset());
            return finish(tok::Fa_TokenType::INTEGER, number, src_loc);
        }

        if (IS_ARDIGIT(current)) {
            u32 const start_byte = m_source_manager.get_file_offset();

            while (IS_ARDIGIT(current))
                current = m_source_manager.next_char();

            u32 const end_byte = m_source_manager.get_file_offset();
            return finish(tok::Fa_TokenType::INTEGER, m_source_manager.source_slice(start_byte, end_byte), src_loc);
        }

        if (IS_IDENT_S(current)) {
            u32 const start_byte = m_source_manager.get_file_offset();

            while (IS_IDENT_C(current))
                current = m_source_manager.next_char();

            u32 const end_byte = m_source_manager.get_file_offset();
            Fa_StringRef ident = m_source_manager.source_slice(start_byte, end_byte);

            tok::Fa_TokenType tt = tok::Fa_TokenType::IDENTIFIER;
            if (auto m_type = tok::lookup_keyword(ident))
                tt = *m_type;

            return finish(tt, ident, src_loc);
        }

        Fa_StringRef source_line = get_line_at(src_loc.line);
        std::string snippet = source_line.empty() ? std::string() : std::string(source_line.data(), source_line.len());
        diagnostic::report(diagnostic::Severity::ERROR, src_loc, ErrorCode::INVALID_CHARACTER, snippet);
        diagnostic::panic(ErrorCode::INVALID_CHARACTER, "U+" + [](u32 cp) {char buf[8]; std::snprintf(buf, sizeof(buf), "%04X", cp); return std::string(buf); }(current));
    }

    if (!m_tok_stream.empty() && m_tok_stream.back()->type() == tok::Fa_TokenType::ENDMARKER)
        return m_tok_stream.back();

    Fa_SourceLocation last_loc = m_source_manager.get_source_location();

    while (m_indent_level > 0) {
        m_indent_level -= 1;
        m_indent_stack.pop();
        m_alt_indent_stack.pop();
        store(make_token(tok::Fa_TokenType::DEDENT, "", last_loc));
    }

    return finish(tok::Fa_TokenType::ENDMARKER, "", last_loc);
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

tok::Fa_Token const* Fa_Lexer::current() const
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
