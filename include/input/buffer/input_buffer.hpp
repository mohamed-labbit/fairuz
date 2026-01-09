#pragma once

#include "../file_manager.hpp"
#include "base.hpp"
#include <stack>


namespace mylang {
namespace lex {
namespace buffer {

struct Position
{
  std::size_t line{0};
  std::size_t column{0};
  std::size_t FilePos{0};

  Position() = default;

  Position(const std::size_t line, const std::size_t col, std::size_t fpos) :
      line(line),
      column(col),
      FilePos(fpos)
  {
  }
};

class InputBuffer: public InputBufferBase
{
 public:
  using Pointer = char16_t*;

  InputBuffer() = default;

  InputBuffer(input::FileManager* file_manager, std::size_t cap = DEFAULT_CAPACITY) :
      Capacity_(cap),
      InputBufferBase(file_manager, cap)
  {
    Buffers_[0].resize(Capacity_ + 1, BUFFER_END);
    Buffers_[1].resize(Capacity_ + 1, BUFFER_END);
    reset();
  }

  std::size_t size() const;
  std::size_t bufferOffset() const;
  bool empty() const;
  char16_t at(const std::size_t idx) const;
  char16_t consumeChar();
  MYLANG_NODISCARD
  const char16_t& current();
  MYLANG_NODISCARD
  const char16_t& peek();
  StringType nPeek(std::size_t n);
  void consume(std::size_t len);
  void unget(char16_t ch);
  void reset();
  Position position() const MYLANG_NOEXCEPT;

 private:
  struct PushbackEntry
  {
    char16_t ch;
    Position pos;
  };

  std::size_t Capacity_ = DEFAULT_CAPACITY;
  Pointer Current_{nullptr};
  uint8_t CurrentBuffer_{0};
  std::size_t FilePos_{0};
  Position CurrentPosition_;
  std::stack<std::size_t> Columns_;
  std::stack<PushbackEntry> UngetStack_;

  void swapBuffers_();
  void advancePosition_(char16_t ch);
  void rewindPosition_(char16_t ch);
};

}
}  // lex
}  // mylang