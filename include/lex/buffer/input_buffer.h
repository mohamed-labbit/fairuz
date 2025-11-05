#pragma once

#include "base.h"


namespace mylang {
namespace lex {
namespace buffer {

struct Position
{
    std::size_t line_;
    std::size_t column_;
    std::size_t file_pos_;

    Position() :
        line_(0),
        column_(0),
        file_pos_(0)
    {
    }

    Position(const std::size_t line, const std::size_t col, std::size_t fpos) :
        line_(line),
        column_(col),
        file_pos_(fpos)
    {
    }

    std::size_t line() const { return this->line_; }
    std::size_t column() const { return this->column_; }
    std::size_t fpos() const { return this->file_pos_; }
};

class InputBuffer: public InputBufferBase
{
   public:
    using char_type = char16_t;
    using pointer = char_type*;
    using string_type = std::u16string;
    using size_type = size_t;

    InputBuffer(std::ifstream& f, size_type cap = DEFAULT_CAPACITY) :
        capacity_(cap),
        InputBufferBase(f, cap)
    {
        buffers_[0].resize(capacity_ + 1, BUFFER_END);
        buffers_[1].resize(capacity_ + 1, BUFFER_END);
        reset();
    }

    size_type size() const;

    size_type buffer_offset() const;

    bool empty() const;

    char_type at(const size_type idx) const;

    char_type consume_char();

    [[nodiscard]]
    const char_type& current();

    [[nodiscard]]
    const char_type& peek();

    string_type n_peek(size_type n);

    void consume(size_type len);

    void unget(char_type ch);

    void reset();

    Position position() const noexcept;

   private:
    size_type capacity_ = DEFAULT_CAPACITY;
    pointer current_{nullptr};
    uint8_t current_buffer_{0};
    size_type file_pos_{0};

    Position current_position_;
    std::stack<size_t> columns_;

    struct PushbackEntry
    {
        char_type ch_;
        Position pos_;
    };

    std::stack<PushbackEntry> unget_stack_;

    void swap_buffers_();

    void advance_position_(char_type ch);

    void rewind_position_(char_type ch);
};

}
}  // lex
}  // mylang