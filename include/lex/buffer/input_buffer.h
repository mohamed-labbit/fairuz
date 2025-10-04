#pragma once

#include "base.h"


namespace mylang {
namespace lex {


struct Position
{
    std::size_t line_;
    std::size_t column_;

    Position() :
        line_(0),
        column_(0)
    {
    }

    Position(const std::size_t line, const std::size_t col) :
        line_(line),
        column_(col)
    {
    }
};

    class InputBuffer: public InputBase
{
   public:
    using char_type   = char16_t;
    using pointer     = char_type*;
    using string_type = std::u16string;
    using size_type   = size_t;

    InputBuffer(std::ifstream& f, size_type cap = DEFAULT_CAPACITY) :
        capacity_(cap),
        InputBase(f, cap)
    {
        buffers_[0].resize(capacity_ + 1, BUF_END);
        buffers_[1].resize(capacity_ + 1, BUF_END);
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
    pointer   current_{nullptr};
    uint8_t   current_buffer_{0};
    size_type file_pos_{0};

    Position           current_position_;
    std::stack<size_t> columns_;

    struct PushbackEntry
    {
        char_type ch;
        Position  pos;
    };

    std::stack<PushbackEntry> unget_stack_;

    void swap_buffers_();

    void advance_position_(char_type ch);

    void rewind_position_(char_type ch);
};


}  // lex
}  // mylang