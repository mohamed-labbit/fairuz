#pragma once

#include "../utfcpp/source/utf8.h"
#include "macros.hpp"
#include "runtime/allocator/arena.hpp"

#include <iomanip>
#include <iostream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


namespace mylang {

struct String;

class StringAllocator
{
 private:
  runtime::allocator::ArenaAllocator Allocator_;

 public:
  StringAllocator() :
      Allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::LINEAR))
  {
  }

  template<typename... Args>
  String* allocate(Args&&... args)
  {
    void* mem = Allocator_.allocate(sizeof(String));
    if (!mem)
      throw std::bad_alloc();
    return new (mem) String(std::forward<Args>(args)...);
  }
};

inline StringAllocator string_allocator;

/*
namespace utf_internal {

// Constants from utf8cpp
static constexpr uint32_t LEAD_SURROGATE_MIN  = 0xd800u;
static constexpr uint32_t LEAD_SURROGATE_MAX  = 0xdbffu;
static constexpr uint32_t TRAIL_SURROGATE_MIN = 0xdc00u;
static constexpr uint32_t TRAIL_SURROGATE_MAX = 0xdfffu;
static constexpr uint32_t LEAD_OFFSET         = 0xd800u - (0x10000 >> 10);
static constexpr uint32_t SURROGATE_OFFSET    = 0x10000u - (0xd800u << 10) - 0xdc00u;

static constexpr uint32_t CODE_POINT_MAX = 0x10ffff;

// UTF-8 byte markers
static constexpr uint8_t TRAIL_BYTE_MASK = 0xc0u;
static constexpr uint8_t TRAIL_BYTE_MARK = 0x80u;

// Check if a UTF-16 value is a lead surrogate
static inline bool is_lead_surrogate(uint16_t cp) noexcept { return cp >= LEAD_SURROGATE_MIN && cp <= LEAD_SURROGATE_MAX; }

// Check if a UTF-16 value is a trail surrogate
static inline bool is_trail_surrogate(uint16_t cp) noexcept { return cp >= TRAIL_SURROGATE_MIN && cp <= TRAIL_SURROGATE_MAX; }

// Check if a UTF-8 byte is a trail byte
static inline bool is_trail(uint8_t octet) noexcept { return (octet & TRAIL_BYTE_MASK) == TRAIL_BYTE_MARK; }

// Check if a code point is valid
static inline bool is_code_point_valid(uint32_t cp) noexcept { return cp <= CODE_POINT_MAX && !is_lead_surrogate(cp) && !is_trail_surrogate(cp); }

// Mask a 16-bit value
static inline uint16_t mask16(uint16_t val) noexcept { return val; }

// Get sequence length from first UTF-8 byte
static inline size_t sequence_length(uint8_t lead) noexcept
{
  if (lead < 0x80)
    return 1;
  else if ((lead >> 5) == 0x6)
    return 2;
  else if ((lead >> 4) == 0xe)
    return 3;
  else if ((lead >> 3) == 0x1e)
    return 4;
  else
    return 0;
}

// Append a code point to UTF-8
template<typename OutputIterator>
static OutputIterator append_utf8(uint32_t cp, OutputIterator result)
{
  if (!is_code_point_valid(cp))
    throw std::runtime_error("Invalid code point");

  if (cp < 0x80)
    // 1-byte sequence
    *result++ = static_cast<uint8_t>(cp);
  else if (cp < 0x800)
  {  // 2-byte sequence
    *result++ = static_cast<uint8_t>((cp >> 6) | 0xc0);
    *result++ = static_cast<uint8_t>((cp & 0x3f) | 0x80);
  }
  else if (cp < 0x10000)
  {  // 3-byte sequence
    *result++ = static_cast<uint8_t>((cp >> 12) | 0xe0);
    *result++ = static_cast<uint8_t>(((cp >> 6) & 0x3f) | 0x80);
    *result++ = static_cast<uint8_t>((cp & 0x3f) | 0x80);
  }
  else
  {  // 4-byte sequence
    *result++ = static_cast<uint8_t>((cp >> 18) | 0xf0);
    *result++ = static_cast<uint8_t>(((cp >> 12) & 0x3f) | 0x80);
    *result++ = static_cast<uint8_t>(((cp >> 6) & 0x3f) | 0x80);
    *result++ = static_cast<uint8_t>((cp & 0x3f) | 0x80);
  }
  return result;
}

template<typename Iterator>
static uint32_t next_utf8(Iterator& it, Iterator end)
{
  if (it >= end)
    throw std::runtime_error("Not enough room");

  uint32_t cp = static_cast<uint8_t>(*it);
  size_t length = sequence_length(static_cast<uint8_t>(*it));

  switch (length)
  {
  case 1:
    ++it;
    break;
  case 2:
    ++it;
    if (it == end || !is_trail(*it))
      throw std::runtime_error("Invalid UTF-8");
    cp = ((cp & 0x1f) << 6) | ((*it) & 0x3f);
    ++it;
    if (cp < 0x80)  // Overlong check
      throw std::runtime_error("Overlong UTF-8 sequence");
    break;
  case 3:
    ++it;
    if (it == end || !is_trail(*it))
      throw std::runtime_error("Invalid UTF-8");
    cp = ((cp & 0x0f) << 12) | ((static_cast<uint8_t>(*it) & 0x3f) << 6);
    ++it;
    if (it == end || !is_trail(*it))
      throw std::runtime_error("Invalid UTF-8");
    cp |= (*it) & 0x3f;
    ++it;
    if (cp < 0x800 || (cp >= 0xd800 && cp <= 0xdfff))
      throw std::runtime_error("Invalid UTF-8");
    break;
  case 4:
    ++it;
    if (it == end || !is_trail(*it))
      throw std::runtime_error("Invalid UTF-8");
    cp = ((cp & 0x07) << 18) | ((static_cast<uint8_t>(*it) & 0x3f) << 12);
    ++it;
    if (it == end || !is_trail(*it))
      throw std::runtime_error("Invalid UTF-8");
    cp |= (static_cast<uint8_t>(*it) & 0x3f) << 6;
    ++it;
    if (it == end || !is_trail(*it))
      throw std::runtime_error("Invalid UTF-8");
    cp |= (*it) & 0x3f;
    ++it;
    if (cp < 0x10000 || cp > 0x10ffff)
      throw std::runtime_error("Invalid UTF-8");
    break;
  default:
    throw std::runtime_error("Invalid UTF-8");
  }

  if (!is_code_point_valid(cp))
    throw std::runtime_error("Invalid code point");

  return cp;
}

// UTF-16 to UTF-8 conversion (based on utf8cpp::utf16to8)
template<typename U16Iterator, typename U8Iterator>
static U8Iterator utf16to8(U16Iterator start, U16Iterator end, U8Iterator result)
{
  while (start != end)
  {
    uint32_t cp = static_cast<uint32_t>(mask16(*start++));

    // Handle surrogate pairs
    if (is_lead_surrogate(cp))
    {
      if (start != end)
      {
        const uint32_t trail_surrogate = static_cast<uint32_t>(mask16(*start++));
        if (is_trail_surrogate(trail_surrogate))
        {
          // Decode: cp = ((lead - 0xD800) << 10) + (trail - 0xDC00) + 0x10000
          cp = ((cp - LEAD_SURROGATE_MIN) << 10) + 
               (trail_surrogate - TRAIL_SURROGATE_MIN) + 0x10000;
        }
        else
          throw std::runtime_error("Invalid UTF-16: expected trail surrogate");
      }
      else
        throw std::runtime_error("Invalid UTF-16: lone lead surrogate");
    }
    // Lone trail surrogate
    else if (is_trail_surrogate(cp))
      throw std::runtime_error("Invalid UTF-16: lone trail surrogate");

    result = append_utf8(cp, result);
  }
  return result;
}

// UTF-8 to UTF-16 conversion (based on utf8cpp::utf8to16)
template<typename U8Iterator, typename U16Iterator>
static U16Iterator utf8to16(U8Iterator start, U8Iterator end, U16Iterator result)
{
  while (start < end)
  {
    const uint32_t cp = next_utf8(start, end);

    if (cp > 0xffff)
    {  // Make a surrogate pair
      *result++ = static_cast<uint16_t>((cp >> 10) + LEAD_OFFSET);
      *result++ = static_cast<uint16_t>((cp & 0x3ff) + TRAIL_SURROGATE_MIN);
    }
    else
      *result++ = static_cast<uint16_t>(cp);
  }
  return result;
}
}  // namespace utf_internal
*/


struct String
{
 public:
  typedef CharType*       Pointer;
  typedef const CharType* ConstPointer;

  Pointer  ptr{nullptr};
  SizeType len{0};
  SizeType cap{0};

  mutable SizeType RefCount{1};

  String() = default;

  String(const String& other)
  {
    if (other.len)
    {
      ptr = string_allocator.allocate<CharType>(other.len + 1);
      std::memcpy(ptr, other.ptr, other.len * sizeof(CharType));
      len = other.len;
      cap = other.cap;
      // terminate
      ptr[len] = BUFFER_END;
    }
  }

  String(const SizeType s)
  {
    if (s)
    {
      ptr = string_allocator.allocate<CharType>(s + 1);
      cap = s + 1;
      len = 0;
      // terminate
      ptr[0] = BUFFER_END;
    }
  }

  String(ConstPointer s)
  {
    // calculate len because the damn libc doesn't provide strlen for utf16
    ConstPointer p = s;
    while (*p++)
      ;
    len = p - s - 1;
    cap = len + 1;
    ptr = string_allocator.allocate<CharType>(cap);
    std::memcpy(ptr, s, len * sizeof(CharType));
    ptr[len] = BUFFER_END;
  }

  String(const SizeType s, const SizeType c)
  {
    len = s;
    cap = len + 1;
    ptr = string_allocator.allocate<CharType>(cap);

    Pointer p = ptr;
    while (p < ptr + len)
      *p++ = c;

    ptr[len] = BUFFER_END;
  }

  String operator=(const String& other)
  {
    if (this != &other && other.len)
    {
      ptr = string_allocator.allocate<CharType>(other.len + 1);
      std::memcpy(ptr, other.ptr, other.len * sizeof(CharType));
      len = other.len;
      cap = other.len + 1;
      // terminate
      ptr[len] = BUFFER_END;
    }
  }

  bool operator==(const String& other) const noexcept
  {
    // Different lengths
    if (len != other.len)
      return false;

    // Both empty
    if (len == 0)
      return true;

    // Self comparison
    if (ptr == other.ptr)
      return true;

    return std::memcmp(ptr, other.ptr, len * sizeof(CharType)) == 0;
  }


  void     increment() { RefCount++; }
  void     decrement() { RefCount--; }
  SizeType referenceCount() const { return RefCount; }
};

class StringRef
{
 private:
  typedef CharType*        Pointer;
  typedef const CharType*  ConstPointer;
  typedef StringRef&       Reference;
  typedef const StringRef& ConstReference;

  String* StringData_;

 public:
  // Default constructor
  StringRef() = default;

  explicit StringRef(const SizeType s);
  StringRef(ConstReference other);
  StringRef(StringRef&& other) noexcept;
  StringRef(ConstPointer lit);
  StringRef(const char* c_str) { *this = fromUtf8(c_str); }
  StringRef(const SizeType s, const CharType c);

  Reference operator=(ConstReference other);
  Reference operator=(StringRef&& other) noexcept;

  // Equality operator
  bool operator==(ConstReference other) const noexcept;
  bool operator!=(ConstReference other) const noexcept { return !(*this == other); }

  // Expand capacity
  void expand(const SizeType new_size);

  // Reserve capacity (doesn't change length, only capacity)
  void reserve(const SizeType new_capacity)
  {
    if (new_capacity > Capacity_)
      expand(new_capacity);
  }

  // Erase character at position
  void erase(const SizeType at);

  // Append another StringRef
  Reference operator+=(ConstReference other);
  Reference operator+=(CharType c);

  CharType  operator[](const SizeType i) const { return Ptr_[i]; }
  CharType& operator[](const SizeType i) { return Ptr_[i]; }

  // Safe access with bounds checking (always checked)
  MYLANG_NODISCARD CharType at(const SizeType i) const
  {
    if (i >= Len_)
      throw std::out_of_range("StringRef::at: index out of bounds");
    return Ptr_[i];
  }

  CharType& at(const SizeType i)
  {
    if (i >= Len_)
      throw std::out_of_range("StringRef::at: index out of bounds");
    return Ptr_[i];
  }

  // Swap with another StringRef (noexcept)
  void swap(StringRef& other) noexcept
  {
    std::swap(Ptr_, other.Ptr_);
    std::swap(Len_, other.Len_);
    std::swap(Capacity_, other.Capacity_);
  }

  // Concatenation operators

  // StringRef + StringRef
  friend StringRef operator+(ConstReference lhs, ConstReference rhs)
  {
    if (lhs.empty() && rhs.empty())
      return StringRef{};

    if (lhs.empty())
      return StringRef(rhs);

    if (rhs.empty())
      return StringRef(lhs);

    SizeType  new_len = lhs.len() + rhs.len();
    StringRef result(new_len);  // Allocate exact size needed

    std::memcpy(result.get(), lhs.cget(), lhs.len() * sizeof(CharType));
    std::memcpy(result.get() + lhs.len(), rhs.cget(), rhs.len() * sizeof(CharType));

    result.Len_          = new_len;
    result.Ptr_[new_len] = BUFFER_END;

    return result;
  }

  // StringRef + const char* (UTF-8 string)
  friend StringRef operator+(ConstReference lhs, const char* rhs)
  {
    if (!rhs || rhs[0] == BUFFER_END)
      return StringRef(lhs);

    StringRef rhs_str = fromUtf8(rhs);
    return lhs + rhs_str;
  }

  // const char* + StringRef
  friend StringRef operator+(const char* lhs, ConstReference rhs)
  {
    if (!lhs || lhs[0] == BUFFER_END)
      return StringRef(rhs);

    StringRef lhs_str = fromUtf8(lhs);
    return lhs_str + rhs;
  }

  // StringRef + CharType (single character)
  friend StringRef operator+(ConstReference lhs, CharType rhs)
  {
    StringRef result(lhs);
    result += rhs;
    return result;
  }

  // CharType + StringRef
  friend StringRef operator+(CharType lhs, ConstReference rhs)
  {
    StringRef result(1);
    result += lhs;
    result += rhs;
    return result;
  }

  // Accessors
  MYLANG_NODISCARD SizeType      len() const noexcept { return Len_; }
  MYLANG_NODISCARD SizeType      cap() const noexcept { return Capacity_; }
  MYLANG_NODISCARD String*       get() const noexcept { return StringData_; }
  MYLANG_NODISCARD const String* cget() const noexcept { return StringData_; }
  MYLANG_NODISCARD bool          empty() const noexcept { return Len_ == 0; }

  // Mutable accessors (use with caution)
  SizeType& len_ref() { return Len_; }

  // Output stream operator
  friend std::ostream& operator<<(std::ostream& os, ConstReference str)
  {
    if (str.empty())
      return os;
    os << str.toUtf8();
    return os;
  }

  // Clear the string (doesn't deallocate memory)
  void clear() noexcept
  {
    if (Ptr_ && Capacity_ > 0)
      Ptr_[0] = BUFFER_END;
    Len_ = 0;
  }

  // Resize capacity (preserves content)
  void resize(const SizeType s)
  {
    if (s > Capacity_)
      expand(s);
  }

  // Find a character
  MYLANG_NODISCARD bool find(const CharType c) const noexcept
  {
    // return std::strchr(Ptr_, c);
    for (SizeType i = 0; i < Len_; ++i)
      if (Ptr_[i] == c)
        return true;
    return false;
  }

  // Find position of a character (returns optional index)
  MYLANG_NODISCARD std::optional<SizeType> find_pos(const CharType c) const noexcept
  {
    for (SizeType i = 0; i < Len_; ++i)
      if (Ptr_[i] == c)
        return i;
    return std::nullopt;
  }

  // Truncate string to specified length
  StringRef& truncate(const SizeType s)
  {
    if (s < Len_ && Ptr_)
    {
      Ptr_[s] = BUFFER_END;
      Len_    = s;
    }
    return *this;
  }

  StringRef substr(std::optional<SizeType> start, std::optional<SizeType> end) const;
  StringRef substr(SizeType start) const { return substr(std::optional<SizeType>(start), std::nullopt); }

  // Convert to double - improved error handling
  double toDouble(SizeType* pos = nullptr) const;

  MYLANG_NODISCARD std::string      toUtf8() const;
  MYLANG_NODISCARD static StringRef fromUtf8(const std::string& utf8_str);

  // Convenience overload for C strings
  MYLANG_NODISCARD static StringRef fromUtf8(const char* utf8_cstr)
  {
    if (!utf8_cstr)
      return StringRef{};
    return fromUtf8(std::string(utf8_cstr));
  }
};  // StringRef

// Hash functor for StringRef
struct StringRefHash
{
  std::size_t operator()(const StringRef& str) const noexcept
  {
    if (str.empty() || str.len() == 0)
      return 0;

    // FNV-1a hash algorithm (good distribution, fast)
    std::size_t       hash  = 14695981039346656037ULL;  // FNV offset basis
    const std::size_t prime = 1099511628211ULL;         // FNV prime

    const CharType* data = str.cget();
    const SizeType  len  = str.len();

    // Hash the raw bytes of char16_t data
    const Byte*       bytes      = reinterpret_cast<const Byte*>(data);
    const std::size_t byte_count = len * sizeof(CharType);

    for (std::size_t i = 0; i < byte_count; ++i)
    {
      hash ^= static_cast<std::size_t>(bytes[i]);
      hash *= prime;
    }

    return hash;
  }
};

// Equal comparison functor for consistency
struct StringRefEqual
{
  bool operator()(const StringRef& lhs, const StringRef& rhs) const noexcept { return lhs == rhs; }
};

}  // namespace mylang

// Specialize std::hash for StringRef
namespace std {

template<>
struct hash<mylang::StringRef>
{
  std::size_t operator()(const mylang::StringRef& str) const noexcept
  {
    if (str.empty() || str.len() == 0)
      return 0;

    // FNV-1a hash
    std::size_t       hash_value = 14695981039346656037ULL;
    const std::size_t prime      = 1099511628211ULL;

    const mylang::CharType* data = str.cget();
    const mylang::SizeType  len  = str.len();

    const unsigned char* bytes      = reinterpret_cast<const unsigned char*>(data);
    const std::size_t    byte_count = len * sizeof(mylang::CharType);

    for (std::size_t i = 0; i < byte_count; ++i)
    {
      hash_value ^= static_cast<std::size_t>(bytes[i]);
      hash_value *= prime;
    }

    return hash_value;
  }
};

}  // namespace std
