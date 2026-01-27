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
    const SizeType chars_to_read = std::max<SizeType>(0, CharOffset - Context_.CharOffset);
    StringRef      result        = readWindowInternal(std::min(chars_to_read, SizeType{1024}));
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
/*
StringRef FileManager::readWindowInternal(const SizeType size)
{
  if (size == 0)
  {
    return StringRef{};
  }

  SizeType to_read = size;

  // Validate size to prevent overflow
  constexpr SizeType MAX_REASONABLE_SIZE = std::numeric_limits<SizeType>::max() / MAX_UTF8_CHAR_BYTES;
  if (size > MAX_REASONABLE_SIZE || size == std::numeric_limits<SizeType>::max())
  {
    // throw error::FileError("Invalid read size: size too large or invalid (-1) : " + std::to_string(size));
    to_read = MAX_REASONABLE_SIZE;
  }

  if (!isOpen())
  {
    throw error::FileError(toString(FileManagerError::FILE_NOT_OPEN));
  }

  const SizeType byte_chunk_size = to_read * MAX_UTF8_CHAR_BYTES;

  // Double-check the multiplication didn't overflow
  if (byte_chunk_size < to_read)
  {
    throw error::FileError("Buffer size overflow");
  }

  std::vector<char> byte_buffer(byte_chunk_size);
  Stream_.read(byte_buffer.data(), byte_chunk_size);
  std::streamsize bytes_read = Stream_.gcount();

  if (bytes_read == 0)
  {
    if (Stream_.eof())
    {
      return StringRef{};
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

  // Convert UTF-8 to UTF-16 (StringRef)
  // Create a null-terminated string for the constructor
  std::string utf8_string(byte_buffer.begin(), byte_buffer.end());
  StringRef   result = StringRef::fromUtf8(utf8_string);

  if (result.len() > to_read)
  {
    // Calculate how many UTF-8 bytes the trimmed characters represent
    SizeType trimmed_utf8_bytes = 0;
    for (SizeType i = to_read; i < result.len(); ++i)
    {
      CharType ch = result[i];

      if (ch <= utf::CODEPOINT_MAX_1BYTE)
      {
        trimmed_utf8_bytes += 1;
      }
      else if (ch <= utf::CODEPOINT_MAX_2BYTE)
      {
        trimmed_utf8_bytes += 2;
      }
      else if (utf::isHighSurrogate(ch))  // High surrogate
      {
        trimmed_utf8_bytes += 4;
        i++;  // Skip low surrogate
      }
      else
      {
        trimmed_utf8_bytes += 3;
      }
    }

    Stream_.seekg(-static_cast<std::streamoff>(trimmed_utf8_bytes), std::ios_base::cur);
    Context_.ByteOffset -= trimmed_utf8_bytes;

    // Create a new StringRef with only the first 'size' characters
    StringRef trimmed_result(to_read + 1);  // +1 for null terminator
    std::memcpy(trimmed_result.get(), result.get(), to_read * sizeof(CharType));
    trimmed_result.len()       = to_read;
    trimmed_result.get()[to_read] = '\0';

    result = trimmed_result;
  }

  Context_.CharOffset += result.len();
  return result;
}
*/

StringRef FileManager::readWindowInternal(SizeType size)
{
  if (size == 0)
  {
    return StringRef{};
  }

  if (!isOpen())
  {
    throw error::FileError(toString(FileManagerError::FILE_NOT_OPEN));
  }

  constexpr SizeType MAX_REASONABLE_SIZE = std::numeric_limits<SizeType>::max() / MAX_UTF8_CHAR_BYTES;

  if (size > MAX_REASONABLE_SIZE)
    throw error::FileError("Invalid read size");

  const SizeType byte_chunk_size = size * MAX_UTF8_CHAR_BYTES;

  std::vector<char> byte_buffer(byte_chunk_size);

  Stream_.read(byte_buffer.data(), static_cast<std::streamsize>(byte_chunk_size));

  const std::streamsize bytes_read = Stream_.gcount();

  if (bytes_read == 0)
  {
    if (Stream_.eof())
    {
      return StringRef{};
    }
    throw error::FileError(toString(FileManagerError::READ_ERROR));
  }

  if (Stream_.bad())
  {
    throw error::FileError(toString(FileManagerError::READ_ERROR));
  }

  byte_buffer.resize(static_cast<SizeType>(bytes_read));

  // --- UTF-8 boundary fix ---
  const SizeType rewind_utf8 = validateUtf8Bound(byte_buffer);

  if (rewind_utf8 > 0)
  {
    byte_buffer.resize(byte_buffer.size() - rewind_utf8);
    Stream_.seekg(-static_cast<std::streamoff>(rewind_utf8), std::ios_base::cur);

    if (Stream_.fail())
    {
      throw error::FileError(toString(FileManagerError::SEEK_OUT_OF_BOUND));
    }
  }

  // --- Decode ---
  StringRef decoded = StringRef::fromUtf8(byte_buffer.data());

  // --- Enforce character window ---
  if (decoded.len() > size)
  {
    // Determine exact UTF-8 byte length of extra chars
    SizeType extra_bytes = 0;

    for (SizeType i = size; i < decoded.len(); ++i)
    {
      CharType ch = decoded[i];

      if (utf::isHighSurrogate(ch))
      {
        extra_bytes += 4;
        ++i;  // skip low surrogate
      }
      else if (ch <= 0x7F)
      {
        extra_bytes += 1;
      }
      else if (ch <= 0x7FF)
      {
        extra_bytes += 2;
      }
      else
      {
        extra_bytes += 3;
      }
    }

    Stream_.seekg(-static_cast<std::streamoff>(extra_bytes), std::ios_base::cur);

    if (Stream_.fail())
    {
      throw error::FileError(toString(FileManagerError::SEEK_OUT_OF_BOUND));
    }

    decoded.truncate(size);
    Context_.ByteOffset -= extra_bytes;
  }

  Context_.ByteOffset += byte_buffer.size();
  Context_.CharOffset += decoded.len();

  return decoded;
}

StringRef FileManager::readLine(const SizeType line_number)
{
  seekToLine(line_number);
  const SizeType LineLength = LineIndices_[line_number].LineLength;
  return readWindowInternal(LineLength);
}

StringRef FileManager::readNextLine()
{
  StringRef line;
  line.resize(100);  // average

  for (;;)
  {
    // check line is not empty
    StringRef chunk = readWindowInternal(1);
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
      StringRef peek = readWindowInternal(1);
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

std::vector<StringRef> FileManager::readLines(const SizeType start, const SizeType count)
{
  if (!LineIndexBuilt_)
  {
    buildLineIndex();
  }

  if (start >= LineIndices_.size())
  {
    throw error::FileError(toString(FileManagerError::INVALID_LINE_NUMBER));
  }

  const SizeType         end = std::min(start + count, LineIndices_.size());
  std::vector<StringRef> lines;
  lines.reserve(end - start);

  for (SizeType i = start; i < end; ++i)
  {
    lines.push_back(std::move(readLine(i)));
  }

  return lines;
}

StringRef FileManager::readAll()
{
  reset();
  StringRef result;
  result.resize(Stats_.TotalBytes / 2);

  for (;;)
  {
    StringRef chunk = readWindowInternal(4096);  // 4 MB
    if (chunk.empty())
    {
      break;
    }
    result += chunk;
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

StringRef FileManager::peekRange(const SizeType start_offset, const SizeType length)
{
  pushPosition();
  seekToChar(start_offset);
  StringRef result = readWindowInternal(length);
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
    StringRef chunk = readWindowInternal(LINE_INDEX_CHUNK);
    if (chunk.empty())
    {
      break;
    }

    SizeType i = 0;
    for (; i < chunk.len(); ++i)
    {
      current_len++;

      if (chunk[i] == u'\n' || chunk[i] == u'\r')
      {
        max_len = std::max(max_len, current_len - 1);
        LineIndices_.push_back(LineIndex{start_byte, start_char, current_len - 1});

        /// TODO:: Handle CRLF
        if (chunk[i] == u'\r' && char_pos + 1 < Context_.CharOffset)
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

StringRef FileManager::getSourceLine(const SizeType line)
{
  std::string ret;
  SizeType    s = line;

  while (s > 0)
  {
    std::getline(Stream_, ret);
    s--;
  }

  return ret.data();
}

}
}