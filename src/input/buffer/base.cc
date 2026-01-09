#include "../../../include/input/buffer/base.hpp"
#include "../../../utfcpp/source/utf8.h"

#include <iostream>
#include <vector>


namespace mylang {
namespace lex {
namespace buffer {

bool InputBufferBase::refreshBuffer(const std::uint32_t to_refresh)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : InputBufferBase::refresh_buffer() called!" << std::endl;
#endif
  if (!FileManager_->isOpen()) return false;

  std::size_t max_chars = Buffers_[to_refresh].size() - 1;
  buffer_t buf = FileManager_->readWindow(max_chars);

  if (buf.empty())
  {
    Buffers_[to_refresh].clear();
    Buffers_[to_refresh].push_back(BUFFER_END);
    return false;
  }

  Buffers_[to_refresh].assign(buf.begin(), buf.end());
  Buffers_[to_refresh].push_back(BUFFER_END);
  return true;
}

}
}  // lex
}  // mylang
