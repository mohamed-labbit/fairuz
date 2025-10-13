#pragma once

#include "macros.h"
#include <fstream>
#include <string>


namespace mylang {
namespace lex {
namespace buffer {

class InputBase
{
   public:
    using buffer_t  = std::u16string;
    using size_type = std::size_t;

    InputBase(std::ifstream& f, size_type cap = DEFAULT_CAPACITY) :
        file_(f),
        byte_position_(0),
        char_count_(0)
    {
        buffers_[0].resize(cap + 1, BUF_END);
        buffers_[1].resize(cap + 1, BUF_END);

        // Open file in binary mode for proper UTF-8 reading
        if (!file_.is_open())
        {
            throw std::runtime_error("File is not open");
        }
    }

    bool empty() const noexcept { return !file_.is_open(); }

    bool refresh_buffer(const unsigned int to_refresh);

   protected:
    std::ifstream& file_;
    buffer_t       buffers_[2];
    std::size_t    byte_position_;  // Current byte position in file
    std::size_t    char_count_;     // Total characters read so far

   private:
    // Read up to max_chars wide characters from the current file position
    buffer_t read_wchar_window(std::size_t max_chars);
};

}
}  // lex
}  // mylang
