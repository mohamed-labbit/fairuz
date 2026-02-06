#pragma once

#include "../../../include/diag/diagnostic.hpp"
#include "../../macros.hpp"
#include "../../types/string.hpp"
#include "../file_manager.hpp"

#include <fstream>
#include <string>


namespace mylang {
namespace lex {
namespace buffer {

class InputBufferBase
{
 public:
  using buffer_t = StringRef;

  InputBufferBase(input::FileManager* fm, SizeType cap = DEFAULT_CAPACITY) :
      FileManager_(fm),
      BytePosition_(0),
      CharCount_(0)
  {
    Buffers_[0] = StringRef(cap + 1);
    Buffers_[1] = StringRef(cap + 1);

    if (!FileManager_ || !FileManager_->isOpen())
      diagnostic::engine.panic("File is not open");
  }

  InputBufferBase() = default;

  bool empty() const MYLANG_NOEXCEPT { return !FileManager_->isOpen() && FileManager_->remaining() > 0; }

  bool refreshBuffer(const std::uint32_t to_refresh);

 protected:
  input::FileManager* FileManager_{nullptr};
  StringRef           Buffers_[2];
  SizeType            BytePosition_;  // Current byte position in file
  SizeType            CharCount_;     // Total characters read so far
};
}
}  // lex
}  // mylang
