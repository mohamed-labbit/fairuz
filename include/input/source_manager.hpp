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

  SizeType line() const { return this->InputBuffer_.position().line; }

  SizeType column() const { return this->InputBuffer_.position().column; }

  SizeType fpos() const { return this->InputBuffer_.position().FilePos; }

  const std::string fpath() const MYLANG_NOEXCEPT { return this->FilePath_; }

  mylang::lex::buffer::Position position() const { return this->InputBuffer_.position(); }

  bool done() const { return this->InputBuffer_.empty(); }

  CharType peek();

  CharType consumeChar() { return this->InputBuffer_.consumeChar(); }

  CharType current() { return InputBuffer_.current(); }

  std::pair<SizeType, SizeType> offsetMap(const SizeType& offset);

  std::pair<SizeType, SizeType> offsetMap_(const SizeType& offset) const;

  StringRef getSourceLine(const SizeType line) { return InputBuffer_.getSourceLine(line); }

 private:
  std::string         FilePath_;
  buffer::InputBuffer InputBuffer_;
  CharType*           Current_{nullptr};
  buffer::Position    CurrentPosition_;
};

}  // lex
}  // mylang