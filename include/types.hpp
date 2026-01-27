#pragma once

#include "macros.hpp"
#include "runtime/allocator/arena.hpp"

#include <optional>
#include <ostream>
#include <stdexcept>
#include <utility>


namespace mylang {

class ASTAllocator
{
 private:
  runtime::allocator::ArenaAllocator Allocator_;

 public:
  ASTAllocator() :
      Allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::LINEAR))
  {
  }

  template<typename T, typename... Args>
  T* allocate(Args&&... args)
  {
    void* mem = Allocator_.allocate<std::byte>(sizeof(T));
    return new (mem) T(std::forward<Args>(args)...);
  }

  template<typename T>
  void deallocate(T* p, SizeType c = 1)
  {
    Allocator_.deallocate<T>(p, c);
  }
};

inline ASTAllocator string_allocator;

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
    if (index >= Len_)
    {
      throw std::out_of_range("StringRef: index out of bounds");
    }
  }

 public:
  // Default constructor
  StringRef() = default;

  // Constructor with size (allocates capacity for s characters + null terminator)
  explicit StringRef(const SizeType s)
  {
    if (s == 0)
    {
      return;
    }

    Ptr_ = string_allocator.allocate<CharType>(s + 1);  // +1 for null terminator
    if (Ptr_ == nullptr)
    {
      throw std::bad_alloc();
    }

    Capacity_ = s + 1;
    Len_      = 0;
    Ptr_[0]   = CharType{0};  // Ensure null termination
  }

  // Copy constructor - FIXED: copy correct amount and set capacity
  StringRef(ConstReference other)
  {
    if (other.len() == 0)
    {
      return;
    }

    // Allocate space for string + null terminator
    Ptr_ = string_allocator.allocate<CharType>(other.len() + 1);
    if (Ptr_ == nullptr)
    {
      throw std::bad_alloc();
    }

    // Copy only the actual string data (not the full capacity)
    std::memcpy(Ptr_, other.get(), other.len() * sizeof(CharType));
    Len_      = other.len();
    Capacity_ = other.len() + 1;

    // Ensure null termination
    Ptr_[Len_] = CharType{0};
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

    // Allocate with space for null terminator
    Ptr_ = string_allocator.allocate<CharType>(length + 1);
    if (Ptr_ == nullptr)
    {
      throw std::bad_alloc();
    }

    // Copy the string data
    std::memcpy(Ptr_, lit, length * sizeof(CharType));
    Len_       = length;
    Capacity_  = length + 1;
    Ptr_[Len_] = CharType{0};
  }

  // Copy assignment operator - FIXED: set capacity correctly
  Reference operator=(ConstReference other)
  {
    if (this == &other)
    {
      return *this;  // Self-assignment guard
    }

    if (other.len() == 0)
    {
      // Clear current string
      if (Ptr_ != nullptr)
      {
        string_allocator.deallocate<CharType>(Ptr_, Capacity_);
      }
      Ptr_      = nullptr;
      Len_      = 0;
      Capacity_ = 0;
      return *this;
    }

    // Deallocate old memory if it exists
    if (Ptr_ != nullptr)
    {
      string_allocator.deallocate<CharType>(Ptr_, Capacity_);
    }

    // Allocate new memory
    Ptr_ = string_allocator.allocate<CharType>(other.len() + 1);
    if (Ptr_ == nullptr)
    {
      throw std::bad_alloc();
    }

    std::memcpy(Ptr_, other.get(), other.len() * sizeof(CharType));
    Len_       = other.len();
    Capacity_  = other.len() + 1;
    Ptr_[Len_] = CharType{0};

    return *this;
  }

  // Move assignment operator
  Reference operator=(StringRef&& other) noexcept
  {
    if (this == &other)
    {
      return *this;
    }

    // Deallocate our current memory
    if (Ptr_ != nullptr)
    {
      string_allocator.deallocate<CharType>(Ptr_, Capacity_);
    }

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
      string_allocator.deallocate<CharType>(Ptr_, Capacity_);
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
    {
      return false;
    }

    // Both empty
    if (Len_ == 0)
    {
      return true;
    }

    // Self comparison
    if (Ptr_ == other.get())
    {
      return true;
    }

    // Deep comparison
    return std::memcmp(Ptr_, other.get(), Len_ * sizeof(CharType)) == 0;
  }

  bool operator!=(ConstReference other) const noexcept { return !(*this == other); }

  // Expand capacity - FIXED: properly track old capacity for deallocation
  void expand(const SizeType new_size)
  {
    if (new_size <= Capacity_)
    {
      return;  // Already have enough capacity
    }

    SizeType old_capacity = Capacity_;
    Pointer  old_ptr      = Ptr_;

    // Try to allocate 1.5x the requested size + 1 for null terminator
    SizeType new_capacity = static_cast<SizeType>(new_size * 1.5 + 1);

    Ptr_ = string_allocator.allocate<CharType>(new_capacity);
    if (Ptr_ == nullptr)
    {
      // Fallback to exact size
      new_capacity = new_size + 1;
      Ptr_         = string_allocator.allocate<CharType>(new_capacity);
      if (Ptr_ == nullptr)
      {
        throw std::bad_alloc();
      }
    }

    // Copy old data if it exists
    if (old_ptr != nullptr)
    {
      std::memcpy(Ptr_, old_ptr, Len_ * sizeof(CharType));
      string_allocator.deallocate<CharType>(old_ptr, old_capacity);
    }

    Capacity_ = new_capacity;

    // Ensure null termination
    if (Len_ < Capacity_)
    {
      Ptr_[Len_] = CharType{0};
    }
  }

  // Erase character at position
  void erase(const SizeType at)
  {
    if (at >= Len_ || Ptr_ == nullptr)
    {
      return;  // Out of bounds or empty string
    }

    // Shift characters left
    for (SizeType i = at; i < Len_ - 1; i++)
    {
      Ptr_[i] = Ptr_[i + 1];
    }

    Len_--;
    Ptr_[Len_] = CharType{0};
  }

  // Append another StringRef
  Reference operator+=(ConstReference other)
  {
    if (other.len() == 0)
    {
      return *this;
    }

    SizeType new_len = Len_ + other.len();

    if (new_len + 1 > Capacity_)  // +1 for null terminator
    {
      expand(new_len + 1);
    }

    std::memcpy(Ptr_ + Len_, other.get(), other.len() * sizeof(CharType));
    Len_       = new_len;
    Ptr_[Len_] = CharType{0};

    return *this;
  }

  // Append a single character
  Reference operator+=(CharType c)
  {
    if (Len_ + 1 >= Capacity_)  // Need space for char + null terminator
    {
      expand(Len_ + 2);
    }

    Ptr_[Len_]     = c;
    Ptr_[Len_ + 1] = CharType{0};
    Len_++;

    return *this;
  }

  // Index operator (const) - FIXED: added bounds checking
  CharType operator[](const SizeType i) const
  {
    checkBounds(i);
    return Ptr_[i];
  }

  // Index operator (non-const) - FIXED: added bounds checking
  CharType& operator[](const SizeType i)
  {
    checkBounds(i);
    return Ptr_[i];
  }

  // Safe access with bounds checking (always checked)
  MYLANG_NODISCARD CharType at(const SizeType i) const
  {
    if (i >= Len_)
    {
      throw std::out_of_range("StringRef::at: index out of bounds");
    }
    return Ptr_[i];
  }

  CharType& at(const SizeType i)
  {
    if (i >= Len_)
    {
      throw std::out_of_range("StringRef::at: index out of bounds");
    }
    return Ptr_[i];
  }

  // Concatenation operators

  // StringRef + StringRef
  friend StringRef operator+(ConstReference lhs, ConstReference rhs)
  {
    if (lhs.empty() && rhs.empty())
    {
      return StringRef{};
    }

    if (lhs.empty())
    {
      return StringRef(rhs);
    }

    if (rhs.empty())
    {
      return StringRef(lhs);
    }

    SizeType  new_len = lhs.len() + rhs.len();
    StringRef result(new_len);  // Allocate exact size needed

    std::memcpy(result.Ptr_, lhs.get(), lhs.len() * sizeof(CharType));
    std::memcpy(result.Ptr_ + lhs.len(), rhs.get(), rhs.len() * sizeof(CharType));

    result.Len_          = new_len;
    result.Ptr_[new_len] = CharType{0};

    return result;
  }

  // StringRef + const char* (UTF-8 string)
  friend StringRef operator+(ConstReference lhs, const char* rhs)
  {
    if (rhs == nullptr || rhs[0] == '\0')
    {
      return StringRef(lhs);
    }

    StringRef rhs_str = fromUtf8(rhs);
    return lhs + rhs_str;
  }

  // const char* + StringRef
  friend StringRef operator+(const char* lhs, ConstReference rhs)
  {
    if (lhs == nullptr || lhs[0] == '\0')
    {
      return StringRef(rhs);
    }

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
  MYLANG_NODISCARD ConstPointer get() const noexcept { return Ptr_; }
  MYLANG_NODISCARD bool         empty() const noexcept { return Len_ == 0; }

  // Mutable accessors (use with caution)
  SizeType& len_ref() { return Len_; }

  // Output stream operator
  friend std::ostream& operator<<(std::ostream& os, ConstReference str)
  {
    if (str.empty())
    {
      return os;
    }

    os << str.toUtf8();
    return os;
  }

  // Clear the string (doesn't deallocate memory)
  void clear() noexcept
  {
    if (Ptr_ != nullptr && Capacity_ > 0)
    {
      Ptr_[0] = u'\0';
    }
    Len_ = 0;
  }

  // Resize capacity (preserves content)
  void resize(const SizeType s)
  {
    if (s > Capacity_)
    {
      expand(s);
    }
  }

  // Find a character
  MYLANG_NODISCARD bool find(const CharType c) const noexcept
  {
    for (SizeType i = 0; i < Len_; ++i)
    {
      if (Ptr_[i] == c)
      {
        return true;
      }
    }
    return false;
  }

  // Find position of a character (returns optional index)
  MYLANG_NODISCARD std::optional<SizeType> find_pos(const CharType c) const noexcept
  {
    for (SizeType i = 0; i < Len_; ++i)
    {
      if (Ptr_[i] == c)
      {
        return i;
      }
    }
    return std::nullopt;
  }

  // Truncate string to specified length
  StringRef& truncate(const SizeType s)
  {
    if (s < Len_ && Ptr_ != nullptr)
    {
      Ptr_[s] = CharType{0};
      Len_    = s;
    }
    return *this;
  }

  // Substring - FIXED: correct end parameter usage and length calculation
  // Returns substring from start (inclusive) to end (inclusive)
  MYLANG_NODISCARD StringRef substr(std::optional<SizeType> start, std::optional<SizeType> end) const
  {
    if (empty())
    {
      return StringRef{};
    }

    SizeType start_val = start.value_or(0);
    SizeType end_val   = end.value_or(Len_ - 1);  // FIXED: use 'end', not 'start'

    // Bounds checking
    if (start_val >= Len_)
    {
      throw std::out_of_range("StringRef::substr: start index out of range");
    }

    if (end_val >= Len_)
    {
      end_val = Len_ - 1;  // Clamp to valid range
    }

    if (end_val < start_val)
    {
      throw std::invalid_argument("StringRef::substr: end must be >= start");
    }

    // FIXED: correct length calculation (end is inclusive)
    SizeType ret_len = end_val - start_val + 1;

    StringRef ret(ret_len);

    for (SizeType i = 0; i < ret_len; i++)
    {
      ret += Ptr_[start_val + i];
    }

    return ret;
  }

  // Overload for convenience: substr(start) returns from start to end
  MYLANG_NODISCARD StringRef substr(SizeType start) const { return substr(std::optional<SizeType>(start), std::nullopt); }

  // Convert to double - improved error handling
  MYLANG_NODISCARD double toDouble(SizeType* pos = nullptr) const
  {
    if (empty() || Len_ == 0)
    {
      throw std::invalid_argument("StringRef::toDouble: empty string");
    }

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
    {
      *pos = static_cast<SizeType>(utf8_pos);
    }

    return result;
  }

  // Convert to UTF-8 string
  MYLANG_NODISCARD std::string toUtf8() const
  {
    if (empty() || Len_ == 0)
    {
      return std::string{};
    }

    std::string result;
    result.reserve(Len_ * 3);  // Reserve space (UTF-8 can be up to 4 bytes per char)

    for (SizeType i = 0; i < Len_; ++i)
    {
      uint32_t cp = static_cast<uint32_t>(Ptr_[i]);

      // Handle surrogate pairs
      if (utf::isHighSurrogate(Ptr_[i]))
      {
        if (i + 1 < Len_ && utf::isLowSurrogate(Ptr_[i + 1]))
        {
          cp = utf::combineSurrogates(Ptr_[i], Ptr_[i + 1]);
          ++i;
        }
        else
        {
          throw std::runtime_error("Invalid UTF-16: lone high surrogate");
        }
      }
      else if (utf::isLowSurrogate(Ptr_[i]))
      {
        throw std::runtime_error("Invalid UTF-16: lone low surrogate");
      }

      // Encode as UTF-8
      if (cp <= utf::CODEPOINT_MAX_1BYTE)
      {
        result.push_back(static_cast<char>(cp));
      }
      else if (cp <= utf::CODEPOINT_MAX_2BYTE)
      {
        result.push_back(static_cast<char>(utf::UTF8_2BYTE_MARKER | (cp >> 6)));
        result.push_back(static_cast<char>(utf::UTF8_CONTINUATION_MARKER | (cp & utf::UTF8_CONTINUATION_VALUE_MASK)));
      }
      else if (cp <= utf::CODEPOINT_MAX_3BYTE)
      {
        result.push_back(static_cast<char>(utf::UTF8_3BYTE_MARKER | (cp >> 12)));
        result.push_back(static_cast<char>(utf::UTF8_CONTINUATION_MARKER | ((cp >> 6) & utf::UTF8_CONTINUATION_VALUE_MASK)));
        result.push_back(static_cast<char>(utf::UTF8_CONTINUATION_MARKER | (cp & utf::UTF8_CONTINUATION_VALUE_MASK)));
      }
      else if (cp <= utf::CODEPOINT_MAX_4BYTE)
      {
        result.push_back(static_cast<char>(utf::UTF8_4BYTE_MARKER | (cp >> 18)));
        result.push_back(static_cast<char>(utf::UTF8_CONTINUATION_MARKER | ((cp >> 12) & utf::UTF8_CONTINUATION_VALUE_MASK)));
        result.push_back(static_cast<char>(utf::UTF8_CONTINUATION_MARKER | ((cp >> 6) & utf::UTF8_CONTINUATION_VALUE_MASK)));
        result.push_back(static_cast<char>(utf::UTF8_CONTINUATION_MARKER | (cp & utf::UTF8_CONTINUATION_VALUE_MASK)));
      }
      else
      {
        throw std::runtime_error("Invalid code point");
      }
    }

    return result;
  }

  // Convert from UTF-8 string
  MYLANG_NODISCARD static StringRef fromUtf8(const std::string& utf8_str)
  {
    if (utf8_str.empty())
    {
      return StringRef{};
    }

    // First pass: count how many UTF-16 code units we need
    SizeType       utf16_len = 0;
    const SizeType utf8_len  = utf8_str.size();
    SizeType       i         = 0;

    while (i < utf8_len)
    {
      Byte byte1 = static_cast<Byte>(utf8_str[i]);

      if (byte1 < utf::UTF8_1BYTE_MASK)  // 1-byte
      {
        utf16_len++;
        i++;
      }
      else if ((byte1 & utf::UTF8_2BYTE_MASK) == utf::UTF8_2BYTE_MARKER)  // 2-byte
      {
        if (i + 1 >= utf8_len)
        {
          throw std::runtime_error("Invalid UTF-8: incomplete 2-byte sequence");
        }
        utf16_len++;
        i += 2;
      }
      else if ((byte1 & utf::UTF8_3BYTE_MASK) == utf::UTF8_3BYTE_MARKER)  // 3-byte
      {
        if (i + 2 >= utf8_len)
        {
          throw std::runtime_error("Invalid UTF-8: incomplete 3-byte sequence");
        }
        utf16_len++;
        i += 3;
      }
      else if ((byte1 & utf::UTF8_4BYTE_MASK) == utf::UTF8_4BYTE_MARKER)  // 4-byte
      {
        if (i + 3 >= utf8_len)
        {
          throw std::runtime_error("Invalid UTF-8: incomplete 4-byte sequence");
        }
        utf16_len += 2;  // 4-byte UTF-8 becomes surrogate pair in UTF-16
        i += 4;
      }
      else
      {
        // Invalid byte, skip it
        i++;
      }
    }

    // Second pass: actually convert
    StringRef result(utf16_len);
    i = 0;

    while (i < utf8_len)
    {
      Byte byte1 = static_cast<Byte>(utf8_str[i]);

      if (byte1 < utf::UTF8_1BYTE_MASK)  // 1-byte
      {
        result += static_cast<CharType>(byte1);
        i++;
      }
      else if ((byte1 & utf::UTF8_2BYTE_MASK) == utf::UTF8_2BYTE_MARKER)  // 2-byte
      {
        if (i + 1 >= utf8_len)
        {
          throw std::runtime_error("Invalid UTF-8: incomplete 2-byte sequence");
        }

        Byte byte2 = static_cast<Byte>(utf8_str[i + 1]);
        if (!utf::isUtf8Continuation(byte2))
        {
          throw std::runtime_error("Invalid UTF-8: bad continuation byte");
        }

        uint32_t cp = ((byte1 & utf::UTF8_2BYTE_VALUE_MASK) << 6) | (byte2 & utf::UTF8_CONTINUATION_VALUE_MASK);
        result += static_cast<CharType>(cp);
        i += 2;
      }
      else if ((byte1 & utf::UTF8_3BYTE_MASK) == utf::UTF8_3BYTE_MARKER)  // 3-byte
      {
        if (i + 2 >= utf8_len)
        {
          throw std::runtime_error("Invalid UTF-8: incomplete 3-byte sequence");
        }

        Byte byte2 = static_cast<Byte>(utf8_str[i + 1]);
        Byte byte3 = static_cast<Byte>(utf8_str[i + 2]);
        if (!utf::isUtf8Continuation(byte2) || !utf::isUtf8Continuation(byte3))
        {
          throw std::runtime_error("Invalid UTF-8: bad continuation byte");
        }

        uint32_t cp = ((byte1 & utf::UTF8_3BYTE_VALUE_MASK) << 12) | ((byte2 & utf::UTF8_CONTINUATION_VALUE_MASK) << 6)
                      | (byte3 & utf::UTF8_CONTINUATION_VALUE_MASK);
        result += static_cast<CharType>(cp);
        i += 3;
      }
      else if ((byte1 & utf::UTF8_4BYTE_MASK) == utf::UTF8_4BYTE_MARKER)  // 4-byte
      {
        if (i + 3 >= utf8_len)
        {
          throw std::runtime_error("Invalid UTF-8: incomplete 4-byte sequence");
        }

        Byte byte2 = static_cast<Byte>(utf8_str[i + 1]);
        Byte byte3 = static_cast<Byte>(utf8_str[i + 2]);
        Byte byte4 = static_cast<Byte>(utf8_str[i + 3]);
        if (!utf::isUtf8Continuation(byte2) || !utf::isUtf8Continuation(byte3) || !utf::isUtf8Continuation(byte4))
        {
          throw std::runtime_error("Invalid UTF-8: bad continuation byte");
        }

        uint32_t cp = ((byte1 & utf::UTF8_4BYTE_VALUE_MASK) << 18) | ((byte2 & utf::UTF8_CONTINUATION_VALUE_MASK) << 12)
                      | ((byte3 & utf::UTF8_CONTINUATION_VALUE_MASK) << 6) | (byte4 & utf::UTF8_CONTINUATION_VALUE_MASK);

        if (cp > utf::CODEPOINT_MAX_4BYTE)
        {
          throw std::runtime_error("Invalid UTF-8: code point out of range");
        }

        CharType high, low;
        utf::splitToSurrogates(cp, high, low);
        result += high;
        result += low;
        i += 4;
      }
      else
      {
        // Invalid byte, skip it
        i++;
      }
    }

    return result;
  }

  // Convenience overload for C strings
  MYLANG_NODISCARD static StringRef fromUtf8(const char* utf8_cstr)
  {
    if (utf8_cstr == nullptr)
    {
      return StringRef{};
    }
    return fromUtf8(std::string(utf8_cstr));
  }
};  // StringRef


// Hash functor for StringRef
struct StringRefHash
{
  std::size_t operator()(const StringRef& str) const noexcept
  {
    if (str.empty() || str.len() == 0)
    {
      return 0;
    }

    // FNV-1a hash algorithm (good distribution, fast)
    std::size_t       hash  = 14695981039346656037ULL;  // FNV offset basis
    const std::size_t prime = 1099511628211ULL;         // FNV prime

    const CharType* data = str.get();
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
    {
      return 0;
    }

    // FNV-1a hash
    std::size_t       hash_value = 14695981039346656037ULL;
    const std::size_t prime      = 1099511628211ULL;

    const mylang::CharType* data = str.get();
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