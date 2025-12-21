#pragma once

#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include "../macros.hpp"

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
constexpr const char* to_string(FileManagerError error) noexcept
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
    std::size_t byte_offset{0};
    std::size_t char_offset{0};
    std::size_t line{0};
    std::size_t column{0};

    void reset() noexcept
    {
      byte_offset = 0;
      char_offset = 0;
      line = 0;
      column = 0;
    }
  };

  struct LineIndex
  {
    std::size_t byte_offset{0};
    std::size_t char_offset{0};
    std::size_t line_length{0};
  };

  struct FileStats
  {
    std::size_t total_bytes{0};
    std::size_t total_lines{0};
    std::size_t max_line_length{0};
    std::size_t average_line_length{0};
    std::size_t total_characters{0};
    std::filesystem::file_time_type last_modified;
  };

  explicit FileManager(const std::string& filepath);

  FileManager(FileManager&&) noexcept = delete;
  FileManager& operator=(FileManager&&) noexcept = delete;

  FileManager(const FileManager&) noexcept = delete;
  FileManager& operator=(FileManager&) noexcept = delete;

  ~FileManager() = default;

  bool is_open() const noexcept;
  bool is_changed_since_last_time() const noexcept;
  Context get_context() const noexcept;

  void reset();

  void seek_to_char(const std::size_t char_offset);
  void seek_to_line(const std::size_t line_number);

  string_type read_window(const std::size_t size);
  string_type read_window_internal(const std::size_t size);
  string_type read_line(const std::size_t line_number);
  string_type read_next_line();
  std::vector<string_type> read_lines(const std::size_t start, const std::size_t count);
  string_type read_all();
  void refresh_stats();
  std::size_t get_line_count();
  std::size_t get_char_count();
  char16_t peek_char(const std::size_t char_offset);
  string_type peek_range(const std::size_t start_offset, const std::size_t length);
  std::string get_path() const noexcept;
  std::size_t remaining() { return static_cast<std::size_t>(stream_.tellg()) - context_.byte_offset; }
  std::size_t file_size() { return static_cast<std::size_t>(stream_.tellg()); }

 private:
  std::string full_path_;
  std::ifstream stream_;
  Context context_;
  std::vector<Context> position_stack_;
  std::vector<LineIndex> line_indices_;
  FileStats stats_;

  // private constants
  static constexpr std::size_t DEFAULT_BUFFER_SIZE = 8192;
  static constexpr std::size_t MAX_UTF8_CHAR_BYTES = 8;
  static constexpr std::size_t SMALL_FILE_THRESHOLD = 1024 * 1024;  // 1 MB
  static constexpr std::size_t LARGE_FILE_THRESHOLD = 1024 * 1024 * 100;  // 100 MB
  static constexpr std::size_t LINE_INDEX_CHUNK = 10;

  bool line_index_built_{false};
  std::filesystem::file_time_type last_known_write_time_;

  void pop_position();
  void push_position();
  std::size_t validate_utf8_bound(std::span<const char> buffer) const;
  void build_line_index();
  FileStats compute_stats();
  bool is_changed_since_last_read() const;
};

}
}