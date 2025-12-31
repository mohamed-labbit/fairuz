#pragma once

#include "../macros.hpp"
#include "buffer/input_buffer.hpp"

#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <wchar.h>


namespace mylang {
namespace lex {

class SourceManager
{
 public:
  explicit SourceManager() = default;
  explicit SourceManager(input::FileManager* file_manager);

  ~SourceManager() = default;

  std::size_t line() const;
  std::size_t column() const;
  std::size_t fpos() const;
  const std::string fpath() const noexcept;
  mylang::lex::buffer::Position position() const;
  bool done() const;
  char16_t peek();
  char16_t consume_char();
  char16_t current();
  std::pair<std::size_t, std::size_t> offset_map(const std::size_t& offset);
  std::pair<std::size_t, std::size_t> offset_map_(const std::size_t& offset) const;

 private:
  std::string filepath_;
  buffer::InputBuffer input_buffer_;
  char16_t* current_{nullptr};
  buffer::Position current_position_;
};

}  // lex
}  // mylang