#pragma once

#include <cassert>
#include <cwchar>
#include <functional>
#include <memory>
#include <stack>
#include <stdexcept>

#include "lex/util.h"
#include "macros.h"


struct Buffer
{
 public:
  wchar_t*    start_;
  std::size_t size_;
  std::size_t capacity_;

  Buffer(std::size_t cap = 4096, const wchar_t* data = nullptr) {
    this->start_ = static_cast<wchar_t*>(std::calloc(cap + 1, sizeof(wchar_t)));

    if (this->start_ == nullptr)
      throw std::runtime_error("Failed to allocate memory: " + std::to_string(cap));

    this->capacity_ = cap;
    this->size_     = 0;

    if (data != nullptr)
    {
      std::size_t len = std::wcslen(data);
      if (len == 0)
      {
        this->start_[0] = BUF_END;
        this->size_     = 0;
        return;
      }

      if (len > cap)
        len = cap;  // truncate to capacity

      std::wmemcpy(this->start_, data, len);
      this->size_ = len;
    }

    this->start_[this->size_] = BUF_END;
  }

  Buffer(Buffer&& other) {
    if (this != &other)
    {
      // free existing resources
      if (start_)
        std::free(start_);

      // steal other's resources
      start_    = other.start_;
      size_     = other.size_;
      capacity_ = other.capacity_;

      // leave other in a safe empty state
      other.start_    = nullptr;
      other.size_     = 0;
      other.capacity_ = 0;
    }
  }

  Buffer& operator=(Buffer&& other) noexcept {
    if (this != &other)
    {
      // free existing resources
      if (start_)
        std::free(start_);

      // steal other's resources
      start_    = other.start_;
      size_     = other.size_;
      capacity_ = other.capacity_;

      // leave other in a safe empty state
      other.start_    = nullptr;
      other.size_     = 0;
      other.capacity_ = 0;
    }
    return *this;
  }

  Buffer(const Buffer&)            = delete;
  Buffer& operator=(const Buffer&) = delete;

  Buffer clone() const {
    Buffer copy(capacity_);
    if (size_ > 0)
      std::wmemcpy(copy.start_, start_, size_);
    copy.size_         = size_;
    copy.start_[size_] = L'\0';
    return copy;
  }

  wchar_t*    data() const { return this->start_; }
  std::size_t size() const { return this->size_; }

  ~Buffer() {
    if (this->start_)
      std::free(this->start_);
  }
};

struct Position
{
  std::size_t                      line{0};
  std::size_t                      column{0};
  std::pair<unsigned, std::size_t> buf_pos;  // first : buffer index, second : offset
  std::size_t                      raw_offset{0};

  Position() = default;

  Position(const std::size_t l,
           const std::size_t c,
           const unsigned    buf_idx,
           const std::size_t offset,
           const std::size_t raw) :
      line(l),
      column(c),
      buf_pos(buf_idx, offset),
      raw_offset(raw) {}
};

struct PushbackEntry
{
  wchar_t  ch;
  Position pos;
};

class InputBuffer
{
 public:
  using size_type = std::size_t;
  using char_type = wchar_t;
  using pointer   = wchar_t*;

  InputBuffer() = default;

  explicit InputBuffer(FILE* fp, size_type cap = 4096) :
      fileptr_(fp),
      capacity_(cap),
      file_pos_(0),
      buffers_{Buffer(cap), Buffer(cap)} {
    assert(fp != nullptr && "Provided file pointer is NULL");
    reset();
  }

  InputBuffer(const InputBuffer& other) :
      buffers_{other.buffers_[0].clone(), other.buffers_[1].clone()},
      current_position_(other.current_position_),
      current_buffer_(other.current_buffer_),
      capacity_(other.capacity_),
      columns_(other.columns_),
      unget_stack_(other.unget_stack_),
      fileptr_(nullptr),
      file_pos_(other.file_pos_) {
    if (other.current_)
    {
      size_type offset_cur =
        static_cast<size_type>(other.current_ - other.buffers_[other.current_buffer_].data());
      current_ = buffers_[current_buffer_].data() + offset_cur;
    }
    else
    {
      current_ = nullptr;
    }

    if (other.forward_)
    {
      size_type offset_fwd =
        static_cast<size_type>(other.forward_ - other.buffers_[other.current_buffer_].data());
      forward_ = buffers_[current_buffer_].data() + offset_fwd;
    }
    else
    {
      forward_ = nullptr;
    }
  }

  // --- Move constructor/assignment ---
  InputBuffer(InputBuffer&&) noexcept            = default;
  InputBuffer& operator=(InputBuffer&&) noexcept = default;

  // --- Swap helper ---
  friend void swap(InputBuffer& lhs, InputBuffer& rhs) noexcept {
    using std::swap;

    // Move-swap the underlying buffers explicitly
    Buffer tmp0     = std::move(lhs.buffers_[0]);
    lhs.buffers_[0] = std::move(rhs.buffers_[0]);
    rhs.buffers_[0] = std::move(tmp0);

    Buffer tmp1     = std::move(lhs.buffers_[1]);
    lhs.buffers_[1] = std::move(rhs.buffers_[1]);
    rhs.buffers_[1] = std::move(tmp1);

    // Swap trivial/standard members
    swap(lhs.current_position_, rhs.current_position_);
    swap(lhs.current_, rhs.current_);
    swap(lhs.forward_, rhs.forward_);
    swap(lhs.current_buffer_, rhs.current_buffer_);
    swap(lhs.capacity_, rhs.capacity_);
    swap(lhs.columns_, rhs.columns_);
    swap(lhs.unget_stack_, rhs.unget_stack_);
    swap(lhs.file_pos_, rhs.file_pos_);

    // FILE* semantics: borrow vs own
    swap(lhs.fileptr_, rhs.fileptr_);
  }

  // get

  Buffer& buffer(const int index) {
    if (index < 0 || index > 2)
      throw std::out_of_range("Index out of range!");
    return buffers_[index];
  }
  pointer   data() const noexcept { return buffers_[current_buffer_].data(); }
  size_type size() const noexcept { return buffers_[0].size_ + buffers_[1].size_; }
  size_type capacity() const noexcept { return capacity_; }
  Position  pos() const noexcept { return current_position_; }

  bool empty() const noexcept {
    pointer p = buffers_[current_buffer_].data();
    return !p || (*p == BUF_END);
  }
  size_type remaining() const noexcept {
    return buffers_[current_buffer_].size_ - current_position_.buf_pos.second;
  }

  // commits change of pointers
  char_type next() {
    if (!unget_stack_.empty())
    {
      auto entry = unget_stack_.top();
      unget_stack_.pop();
      current_position_ = entry.pos;
      return entry.ch;
    }

    if (!forward_ || *forward_ == BUF_END)
    {
      if (!refresh(current_buffer_ ^ 1))
        return BUF_END;
      swap_buffers();
    }

    char_type ch = *forward_;
    ++forward_;
    return ch;
  }

  // gives next char without changing pointers
  char_type peek() {
    if (!forward_)
      return BUF_END;

    pointer nxt = forward_ + 1;
    if (*nxt == BUF_END)
    {
      if (!refresh(current_buffer_ ^ 1))
        return BUF_END;
      swap_buffers();
      nxt = forward_ + 1;
      if (*nxt == BUF_END)
        return BUF_END;
    }
    return *nxt;
  }

  std::wstring n_peek(size_type n) {
    std::wstring out;
    if (!forward_ || n == 0)
      return out;

    pointer   it      = forward_;
    size_type rem     = n;
    unsigned  buf_idx = current_buffer_;
    size_type offset  = static_cast<size_type>(it - buffers_[buf_idx].data());

    while (rem > 0)
    {
      if (offset >= buffers_[buf_idx].size_)
      {
        if (!refresh(buf_idx ^ 1))
          break;
        buf_idx ^= 1;
        it     = buffers_[buf_idx].data();
        offset = 0;
        if (buffers_[buf_idx].size_ == 0)
          break;
      }
      out.push_back(it[offset]);
      ++offset;
      --rem;
    }
    return out;
  }

  void consume_char() {
    char_type ch = next();
    if (ch == BUF_END)
      return;
    advance_position(ch);
  }

  // consumes an entire lexeme at once
  void consume(size_type len = 1) {
    for (; len > 0; --len)
      consume_char();
  }

  void unget(char_type ch) {
    unget_stack_.push(PushbackEntry{ch, current_position_});
    rewind_position(ch);
  }

  void reset() {
    current_buffer_   = 0;
    buffers_[0].size_ = buffers_[1].size_ = 0;
    current_ = forward_ = buffers_[0].data();
    current_position_   = Position(1, 1, 0, 0, 0);
    while (!columns_.empty())
      columns_.pop();
    columns_.push(1);
  }

 private:
  Buffer                    buffers_[2];
  Position                  current_position_;
  pointer                   current_{nullptr};
  pointer                   forward_{nullptr};
  unsigned                  current_buffer_ = 0;
  size_type                 capacity_;
  std::stack<size_type>     columns_;
  std::stack<PushbackEntry> unget_stack_;
  FILE*                     fileptr_;
  size_type                 file_pos_;  // in wchar_t units

  // --- Helpers ---
  void swap_buffers() {
    current_buffer_ ^= 1;
    current_ = forward_       = buffers_[current_buffer_].data();
    current_position_.buf_pos = std::make_pair(current_buffer_, 0);
    if (columns_.empty())
      columns_.push(1);
  }

  bool refresh(int buffer_index) {
    if (buffer_index < 0 || buffer_index > 1 || !fileptr_)
      return false;
    if (std::fseek(fileptr_, static_cast<long>(file_pos_ * sizeof(wchar_t)), SEEK_SET) != 0)
      return false;

    size_type read_count =
      std::fread(buffers_[buffer_index].data(), sizeof(wchar_t), capacity_, fileptr_);
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

  void advance_position(char_type ch) {
    current_position_.raw_offset += utf8_size(ch);
    if (ch == L'\n')
    {
      ++current_position_.line;
      current_position_.column = 1;
      columns_.push(current_position_.column);
    }
    else
    {
      ++current_position_.column;
      if (!columns_.empty())
        columns_.top() = current_position_.column;
      else
        columns_.push(current_position_.column);
    }
    ++current_position_.buf_pos.second;
  }

  void rewind_position(char_type ch) {
    if (ch == L'\n')
    {
      if (!columns_.empty())
        columns_.pop();
      current_position_.line   = std::max<size_type>(1, current_position_.line - 1);
      current_position_.column = columns_.empty() ? 1 : columns_.top();
    }
    else
    {
      current_position_.column = (current_position_.column > 0 ? current_position_.column - 1 : 0);
      if (!columns_.empty())
        columns_.top() = current_position_.column;
    }
    current_position_.raw_offset = (current_position_.raw_offset >= utf8_size(ch))
                                   ? current_position_.raw_offset - utf8_size(ch)
                                   : 0;
    current_position_.buf_pos.second =
      (current_position_.buf_pos.second > 0 ? current_position_.buf_pos.second - 1 : 0);
  }
};