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
    FullPath_(filepath)
{
  std::ifstream in(filepath, std::ios::binary);
  if (!in)
    diagnostic::engine.panic(toString(FileManagerError::FILE_NOT_OPEN));

  InputBuffer_        = StringRef::fromUtf8(std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()));
  LastKnownWriteTime_ = fs::last_write_time(filepath);
}

void FileManager::reset() { Context_.reset(); }

void FileManager::seekToChar(SizeType off)
{
  if (off > InputBuffer_.len())
    throw error::FileError("seek past eof");

  Context_.CharOffset = off;
  Context_.ByteOffset = off * sizeof(CharType);
}

StringRef FileManager::readWindowInternal(SizeType size)
{
  if (Context_.CharOffset >= InputBuffer_.len())
    return u"";

  SizeType  remaining = InputBuffer_.len() - Context_.CharOffset;
  SizeType  count     = std::min(size, remaining);
  StringRef ret       = InputBuffer_.slice(Context_.CharOffset, Context_.CharOffset + count);

  Context_.CharOffset += count;
  Context_.ByteOffset += count * sizeof(CharType);
  Context_.column += count;  // optional

  return ret;
}

StringRef FileManager::readLine(const SizeType line_number)
{
  seekToLine(line_number);
  const SizeType LineLength = LineIndices_[line_number].LineLength;
  return readWindowInternal(LineLength);
}

StringRef FileManager::readNextLine()
{
  SizeType start = Context_.CharOffset;

  while (Context_.CharOffset < InputBuffer_.len())
  {
    CharType c = InputBuffer_[Context_.CharOffset++];

    if (c == u'\n' || c == u'\r')
    {
      SizeType len = Context_.CharOffset - start - 1;

      // handle CRLF
      if (c == u'\r' && Context_.CharOffset < InputBuffer_.len() && InputBuffer_[Context_.CharOffset] == u'\n')
        ++Context_.CharOffset;

      ++Context_.line;
      Context_.column = 0;

      return InputBuffer_.substr(start, len);
    }
  }

  // EOF line
  if (Context_.CharOffset > start)
    return InputBuffer_.substr(start, Context_.CharOffset - start);

  return u"";
}

std::vector<StringRef> FileManager::readLines(const SizeType start, const SizeType count)
{
  if (!LineIndexBuilt_)
    buildLineIndex();

  if (start >= LineIndices_.size())
    throw error::FileError(toString(FileManagerError::INVALID_LINE_NUMBER));

  const SizeType         end = std::min(start + count, LineIndices_.size());
  std::vector<StringRef> lines;
  lines.reserve(end - start);

  for (SizeType i = start; i < end; ++i)
    lines.push_back(readLine(i));

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
      break;
    result += chunk;
  }

  return result;
}

typename FileManager::FileStats FileManager::computeStats()
{
  FileStats stats;
  stats.TotalBytes = fs::file_size(FullPath_);
  /// TODO: : detect file encoding
  return stats;
}

void FileManager::refreshStats()
{
  FileStats new_stats = computeStats();
  Stats_              = new_stats;
  LastKnownWriteTime_ = fs::last_write_time(FullPath_);
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
    return;

  LineIndices_.clear();
  LineIndices_.reserve(InputBuffer_.len() / 80);

  size_t start = 0;
  for (size_t i = 0; i < InputBuffer_.len(); ++i)
  {
    if (InputBuffer_[i] == '\n')
    {
      LineIndices_.push_back({start, i - start});
      start = i + 1;
    }
    else if (InputBuffer_[i] == '\r')
    {
      size_t len = i - start;
      if (i + 1 < InputBuffer_.len() && InputBuffer_[i + 1] == '\n')
        ++i;  // Skip LF in CRLF
      LineIndices_.push_back({start, len});
      start = i + 1;
    }
  }

  // Last line without newline
  if (start < InputBuffer_.len())
    LineIndices_.push_back({start, InputBuffer_.len() - start});

  Stats_.TotalLines = LineIndices_.size();
  LineIndexBuilt_   = true;
}

StringRef FileManager::getSourceLine(SizeType line)
{
  if (!LineIndexBuilt_)
    buildLineIndex();

  if (line >= LineIndices_.size())
    return u"";

  auto& idx = LineIndices_[line];
  return InputBuffer_.substr(idx.CharOffset, idx.LineLength);
}

}
}