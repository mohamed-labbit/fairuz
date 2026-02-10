#include "../../include/types/string.hpp"
#include "../../include/lex/util.hpp"

#include <arm_neon.h>
#include <cassert>
#include <simdutf.h>

namespace mylang {

void StringRef::expand(SizeType const new_size)
{
    ensureUnique();

    if (new_size <= cap())
        return;

    CharType const* old_ptr = data();
    SizeType old_len = len();
    SizeType new_capacity;

    if (cap() < 1024)
        new_capacity = std::max(new_size + 1, cap() * 2);
    else
        new_capacity = std::max(new_size + 1, cap() + cap() / 2);

    CharType* new_ptr = string_allocator.allocateArray<CharType>(new_capacity);

    // Copy old data
    if (old_ptr && old_len > 0)
        ::memcpy(new_ptr, old_ptr, old_len * sizeof(CharType));

    if (StringData_->isHeap() && StringData_->storage_.heap.ptr)
        string_allocator.deallocateArray<CharType>(StringData_->storage_.heap.ptr, StringData_->storage_.heap.cap);

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
        ++Offset_, --Length_;
        return;
    }

    // Optimization: if erasing from the end, just adjust length
    if (at == Length_ - 1) {
        --Length_;
        return;
    }

    // Middle deletion requires actual modification
    ensureUnique();

    if (!data())
        return;

    // Shift remaining characters left
    ::memmove(data() + at, data() + at + 1, (len() - at - 1) * sizeof(CharType));

    // Update view length
    --Length_;

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
    CharType* dst = StringData_->ptr() + Offset_ + Length_;
    ::memcpy(dst, other.data(), other.Length_ * sizeof(CharType));

    // Update view length
    Length_ = new_len;

    // Update underlying data length
    StringData_->setLen(Offset_ + Length_);
    StringData_->terminate();

    return *this;
}

StringRef& StringRef::operator+=(CharType c)
{
    if (!StringData_)
        return *this;

    ensureUnique();

    SizeType total_len = Offset_ + Length_;

    // Ensure capacity (need space for char + null)
    if (total_len + 1 >= StringData_->cap())
        expand(Length_ + 1);

    CharType* ptr = StringData_->ptr();

    // Append character at end of our view
    ptr[Offset_ + Length_] = c;
    Length_++;

    // Update underlying data length
    StringData_->setLen(Offset_ + Length_);
    StringData_->terminate();

    return *this;
}

// Bounds-checked access (const)
CharType StringRef::operator[](SizeType const i) const
{
    if (!StringData_ || !data())
        throw std::runtime_error("StringRef::operator[]: null string data");
    if (i >= Length_)
        throw std::out_of_range("StringRef::operator[]: index out of bounds");
    return (*StringData_)[i + Offset_];
}

// Bounds-checked access (non-const)
CharType& StringRef::operator[](SizeType const i)
{
    ensureUnique();
    if (!StringData_ || !data())
        throw std::runtime_error("StringRef::operator[]: null string data");
    if (i >= Length_)
        throw std::out_of_range("StringRef::operator[]: index out of bounds");
    return (*StringData_)[i + Offset_];
}

// Safe access with bounds checking (always checked)
MYLANG_NODISCARD CharType StringRef::at(SizeType const i) const
{
    if (!StringData_ || !data())
        throw std::runtime_error("StringRef::at: null string data");
    if (i >= Length_)
        throw std::out_of_range("StringRef::at: index out of bounds");
    return (*StringData_)[i + Offset_];
}

CharType& StringRef::at(SizeType const i)
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

    // Optionally move offset to 0 for cleaner state
    // (useful if this ref will be reused for appending)
    if (StringData_ && StringData_->referenceCount() == 1)
        // Reset to beginning of buffer for better append performance
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
MYLANG_NODISCARD bool StringRef::find(CharType const c) const noexcept
{
    if (!StringData_ || !data())
        return false;

    CharType const* p = data();
    CharType const* end = data() + Length_;

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
        if (::memcmp(data() + i, s.data(), search_len * sizeof(CharType)) == 0)
            return true;
    }

    return false;
}

// Find position of a character - purely view-based
MYLANG_NODISCARD std::optional<SizeType> StringRef::find_pos(CharType const c) const noexcept
{
    if (!StringData_ || !data())
        return std::nullopt;

    CharType const* p = data();
    CharType const* end = data() + Length_;

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

// Substring - creates new StringData with actual copy
StringRef StringRef::substr(std::optional<SizeType> start, std::optional<SizeType> end) const
{
    if (empty())
        return StringRef {};

    SizeType start_val = start.value_or(0);
    SizeType end_val = end.value_or(len() - 1);

    if (start_val >= len())
        throw std::out_of_range("StringRef::substr: start index out of range");

    if (end_val >= len())
        end_val = len() - 1;

    if (end_val < start_val)
        throw std::invalid_argument("StringRef::substr: end must be >= start");

    SizeType ret_len = end_val - start_val + 1;
    String* ret_data = string_allocator.allocateObject<String>();

    if (ret_len < SSO_SIZE) {
        ret_data->is_heap = false;
        ::memcpy(ret_data->storage_.sso, data() + start_val, ret_len * sizeof(CharType));
    } else {
        ret_data->is_heap = true;
        ret_data->storage_.heap.cap = ret_len + 1;
        ret_data->storage_.heap.ptr = string_allocator.allocateArray<CharType>(ret_data->storage_.heap.cap);
        ::memcpy(ret_data->storage_.heap.ptr, data() + start_val, ret_len * sizeof(CharType));
    }

    ret_data->setLen(ret_len);
    ret_data->terminate();

    return StringRef(ret_data);
}

// Convert to double - purely view-based
double StringRef::toDouble(SizeType* pos) const
{
    if (empty() || len() == 0)
        throw std::invalid_argument("StringRef::toDouble: empty string");

    // Convert UTF-16 to UTF-8 for standard library parsing
    std::string utf8_str = toUtf8();

    // Use std::stod on the UTF-8 string
    std::size_t utf8_pos = 0;
    double result;

    try {
        result = std::stod(utf8_str, &utf8_pos);
    } catch (std::invalid_argument const&) {
        throw std::invalid_argument("StringRef::toDouble: invalid number format");
    } catch (std::out_of_range const&) {
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
        return "";

    CharType const* src = data();
    size_t src_len = len();

    // Calculate required UTF-8 buffer size
    size_t utf8_len = simdutf::utf8_length_from_utf16(reinterpret_cast<char16_t const*>(src), src_len);

    // Allocate output
    std::string out;
    out.resize(utf8_len);

    // Convert UTF-16 to UTF-8
    size_t written = simdutf::convert_utf16_to_utf8(reinterpret_cast<char16_t const*>(src), src_len, out.data());

    // Sanity check (should always match)
    assert(written == utf8_len);

    return out;
}

MYLANG_NODISCARD StringRef StringRef::fromUtf8(std::string const& utf8_str)
{
    if (utf8_str.empty())
        return "";

    char const* src = utf8_str.data();
    size_t src_len = utf8_str.size();

    // Validate and get exact UTF-16 length in one pass
    simdutf::result validation = simdutf::validate_utf8_with_errors(src, src_len);

    if (validation.error != simdutf::error_code::SUCCESS)
        throw std::runtime_error("Invalid UTF-8 at position " + std::to_string(validation.count));

    size_t utf16_len = simdutf::utf16_length_from_utf8(src, src_len);

    // Allocate String object
    String* ret_data = string_allocator.allocateObject<String>();
    CharType* dest;

    if (utf16_len < SSO_SIZE) {
        ret_data->is_heap = false;
        dest = ret_data->storage_.sso;
    } else {
        ret_data->is_heap = true;
        ret_data->storage_.heap.cap = utf16_len + 1;
        ret_data->storage_.heap.ptr = string_allocator.allocateArray<CharType>(utf16_len + 1);
        dest = ret_data->storage_.heap.ptr;
    }

    // Convert UTF-8 to UTF-16
    size_t written = simdutf::convert_utf8_to_utf16(src, src_len, reinterpret_cast<char16_t*>(dest));

    assert(written == utf16_len);

    ret_data->setLen(utf16_len);
    ret_data->terminate();
    return StringRef(ret_data);
}

// Convenience overload for C strings
MYLANG_NODISCARD StringRef StringRef::fromUtf8(char const* utf8_cstr)
{
    if (!utf8_cstr)
        return "";
    return fromUtf8(std::string(utf8_cstr));
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

    // Use constructor that handles refcount
    // The new slice will have Offset_ = this->Offset_ + start, Length_ = slice_len
    return StringRef(*this, start, slice_len);
}

StringRef& StringRef::trimWhitespace(std::optional<bool const> leading, std::optional<bool const> trailing)
{
    bool const trim_leading = leading.value_or(true);
    bool const trim_trailing = trailing.value_or(true);

    if (trim_leading) {
        while (Length_ > 0 && util::isWhitespace((*this)[0]))
            ++Offset_, --Length_;
    }

    if (trim_trailing) {
        while (Length_ > 0 && util::isWhitespace((*this)[Length_ - 1]))
            --Length_;
    }

    return *this;
}

}
