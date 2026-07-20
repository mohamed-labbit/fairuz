/// string.tpp

#include "diagnostic.hpp"
#include "util.hpp"

#include <cassert>
#include <charconv>
#include <simdutf.h>

namespace fairuz {

using ErrorCode = diagnostic::errc::container::Code;

template<class Allocator>
StringBase<Allocator>::StringBase(size_t const s, Allocator* allocator)
    : m_is_heap(s >= SSO_SIZE)
    , m_allocator(resolve_allocator(allocator))
{
    if (s < SSO_SIZE) {
        m_storage.sso[0] = 0;
    } else {
        m_storage.heap.cap = s + 1;
        m_storage.heap.ptr = m_allocator->template allocate_array<char>(m_storage.heap.cap);
        m_storage.heap.ptr[0] = 0;
    }
}

template<class Allocator>
StringBase<Allocator>::StringBase(size_t const s, char const c, Allocator* allocator)
    : m_is_heap(s >= SSO_SIZE)
    , m_allocator(resolve_allocator(allocator))
{
    if (s < SSO_SIZE) {
        ::memset(m_storage.sso, c, s);
        m_storage.sso[s] = 0;
    } else {
        m_storage.heap.cap = s + 1;
        m_storage.heap.ptr = m_allocator->template allocate_array<char>(m_storage.heap.cap);
        ::memset(m_storage.heap.ptr, c, s);
        m_storage.heap.ptr[s] = 0;
    }
}

template<class Allocator>
StringBase<Allocator>::StringBase(char const* s, size_t n, Allocator* allocator)
    : m_allocator(resolve_allocator(allocator))
{
    if (s == nullptr || n == 0) {
        m_is_heap = false;
        m_storage.sso[0] = 0;
        return;
    }

    m_is_heap = (n >= SSO_SIZE);

    if (!m_is_heap) {
        ::memcpy(m_storage.sso, s, n);
        m_storage.sso[n] = 0;
    } else {
        m_storage.heap.cap = n + 1;
        m_storage.heap.ptr = m_allocator->template allocate_array<char>(m_storage.heap.cap);
        if (m_storage.heap.ptr == nullptr)
            diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED,
                "allocateArray<char>(size=" + std::to_string(m_storage.heap.cap) + ") failed!");
        ::memcpy(m_storage.heap.ptr, s, n);
        m_storage.heap.ptr[n] = 0;
    }
}

template<class Allocator>
StringBase<Allocator>::StringBase(char const* s, Allocator* allocator)
    : m_allocator(resolve_allocator(allocator))
{
    if (s == nullptr) {
        m_is_heap = false;
        m_storage.sso[0] = 0;
        return;
    }

    size_t n = ::strlen(s);
    m_is_heap = (n >= SSO_SIZE);

    if (!m_is_heap) {
        ::memcpy(m_storage.sso, s, n + 1);
    } else {
        m_storage.heap.cap = n + 1;
        m_storage.heap.ptr = m_allocator->template allocate_array<char>(m_storage.heap.cap);
        if (m_storage.heap.ptr == nullptr)
            diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED,
                "allocateArray<char>(size=" + std::to_string(m_storage.heap.cap) + ") failed!");
        ::memcpy(m_storage.heap.ptr, s, n + 1);
    }
}

template<class Allocator>
Fa_StringRefImpl<Allocator>::Fa_StringRefImpl(size_t const s, Allocator* allocator)
    : m_allocator(resolve_allocator(allocator))
    , m_offset(0)
    , m_length(0)
{
    m_string_data = m_allocator->template allocate_object<StringBase<Allocator>>(s, m_allocator);
}

template<class Allocator>
Fa_StringRefImpl<Allocator>::Fa_StringRefImpl(Fa_StringRefImpl const& other, size_t offset, size_t length)
    : m_string_data(other.m_string_data)
    , m_offset(other.m_offset + offset)
    , m_length(length ? length : (offset <= other.m_length ? other.m_length - offset : 0))
    , m_allocator(other.m_allocator)
{
    if (m_string_data)
        m_string_data->increment();
}

// relies on lit being nul terminated
template<class Allocator>
Fa_StringRefImpl<Allocator>::Fa_StringRefImpl(char const* lit, Allocator* allocator)
    : m_allocator(resolve_allocator(allocator))
{
    if (lit == nullptr || !lit[0]) {
        m_string_data = detail::empty_string_singleton<Allocator>();
        m_string_data->increment();
        m_offset = 0;
        m_length = 0;
        return;
    }

    m_string_data = m_allocator->template allocate_object<StringBase<Allocator>>(lit, m_allocator);
    m_offset = 0;
    m_length = ::strlen(lit);
}

template<class Allocator>
Fa_StringRefImpl<Allocator>::Fa_StringRefImpl(char16_t const* u16_str, Allocator* allocator)
    : m_allocator(resolve_allocator(allocator))
{
    if (u16_str == nullptr || !u16_str[0]) {
        m_string_data = detail::empty_string_singleton<Allocator>();
        m_string_data->increment();
        m_offset = 0;
        m_length = 0;
        return;
    }

    Fa_StringRefImpl temp = from_utf16(u16_str, m_allocator);
    m_string_data = temp.m_string_data;
    m_offset = temp.m_offset;
    m_length = temp.m_length;
    temp.m_string_data = nullptr;
}

template<class Allocator>
Fa_StringRefImpl<Allocator>::Fa_StringRefImpl(size_t const s, char const c, Allocator* allocator)
    : m_allocator(resolve_allocator(allocator))
    , m_offset(0)
    , m_length(s)
{
    m_string_data = m_allocator->template allocate_object<StringBase<Allocator>>(s, c, m_allocator);
}

template<class Allocator>
Fa_StringRefImpl<Allocator>::Fa_StringRefImpl(StringBase<Allocator>* data, size_t offset, size_t length)
    : m_offset(offset)
    , m_length(length)
{
    if (data != nullptr) {
        m_string_data = data;
        m_allocator = data->allocator();
    } else {
        m_string_data = detail::empty_string_singleton<Allocator>();
        m_allocator = resolve_allocator(nullptr);
    }

    if (this->m_length == 0) {
        size_t const len = ::strlen(m_string_data->ptr());
        this->m_length = len > this->m_offset ? len - this->m_offset : 0;
    }

    m_string_data->increment();
}

template<class Allocator>
Fa_StringRefImpl<Allocator>::Fa_StringRefImpl(Fa_StringRefImpl&& other) noexcept
    : m_string_data(other.m_string_data)
    , m_offset(other.m_offset)
    , m_length(other.m_length)
    , m_allocator(other.m_allocator)
{
    other.m_string_data = nullptr;
    other.m_offset = 0;
    other.m_length = 0;
    other.m_allocator = nullptr;
}

template<class Allocator>
Fa_StringRefImpl<Allocator>& Fa_StringRefImpl<Allocator>::operator=(Fa_StringRefImpl&& other) noexcept
{
    if (this == &other)
        return *this;

    if (m_string_data) {
        m_string_data->decrement();
        if (m_string_data->reference_count() == 0) {
            Allocator* freeing_allocator = m_string_data->allocator();
            m_string_data->~StringBase();
            freeing_allocator->template deallocate_object<StringBase<Allocator>>(m_string_data);
        }
    }

    m_string_data = other.m_string_data;
    m_offset = other.m_offset;
    m_length = other.m_length;
    m_allocator = other.m_allocator;
    other.m_string_data = nullptr;
    other.m_offset = 0;
    other.m_length = 0;
    other.m_allocator = nullptr;

    return *this;
}

template<class Allocator>
Fa_StringRefImpl<Allocator>::~Fa_StringRefImpl()
{
    if (m_string_data == nullptr)
        return;

    m_string_data->decrement();

    if (m_string_data->reference_count() == 0) {
        Allocator* freeing_allocator = m_string_data->allocator();
        m_string_data->~StringBase();
        freeing_allocator->template deallocate_object<StringBase<Allocator>>(m_string_data);
        m_string_data = nullptr;
    }
}

template<class Allocator>
Fa_StringRefImpl<Allocator>& Fa_StringRefImpl<Allocator>::operator=(Fa_StringRefImpl const& other)
{
    if (this == &other)
        return *this;

    if (m_string_data)
        m_string_data->decrement();

    m_string_data = other.m_string_data;
    m_offset = other.m_offset;
    m_length = other.m_length;
    m_allocator = other.m_allocator;

    if (m_string_data != nullptr)
        m_string_data->increment();

    return *this;
}

template<class Allocator>
void Fa_StringRefImpl<Allocator>::expand(size_t const new_size)
{
    ensure_unique();

    if (new_size <= cap())
        return;

    char const* old_ptr = data();
    size_t old_len = len();

    size_t new_capacity = (cap() < 1024) ? std::max(new_size + 1, cap() * 2) : std::max(new_size + 1, cap() + cap() / 2);
    char* new_ptr = m_allocator->template allocate_array<char>(new_capacity);

    if (old_ptr != nullptr && old_len > 0)
        ::memcpy(new_ptr, old_ptr, old_len);

    if (m_string_data->is_heap() && m_string_data->m_storage.heap.ptr)
        m_allocator->template deallocate_array<char>(m_string_data->m_storage.heap.ptr, m_string_data->m_storage.heap.cap);

    m_string_data->m_storage.heap.ptr = new_ptr;
    m_string_data->m_storage.heap.cap = new_capacity;
    m_string_data->m_is_heap = true;
    m_string_data->ptr()[m_length] = 0;
    m_offset = 0;
}

template<class Allocator>
void Fa_StringRefImpl<Allocator>::reserve(size_t const new_capacity)
{
    if (new_capacity <= cap())
        return;

    expand(new_capacity);
}

template<class Allocator>
void Fa_StringRefImpl<Allocator>::erase(size_t const at)
{
    if (empty() || at >= len())
        return;

    if (at == 0) {
        m_offset += 1;
        m_length -= 1;
        return;
    }

    if (at == m_length - 1) {
        m_length -= 1;
        return;
    }

    ensure_unique();
    if (data() == nullptr)
        return;

    ::memmove(data() + at, data() + at + 1, len() - at - 1);
    m_length -= 1;
    m_string_data->ptr()[m_length] = 0;
}

template<class Allocator>
Fa_StringRefImpl<Allocator>& Fa_StringRefImpl<Allocator>::operator+=(Fa_StringRefImpl const& other)
{
    if (other.empty())
        return *this;

    ensure_unique();

    size_t new_len = m_length + other.m_length;
    if (m_offset + new_len >= cap())
        expand(new_len);

    ::memcpy(m_string_data->ptr() + m_offset + m_length, other.data(), other.m_length);
    m_length = new_len;
    m_string_data->ptr()[m_length] = 0;

    return *this;
}

template<class Allocator>
Fa_StringRefImpl<Allocator>& Fa_StringRefImpl<Allocator>::operator+=(char c)
{
    ensure_unique();

    if (UNLIKELY(m_offset + m_length + 1 >= m_string_data->cap()))
        expand(m_length + 1);

    m_string_data->ptr()[m_offset + m_length] = c;
    m_length += 1;
    m_string_data->ptr()[m_offset + m_length] = 0;

    return *this;
}

template<class Allocator>
char Fa_StringRefImpl<Allocator>::operator[](size_t const i) const
{
    return (*m_string_data)[i + m_offset];
}

template<class Allocator>
char& Fa_StringRefImpl<Allocator>::operator[](size_t const i)
{
    ensure_unique();
    return (*m_string_data)[i + m_offset];
}

template<class Allocator>
char Fa_StringRefImpl<Allocator>::at(size_t const i) const
{
    if (UNLIKELY(i >= m_length))
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR,
            "Fa_StringRefImpl::at: index out of bounds", diagnostic::Severity::FATAL);

    return (*m_string_data)[i + m_offset];
}

template<class Allocator>
char& Fa_StringRefImpl<Allocator>::at(size_t const i)
{
    ensure_unique();
    if (UNLIKELY(i >= m_length))
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR,
            "Fa_StringRefImpl::at: index out of bounds", diagnostic::Severity::FATAL);

    return (*m_string_data)[i + m_offset];
}

template<class Allocator>
bool Fa_StringRefImpl<Allocator>::find(char const c) const noexcept
{
    char const* p = data();
    char const* end = p + m_length;
    while (p < end && *p != c)
        p += 1;
    return p < end;
}

template<class Allocator>
bool Fa_StringRefImpl<Allocator>::find(Fa_StringRefImpl const& s) const noexcept
{
    if (s.empty() || s.len() > len())
        return false;

    size_t search_len = s.len();
    size_t max_start = len() - search_len;

    for (size_t i = 0; i <= max_start; i += 1) {
        if (::memcmp(data() + i, s.data(), search_len) == 0)
            return true;
    }

    return false;
}

template<class Allocator>
std::optional<size_t> Fa_StringRefImpl<Allocator>::find_pos(char const c) const noexcept
{
    char const* p = data();
    char const* end = p + m_length;

    while (p < end && *p != c)
        p += 1;

    return (p < end) ? std::optional<size_t>(p - data()) : std::nullopt;
}

template<class Allocator>
Fa_StringRefImpl<Allocator>& Fa_StringRefImpl<Allocator>::trim_whitespace(bool leading, bool trailing) noexcept
{
    if (leading) {
        while (m_length > 0 && util::is_whitespace(data()[0])) {
            m_offset += 1;
            m_length -= 1;
        }
    }

    if (trailing) {
        while (m_length > 0 && util::is_whitespace(data()[m_length - 1]))
            m_length -= 1;
    }

    return *this;
}

template<class Allocator>
Fa_StringRefImpl<Allocator>& Fa_StringRefImpl<Allocator>::truncate(size_t const s) noexcept
{
    if (s < m_length)
        m_length = s;

    return *this;
}

template<class Allocator>
void Fa_StringRefImpl<Allocator>::resize(size_t const s)
{
    ensure_unique();
    if (s > cap())
        expand(s);
}

template<class Allocator>
Fa_StringRefImpl<Allocator> Fa_StringRefImpl<Allocator>::slice(size_t start, size_t end) const
{
    if (m_length == 0)
        return Fa_StringRefImpl(static_cast<size_t>(0), m_allocator);

    if (start > m_length) {
        diagnostic::emit(ErrorCode::STRING_SLICE_START_OOB);
        return Fa_StringRefImpl(static_cast<size_t>(0), m_allocator);
    }

    if (end > m_length || end == SIZE_MAX)
        end = m_length;

    if (end < start) {
        diagnostic::emit(ErrorCode::STRING_SLICE_END_BEFORE_START);
        return Fa_StringRefImpl(static_cast<size_t>(0), m_allocator);
    }

    return Fa_StringRefImpl(*this, start, end - start);
}

template<class Allocator>
Fa_StringRefImpl<Allocator> Fa_StringRefImpl<Allocator>::substr_copy(size_t start, size_t end) const
{
    if (m_length == 0)
        return Fa_StringRefImpl(static_cast<size_t>(0), m_allocator);
    if (start > m_length)
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR,
            "Fa_StringRefImpl::substrCopy: start index out of range", diagnostic::Severity::FATAL);

    if (end > m_length || end == SIZE_MAX)
        end = m_length;
    if (end < start)
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR,
            "Fa_StringRefImpl::substrCopy: start index out of range", diagnostic::Severity::FATAL);

    size_t copy_len = end - start;

    if (copy_len == 0)
        return Fa_StringRefImpl(static_cast<size_t>(0), m_allocator);

    StringBase<Allocator>* ret = m_allocator->template allocate_object<StringBase<Allocator>>(copy_len, m_allocator);

    ::memcpy(ret->ptr(), data() + start, copy_len);
    ret->ptr()[copy_len] = 0;

    return Fa_StringRefImpl(ret);
}

template<class Allocator>
f64 Fa_StringRefImpl<Allocator>::to_double(size_t* pos) const
{
    if (empty())
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR,
            "Fa_StringRefImpl::toDouble: empty string", diagnostic::Severity::FATAL);

    f64 result { };
    auto [end_ptr, ec] = std::from_chars(data(), data() + m_length, result);

    if (ec == std::errc::invalid_argument)
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR,
            "Fa_StringRefImpl::toDouble: invalid number format", diagnostic::Severity::FATAL);
    if (ec == std::errc::result_out_of_range)
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR,
            "Fa_StringRefImpl::toDouble: number out of range", diagnostic::Severity::FATAL);

    if (pos)
        *pos = static_cast<size_t>(end_ptr - data());

    return result;
}

template<class Allocator>
Fa_StringRefImpl<Allocator> Fa_StringRefImpl<Allocator>::from_utf16(char16_t const* src, Allocator* allocator)
{
    Allocator* used_allocator = resolve_allocator(allocator);

    if (src == nullptr || !src[0])
        return Fa_StringRefImpl(static_cast<size_t>(0), used_allocator);

    char16_t const* p = src;
    while (*p)
        p += 1;
    auto src_len = static_cast<size_t>(p - src);

    simdutf::result validation = simdutf::validate_utf16_with_errors(src, src_len);
    if (validation.error != simdutf::error_code::SUCCESS)
        diagnostic::emit(diagnostic::errc::general::Code::INTERNAL_ERROR,
            "Invalid UTF-16 at code unit " + std::to_string(validation.count), diagnostic::Severity::FATAL);

    size_t utf8_len = simdutf::utf8_length_from_utf16(src, src_len);
    StringBase<Allocator>* ret_data = used_allocator->template allocate_object<StringBase<Allocator>>(utf8_len, used_allocator);
    char* dest = ret_data->ptr();
    size_t written = simdutf::convert_utf16_to_utf8(src, src_len, dest);
    assert(written == utf8_len);
    ret_data->ptr()[utf8_len] = 0;

    return Fa_StringRefImpl(ret_data);
}

template<class Allocator>
void Fa_StringRefImpl<Allocator>::detach()
{
    size_t const len = ::strlen(m_string_data->ptr());
    size_t const copy_len = m_length > 0 ? m_length : (len - m_offset);
    StringBase<Allocator>* s = m_allocator->template allocate_object<StringBase<Allocator>>(copy_len, m_allocator);

    if (copy_len > 0)
        ::memcpy(s->ptr(), m_string_data->ptr() + m_offset, copy_len);

    s->ptr()[copy_len] = 0;
    m_string_data->decrement();
    m_string_data = s;
    m_offset = 0;
    m_length = copy_len;
}

namespace detail {

template<class Allocator>
StringBase<Allocator>* empty_string_singleton() noexcept
{
    static StringBase<Allocator> instance;
    return &instance;
}

}

} // namespace fairuz