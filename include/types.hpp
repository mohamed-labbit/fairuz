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

class StringAllocator
{
 private:
  runtime::allocator::ArenaAllocator Allocator_;

 public:
  StringAllocator() :
      Allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::LINEAR))
  {
  }

  template<typename T, typename... Args>
  T* allocate(SizeType count, Args&&... args)
  {
    void* mem = Allocator_.allocate(count * sizeof(T));
    return new (mem) T(std::forward<Args>(args)...);
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

class StringRef
{
 private:
  typedef CharType*        Pointer;
  typedef const CharType*  ConstPointer;
  typedef StringRef&       Reference;
  typedef const StringRef& ConstReference;

  CharType* Ptr_{nullptr};
  SizeType  Len_{0};       // length of the string (not including null terminator)
  SizeType  Capacity_{0};  // allocated memory (including space for null terminator)

  // Helper to check bounds in debug mode
  void checkBounds(SizeType index) const
  {
#ifndef NDEBUG
    if (index >= Len_)
      throw std::out_of_range("StringRef: index out of bounds");
#endif
  }

 public:
  // Default constructor
  StringRef() = default;

  // Constructor with size (allocates capacity for s characters + null terminator)
  explicit StringRef(const SizeType s)
  {
    if (s == 0)
      return;

    Ptr_ = string_allocator.allocate<CharType>(s + 1);  // +1 for null terminator
    if (Ptr_ == nullptr)
      throw std::bad_alloc();

    Capacity_ = s + 1;
    Len_      = 0;
    Ptr_[0]   = BUFFER_END;  // Ensure null termination
  }

  // Copy constructor - FIXED: copy correct amount and set capacity
  StringRef(ConstReference other)
  {
    if (other.len() == 0)
      return;

    // Allocate space for string + null terminator
    Ptr_ = string_allocator.allocate<CharType>(other.len() + 1);
    if (Ptr_ == nullptr)
      throw std::bad_alloc();

    // using std::memcpy just ruined the buffer somehow
    for (SizeType i = 0; i < other.len(); i++)
      Ptr_[i] = other[i];

    Len_      = other.len();
    Capacity_ = other.len() + 1;

    // Ensure null termination
    Ptr_[Len_] = BUFFER_END;
  }

  // Move constructor
  StringRef(StringRef&& other) noexcept :
      Ptr_(other.Ptr_),
      Len_(other.Len_),
      Capacity_(other.Capacity_)
  {
    other.Ptr_      = nullptr;
    other.Len_      = 0;
    other.Capacity_ = 0;
  }

  // Constructor from C-style CharType string
  StringRef(ConstPointer lit)
  {
    if (lit == nullptr)
      return;

    // Calculate length of the null-terminated string
    SizeType length = 0;
    while (lit[length] != BUFFER_END)
      ++length;

    // Handle empty string
    if (length == 0)
      return;

    // Allocate with space for null terminator
    Ptr_ = string_allocator.allocate<CharType>(length + 1);
    if (Ptr_ == nullptr)
      throw std::bad_alloc();

    // Copy the string data
    std::memcpy(Ptr_, lit, length * sizeof(CharType));
    Len_       = length;
    Capacity_  = length + 1;
    Ptr_[Len_] = BUFFER_END;
  }

  /*
  Reference operator=(ConstReference other)
  {
    if (this == &other)
    return *this;  // Self-assignment guard
    
    if (other.len() == 0)
    {
      // Clear current string
      // if (Ptr_ != nullptr)
        // string_allocator.deallocate<CharType>(Ptr_, Capacity_);
        
        Ptr_      = nullptr;
        Len_      = 0;
        Capacity_ = 0;
        return *this;
      }
      
    // Deallocate old memory if it exists
    // if (Ptr_ != nullptr)
    // string_allocator.deallocate<CharType>(Ptr_, Capacity_);
    
    // Allocate new memory
    Ptr_ = string_allocator.allocate<CharType>(other.len() + 1);
    if (Ptr_ == nullptr)
    throw std::bad_alloc();
    
    // std::memcpy(Ptr_, other.get(), other.len() * sizeof(CharType));
    
    std::cout << "s1 from inside before copy : " << other << std::endl;
    for (SizeType i = 0; i < other.len(); i++)
      Ptr_[i] = other[i];
    
    std::cout << "s1 from inside after copy : " << other << std::endl;
    Len_       = other.len();
    Capacity_  = other.len() + 1;
    Ptr_[Len_] = BUFFER_END;
    
    return *this;
  }
  */

  Reference operator=(ConstReference other)
  {
    if (this == &other)
      return *this;  // Self-assignment guard

    if (other.len() == 0)
    {
      // Clear current string
      // if (Ptr_ != nullptr)
      // string_allocator.deallocate<CharType>(Ptr_, Capacity_);

      Ptr_      = nullptr;
      Len_      = 0;
      Capacity_ = 0;
      return *this;
    }

    // FIXED: Allocate NEW memory FIRST, before deallocating old
    Pointer new_ptr = string_allocator.allocate<CharType>(other.len() + 1);
    if (new_ptr == nullptr)
      throw std::bad_alloc();

    // std::cout << "other pointer : " << std::hex << std::showbase << static_cast<const void*>(other.cget()) << std::endl;
    // std::cout << "new pointer   : " << std::hex << std::showbase << static_cast<const void*>(new_ptr) << std::endl;

    // Copy data to NEW memory (other is still intact)
    // std::memcpy(new_ptr, other.cget(), other.len() * sizeof(CharType));
    for (SizeType i = 0; i < other.len(); i++)
      new_ptr[i] = other[i];

    new_ptr[other.len()] = BUFFER_END;

    // NOW deallocate old memory (after we've copied other)
    // if (Ptr_ != nullptr)
    // string_allocator.deallocate<CharType>(Ptr_, Capacity_);

    // Update pointers
    Ptr_      = new_ptr;
    Len_      = other.len();
    Capacity_ = other.len() + 1;

    return *this;
  }

  // Move assignment operator
  Reference operator=(StringRef&& other) noexcept
  {
    if (this == &other)
      return *this;

    // Deallocate our current memory
    // if (Ptr_ != nullptr)
    // string_allocator.deallocate<CharType>(Ptr_, Capacity_);

    // Take ownership of other's resources
    Ptr_      = other.Ptr_;
    Len_      = other.Len_;
    Capacity_ = other.Capacity_;

    // Leave other in valid empty state
    other.Ptr_      = nullptr;
    other.Len_      = 0;
    other.Capacity_ = 0;

    return *this;
  }

  // Constructor from UTF-8 string
  StringRef(const char* utf8_str) { *this = fromUtf8(utf8_str); }

  // Assignment from UTF-8 string
  Reference operator=(const char* utf8_str)
  {
    *this = fromUtf8(utf8_str);
    return *this;
  }

  // Destructor - deallocates memory
  ~StringRef()
  {
    if (Ptr_ != nullptr)
    {
      // string_allocator.deallocate<CharType>(Ptr_, Capacity_);
      Ptr_      = nullptr;
      Len_      = 0;
      Capacity_ = 0;
    }
  }

  // Equality operator
  bool operator==(ConstReference other) const noexcept
  {
    // Different lengths
    if (Len_ != other.len())
      return false;

    // Both empty
    if (Len_ == 0)
      return true;

    // Self comparison
    if (Ptr_ == other.cget())
      return true;

    // Deep comparison
    // return std::memcmp(Ptr_, other.cget(), Len_ * sizeof(CharType)) == 0;
    for (SizeType i = 0; i < Len_; ++i)
      if (Ptr_[i] != other[i])
        return false;

    return true;
  }

  bool operator!=(ConstReference other) const noexcept { return !(*this == other); }

  // Expand capacity - FIXED: properly track old capacity and check overflow
  void expand(const SizeType new_size)
  {
    if (new_size <= Capacity_)
      return;  // Already have enough capacity

    SizeType old_capacity = Capacity_;
    Pointer  old_ptr      = Ptr_;
    SizeType new_capacity;

    // FIXED: Check for overflow before multiplication
    // If new_size > (SIZE_MAX / 3) * 2, then new_size * 1.5 would overflow
    constexpr SizeType overflow_threshold = (std::numeric_limits<SizeType>::max() / 3) * 2;

    if (new_size > overflow_threshold)
      // Near overflow, use exact size
      new_capacity = new_size + 1;
    else
      // Try to allocate 1.5x the requested size + 1 for null terminator
      new_capacity = static_cast<SizeType>(new_size * 1.5 + 1);

    Ptr_ = string_allocator.allocate<CharType>(new_capacity);
    if (Ptr_ == nullptr)
    {
      // Fallback to exact size
      new_capacity = new_size + 1;
      Ptr_         = string_allocator.allocate<CharType>(new_capacity);
      if (Ptr_ == nullptr)
        throw std::bad_alloc();
    }

    // Copy old data if it exists
    if (old_ptr != nullptr)
    {
      std::memcpy(Ptr_, old_ptr, Len_ * sizeof(CharType));
      // string_allocator.deallocate<CharType>(old_ptr, old_capacity);
    }

    Capacity_ = new_capacity;

    // Ensure null termination
    if (Len_ < Capacity_)
      Ptr_[Len_] = BUFFER_END;
  }

  // Reserve capacity (doesn't change length, only capacity)
  void reserve(const SizeType new_capacity)
  {
    if (new_capacity > Capacity_)
      expand(new_capacity);
  }

  // Erase character at position
  void erase(const SizeType at)
  {
    if (at >= Len_ || Ptr_ == nullptr)
      return;  // Out of bounds or empty string

    // Shift characters left
    for (SizeType i = at; i < Len_ - 1; i++)
      Ptr_[i] = Ptr_[i + 1];

    Len_--;
    Ptr_[Len_] = BUFFER_END;
  }

  // Append another StringRef
  Reference operator+=(ConstReference other)
  {
    if (other.len() == 0)
      return *this;

    SizeType new_len = Len_ + other.len();

    if (new_len + 1 > Capacity_)  // +1 for null terminator
      expand(new_len + 1);

    std::memcpy(Ptr_ + Len_, other.cget(), other.len() * sizeof(CharType));
    Len_       = new_len;
    Ptr_[Len_] = BUFFER_END;

    return *this;
  }

  // Append a single character - FIXED: correct capacity check
  Reference operator+=(CharType c)
  {
    // FIXED: Need Len_ + 2 to have space for new char + null terminator
    if (Len_ + 2 > Capacity_)
      expand(Len_ + 2);

    Ptr_[Len_]     = c;
    Ptr_[Len_ + 1] = BUFFER_END;
    Len_++;

    return *this;
  }

  // Index operator (const) - FIXED: added bounds checking
  CharType operator[](const SizeType i) const
  {
    // checkBounds(i);
    return Ptr_[i];
  }

  // Index operator (non-const) - FIXED: added bounds checking
  CharType& operator[](const SizeType i)
  {
    // checkBounds(i);
    return Ptr_[i];
  }

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

    // std::memcpy(result.get(), lhs.cget(), lhs.len() * sizeof(CharType));
    // std::memcpy(result.get() + lhs.len(), rhs.cget(), rhs.len() * sizeof(CharType));

    SizeType i = 0;
    for (; i < lhs.len(); i++)
      result[i] = lhs[i];

    for (SizeType k = 0; k < rhs.len(); k++)
      result[i++] = rhs[k];

    result.Len_          = new_len;
    result.Ptr_[new_len] = BUFFER_END;

    return result;
  }

  // StringRef + const char* (UTF-8 string)
  friend StringRef operator+(ConstReference lhs, const char* rhs)
  {
    if (rhs == nullptr || rhs[0] == BUFFER_END)
      return StringRef(lhs);

    StringRef rhs_str = fromUtf8(rhs);
    return lhs + rhs_str;
  }

  // const char* + StringRef
  friend StringRef operator+(const char* lhs, ConstReference rhs)
  {
    if (lhs == nullptr || lhs[0] == BUFFER_END)
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
  MYLANG_NODISCARD SizeType     len() const noexcept { return Len_; }
  MYLANG_NODISCARD SizeType     cap() const noexcept { return Capacity_; }
  MYLANG_NODISCARD Pointer      get() noexcept { return Ptr_; }
  MYLANG_NODISCARD ConstPointer cget() const noexcept { return Ptr_; }
  MYLANG_NODISCARD bool         empty() const noexcept { return Len_ == 0; }

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
    if (Ptr_ != nullptr && Capacity_ > 0)
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
    if (s < Len_ && Ptr_ != nullptr)
    {
      Ptr_[s] = BUFFER_END;
      Len_    = s;
    }
    return *this;
  }

  // Substring - FIXED: optimized to use memcpy instead of character-by-character
  // Returns substring from start (inclusive) to end (inclusive)
  StringRef substr(std::optional<SizeType> start, std::optional<SizeType> end) const
  {
    if (empty())
      return StringRef{};

    SizeType start_val = start.value_or(0);
    SizeType end_val   = end.value_or(Len_ - 1);

    // Bounds checking
    if (start_val >= Len_)
      throw std::out_of_range("StringRef::substr: start index out of range");

    if (end_val >= Len_)
      end_val = Len_ - 1;  // Clamp to valid range

    if (end_val < start_val)
      throw std::invalid_argument("StringRef::substr: end must be >= start");

    // Calculate length (end is inclusive)
    SizeType ret_len = end_val - start_val + 1;

    // FIXED: Optimized - use direct memory copy
    StringRef ret(ret_len);
    std::memcpy(ret.Ptr_, Ptr_ + start_val, ret_len * sizeof(CharType));
    ret.Len_          = ret_len;
    ret.Ptr_[ret_len] = BUFFER_END;

    return ret;
  }

  // Overload for convenience: substr(start) returns from start to end
  StringRef substr(SizeType start) const { return substr(std::optional<SizeType>(start), std::nullopt); }

  // Convert to double - improved error handling
  double toDouble(SizeType* pos = nullptr) const
  {
    if (empty() || Len_ == 0)
      throw std::invalid_argument("StringRef::toDouble: empty string");

    // Convert UTF-16 to UTF-8 for standard library parsing
    std::string utf8_str = toUtf8();

    // Use std::stod on the UTF-8 string
    std::size_t utf8_pos = 0;
    double      result;

    try
    {
      result = std::stod(utf8_str, &utf8_pos);
    } catch (const std::invalid_argument&)
    {
      throw std::invalid_argument("StringRef::toDouble: invalid number format");
    } catch (const std::out_of_range&)
    {
      throw std::out_of_range("StringRef::toDouble: number out of range");
    }

    // Convert UTF-8 position back to UTF-16 position if requested
    // This is approximate and may not be exact for multi-byte characters
    if (pos != nullptr)
      *pos = static_cast<SizeType>(utf8_pos);

    return result;
  }

  // Convert UTF-16 to UTF-8 using embedded utf8cpp logic
  MYLANG_NODISCARD std::string toUtf8() const
  {
    if (empty() || Len_ == 0)
      return std::string{};

    std::string result;

    try
    {
      // Use embedded utf16to8 conversion
      utf8::utf16to8(Ptr_, Ptr_ + Len_, std::back_inserter(result));
    } catch (const std::runtime_error& e)
    {
      throw std::runtime_error(std::string("UTF-16 to UTF-8 conversion failed: ") + e.what());
    }

    return result;
  }

  // Convert UTF-8 to UTF-16 using embedded utf8cpp logic - FIXED: optimized
  MYLANG_NODISCARD static StringRef fromUtf8(const std::string& utf8_str)
  {
    if (utf8_str.empty())
      return StringRef{};

    // First, convert to UTF-16
    // std::vector<CharType> utf16_buffer;

    std::u16string utf16_str = u"";

    try
    {
      // utf8::utf8to16(utf8_str.begin(), utf8_str.end(), std::back_inserter(utf16_buffer));
      utf16_str = utf8::utf8to16(utf8_str);
    } catch (const std::runtime_error& e)
    {
      throw std::runtime_error(std::string("UTF-8 to UTF-16 conversion failed: ") + e.what());
    }

    // Create StringRef from the converted data
    // if (utf16_str.empty())
    //   return StringRef{};

    // FIXED: Optimized - use direct memory copy instead of character-by-character
    // StringRef result(utf16_str.length());
    // std::memcpy(result.Ptr_, utf16_str.data(), utf16_str.size() * sizeof(CharType));

    // result.Len_                     = utf16_str.length();
    // result.Ptr_[utf16_str.length()] = BUFFER_END;

    return StringRef(utf16_str.data());
  }

  // Convenience overload for C strings
  MYLANG_NODISCARD static StringRef fromUtf8(const char* utf8_cstr)
  {
    if (utf8_cstr == nullptr)
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
