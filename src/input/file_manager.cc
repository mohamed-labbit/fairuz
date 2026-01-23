#include "../../include/input/file_manager.hpp"
#include "../../include/diag/diagnostic.hpp"
#include "../../include/input/error.hpp"
#include "../../utfcpp/source/utf8.h"

#include <filesystem>
#include <iostream>
#include <span>


namespace fs = std::filesystem;


namespace mylang {
namespace input {

FileManager::FileManager(const std::string& filepath) :
    FullPath_(filepath),
    Stream_(FullPath_, std::ios::binary)
{
  if (!fs::exists(fs::path(filepath)))
  {
    diagnostic::engine.panic(toString(FileManagerError::FILE_NOT_FOUND));
  }
  if (!Stream_.is_open())
  {
    diagnostic::engine.panic(toString(FileManagerError::FILE_NOT_OPEN));
  }
  LastKnownWriteTime_ = fs::last_write_time(filepath);
}

void FileManager::reset()
{
  if (!isOpen())
  {
    throw error::FileError(toString(FileManagerError::FILE_NOT_FOUND));
  }

  Stream_.clear();
  Stream_.seekg(0, std::ios::beg);

  if (Stream_.fail())
  {
    throw error::FileError(toString(FileManagerError::SEEK_OUT_OF_BOUND));
  }

  Context_.reset();
}

void FileManager::seekToChar(const SizeType CharOffset)
{
  if (CharOffset == Context_.CharOffset)
  {
    return;
  }

  /// TODO:  : revisit this stupid magic number
  if (CharOffset < Context_.CharOffset || CharOffset - Context_.CharOffset > 10000)
  {
    reset();
  }

  while (Context_.CharOffset < CharOffset)
  {
    const SizeType chars_to_read = CharOffset - Context_.CharOffset;
    StringType     result        = readWindowInternal(std::min(chars_to_read, SizeType{1024}));
    if (result.empty())
    {
      throw error::FileError(toString(FileManagerError::UNEXPECTED_EOF));
    }
  }
}

constexpr SizeType getUtf8SequenceLength(unsigned char first_byte)
{
  if ((first_byte & 0x80) == 0)
  {
    return 1;
  }
  if ((first_byte & 0xE0) == 0xC0)
  {
    return 2;
  }
  if ((first_byte & 0xF0) == 0xE0)
  {
    return 3;
  }
  if ((first_byte & 0xF8) == 0xF0)
  {
    return 4;
  }
  return 1;
}

SizeType FileManager::validateUtf8Bound(std::span<const char> buffer) const
{
  if (buffer.empty())
  {
    return 0;
  }

  const SizeType size = buffer.size();

  for (SizeType i = 0, n = std::min(MAX_UTF8_CHAR_BYTES, size); i < n; ++i)
  {
    const SizeType      pos  = size - 1 - i;
    const unsigned char byte = static_cast<unsigned char>(buffer[pos]);

    if ((byte & 0x80) == 0)
    {
      return 0;
    }

    if ((byte & 0xC0) == 0xC0)
    {
      const SizeType expected  = getUtf8SequenceLength(byte);
      const SizeType available = size - pos;

      if (available < expected)
      {
        return i + 1;
      }

      return 0;
    }
  }

  return std::min(MAX_UTF8_CHAR_BYTES, size);
}

StringType FileManager::readWindowInternal(const SizeType size)
{
  if (size == 0)
  {
    return StringType{};
  }

  if (!isOpen())
  {
    throw error::FileError(toString(FileManagerError::FILE_NOT_OPEN));
  }

  const SizeType    byte_chunk_size = size * MAX_UTF8_CHAR_BYTES;
  std::vector<char> byte_buffer(byte_chunk_size);
  Stream_.read(byte_buffer.data(), byte_chunk_size);
  std::streamsize bytes_read = Stream_.gcount();

  if (bytes_read == 0)
  {
    if (Stream_.eof())
    {
      return StringType{};
    }
    else if (byte_buffer.empty())
    {
      throw error::FileError(toString(FileManagerError::READ_ERROR));
    }
  }

  if (bytes_read < 0)
  {
    throw error::FileError(toString(FileManagerError::READ_ERROR));
  }

  byte_buffer.resize(static_cast<SizeType>(bytes_read));
  const SizeType bytes_to_rewind = validateUtf8Bound(byte_buffer);

  if (bytes_to_rewind > 0)
  {
    byte_buffer.resize(bytes_read - bytes_to_rewind);
    Stream_.seekg(-static_cast<std::streamoff>(bytes_to_rewind), std::ios_base::cur);

    if (Stream_.fail())
    {
      throw error::FileError(toString(FileManagerError::SEEK_OUT_OF_BOUND));
    }
  }

  const SizeType valid_bytes = byte_buffer.size();
  Context_.ByteOffset += valid_bytes;
  // Context_.bytes_read_total += valid_bytes
  StringType result = utf8::utf8to16(std::string(byte_buffer.begin(), byte_buffer.end()));

  if (result.size() > size)
  {
    const SizeType chars_to_trim = result.size() - size;
    std::string    trimmed_utf8;
    auto           start = result.begin() + static_cast<std::ptrdiff_t>(size);
    utf8::utf16to8(start, result.end(), std::back_inserter(trimmed_utf8));
    Stream_.seekg(-static_cast<std::streamoff>(trimmed_utf8.size()), std::ios_base::cur);
    Context_.ByteOffset -= trimmed_utf8.size();
    result.resize(size);
  }

  Context_.CharOffset += result.size();
  // Context_.chars_read_total += result.size();
  return result;
}

StringType FileManager::readLine(const SizeType line_number)
{
  seekToLine(line_number);
  const SizeType LineLength = LineIndices_[line_number].LineLength;
  return readWindowInternal(LineLength);
}

StringType FileManager::readNextLine()
{
  StringType line;
  line.reserve(100);  // average

  for (;;)
  {
    // check line is not empty
    StringType chunk = readWindowInternal(1);
    if (chunk.empty())
    {
      break;
    }

    const CharType c = chunk[0];

    if (c == u'\n')
    {
      Context_.line++;
      Context_.column = 0;
      break;
    }

    if (c == u'\r')
    {
      StringType peek = readWindowInternal(1);
      if (!peek.empty() && peek[0] == u'\n')
      {  // consume \n
        ;
      }
      else
      {
        // rewind
        Stream_.seekg(-1, std::ios_base::cur);
        Context_.ByteOffset--, Context_.CharOffset--;
      }
      Context_.line++;
      Context_.column = 0;
      break;
    }

    line += c;
    Context_.column++;
  }

  return line;
}

std::vector<StringType> FileManager::readLines(const SizeType start, const SizeType count)
{
  if (!LineIndexBuilt_)
  {
    buildLineIndex();
  }

  if (start >= LineIndices_.size())
  {
    throw error::FileError(toString(FileManagerError::INVALID_LINE_NUMBER));
  }

  const SizeType          end = std::min(start + count, LineIndices_.size());
  std::vector<StringType> lines;
  lines.reserve(end - start);

  for (SizeType i = start; i < end; ++i)
  {
    lines.push_back(std::move(readLine(i)));
  }

  return lines;
}

StringType FileManager::readAll()
{
  reset();
  StringType result;
  result.reserve(Stats_.TotalBytes / 2);

  for (;;)
  {
    StringType chunk = readWindowInternal(4096);  // 4 MB
    if (chunk.empty())
    {
      break;
    }
    result.append(chunk);
  }

  return result;
}

typename FileManager::FileStats FileManager::computeStats()
{
  FileStats stats;
  stats.TotalBytes   = fs::file_size(FullPath_);
  stats.LastModified = fs::last_write_time(FullPath_);
  /// TODO: : detect file encoding
  return stats;
}

void FileManager::refreshStats()
{
  FileStats new_stats = computeStats();
  Stats_              = new_stats;
  LastKnownWriteTime_ = Stats_.LastModified;
  // Invalidate line index if file changed
  if (isChangedSinceLastRead())
  {
    LineIndices_.clear();
    LineIndexBuilt_ = false;
    // cache_.clear();
  }
}

CharType FileManager::peekChar(const SizeType CharOffset)
{
  pushPosition();
  seekToChar(CharOffset);
  return readWindowInternal(1)[0];
}

StringType FileManager::peekRange(const SizeType start_offset, const SizeType length)
{
  pushPosition();
  seekToChar(start_offset);
  StringType result = readWindowInternal(length);
  popPosition();
  return result;
}

void FileManager::buildLineIndex()
{
  if (LineIndexBuilt_)
  {
    return;
  }

  reset();
  LineIndices_.clear();
  LineIndices_.reserve(Stats_.TotalBytes / 80);  // roughly

  SizeType byte_pos    = 0;
  SizeType char_pos    = 0;
  SizeType start_byte  = 0;
  SizeType start_char  = 0;
  SizeType max_len     = 0;
  SizeType current_len = 0;

  for (;;)
  {
    StringType chunk = readWindowInternal(LINE_INDEX_CHUNK);
    if (chunk.empty())
    {
      break;
    }

    for (CharType c : chunk)
    {
      current_len++;

      if (c == u'\n' || c == u'\r')
      {
        max_len = std::max(max_len, current_len - 1);
        LineIndices_.push_back(LineIndex{start_byte, start_char, current_len - 1});

        /// TODO:: Handle CRLF
        if (c == u'\r' && char_pos + 1 < Context_.CharOffset)
        {
          // Peek next char (already in chunk if available)
          // This is simplified; in practice we'd need to handle this better
        }

        start_byte  = Context_.ByteOffset;
        start_char  = Context_.CharOffset;
        current_len = 0;
      }

      char_pos++;
    }
  }

  if (current_len > 0)
  {
    max_len = std::max(max_len, current_len);
    LineIndices_.push_back(LineIndex{start_byte, start_char, current_len});
  }

  Stats_.TotalLines    = LineIndices_.size();
  Stats_.MaxLineLength = max_len;

  if (Stats_.TotalLines > 0)
  {
    Stats_.AverageLineLength = Stats_.TotalCharacters / Stats_.TotalLines;
  }

  LineIndexBuilt_ = true;
  reset();
}

StringType FileManager::getSourceLine(const SizeType line)
{
  std::string ret;
  SizeType    s = line;

  while (s > 0)
  {
    std::getline(Stream_, ret);
    s--;
  }

  return utf8::utf8to16(ret);
}

}
}