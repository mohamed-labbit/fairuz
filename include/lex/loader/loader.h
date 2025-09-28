#pragma once

#include <fstream>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string>

#include "buffer.h"
#include "macros.h"


class Loader
{
 public:
  using char_type   = wchar_t;
  using size_type   = std::size_t;
  using string_type = std::wstring;
  using pointer     = char_type*;

  Loader(const std::string& filename, size_type buf_size = 4096) {
    fileptr_ = std::fopen(filename.c_str(), "rb");
    if (!fileptr_)
      throw std::runtime_error("File not found: " + filename);

    buffer_ = std::move(InputBuffer(fileptr_, buf_size));
    std::setlocale(LC_ALL, "en_US.UTF-8");  // maybe move this out of class
  }

  // non copyable
  Loader(const Loader&)            = delete;
  Loader& operator=(const Loader&) = delete;

  ~Loader() {
    if (fileptr_)
      std::fclose(fileptr_);
  }

  InputBuffer&       get() { return buffer_; }
  const InputBuffer& cget() const { return buffer_; }

  size_type buffer_size() const { return this->buffer_.size(); }

 private:
  FILE*       fileptr_ = nullptr;
  InputBuffer buffer_;
};