#ifndef LEXER_HPP
#define LEXER_HPP

#include "array.hpp"
#include "token.hpp"

#include <filesystem>
#include <stack>

#define MAKE_TOKEN(t, l, ln, c) get_allocator().allocate_object<tok::Fa_Token>(l, t, ln, c)

namespace fairuz::lex {

enum class FileManagerError {
    FILE_NOT_FOUND,
    FILE_NOT_OPEN,
    SEEK_OUT_OF_BOUND,
    READ_ERROR,
    INVALID_UTF8,
    INVALID_CHAR_OFFSET,
    PERMISSION_DENIED,
    UNEXPECTED_EOF,
    SYSTEM_ERROR,
    ENCODING_ERROR,
    CACHE_ERROR,
    INVALID_LINE_NUMBER,
    BUFFER_TOO_SMALL
}; // enum FileManagerError

static constexpr char const* to_string(FileManagerError error) noexcept
{
    switch (error) {
    case FileManagerError::FILE_NOT_FOUND:
        return "File not found";
    case FileManagerError::FILE_NOT_OPEN:
        return "File is not open";
    case FileManagerError::SEEK_OUT_OF_BOUND:
        return "Seek position out of bounds";
    case FileManagerError::READ_ERROR:
        return "Failed to read from file";
    case FileManagerError::INVALID_UTF8:
        return "Invalid UTF-8 sequence encountered";
    case FileManagerError::INVALID_CHAR_OFFSET:
        return "Invalid character offset";
    case FileManagerError::PERMISSION_DENIED:
        return "Permission denied";
    case FileManagerError::UNEXPECTED_EOF:
        return "Unexpected end of file";
    case FileManagerError::SYSTEM_ERROR:
        return "System error occurred";
    case FileManagerError::ENCODING_ERROR:
        return "Encoding conversion error";
    case FileManagerError::CACHE_ERROR:
        return "Cache operation failed";
    case FileManagerError::INVALID_LINE_NUMBER:
        return "Invalid line number";
    case FileManagerError::BUFFER_TOO_SMALL:
        return "Buffer too small for operation";
    default:
        return "Unknown error";
    }
}

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

    Fa_StringRef get_line_at(u32 const line_idx) const
    {
        if (line_idx == 0 || m_input_buffer.empty())
            return { };

        char const* data = m_input_buffer.data();
        size_t const m_size = m_input_buffer.len();

        u32 current_line = 1;
        size_t line_start = 0;

        for (size_t i = 0; i <= m_size; i += 1) {
            if (i == m_size || data[i] == '\n') {
                if (current_line == line_idx) {
                    size_t len = i - line_start;
                    if (len > 0 && data[line_start + len - 1] == '\r')
                        len -= 1;

                    return Fa_StringRef(m_input_buffer.get(), line_start, len);
                }
                current_line += 1;
                line_start = i + 1;
            }
        }

        return { };
    }

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
        if (!m_file_manager)
            throw std::runtime_error("Fa_SourceManager: Fa_FileManager is null");

        reset();
    }

    void reset();

    u32 get_line_number() const { return m_context.m_line; }
    u32 get_column_number() const { return m_context.m_column; }
    u64 get_file_offset() const { return m_context.m_offset; }
    std::string fpath() const noexcept { return m_file_manager->get_path(); }
    bool done() const { return m_context.m_offset >= m_file_manager->buffer().len(); }
    [[nodiscard]] u32 peek_char();
    u32 current_char() const;
    void consume_char();
    u32 next_char();
    void unget(u32 const cp);
    Fa_StringRef get_line_at(u32 const line_idx) const { return m_file_manager->get_line_at(line_idx); }

private:
    struct Context {
        u32 m_line { 1 };
        u32 m_column { 1 };
        u64 m_offset { 0 }; // byte offset
    }; // struct Context

    struct PushbackEntry {
        u32 ch { 0 };
        Context ctx;
        u64 bytes { 0 };
    }; // struct PushBackEntry

    Fa_FileManager* m_file_manager { nullptr };
    Context m_context;
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
    explicit Fa_Lexer(Fa_Array<tok::Fa_Token const*>& seq /*, size_t const s*/);

    tok::Fa_Token const* operator()() { return m_next(); }
    tok::Fa_Token const* m_current() const;
    tok::Fa_Token const* m_next();
    tok::Fa_Token const* peek(size_t n = 1);
    Fa_Array<tok::Fa_Token const*> tokenize();
    Fa_StringRef get_line_at(u32 const line_idx) const { return m_source_manager.get_line_at(line_idx); }

private:
    Fa_SourceManager m_source_manager;
    size_t m_tok_index { 0 };
    unsigned int m_indent_size { 0 };
    unsigned int m_indent_level { 0 };
    Fa_Array<tok::Fa_Token const*> m_tok_stream;
    Fa_Array<unsigned int> m_indent_stack;
    Fa_Array<unsigned int> m_alt_indent_stack;
    bool m_at_bol { true };

    // main lexer loop
    tok::Fa_Token const* lex_token();

    void store(tok::Fa_Token const* tok);
}; // class Fa_Lexer

} // namespace fairuz::lex

#endif // LEXER_HPP
