#include "../../include/types/string.hpp"


namespace mylang {

void StringRef::expand(const SizeType new_size)
{
  ensureUnique();

  if (new_size <= cap())
    return;

  ConstPointer old_ptr = data();
  SizeType     old_len = len();
  SizeType     new_capacity;

  if (cap() < 1024)
    new_capacity = std::max(new_size + 1, cap() * 2);
  else
    new_capacity = std::max(new_size + 1, cap() + cap() / 2);

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
void StringRef::reserve(const SizeType new_capacity)
{
  if (new_capacity <= cap())
    return;
  expand(new_capacity);
}

// Erase character at position
void StringRef::erase(const SizeType at)
{
  if (at == Offset_)
  {
    Offset_++;
    return;
  }

  if (at == Length_ - 1)
  {
    Length_--;
    return;
  }

  ensureUnique();

  if (at >= len() || !data())
    return;  // Out of bounds or empty string

  ::memmove(data() + at, data() + at + 1, (len() - at - 1) * sizeof(CharType));

  StringData_->setLen(len() - 1);
  StringData_->terminate();
  Length_--;
}

// Append another StringRef
typename StringRef::Reference StringRef::operator+=(ConstReference other)
{
  ensureUnique();

  SizeType old_len = Length_;
  SizeType add_len = other.Length_;
  SizeType new_len = old_len + add_len;

  if (new_len >= cap())
    expand(new_len);

  Pointer dst = StringData_->ptr() + old_len;
  ::memcpy(dst, other.data(), add_len * sizeof(CharType));

  StringData_->setLen(new_len);
  dst[add_len] = BUFFER_END;
  Length_ += add_len;

  return *this;
}

typename StringRef::Reference StringRef::operator+=(CharType c)
{
  if (!StringData_)
    return *this;

  SizeType l = StringData_->length();

  // ensure unique before modification
  ensureUnique();

  // ensure capacity (need space for char + null)
  if (l + 1 >= StringData_->cap())
    expand(l + 1);

  Pointer ptr = data();

  ptr[l]     = c;
  ptr[l + 1] = 0;

  StringData_->setLen(l + 1);
  Length_++;

  return *this;
}

// FIXED: Add null checks
CharType StringRef::operator[](const SizeType i) const
{
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::operator[]: null string data");
  return (*StringData_)[i + Offset_];
}

CharType& StringRef::operator[](const SizeType i)
{
  ensureUnique();
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::operator[]: null string data");
  return (*StringData_)[i + Offset_];
}

// Safe access with bounds checking (always checked)
MYLANG_NODISCARD CharType StringRef::at(const SizeType i) const
{
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::at: null string data");
  if (i >= Length_ + Offset_)
    throw std::out_of_range("StringRef::at: index out of bounds");
  return (*StringData_)[i + Offset_];
}

CharType& StringRef::at(const SizeType i)
{
  ensureUnique();
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::at: null string data");
  if (i >= Length_ + Offset_)
    throw std::out_of_range("StringRef::at: index out of bounds");
  return (*StringData_)[i];
}

// Accessors
MYLANG_NODISCARD SizeType StringRef::len() const noexcept
{
  return Length_;  // should return only the visible length
}

MYLANG_NODISCARD SizeType        StringRef::cap() const noexcept { return StringData_ ? StringData_->cap() : 0; }
MYLANG_NODISCARD String*         StringRef::get() const noexcept { return StringData_; }
MYLANG_NODISCARD bool            StringRef::empty() const noexcept { return Length_ == 0; }
typename StringRef::ConstPointer StringRef::data() const noexcept { return StringData_ ? StringData_->ptr() + Offset_ : nullptr; }

typename StringRef::Pointer StringRef::data() noexcept
{
  ensureUnique();
  return StringData_ ? StringData_->ptr() + Offset_ : nullptr;
}

// Clear the string (doesn't deallocate memory)
void StringRef::clear() noexcept
{
  ensureUnique();
  if (data() && cap() > 0)
    (*StringData_)[0] = BUFFER_END;
  StringData_->setLen(0);
}

// Resize capacity (preserves content)
void StringRef::resize(const SizeType s)
{
  ensureUnique();
  if (s > cap())
    expand(s);
}

// Find a character
MYLANG_NODISCARD bool StringRef::find(const CharType c) const noexcept
{
  if (!StringData_ || !data())
    return false;

  ConstPointer p = data();
  while (*p && *p != c)
    ++p;

  return *p == c;
}

MYLANG_NODISCARD bool StringRef::find(const StringRef& s) const noexcept
{
  if (!StringData_ || !data())
    return false;

  if (s.len() > len())
    return false;

  SizeType start = 0;
  SizeType end   = s.len() - 1;

  while (start < end && end < len())
  {
    if (s == substr(start, end))
      return true;
    start++, end++;
  }

  return false;
}

// Find position of a character (returns optional index)
MYLANG_NODISCARD std::optional<SizeType> StringRef::find_pos(const CharType c) const noexcept
{
  if (!StringData_ || !data())
    return std::nullopt;

  ConstPointer p = data();
  while (*p && *p != c)
    ++p;
  return *p == c ? std::optional<SizeType>(p - data()) : std::nullopt;
}

// Truncate string to specified length
StringRef& StringRef::truncate(const SizeType s)
{
  ensureUnique();
  if (s < len() && data())
  {
    StringData_->setLen(s);
    StringData_->terminate();
  }
  return *this;
}

StringRef StringRef::substr(std::optional<SizeType> start, std::optional<SizeType> end) const
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

StringRef StringRef::substr(SizeType start) const { return substr(std::optional<SizeType>(start), std::nullopt); }

// Convert to double - improved error handling
double StringRef::toDouble(SizeType* pos) const
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

MYLANG_NODISCARD std::string StringRef::toUtf8() const
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

MYLANG_NODISCARD StringRef StringRef::fromUtf8(const std::string& utf8_str)
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
MYLANG_NODISCARD StringRef StringRef::fromUtf8(const char* utf8_cstr)
{
  if (!utf8_cstr)
    return StringRef{};
  return fromUtf8(std::string(utf8_cstr));
}

// Zero-copy slicing: returns a view into the existing string
StringRef StringRef::slice(SizeType start, std::optional<SizeType> end) const
{
  if (!StringData_ || Length_ == 0)
    return StringRef{};

  const SizeType src_len = Length_;

  if (start >= src_len)
    throw std::out_of_range("StringRef::slice: start index out of range");

  SizeType end_val = end.value_or(src_len - 1);
  if (end_val >= src_len)
    end_val = src_len - 1;

  if (end_val < start)
    throw std::invalid_argument("StringRef::slice: end must be >= start");

  SizeType slice_len = end_val - start + 1;

  // Use constructor that handles refcount
  return StringRef(*this, start, slice_len);
}

}