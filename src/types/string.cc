#include "../../include/types/string.hpp"
#include <arm_neon.h>


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

  // After expansion, we're working with a fresh buffer, so reset offset
  Offset_ = 0;

  // Set the actual length in StringData_
  StringData_->setLen(old_len);
  StringData_->terminate();
}


// Reserve capacity (doesn't change length, only capacity)
void StringRef::reserve(const SizeType new_capacity)
{
  if (new_capacity <= cap())
    return;
  expand(new_capacity);
}

// Erase character at position - use view manipulation when possible
void StringRef::erase(const SizeType at)
{
  if (empty() || at >= len())
    return;

  // Optimization: if erasing from the beginning, just adjust offset
  if (at == 0)
  {
    Offset_++;
    Length_--;
    return;
  }

  // Optimization: if erasing from the end, just adjust length
  if (at == Length_ - 1)
  {
    Length_--;
    return;
  }

  // Middle deletion requires actual modification
  ensureUnique();

  if (!data())
    return;

  // Shift remaining characters left
  ::memmove(data() + at, data() + at + 1, (len() - at - 1) * sizeof(CharType));

  // Update view length
  Length_--;

  // Update underlying data length
  StringData_->setLen(Offset_ + Length_);
  StringData_->terminate();
}

// Append another StringRef
typename StringRef::Reference StringRef::operator+=(ConstReference other)
{
  if (other.empty())
    return *this;

  ensureUnique();

  SizeType old_len   = Length_;
  SizeType add_len   = other.Length_;
  SizeType new_len   = old_len + add_len;
  SizeType total_len = Offset_ + new_len;

  // Ensure capacity
  if (total_len >= cap())
    expand(new_len);

  // Copy data to end of our view
  Pointer dst = StringData_->ptr() + Offset_ + old_len;
  ::memcpy(dst, other.data(), add_len * sizeof(CharType));

  // Update view length
  Length_ = new_len;

  // Update underlying data length
  StringData_->setLen(Offset_ + Length_);
  StringData_->terminate();

  return *this;
}

typename StringRef::Reference StringRef::operator+=(CharType c)
{
  if (!StringData_)
    return *this;

  ensureUnique();

  SizeType total_len = Offset_ + Length_;

  // Ensure capacity (need space for char + null)
  if (total_len + 1 >= StringData_->cap())
    expand(Length_ + 1);

  Pointer ptr = StringData_->ptr();

  // Append character at end of our view
  ptr[Offset_ + Length_] = c;
  Length_++;

  // Update underlying data length
  StringData_->setLen(Offset_ + Length_);
  StringData_->terminate();

  return *this;
}

// Bounds-checked access (const)
CharType StringRef::operator[](const SizeType i) const
{
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::operator[]: null string data");
  if (i >= Length_)
    throw std::out_of_range("StringRef::operator[]: index out of bounds");
  return (*StringData_)[i + Offset_];
}

// Bounds-checked access (non-const)
CharType& StringRef::operator[](const SizeType i)
{
  ensureUnique();
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::operator[]: null string data");
  if (i >= Length_)
    throw std::out_of_range("StringRef::operator[]: index out of bounds");
  return (*StringData_)[i + Offset_];
}

// Safe access with bounds checking (always checked)
MYLANG_NODISCARD CharType StringRef::at(const SizeType i) const
{
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::at: null string data");
  if (i >= Length_)
    throw std::out_of_range("StringRef::at: index out of bounds");
  return (*StringData_)[i + Offset_];
}

CharType& StringRef::at(const SizeType i)
{
  ensureUnique();
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::at: null string data");
  if (i >= Length_)
    throw std::out_of_range("StringRef::at: index out of bounds");
  return (*StringData_)[i + Offset_];
}

// Accessors
MYLANG_NODISCARD SizeType StringRef::len() const noexcept { return Length_; }

MYLANG_NODISCARD SizeType        StringRef::cap() const noexcept { return StringData_ ? StringData_->cap() : 0; }
MYLANG_NODISCARD String*         StringRef::get() const noexcept { return StringData_; }
MYLANG_NODISCARD bool            StringRef::empty() const noexcept { return Length_ == 0; }
typename StringRef::ConstPointer StringRef::data() const noexcept { return StringData_ ? StringData_->ptr() + Offset_ : nullptr; }

typename StringRef::Pointer StringRef::data() noexcept
{
  ensureUnique();
  return StringData_ ? StringData_->ptr() + Offset_ : nullptr;
}

// Clear the string - use view manipulation exclusively
void StringRef::clear() noexcept
{
  // Always just reset the view, never modify underlying data
  // This is safe even for exclusively owned strings
  Length_ = 0;

  // Optionally move offset to 0 for cleaner state
  // (useful if this ref will be reused for appending)
  if (StringData_ && StringData_->referenceCount() == 1)
  {
    // Reset to beginning of buffer for better append performance
    Offset_ = 0;
  }
}

// Resize capacity (preserves content)
void StringRef::resize(const SizeType s)
{
  ensureUnique();
  if (s > cap())
    expand(s);
}

// Find a character - purely view-based
MYLANG_NODISCARD bool StringRef::find(const CharType c) const noexcept
{
  if (!StringData_ || !data())
    return false;

  ConstPointer p   = data();
  ConstPointer end = data() + Length_;

  while (p < end && *p != c)
    ++p;

  return p < end;
}

// Find substring - purely view-based
MYLANG_NODISCARD bool StringRef::find(const StringRef& s) const noexcept
{
  if (!StringData_ || !data() || s.empty())
    return false;

  if (s.len() > len())
    return false;

  SizeType search_len = s.len();
  SizeType max_start  = len() - search_len;

  for (SizeType i = 0; i <= max_start; ++i)
  {
    if (::memcmp(data() + i, s.data(), search_len * sizeof(CharType)) == 0)
      return true;
  }

  return false;
}

// Find position of a character - purely view-based
MYLANG_NODISCARD std::optional<SizeType> StringRef::find_pos(const CharType c) const noexcept
{
  if (!StringData_ || !data())
    return std::nullopt;

  ConstPointer p   = data();
  ConstPointer end = data() + Length_;

  while (p < end && *p != c)
    ++p;

  return (p < end) ? std::optional<SizeType>(p - data()) : std::nullopt;
}

// Truncate string - purely view-based
StringRef& StringRef::truncate(const SizeType s)
{
  if (s >= Length_)
    return *this;

  // Simply adjust the view length - no modification to StringData_ needed
  Length_ = s;

  return *this;
}

// Substring - creates new StringData with actual copy
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
    ret_data->is_heap = false;
    ::memcpy(ret_data->storage_.sso, data() + start_val, ret_len * sizeof(CharType));
  }
  else
  {
    ret_data->is_heap           = true;
    ret_data->storage_.heap.cap = ret_len + 1;
    ret_data->storage_.heap.ptr = string_allocator.allocateArray<CharType>(ret_data->storage_.heap.cap);
    ::memcpy(ret_data->storage_.heap.ptr, data() + start_val, ret_len * sizeof(CharType));
  }

  ret_data->setLen(ret_len);
  ret_data->terminate();

  return StringRef(ret_data);
}

StringRef StringRef::substr(SizeType start) const { return substr(std::optional<SizeType>(start), std::nullopt); }

// Convert to double - purely view-based
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

// Convert to UTF-8 - purely view-based
MYLANG_NODISCARD std::string StringRef::toUtf8() const
{
  if (empty() || len() == 0)
    return "";

  const CharType* src     = data();
  const CharType* src_end = src + len();

  bool is_ascii = true;

#if defined(__ARM_NEON)

  const uint16_t* p16 = reinterpret_cast<const uint16_t*>(src);
  const uint16_t* end = reinterpret_cast<const uint16_t*>(src_end);

  uint16x8_t high_mask = vdupq_n_u16(0xFF80);

  while (p16 + 32 <= end)
  {
    uint16x8_t c1 = vld1q_u16(p16);
    uint16x8_t c2 = vld1q_u16(p16 + 8);
    uint16x8_t c3 = vld1q_u16(p16 + 16);
    uint16x8_t c4 = vld1q_u16(p16 + 24);

    uint16x8_t comb = vorrq_u16(vorrq_u16(c1, c2), vorrq_u16(c3, c4));
    uint16x8_t test = vandq_u16(comb, high_mask);

    uint64x2_t t = vreinterpretq_u64_u16(test);
    if (vgetq_lane_u64(t, 0) | vgetq_lane_u64(t, 1))
    {
      is_ascii = false;
      break;
    }
    p16 += 32;
  }

  while (is_ascii && p16 + 8 <= end)
  {
    uint16x8_t chunk = vld1q_u16(p16);
    uint16x8_t test  = vandq_u16(chunk, high_mask);

    uint64x2_t t = vreinterpretq_u64_u16(test);
    if (vgetq_lane_u64(t, 0) | vgetq_lane_u64(t, 1))
    {
      is_ascii = false;
      break;
    }
    p16 += 8;
  }

  while (is_ascii && p16 < end)
  {
    if (*p16 & 0xFF80)
      is_ascii = false;
    ++p16;
  }

#else

  for (const CharType* p = src; p < src_end; ++p)
  {
    if (*p > 0x7F)
    {
      is_ascii = false;
      break;
    }
  }

#endif

  if (is_ascii)
  {
    std::string out;
    out.resize(len());

#if defined(__ARM_NEON)

    const uint16_t* p16 = reinterpret_cast<const uint16_t*>(src);
    uint8_t*        dst = reinterpret_cast<uint8_t*>(&out[0]);

    SizeType i = 0;
    SizeType n = len() & ~SizeType(15);

    for (; i < n; i += 16)
    {
      uint16x8_t lo = vld1q_u16(p16 + i);
      uint16x8_t hi = vld1q_u16(p16 + i + 8);

      uint8x8_t lo_n = vmovn_u16(lo);
      uint8x8_t hi_n = vmovn_u16(hi);

      uint8x16_t packed = vcombine_u8(lo_n, hi_n);
      vst1q_u8(dst + i, packed);
    }

    for (; i < len(); ++i)
      dst[i] = static_cast<uint8_t>(src[i]);

#else

    for (SizeType i = 0; i < len(); ++i)
      out[i] = static_cast<char>(src[i]);

#endif

    return out;
  }

  size_t utf8_len = 0;

  const CharType* p = src;
  while (p < src_end)
  {
    uint32_t c = *p++;

    if (c < 0x80)
      utf8_len += 1;
    else if (c < 0x800)
      utf8_len += 2;
    else if (c >= 0xD800 && c <= 0xDBFF)
    {
      // surrogate pair
      if (p >= src_end)
        throw std::runtime_error("Invalid UTF-16: truncated surrogate");

      uint32_t low = *p++;
      if (low < 0xDC00 || low > 0xDFFF)
        throw std::runtime_error("Invalid UTF-16: bad surrogate pair");

      utf8_len += 4;
    }
    else if (c >= 0xDC00 && c <= 0xDFFF)
      throw std::runtime_error("Invalid UTF-16: stray low surrogate");
    else
      utf8_len += 3;
  }

  std::string out;
  out.resize(utf8_len);

  char* outp = &out[0];
  p          = src;

  while (p < src_end)
  {
    uint32_t c = *p++;

    if (c < 0x80)
    {
      *outp++ = static_cast<char>(c);
    }
    else if (c < 0x800)
    {
      *outp++ = static_cast<char>(0xC0 | (c >> 6));
      *outp++ = static_cast<char>(0x80 | (c & 0x3F));
    }
    else if (c >= 0xD800 && c <= 0xDBFF)
    {
      uint32_t low = *p++;

      if (low < 0xDC00 || low > 0xDFFF)
        throw std::runtime_error("Invalid UTF-16 surrogate pair");

      uint32_t cp = 0x10000 + (((c - 0xD800) << 10) | (low - 0xDC00));

      *outp++ = static_cast<char>(0xF0 | (cp >> 18));
      *outp++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      *outp++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      *outp++ = static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (c >= 0xDC00 && c <= 0xDFFF)
      throw std::runtime_error("Invalid UTF-16: stray low surrogate");
    else
    {
      *outp++ = static_cast<char>(0xE0 | (c >> 12));
      *outp++ = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      *outp++ = static_cast<char>(0x80 | (c & 0x3F));
    }
  }

  return out;
}

MYLANG_NODISCARD StringRef StringRef::fromUtf8(const std::string& utf8_str)
{
  if (utf8_str.empty())
    return StringRef{};

  const char* src     = utf8_str.data();
  const char* src_end = src + utf8_str.size();

  const uint8_t* p_u8   = reinterpret_cast<const uint8_t*>(src);
  const uint8_t* end_u8 = reinterpret_cast<const uint8_t*>(src_end);

  bool is_ascii = true;

#if defined(__ARM_NEON)
  const uint8x16_t mask = vdupq_n_u8(0x80);

  while (p_u8 + 64 <= end_u8)
  {
    uint8x16_t c1 = vld1q_u8(p_u8);
    uint8x16_t c2 = vld1q_u8(p_u8 + 16);
    uint8x16_t c3 = vld1q_u8(p_u8 + 32);
    uint8x16_t c4 = vld1q_u8(p_u8 + 48);

    uint8x16_t combined = vorrq_u8(vorrq_u8(c1, c2), vorrq_u8(c3, c4));
    uint8x16_t test     = vandq_u8(combined, mask);

    uint64x2_t t = vreinterpretq_u64_u8(test);
    if (vgetq_lane_u64(t, 0) | vgetq_lane_u64(t, 1))
    {
      is_ascii = false;
      break;
    }
    p_u8 += 64;
  }

  while (is_ascii && p_u8 + 16 <= end_u8)
  {
    uint8x16_t chunk = vld1q_u8(p_u8);
    uint8x16_t test  = vandq_u8(chunk, mask);

    uint64x2_t t = vreinterpretq_u64_u8(test);
    if (vgetq_lane_u64(t, 0) | vgetq_lane_u64(t, 1))
    {
      is_ascii = false;
      break;
    }
    p_u8 += 16;
  }
#endif

  // ---- scalar tail ----
  while (is_ascii && p_u8 < end_u8)
  {
    if (*p_u8 & 0x80)
      is_ascii = false;
    ++p_u8;
  }

  if (is_ascii) [[likely]]
  {
    SizeType len      = utf8_str.size();
    String*  ret_data = string_allocator.allocateObject<String>();

    if (len < SSO_SIZE) [[likely]]
    {
      ret_data->is_heap = false;
      for (SizeType i = 0; i < len; ++i)
        ret_data->storage_.sso[i] = static_cast<CharType>(src[i]);
    }
    else
    {
      ret_data->is_heap           = true;
      ret_data->storage_.heap.cap = len + 1;
      ret_data->storage_.heap.ptr = string_allocator.allocateArray<CharType>(len + 1);

      CharType* dest = ret_data->storage_.heap.ptr;
      SizeType  i    = 0;

#if defined(__ARM_NEON)
      const uint8_t* src_u8 = reinterpret_cast<const uint8_t*>(src);

      // 16 chars per iteration
      for (SizeType n = len & ~SizeType(15); i < n; i += 16)
      {
        uint8x16_t v = vld1q_u8(src_u8 + i);

        uint16x8_t lo = vmovl_u8(vget_low_u8(v));
        uint16x8_t hi = vmovl_u8(vget_high_u8(v));

        vst1q_u16(reinterpret_cast<uint16_t*>(dest + i), lo);
        vst1q_u16(reinterpret_cast<uint16_t*>(dest + i + 8), hi);
      }
#endif

      for (; i < len; ++i)
        dest[i] = static_cast<CharType>(src[i]);
    }

    ret_data->setLen(len);
    ret_data->terminate();
    return StringRef(ret_data);
  }

  SizeType             utf16_len = 0;
  const unsigned char* p         = reinterpret_cast<const unsigned char*>(src);
  const unsigned char* e         = reinterpret_cast<const unsigned char*>(src_end);

  while (p < e)
  {
    uint32_t c = *p;

    if (c < 0x80)
    {
      utf16_len++;
      p++;
    }
    else if ((c & 0xE0) == 0xC0)
    {
      if (p + 1 >= e || (p[1] & 0xC0) != 0x80)
        throw std::runtime_error("Invalid UTF-8");

      uint32_t cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);
      if (cp < 0x80)  // overlong
        throw std::runtime_error("Overlong UTF-8");

      utf16_len++;
      p += 2;
    }
    else if ((c & 0xF0) == 0xE0)
    {
      if (p + 2 >= e || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80)
        throw std::runtime_error("Invalid UTF-8");

      uint32_t cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
      if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))
        throw std::runtime_error("Invalid UTF-8");

      utf16_len++;
      p += 3;
    }
    else if ((c & 0xF8) == 0xF0)
    {
      if (p + 3 >= e || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80)
        throw std::runtime_error("Invalid UTF-8");

      uint32_t cp = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);

      if (cp < 0x10000 || cp > 0x10FFFF)
        throw std::runtime_error("Invalid UTF-8");

      utf16_len += 2;
      p += 4;
    }
    else
      throw std::runtime_error("Invalid UTF-8");
  }

  // Allocate
  String*   ret_data = string_allocator.allocateObject<String>();
  CharType* dest;

  if (utf16_len < SSO_SIZE) [[likely]]
  {
    ret_data->is_heap = false;
    dest              = ret_data->storage_.sso;
  }
  else
  {
    ret_data->is_heap           = true;
    ret_data->storage_.heap.cap = utf16_len + 1;
    ret_data->storage_.heap.ptr = string_allocator.allocateArray<CharType>(utf16_len + 1);
    dest                        = ret_data->storage_.heap.ptr;
  }

  // Decode pass
  p             = reinterpret_cast<const unsigned char*>(src);
  CharType* out = dest;

  while (p < e)
  {
    uint32_t c = *p;

    if (c < 0x80)
    {
      *out++ = (CharType) c;
      p++;
    }
    else if ((c & 0xE0) == 0xC0)
    {
      uint32_t cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);
      *out++      = (CharType) cp;
      p += 2;
    }
    else if ((c & 0xF0) == 0xE0)
    {
      uint32_t cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
      *out++      = (CharType) cp;
      p += 3;
    }
    else
    {
      uint32_t cp = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);

      cp -= 0x10000;
      *out++ = (CharType) (0xD800 + (cp >> 10));
      *out++ = (CharType) (0xDC00 + (cp & 0x3FF));
      p += 4;
    }
  }

  ret_data->setLen(utf16_len);
  ret_data->terminate();
  return StringRef(ret_data);
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
  // The new slice will have Offset_ = this->Offset_ + start, Length_ = slice_len
  return StringRef(*this, start, slice_len);
}

}