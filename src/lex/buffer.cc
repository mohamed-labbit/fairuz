#include "lex/buffer.h"


bool InputBase::refresh_buffer(const unsigned int to_refresh) {
    if (!file_.is_open())
    {
        return false;
    }

    size_type max_chars = buffers_[to_refresh].size() - 1;
    auto      buf       = read_wchar_window(max_chars);

    if (buf.empty())
    {
        buffers_[to_refresh].clear();
        buffers_[to_refresh].push_back(BUF_END);
        return false;
    }

    buffers_[to_refresh].assign(buf.begin(), buf.end());
    buffers_[to_refresh].push_back(BUF_END);

    return true;
}

typename InputBase::buffer_t InputBase::read_wchar_window(size_type max_chars) {
    if (max_chars == 0)
    {
        return {};
    }

    if (!file_.is_open())
    {
        return {};
    }

    size_type         byte_chunk_size = max_chars * 4;  // Conservative estimate
    std::vector<char> byte_buffer(byte_chunk_size);

    file_.read(byte_buffer.data(), byte_chunk_size);
    std::streamsize bytes_read = file_.gcount();

    if (bytes_read <= 0)
    {
        return {};
    }

    byte_buffer.resize(bytes_read);

    size_type valid_bytes = bytes_read;

    if (bytes_read > 0)
    {
        unsigned char last_byte = static_cast<unsigned char>(byte_buffer[bytes_read - 1]);

        int bytes_to_rewind = 0;

        for (int i = bytes_read - 1; i >= 0 && i >= bytes_read - 4; --i)
        {
            unsigned char byte = static_cast<unsigned char>(byte_buffer[i]);

            if ((byte & 0x80) == 0)
            {
                break;
            }
            else if ((byte & 0xC0) == 0xC0)
            {
                int expected_bytes = 0;
                if ((byte & 0xE0) == 0xC0)
                {
                    expected_bytes = 2;
                }
                else if ((byte & 0xF0) == 0xE0)
                {
                    expected_bytes = 3;
                }
                else if ((byte & 0xF8) == 0xF0)
                {
                    expected_bytes = 4;
                }

                if (i + expected_bytes > bytes_read)
                {
                    bytes_to_rewind = bytes_read - i;
                }
                break;
            }
        }

        if (bytes_to_rewind > 0)
        {
            valid_bytes = bytes_read - bytes_to_rewind;
            byte_buffer.resize(valid_bytes);
            file_.seekg(-bytes_to_rewind, std::ios_base::cur);
        }
    }

    byte_position_ += valid_bytes;

    std::string u8_str;
    for (auto b : byte_buffer)
    {
        u8_str.push_back(b);
    }

    buffer_t result = utf8::utf8to16(u8_str);

    if (result.size() > max_chars)
    {
        result.resize(max_chars);
    }

    char_count_ += result.size();

    return result;
}


typename InputBuffer::size_type InputBuffer::size() const { return this->buffers_[this->current_buffer_].length(); }

typename InputBuffer::size_type InputBuffer::buffer_offset() const { return this->current_; }

bool InputBuffer::empty() const { return (!this->file_.is_open()); }

typename InputBuffer::char_type InputBuffer::at(const size_type idx) const {
    return this->buffers_[this->current_buffer_][idx];
}

typename InputBuffer::char_type InputBuffer::consume_char() {
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
        if (this->buffers_[this->current_buffer_][this->current_] == BUF_END)
        {
            if (!this->refresh_buffer(this->current_buffer_ ^ 1))
            {
                return BUF_END;
            }

            this->swap_buffers_();
        }

        ch = this->buffers_[this->current_buffer_][this->current_];
        this->current_ += 1;
    }

    this->advance_position_(ch);
    return ch;
}

[[nodiscard]]
typename InputBuffer::char_type InputBuffer::current() {
    if (this->buffers_[this->current_buffer_][this->current_] == BUF_END)
    {
        if (!this->refresh_buffer(this->current_buffer_ ^ 1))
        {
            return BUF_END;
        }

        this->swap_buffers_();
    }

    return this->buffers_[this->current_buffer_][this->current_];
}

[[nodiscard]]
typename InputBuffer::char_type InputBuffer::peek() {
    size_type next_idx = this->current_ + 1;
    if (next_idx >= this->buffers_[this->current_buffer_].size())
    {
        if (!this->refresh_buffer(this->current_buffer_ ^ 1))
        {
            return BUF_END;
        }

        this->swap_buffers_();
        next_idx = 0;
    }

    if (this->buffers_[this->current_buffer_][next_idx] == BUF_END)
    {
        if (!this->refresh_buffer(this->current_buffer_ ^ 1))
        {
            return BUF_END;
        }

        this->swap_buffers_();
        next_idx = 0;
    }

    return this->buffers_[this->current_buffer_][next_idx];
}

typename InputBuffer::string_type InputBuffer::n_peek(size_type n) {
    string_type out;
    if (n == 0)
    {
        return out;
    }

    size_type rem     = n;
    int       buf_idx = this->current_buffer_;
    size_type offset  = this->current_ + 1;

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


void InputBuffer::consume(size_type len) {
    while (len-- > 0)
    {
        this->consume_char();
    }
}

void InputBuffer::unget(char_type ch) {
    // store previous position instead of current one
    Position prev_pos = this->current_position_;
    this->rewind_position_(ch);
    this->unget_stack_.push({ch, prev_pos});
}

void InputBuffer::reset() {
    this->current_buffer_ = 0;
    this->buffers_[0][0] = this->buffers_[1][0] = BUF_END;
    this->current_                              = 0;
    this->current_position_                     = {1, 1};

    while (!this->columns_.empty())
    {
        this->columns_.pop();
    }

    this->columns_.push(1);
}

Position InputBuffer::position() const noexcept { return this->current_position_; }

void InputBuffer::swap_buffers_() {
    this->current_buffer_ ^= 1;
    this->current_ = 0;

    if (this->columns_.empty())
    {
        this->columns_.push(1);
    }
}

void InputBuffer::advance_position_(char_type ch) {
    if (ch == u'\n')
    {
        this->current_position_.line_++;
        this->current_position_.column_ = 1;
        this->columns_.push(1);
    }
    else
    {
        this->current_position_.column_++;
        if (!this->columns_.empty())
        {
            this->columns_.top() = this->current_position_.column_;
        }
        else
        {
            this->columns_.push(this->current_position_.column_);
        }
    }
}

void InputBuffer::rewind_position_(char_type ch) {
    if (ch == u'\n')
    {
        if (!this->columns_.empty())
        {
            this->columns_.pop();
        }

        this->current_position_.line_   = std::max<size_type>(1, this->current_position_.line_ - 1);
        this->current_position_.column_ = this->columns_.empty() ? 1 : this->columns_.top();
    }
    else
    {
        this->current_position_.column_ =
          (this->current_position_.column_ > 0 ? this->current_position_.column_ - 1 : 0);
        if (!this->columns_.empty())
        {
            this->columns_.top() = this->current_position_.column_;
        }
    }
}