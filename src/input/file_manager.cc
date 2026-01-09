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
    FullPath_(filepath),
    Stream_(FullPath_, std::ios::binary)
{
  if (!fs::exists(fs::path(filepath))) diagnostic::engine.panic(toString(FileManagerError::FILE_NOT_FOUND));
  if (!Stream_.is_open()) diagnostic::engine.panic(toString(FileManagerError::FILE_NOT_OPEN));
  LastKnownWriteTime_ = fs::last_write_time(filepath);
}

bool FileManager::isOpen() const noexcept
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::isOpen() called!" << std::endl;
#endif
  bool ret = Stream_.is_open() /*&& Stream_.good()*/;
#if DEBUG_PRINT
  if (!ret) diagnostic::engine.emit(". DEBUG : FileManager::isOpen() returns 'false'");

#endif
  return ret;
}

bool FileManager::isChangedSinceLastTime() const noexcept
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::isChangedSinceLastTime() called!" << std::endl;
  diagnostic::engine.emit(". DEBUG : FileManager::isOpen() return 'false'");
#endif
  return fs::last_write_time(FullPath_) != LastKnownWriteTime_;
}

typename FileManager::Context FileManager::getContext() const noexcept
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::get_context() called!" << std::endl;
#endif
  return Context_;
}

void FileManager::reset()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::reset() called!" << std::endl;
#endif
  if (!isOpen()) throw error::FileError(toString(FileManagerError::FILE_NOT_FOUND));
  Stream_.clear();
  Stream_.seekg(0, std::ios::beg);
  if (Stream_.fail()) throw error::FileError(toString(FileManagerError::SEEK_OUT_OF_BOUND));
  Context_.reset();
}

void FileManager::seekToChar(const std::size_t CharOffset)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::seek_to_char() called!" << std::endl;
#endif
  if (CharOffset == Context_.CharOffset) return;
  /// @todo  : revisit this stupid magic number
  if (CharOffset < Context_.CharOffset || CharOffset - Context_.CharOffset > 10000) reset();
  while (Context_.CharOffset < CharOffset)
  {
    const std::size_t chars_to_read = CharOffset - Context_.CharOffset;
    StringType result = readWindowInternal(std::min(chars_to_read, std::size_t{1024}));
    if (result.empty()) throw error::FileError(toString(FileManagerError::UNEXPECTED_EOF));
  }
}

void FileManager::seekToLine(const std::size_t line_number)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::seekToLine() called!" << std::endl;
#endif
}

StringType FileManager::readWindow(const std::size_t size)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::readWindow() called!" << std::endl;
#endif
  /// @todo : add cache validation
  return readWindowInternal(size);
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

std::size_t FileManager::validateUtf8Bound(std::span<const char> buffer) const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::validateUtf8Bound() called!" << std::endl;
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

StringType FileManager::readWindowInternal(const std::size_t size)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::readWindowInternal() called!" << std::endl;
#endif
  if (size == 0)
  {
#if DEBUG_PRINT
    diagnostic::engine.emit("-- DEBUG : argument 'size' is zero, an empty string is returned!");
#endif
    return StringType{};
  }
  if (!isOpen()) throw error::FileError(toString(FileManagerError::FILE_NOT_OPEN));
  const std::size_t byte_chunk_size = size * MAX_UTF8_CHAR_BYTES;
  std::vector<char> byte_buffer(byte_chunk_size);
  Stream_.read(byte_buffer.data(), byte_chunk_size);
  std::streamsize bytes_read = Stream_.gcount();
  if (bytes_read == 0)
  {
    if (Stream_.eof())
    {
#if DEBUG_PRINT
      diagnostic::engine.emit("-- DEBUG : stream is empty!");
#endif
      return StringType{};
    }
    else if (byte_buffer.empty()) { throw error::FileError(toString(FileManagerError::READ_ERROR)); }
  }
  if (bytes_read < 0) throw error::FileError(toString(FileManagerError::READ_ERROR));
  byte_buffer.resize(static_cast<std::size_t>(bytes_read));
  const std::size_t bytes_to_rewind = validateUtf8Bound(byte_buffer);
  if (bytes_to_rewind > 0)
  {
    byte_buffer.resize(bytes_read - bytes_to_rewind);
    Stream_.seekg(-static_cast<std::streamoff>(bytes_to_rewind), std::ios_base::cur);
    if (Stream_.fail()) throw error::FileError(toString(FileManagerError::SEEK_OUT_OF_BOUND));
  }
  const std::size_t valid_bytes = byte_buffer.size();
  Context_.ByteOffset += valid_bytes;
  // Context_.bytes_read_total += valid_bytes
  StringType result = utf8::utf8to16(std::string(byte_buffer.begin(), byte_buffer.end()));
  if (result.size() > size)
  {
    const std::size_t chars_to_trim = result.size() - size;
    std::string trimmed_utf8;
    auto start = result.begin() + static_cast<std::ptrdiff_t>(size);
    utf8::utf16to8(start, result.end(), std::back_inserter(trimmed_utf8));
    Stream_.seekg(-static_cast<std::streamoff>(trimmed_utf8.size()), std::ios_base::cur);
    Context_.ByteOffset -= trimmed_utf8.size();
    result.resize(size);
  }
  Context_.CharOffset += result.size();
  // Context_.chars_read_total += result.size();
#if DEBUG_PRINT
  std::cout << "--DEBUG : result = " << utf8::utf16to8(result) << std::endl;
#endif
  return result;
}

StringType FileManager::readLine(const std::size_t line_number)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::readLine() called!" << std::endl;
#endif
  seekToLine(line_number);
  const std::size_t LineLength = LineIndices_[line_number].LineLength;
  return readWindowInternal(LineLength);
}

StringType FileManager::readNextLine()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::readNextLine() called!" << std::endl;
#endif
  StringType line;
  line.reserve(100);  // average
  while (true)
  {
    // check line is not empty
    StringType chunk = readWindowInternal(1);
    if (chunk.empty()) break;
    const char16_t c = chunk[0];
    if (c == u'\n')
    {
      Context_.line += 1;
      Context_.column = 0;
      break;
    }
    if (c == u'\r')
    {
      StringType peek = readWindowInternal(1);
      if (!peek.empty() && peek[0] == u'\n')
      {
        // consume \n
      }
      else
      {
        // rewind
        Stream_.seekg(-1, std::ios_base::cur);
        Context_.ByteOffset -= 1;
        Context_.CharOffset -= 1;
      }
      Context_.line += 1;
      Context_.column = 0;
      break;
    }
    line += c;
    Context_.column += 1;
  }
  return line;
}

std::vector<StringType> FileManager::readLines(const std::size_t start, const std::size_t count)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::readLines() called!" << std::endl;
#endif
  if (!LineIndexBuilt_) buildLineIndex();
  if (start >= LineIndices_.size()) throw error::FileError(toString(FileManagerError::INVALID_LINE_NUMBER));
  const std::size_t end = std::min(start + count, LineIndices_.size());
  std::vector<StringType> lines;
  lines.reserve(end - start);
  for (std::size_t i = start; i < end; ++i) lines.push_back(std::move(readLine(i)));
  return lines;
}

StringType FileManager::readAll()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::read_all() called!" << std::endl;
#endif
  reset();
  StringType result;
  result.reserve(Stats_.TotalBytes / 2);
  while (true)
  {
    StringType chunk = readWindowInternal(4096);  // 4 MB
    if (chunk.empty()) break;
    result.append(chunk);
  }
  return result;
}

typename FileManager::FileStats FileManager::computeStats()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::compute_stats() called!" << std::endl;
#endif
  FileStats stats;
  stats.TotalBytes = fs::file_size(FullPath_);
  stats.LastModified = fs::last_write_time(FullPath_);
  /// @todo : detect file encoding
  return stats;
}

bool FileManager::isChangedSinceLastRead() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::is_changed_since_last_read() called!" << std::endl;
#endif
  return fs::last_write_time(FullPath_) != LastKnownWriteTime_;
}

void FileManager::refreshStats()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::refreshStats() called!" << std::endl;
#endif
  FileStats new_stats = computeStats();
  Stats_ = new_stats;
  LastKnownWriteTime_ = Stats_.LastModified;
  // Invalidate line index if file changed
  if (isChangedSinceLastRead())
  {
    LineIndices_.clear();
    LineIndexBuilt_ = false;
    // cache_.clear();
  }
}

std::size_t FileManager::getLineCount()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::get_line_count() called!" << std::endl;
#endif
  if (!LineIndexBuilt_) buildLineIndex();
  return Stats_.TotalLines;
}

std::size_t FileManager::getCharCount()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::get_char_count() called!" << std::endl;
#endif
  if (!LineIndexBuilt_) buildLineIndex();
  return Stats_.TotalCharacters;
}

void FileManager::pushPosition()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::pushPosition() called!" << std::endl;
#endif
  PositionStack_.push_back(Context_);
  // Limit stack size
  if (PositionStack_.size() > 100) PositionStack_.erase(PositionStack_.begin());
}

void FileManager::popPosition()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::popPosition() called!" << std::endl;
#endif
  if (!PositionStack_.empty()) PositionStack_.erase(PositionStack_.begin());
}

char16_t FileManager::peekChar(const std::size_t CharOffset)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::peek_to_char() called!" << std::endl;
#endif
  pushPosition();
  seekToChar(CharOffset);
  return readWindowInternal(1)[0];
}

StringType FileManager::peekRange(const std::size_t start_offset, const std::size_t length)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::peek_range() called!" << std::endl;
#endif
  pushPosition();
  seekToChar(start_offset);
  StringType result = readWindowInternal(length);
  popPosition();
  return result;
}

std::string FileManager::getPath() const noexcept
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::get_path() called!" << std::endl;
#endif
  return FullPath_;
}

void FileManager::buildLineIndex()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : FileManager::buildLineIndex() called!" << std::endl;
#endif
  if (LineIndexBuilt_) return;
  reset();
  LineIndices_.clear();
  LineIndices_.reserve(Stats_.TotalBytes / 80);  // roughly
  std::size_t byte_pos = 0;
  std::size_t char_pos = 0;
  std::size_t start_byte = 0;
  std::size_t start_char = 0;
  std::size_t max_len = 0;
  std::size_t current_len = 0;
  while (true)
  {
    StringType chunk = readWindowInternal(LINE_INDEX_CHUNK);
    if (chunk.empty()) break;

    for (char16_t c : chunk)
    {
      current_len += 1;
      if (c == u'\n' || c == u'\r')
      {
        max_len = std::max(max_len, current_len - 1);
        LineIndices_.push_back(LineIndex{start_byte, start_char, current_len - 1});
        /// @todo: Handle CRLF
        if (c == u'\r' && char_pos + 1 < Context_.CharOffset)
        {
          // Peek next char (already in chunk if available)
          // This is simplified; in practice we'd need to handle this better
        }
        start_byte = Context_.ByteOffset;
        start_char = Context_.CharOffset;
        current_len = 0;
      }
      char_pos += 1;
    }
  }
  if (current_len > 0)
  {
    max_len = std::max(max_len, current_len);
    LineIndices_.push_back(LineIndex{start_byte, start_char, current_len});
  }
  Stats_.TotalLines = LineIndices_.size();
  Stats_.MaxLineLength = max_len;
  if (Stats_.TotalLines > 0) Stats_.AverageLineLength = Stats_.TotalCharacters / Stats_.TotalLines;
  LineIndexBuilt_ = true;
  reset();
}

}
}