#ifndef FA_LEXER_HPP
#define FA_LEXER_HPP

#include "farray.hpp"
#include "fdiagnostic.hpp"
#include "ftoken.hpp"

#include <filesystem>
#include <stack>
#include <utility>

namespace fairuz {

using TokenPtr = tok::Token const*;

static inline TokenPtr make_token(tok::TokenType tt, StringRef lexeme, SourceLocation loc)
{
    return get_allocator().allocate_object<tok::Token>(lexeme, tt, loc);
}

} // namespace fairuz

namespace fairuz::lex {

class FileManager {
public:
    FileManager() = default;
    explicit FileManager(std::string const& filepath);

    FileManager(FileManager&&) noexcept = delete;
    FileManager& operator=(FileManager&&) noexcept = delete;

    FileManager(FileManager const&) noexcept = delete;
    FileManager& operator=(FileManager&) noexcept = delete;

    ~FileManager() = default;

    StringRef load(std::string const& filepath, bool const replace = false);

    StringRef& buffer() { return m_input_buffer; }
    StringRef const& buffer() const { return m_input_buffer; }

    std::string get_path() const { return m_file_path; }

    StringRef get_line_at(u32 const line_idx) const;

private:
    std::string m_file_path;
    StringRef m_input_buffer;
    std::filesystem::file_time_type m_last_known_write_time;
}; // class FileManager

class SourceManager {
public:
    explicit SourceManager() = default;

    explicit SourceManager(FileManager* fm)
        : m_file_manager(fm)
    {
        if (m_file_manager == nullptr)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR);

        reset();
    }

    void reset();

    u32 get_line_number() const { return m_context.line; }
    u32 get_column_number() const { return m_context.column; }
    u64 get_file_offset() const { return m_context.offset; }
    std::string get_file_path() const noexcept { return m_file_manager->get_path(); }

    bool done() const { return m_context.offset >= m_file_manager->buffer().len(); }

    [[nodiscard]] u32 peek_char()
    {
        SourceLocation saved_ctx = m_context;
        u32 saved_current = m_current;
        u64 saved_bytes = m_current_bytes;

        consume_char();
        u32 cp = m_current;

        m_context = saved_ctx;
        m_current = saved_current;
        m_current_bytes = saved_bytes;

        return cp;
    }

    [[nodiscard]] u32 current_char() const { return m_current; }

    void consume_char()
    {
        if (m_context.offset >= m_file_manager->buffer().len()) {
            m_current = 0;
            m_current_bytes = 0;
            return;
        }

        advance(m_current, m_current_bytes);
        refresh_current_();
    }

    u32 next_char()
    {
        consume_char();
        return m_current;
    }

    void unget(u32 const cp)
    {
        PushbackEntry e = { cp, m_context, m_current_bytes };
        rewind_position_(cp, e.bytes);
        m_unget_stack.push(e);
    }

    StringRef get_line_at(u32 const line_idx) const { return m_file_manager->get_line_at(line_idx); }

    SourceLocation get_source_location() const { return m_context; }

    StringRef source_slice(u64 const start, u64 const end) { return m_file_manager->buffer().slice(start, end); }

    void refresh_current_();

private:
    struct PushbackEntry {
        u32 ch { 0 };
        SourceLocation ctx;
        u64 bytes { 0 };
    }; // struct PushBackEntry

    FileManager* m_file_manager { nullptr };
    SourceLocation m_context;
    u32 m_current { 0 };
    u64 m_current_bytes { 0 };
    std::stack<PushbackEntry> m_unget_stack;

    void advance(u32 const cp, u64 const bytes);
    void rewind_position_(u32 const cp, u64 const bytes);
    u32 calculate_column_at_offset(u64 const target_offset) const;
}; // class SourceManager

class Lexer {
public:
    explicit Lexer() = default;
    explicit Lexer(FileManager* m_file_manager);
    explicit Lexer(Lexer const&) = delete;
    explicit Lexer(Array<TokenPtr>& seq);

    TokenPtr operator()() { return next(); }
    TokenPtr current() const;
    TokenPtr next();
    TokenPtr peek(size_t n = 1);
    Array<TokenPtr> tokenize();
    StringRef get_line_at(u32 const line_idx) const { return m_source_manager.get_line_at(line_idx); }
    diagnostic::SourcePtr source() const { return m_source; }
    auto take_error() { return std::exchange(m_pending_error, std::nullopt); }

private:
    SourceManager m_source_manager;
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

#endif // FA_LEXER_HPP
