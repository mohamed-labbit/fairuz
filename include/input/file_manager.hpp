#pragma once

#include "../macros.hpp"
#include "../types/string.hpp"
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace mylang {
namespace input {

/// @brief Error codes for FileManager operations
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
};

/// @brief Convert error code to human-readable string
MYLANG_CONSTEXPR const char* toString(FileManagerError error) MYLANG_NOEXCEPT
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

class FileManager {
public:
    struct Context {
        SizeType ByteOffset { 0 };
        SizeType CharOffset { 0 };
        SizeType line { 0 };
        SizeType column { 0 };

        void reset() MYLANG_NOEXCEPT
        {
            ByteOffset = 0;
            CharOffset = 0;
            line = 0;
            column = 0;
        }
    };

    struct LineIndex {
        SizeType ByteOffset { 0 };
        SizeType CharOffset { 0 };
        SizeType LineLength { 0 };
    };

    struct FileStats {
        SizeType TotalBytes { 0 };
        SizeType TotalLines { 0 };
        SizeType TotalCharacters { 0 };
    };

    explicit FileManager(std::string const& filepath);

    FileManager(FileManager&&) MYLANG_NOEXCEPT = delete;
    FileManager& operator=(FileManager&&) MYLANG_NOEXCEPT = delete;

    FileManager(FileManager const&) MYLANG_NOEXCEPT = delete;
    FileManager& operator=(FileManager&) MYLANG_NOEXCEPT = delete;

    ~FileManager() = default;

    bool isChangedSinceLastTime() const MYLANG_NOEXCEPT { return fs::last_write_time(FullPath_) != LastKnownWriteTime_; }

    Context getContext() const MYLANG_NOEXCEPT { return Context_; }

    std::string getPath() const MYLANG_NOEXCEPT { return FullPath_; }

    void reset();

    void seekToChar(SizeType const CharOffset);
    void seekToLine(SizeType const line_number);

    StringRef readWindow(SizeType const size)
    {
        return readWindowInternal(size);
    }

    StringRef readWindowInternal(SizeType size);

    StringRef readLine(SizeType const line_number);

    StringRef readNextLine();

    std::vector<StringRef> readLines(SizeType const start, SizeType const count);

    StringRef readAll();

    void refreshStats();

    SizeType getLineCount()
    {
        if (!LineIndexBuilt_)
            buildLineIndex();
        return Stats_.TotalLines;
    }

    SizeType getCharCount()
    {
        if (!LineIndexBuilt_)
            buildLineIndex();
        return Stats_.TotalCharacters;
    }

    CharType peekChar(SizeType const CharOffset);
    
    StringRef peekRange(SizeType const start_offset, SizeType const length);

    SizeType remaining() { return InputBuffer_.len() - Context_.ByteOffset; }

    SizeType fileSize() { return InputBuffer_.len() * sizeof(CharType); }

    StringRef getSourceLine(SizeType const line);

private:
    std::string FullPath_ { "" };
    Context Context_ {};
    std::vector<Context> PositionStack_ {};
    std::vector<LineIndex> LineIndices_ {};
    FileStats Stats_ {};
    StringRef InputBuffer_ {};

    // private constants
    static MYLANG_CONSTEXPR SizeType DEFAULT_BUFFER_SIZE = 8192;
    static MYLANG_CONSTEXPR SizeType MAX_UTF8_CHAR_BYTES = 8;
    static MYLANG_CONSTEXPR SizeType SMALL_FILE_THRESHOLD = 1024 * 1024;       // 1 MB
    static MYLANG_CONSTEXPR SizeType LARGE_FILE_THRESHOLD = 1024 * 1024 * 100; // 100 MB
    static MYLANG_CONSTEXPR SizeType LINE_INDEX_CHUNK = 10;

    bool LineIndexBuilt_ { false };
    std::filesystem::file_time_type LastKnownWriteTime_;

    void popPosition()
    {
        if (!PositionStack_.empty())
            PositionStack_.erase(PositionStack_.begin());
    }

    void pushPosition()
    {
        PositionStack_.push_back(Context_);
        // Limit stack size
        if (PositionStack_.size() > 100)
            PositionStack_.erase(PositionStack_.begin());
    }

    SizeType validateUtf8Bound(std::span<std::byte const> buffer) const;

    void buildLineIndex();

    FileStats computeStats();

    bool isChangedSinceLastRead() const { return fs::last_write_time(FullPath_) != LastKnownWriteTime_; }
};

}
}
