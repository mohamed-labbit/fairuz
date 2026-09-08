#ifndef FA_LEXER_HPP
#define FA_LEXER_HPP

#include "farray.hpp"
#include "fdiagnostic.hpp"
#include "ftoken.hpp"

#include <filesystem>
#include <stack>

namespace fairuz {

using TokenPtr = tok::Fa_Token const*;

static inline TokenPtr Fa_make_token(tok::Fa_TokenType tt, Fa_StringRef lexeme, Fa_SourceLocation loc)
{
    return get_allocator().allocate_object<tok::Fa_Token>(lexeme, tt, loc);
}

} // namespace fairuz

namespace fairuz::lex {

using FileManagerError = diagnostic::errc::FileManager::Code;

class Fa_FileManager {
public:
    Fa_FileManager() = default;
    explicit Fa_FileManager(std::string const& filepath);

    Fa_FileManager(Fa_FileManager&&) noexcept = delete;
    Fa_FileManager& operator=(Fa_FileManager&&) noexcept = delete;

    Fa_FileManager(Fa_FileManager const&) noexcept = delete;
    Fa_FileManager& operator=(Fa_FileManager&) noexcept = delete;

    ~Fa_FileManager() = default;

    Fa_StringRef load(std::string const& filepath, bool const replace = false);

    Fa_StringRef& buffer() { return m_input_buffer; }
    Fa_StringRef const& buffer() const { return m_input_buffer; }

    std::string get_path() const { return m_file_path; }

    Fa_StringRef get_line_at(u32 const line_idx) const;

private:
    std::string m_file_path;
    Fa_StringRef m_input_buffer;
    std::filesystem::file_time_type m_last_known_write_time;
}; // class Fa_FileManager

class Fa_SourceManager {
public:
    explicit Fa_SourceManager() = default;

    explicit Fa_SourceManager(Fa_FileManager* fm)
        : m_file_manager(fm)
    {
        if (m_file_manager == nullptr)
            diagnostic::fatal_error(diagnostic::errc::general::Code::INTERNAL_ERROR);

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

    Fa_StringRef get_line_at(u32 const line_idx) const { return m_file_manager->get_line_at(line_idx); }

    Fa_SourceLocation get_source_location() const { return m_context; }

    Fa_StringRef source_slice(u64 const start, u64 const end) { return m_file_manager->buffer().slice(start, end); }

    void refresh_current_();

private:
    struct PushbackEntry {
        u32 ch { 0 };
        Fa_SourceLocation ctx;
        u64 bytes { 0 };
    }; // struct PushBackEntry

    Fa_FileManager* m_file_manager { nullptr };
    Fa_SourceLocation m_context;
    u32 m_current { 0 };
    u64 m_current_bytes { 0 };
    std::stack<PushbackEntry> m_unget_stack;

    void advance(u32 const cp, u64 const bytes);
    void rewind_position_(u32 const cp, u64 const bytes);
    u32 calculate_column_at_offset(u64 const target_offset) const;
}; // class Fa_SourceManager

class Fa_Lexer {
public:
    explicit Fa_Lexer() = default;
    explicit Fa_Lexer(Fa_FileManager* m_file_manager);
    explicit Fa_Lexer(Fa_Lexer const&) = delete;
    explicit Fa_Lexer(Fa_Array<TokenPtr>& seq);

    TokenPtr operator()() { return next(); }
    TokenPtr current() const;
    TokenPtr next();
    TokenPtr peek(size_t n = 1);
    Fa_Array<TokenPtr> tokenize();
    Fa_StringRef get_line_at(u32 const line_idx) const { return m_source_manager.get_line_at(line_idx); }

private:
    Fa_SourceManager m_source_manager;
    size_t m_tok_index { 0 };
    u32 m_indent_size { 0 };
    u32 m_indent_level { 0 };
    Fa_Array<TokenPtr> m_tok_stream;
    Fa_Array<u32> m_indent_stack;
    Fa_Array<u32> m_alt_indent_stack;
    bool m_at_bol { true };
    u32 m_bracket_depth { 0 };

    // main lexer loop
    TokenPtr lex_token();

    void store(TokenPtr tok);
}; // class Fa_Lexer

} // namespace fairuz::lex

#endif // FA_LEXER_HPP
