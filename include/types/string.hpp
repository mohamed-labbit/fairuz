#pragma once

#include "../macros.hpp"
#include "string_allocator.hpp"
#include "string_data.hpp"


namespace mylang {

class StringRef
{
 private:
  typedef CharType*        Pointer;
  typedef const CharType*  ConstPointer;
  typedef StringRef&       Reference;
  typedef const StringRef& ConstReference;

  String* StringData_;

  // Helper to create empty string
  static String* createEmpty()
  {
    String* s = string_allocator.allocateObject<String>();
    return s;
  }

 public:
  // FIXED: Initialize StringData_ properly
  StringRef() :
      StringData_(createEmpty())
  {
  }

  explicit StringRef(const SizeType s) :
      StringData_(string_allocator.allocateObject<String>(s))
  {
  }

  StringRef(ConstReference other) :
      StringData_(other.get())
  {
    if (StringData_)
      StringData_->increment();
  }

  StringRef(ConstPointer lit) :
      StringData_(string_allocator.allocateObject<String>(lit))
  {
  }

  StringRef(const char* c_str) :
      StringData_(nullptr)
  {
    if (!c_str || !c_str[0])
      StringData_ = createEmpty();
    else
    {
      // Don't use operator= on uninitialized object!
      StringRef temp = fromUtf8(c_str);
      StringData_    = temp.StringData_;
      if (StringData_)
        StringData_->increment();
    }
  }

  StringRef(const SizeType s, const CharType c) :
      StringData_(string_allocator.allocateObject<String>(s, c))
  {
  }

  explicit StringRef(String* data) :
      StringData_(data ? data : createEmpty())
  {
  }

  ~StringRef()
  {
    if (StringData_)
    {
      /// NOTE: this will only be deallocated if it's the last pointer in the arena
      /// we should probably make a better allocator for that ...
      StringData_->decrement();
      if (StringData_->referenceCount() == 0)
      {
        StringData_->~String();  // deallocate if possible the string array
        string_allocator.deallocateObject<String>(StringData_);
      }
    }
  }

  // FIXED: Properly decrement old reference before assignment
  Reference operator=(ConstReference other)
  {
    if (this == &other)
      return *this;

    // Decrement old reference
    if (StringData_)
      StringData_->decrement();

    // Assign new reference
    StringData_ = other.get();
    if (StringData_)
      StringData_->increment();

    return *this;
  }

  // Equality operator
  bool operator==(ConstReference other) const noexcept
  {
    if (!StringData_ || !other.StringData_)
      return StringData_ == other.StringData_;
    return *StringData_ == *other.get();
  }

  bool operator!=(ConstReference other) const noexcept { return !(*this == other); }

  // Expand capacity
  void expand(const SizeType new_size);

  // Reserve capacity (doesn't change length, only capacity)
  void reserve(const SizeType new_capacity);

  // Erase character at position
  void erase(const SizeType at);

  // Append another StringRef
  Reference operator+=(ConstReference other);

  Reference operator+=(CharType c);

  // FIXED: Add null checks
  CharType operator[](const SizeType i) const;

  CharType& operator[](const SizeType i);

  // Safe access with bounds checking (always checked)
  MYLANG_NODISCARD CharType at(const SizeType i) const;

  CharType& at(const SizeType i);
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

    // FIXED: Correct length calculation
    SizeType actual_len = lhs.len() + rhs.len();
    String*  result     = string_allocator.allocateObject<String>(actual_len);

    ::memcpy(result->ptr(), lhs.data(), lhs.len() * sizeof(CharType));
    ::memcpy(result->ptr() + lhs.len(), rhs.data(), rhs.len() * sizeof(CharType));

    result->setLen(actual_len);
    result->terminate();

    return StringRef(result);
  }

  // StringRef + const char* (UTF-8 string)
  friend StringRef operator+(ConstReference lhs, const char* rhs)
  {
    if (!rhs || !rhs[0])
      return StringRef(lhs);

    StringRef rhs_str = fromUtf8(rhs);
    return lhs + rhs_str;
  }

  // const char* + StringRef
  friend StringRef operator+(const char* lhs, ConstReference rhs)
  {
    if (!lhs || !lhs[0])
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
    StringRef result(1, BUFFER_END);
    result.clear();
    result += lhs;
    result += rhs;
    return result;
  }

  // Accessors
  MYLANG_NODISCARD SizeType len() const noexcept;
  MYLANG_NODISCARD SizeType cap() const noexcept;
  MYLANG_NODISCARD String*  get() const noexcept;
  MYLANG_NODISCARD bool     empty() const noexcept;
  ConstPointer              data() const noexcept;

  Pointer data() noexcept;

  // Output stream operator
  friend std::ostream& operator<<(std::ostream& os, ConstReference str)
  {
    if (str.empty())
      return os;
    os << str.toUtf8();
    return os;
  }

  // Clear the string (doesn't deallocate memory)
  void clear() noexcept;

  // Resize capacity (preserves content)
  void resize(const SizeType s);

  // Find a character
  MYLANG_NODISCARD bool find(const CharType c) const noexcept;
  MYLANG_NODISCARD bool find(const StringRef& s) const noexcept;

  // Find position of a character (returns optional index)
  MYLANG_NODISCARD std::optional<SizeType> find_pos(const CharType c) const noexcept;

  // Truncate string to specified length
  StringRef& truncate(const SizeType s);

  StringRef substr(std::optional<SizeType> start, std::optional<SizeType> end) const;

  StringRef substr(SizeType start) const;

  // Convert to double - improved error handling
  double toDouble(SizeType* pos = nullptr) const;

  MYLANG_NODISCARD std::string toUtf8() const;

  MYLANG_NODISCARD static StringRef fromUtf8(const std::string& utf8_str);

  // Convenience overload for C strings
  MYLANG_NODISCARD static StringRef fromUtf8(const char* utf8_cstr);

  void COW();
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

    const CharType* data = str.data();
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

    const mylang::CharType* data = str.data();
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