#include "../../../include/input/buffer/base.hpp"
#include "../../../utfcpp/source/utf8.h"

#include <iostream>
#include <vector>


namespace mylang {
namespace lex {
namespace buffer {

/*
bool InputBufferBase::refreshBuffer(const std::uint32_t to_refresh)
{
  if (!FileManager_->isOpen())
  {
    return false;
  }

  SizeType  max_chars = Buffers_[to_refresh].len() - 1;
  StringRef buf       = FileManager_->readWindow(max_chars);

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
*/

bool InputBufferBase::refreshBuffer(const std::uint32_t to_refresh)
{
  if (!FileManager_->isOpen())
  {
    return false;
  }

  // Get current capacity or use a sensible default
  SizeType max_chars = Buffers_[to_refresh].cap();
  if (max_chars == 0)
  {
    max_chars = 4096;  // Default buffer size
  }
  else if (max_chars > 0)
  {
    max_chars -= 1;  // Leave room for BUFFER_END
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

}
}  // lex
}  // mylang
