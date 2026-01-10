#include "../../../include/input/buffer/input_buffer.hpp"

#include <functional>
#include <iostream>


namespace mylang {
namespace lex {
namespace buffer {

char16_t InputBuffer::consumeChar()
{

  char16_t ch;
  if (!UngetStack_.empty())
  {
    CurrentPosition_ = UngetStack_.top().pos;
    ch = UngetStack_.top().ch;
    UngetStack_.pop();
  }
  else
  {
    if (*Current_ == BUFFER_END)
    {
      if (!refreshBuffer(CurrentBuffer_ ^ 1)) return BUFFER_END;
      swapBuffers_();
    }
    ch = *Current_;
    Current_ += 1;
  }
  advancePosition_(ch);
  return ch;
}

MYLANG_NODISCARD
const char16_t& InputBuffer::current()
{

  static const char16_t end = BUFFER_END;
  if (Current_ == nullptr) return end;
  if (*Current_ == BUFFER_END)
  {
    if (!refreshBuffer(CurrentBuffer_ ^ 1)) return end;
    swapBuffers_();
  }
  return *Current_;
}

MYLANG_NODISCARD
const char16_t& InputBuffer::peek()
{

  static const char16_t end = BUFFER_END;
  if (Current_ == nullptr) return end;
  Pointer forward = Current_ + 1;
  if (*Current_ == BUFFER_END)
  {
    if (!refreshBuffer(CurrentBuffer_ ^ 1)) return end;
    swapBuffers_();
    forward = Current_ + 1;
  }

  if (*forward == BUFFER_END)
  {
    if (!refreshBuffer(CurrentBuffer_ ^ 1)) return end;
    swapBuffers_();
    forward = Current_ + 1;
  }
  return *forward;
}

StringType InputBuffer::nPeek(std::size_t n)
{

  StringType out;
  if (n == 0) return out;
  std::size_t rem = n;
  std::int32_t buf_idx = CurrentBuffer_;
  std::size_t offset = static_cast<std::size_t>(Current_ - Buffers_[buf_idx].data() + 1);
  while (rem > 0)
  {
    if (offset >= Buffers_[buf_idx].size() || Buffers_[buf_idx][offset] == BUFFER_END)
    {
      if (!refreshBuffer(buf_idx ^ 1)) break;
      buf_idx ^= 1;
      offset = 0;
    }
    out.push_back(Buffers_[buf_idx][offset]);
    offset++;
    rem--;
  }
  return out;
}

void InputBuffer::unget(char16_t ch)
{

  Position prev_pos = CurrentPosition_;
  rewindPosition_(ch);
  UngetStack_.push({ch, prev_pos});
}

void InputBuffer::reset()
{

  CurrentBuffer_ = 0;
  Buffers_[0][0] = BUFFER_END;
  Buffers_[0][1] = BUFFER_END;
  Current_ = Buffers_[0].data();
  CurrentPosition_ = {1, 1, 0};
  while (!Columns_.empty()) Columns_.pop();
  Columns_.push(1);
}

void InputBuffer::swapBuffers_()
{
  CurrentBuffer_ ^= 1;
  Current_ = Buffers_[CurrentBuffer_].data();
  if (Columns_.empty()) Columns_.push(1);
}

void InputBuffer::advancePosition_(char16_t ch)
{
  CurrentPosition_.FilePos += 1;
  if (ch == u'\n')
  {
    CurrentPosition_.line += 1;
    CurrentPosition_.column = 1;
    Columns_.push(1);
  }
  else
  {
    CurrentPosition_.column += 1;
    if (!Columns_.empty())
      Columns_.top() = CurrentPosition_.column;
    else
      Columns_.push(CurrentPosition_.column);
  }
}

void InputBuffer::rewindPosition_(char16_t ch)
{
  /// TODO:: ultimately should emit an error
  if (CurrentPosition_.FilePos == 0) return;
  CurrentPosition_.FilePos = std::max<std::size_t>(0, CurrentPosition_.FilePos - 1);
  if (ch == u'\n')
  {
    if (!Columns_.empty()) Columns_.pop();
    CurrentPosition_.line = std::max<std::size_t>(1, CurrentPosition_.line - 1);
    CurrentPosition_.column = Columns_.empty() ? 1 : Columns_.top();
  }
  else
  {
    CurrentPosition_.column = (CurrentPosition_.column > 0 ? CurrentPosition_.column - 1 : 0);
    if (!Columns_.empty()) Columns_.top() = CurrentPosition_.column;
  }
}

}
}  // lex
}  // mylang