#include "lex/buffer/input_buffer.h"


namespace mylang {
namespace lex {
namespace buffer {


typename InputBuffer::size_type InputBuffer::size() const { return this->buffers_[this->current_buffer_].length(); }

typename InputBuffer::size_type InputBuffer::buffer_offset() const
{
    return static_cast<size_type>(this->current_ - this->buffers_[this->current_buffer_].data());
}

bool InputBuffer::empty() const
{
    return (!this->file_.is_open() || this->current_ == nullptr || this->buffers_[this->current_buffer_].empty());
}

typename InputBuffer::char_type InputBuffer::at(const size_type idx) const
{
    return this->buffers_[this->current_buffer_][idx];
}

typename InputBuffer::char_type InputBuffer::consume_char()
{
    char_type ch;
    if (!this->unget_stack_.empty())
    {
        auto entry = unget_stack_.top();
        unget_stack_.pop();
        this->current_position_ = entry.pos;
        ch                      = entry.ch;
    }
    else
    {
        if (*this->current_ == BUF_END)
        {
            if (!this->refresh_buffer(this->current_buffer_ ^ 1))
            {
                return BUF_END;
            }

            this->swap_buffers_();
        }

        ch = *this->current_;
        this->current_ += 1;
    }

    this->advance_position_(ch);
    return ch;
}

[[nodiscard]]
const typename InputBuffer::char_type& InputBuffer::current()
{
    if (current_ == nullptr)
    {
        char_type end = BUF_END;
        return std::cref<char_type>(end);
    }

    if (*current_ == BUF_END)
    {
        if (!this->refresh_buffer(this->current_buffer_ ^ 1))
        {
            char_type end = BUF_END;
            return std::cref<char_type>(end);
        }

        this->swap_buffers_();
    }

    return *current_;
}

[[nodiscard]]
const typename InputBuffer::char_type& InputBuffer::peek()
{
    if (this->current_ == nullptr)
    {
        char_type end = BUF_END;
        return std::cref<char_type>(end);
    }

    pointer forward = this->current_ + 1;
    if (*this->current_ == BUF_END)
    {
        if (!this->refresh_buffer(this->current_buffer_ ^ 1))
        {
            char_type end = BUF_END;
            return std::cref<char_type>(end);
        }

        this->swap_buffers_();
        forward = this->current_ + 1;
    }

    if (*forward == BUF_END)
    {
        if (!this->refresh_buffer(this->current_buffer_ ^ 1))
        {
            char_type end = BUF_END;
            return std::cref<char_type>(end);
        }

        this->swap_buffers_();
        forward = this->current_ + 1;
    }

    return *forward;
}

typename InputBuffer::string_type InputBuffer::n_peek(size_type n)
{
    string_type out;
    if (n == 0)
    {
        return out;
    }

    size_type rem     = n;
    int       buf_idx = this->current_buffer_;
    size_type offset  = static_cast<size_type>(this->current_ - this->buffers_[this->current_buffer_].data() + 1);

    while (rem > 0)
    {
        if (offset >= this->buffers_[buf_idx].size() || this->buffers_[buf_idx][offset] == BUF_END)
        {
            if (!this->refresh_buffer(buf_idx ^ 1))
            {
                break;
            }

            buf_idx ^= 1;
            offset = 0;
        }

        out.push_back(this->buffers_[buf_idx][offset]);
        offset++;
        rem--;
    }

    return out;
}


void InputBuffer::consume(size_type len)
{
    while (len-- > 0)
    {
        this->consume_char();
    }
}

void InputBuffer::unget(char_type ch)
{
    // store previous position instead of current one
    Position prev_pos = this->current_position_;
    this->rewind_position_(ch);
    this->unget_stack_.push({ch, prev_pos});
}

void InputBuffer::reset()
{
    this->current_buffer_   = 0;
    this->buffers_[0][0]    = BUF_END;
    this->buffers_[0][1]    = BUF_END;
    this->current_          = this->buffers_[0].data();
    this->current_position_ = {1, 1, 0};

    while (!this->columns_.empty())
    {
        this->columns_.pop();
    }

    this->columns_.push(1);
}

Position InputBuffer::position() const noexcept { return this->current_position_; }

void InputBuffer::swap_buffers_()
{
    this->current_buffer_ ^= 1;
    this->current_ = this->buffers_[this->current_buffer_].data();

    if (this->columns_.empty())
    {
        this->columns_.push(1);
    }
}

void InputBuffer::advance_position_(char_type ch)
{
    this->current_position_.file_pos_ += 1;

    if (ch == u'\n')
    {
        this->current_position_.line_ += 1;
        this->current_position_.column_ = 1;
        this->columns_.push(1);
    }
    else
    {
        this->current_position_.column_ += 1;
        if (!this->columns_.empty())
        {
            this->columns_.top() = this->current_position_.column();
        }
        else
        {
            this->columns_.push(this->current_position_.column());
        }
    }
}

void InputBuffer::rewind_position_(char_type ch)
{
    if (this->current_position_.fpos() == 0)
    {
        // ultimately should emit an error
        return;
    }

    this->current_position_.file_pos_ = std::max<size_type>(0, this->current_position_.fpos() - 1);

    if (ch == u'\n')
    {
        if (!this->columns_.empty())
        {
            this->columns_.pop();
        }

        this->current_position_.line_   = std::max<size_type>(1, this->current_position_.line() - 1);
        this->current_position_.column_ = this->columns_.empty() ? 1 : this->columns_.top();
    }
    else
    {
        this->current_position_.column_ =
          (this->current_position_.column() > 0 ? this->current_position_.column() - 1 : 0);
        if (!this->columns_.empty())
        {
            this->columns_.top() = this->current_position_.column();
        }
    }
}

}
}  // lex
}  // mylang