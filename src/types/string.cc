#include "../../include/types/string.hpp"
#include "../../include/util.hpp"

#include <arm_neon.h>
#include <cassert>
#include <simdutf.h>

namespace mylang {

StringRef::StringRef()
    : StringData_(createEmpty())
{
}

StringRef::StringRef(SizeType const s)
    : StringData_(string_allocator.allocateObject<String>(s))
{
}

StringRef::StringRef(StringRef const& other, SizeType offset, SizeType length)
    : StringData_(other.get())
    , Offset_(other.Offset_ + offset)
    , Length_(length)
{
    if (Length_ == 0)
        Length_ = other.Length_ - offset;

    if (StringData_)
        StringData_->increment();
}

StringRef::StringRef(char const* lit)
    : StringData_(string_allocator.allocateObject<String>(lit))
{
    Length_ = StringData_->length();
}

StringRef::StringRef(char16_t const* u16_str)
{
    if (!u16_str || !u16_str[0]) {
        StringData_ = createEmpty();
        Offset_ = 0;
        Length_ = 0;
    } else {
        StringRef temp = fromUtf16(u16_str);
        StringData_ = temp.StringData_;
        Offset_ = temp.Offset_;
        Length_ = temp.Length_;
        if (StringData_)
            StringData_->increment();
        temp.StringData_ = nullptr; // Prevent temp's destructor from decrementing
    }
}

StringRef::StringRef(SizeType const s, char const c)
    : StringData_(string_allocator.allocateObject<String>(s, c))
{
}

StringRef::StringRef(String* data, SizeType offset, SizeType length)
    : StringData_(data ? data : createEmpty())
    , Offset_(offset)
    , Length_(length)
{
    if (StringData_)
        StringData_->increment();

    if (!Length_)
        Length_ = StringData_->length() - offset;
}

StringRef::StringRef(StringRef&& other) noexcept
    : StringData_(other.StringData_)
    , Offset_(other.Offset_)
    , Length_(other.Length_)
{
    other.StringData_ = nullptr;
    other.Offset_ = 0;
    other.Length_ = 0;
}

StringRef& StringRef::operator=(StringRef&& other) noexcept
{
    if (this == &other)
        return *this;

    // Clean up current data
    if (StringData_) {
        StringData_->decrement();
        if (StringData_->referenceCount() == 0) {
            StringData_->~String();
            string_allocator.deallocateObject<String>(StringData_);
        }
    }

    // Move from other
    StringData_ = other.StringData_;
    Offset_ = other.Offset_;
    Length_ = other.Length_;

    other.StringData_ = nullptr;
    other.Offset_ = 0;
    other.Length_ = 0;

    return *this;
}

StringRef::~StringRef()
{
    if (StringData_) {
        /// NOTE: this will only be deallocated if it's the last pointer in the arena
        /// we should probably make a better allocator for that ...
        StringData_->decrement();
        if (StringData_->referenceCount() == 0) {
            StringData_->~String(); // deallocate if possible the string array
            string_allocator.deallocateObject<String>(StringData_);
            StringData_ = nullptr;
        }
    }
}

StringRef& StringRef::operator=(StringRef const& other)
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

    Offset_ = other.Offset_;
    Length_ = other.Length_;

    return *this;
}

void StringRef::expand(SizeType const new_size)
{
    ensureUnique();

    if (new_size <= cap())
        return;

    char const* old_ptr = data();
    SizeType old_len = len();
    SizeType new_capacity;

    if (cap() < 1024)
        new_capacity = std::max(new_size + 1, cap() * 2);
    else
        new_capacity = std::max(new_size + 1, cap() + cap() / 2);

    char* new_ptr = string_allocator.allocateArray<char>(new_capacity);

    // Copy old data
    if (old_ptr && old_len > 0)
        ::memcpy(new_ptr, old_ptr, old_len * sizeof(char));

    if (StringData_->isHeap() && StringData_->storage_.heap.ptr)
        string_allocator.deallocateArray<char>(StringData_->storage_.heap.ptr, StringData_->storage_.heap.cap);

    // Now set heap storage
    StringData_->storage_.heap.ptr = new_ptr;
    StringData_->storage_.heap.cap = new_capacity;
    StringData_->is_heap = true;

    // After expansion, we're working with a fresh buffer, so reset offset
    Offset_ = 0;

    // Set the actual length in StringData_
    StringData_->setLen(old_len);
    StringData_->terminate();
}

// Reserve capacity (doesn't change length, only capacity)
void StringRef::reserve(SizeType const new_capacity)
{
    if (new_capacity <= cap())
        return;
    expand(new_capacity);
}

// Erase character at position - use view manipulation when possible
void StringRef::erase(SizeType const at)
{
    if (empty() || at >= len())
        return;

    // Optimization: if erasing from the beginning, just adjust offset
    if (at == 0) {
        Offset_++;
        Length_--;
        return;
    }

    // Optimization: if erasing from the end, just adjust length
    if (at == Length_ - 1) {
        Length_--;
        return;
    }

    // Middle deletion requires actual modification
    ensureUnique();

    if (!data())
        return;

    // Shift remaining characters left
    ::memmove(data() + at, data() + at + 1, (len() - at - 1) * sizeof(char));

    // Update view length
    Length_--;

    // Update underlying data length
    StringData_->setLen(Offset_ + Length_);
    StringData_->terminate();
}

// Append another StringRef
StringRef& StringRef::operator+=(StringRef const& other)
{
    if (other.empty())
        return *this;

    ensureUnique();

    SizeType new_len = Length_ + other.Length_;

    // Ensure capacity
    if (Offset_ + new_len >= cap())
        expand(new_len);

    // Copy data to end of our view
    char* dst = StringData_->ptr() + Offset_ + Length_;
    ::memcpy(dst, other.data(), other.Length_ * sizeof(char));

    // Update view length
    Length_ = new_len;

    // Update underlying data length
    StringData_->setLen(Offset_ + Length_);
    StringData_->terminate();

    return *this;
}

StringRef& StringRef::operator+=(char c)
{
    if (!StringData_)
        return *this;

    ensureUnique();

    SizeType total_len = Offset_ + Length_;

    // Ensure capacity (need space for char + null)
    if (total_len + 1 >= StringData_->cap())
        expand(Length_ + 1);

    char* ptr = StringData_->ptr();

    // Append character at end of our view
    ptr[Offset_ + Length_] = c;
    Length_++;

    // Update underlying data length
    StringData_->setLen(Offset_ + Length_);
    StringData_->terminate();

    return *this;
}

// Bounds-checked access (const)
char StringRef::operator[](SizeType const i) const
{
    if (!StringData_ || !data())
        throw std::runtime_error("StringRef::operator[]: null string data");
    if (i >= Length_)
        throw std::out_of_range("StringRef::operator[]: index out of bounds");
    return (*StringData_)[i + Offset_];
}

// Bounds-checked access (non-const)
char& StringRef::operator[](SizeType const i)
{
    ensureUnique();
    if (!StringData_ || !data())
        throw std::runtime_error("StringRef::operator[]: null string data");
    if (i >= Length_)
        throw std::out_of_range("StringRef::operator[]: index out of bounds");
    return (*StringData_)[i + Offset_];
}

// Safe access with bounds checking (always checked)
MYLANG_NODISCARD char StringRef::at(SizeType const i) const
{
    if (!StringData_ || !data())
        throw std::runtime_error("StringRef::at: null string data");
    if (i >= Length_)
        throw std::out_of_range("StringRef::at: index out of bounds");
    return (*StringData_)[i + Offset_];
}

char& StringRef::at(SizeType const i)
{
    ensureUnique();
    if (!StringData_ || !data())
        throw std::runtime_error("StringRef::at: null string data");
    if (i >= Length_)
        throw std::out_of_range("StringRef::at: index out of bounds");
    return (*StringData_)[i + Offset_];
}

// Clear the string - use view manipulation exclusively
void StringRef::clear() noexcept
{
    // Always just reset the view, never modify underlying data
    // This is safe even for exclusively owned strings
    Length_ = 0;
    Offset_ = 0;
}

// Resize capacity (preserves content)
void StringRef::resize(SizeType const s)
{
    ensureUnique();
    if (s > cap())
        expand(s);
}

// Find a character - purely view-based
MYLANG_NODISCARD bool StringRef::find(char const c) const noexcept
{
    if (!StringData_ || !data())
        return false;

    char const* p = data();
    char const* end = data() + Length_;

    while (p < end && *p != c)
        ++p;

    return p < end;
}

// Find substring - purely view-based
MYLANG_NODISCARD bool StringRef::find(StringRef const& s) const noexcept
{
    if (!StringData_ || !data() || s.empty())
        return false;

    if (s.len() > len())
        return false;

    SizeType search_len = s.len();
    SizeType max_start = len() - search_len;

    for (SizeType i = 0; i <= max_start; ++i) {
        if (::memcmp(data() + i, s.data(), search_len * sizeof(char)) == 0)
            return true;
    }

    return false;
}

// Find position of a character - purely view-based
MYLANG_NODISCARD std::optional<SizeType> StringRef::find_pos(char const c) const noexcept
{
    if (!StringData_ || !data())
        return std::nullopt;

    char const* p = data();
    char const* end = data() + Length_;

    while (p < end && *p != c)
        ++p;

    return (p < end) ? std::optional<SizeType>(p - data()) : std::nullopt;
}

// Truncate string - purely view-based
StringRef& StringRef::truncate(SizeType const s)
{
    if (s >= Length_)
        return *this;

    // Simply adjust the view length
    Length_ = s;
    return *this;
}

StringRef StringRef::substr(std::optional<SizeType> start, std::optional<SizeType> end) const
{
    if (empty())
        return "";

    SizeType start_val = start.value_or(0);
    SizeType end_val = end.value_or(len());

    if (start_val >= len())
        throw std::out_of_range("StringRef::substr: start index out of range");

    if (end_val > len())
        end_val = len();

    if (end_val < start_val)
        throw std::invalid_argument("StringRef::substr: end must be >= start");

    SizeType ret_len = end_val - start_val;
    String* ret_data = string_allocator.allocateObject<String>();

    if (ret_len < SSO_SIZE) {
        ret_data->is_heap = false;
        ::memcpy(ret_data->storage_.sso, data() + start_val, ret_len * sizeof(char));
    } else {
        ret_data->is_heap = true;
        ret_data->storage_.heap.cap = ret_len + 1;
        ret_data->storage_.heap.ptr = string_allocator.allocateArray<char>(ret_data->storage_.heap.cap);
        ::memcpy(ret_data->storage_.heap.ptr, data() + start_val, ret_len * sizeof(char));
    }

    ret_data->setLen(ret_len);
    ret_data->terminate();

    return StringRef(ret_data);
}

// Zero-copy slicing: returns a view into the existing string
StringRef StringRef::slice(SizeType start, std::optional<SizeType> end) const
{
    if (!StringData_ || Length_ == 0)
        return "";

    SizeType const src_len = Length_;

    if (start >= src_len)
        throw std::out_of_range("StringRef::slice: start index out of range");

    SizeType end_val = end.value_or(src_len - 1);

    if (end_val >= src_len)
        end_val = src_len - 1;

    if (end_val < start)
        throw std::invalid_argument("StringRef::slice: end must be >= start");

    SizeType slice_len = end_val - start + 1;

    return StringRef(*this, start, slice_len);
}

// Convert to double - purely view-based
double StringRef::toDouble(SizeType* pos) const
{
    if (empty() || len() == 0)
        throw std::invalid_argument("StringRef::toDouble: empty string");

    // Use std::stod on the UTF-8 string
    SizeType _pos = 0;
    double result;

    try {
        result = std::stod(std::string(data()), &_pos);
    } catch (std::invalid_argument const&) {
        throw std::invalid_argument("StringRef::toDouble: invalid number format");
    } catch (std::out_of_range const&) {
        throw std::out_of_range("StringRef::toDouble: number out of range");
    }

    // Convert UTF-8 position back to UTF-16 position if requested
    // This is approximate and may not be exact for multi-byte characters
    if (pos)
        *pos = _pos;

    return result;
}

MYLANG_NODISCARD StringRef StringRef::fromUtf16(char16_t const* src)
{
    if (!src || !src[0])
        return "";

    char16_t const* p = src;
    while (*p++)
        ;
    size_t src_len = p - src - 1;
    // Validate and get exact UTF-16 length in one pass
    simdutf::result validation = simdutf::validate_utf16_with_errors(src, src_len);

    if (validation.error != simdutf::error_code::SUCCESS)
        throw std::runtime_error("Invalid UTF-8 at position " + std::to_string(validation.count));

    size_t utf8_len = simdutf::utf8_length_from_utf16(src, src_len);

    // Allocate String object
    String* ret_data = string_allocator.allocateObject<String>();
    char* dest;

    if (utf8_len < SSO_SIZE) {
        ret_data->is_heap = false;
        dest = ret_data->storage_.sso;
    } else {
        ret_data->is_heap = true;
        ret_data->storage_.heap.cap = utf8_len + 1;
        ret_data->storage_.heap.ptr = string_allocator.allocateArray<char>(utf8_len + 1);
        dest = ret_data->storage_.heap.ptr;
    }

    // Convert UTF-8 to UTF-16
    size_t written = simdutf::convert_utf16_to_utf8(src, src_len, dest);

    assert(written == utf8_len);

    ret_data->setLen(utf8_len);
    ret_data->terminate();
    return StringRef(ret_data);
}

StringRef& StringRef::trimWhitespace(std::optional<bool const> leading, std::optional<bool const> trailing)
{
    bool const trim_leading = leading.value_or(true);
    bool const trim_trailing = trailing.value_or(true);

    if (trim_leading) {
        while (Length_ > 0 && util::isWhitespace(data()[0]))
            ++Offset_, --Length_;
    }

    if (trim_trailing) {
        while (Length_ > 0 && util::isWhitespace(data()[Length_ - 1]))
            --Length_;
    }

    return *this;
}

void StringRef::detach()
{
    if (!StringData_)
        return;

    // Determine slice length
    SizeType copy_len = (Length_ > 0) ? Length_ : (StringData_->length() - Offset_);

    // Allocate a new string of exactly the required size
    String* s = string_allocator.allocateObject<String>(copy_len);

    // Copy only the relevant portion
    if (copy_len > 0)
        ::memcpy(s->ptr(), const_cast<char const*>(StringData_->ptr() + Offset_), copy_len * sizeof(char));

    s->setLen(copy_len);
    s->terminate();

    // Decrement old reference
    StringData_->decrement();

    // Update StringRef to point to new data
    StringData_ = s;
    Offset_ = 0;
    Length_ = copy_len;
}

}
