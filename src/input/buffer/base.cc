#include "../../../include/input/buffer/base.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

namespace mylang {
namespace lex {
namespace buffer {

bool InputBufferBase::refreshBuffer(const std::uint32_t to_refresh)
{
  // Validate file manager state
  if (!FileManager_)
    return false;

  // Validate buffer index
  if (to_refresh >= 2)
    return false;

  // Determine the buffer capacity to use
  SizeType buf_capacity = Buffers_[to_refresh].cap();

  // If buffer has no capacity yet, initialize with a reasonable default
  constexpr SizeType DEFAULT_BUFFER_SIZE = 4096;

  if (buf_capacity == 0)
  {
    buf_capacity = DEFAULT_BUFFER_SIZE;
    Buffers_[to_refresh].reserve(buf_capacity);
  }

  // Read from file manager
  StringRef buf = FileManager_->readWindow(buf_capacity);
  if (buf.empty())
  {
    // Clear buffer marker
    Buffers_[to_refresh].clear();
    return false;
  }

  Buffers_[to_refresh].clear();

  if (Buffers_[to_refresh].cap() < buf.len())
    Buffers_[to_refresh].reserve(buf.len());

  Buffers_[to_refresh] += buf;

  return true;
}

}  // namespace buffer
}  // namespace lex
}  // namespace mylang