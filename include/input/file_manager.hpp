#pragma once

#include "../macros.hpp"

#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>


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
  switch (error)
  {
  case FileManagerError::FILE_NOT_FOUND : return "File not found";
  case FileManagerError::FILE_NOT_OPEN : return "File is not open";
  case FileManagerError::SEEK_OUT_OF_BOUND : return "Seek position out of bounds";
  case FileManagerError::READ_ERROR : return "Failed to read from file";
  case FileManagerError::INVALID_UTF8 : return "Invalid UTF-8 sequence encountered";
  case FileManagerError::INVALID_CHAR_OFFSET : return "Invalid character offset";
  case FileManagerError::PERMISSION_DENIED : return "Permission denied";
  case FileManagerError::UNEXPECTED_EOF : return "Unexpected end of file";
  case FileManagerError::SYSTEM_ERROR : return "System error occurred";
  case FileManagerError::ENCODING_ERROR : return "Encoding conversion error";
  case FileManagerError::CACHE_ERROR : return "Cache operation failed";
  case FileManagerError::INVALID_LINE_NUMBER : return "Invalid line number";
  case FileManagerError::BUFFER_TOO_SMALL : return "Buffer too small for operation";
  default : return "Unknown error";
  }
}

class FileManager
{
 public:
  struct Context
  {
    std::size_t ByteOffset{0};
    std::size_t CharOffset{0};
    std::size_t line{0};
    std::size_t column{0};

    void reset() MYLANG_NOEXCEPT
    {
      ByteOffset = 0;
      CharOffset = 0;
      line = 0;
      column = 0;
    }
  };

  struct LineIndex
  {
    std::size_t ByteOffset{0};
    std::size_t CharOffset{0};
    std::size_t LineLength{0};
  };

  struct FileStats
  {
    std::size_t TotalBytes{0};
    std::size_t TotalLines{0};
    std::size_t MaxLineLength{0};
    std::size_t AverageLineLength{0};
    std::size_t TotalCharacters{0};
    std::filesystem::file_time_type LastModified;
  };

  explicit FileManager(const std::string& filepath);

  FileManager(FileManager&&) MYLANG_NOEXCEPT = delete;
  FileManager& operator=(FileManager&&) MYLANG_NOEXCEPT = delete;

  FileManager(const FileManager&) MYLANG_NOEXCEPT = delete;
  FileManager& operator=(FileManager&) MYLANG_NOEXCEPT = delete;

  ~FileManager() = default;

  bool isOpen() const MYLANG_NOEXCEPT { return Stream_.is_open() /*&& Stream_.good()*/; }

  bool isChangedSinceLastTime() const MYLANG_NOEXCEPT { return fs::last_write_time(FullPath_) != LastKnownWriteTime_; }

  Context getContext() const MYLANG_NOEXCEPT { return Context_; }

  std::string getPath() const MYLANG_NOEXCEPT { return FullPath_; }

  void reset();

  void seekToChar(const std::size_t CharOffset);

  void seekToLine(const std::size_t line_number)
  {
    /// TODO:
  }

  StringType readWindow(const std::size_t size)
  {
    /// TODO: : add cache validation
    return readWindowInternal(size);
  }

  StringType readWindowInternal(const std::size_t size);

  StringType readLine(const std::size_t line_number);

  StringType readNextLine();

  std::vector<StringType> readLines(const std::size_t start, const std::size_t count);

  StringType readAll();

  void refreshStats();

  std::size_t getLineCount()
  {
    if (!LineIndexBuilt_) buildLineIndex();
    return Stats_.TotalLines;
  }

  std::size_t getCharCount()
  {
    if (!LineIndexBuilt_) buildLineIndex();
    return Stats_.TotalCharacters;
  }

  char16_t peekChar(const std::size_t CharOffset);

  StringType peekRange(const std::size_t start_offset, const std::size_t length);

  std::size_t remaining() { return static_cast<std::size_t>(Stream_.tellg()) - Context_.ByteOffset; }

  std::size_t fileSize() { return static_cast<std::size_t>(Stream_.tellg()); }

 private:
  std::string FullPath_;
  std::ifstream Stream_;
  Context Context_;
  std::vector<Context> PositionStack_;
  std::vector<LineIndex> LineIndices_;
  FileStats Stats_;

  // private constants
  static MYLANG_CONSTEXPR std::size_t DEFAULT_BUFFER_SIZE = 8192;
  static MYLANG_CONSTEXPR std::size_t MAX_UTF8_CHAR_BYTES = 8;
  static MYLANG_CONSTEXPR std::size_t SMALL_FILE_THRESHOLD = 1024 * 1024;        // 1 MB
  static MYLANG_CONSTEXPR std::size_t LARGE_FILE_THRESHOLD = 1024 * 1024 * 100;  // 100 MB
  static MYLANG_CONSTEXPR std::size_t LINE_INDEX_CHUNK = 10;

  bool LineIndexBuilt_{false};
  std::filesystem::file_time_type LastKnownWriteTime_;

  void popPosition()
  {
    if (!PositionStack_.empty()) PositionStack_.erase(PositionStack_.begin());
  }

  void pushPosition()
  {
    PositionStack_.push_back(Context_);
    // Limit stack size
    if (PositionStack_.size() > 100) PositionStack_.erase(PositionStack_.begin());
  }

  std::size_t validateUtf8Bound(std::span<const char> buffer) const;

  void buildLineIndex();

  FileStats computeStats();

  bool isChangedSinceLastRead() const { return fs::last_write_time(FullPath_) != LastKnownWriteTime_; }
};

}
}