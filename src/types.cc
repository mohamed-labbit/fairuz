#include "../include/types.hpp"


namespace mylang {

// constructors

StringRef::StringRef(const SizeType s) { StringData_ = string_allocator.allocate(s); }

StringRef::StringRef(ConstReference other)
{
  StringData_ = other.get();
  StringData_->increment();
}

StringRef::StringRef(ConstPointer lit) { StringData_ = string_allocator.allocate(lit); }

StringRef::StringRef(const SizeType s, const CharType c) { StringData_ = string_allocator.allocate(s, c); }

// operators

// assignment

typename StringRef::Reference StringRef::operator=(ConstReference other)
{
  if (this == &other)
    return *this;  // Self-assignment guard

  StringData_ = other.get();
  StringData_->increment();

  return *this;
}

bool StringRef::operator==(ConstReference other) const noexcept { return *StringData_ == *other.get(); }

typename StringRef::Reference StringRef::operator+=(ConstReference other)
{
  if (StringData_->referenceCount() > 1)
  {
    String* p   = StringData_;
    StringData_ = string_allocator.allocate(*p);
    p->decrement();
  }

  if (other.len() == 0)
    return *this;

  SizeType new_len = StringData_->len + other.len();

  if (new_len + 1 > StringData_->cap)  // +1 for null terminator
    expand(new_len + 1);

  std::memcpy(StringData_->ptr + StringData_->len, other.get(), other.len() * sizeof(CharType));
  StringData_->len                   = new_len;
  StringData_->ptr[StringData_->len] = BUFFER_END;

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

  std::memmove(Ptr_ + at, Ptr_ + at + 1, (Len_ - at - 1) * sizeof(*Ptr_));

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