#pragma once

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
  using buffer_t = string_type;

  InputBufferBase(input::FileManager* file_manager, std::size_t cap = DEFAULT_CAPACITY) :
      file_manager_(file_manager),
      byte_position_(0),
      char_count_(0)
  {
    buffers_[0].resize(cap + 1, BUFFER_END);
    buffers_[1].resize(cap + 1, BUFFER_END);

    if (file_manager_ == nullptr || !file_manager_->is_open()) throw std::runtime_error("File is not open");
  }

  InputBufferBase() = default;

  bool empty() const MYLANG_NOEXCEPT { return !file_manager_->is_open() && file_manager_->remaining() > 0; }
  bool refresh_buffer(const std::uint32_t to_refresh);

 protected:
  input::FileManager* file_manager_{nullptr};
  buffer_t            buffers_[2];
  std::size_t         byte_position_;  // Current byte position in file
  std::size_t         char_count_;     // Total characters read so far
};
}
}  // lex
}  // mylang
