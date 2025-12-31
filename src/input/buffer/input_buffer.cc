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
  return buffers_[current_buffer_].length();
}

std::size_t InputBuffer::buffer_offset() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::buffer_offset() called!" << std::endl;
#endif
  return static_cast<std::size_t>(current_ - buffers_[current_buffer_].data());
}

bool InputBuffer::empty() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::empty() called!" << std::endl;
#endif
  return (!file_manager_->is_open() || current_ == nullptr || buffers_[current_buffer_].empty());
}

char16_t InputBuffer::at(const std::size_t idx) const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::at() called!" << std::endl;
#endif
  return buffers_[current_buffer_][idx];
}

char16_t InputBuffer::consume_char()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::consume_char() called!" << std::endl;
#endif
  auto& unget_stack = unget_stack_;
  auto& cur_pos = current_position_;
  auto& cur_buf = current_buffer_;
  auto& cur = current_;
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
  auto& cur = current_;
  auto& cur_buf = current_buffer_;
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
  auto& cur = current_;
  auto& cur_buf = current_buffer_;
  static const char16_t end = BUFFER_END;
  if (cur == nullptr) return end;
  pointer forward = cur + 1;
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

string_type InputBuffer::n_peek(std::size_t n)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::n_peek() called!" << std::endl;
#endif
  auto& cur_buf = current_buffer_;
  auto& bufs = buffers_;
  auto& cur = current_;
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
  while (len-- > 0)
    consume_char();
}

void InputBuffer::unget(char16_t ch)
{
#if DEBUG_PRINT
  // store previous position instead of current one
  std::cout << "-- DEBUG : InputBuffer::unget() called!" << std::endl;
#endif
  Position prev_pos = current_position_;
  rewind_position_(ch);
  unget_stack_.push({ch, prev_pos});
}

void InputBuffer::reset()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::reset() called!" << std::endl;
#endif
  auto& cur_buf = current_buffer_;
  auto& bufs = buffers_;
  auto& cur = current_;
  auto& cur_pos = current_position_;
  auto& cols = columns_;
  cur_buf = 0;
  bufs[0][0] = BUFFER_END;
  bufs[0][1] = BUFFER_END;
  cur = bufs[0].data();
  cur_pos = {1, 1, 0};
  while (!cols.empty())
    cols.pop();
  cols.push(1);
}

Position InputBuffer::position() const MYLANG_NOEXCEPT { return current_position_; }

void InputBuffer::swap_buffers_()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::swap_buffers_() called!" << std::endl;
#endif
  auto& cur_buf = current_buffer_;
  auto& bufs = buffers_;
  auto& cur = current_;
  auto& cols = columns_;
  cur_buf ^= 1;
  cur = bufs[cur_buf].data();
  if (cols.empty()) cols.push(1);
}

void InputBuffer::advance_position_(char16_t ch)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBuffer::advance_position_() called!" << std::endl;
#endif
  auto& cur_pos = current_position_;
  auto& cols = columns_;
  cur_pos.filepos += 1;
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
  auto& cur_pos = current_position_;
  auto& cols = columns_;
  if (cur_pos.filepos == 0)
    // TODO: ultimately should emit an error
    return;
  cur_pos.filepos = std::max<std::size_t>(0, cur_pos.filepos - 1);
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