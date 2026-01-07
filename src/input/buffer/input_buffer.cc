#include "../../../include/input/buffer/input_buffer.hpp"

#include <functional>
#include <iostream>


namespace mylang {
namespace lex {
namespace buffer {


std::size_t InputBuffer::size() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::size() called!" << std::endl;
#endif
  return Buffers_[CurrentBuffer_].length();
}

std::size_t InputBuffer::bufferOffset() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::bufferOffset() called!" << std::endl;
#endif
  return static_cast<std::size_t>(Current_ - Buffers_[CurrentBuffer_].data());
}

bool InputBuffer::empty() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::empty() called!" << std::endl;
#endif
  return (!FileManager_->isOpen() || Current_ == nullptr || Buffers_[CurrentBuffer_].empty());
}

char16_t InputBuffer::at(const std::size_t idx) const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::at() called!" << std::endl;
#endif
  return Buffers_[CurrentBuffer_][idx];
}

char16_t InputBuffer::consumeChar()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::consumeChar() called!" << std::endl;
#endif
  auto& unget_stack = UngetStack_;
  auto& cur_pos = CurrentPosition_;
  auto& cur_buf = CurrentBuffer_;
  auto& cur = Current_;
  char16_t ch;
  if (!unget_stack.empty())
  {
    cur_pos = unget_stack.top().pos;
    ch = unget_stack.top().ch;
    unget_stack.pop();
  }
  else
  {
    if (*cur == BUFFER_END)
    {
      if (!refresh_buffer(cur_buf ^ 1)) return BUFFER_END;
      swap_buffers_();
    }
    ch = *cur;
    cur += 1;
  }
  advance_position_(ch);
  return ch;
}

MYLANG_NODISCARD
const char16_t& InputBuffer::current()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::current() called!" << std::endl;
#endif
  auto& cur = Current_;
  auto& cur_buf = CurrentBuffer_;
  static const char16_t end = BUFFER_END;
  if (cur == nullptr) return end;
  if (*cur == BUFFER_END)
  {
    if (!refresh_buffer(cur_buf ^ 1)) return end;
    swap_buffers_();
  }
  return *cur;
}

MYLANG_NODISCARD
const char16_t& InputBuffer::peek()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::peek() called!" << std::endl;
#endif
  auto& cur = Current_;
  auto& cur_buf = CurrentBuffer_;
  static const char16_t end = BUFFER_END;
  if (cur == nullptr) return end;
  Pointer forward = cur + 1;
  if (*cur == BUFFER_END)
  {
    if (!refresh_buffer(cur_buf ^ 1)) return end;
    swap_buffers_();
    forward = cur + 1;
  }

  if (*forward == BUFFER_END)
  {
    if (!refresh_buffer(cur_buf ^ 1)) return end;
    swap_buffers_();
    forward = cur + 1;
  }
  return *forward;
}

string_type InputBuffer::nPeek(std::size_t n)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::n_peek() called!" << std::endl;
#endif
  auto& cur_buf = CurrentBuffer_;
  auto& bufs = Buffers_;
  auto& cur = Current_;
  string_type out;
  if (n == 0) return out;
  std::size_t rem = n;
  std::int32_t buf_idx = cur_buf;
  std::size_t offset = static_cast<std::size_t>(cur - bufs[buf_idx].data() + 1);
  while (rem > 0)
  {
    if (offset >= bufs[buf_idx].size() || bufs[buf_idx][offset] == BUFFER_END)
    {
      if (!refresh_buffer(buf_idx ^ 1)) break;
      buf_idx ^= 1;
      offset = 0;
    }
    out.push_back(bufs[buf_idx][offset]);
    offset++;
    rem--;
  }
  return out;
}


void InputBuffer::consume(std::size_t len)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::consume() called!" << std::endl;
#endif
  while (len-- > 0) consumeChar();
}

void InputBuffer::unget(char16_t ch)
{
#if DEBUG_PRINT
  // store previous position instead of current one
  std::cout << "-- DEBUG : InputBuffer::unget() called!" << std::endl;
#endif
  Position prev_pos = CurrentPosition_;
  rewind_position_(ch);
  UngetStack_.push({ch, prev_pos});
}

void InputBuffer::reset()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::reset() called!" << std::endl;
#endif
  auto& cur_buf = CurrentBuffer_;
  auto& bufs = Buffers_;
  auto& cur = Current_;
  auto& cur_pos = CurrentPosition_;
  auto& cols = Columns_;
  cur_buf = 0;
  bufs[0][0] = BUFFER_END;
  bufs[0][1] = BUFFER_END;
  cur = bufs[0].data();
  cur_pos = {1, 1, 0};
  while (!cols.empty()) cols.pop();
  cols.push(1);
}

Position InputBuffer::position() const MYLANG_NOEXCEPT { return CurrentPosition_; }

void InputBuffer::swap_buffers_()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::swap_buffers_() called!" << std::endl;
#endif
  auto& cur_buf = CurrentBuffer_;
  auto& bufs = Buffers_;
  auto& cur = Current_;
  auto& cols = Columns_;
  cur_buf ^= 1;
  cur = bufs[cur_buf].data();
  if (cols.empty()) cols.push(1);
}

void InputBuffer::advance_position_(char16_t ch)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::advance_position_() called!" << std::endl;
#endif
  auto& cur_pos = CurrentPosition_;
  auto& cols = Columns_;
  cur_pos.FilePos += 1;
  if (ch == u'\n')
  {
    cur_pos.line += 1;
    cur_pos.column = 1;
    cols.push(1);
  }
  else
  {
    cur_pos.column += 1;
    if (!cols.empty())
      cols.top() = cur_pos.column;
    else
      cols.push(cur_pos.column);
  }
}

void InputBuffer::rewind_position_(char16_t ch)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::rewind_position_() called!" << std::endl;
#endif
  auto& cur_pos = CurrentPosition_;
  auto& cols = Columns_;
  if (cur_pos.FilePos == 0)
    /// @todo: ultimately should emit an error
    return;
  cur_pos.FilePos = std::max<std::size_t>(0, cur_pos.FilePos - 1);
  if (ch == u'\n')
  {
    if (!cols.empty()) cols.pop();
    cur_pos.line = std::max<std::size_t>(1, cur_pos.line - 1);
    cur_pos.column = cols.empty() ? 1 : cols.top();
  }
  else
  {
    cur_pos.column = (cur_pos.column > 0 ? cur_pos.column - 1 : 0);
    if (!cols.empty()) cols.top() = cur_pos.column;
  }
}

}
}  // lex
}  // mylang