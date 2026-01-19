#pragma once

#include "../../../include/diag/diagnostic.hpp"
#include "../../macros.hpp"
#include "../file_manager.hpp"

#include <fstream>
#include <string>


namespace mylang {
namespace lex {
namespace buffer {

class InputBufferBase
{
 public:
  using buffer_t = StringType;

  InputBufferBase(input::FileManager* fm, SizeType cap = DEFAULT_CAPACITY) :
      FileManager_(fm),
      BytePosition_(0),
      CharCount_(0)
  {
    Buffers_[0].resize(cap + 1, BUFFER_END);
    Buffers_[1].resize(cap + 1, BUFFER_END);

    if (FileManager_ == nullptr || !FileManager_->isOpen())
      diagnostic::engine.panic("File is not open");
  }

  InputBufferBase() = default;

  bool empty() const MYLANG_NOEXCEPT { return !FileManager_->isOpen() && FileManager_->remaining() > 0; }
  bool refreshBuffer(const std::uint32_t to_refresh);

 protected:
  input::FileManager* FileManager_{nullptr};
  buffer_t            Buffers_[2];
  SizeType            BytePosition_;  // Current byte position in file
  SizeType            CharCount_;     // Total characters read so far
};
}
}  // lex
}  // mylang
