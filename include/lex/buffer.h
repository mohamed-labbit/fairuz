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
        std::cout << utf8::utf16to8(std::u16string(1, vec[i]));
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

    bool refresh_buffer(const unsigned int to_refresh);

   protected:
    std::ifstream& file_;
    buffer_t       buffers_[2];
    std::size_t    byte_position_;  // Current byte position in file
    std::size_t    char_count_;     // Total characters read so far

   private:
    // Read up to max_chars wide characters from the current file position
    std::u16string read_wchar_window(std::size_t max_chars);
};
/*

class InputBase
{
    public:
    using buffer_t  = std::u16string;
    using size_type = std::size_t;
    
    InputBase(std::ifstream& f, size_type cap = DEFAULT_CAPACITY) :
        file_(f),
        byte_position_(0),
        char_count_(0) {
            buffer_.resize(cap + 1, BUF_END);

            // Open file in binary mode for proper UTF-8 reading
        if (!file_.is_open())
        {
            throw std::runtime_error("File is not open");
        }

        this->buffer_ = read_file();
    }
    
    bool empty() const noexcept { return !file_.is_open(); }

    protected:
    std::ifstream& file_;
    buffer_t       buffer_;
    std::size_t    byte_position_;  // Current byte position in file
    std::size_t    char_count_;     // Total characters read so far
    
    private:
    // Read up to max_chars wide characters from the current file position
    std::u16string read_file() {
        if (!file_.is_open())
        {
            return {};
        }
        
        std::string content((std::istreambuf_iterator<char>(file_)), std::istreambuf_iterator<char>());
        
        std::u16string ret = utf8::utf8to16(content);
        
        return ret;
    }
};
*/

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

    size_type size() const;

    size_type buffer_offset() const;

    bool empty() const;

    char_type at(const size_type idx) const;

    char_type consume_char();

    [[nodiscard]]
    char_type current();

    [[nodiscard]]
    char_type peek();

    string_type n_peek(size_type n);

    void consume(size_type len);

    void unget(char_type ch);

    void reset();

    Position position() const noexcept;

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

    void swap_buffers_();

    void advance_position_(char_type ch);

    void rewind_position_(char_type ch);
};

/*
class InputBuffer: public InputBase
{
    public:
    using char_type   = char16_t;
    using pointer     = char_type*;
    using string_type = std::u16string;
    using size_type   = size_t;
    
    InputBuffer(std::ifstream& f, size_type cap = DEFAULT_CAPACITY) :
    capacity_(cap),
    current_(0),
    InputBase(f, cap) {
        reset();
        }
        
        bool empty() const { return (!this->file_.is_open()); }
        
        char_type consume_char() {
            char_type ch = this->buffer_[this->current_];
            this->current_++;
            this->advance_position_(ch);
            return ch;
            }
            
            [[nodiscard]]
            char_type current() {
        return this->buffer_[this->current_];
        }
    
    [[nodiscard]]
    char_type peek() {
        return this->buffer_[this->current_ + 1];
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
       this->buffer_[this->buffer_.length()] = BUF_END;
       this->current_                        = 0;
       this->current_position_               = {1, 1};
       
       while (!this->columns_.empty())
       {
           this->columns_.pop();
        }
        
        this->columns_.push(1);
    }
    
    Position position() const noexcept { return this->current_position_; }
    
    private:
    size_type capacity_ = DEFAULT_CAPACITY;
    
    size_type current_  = 0;
    size_type file_pos_ = 0;
    
    Position           current_position_;
    std::stack<size_t> columns_;
    
    struct PushbackEntry
    {
        char_type ch;
        Position  pos;
    };
    
    std::stack<PushbackEntry> unget_stack_;
    
    void advance_position_(char_type ch) {
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
    
    void rewind_position_(char_type ch) {
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
};

*/