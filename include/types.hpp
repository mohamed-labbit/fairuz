#pragma once

#include "macros.hpp"
#include "runtime/allocator/arena.hpp"
#include <ostream>


namespace mylang {

class StringRef
{
 private:
  typedef CharType*        Pointer;
  typedef const CharType*  ConstPointer;
  typedef StringRef&       Reference;
  typedef const StringRef& ConstReference;

  CharType* Ptr_{nullptr};
  SizeType  Len_{0};

 public:
  StringRef() = default;

  StringRef(const SizeType s)
  {
    Ptr_ = runtime::allocator::arena_allocator.allocate<CharType>(s);
    if (Ptr_ == nullptr)
    {
      throw std::bad_alloc();
    }

    Len_ = s;
  }

  StringRef(ConstReference other)
  {
    if (other.len() == 0)
    {
      return;
    }

    Ptr_ = runtime::allocator::arena_allocator.allocate<CharType>(other.len());
    if (Ptr_ == nullptr)
    {
      throw std::bad_alloc();
    }

    std::memcpy(Ptr_, other.get(), other.len());
    Len_ = other.len();
  }

  StringRef(ConstPointer lit)
  {
    if (lit == nullptr)
    {
      return;
    }

    // Calculate length of the null-terminated string
    SizeType length = 0;
    while (lit[length] != CharType{0})
    {
      ++length;
    }

    // Handle empty string
    if (length == 0)
    {
      return;
    }

    // Allocate
    Ptr_ = runtime::allocator::arena_allocator.allocate<CharType>(length);
    if (Ptr_ == nullptr)
    {
      throw std::bad_alloc();
    }

    // Copy the string data
    std::memcpy(Ptr_, lit, length * sizeof(CharType));
    Len_ = length;
  }

  Reference operator=(ConstReference other)
  {
    if (this == &other)
    {
      return *this;  // Self-assignment guard
    }

    if (other.len() == 0)
    {
      Ptr_ = nullptr;
      Len_ = 0;
      return *this;
    }

    Ptr_ = runtime::allocator::arena_allocator.allocate<CharType>(other.len());
    if (Ptr_ == nullptr)
    {
      throw std::bad_alloc();
    }

    std::memcpy(Ptr_, other.get(), other.len());
    Len_ = other.len();

    return *this;  // Return reference for chaining
  }

  ~StringRef() = default;

  bool operator==(ConstReference other) const noexcept
  {
    // different lengths
    if (Len_ != other.Len_)
    {
      return false;
    }

    // both empty
    if (Len_ == 0)
    {
      return true;
    }

    // self comparison
    if (Ptr_ == other.Ptr_)
    {
      return true;
    }

    // Deep comparison
    return std::memcmp(Ptr_, other.Ptr_, Len_ * sizeof(CharType)) == 0;
  }

  bool operator!=(ConstReference other) const noexcept { return !(*this == other); }

  SizeType     len() const noexcept { return Len_; }
  ConstPointer get() const noexcept { return Ptr_; }
  bool         empty() const noexcept { return Ptr_ == nullptr; }

  friend std::ostream& operator<<(std::ostream& os, ConstReference str)
  {
    if (str.empty())
    {
      return os;
    }

    std::string utf8_result;
    utf8_result.reserve(str.len() * 3);  // 3 bytes at most

    for (SizeType i = 0; i < str.len(); ++i)
    {
      CharType ch = str.get()[i];

      if (ch < 0x80)  // ASCII (1 byte)
      {
        utf8_result.push_back(static_cast<char>(ch));
      }
      else if (ch < 0x800)  // 2 bytes
      {
        utf8_result.push_back(static_cast<char>(0xC0 | (ch >> 6)));
        utf8_result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
      }
      else if (ch >= 0xD800 && ch <= 0xDBFF)  // Surrogate pair (4 bytes)
      {
        if (i + 1 < str.len())
        {
          char16_t ch2 = str.get()[i + 1];
          if (ch2 >= 0xDC00 && ch2 <= 0xDFFF)
          {
            uint32_t codepoint = 0x10000 + ((ch & 0x3FF) << 10) + (ch2 & 0x3FF);
            utf8_result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            utf8_result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            utf8_result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            utf8_result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            ++i;  // skip the next char16_t (low surrogate)
            continue;
          }
        }
      }
      else  // 3 bytes
      {
        utf8_result.push_back(static_cast<char>(0xE0 | (ch >> 12)));
        utf8_result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
        utf8_result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
      }
    }

    os << utf8_result;
    return os;
  }
};

}