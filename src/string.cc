/// string.cc

#include "../include/string.hpp"
#include "../include/util.hpp"

#include <cassert>
#include <charconv>
#include <simdutf.h>

namespace mylang {

namespace detail {

static char g_empty_string_storage[sizeof(String)];
static String *g_empty_string = nullptr;

String *emptyStringSingleton() noexcept {
  if (__builtin_expect(g_empty_string != nullptr, 1))
    return g_empty_string;

  g_empty_string = new (g_empty_string_storage) String();
  for (int i = 0; i < 1024; ++i)
    g_empty_string->increment();

  return g_empty_string;
}

} // namespace detail

String::String(size_t const s) : is_heap(s >= SSO_SIZE), len_(0) {
  if (s < SSO_SIZE) {
    storage_.sso[0] = BUFFER_END;
  } else {
    storage_.heap.cap = s + 1;
    storage_.heap.ptr = getAllocator().allocateArray<char>(storage_.heap.cap);
    storage_.heap.ptr[0] = BUFFER_END;
  }
}

String::String(size_t const s, char const c) : is_heap(s >= SSO_SIZE), len_(s) {
  if (s < SSO_SIZE) {
    ::memset(storage_.sso, c, s);
    storage_.sso[s] = BUFFER_END;
  } else {
    storage_.heap.cap = s + 1;
    storage_.heap.ptr = getAllocator().allocateArray<char>(storage_.heap.cap);
    ::memset(storage_.heap.ptr, c, s);
    storage_.heap.ptr[s] = BUFFER_END;
  }
}

String::String(char const *s, size_t n) {
  if (!s || n == 0) {
    is_heap = false;
    len_ = 0;
    storage_.sso[0] = BUFFER_END;
    return;
  }

  is_heap = (n >= SSO_SIZE);
  len_ = n;

  if (!is_heap) {
    ::memcpy(storage_.sso, s, n);
    storage_.sso[n] = BUFFER_END;
  } else {
    storage_.heap.cap = n + 1;
    storage_.heap.ptr = getAllocator().allocateArray<char>(storage_.heap.cap);
    ::memcpy(storage_.heap.ptr, s, n);
    storage_.heap.ptr[n] = BUFFER_END;
  }
}

String::String(char const *s) {
  if (!s) {
    is_heap = false;
    len_ = 0;
    storage_.sso[0] = BUFFER_END;
    return;
  }

  size_t n = ::strlen(s);
  is_heap = (n >= SSO_SIZE);
  len_ = n;

  if (!is_heap) {
    ::memcpy(storage_.sso, s, n + 1);
  } else {
    storage_.heap.cap = n + 1;
    storage_.heap.ptr = getAllocator().allocateArray<char>(storage_.heap.cap);
    ::memcpy(storage_.heap.ptr, s, n + 1);
  }
}

bool String::operator==(String const &other) const noexcept {
  if (len_ != other.len_)
    return false;
  if (len_ == 0)
    return true;
  return ::memcmp(ptr(), other.ptr(), len_) == 0;
}

StringRef::StringRef(size_t const s) : StringData_(getAllocator().allocateObject<String>(s)), Offset_(0), Length_(0) {}

StringRef::StringRef(StringRef const &other, size_t offset, size_t length)
    : StringData_(other.StringData_), Offset_(other.Offset_ + offset),
      Length_(length ? length : (other.Length_ > offset ? other.Length_ - offset : 0)) {
  if (StringData_)
    StringData_->increment();
}

StringRef::StringRef(char const *lit) {
  if (!lit || !lit[0]) {
    StringData_ = detail::emptyStringSingleton();
    StringData_->increment();
    Offset_ = 0;
    Length_ = 0;
    return;
  }

  StringData_ = getAllocator().allocateObject<String>(lit);
  Offset_ = 0;
  Length_ = StringData_->length();
}

StringRef::StringRef(char16_t const *u16_str) {
  if (!u16_str || !u16_str[0]) {
    StringData_ = detail::emptyStringSingleton();
    StringData_->increment();
    Offset_ = 0;
    Length_ = 0;
    return;
  }

  StringRef temp = fromUtf16(u16_str);
  StringData_ = temp.StringData_;
  Offset_ = temp.Offset_;
  Length_ = temp.Length_;
  temp.StringData_ = nullptr;
}

StringRef::StringRef(size_t const s, char const c) : StringData_(getAllocator().allocateObject<String>(s, c)), Offset_(0), Length_(s) {}

StringRef::StringRef(String *data, size_t offset, size_t length)
    : StringData_(data ? data : detail::emptyStringSingleton()), Offset_(offset), Length_(length) {
  if (!length)
    Length_ = StringData_->length() > offset ? StringData_->length() - offset : 0;
  StringData_->increment();
}

StringRef::StringRef(StringRef &&other) noexcept : StringData_(other.StringData_), Offset_(other.Offset_), Length_(other.Length_) {
  other.StringData_ = nullptr;
  other.Offset_ = 0;
  other.Length_ = 0;
}

StringRef &StringRef::operator=(StringRef &&other) noexcept {
  if (this == &other)
    return *this;

  if (StringData_) {
    StringData_->decrement();
    if (StringData_->referenceCount() == 0) {
      StringData_->~String();
      getAllocator().deallocateObject<String>(StringData_);
    }
  }

  StringData_ = other.StringData_;
  Offset_ = other.Offset_;
  Length_ = other.Length_;
  other.StringData_ = nullptr;
  other.Offset_ = 0;
  other.Length_ = 0;

  return *this;
}

StringRef::~StringRef() {
  if (!StringData_)
    return;

  StringData_->decrement();
  if (StringData_->referenceCount() == 0) {
    StringData_->~String();
    getAllocator().deallocateObject<String>(StringData_);
    StringData_ = nullptr;
  }
}

StringRef &StringRef::operator=(StringRef const &other) {
  if (this == &other)
    return *this;

  if (StringData_)
    StringData_->decrement();

  StringData_ = other.StringData_;
  Offset_ = other.Offset_;
  Length_ = other.Length_;

  if (StringData_)
    StringData_->increment();

  return *this;
}

void StringRef::expand(size_t const new_size) {
  ensureUnique();

  if (new_size <= cap())
    return;

  char const *old_ptr = data();
  size_t old_len = len();

  size_t new_capacity = (cap() < 1024) ? std::max(new_size + 1, cap() * 2) : std::max(new_size + 1, cap() + cap() / 2);

  char *new_ptr = getAllocator().allocateArray<char>(new_capacity);

  if (old_ptr && old_len > 0)
    ::memcpy(new_ptr, old_ptr, old_len);

  if (StringData_->isHeap() && StringData_->storage_.heap.ptr)
    getAllocator().deallocateArray<char>(StringData_->storage_.heap.ptr, StringData_->storage_.heap.cap);

  StringData_->storage_.heap.ptr = new_ptr;
  StringData_->storage_.heap.cap = new_capacity;
  StringData_->is_heap = true;
  Offset_ = 0;

  StringData_->setLen(old_len);
  StringData_->terminate();
}

void StringRef::reserve(size_t const new_capacity) {
  if (new_capacity <= cap())
    return;
  expand(new_capacity);
}

void StringRef::erase(size_t const at) {
  if (empty() || at >= len())
    return;

  if (at == 0) {
    ++Offset_;
    --Length_;
    return;
  }
  if (at == Length_ - 1) {
    --Length_;
    return;
  }

  ensureUnique();
  if (!data())
    return;

  ::memmove(data() + at, data() + at + 1, len() - at - 1);
  --Length_;
  StringData_->setLen(Offset_ + Length_);
  StringData_->terminate();
}

StringRef &StringRef::operator+=(StringRef const &other) {
  if (other.empty())
    return *this;

  ensureUnique();

  size_t new_len = Length_ + other.Length_;
  if (Offset_ + new_len >= cap())
    expand(new_len);

  ::memcpy(StringData_->ptr() + Offset_ + Length_, other.data(), other.Length_);
  Length_ = new_len;
  StringData_->setLen(Offset_ + Length_);
  StringData_->terminate();

  return *this;
}

StringRef &StringRef::operator+=(char c) {
  if (!StringData_)
    return *this;

  ensureUnique();

  if (Offset_ + Length_ + 1 >= StringData_->cap())
    expand(Length_ + 1);

  StringData_->ptr()[Offset_ + Length_] = c;
  ++Length_;
  StringData_->setLen(Offset_ + Length_);
  StringData_->terminate();

  return *this;
}

char StringRef::operator[](size_t const i) const {
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::operator[]: null string data");
  if (i >= Length_)
    throw std::out_of_range("StringRef::operator[]: index out of bounds");
  return (*StringData_)[i + Offset_];
}

char &StringRef::operator[](size_t const i) {
  ensureUnique();
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::operator[]: null string data");
  if (i >= Length_)
    throw std::out_of_range("StringRef::operator[]: index out of bounds");
  return (*StringData_)[i + Offset_];
}

char StringRef::at(size_t const i) const {
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::at: null string data");
  if (i >= Length_)
    throw std::out_of_range("StringRef::at: index out of bounds");
  return (*StringData_)[i + Offset_];
}

char &StringRef::at(size_t const i) {
  ensureUnique();
  if (!StringData_ || !data())
    throw std::runtime_error("StringRef::at: null string data");
  if (i >= Length_)
    throw std::out_of_range("StringRef::at: index out of bounds");
  return (*StringData_)[i + Offset_];
}

bool StringRef::find(char const c) const noexcept {
  if (!StringData_ || !data())
    return false;
  char const *p = data();
  char const *end = p + Length_;
  while (p < end && *p != c)
    ++p;
  return p < end;
}

bool StringRef::find(StringRef const &s) const noexcept {
  if (!StringData_ || !data() || s.empty() || s.len() > len())
    return false;
  size_t search_len = s.len();
  size_t max_start = len() - search_len;
  for (size_t i = 0; i <= max_start; ++i) {
    if (::memcmp(data() + i, s.data(), search_len) == 0)
      return true;
  }
  return false;
}

std::optional<size_t> StringRef::find_pos(char const c) const noexcept {
  if (!StringData_ || !data())
    return std::nullopt;
  char const *p = data();
  char const *end = p + Length_;
  while (p < end && *p != c)
    ++p;
  return (p < end) ? std::optional<size_t>(p - data()) : std::nullopt;
}

StringRef &StringRef::trimWhitespace(bool leading, bool trailing) noexcept {
  if (leading) {
    while (Length_ > 0 && util::isWhitespace(data()[0])) {
      ++Offset_;
      --Length_;
    }
  }

  if (trailing) {
    while (Length_ > 0 && util::isWhitespace(data()[Length_ - 1]))
      --Length_;
  }

  return *this;
}

StringRef &StringRef::truncate(size_t const s) noexcept {
  if (s < Length_)
    Length_ = s;
  return *this;
}

void StringRef::resize(size_t const s) {
  ensureUnique();
  if (s > cap())
    expand(s);
}

StringRef StringRef::slice(size_t start, size_t end) const {
  if (!StringData_ || Length_ == 0)
    return {};

  if (start > Length_) {
    diagnostic::emit("StringRef::slice: start index out of range");
    return {};
  }

  if (end > Length_ || end == SIZE_MAX)
    end = Length_;

  if (end < start) {
    diagnostic::emit("StringRef::slice: end must be >= start");
    return {};
  }

  return StringRef(*this, start, end - start);
}

StringRef StringRef::substrCopy(size_t start, size_t end) const {
  if (!StringData_ || Length_ == 0)
    return {};

  if (start > Length_)
    throw std::out_of_range("StringRef::substrCopy: start index out of range");

  if (end > Length_ || end == SIZE_MAX)
    end = Length_;

  if (end < start)
    throw std::invalid_argument("StringRef::substrCopy: end must be >= start");

  size_t copy_len = end - start;

  if (copy_len == 0)
    return {};

  String *ret = getAllocator().allocateObject<String>(copy_len);

  ::memcpy(ret->ptr(), data() + start, copy_len);
  ret->setLen(copy_len);
  ret->terminate();

  return StringRef(ret);
}

double StringRef::toDouble(size_t *pos) const {
  if (empty())
    throw std::invalid_argument("StringRef::toDouble: empty string");

  double result{};
  auto [end_ptr, ec] = std::from_chars(data(), data() + Length_, result);

  if (ec == std::errc::invalid_argument)
    throw std::invalid_argument("StringRef::toDouble: invalid number format");

  if (ec == std::errc::result_out_of_range)
    throw std::out_of_range("StringRef::toDouble: number out of range");

  if (pos)
    *pos = static_cast<size_t>(end_ptr - data());

  return result;
}

StringRef StringRef::fromUtf16(char16_t const *src) {
  if (!src || !src[0])
    return {};

  char16_t const *p = src;
  while (*p)
    ++p;
  size_t src_len = static_cast<size_t>(p - src);

  simdutf::result validation = simdutf::validate_utf16_with_errors(src, src_len);
  if (validation.error != simdutf::error_code::SUCCESS)
    throw std::runtime_error("Invalid UTF-16 at code unit " + std::to_string(validation.count));

  size_t utf8_len = simdutf::utf8_length_from_utf16(src, src_len);

  String *ret_data = getAllocator().allocateObject<String>(utf8_len);
  char *dest = ret_data->ptr();

  size_t written = simdutf::convert_utf16_to_utf8(src, src_len, dest);
  assert(written == utf8_len);

  ret_data->setLen(utf8_len);
  ret_data->terminate();

  return StringRef(ret_data);
}

void StringRef::detach() {
  if (!StringData_)
    return;

  size_t copy_len = Length_ > 0 ? Length_ : (StringData_->length() - Offset_);

  String *s = getAllocator().allocateObject<String>(copy_len);

  if (copy_len > 0)
    ::memcpy(s->ptr(), StringData_->ptr() + Offset_, copy_len);

  s->setLen(copy_len);
  s->terminate();

  StringData_->decrement();
  StringData_ = s;
  Offset_ = 0;
  Length_ = copy_len;
}

} // namespace mylang
