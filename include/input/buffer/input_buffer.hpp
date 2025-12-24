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
  std::size_t filepos{0};

  Position() = default;

  Position(const std::size_t line, const std::size_t col, std::size_t fpos) :
      line(line),
      column(col),
      filepos(fpos)
  {
  }
};

class InputBuffer: public InputBufferBase
{
 public:
  using pointer = char16_t*;

  InputBuffer() = default;

  InputBuffer(input::FileManager* file_manager, std::size_t cap = DEFAULT_CAPACITY) :
      capacity_(cap),
      InputBufferBase(file_manager, cap)
  {
    buffers_[0].resize(capacity_ + 1, BUFFER_END);
    buffers_[1].resize(capacity_ + 1, BUFFER_END);
    reset();
  }

  std::size_t size() const;
  std::size_t buffer_offset() const;
  bool        empty() const;
  char16_t    at(const std::size_t idx) const;
  char16_t    consume_char();
  MYLANG_NODISCARD
  const char16_t& current();
  MYLANG_NODISCARD
  const char16_t& peek();
  string_type     n_peek(std::size_t n);
  void            consume(std::size_t len);
  void            unget(char16_t ch);
  void            reset();
  Position        position() const MYLANG_NOEXCEPT;

 private:
  struct PushbackEntry
  {
    char16_t ch;
    Position pos;
  };

  std::size_t               capacity_ = DEFAULT_CAPACITY;
  pointer                   current_{nullptr};
  uint8_t                   current_buffer_{0};
  std::size_t               file_pos_{0};
  Position                  current_position_;
  std::stack<std::size_t>   columns_;
  std::stack<PushbackEntry> unget_stack_;

  void swap_buffers_();
  void advance_position_(char16_t ch);
  void rewind_position_(char16_t ch);
};

}
}  // lex
}  // mylang