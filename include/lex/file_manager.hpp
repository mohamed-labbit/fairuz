#pragma once

#include "../macros.hpp"
#include "../string.hpp"

#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace mylang {
namespace lex {

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

static constexpr char const* toString(FileManagerError error) noexcept
{
    switch (error) {
    case FileManagerError::FILE_NOT_FOUND: return "File not found";
    case FileManagerError::FILE_NOT_OPEN: return "File is not open";
    case FileManagerError::SEEK_OUT_OF_BOUND: return "Seek position out of bounds";
    case FileManagerError::READ_ERROR: return "Failed to read from file";
    case FileManagerError::INVALID_UTF8: return "Invalid UTF-8 sequence encountered";
    case FileManagerError::INVALID_CHAR_OFFSET: return "Invalid character offset";
    case FileManagerError::PERMISSION_DENIED: return "Permission denied";
    case FileManagerError::UNEXPECTED_EOF: return "Unexpected end of file";
    case FileManagerError::SYSTEM_ERROR: return "System error occurred";
    case FileManagerError::ENCODING_ERROR: return "Encoding conversion error";
    case FileManagerError::CACHE_ERROR: return "Cache operation failed";
    case FileManagerError::INVALID_LINE_NUMBER: return "Invalid line number";
    case FileManagerError::BUFFER_TOO_SMALL: return "Buffer too small for operation";
    default:
        return "Unknown error";
    }
}

class FileManager {
public:
    explicit FileManager(std::string const& filepath);

    FileManager(FileManager&&) noexcept = delete;
    FileManager& operator=(FileManager&&) noexcept = delete;

    FileManager(FileManager const&) noexcept = delete;
    FileManager& operator=(FileManager&) noexcept = delete;

    ~FileManager() = default;

    StringRef load(std::string const& filepath, bool const replace = false);

    StringRef& buffer() { return InputBuffer_; }

    StringRef const& buffer() const { return InputBuffer_; }

    std::string getPath() const { return FilePath_; }

private:
    std::string FilePath_;
    StringRef InputBuffer_;
    std::filesystem::file_time_type LastKnownWriteTime_;
};

}
}
