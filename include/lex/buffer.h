#pragma once

#include <cassert>
#include <fstream>
#include <iostream>
#include <stack>

#include "../utfcpp/source/utf8.h"  // for utf-8 encoding/decoding
#include "lex/util.h"
#include "macros.h"

struct Position
{
    std::size_t line_;
    std::size_t column_;

    Position() :
        line_(0),
        column_(0) {}

    Position(const std::size_t line, const std::size_t col) :
        line_(line),
        column_(col) {}
};

// TODO : remove after debug
static inline void print_vector(const std::vector<char16_t>& vec) {
    std::cout << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        std::cout << to_utf8(std::wstring(1, vec[i]));
        if (i + 1 < vec.size())
        {
            std::cout << ", ";
        }
    }
    std::cout << "]\n";
}

class InputBase
{
   public:
    using buffer_t  = std::u16string;
    using size_type = std::size_t;

    InputBase(std::ifstream& f, size_type cap = DEFAULT_CAPACITY) :
        file_(f),
        byte_position_(0),
        char_count_(0) {
        buffers_[0].resize(cap + 1, BUF_END);
        buffers_[1].resize(cap + 1, BUF_END);

        // Open file in binary mode for proper UTF-8 reading
        if (!file_.is_open())
        {
            throw std::runtime_error("File is not open");
        }
    }

    bool empty() const noexcept { return !file_.is_open(); }

    bool refresh_buffer(const unsigned int to_refresh) {
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

   protected:
    std::ifstream& file_;
    buffer_t       buffers_[2];
    std::size_t    byte_position_;  // Current byte position in file
    std::size_t    char_count_;     // Total characters read so far

   private:
    // Read up to max_chars wide characters from the current file position
    std::u16string read_wchar_window(std::size_t max_chars) {
        if (max_chars == 0)
        {
            return {};
        }

        if (!file_)
        {
            return {};
        }

        // Read a chunk of UTF-8 bytes
        // We read more bytes than characters needed because UTF-8 is variable-length
        // Average UTF-8 character is about 1.5 bytes for typical text
        size_t            byte_chunk_size = max_chars * 4;  // Conservative estimate
        std::vector<char> byte_buffer(byte_chunk_size);

        file_.read(byte_buffer.data(), byte_chunk_size);
        std::streamsize bytes_read = file_.gcount();

        if (bytes_read <= 0)
        {
            return {};
        }

        // Resize to actual bytes read
        byte_buffer.resize(bytes_read);

        // Handle partial UTF-8 characters at the end
        size_t valid_bytes = bytes_read;

        // Check if we ended in the middle of a multi-byte character
        if (bytes_read > 0)
        {
            unsigned char last_byte = static_cast<unsigned char>(byte_buffer[bytes_read - 1]);

            // If the last byte is a continuation byte or start of multi-byte sequence,
            // we need to find where the last complete character ends
            int bytes_to_rewind = 0;

            for (int i = bytes_read - 1; i >= 0 && i >= bytes_read - 4; --i)
            {
                unsigned char byte = static_cast<unsigned char>(byte_buffer[i]);

                if ((byte & 0x80) == 0)
                {
                    // Single-byte character (ASCII)
                    break;
                }
                else if ((byte & 0xC0) == 0xC0)
                {
                    // Start of multi-byte character
                    // Check if we have all bytes for this character
                    int expected_bytes = 0;
                    if ((byte & 0xE0) == 0xC0)
                        expected_bytes = 2;
                    else if ((byte & 0xF0) == 0xE0)
                        expected_bytes = 3;
                    else if ((byte & 0xF8) == 0xF0)
                        expected_bytes = 4;

                    if (i + expected_bytes > bytes_read)
                    {
                        // Incomplete character, rewind to before it
                        bytes_to_rewind = bytes_read - i;
                    }
                    break;
                }
            }

            if (bytes_to_rewind > 0)
            {
                valid_bytes = bytes_read - bytes_to_rewind;
                byte_buffer.resize(valid_bytes);

                // Seek back so we can read this character next time
                file_.seekg(-bytes_to_rewind, std::ios_base::cur);
            }
        }

        // Update byte position
        byte_position_ += valid_bytes;

        // Decode UTF-8 to wide characters
        std::string u8_str;
        for (auto b : byte_buffer)
        {
            u8_str.push_back(b);
        }

        std::u16string result = utf8::utf8to16(u8_str);

        // Limit to max_chars if we got more
        if (result.size() > max_chars)
        {
            // Need to seek back the extra bytes we read
            // This is approximate - we'd need to track exact byte positions per character
            result.resize(max_chars);
        }

        char_count_ += result.size();

        return result;
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
        InputBase(f, cap) {
        buffers_[0].resize(capacity_ + 1, BUF_END);
        buffers_[1].resize(capacity_ + 1, BUF_END);
        reset();
    }

    bool empty() const { return (!this->file_.is_open()); }

    char_type consume_char() {
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
    char_type current() {
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
    char_type peek() {
        size_type next_idx = this->current_ + 1;
        if (next_idx >= this->buffers_[this->current_buffer_].size())
        {
            // Avoid out-of-bounds
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

    string_type n_peek(size_type n) {
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

    void consume(size_type len) {
        while (len-- > 0)
        {
            this->consume_char();
        }
    }

    void unget(char_type ch) {
        // store previous position instead of current one
        Position prev_pos = this->current_position_;
        this->rewind_position_(ch);
        this->unget_stack_.push({ch, prev_pos});
    }

    void reset() {
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

    Position position() const noexcept { return this->current_position_; }

   private:
    size_type capacity_ = DEFAULT_CAPACITY;

    size_type current_        = 0;
    int       current_buffer_ = 0;
    size_type file_pos_       = 0;

    Position           current_position_;
    std::stack<size_t> columns_;

    struct PushbackEntry
    {
        char_type ch;
        Position  pos;
    };

    std::stack<PushbackEntry> unget_stack_;

    void swap_buffers_() {
        this->current_buffer_ ^= 1;
        this->current_ = 0;

        if (this->columns_.empty())
        {
            this->columns_.push(1);
        }
    }

    void advance_position_(char_type ch) {
        if (ch == L'\n')
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

    void rewind_position_(char_type ch) {
        if (ch == L'\n')
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
};