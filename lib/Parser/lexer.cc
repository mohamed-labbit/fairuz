//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Tokenizes Fairuz source text.
//===----------------------------------------------------------------------===//

#include "Parser/lexer.hpp"
#include "Macros.hpp"
#include "Parser/token.hpp"
#include "Support/string.hpp"
#include "Util/ctype.hpp"
#include "Util/utf8.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#define CONSUME_BASE_DIGITS(valid_fa_expr, token_type, err_code, detail) \
    do {                                                                 \
        current = next_char();                                           \
        bool any = false;                                                \
        for (;;) {                                                       \
            if (current == '_') {                                        \
                current = next_char();                                   \
                continue;                                                \
            }                                                            \
            if (!(valid_fa_expr)) break;                                 \
            current = next_char();                                       \
            any = true;                                                  \
        }                                                                \
        if (!any) fail(err_code, src_loc, detail);                       \
        StringRef number = source_slice(start_byte, get_file_offset());  \
        return finish(token_type, number, src_loc);                      \
    } while (0)

#define OCTAL_DIGIT(c) ((c) >= '0' && (c) <= '7' ? true : (IS_DIGIT(c) ? (fail(ErrorCode::INVALID_OCTAL_DIGIT, src_loc), false) : false))

#define BINARY_DIGIT(c) ((c) == '0' || (c) == '1' ? true : (IS_DIGIT(c) ? (fail(ErrorCode::INVALID_BINARY_DIGIT, src_loc), false) : false))

namespace fairuz::lex {

namespace {

StringRef copy_source_bytes(std::string_view bytes)
{
    StringRef result(bytes.size(), '\0');
    if (!bytes.empty()) std::memcpy(result.data(), bytes.data(), bytes.size());
    return result;
}

} // namespace

StringRef Lexer::get_line_at(u32 line_idx) const { return m_source ? copy_source_bytes(m_source->line(line_idx)) : StringRef { }; }

StringRef Lexer::source_slice(size_t start, size_t end) const
{
    if (!m_source) return { };
    auto bytes = m_source->source_slice(start, end);
    if (bytes.empty()) return { };
    if (m_lexeme_buffer.empty()) // Token replay lexers need no full input copy.
        return copy_source_bytes(bytes);
    return StringRef(m_lexeme_buffer, start, bytes.size());
}

void Lexer::refresh_current()
{
    m_current = 0;
    m_current_bytes = 0;
    if (done()) return;

    auto bytes = m_source->buffer();
    auto first = static_cast<unsigned char>(bytes[m_context.offset]);
    if (first < 0x80) {
        m_current = first;
        m_current_bytes = 1;
        return;
    }
    size_t width = first >= 0xC2 && first <= 0xDF ? 2 : first >= 0xE0 && first <= 0xEF ? 3 : first >= 0xF0 && first <= 0xF4 ? 4 : 0;
    if (!width) fail(ErrorCode::INVALID_UTF8, m_context, "Invalid UTF-8 start byte");
    if (width > bytes.size() - m_context.offset) fail(ErrorCode::INVALID_UTF8, m_context, "Truncated UTF-8 sequence");
    u32 cp = first & ((1u << (7 - width)) - 1);
    for (size_t i = 1; i < width; ++i) {
        auto byte = static_cast<unsigned char>(bytes[m_context.offset + i]);
        if ((byte & 0xC0) != 0x80) fail(ErrorCode::INVALID_UTF8, m_context, "Invalid UTF-8 continuation byte");
        cp = (cp << 6) | (byte & 0x3F);
    }
    if (cp < (width == 2 ? 0x80u : width == 3 ? 0x800u : 0x10000u) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        fail(ErrorCode::INVALID_UTF8, m_context, "Invalid UTF-8 codepoint");
    m_current = cp;
    m_current_bytes = width;
}

void Lexer::consume_char()
{
    if (done()) return;
    m_context.offset += m_current_bytes;
    if (m_current == '\n') {
        ++m_context.line;
        m_context.column = 1;
    } else {
        ++m_context.column;
    }
    refresh_current();
}

Lexer::Lexer(FileManager* fm)
    : m_tok_index(0)
    , m_indent_size(4)
    , m_indent_level(0)
    , m_at_bol(true)
{
    if (!fm) diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Lexer requires a source file");
    diagnostic::set_source(fm);
    m_source = diagnostic::engine.source();
    refresh_current();
    m_lexeme_buffer = copy_source_bytes(m_source->buffer());
    m_tok_stream = Array<TokenPtr>::with_capacity(1024);
    m_indent_stack = Array<u32>::with_capacity(8);
    m_alt_indent_stack = Array<u32>::with_capacity(8);
    m_indent_stack.push(0);
    m_alt_indent_stack.push(0);
}

Lexer::Lexer(Array<TokenPtr> const& seq, diagnostic::SourcePtr source)
    : m_tok_index(0)
    , m_indent_size(4)
    , m_indent_level(0)
    , m_tok_stream(seq)
    , m_at_bol(true)
    , m_source(std::move(source))
{
    m_indent_stack.push(0);
    m_alt_indent_stack.push(0);
}

void Lexer::fail(ErrorCode code, SourceLocation loc, std::string const& detail)
{
    diagnostic::SourceScope source_scope(m_source);
    loc.length = std::max<u16>(loc.length, 1);
    auto id = diagnostic::report(diagnostic::Severity::ERROR, loc, code, detail);
    if (code == ErrorCode::INVALID_UNINDENT || code == ErrorCode::INCONSISTENT_INDENTATION || code == ErrorCode::MIXED_INDENTATION)
        diagnostic::engine.add_suggestion(id, "Use spaces consistently and align with an enclosing block.");
    diagnostic::engine.panic("");
}

TokenPtr Lexer::lex_token()
{
    diagnostic::SourceScope source_scope(m_source);
    auto delimiter_error = [this](ErrorCode code, SourceLocation loc, std::string const& detail) {
        loc.length = 1;
        auto id = diagnostic::report(diagnostic::Severity::ERROR, loc, code, detail);
        m_pending_error = std::make_pair(static_cast<u16>(code), id);
    };
    auto finish = [this](tok::TokenType tt, StringRef str, SourceLocation src_loc) {
        auto end = get_source_location();
        if (src_loc.line == end.line && end.column >= src_loc.column) src_loc.length = end.column - src_loc.column;
        TokenPtr ret = make_token(tt, str, src_loc);
        store(ret);
        return m_tok_stream.back();
    };

    if (m_tok_stream.empty()) return finish(tok::TokenType::BEGINMARKER, "", { });

    auto next_line = [this](SourceLocation src_loc) {
        if (!m_at_bol) return;

        m_at_bol = false;
        src_loc = get_source_location();

        u32 size = 0;
        u32 alt_size = 0;
        u32 cont_line_col = 0;
        u32 current = current_char();

        // count leading whitespace
        for (;;) {
            if (current == ' ') {
                consume_char();
                size++;
                alt_size++;
            } else if (current == '\t') {
                consume_char();
                size = (size / m_indent_size + 1) * m_indent_size;
                alt_size = (alt_size / TABWIDTH + 1) * TABWIDTH;
            } else if (current == '\f') {
                // form-feed resets the column counter — handled explicitly,
                // NOT via ml_is_space, because it has special indent semantics.
                consume_char();
                size = alt_size = 0;
            } else if (current == '\\') {
                consume_char();
                if (!cont_line_col) cont_line_col = size;
            } else {
                break;
            }

            current = current_char();
        }

        // blank
        if (current == '#' || current == '\n' || current == '\r') return;
        if (current == 0) return;
        if (cont_line_col) size = alt_size = cont_line_col;

        if (size == m_indent_stack.back()) {
            if (alt_size != m_alt_indent_stack.back()) fail(ErrorCode::INCONSISTENT_INDENTATION, src_loc);

        } else if (size > m_indent_stack.back()) {
            if (m_indent_level + 1 > MAX_ALLOWED_INDENT) fail(ErrorCode::TOO_MANY_INDENT_LEVELS, src_loc);
            if (alt_size <= m_alt_indent_stack.back()) fail(ErrorCode::MIXED_INDENTATION, src_loc);

            m_indent_level++;
            m_indent_stack.push(size);
            m_alt_indent_stack.push(alt_size);
            store(make_token(tok::TokenType::INDENT, "", src_loc));

        } else {
            u32 dedent_count = 0;
            while (m_indent_level > 0 && size < m_indent_stack.back()) {
                m_indent_level -= 1;
                m_indent_stack.pop();
                m_alt_indent_stack.pop();
                dedent_count++;
            }

            if (size != m_indent_stack.back()) fail(ErrorCode::INVALID_UNINDENT, src_loc);
            if (alt_size != m_alt_indent_stack.back()) fail(ErrorCode::INCONSISTENT_INDENTATION, src_loc);

            for (u32 i = 0; i < dedent_count; i++) store(make_token(tok::TokenType::DEDENT, "", src_loc));
        }
    };

    for (;;) {
        SourceLocation src_loc = get_source_location();
        u32 current = current_char();

        if (current == 0) {
            if (!done()) fail(ErrorCode::INVALID_CHARACTER, src_loc, "U+0000");
            break;
        }

        if (current == '\n') {
            if (m_bracket_depth > 0) {
                consume_char();
                continue; // inside ( [ { — newline is just whitespace, no NEWLINE/INDENT tracking
            }
            u32 const start_byte = get_file_offset();
            consume_char();
            m_at_bol = true;
            StringRef endl_str = source_slice(start_byte, start_byte + 1);
            TokenPtr ret = make_token(tok::TokenType::NEWLINE, endl_str, src_loc);
            store(ret);
            next_line(src_loc);
            return ret;
        }

        if (current == ' ' || current == '\t' || current == '\r') {
            consume_char();
            continue;
        }

        if (current == '#') {
            while (current != '\n' && current != 0) current = next_char();

            continue;
        }

        if (current == '\'' || current == '"') {
            u32 quote = current;
            current = next_char();
            u32 const start_byte = get_file_offset();

            std::string decoded;
            bool escaped = false;
            while (current != '\n' && current != 0 && current != quote) {
                if (current != '\\') {
                    StringRef bytes = util::encode_utf8_str(current);
                    decoded.append(bytes.data(), bytes.len());
                    current = next_char();
                    continue;
                }

                escaped = true;
                current = next_char();
                switch (current) {
                case '\\': decoded.push_back('\\'); break;
                case '\'': decoded.push_back('\''); break;
                case '"': decoded.push_back('"'); break;
                case 'n': decoded.push_back('\n'); break;
                case 'r': decoded.push_back('\r'); break;
                case 't': decoded.push_back('\t'); break;
                case 'b': decoded.push_back('\b'); break;
                case 'f': decoded.push_back('\f'); break;
                case '/': decoded.push_back('/'); break;
                case 'u': {
                    u32 codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        current = next_char();
                        int digit = -1;
                        if (current >= '0' && current <= '9') digit = static_cast<int>(current - '0');
                        else if (current >= 'a' && current <= 'f') digit = static_cast<int>(current - 'a' + 10);
                        else if (current >= 'A' && current <= 'F') digit = static_cast<int>(current - 'A' + 10);
                        if (digit < 0) fail(ErrorCode::INVALID_ESCAPE_SEQUENCE, src_loc, "\\u must be followed by four hexadecimal digits");
                        codepoint = (codepoint << 4) | static_cast<u32>(digit);
                    }
                    if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
                        fail(ErrorCode::INVALID_ESCAPE_SEQUENCE, src_loc, "surrogate code points are not valid Unicode characters");
                    StringRef bytes = util::encode_utf8_str(codepoint);
                    decoded.append(bytes.data(), bytes.len());
                    current = next_char();
                    continue;
                }
                default: fail(ErrorCode::INVALID_ESCAPE_SEQUENCE, src_loc);
                }
                current = next_char();
            }

            if (current != quote) fail(ErrorCode::UNTERMINATED_STRING, src_loc, std::string("expected closing ") + static_cast<char>(quote));

            StringRef str_lit = escaped ? StringRef(decoded.c_str()) : source_slice(start_byte, get_file_offset());
            consume_char(); // closing quote
            return finish(tok::TokenType::STRING, str_lit, src_loc);
        }

        if (current == ',' || current == '.' || current == u'،' || current == u'٬') {
            u32 const start_byte = get_file_offset();
            consume_char();
            StringRef comma = source_slice(start_byte, get_file_offset());
            return finish((current == '.') ? tok::TokenType::DOT : tok::TokenType::COMMA, util::encode_utf8_str(current), src_loc);
        }

        if (current == '{' || current == '}' || current == '[' || current == ']' || current == '(' || current == ')' || current == ':') {
            if (current == '{' || current == '[' || current == '(') m_brackets.push_back({ current, src_loc });
            else if (current != ':') {
                u32 opening = current == '}' ? '{' : current == ']' ? '[' : '(';
                if (m_brackets.empty()) {
                    delimiter_error(ErrorCode::MISMATCHED_DELIMITER, src_loc, std::string(1, static_cast<char>(current)));
                } else if (m_brackets.back().first != opening) {
                    auto const& [character, location] = m_brackets.back();
                    delimiter_error(ErrorCode::MISMATCHED_DELIMITER, src_loc,
                        std::string(1, static_cast<char>(current)) + " does not match '" + static_cast<char>(character) + "' opened on line "
                            + std::to_string(location.line));
                }
                if (!m_brackets.empty()) m_brackets.pop_back();
            }
            u32 const start_byte = get_file_offset();
            consume_char();

            tok::TokenType tt;
            switch (current) {
            case '{':
                tt = tok::TokenType::LBRACE;
                m_bracket_depth++;
                break;
            case '}':
                tt = tok::TokenType::RBRACE;
                if (m_bracket_depth > 0) m_bracket_depth -= 1;
                break;
            case '[':
                tt = tok::TokenType::LBRACKET;
                m_bracket_depth++;
                break;
            case ']':
                tt = tok::TokenType::RBRACKET;
                if (m_bracket_depth > 0) m_bracket_depth -= 1;
                break;
            case '(':
                tt = tok::TokenType::LPAREN;
                m_bracket_depth++;
                break;
            case ')':
                tt = tok::TokenType::RPAREN;
                if (m_bracket_depth > 0) m_bracket_depth -= 1;
                break;
            case ':': {
                if (current_char() == '=') {
                    consume_char();
                    tt = tok::TokenType::OP_ASSIGN;
                } else {
                    tt = tok::TokenType::COLON;
                }
                break;
            }
            default: tt = tok::TokenType::INVALID; break;
            }

            StringRef symbol = source_slice(start_byte, get_file_offset());
            return finish(tt, symbol, src_loc);
        }

        if (current == '=' || current == '<' || current == '>' || current == '!' || current == '+' || current == '-' || current == '|'
            || current == '&' || current == '*' || current == '/' || current == '%' || current == u'٪' || current == '^' || current == '~') {

            auto is_shift_eq_prefix
                = [](u32 first, u32 second) -> bool { return (first == '>' && second == '>') || (first == '<' && second == '<'); };

            u32 const start_byte = get_file_offset();
            consume_char();

            u32 second = current_char();
            if (second != 0) {
                if (is_shift_eq_prefix(current, second)) {
                    consume_char();                            // second
                    if (current_char() == '=') consume_char(); // optional assignment suffix

                    u32 const end_byte = get_file_offset();
                    StringRef op_str = source_slice(start_byte, end_byte);

                    if (auto type = tok::lookup_operator(op_str)) return finish(*type, op_str, src_loc);

                    return finish(tok::TokenType::INVALID, op_str, src_loc);
                }

                StringRef two = source_slice(start_byte, get_file_offset() + util::utf8_codepoint_size(second));
                if (auto type = tok::lookup_operator(two)) {
                    consume_char();
                    return finish(*type, two, src_loc);
                }
            }

            StringRef operator_str = source_slice(start_byte, get_file_offset());

            if (auto type = tok::lookup_operator(operator_str)) return finish(*type, operator_str, src_loc);

            return finish(tok::TokenType::INVALID, operator_str, src_loc);
        }

        if (IS_DIGIT(current)) {
            u32 const start_byte = get_file_offset();
            u32 next = next_char();

            // numeric base prefix: 0x / 0o / 0b
            if (current == '0') {
                if (next == 'x' || next == 'X')
                    CONSUME_BASE_DIGITS(IS_XDIGIT(current), tok::TokenType::HEX, ErrorCode::INVALID_BASE_LITERAL, "no digits after 0x");
                if (next == 'o' || next == 'O')
                    CONSUME_BASE_DIGITS(OCTAL_DIGIT(current), tok::TokenType::OCTAL, ErrorCode::INVALID_BASE_LITERAL, "no digits after 0o");
                if (next == 'b' || next == 'B')
                    CONSUME_BASE_DIGITS(BINARY_DIGIT(current), tok::TokenType::BINARY, ErrorCode::INVALID_BASE_LITERAL, "no digits after 0b");
            }

            current = current_char();
            for (;;) {
                if (current == '_') {
                    current = next_char();
                    continue;
                }

                if (!IS_DIGIT(current)) break;

                current = next_char();
            }

            if (current == '.') {
                current = next_char();

                for (;;) {
                    if (current == '_') {
                        current = next_char();
                        continue;
                    }

                    if (!IS_DIGIT(current)) break;

                    current = next_char();
                }

                StringRef number = source_slice(start_byte, get_file_offset());
                return finish(tok::TokenType::DECIMAL, number, src_loc);
            }

            StringRef number = source_slice(start_byte, get_file_offset());
            return finish(tok::TokenType::INTEGER, number, src_loc);
        }

        if (IS_ARDIGIT(current)) {
            u32 const start_byte = get_file_offset();

            while (IS_ARDIGIT(current)) current = next_char();

            u32 const end_byte = get_file_offset();
            return finish(tok::TokenType::INTEGER, source_slice(start_byte, end_byte), src_loc);
        }

        if (IS_IDENT_S(current)) {
            u32 const start_byte = get_file_offset();

            while (IS_IDENT_C(current)) current = next_char();

            u32 const end_byte = get_file_offset();
            StringRef ident = source_slice(start_byte, end_byte);

            tok::TokenType tt = tok::TokenType::IDENTIFIER;
            if (auto m_type = tok::lookup_keyword(ident)) tt = *m_type;

            return finish(tt, ident, src_loc);
        }

        fail(
            ErrorCode::INVALID_CHARACTER, src_loc, "U+" + [](u32 cp) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%04X", cp);
                return std::string(buf);
            }(current));
    }

    if (!m_tok_stream.empty() && m_tok_stream.back()->type() == tok::TokenType::ENDMARKER) return m_tok_stream.back();

    SourceLocation last_loc = get_source_location();
    if (!m_brackets.empty()) {
        delimiter_error(ErrorCode::UNCLOSED_DELIMITER, m_brackets.back().second, std::string(1, static_cast<char>(m_brackets.back().first)));
        m_brackets.clear();
        m_bracket_depth = 0;
    }

    while (m_indent_level > 0) {
        m_indent_level -= 1;
        m_indent_stack.pop();
        m_alt_indent_stack.pop();
        store(make_token(tok::TokenType::DEDENT, "", last_loc));
    }

    return finish(tok::TokenType::ENDMARKER, "", last_loc);
}

TokenPtr Lexer::next()
{
    if (current() != nullptr && current()->is(tok::TokenType::ENDMARKER)) return current();
    if (m_tok_index + 1 < m_tok_stream.size()) {
        m_tok_index++;
        return m_tok_stream[m_tok_index];
    }

    while (true) {
        lex_token();

        if (!m_tok_stream.empty() && m_tok_stream.back()->type() == tok::TokenType::ENDMARKER) break;
        if (m_tok_index + 1 < m_tok_stream.size()) break;
    }

    if (m_tok_index + 1 < m_tok_stream.size()) m_tok_index++;

    return m_tok_stream[m_tok_index];
}

TokenPtr Lexer::current() const
{
    if (m_tok_index < m_tok_stream.size()) return m_tok_stream[m_tok_index];
    if (!m_tok_stream.empty()) return m_tok_stream.back();

    return nullptr;
}

TokenPtr Lexer::peek(size_t n)
{
    while (m_tok_index + n >= m_tok_stream.size()) {
        if (!m_tok_stream.empty() && m_tok_stream.back()->type() == tok::TokenType::ENDMARKER) return m_tok_stream.back();

        lex_token();
    }

    return m_tok_stream[m_tok_index + n];
}

void Lexer::store(TokenPtr tok) { m_tok_stream.push(tok); }

Array<TokenPtr> Lexer::tokenize()
{
    while (next()->type() != tok::TokenType::ENDMARKER);

    return this->m_tok_stream;
}

} // namespace fairuz::lex
