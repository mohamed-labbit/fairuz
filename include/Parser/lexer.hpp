//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Tokenizes Fairuz source text.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_PARSER_LEXER_HPP
#define FAIRUZ_PARSER_LEXER_HPP

#include "Diag/diagnostic.hpp"
#include "Support/array.hpp"
#include "token.hpp"

#include <utility>

namespace fairuz {

using TokenPtr = tok::Token const*;

static inline TokenPtr make_token(tok::TokenType tt, StringRef lexeme, SourceLocation loc)
{
    return get_allocator().allocate_object<tok::Token>(lexeme, tt, loc);
}

} // namespace fairuz

namespace fairuz::lex {

// Compatibility name for existing lexer clients; defined independently in ffile.hpp.
using FileManager = fairuz::FileManager;

class Lexer {
public:
    explicit Lexer() = default;
    explicit Lexer(FileManager* m_file_manager);
    explicit Lexer(Lexer const&) = delete;
    explicit Lexer(Array<TokenPtr> const& seq, diagnostic::SourcePtr source = nullptr);

    TokenPtr operator()() { return next(); }
    TokenPtr current() const;
    TokenPtr next();
    TokenPtr peek(size_t n = 1);
    Array<TokenPtr> tokenize();
    StringRef get_line_at(u32 line_idx) const;
    diagnostic::SourcePtr source() const { return m_source; }
    StringRef source_slice(size_t start, size_t end) const;
    auto take_error() { return std::exchange(m_pending_error, std::nullopt); }

private:
    // Unicode cursor belongs to the lexer; offsets remain byte offsets.
    SourceLocation m_context { 1, 1, 0, 0 };
    u32 m_current { 0 };
    u64 m_current_bytes { 0 };
    bool done() const { return !m_source || m_context.offset >= m_source->text.size(); }
    u32 current_char() const { return m_current; }
    void refresh_current();
    void consume_char();
    u32 next_char()
    {
        consume_char();
        return m_current;
    }
    SourceLocation get_source_location() const { return m_context; }
    u64 get_file_offset() const { return m_context.offset; }
    // Token lexemes share an arena-backed copy and can outlive this lexer.
    StringRef m_lexeme_buffer;
    size_t m_tok_index { 0 };
    u32 m_indent_size { 0 };
    u32 m_indent_level { 0 };
    Array<TokenPtr> m_tok_stream;
    Array<u32> m_indent_stack;
    Array<u32> m_alt_indent_stack;
    bool m_at_bol { true };
    u32 m_bracket_depth { 0 };
    diagnostic::SourcePtr m_source;
    std::vector<std::pair<u32, SourceLocation>> m_brackets;
    std::optional<std::pair<u16, diagnostic::DiagnosticEngine::DiagnosticId>> m_pending_error;
    [[noreturn]] void fail(ErrorCode code, SourceLocation loc, std::string const& detail = "");

    // main lexer loop
    TokenPtr lex_token();

    void store(TokenPtr tok);
}; // class Lexer

} // namespace fairuz::lex

#endif // FAIRUZ_PARSER_LEXER_HPP
