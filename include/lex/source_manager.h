#pragma once

#include "../macros.h"
#include "buffer/input_buffer.h"
#include <fstream>
#include <stdexcept>
#include <string>
#include <wchar.h>


namespace mylang {
namespace lex {

class SourceManager
{
   public:
    explicit SourceManager(const std::string& filename) :
        file_(filename, std::ios::binary),
        input_buffer_(file_, DEFAULT_CAPACITY)
    {
        if (!file_.is_open())
            throw std::runtime_error("File not found: " + filename);
    }

    ~SourceManager()
    {
        if (file_.is_open())
            file_.close();
    }

    std::size_t line() const;
    std::size_t column() const;
    std::size_t fpos() const;
    const std::string& fpath() const;
    mylang::lex::buffer::Position position() const;
    bool done() const;
    // next consumes the character, peek does not
    char16_t peek();
    char16_t consume_char();
    char16_t current();
    std::pair<std::size_t, std::size_t> offset_map(const std::size_t& offset);
    std::pair<std::size_t, std::size_t> offset_map_(const std::size_t& offset) const;

   private:
    std::ifstream file_;
    std::string filepath_;
    buffer::InputBuffer input_buffer_;
};
}  // lex
}  // mylang