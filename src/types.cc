#include "../include/types.hpp"


namespace mylang {

// constructors

StringRef::StringRef(const SizeType s)
{
  if (s == 0)
    return;

  Ptr_ = string_allocator.allocate<CharType>(s + 1);  // +1 for null terminator
  if (!Ptr_)
    throw std::bad_alloc();

  Capacity_ = s + 1;
  Len_      = 0;
  Ptr_[0]   = BUFFER_END;  // Ensure null termination
}

StringRef::StringRef(ConstReference other)
{
  if (other.len() == 0)
    return;

  // Allocate space for string + null terminator
  Ptr_ = string_allocator.allocate<CharType>(other.len() + 1);
  if (!Ptr_)
    throw std::bad_alloc();

  // using std::memcpy just ruined the buffer somehow
  for (SizeType i = 0; i < other.len(); ++i)
    Ptr_[i] = other[i];

  Len_      = other.len();
  Capacity_ = other.len() + 1;

  // Ensure null termination
  Ptr_[Len_] = BUFFER_END;
}

StringRef::StringRef(StringRef&& other) noexcept :
    Ptr_(other.Ptr_),
    Len_(other.Len_),
    Capacity_(other.Capacity_)
{
  other.Ptr_      = nullptr;
  other.Len_      = 0;
  other.Capacity_ = 0;
}

StringRef::StringRef(ConstPointer lit)
{
  if (!lit)
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
  if (!Ptr_)
    throw std::bad_alloc();

  // Copy the string data
  std::memcpy(Ptr_, lit, length * sizeof(CharType));
  Len_       = length;
  Capacity_  = length + 1;
  Ptr_[Len_] = BUFFER_END;
}

StringRef::StringRef(const SizeType s, const CharType c)
{
  if (s == 0)
  {
    *this = StringRef();
    return;
  }

  Ptr_ = string_allocator.allocate<CharType>(s + 1);
  if (!Ptr_)
    throw std::bad_alloc();

  for (SizeType i = 0; i < s; ++i)
    Ptr_[i] = c;

  Ptr_[s]   = BUFFER_END;
  Capacity_ = s + 1;
}

// operators

// assignment

typename StringRef::Reference StringRef::operator=(ConstReference other)
{
  if (this == &other)
    return *this;  // Self-assignment guard

  if (other.len() == 0)
  {
    Ptr_      = nullptr;
    Len_      = 0;
    Capacity_ = 0;
    return *this;
  }

  // FIXED: Allocate NEW memory FIRST, before deallocating old
  Pointer new_ptr = string_allocator.allocate<CharType>(other.len() + 1);
  if (!new_ptr)
    throw std::bad_alloc();

  // Copy data to NEW memory (other is still intact)
  // std::memcpy(new_ptr, other.cget(), other.len() * sizeof(CharType));
  for (SizeType i = 0; i < other.len(); ++i)
    new_ptr[i] = other[i];

  new_ptr[other.len()] = BUFFER_END;

  // Update pointers
  Ptr_      = new_ptr;
  Len_      = other.len();
  Capacity_ = other.len() + 1;

  return *this;
}

typename StringRef::Reference StringRef::operator=(StringRef&& other) noexcept
{
  if (this == &other)
    return *this;

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

bool StringRef::operator==(ConstReference other) const noexcept
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

typename StringRef::Reference StringRef::operator+=(ConstReference other)
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

typename StringRef::Reference StringRef::operator+=(CharType c)
{
  if (Len_ + 2 > Capacity_)
    expand(Len_ + 2);

  Ptr_[Len_]     = c;
  Ptr_[Len_ + 1] = BUFFER_END;
  ++Len_;

  return *this;
}

// utils

StringRef StringRef::substr(std::optional<SizeType> start, std::optional<SizeType> end) const
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

double StringRef::toDouble(SizeType* pos) const
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
  if (pos)
    *pos = static_cast<SizeType>(utf8_pos);

  return result;
}

std::string StringRef::toUtf8() const
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

StringRef StringRef::fromUtf8(const std::string& utf8_str)
{
  if (utf8_str.empty())
    return StringRef{};

  std::u16string utf16_str = u"";

  try
  {
    utf16_str = utf8::utf8to16(utf8_str);
  } catch (const std::runtime_error& e)
  {
    throw std::runtime_error(std::string("UTF-8 to UTF-16 conversion failed: ") + e.what());
  }

  return StringRef(utf16_str.data());
}

void StringRef::erase(const SizeType at)
{
  if (at >= Len_ || !Ptr_)
    return;  // Out of bounds or empty string

  // Shift characters left
  for (SizeType i = at; i < Len_ - 1; ++i)
    Ptr_[i] = Ptr_[i + 1];

  --Len_;
  Ptr_[Len_] = BUFFER_END;
}

void StringRef::expand(const SizeType new_size)
{
  if (new_size <= Capacity_)
    return;  // Already have enough capacity

  SizeType old_capacity = Capacity_;
  Pointer  old_ptr      = Ptr_;
  SizeType new_capacity = 0;

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
  if (!Ptr_)
  {
    // Fallback to exact size
    new_capacity = new_size + 1;
    Ptr_         = string_allocator.allocate<CharType>(new_capacity);
    if (!Ptr_)
      throw std::bad_alloc();
  }

  // Copy old data if it exists
  if (old_ptr)
    std::memcpy(Ptr_, old_ptr, Len_ * sizeof(CharType));

  Capacity_ = new_capacity;

  // Ensure null termination
  if (Len_ < Capacity_)
    Ptr_[Len_] = BUFFER_END;
}


}