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
  if (!FileManager_ || !FileManager_->isOpen())
    return false;

  // Validate buffer index
  if (to_refresh >= 2)  // Assuming double buffering (0 and 1)
    return false;

  // Determine the buffer capacity to use
  SizeType buffer_capacity = Buffers_[to_refresh].cap();

  // If buffer has no capacity yet, initialize with a reasonable default
  constexpr SizeType DEFAULT_BUFFER_SIZE = 4096;

  if (buffer_capacity == 0)
  {
    buffer_capacity = DEFAULT_BUFFER_SIZE;
    // Pre-allocate the buffer to avoid reallocations
    Buffers_[to_refresh].reserve(buffer_capacity);
  }

  // Calculate max characters we can read
  // Reserve 1 character for BUFFER_END sentinel
  SizeType max_chars = (buffer_capacity > 1) ? (buffer_capacity - 1) : (DEFAULT_BUFFER_SIZE - 1);

  // Read from file manager
  StringRef buf = FileManager_->readWindow(max_chars);

  // Handle end of file or read error
  if (buf.empty())
  {
    // Clear buffer and add BUFFER_END marker
    Buffers_[to_refresh].clear();
    Buffers_[to_refresh] += BUFFER_END;
    return false;
  }

  // Option 1: Replace buffer contents (may cause reallocation)
  // Buffers_[to_refresh] = buf;
  // Buffers_[to_refresh] += BUFFER_END;

  // Option 2: More efficient - clear and rebuild without reallocation
  Buffers_[to_refresh].clear();

  // Reserve space if needed (capacity + 1 for BUFFER_END)
  if (Buffers_[to_refresh].cap() < buf.len() + 1)
    Buffers_[to_refresh].reserve(buf.len() + 1);

  // Append content and sentinel
  Buffers_[to_refresh] += buf;
  Buffers_[to_refresh] += BUFFER_END;

  return true;
}

}  // namespace buffer
}  // namespace lex
}  // namespace mylang