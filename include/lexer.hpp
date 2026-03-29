#ifndef LEXER_HPP
#define LEXER_HPP

#include "array.hpp"
#include "token.hpp"

#include <filesystem>
#include <stack>

#define MAKE_TOKEN(t, l, ln, c) getAllocator().allocateObject<tok::Fa_Token>(l, t, ln, c)

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

static constexpr char const* toString(FileManagerError error) noexcept
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
    Fa_StringRef& buffer() { return InputBuffer_; }
    Fa_StringRef const& buffer() const { return InputBuffer_; }
    std::string getPath() const { return FilePath_; }

    Fa_StringRef getLineAt(u32 const line_idx) const
    {
        if (line_idx == 0 || InputBuffer_.empty())
            return { };

        char const* data = InputBuffer_.data();
        size_t const size = InputBuffer_.len();

        u32 current_line = 1;
        size_t line_start = 0;

        for (size_t i = 0; i <= size; ++i) {
            if (i == size || data[i] == '\n') {
                if (current_line == line_idx) {
                    size_t len = i - line_start;
                    if (len > 0 && data[line_start + len - 1] == '\r')
                        --len;

                    return Fa_StringRef(InputBuffer_.get(), line_start, len);
                }
                ++current_line;
                line_start = i + 1;
            }
        }

        return { };
    }

private:
    std::string FilePath_;
    Fa_StringRef InputBuffer_;
    std::filesystem::file_time_type LastKnownWriteTime_;
}; // class Fa_FileManager

class Fa_SourceManager {
public:
    explicit Fa_SourceManager() = default;

    explicit Fa_SourceManager(Fa_FileManager* fm)
        : FileManager_(fm)
    {
        if (!FileManager_)
            throw std::runtime_error("Fa_SourceManager: Fa_FileManager is null");

        reset();
    }

    void reset();

    u32 getLineNumber() const { return Context_.line; }
    u32 getColumnNumber() const { return Context_.column; }
    u64 getFileOffset() const { return Context_.offset; }
    std::string fpath() const noexcept { return FileManager_->getPath(); }
    bool done() const { return Context_.offset >= FileManager_->buffer().len(); }
    [[nodiscard]] u32 peekChar();
    u32 currentChar() const;
    void consumeChar();
    u32 nextChar();
    void unget(u32 const cp);
    Fa_StringRef getLineAt(u32 const line_idx) const { return FileManager_->getLineAt(line_idx); }

private:
    struct Context {
        u32 line { 1 };
        u32 column { 1 };
        u64 offset { 0 }; // byte offset
    }; // struct Context

    struct PushbackEntry {
        u32 ch { 0 };
        Context ctx;
        u64 bytes { 0 };
    }; // struct PushBackEntry

    Fa_FileManager* FileManager_ { nullptr };
    Context Context_;
    u32 Current_ { 0 };
    u64 CurrentBytes_ { 0 };
    std::stack<PushbackEntry> UngetStack_;

    void advance(u32 const cp, u64 const bytes);
    void rewindPosition_(u32 const cp, u64 const bytes);
    u32 calculateColumnAtOffset(u64 const target_offset) const;
}; // class Fa_SourceManager

class Fa_Lexer {
public:
    explicit Fa_Lexer() = default;
    explicit Fa_Lexer(Fa_FileManager* file_manager);
    explicit Fa_Lexer(Fa_Lexer const&) = delete;
    explicit Fa_Lexer(Fa_Array<tok::Fa_Token const*>& seq /*, size_t const s*/);

    tok::Fa_Token const* operator()() { return next(); }
    tok::Fa_Token const* current() const;
    tok::Fa_Token const* next();
    tok::Fa_Token const* peek(size_t n = 1);
    Fa_Array<tok::Fa_Token const*> tokenize();
    Fa_StringRef getLineAt(u32 const line_idx) const { return SourceManager_.getLineAt(line_idx); }

private:
    Fa_SourceManager SourceManager_;
    size_t TokIndex_ { 0 };
    unsigned int IndentSize_ { 0 };
    unsigned int IndentLevel_ { 0 };
    Fa_Array<tok::Fa_Token const*> TokStream_;
    Fa_Array<unsigned int> IndentStack_;
    Fa_Array<unsigned int> AltIndentStack_;
    bool AtBOL_ { true };

    // main lexer loop
    tok::Fa_Token const* lexToken();

    void store(tok::Fa_Token const* tok);
}; // class Fa_Lexer

} // namespace fairuz::lex

#endif // LEXER_HPP
