#pragma once

#include "buffer.h"
#include "macros.h"
#include <fstream>
#include <stdexcept>
#include <string>
#include <wchar.h>


class SourceManager
{
   public:
    using char_type   = wchar_t;
    using string_type = std::wstring;
    using size_type   = std::size_t;

    explicit SourceManager(const std::string& filename) :
        file_(filename, std::ios::binary),
        input_buffer_(file_, DEFAULT_CAPACITY) {
        if (!file_.is_open())
        {
            throw std::runtime_error("File not found: " + filename);
        }
    }

    ~SourceManager() {
        if (file_.is_open())
        {
            file_.close();
        }
    }

    size_type line() const { return input_buffer_.position().line_; }
    size_type column() const { return input_buffer_.position().column_; }

    Position position() const { return input_buffer_.position(); }

    bool done() const { return input_buffer_.empty(); }

    // next consumes the character, peek does not
    char_type peek() { return input_buffer_.peek(); }
    char_type consume_char() { return input_buffer_.consume_char(); }
    char_type current() { return input_buffer_.current(); }

   private:
    std::ifstream file_;
    InputBuffer   input_buffer_;
};