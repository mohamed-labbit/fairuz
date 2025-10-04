#pragma once

#include "buffer/input_buffer.h"
#include "macros.h"
#include <fstream>
#include <stdexcept>
#include <string>
#include <wchar.h>


namespace mylang {
namespace lex {

class SourceManager
{
   public:
    using char_type   = wchar_t;
    using string_type = std::wstring;
    using size_type   = std::size_t;

    explicit SourceManager(const std::string& filename) :
        file_(filename, std::ios::binary),
        input_buffer_(file_, DEFAULT_CAPACITY)
    {
        if (!file_.is_open())
        {
            throw std::runtime_error("File not found: " + filename);
        }
    }

    ~SourceManager()
    {
        if (file_.is_open())
        {
            file_.close();
        }
    }

    size_type line() const;

    size_type column() const;

    lex::Position position() const;

    bool done() const;

    // next consumes the character, peek does not
    char_type peek();

    char_type consume_char();

    char_type current();

    lex::Position offset_map(const size_type& offset);

    lex::Position offset_map_(const size_type& offset) const;

   private:
    std::ifstream file_;
    InputBuffer   input_buffer_;
};
}  // lex
}  // mylang