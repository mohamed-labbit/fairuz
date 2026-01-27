#include "../../../include/input/buffer/base.hpp"

#include <iostream>
#include <vector>


namespace mylang {
namespace lex {
namespace buffer {

bool InputBufferBase::refreshBuffer(const std::uint32_t to_refresh)
{
  if (!FileManager_->isOpen())
  {
    return false;
  }

  // FIXED: Prevent unsigned integer underflow when buffer is empty
  SizeType max_chars = 0;
  SizeType current_len = Buffers_[to_refresh].len();
  
  if (current_len > 0)
  {
    max_chars = current_len - 1;
  }
  else
  {
    // Buffer is empty or uninitialized, use capacity if available
    SizeType cap = Buffers_[to_refresh].cap();
    if (cap > 1)
    {
      max_chars = cap - 1;  // Reserve space for BUFFER_END
    }
    else
    {
      max_chars = 4096;  // Default buffer size
    }
  }

  StringRef buf = FileManager_->readWindow(max_chars);

  if (buf.empty())
  {
    Buffers_[to_refresh].clear();
    Buffers_[to_refresh] += BUFFER_END;
    return false;
  }

  Buffers_[to_refresh] = buf;
  Buffers_[to_refresh] += BUFFER_END;
  return true;
}

}  // namespace buffer
}  // namespace lex
}  // namespace mylang
