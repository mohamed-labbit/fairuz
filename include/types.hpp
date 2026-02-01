#pragma once

#include "../utfcpp/source/utf8.h"
#include "macros.hpp"
#include "runtime/allocator/arena.hpp"

#include <atomic>
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
      Allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::EXPONENTIAL))
  {
  }

  void* allocateBytes(const SizeType size)
  {
    void* mem = Allocator_.allocate(size);
    if (!mem)
      throw std::bad_alloc();
    return mem;
  }

  template<typename T>
  T* allocateArray(const SizeType count)
  {
    return static_cast<T*>(allocateBytes(count * sizeof(T)));
  }

  template<typename T>
  void deallocateArray(T* p, const SizeType count)
  {
    Allocator_.deallocate((void*) p, count * sizeof(T));
  }

  template<typename T, typename... Args>
  MYLANG_NODISCARD T* allocateObject(Args&&... args)
  {
    static_assert(std::is_constructible_v<T, Args...>, "T must be constructible with Args...");
    void* mem = allocateBytes(sizeof(T));
    return ::new (mem) T(std::forward<Args>(args)...);
  }

  template<typename T>
  void deallocateObject(T* obj)
  {
    Allocator_.deallocate((void*) obj, sizeof(T));
  }

  std::string toString(bool verbose) const { return Allocator_.toString(verbose); }
};

inline StringAllocator string_allocator;

class StringRef;

class String
{
  friend class StringRef;

  using Pointer      = CharType*;
  using ConstPointer = const CharType*;

 private:
  // static constexpr SizeType SSO_FLAG = SizeType(1) << (sizeof(SizeType) * 8 - 1);

  union Storage {
    struct
    {
      Pointer  ptr;
      SizeType cap;
    } heap;

    CharType sso[SSO_SIZE];

    Storage() :
        heap{nullptr, 0}
    {
    }
  } storage_;

  bool             is_heap;
  SizeType         len_{0};  // includes SSO_FLAG
  mutable SizeType RefCount{1};

 public:
  String() :
      is_heap{false}
  {
    len_            = 0;
    storage_.sso[0] = BUFFER_END;
  }

  ~String()
  {
    if (isHeap())
      string_allocator.deallocateArray<CharType>(storage_.heap.ptr, storage_.heap.cap);
  }

  // ---- helpers ----

  SizeType length() const noexcept { return len_; }

  bool isHeap() const noexcept { return is_heap; }

  bool isInlined() const noexcept { return !isHeap(); }

  Pointer ptr() noexcept { return isHeap() ? storage_.heap.ptr : storage_.sso; }

  ConstPointer ptr() const noexcept { return isHeap() ? storage_.heap.ptr : storage_.sso; }

  SizeType cap() const noexcept { return isHeap() ? storage_.heap.cap - 1 /*subtract the nul terminator*/ : SSO_SIZE - 1; }

  void setLen(const SizeType n)
  {
    if (!isHeap() && n > cap())
      throw std::invalid_argument("String::setLen(unsigned long n = " + std::to_string(n) + ") : invalid length");
    len_ = n;
  }

  void terminate() noexcept { ptr()[length()] = BUFFER_END; }

  // ---- constructors ----

  String(const String& other) :
      is_heap(other.isHeap())
  {
    setLen(other.length());

    if (other.isInlined())
      ::memcpy(storage_.sso, other.storage_.sso, (length() + 1) * sizeof(CharType));
    else
    {
      storage_.heap.cap = other.storage_.heap.cap;
      storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);

      ::memcpy(storage_.heap.ptr, other.storage_.heap.ptr, (length() + 1) * sizeof(CharType));
    }
  }

  String(const SizeType s)
  {
    if (s < SSO_SIZE)
    {
      is_heap = false;
      setLen(0);
    }
    else
    {
      is_heap = true;
      setLen(0);
      storage_.heap.cap = s + 1;
      storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);
    }
    terminate();
  }

  String(const SizeType s, const CharType c)
  {
    if (s < SSO_SIZE)
    {
      is_heap = false;
      ::memset(storage_.sso, c, s);
      setLen(s);
    }
    else
    {
      is_heap           = true;
      storage_.heap.cap = s + 1;
      storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);
      ::memset(storage_.heap.ptr, c, s);
      setLen(s);
    }

    terminate();
  }

  String(ConstPointer s, SizeType n)
  {
    if (!s || n == 0)
    {
      is_heap = false;
      setLen(0);
      terminate();
      return;
    }

    if (n < SSO_SIZE)
    {
      is_heap = false;
      setLen(n);
      ::memcpy(storage_.sso, s, n * sizeof(CharType));
    }
    else
    {
      is_heap           = true;
      storage_.heap.cap = n + 1;
      storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);
      ::memcpy(storage_.heap.ptr, s, n * sizeof(CharType));
    }

    terminate();
  }

  String(ConstPointer s)
  {
    if (!s)
    {
      len_            = 0;
      storage_.sso[0] = BUFFER_END;
      return;
    }

    ConstPointer p = s;
    while (*p++)
      ;

    SizeType n = p - s - 1;

    if (n < SSO_SIZE)
    {
      is_heap = false;
      setLen(n);
      ::memcpy(storage_.sso, s, n * sizeof(CharType));
    }
    else
    {
      is_heap = true;
      setLen(n);
      storage_.heap.cap = n + 1;
      storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);
      ::memcpy(storage_.heap.ptr, s, (n + 1) * sizeof(CharType));
    }

    terminate();
  }


  // ---- ops ----

  bool operator==(const String& other) const noexcept
  {
    if (length() != other.length())
      return false;

    if (length() == 0)
      return true;

    return ::memcmp(ptr(), other.ptr(), length() * sizeof(CharType)) == 0;
  }

  CharType  operator[](const SizeType i) const noexcept { return ptr()[i]; }
  CharType& operator[](const SizeType i) noexcept { return ptr()[i]; }

  void     increment() const noexcept { ++RefCount; }
  void     decrement() const noexcept { --RefCount; }
  SizeType referenceCount() const noexcept { return RefCount; }
};

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
  void expand(const SizeType new_size)
  {
    COW();

    if (new_size <= cap())
      return;

    ConstPointer old_ptr = data();
    SizeType     old_len = len();
    SizeType     new_capacity;

    constexpr SizeType overflow_threshold = (std::numeric_limits<SizeType>::max() / 3) * 2;

    if (new_size > overflow_threshold)
      new_capacity = new_size + 1;
    else
      new_capacity = static_cast<SizeType>(new_size * 1.5 + 1);

    Pointer new_ptr = string_allocator.allocateArray<CharType>(new_capacity);

    // Copy old data
    if (old_ptr && old_len > 0)
      ::memcpy(new_ptr, old_ptr, old_len * sizeof(CharType));

    if (StringData_->isHeap() && StringData_->storage_.heap.ptr)
      string_allocator.deallocateArray<CharType>(StringData_->storage_.heap.ptr, StringData_->storage_.heap.cap);

    // Now set heap storage
    StringData_->storage_.heap.ptr = new_ptr;
    StringData_->storage_.heap.cap = new_capacity;
    StringData_->is_heap           = true;

    // Ensure null termination
    if (old_len < new_capacity)
      StringData_->terminate();
  }


  // Reserve capacity (doesn't change length, only capacity)
  void reserve(const SizeType new_capacity)
  {
    if (new_capacity > cap())
    {
      COW();
      expand(new_capacity);
    }
  }

  // Erase character at position
  void erase(const SizeType at)
  {
    COW();
    if (at >= len() || !data())
      return;  // Out of bounds or empty string

    ::memmove(StringData_->ptr() + at, data() + at + 1, (len() - at - 1) * sizeof(CharType));

    StringData_->setLen(len() - 1);
    StringData_->terminate();
  }

  // Append another StringRef
  Reference operator+=(ConstReference other)
  {
    COW();

    if (other.len() == 0)
      return *this;

    SizeType new_len = len() + other.len();

    if (new_len > cap())
      expand(new_len);  // Handles SSO→heap transition correctly

    ::memcpy(StringData_->ptr() + len(), other.data(), other.len() * sizeof(CharType));

    // REMOVED: StringData_->len_ |= String::SSO_FLAG;
    // The expand() method already sets this flag when transitioning to heap
    // The setLen() method preserves it correctly

    StringData_->setLen(new_len);
    StringData_->terminate();

    return *this;
  }
  Reference operator+=(CharType c)
  {
    COW();

    if (len() + 2 > cap())
      expand(len() + 1);  // expand does account for nul

    (*StringData_)[len()] = c;
    StringData_->setLen(len() + 1);
    StringData_->terminate();

    return *this;
  }

  // FIXED: Add null checks
  CharType operator[](const SizeType i) const
  {
    if (!StringData_ || !StringData_->ptr())
      throw std::runtime_error("StringRef::operator[]: null string data");
    return (*StringData_)[i];
  }

  CharType& operator[](const SizeType i)
  {
    COW();
    if (!StringData_ || !StringData_->ptr())
      throw std::runtime_error("StringRef::operator[]: null string data");
    return (*StringData_)[i];
  }

  // Safe access with bounds checking (always checked)
  MYLANG_NODISCARD CharType at(const SizeType i) const
  {
    if (!StringData_ || !StringData_->ptr())
      throw std::runtime_error("StringRef::at: null string data");
    if (i >= len())
      throw std::out_of_range("StringRef::at: index out of bounds");
    return (*StringData_)[i];
  }

  CharType& at(const SizeType i)
  {
    COW();
    if (!StringData_ || !StringData_->ptr())
      throw std::runtime_error("StringRef::at: null string data");
    if (i >= len())
      throw std::out_of_range("StringRef::at: index out of bounds");
    return (*StringData_)[i];
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
  MYLANG_NODISCARD SizeType len() const noexcept { return StringData_ ? StringData_->length() : 0; }
  MYLANG_NODISCARD SizeType cap() const noexcept { return StringData_ ? StringData_->cap() : 0; }
  MYLANG_NODISCARD String*  get() const noexcept { return StringData_; }
  MYLANG_NODISCARD bool     empty() const noexcept { return StringData_ ? StringData_->length() == 0 : true; }
  ConstPointer              data() const noexcept { return StringData_ ? StringData_->ptr() : nullptr; }

  Pointer data() noexcept
  {
    COW();
    return StringData_ ? StringData_->ptr() : nullptr;
  }

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
    COW();
    if (data() && cap() > 0)
      (*StringData_)[0] = BUFFER_END;
    StringData_->setLen(0);
  }

  // Resize capacity (preserves content)
  void resize(const SizeType s)
  {
    COW();
    if (s > cap())
      expand(s);
  }

  // Find a character
  MYLANG_NODISCARD bool find(const CharType c) const noexcept
  {
    if (!StringData_ || !StringData_->ptr())
      return false;

    Pointer p = StringData_->ptr();
    while (*p && *p != c)
      ++p;
    return *p == c;
  }

  // Find position of a character (returns optional index)
  MYLANG_NODISCARD std::optional<SizeType> find_pos(const CharType c) const noexcept
  {
    if (!StringData_ || !StringData_->ptr())
      return std::nullopt;

    Pointer p = StringData_->ptr();
    while (*p && *p != c)
      ++p;
    return *p == c ? std::optional<SizeType>(p - StringData_->ptr()) : std::nullopt;
  }

  // Truncate string to specified length
  StringRef& truncate(const SizeType s)
  {
    COW();
    if (s < len() && StringData_->ptr())
    {
      StringData_->setLen(s);
      StringData_->terminate();
    }
    return *this;
  }

  StringRef substr(std::optional<SizeType> start, std::optional<SizeType> end) const
  {
    if (empty())
      return StringRef{};

    SizeType start_val = start.value_or(0);
    SizeType end_val   = end.value_or(len() - 1);

    if (start_val >= len())
      throw std::out_of_range("StringRef::substr: start index out of range");

    if (end_val >= len())
      end_val = len() - 1;

    if (end_val < start_val)
      throw std::invalid_argument("StringRef::substr: end must be >= start");

    SizeType ret_len  = end_val - start_val + 1;
    String*  ret_data = string_allocator.allocateObject<String>();

    if (ret_len < SSO_SIZE)
    {
      ret_data->is_heap = false;  // ✓ Set flag
      ::memcpy(ret_data->storage_.sso, data() + start_val, ret_len * sizeof(CharType));
    }
    else
    {
      ret_data->is_heap           = true;  // ✓ Set flag
      ret_data->storage_.heap.cap = ret_len + 1;
      ret_data->storage_.heap.ptr = string_allocator.allocateArray<CharType>(ret_data->storage_.heap.cap);
      ::memcpy(ret_data->storage_.heap.ptr, data() + start_val, ret_len * sizeof(CharType));
    }

    ret_data->setLen(ret_len);
    ret_data->terminate();

    return StringRef(ret_data);
  }

  StringRef substr(SizeType start) const { return substr(std::optional<SizeType>(start), std::nullopt); }

  // Convert to double - improved error handling
  double toDouble(SizeType* pos = nullptr) const
  {
    if (empty() || len() == 0)
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
    if (pos)
      *pos = static_cast<SizeType>(utf8_pos);

    return result;
  }

  MYLANG_NODISCARD std::string toUtf8() const
  {
    if (empty() || len() == 0)
      return std::string{};

    std::string result;
    result.reserve(len());  // PERFORMANCE: Pre-allocate approximate size

    try
    {
      // Use embedded utf16to8 conversion
      utf8::utf16to8(data(), data() + len(), std::back_inserter(result));
    } catch (const std::runtime_error& e)
    {
      throw std::runtime_error(std::string("UTF-16 to UTF-8 conversion failed: ") + e.what());
    }

    return result;
  }

  MYLANG_NODISCARD static StringRef fromUtf8(const std::string& utf8_str)
  {
    if (utf8_str.empty())
      return StringRef{};

    std::u16string utf16_str;
    utf16_str.reserve(utf8_str.size());  // PERFORMANCE: Pre-allocate

    try
    {
      utf8::utf8to16(utf8_str.begin(), utf8_str.end(), std::back_inserter(utf16_str));
    } catch (const std::runtime_error& e)
    {
      throw std::runtime_error(std::string("UTF-8 to UTF-16 conversion failed: ") + e.what());
    }

    return StringRef(utf16_str.data());
  }

  // Convenience overload for C strings
  MYLANG_NODISCARD static StringRef fromUtf8(const char* utf8_cstr)
  {
    if (!utf8_cstr)
      return StringRef{};
    return fromUtf8(std::string(utf8_cstr));
  }

  // PERFORMANCE: Only copy when actually shared
  void COW()
  {
    /// TODO: add sso support
    if (!StringData_)
      return;

    if (StringData_->referenceCount() > 1)
    {
      String* s = string_allocator.allocateObject<String>(*StringData_);
      StringData_->decrement();
      StringData_ = s;
    }
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