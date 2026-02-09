#pragma once

#include "../file_manager.hpp"
#include "base.hpp"
#include <stack>


namespace mylang {
namespace lex {
namespace buffer {

struct Position
{
  SizeType line{0};
  SizeType column{0};
  SizeType FilePos{0};

  Position() = default;

  Position(const SizeType line, const SizeType col, SizeType fpos) :
      line(line),
      column(col),
      FilePos(fpos)
  {
  }
};

class InputBuffer: public InputBufferBase
{
 public:
  using Pointer = CharType*;

  InputBuffer() = default;

  InputBuffer(input::FileManager* file_manager, SizeType cap = DEFAULT_CAPACITY) :
      Capacity_(cap),
      InputBufferBase(file_manager, cap)
  {
    reset();
  }

  SizeType size() const { return Buffers_[CurrentBuffer_].len(); }

  SizeType bufferOffset() const { return static_cast<SizeType>(Current_ - Buffers_[CurrentBuffer_].data()); }

  bool empty() const { return (!Current_ || Buffers_[CurrentBuffer_].empty()); }

  CharType at(const SizeType idx) const { return Buffers_[CurrentBuffer_][idx]; }

  CharType consumeChar();

  MYLANG_NODISCARD
  const CharType& current();

  MYLANG_NODISCARD
  const CharType& peek();

  StringRef nPeek(SizeType n);

  void consume(SizeType len)
  {
    while (len-- > 0)
      consumeChar();
  }

  void unget(CharType ch);

  void reset();

  Position position() const MYLANG_NOEXCEPT { return CurrentPosition_; }

  StringRef getSourceLine(const SizeType line) { return FileManager_->getSourceLine(line); }

 private:
  struct PushbackEntry
  {
    CharType ch;
    Position pos;
  };

  SizeType                  Capacity_ = DEFAULT_CAPACITY;
  Pointer                   Current_{nullptr};
  uint8_t                   CurrentBuffer_{0};
  SizeType                  FilePos_{0};
  Position                  CurrentPosition_;
  std::stack<SizeType>      Columns_;
  std::stack<PushbackEntry> UngetStack_;

  void swapBuffers_();

  void advancePosition_(CharType ch);

  void rewindPosition_(CharType ch);
};

}
}  // lex
}  // mylang