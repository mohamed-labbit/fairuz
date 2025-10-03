
/*
#include "lex/buffer.h"


typename InputBuffer::pointer InputBuffer::data() const noexcept { return buffers_[current_buffer_].data(); }

typename InputBuffer::size_type InputBuffer::size() const noexcept { return buffers_[0].size_ + buffers_[1].size_; }

typename InputBuffer::size_type InputBuffer::capacity() const noexcept { return capacity_; }

Position InputBuffer::position() const noexcept { return current_position_; }

bool InputBuffer::empty() const noexcept {
    pointer p = buffers_[current_buffer_].data();
    return p == nullptr || (*p == BUF_END);
}

typename InputBuffer::char_type InputBuffer::consume_char() {
    char_type ch;
    
    if (unget_stack_.empty() == false)
    {
        auto entry = unget_stack_.top();
        unget_stack_.pop();
        current_position_ = entry.pos;
        ch                = entry.ch;
    }
    else
    {
        if (forward_ == nullptr || *forward_ == BUF_END)
        {
            if (refresh_(current_buffer_ ^ 1) == false)
            {
                return BUF_END;
            }
            
            swap_buffers_();
        }
        
        ch = *forward_;
        if (ch == BUF_END)
        {
            return BUF_END;
        }
        
        forward_ += 1;
    }
    
    advance_position_(ch);
    return ch;
}

typename InputBuffer::char_type InputBuffer::current() {
    if (!current_ || *current_ == BUF_END)
    {
        if (!refresh_(current_buffer_ ^ 1))
        {
            return BUF_END;
        }
        
        swap_buffers_();
    }
    return *current_;
}

// gives next char without changing pointers
typename InputBuffer::char_type InputBuffer::peek() {
    if (forward_ == nullptr)
    {
        return BUF_END;
    }
    
    pointer nxt = forward_;
    if (*forward_ == BUF_END)
    {
        if (refresh_(current_buffer_ ^ 1) == false)
        {
            return BUF_END;
        }
        
        swap_buffers_();
    }
    return *forward_;
}

typename InputBuffer::string_type InputBuffer::n_peek(size_type n) {
    std::wstring out;
    if (forward_ == nullptr || n == 0)
    {
        return out;
    }
    
    pointer   it      = forward_;
    size_type rem     = n;
    unsigned  buf_idx = current_buffer_;
    size_type offset  = static_cast<size_type>(it - buffers_[buf_idx].data());
    
    while (rem > 0)
    {
        if (offset >= buffers_[buf_idx].size_)
        {
            if (refresh_(buf_idx ^ 1) == false)
            {
                break;
            }
            
            buf_idx ^= 1;
            it     = buffers_[buf_idx].data();
            offset = 0;
            if (buffers_[buf_idx].size_ == 0)
            {
                break;
            }
        }
        out.push_back(it[offset]);
        offset += 1;
        rem -= 1;
    }
    return out;
}


// consumes an entire lexeme at once
void InputBuffer::consume(size_type len) {
    for (; len > 0; len -= 1)
    {
        consume_char();
    }
}

void InputBuffer::unget(char_type ch) {
    unget_stack_.push(PushbackEntry{ch, current_position_});
    rewind_position_(ch);
}

void InputBuffer::reset_() {
    current_buffer_   = 0;
    buffers_[0].size_ = buffers_[1].size_ = 0;
    buffers_[0].data()[0] = buffers_[1].data()[0] = BUF_END;
    current_ = forward_       = buffers_[0].data();
    current_position_.line_   = 1;
    current_position_.column_ = 1;
    
    while (columns_.empty() == false)
    {
        columns_.pop();
    }
    
    columns_.push(1);
}

// --- Helpers ---
void InputBuffer::swap_buffers_() {
    current_buffer_ ^= 1;
    current_ = forward_ = buffers_[current_buffer_].data();
    
    if (columns_.empty())
    {
        columns_.push(1);
    }
}

bool InputBuffer::refresh_(int buffer_index) {
    if (buffer_index < 0 || buffer_index > 1 || !fileptr_)
    {
        return false;
    }

    size_type read_count = std::fread(buffers_[buffer_index].data(), sizeof(wchar_t), capacity_, fileptr_);
    if (read_count == 0)
    {
        buffers_[buffer_index].size_     = 0;
        buffers_[buffer_index].data()[0] = BUF_END;
        return false;
    }
    
    buffers_[buffer_index].size_              = read_count;
    buffers_[buffer_index].data()[read_count] = BUF_END;
    file_pos_ += read_count;
    
    return true;
}

void InputBuffer::advance_position_(char_type ch) {
    if (ch == L'\n')
    {
        current_position_.line_ += 1;
        current_position_.column_ = 1;
        columns_.push(current_position_.column_);
    }
    else
    {
        current_position_.column_ += 1;
        if (columns_.empty() == false)
        {
            columns_.top() = current_position_.column_;
        }
        else
        {
            columns_.push(current_position_.column_);
        }
    }
}

void InputBuffer::rewind_position_(char_type ch) {
    if (ch == L'\n')
    {
        if (columns_.empty() == false)
        {
            columns_.pop();
        }
        
        current_position_.line_   = std::max<size_type>(1, current_position_.line_ - 1);
        current_position_.column_ = columns_.empty() ? 1 : columns_.top();
    }
    else
    {
        current_position_.column_ = (current_position_.column_ > 0 ? current_position_.column_ - 1 : 0);
        if (columns_.empty() == false)
        {
            columns_.top() = current_position_.column_;
        }
    }
}
*/