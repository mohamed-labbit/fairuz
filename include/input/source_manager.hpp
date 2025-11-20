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

    explicit SourceManager(std::ifstream* file) :
        file_(file),
        input_buffer_(buffer::InputBuffer(file_, DEFAULT_CAPACITY)),
        use_file_buffer_(true)
    {
        if (!file_->is_open()) throw std::invalid_argument("File not open");
    }

    explicit SourceManager(const std::u16string source) :
        source_(source),
        current_(source_.value().data()),
        filepath_(""),
        input_buffer_(std::nullopt),
        use_file_buffer_(false)
    {
    }

    ~SourceManager() = default;

    std::size_t line() const;

    std::size_t column() const;

    std::size_t fpos() const;

    const std::string fpath() const;

    mylang::lex::buffer::Position position() const;

    bool done() const;

    char16_t peek();

    char16_t consume_char();

    char16_t current();

    std::pair<std::size_t, std::size_t> offset_map(const std::size_t& offset);

    std::pair<std::size_t, std::size_t> offset_map_(const std::size_t& offset) const;

   private:
    std::ifstream* file_{nullptr};
    std::string filepath_;
    std::optional<buffer::InputBuffer> input_buffer_;
    std::optional<std::u16string> source_;
    std::optional<char16_t*> current_{nullptr};
    std::optional<buffer::Position> current_position_;

    bool use_file_buffer_{false};
};

}  // lex
}  // mylang