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
  explicit SourceManager(input::FileManager* fm) :
      InputBuffer_(fm, DEFAULT_CAPACITY)
  {
    // ...
  }

  ~SourceManager() = default;

  std::size_t line() const { return this->InputBuffer_.position().line; }

  std::size_t column() const { return this->InputBuffer_.position().column; }

  std::size_t fpos() const { return this->InputBuffer_.position().FilePos; }

  const std::string fpath() const MYLANG_NOEXCEPT { return this->FilePath_; }

  mylang::lex::buffer::Position position() const { return this->InputBuffer_.position(); }

  bool done() const { return this->InputBuffer_.empty(); }

  char16_t peek();

  char16_t consumeChar() { return this->InputBuffer_.consumeChar(); }

  char16_t current() { return InputBuffer_.current(); }

  std::pair<std::size_t, std::size_t> offsetMap(const std::size_t& offset);

  std::pair<std::size_t, std::size_t> offsetMap_(const std::size_t& offset) const;

 private:
  std::string         FilePath_;
  buffer::InputBuffer InputBuffer_;
  char16_t*           Current_{nullptr};
  buffer::Position    CurrentPosition_;
};

}  // lex
}  // mylang