#pragma once

#include "base.hpp"


namespace mylang {
namespace lex {
namespace buffer {

struct Position
{
    std::size_t line;
    std::size_t column;
    std::size_t file_pos;

    Position() :
        line(0),
        column(0),
        file_pos(0)
    {
    }

    Position(const std::size_t line, const std::size_t col, std::size_t fpos) :
        line(line),
        column(col),
        file_pos(fpos)
    {
    }
};

class InputBuffer: public InputBufferBase
{
   public:
    using pointer = char16_t*;

    InputBuffer(std::ifstream& f, std::size_t cap = DEFAULT_CAPACITY) :
        capacity_(cap),
        InputBufferBase(f, cap)
    {
        buffers_[0].resize(capacity_ + 1, BUFFER_END);
        buffers_[1].resize(capacity_ + 1, BUFFER_END);
        reset();
    }

    std::size_t size() const;
    std::size_t buffer_offset() const;
    bool empty() const;
    char16_t at(const std::size_t idx) const;
    char16_t consume_char();
    [[nodiscard]]
    const char16_t& current();
    [[nodiscard]]
    const char16_t& peek();
    std::u16string n_peek(std::size_t n);
    void consume(std::size_t len);
    void unget(char16_t ch);
    void reset();
    Position position() const noexcept;

   private:
    struct PushbackEntry
    {
        char16_t ch;
        Position pos;
    };

    std::size_t capacity_ = DEFAULT_CAPACITY;
    pointer current_{nullptr};
    uint8_t current_buffer_{0};
    std::size_t file_pos_{0};
    Position current_position_;
    std::stack<size_t> columns_;
    std::stack<PushbackEntry> unget_stack_;

    void swap_buffers_();
    void advance_position_(char16_t ch);
    void rewind_position_(char16_t ch);
};

}
}  // lex
}  // mylang