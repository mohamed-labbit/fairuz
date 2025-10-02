#pragma once

#include "buffer.h"
#include "macros.h"
#include <string>
#include <wchar.h>


class SourceManager
{
   public:
    using char_type   = wchar_t;
    using string_type = std::wstring;
    using size_type   = std::size_t;

    SourceManager(const std::string& filename) {
        if ((file_ptr_ = std::fopen(filename.c_str(), "r")) == nullptr)
        {
            throw std::invalid_argument("File not found :" + filename);
        }

        input_buffer_ = InputBuffer(file_ptr_);
    }

    ~SourceManager() {
        if (file_ptr_ != nullptr)
        {
            std::fclose(file_ptr_);
        }
    }

    // string_type getRaw() const;
    size_type remaining() const;
    size_type line() const;
    size_type column() const;
    // char_type current() const;
    bool done() const;

    // next consumes the character while peek doesn't
    char_type peek();

    // including current
    // string_type lookahead(unsigned len = 1) const;

    /*
  char_type lookahead_char() const {
    if (offset_ == source_.length() - 1)
    return EOF;
    return source_[offset_ + 1];
  }
  */

    // excluding current
    //string_type lookbehind(unsigned len = 1) const;
    //char_type   lookbehind_char() const;

    char_type consume_char();

    char_type current() { return this->input_buffer_.current(); }

   private:
    InputBuffer input_buffer_;
    FILE*       file_ptr_;
};