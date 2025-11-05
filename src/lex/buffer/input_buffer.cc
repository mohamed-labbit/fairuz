#include "../../../include/lex/buffer/input_buffer.h"


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

typename InputBuffer::char_type InputBuffer::at(const size_type idx) const { return this->buffers_[this->current_buffer_][idx]; }

typename InputBuffer::char_type InputBuffer::consume_char()
{
    auto& unget_stack = this->unget_stack_;
    auto& cur_pos = this->current_position_;
    auto& cur_buf = this->current_buffer_;
    auto& cur = this->current_;

    char_type ch;
    if (!unget_stack.empty())
    {
        cur_pos = unget_stack.top().pos_;
        ch = unget_stack.top().ch_;
        unget_stack.pop();
    }
    else
    {
        if (*cur == BUFFER_END)
        {
            if (!refresh_buffer(cur_buf ^ 1))
            {
                return BUFFER_END;
            }

            swap_buffers_();
        }

        ch = *cur;
        cur += 1;
    }

    advance_position_(ch);
    return ch;
}

[[nodiscard]]
const typename InputBuffer::char_type& InputBuffer::current()
{
    auto& cur = this->current_;
    auto& cur_buf = this->current_buffer_;

    if (cur == nullptr)
    {
        char_type end = BUFFER_END;
        return std::cref<char_type>(end);
    }

    if (*cur == BUFFER_END)
    {
        if (!this->refresh_buffer(cur_buf ^ 1))
        {
            char_type end = BUFFER_END;
            return std::cref<char_type>(end);
        }

        this->swap_buffers_();
    }

    return *cur;
}

[[nodiscard]]
const typename InputBuffer::char_type& InputBuffer::peek()
{
    auto& cur = this->current_;
    auto& cur_buf = this->current_buffer_;

    if (cur == nullptr)
    {
        char_type end = BUFFER_END;
        return std::cref<char_type>(end);
    }

    pointer forward = cur + 1;
    if (*cur == BUFFER_END)
    {
        if (!refresh_buffer(cur_buf ^ 1))
        {
            char_type end = BUFFER_END;
            return std::cref<char_type>(end);
        }

        swap_buffers_();
        forward = cur + 1;
    }

    if (*forward == BUFFER_END)
    {
        if (!refresh_buffer(cur_buf ^ 1))
        {
            char_type end = BUFFER_END;
            return std::cref<char_type>(end);
        }

        swap_buffers_();
        forward = cur + 1;
    }

    return *forward;
}

typename InputBuffer::string_type InputBuffer::n_peek(size_type n)
{
    auto& cur_buf = this->current_buffer_;
    auto& bufs = this->buffers_;
    auto& cur = this->current_;

    string_type out;
    if (n == 0)
    {
        return out;
    }

    size_type rem = n;
    int buf_idx = cur_buf;
    size_type offset = static_cast<size_type>(cur - bufs[buf_idx].data() + 1);

    while (rem > 0)
    {
        if (offset >= bufs[buf_idx].size() || bufs[buf_idx][offset] == BUFFER_END)
        {
            if (!refresh_buffer(buf_idx ^ 1))
            {
                break;
            }

            buf_idx ^= 1;
            offset = 0;
        }

        out.push_back(bufs[buf_idx][offset]);
        offset++;
        rem--;
    }

    return out;
}


void InputBuffer::consume(size_type len)
{
    while (len-- > 0)
    {
        consume_char();
    }
}

void InputBuffer::unget(char_type ch)
{
    // store previous position instead of current one
    Position prev_pos = current_position_;
    rewind_position_(ch);
    unget_stack_.push({ch, prev_pos});
}

void InputBuffer::reset()
{
    auto& cur_buf = this->current_buffer_;
    auto& bufs = this->buffers_;
    auto& cur = this->current_;
    auto& cur_pos = this->current_position_;
    auto& cols = this->columns_;

    cur_buf = 0;
    bufs[0][0] = BUFFER_END;
    bufs[0][1] = BUFFER_END;
    cur = bufs[0].data();
    cur_pos = {1, 1, 0};

    while (!cols.empty())
    {
        cols.pop();
    }

    cols.push(1);
}

Position InputBuffer::position() const noexcept { return this->current_position_; }

void InputBuffer::swap_buffers_()
{
    auto& cur_buf = this->current_buffer_;
    auto& bufs = this->buffers_;
    auto& cur = this->current_;
    auto& cols = this->columns_;

    cur_buf ^= 1;
    cur = bufs[cur_buf].data();

    if (cols.empty())
    {
        cols.push(1);
    }
}

void InputBuffer::advance_position_(char_type ch)
{
    auto& cur_pos = this->current_position_;
    auto& cols = this->columns_;

    cur_pos.file_pos_ += 1;

    if (ch == u'\n')
    {
        cur_pos.line_ += 1;
        cur_pos.column_ = 1;
        cols.push(1);
    }
    else
    {
        cur_pos.column_ += 1;
        if (!cols.empty())
        {
            cols.top() = cur_pos.column();
        }
        else
        {
            cols.push(cur_pos.column());
        }
    }
}

void InputBuffer::rewind_position_(char_type ch)
{
    auto& cur_pos = this->current_position_;
    auto& cols = this->columns_;

    if (cur_pos.fpos() == 0)
    {
        // TODO: ultimately should emit an error
        return;
    }

    cur_pos.file_pos_ = std::max<size_type>(0, cur_pos.fpos() - 1);

    if (ch == u'\n')
    {
        if (!cols.empty())
        {
            cols.pop();
        }

        cur_pos.line_ = std::max<size_type>(1, cur_pos.line() - 1);
        cur_pos.column_ = cols.empty() ? 1 : cols.top();
    }
    else
    {
        cur_pos.column_ = (cur_pos.column() > 0 ? cur_pos.column() - 1 : 0);
        if (!cols.empty())
        {
            cols.top() = cur_pos.column();
        }
    }
}

}
}  // lex
}  // mylang