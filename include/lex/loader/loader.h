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

  Loader(const std::string& filename, size_type buf_size = 4096) :
      buffer_(buf_size,
              [this](pointer data, size_type capacity, size_type& filled) {
                return this->shift(data, capacity, filled);
              }),
      start_(0) {
    fileptr_ = std::fopen(filename.c_str(), "rb");
    if (!fileptr_)
      throw std::runtime_error("File not found: " + filename);

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
  bool shift(pointer data, size_type capacity, size_type& filled) {
    if (!fileptr_)
      return false;

    // seek to current position
    if (std::fseek(fileptr_, start_, SEEK_SET) != 0)
      return false;

    // read up to capacity bytes
    size_type read_size = std::fread(data, sizeof(char_type), capacity, fileptr_);
    if (read_size == 0)
      return false;  // EOF or error

    filled = read_size;
    start_ += read_size;
    return true;
  }

  FILE*       fileptr_ = nullptr;
  size_type   start_;
  InputBuffer buffer_;
};