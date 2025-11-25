#include "../../../include/input/buffer/input_buffer.hpp"

#include <functional>


namespace mylang {
namespace lex {
namespace buffer {


std::size_t InputBuffer::size() const { return buffers_[current_buffer_].length(); }

std::size_t InputBuffer::buffer_offset() const
{
    return static_cast<std::size_t>(current_ - buffers_[current_buffer_].data());
}

bool InputBuffer::empty() const
{
    return (!file_manager_->is_open() || current_ == nullptr || buffers_[current_buffer_].empty());
}

char16_t InputBuffer::at(const std::size_t idx) const { return buffers_[current_buffer_][idx]; }

char16_t InputBuffer::consume_char()
{
    auto& unget_stack = unget_stack_;
    auto& cur_pos = current_position_;
    auto& cur_buf = current_buffer_;
    auto& cur = current_;

    char16_t ch;
    if (!unget_stack.empty())
    {
        cur_pos = unget_stack.top().pos;
        ch = unget_stack.top().ch;
        unget_stack.pop();
    }
    else
    {
        if (*cur == BUFFER_END)
        {
            if (!refresh_buffer(cur_buf ^ 1))
                return BUFFER_END;
            swap_buffers_();
        }
        ch = *cur;
        cur += 1;
    }
    advance_position_(ch);
    return ch;
}

MYLANG_NODISCARD
const char16_t& InputBuffer::current()
{
    auto& cur = current_;
    auto& cur_buf = current_buffer_;
    static const char16_t end = BUFFER_END;

    if (cur == nullptr)
        return end;
    if (*cur == BUFFER_END)
    {
        if (!refresh_buffer(cur_buf ^ 1))
            return end;
        swap_buffers_();
    }
    return *cur;
}

MYLANG_NODISCARD
const char16_t& InputBuffer::peek()
{
    auto& cur = current_;
    auto& cur_buf = current_buffer_;
    static const char16_t end = BUFFER_END;

    if (cur == nullptr)
        return end;
    pointer forward = cur + 1;
    if (*cur == BUFFER_END)
    {
        if (!refresh_buffer(cur_buf ^ 1))
            return end;
        swap_buffers_();
        forward = cur + 1;
    }

    if (*forward == BUFFER_END)
    {
        if (!refresh_buffer(cur_buf ^ 1))
            return end;
        swap_buffers_();
        forward = cur + 1;
    }
    return *forward;
}

std::u16string InputBuffer::n_peek(std::size_t n)
{
    auto& cur_buf = current_buffer_;
    auto& bufs = buffers_;
    auto& cur = current_;

    std::u16string out;
    if (n == 0)
        return out;

    std::size_t rem = n;
    std::int32_t buf_idx = cur_buf;
    std::size_t offset = static_cast<std::size_t>(cur - bufs[buf_idx].data() + 1);
    while (rem > 0)
    {
        if (offset >= bufs[buf_idx].size() || bufs[buf_idx][offset] == BUFFER_END)
        {
            if (!refresh_buffer(buf_idx ^ 1))
                break;
            buf_idx ^= 1;
            offset = 0;
        }
        out.push_back(bufs[buf_idx][offset]);
        offset++;
        rem--;
    }

    return out;
}


void InputBuffer::consume(std::size_t len)
{
    while (len-- > 0)
        consume_char();
}

void InputBuffer::unget(char16_t ch)
{
    // store previous position instead of current one
    Position prev_pos = current_position_;
    rewind_position_(ch);
    unget_stack_.push({ch, prev_pos});
}

void InputBuffer::reset()
{
    auto& cur_buf = current_buffer_;
    auto& bufs = buffers_;
    auto& cur = current_;
    auto& cur_pos = current_position_;
    auto& cols = columns_;

    cur_buf = 0;
    bufs[0][0] = BUFFER_END;
    bufs[0][1] = BUFFER_END;
    cur = bufs[0].data();
    cur_pos = {1, 1, 0};

    while (!cols.empty())
        cols.pop();

    cols.push(1);
}

Position InputBuffer::position() const MYLANG_NOEXCEPT { return current_position_; }

void InputBuffer::swap_buffers_()
{
    auto& cur_buf = current_buffer_;
    auto& bufs = buffers_;
    auto& cur = current_;
    auto& cols = columns_;

    cur_buf ^= 1;
    cur = bufs[cur_buf].data();

    if (cols.empty())
        cols.push(1);
}

void InputBuffer::advance_position_(char16_t ch)
{
    auto& cur_pos = current_position_;
    auto& cols = columns_;
    cur_pos.filepos += 1;

    if (ch == u'\n')
    {
        cur_pos.line += 1;
        cur_pos.column = 1;
        cols.push(1);
    }
    else
    {
        cur_pos.column += 1;
        if (!cols.empty())
            cols.top() = cur_pos.column;
        else
            cols.push(cur_pos.column);
    }
}

void InputBuffer::rewind_position_(char16_t ch)
{
    auto& cur_pos = current_position_;
    auto& cols = columns_;

    if (cur_pos.filepos == 0)
        // TODO: ultimately should emit an error
        return;

    cur_pos.filepos = std::max<std::size_t>(0, cur_pos.filepos - 1);
    if (ch == u'\n')
    {
        if (!cols.empty())
            cols.pop();
        cur_pos.line = std::max<std::size_t>(1, cur_pos.line - 1);
        cur_pos.column = cols.empty() ? 1 : cols.top();
    }
    else
    {
        cur_pos.column = (cur_pos.column > 0 ? cur_pos.column - 1 : 0);
        if (!cols.empty())
            cols.top() = cur_pos.column;
    }
}

}
}  // lex
}  // mylang