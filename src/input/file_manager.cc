#include "../../include/input/file_manager.hpp"
#include "../../include/diag/diagnostic.hpp"
#include "../../include/input/error.hpp"
#include "../../utfcpp/source/utf8.h"

#include <filesystem>
#include <iostream>


namespace fs = std::filesystem;


namespace mylang {
namespace input {

FileManager::FileManager(const std::string& filepath) :
    full_path_(filepath),
    stream_(full_path_, std::ios::binary)
{
  if (!fs::exists(fs::path(filepath))) diagnostic::engine.panic(to_string(FileManagerError::FILE_NOT_FOUND));
  if (!stream_.is_open()) diagnostic::engine.panic(to_string(FileManagerError::FILE_NOT_OPEN));
  last_known_write_time_ = fs::last_write_time(filepath);
}

bool FileManager::is_open() const noexcept
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::is_open() called!" << std::endl;
#endif
  bool ret = stream_.is_open() /*&& stream_.good()*/;
#if DEBUG_PRINT
  if (!ret) diagnostic::engine.emit("-- DEBUG : FileManager::is_open() returns 'false'");

#endif
  return ret;
}

bool FileManager::is_changed_since_last_time() const noexcept
{
#if DEBUG_PRINT
  // std::cout << "-- DEBUG : FileManager::is_changed_since_last_time() called!" << std::endl;
  diagnostic::engine.emit("-- DEBUG : FileManager::is_open() return 'false'");
#endif
  auto current_write_time = fs::last_write_time(full_path_);
  return current_write_time != last_known_write_time_;
}

typename FileManager::Context FileManager::get_context() const noexcept
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::get_context() called!" << std::endl;
#endif
  return context_;
}

void FileManager::reset()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::reset() called!" << std::endl;
#endif
  if (!is_open()) throw error::file_error(to_string(FileManagerError::FILE_NOT_FOUND));
  stream_.clear();
  stream_.seekg(0, std::ios::beg);
  if (stream_.fail()) throw error::file_error(to_string(FileManagerError::SEEK_OUT_OF_BOUND));
  context_.reset();
}

void FileManager::seek_to_char(const std::size_t char_offset)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::seek_to_char() called!" << std::endl;
#endif
  if (char_offset == context_.char_offset) return;
  /// @todo  : revisit this stupid magic number
  if (char_offset < context_.char_offset || char_offset - context_.char_offset > 10000) reset();
  while (context_.char_offset < char_offset)
  {
    const std::size_t chars_to_read = char_offset - context_.char_offset;
    string_type result = read_window_internal(std::min(chars_to_read, std::size_t{1024}));
    if (result.empty()) throw error::file_error(to_string(FileManagerError::UNEXPECTED_EOF));
  }
}

void FileManager::seek_to_line(const std::size_t line_number)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::seek_to_line() called!" << std::endl;
#endif
}

string_type FileManager::read_window(const std::size_t size)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::read_window() called!" << std::endl;
#endif
  /// @todo : add cache validation
  return read_window_internal(size);
}

constexpr std::size_t get_utf8_sequence_length(unsigned char first_byte)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::get_utf8_sequence_length() called!" << std::endl;
#endif
  if ((first_byte & 0x80) == 0) return 1;
  if ((first_byte & 0xE0) == 0xC0) return 2;
  if ((first_byte & 0xF0) == 0xE0) return 3;
  if ((first_byte & 0xF8) == 0xF0) return 4;
  return 1;
}

std::size_t FileManager::validate_utf8_bound(std::span<const char> buffer) const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::validate_utf8_bound() called!" << std::endl;
#endif
  if (buffer.empty()) return 0;
  const std::size_t size = buffer.size();
  for (std::size_t i = 0, n = std::min(MAX_UTF8_CHAR_BYTES, size); i < n; ++i)
  {
    const std::size_t pos = size - 1 - i;
    const unsigned char byte = static_cast<unsigned char>(buffer[pos]);
    if ((byte & 0x80) == 0) return 0;
    if ((byte & 0xC0) == 0xC0)
    {
      const std::size_t expected = get_utf8_sequence_length(byte);
      const std::size_t available = size - pos;
      if (available < expected) return i + 1;
      return 0;
    }
  }
  return std::min(MAX_UTF8_CHAR_BYTES, size);
}

string_type FileManager::read_window_internal(const std::size_t size)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::read_window_internal() called!" << std::endl;
#endif
  if (size == 0)
  {
#if DEBUG_PRINT
    diagnostic::engine.emit("-- DEBUG : argument 'size' is zero, an empty string is returned!");
#endif
    return string_type{};
  }
  if (!is_open()) throw error::file_error(to_string(FileManagerError::FILE_NOT_OPEN));
  const std::size_t byte_chunk_size = size * MAX_UTF8_CHAR_BYTES;
  std::vector<char> byte_buffer(byte_chunk_size);
  stream_.read(byte_buffer.data(), byte_chunk_size);
  std::streamsize bytes_read = stream_.gcount();
  if (bytes_read == 0)
  {
    if (stream_.eof())
    {
#if DEBUG_PRINT
      diagnostic::engine.emit("-- DEBUG : stream is empty!");
#endif
      return string_type{};
    }
    else if (byte_buffer.empty()) { throw error::file_error(to_string(FileManagerError::READ_ERROR)); }
  }
  if (bytes_read < 0) throw error::file_error(to_string(FileManagerError::READ_ERROR));
  byte_buffer.resize(static_cast<std::size_t>(bytes_read));
  const std::size_t bytes_to_rewind = validate_utf8_bound(byte_buffer);
  if (bytes_to_rewind > 0)
  {
    byte_buffer.resize(bytes_read - bytes_to_rewind);
    stream_.seekg(-static_cast<std::streamoff>(bytes_to_rewind), std::ios_base::cur);
    if (stream_.fail()) throw error::file_error(to_string(FileManagerError::SEEK_OUT_OF_BOUND));
  }
  const std::size_t valid_bytes = byte_buffer.size();
  context_.byte_offset += valid_bytes;
  // context_.bytes_read_total += valid_bytes
  string_type result = utf8::utf8to16(std::string(byte_buffer.begin(), byte_buffer.end()));
  if (result.size() > size)
  {
    const std::size_t chars_to_trim = result.size() - size;
    std::string trimmed_utf8;
    auto start = result.begin() + static_cast<std::ptrdiff_t>(size);
    utf8::utf16to8(start, result.end(), std::back_inserter(trimmed_utf8));
    stream_.seekg(-static_cast<std::streamoff>(trimmed_utf8.size()), std::ios_base::cur);
    context_.byte_offset -= trimmed_utf8.size();
    result.resize(size);
  }
  context_.char_offset += result.size();
  // context_.chars_read_total += result.size();
#if DEBUG_PRINT
  std::cout << "--DEBUG : result = " << utf8::utf16to8(result) << std::endl;
#endif
  return result;
}

string_type FileManager::read_line(const std::size_t line_number)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::read_line() called!" << std::endl;
#endif
  seek_to_line(line_number);
  const std::size_t line_length = line_indices_[line_number].line_length;
  return read_window_internal(line_length);
}

string_type FileManager::read_next_line()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::read_next_line() called!" << std::endl;
#endif
  string_type line;
  line.reserve(100);  // average
  while (true)
  {
    // check line is not empty
    string_type chunk = read_window_internal(1);
    if (chunk.empty()) break;
    const char16_t c = chunk[0];
    if (c == u'\n')
    {
      context_.line += 1;
      context_.column = 0;
      break;
    }
    if (c == u'\r')
    {
      string_type peek = read_window_internal(1);
      if (!peek.empty() && peek[0] == u'\n')
      {
        // consume \n
      }
      else
      {
        // rewind
        stream_.seekg(-1, std::ios_base::cur);
        context_.byte_offset -= 1;
        context_.char_offset -= 1;
      }
      context_.line += 1;
      context_.column = 0;
      break;
    }
    line += c;
    context_.column += 1;
  }
  return line;
}

std::vector<string_type> FileManager::read_lines(const std::size_t start, const std::size_t count)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::read_lines() called!" << std::endl;
#endif
  if (!line_index_built_) build_line_index();
  if (start >= line_indices_.size()) throw error::file_error(to_string(FileManagerError::INVALID_LINE_NUMBER));
  const std::size_t end = std::min(start + count, line_indices_.size());
  std::vector<string_type> lines;
  lines.reserve(end - start);
  for (std::size_t i = start; i < end; ++i)
    lines.push_back(std::move(read_line(i)));
  return lines;
}

string_type FileManager::read_all()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::read_all() called!" << std::endl;
#endif
  reset();
  string_type result;
  result.reserve(stats_.total_bytes / 2);
  while (true)
  {
    auto chunk = read_window_internal(4096);  // 4 MB
    if (chunk.empty()) break;
    result.append(chunk);
  }
  return result;
}

typename FileManager::FileStats FileManager::compute_stats()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::compute_stats() called!" << std::endl;
#endif
  FileStats stats;
  stats.total_bytes = fs::file_size(full_path_);
  stats.last_modified = fs::last_write_time(full_path_);
  /// @todo : detect file encoding
  return stats;
}

bool FileManager::is_changed_since_last_read() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::is_changed_since_last_read() called!" << std::endl;
#endif
  return fs::last_write_time(full_path_) != last_known_write_time_;
}

void FileManager::refresh_stats()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::refresh_stats() called!" << std::endl;
#endif
  auto new_stats = compute_stats();
  stats_ = new_stats;
  last_known_write_time_ = stats_.last_modified;
  // Invalidate line index if file changed
  if (is_changed_since_last_read())
  {
    line_indices_.clear();
    line_index_built_ = false;
    // cache_.clear();
  }
}

std::size_t FileManager::get_line_count()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::get_line_count() called!" << std::endl;
#endif
  if (!line_index_built_) build_line_index();
  return stats_.total_lines;
}

std::size_t FileManager::get_char_count()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::get_char_count() called!" << std::endl;
#endif
  if (!line_index_built_) build_line_index();
  return stats_.total_characters;
}

void FileManager::push_position()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::push_position() called!" << std::endl;
#endif
  position_stack_.push_back(context_);
  // Limit stack size
  if (position_stack_.size() > 100) position_stack_.erase(position_stack_.begin());
}

void FileManager::pop_position()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::pop_position() called!" << std::endl;
#endif
  if (!position_stack_.empty()) position_stack_.erase(position_stack_.begin());
}

char16_t FileManager::peek_char(const std::size_t char_offset)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::peek_to_char() called!" << std::endl;
#endif
  push_position();
  seek_to_char(char_offset);
  return read_window_internal(1)[0];
}

string_type FileManager::peek_range(const std::size_t start_offset, const std::size_t length)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::peek_range() called!" << std::endl;
#endif
  push_position();
  seek_to_char(start_offset);
  auto result = read_window_internal(length);
  pop_position();
  return result;
}

std::string FileManager::get_path() const noexcept
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::get_path() called!" << std::endl;
#endif
  return full_path_;
}

void FileManager::build_line_index()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::build_line_index() called!" << std::endl;
#endif
  if (line_index_built_) return;
  reset();
  line_indices_.clear();
  line_indices_.reserve(stats_.total_bytes / 80);  // roughly
  std::size_t byte_pos = 0;
  std::size_t char_pos = 0;
  std::size_t start_byte = 0;
  std::size_t start_char = 0;
  std::size_t max_len = 0;
  std::size_t current_len = 0;
  while (true)
  {
    auto chunk = read_window_internal(LINE_INDEX_CHUNK);
    if (chunk.empty()) break;

    for (auto c : chunk)
    {
      current_len += 1;
      if (c == u'\n' || c == u'\r')
      {
        max_len = std::max(max_len, current_len - 1);
        line_indices_.push_back(LineIndex{start_byte, start_char, current_len - 1});
        /// @todo: Handle CRLF
        if (c == u'\r' && char_pos + 1 < context_.char_offset)
        {
          // Peek next char (already in chunk if available)
          // This is simplified; in practice we'd need to handle this better
        }
        start_byte = context_.byte_offset;
        start_char = context_.char_offset;
        current_len = 0;
      }
      char_pos += 1;
    }
  }
  if (current_len > 0)
  {
    max_len = std::max(max_len, current_len);
    line_indices_.push_back(LineIndex{start_byte, start_char, current_len});
  }
  stats_.total_lines = line_indices_.size();
  stats_.max_line_length = max_len;
  if (stats_.total_lines > 0) stats_.average_line_length = stats_.total_characters / stats_.total_lines;
  line_index_built_ = true;
  reset();
}

}
}