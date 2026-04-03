/// string.cc

#include "../include/string.hpp"
#include "../include/diagnostic.hpp"
#include "../include/util.hpp"

#include <cassert>
#include <charconv>
#include <simdutf.h>

namespace fairuz {

namespace detail {

static char g_empty_string_storage[sizeof(StringBase)];
static StringBase* g_empty_string = nullptr;

StringBase* empty_string_singleton() noexcept
{
    if (LIKELY(g_empty_string != nullptr))
        return g_empty_string;

    g_empty_string = new (g_empty_string_storage) StringBase();
    for (int i = 0; i < 1024; i += 1)
        g_empty_string->increment();

    return g_empty_string;
}

} // namespace detail

StringBase::StringBase(size_t const s)
    : m_is_heap(s >= SSO_SIZE)
{
    if (s < SSO_SIZE) {
        m_storage.sso[0] = 0;
    } else {
        m_storage.heap.m_cap = s + 1;
        m_storage.heap.ptr = get_allocator().allocate_array<char>(m_storage.heap.m_cap);
        m_storage.heap.ptr[0] = 0;
    }
}

StringBase::StringBase(size_t const s, char const c)
    : m_is_heap(s >= SSO_SIZE)
{
    if (s < SSO_SIZE) {
        ::memset(m_storage.sso, c, s);
        m_storage.sso[s] = 0;
    } else {
        m_storage.heap.m_cap = s + 1;
        m_storage.heap.ptr = get_allocator().allocate_array<char>(m_storage.heap.m_cap);
        ::memset(m_storage.heap.ptr, c, s);
        m_storage.heap.ptr[s] = 0;
    }
}

StringBase::StringBase(char const* s, size_t n)
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
        m_storage.heap.m_cap = n + 1;
        m_storage.heap.ptr = get_allocator().allocate_array<char>(m_storage.heap.m_cap);
        if (m_storage.heap.ptr == nullptr)
            diagnostic::panic("allocateArray<char>(size=" + std::to_string(m_storage.heap.m_cap) + ") failed!");
        ::memcpy(m_storage.heap.ptr, s, n);
        m_storage.heap.ptr[n] = 0;
    }
}

StringBase::StringBase(char const* s)
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
        m_storage.heap.m_cap = n + 1;
        m_storage.heap.ptr = get_allocator().allocate_array<char>(m_storage.heap.m_cap);
        if (m_storage.heap.ptr == nullptr)
            diagnostic::panic("allocateArray<char>(size=" + std::to_string(m_storage.heap.m_cap) + ") failed!");
        ::memcpy(m_storage.heap.ptr, s, n + 1);
    }
}

Fa_StringRef::Fa_StringRef(size_t const s)
    : m_string_data(get_allocator().allocate_object<StringBase>(s))
    , m_offset(0)
    , m_length(0)
{
}

Fa_StringRef::Fa_StringRef(Fa_StringRef const& other, size_t m_offset, size_t m_length)
    : m_string_data(other.m_string_data)
    , m_offset(other.m_offset + m_offset)
    , m_length(m_length ? m_length : (other.m_length > m_offset ? other.m_length - m_offset : 0))
{
    if (m_string_data)
        m_string_data->increment();
}

// relies on lit being nul terminated
Fa_StringRef::Fa_StringRef(char const* lit)
{
    if (lit == nullptr || !lit[0]) {
        m_string_data = detail::empty_string_singleton();
        m_string_data->increment();
        m_offset = 0;
        m_length = 0;
        return;
    }

    m_string_data = get_allocator().allocate_object<StringBase>(lit);
    m_offset = 0;
    m_length = ::strlen(lit);
}

Fa_StringRef::Fa_StringRef(char16_t const* u16_str)
{
    if (u16_str == nullptr || !u16_str[0]) {
        m_string_data = detail::empty_string_singleton();
        m_string_data->increment();
        m_offset = 0;
        m_length = 0;
        return;
    }

    Fa_StringRef temp = from_utf16(u16_str);
    m_string_data = temp.m_string_data;
    m_offset = temp.m_offset;
    m_length = temp.m_length;
    temp.m_string_data = nullptr;
}

Fa_StringRef::Fa_StringRef(size_t const s, char const c)
    : m_string_data(get_allocator().allocate_object<StringBase>(s, c))
    , m_offset(0)
    , m_length(s)
{
}

Fa_StringRef::Fa_StringRef(StringBase* data, size_t m_offset, size_t m_length)
    : m_string_data(data ? data : detail::empty_string_singleton())
    , m_offset(m_offset)
    , m_length(m_length)
{
    if (this->m_length == 0) {
        size_t const len = ::strlen(m_string_data->ptr());
        this->m_length = len > this->m_offset ? len - this->m_offset : 0;
    }

    m_string_data->increment();
}

Fa_StringRef::Fa_StringRef(Fa_StringRef&& other) noexcept
    : m_string_data(other.m_string_data)
    , m_offset(other.m_offset)
    , m_length(other.m_length)
{
    other.m_string_data = nullptr;
    other.m_offset = 0;
    other.m_length = 0;
}

Fa_StringRef& Fa_StringRef::operator=(Fa_StringRef&& other) noexcept
{
    if (this == &other)
        return *this;

    if (m_string_data) {
        m_string_data->decrement();
        if (m_string_data->reference_count() == 0) {
            m_string_data->~StringBase();
            get_allocator().deallocate_object<StringBase>(m_string_data);
        }
    }

    m_string_data = other.m_string_data;
    m_offset = other.m_offset;
    m_length = other.m_length;
    other.m_string_data = nullptr;
    other.m_offset = 0;
    other.m_length = 0;

    return *this;
}

Fa_StringRef::~Fa_StringRef()
{
    if (m_string_data == nullptr)
        return;

    m_string_data->decrement();

    if (m_string_data->reference_count() == 0) {
        m_string_data->~StringBase();
        get_allocator().deallocate_object<StringBase>(m_string_data);
        m_string_data = nullptr;
    }
}

Fa_StringRef& Fa_StringRef::operator=(Fa_StringRef const& other)
{
    if (this == &other)
        return *this;

    if (m_string_data)
        m_string_data->decrement();

    m_string_data = other.m_string_data;
    m_offset = other.m_offset;
    m_length = other.m_length;

    if (m_string_data != nullptr)
        m_string_data->increment();

    return *this;
}

void Fa_StringRef::expand(size_t const new_size)
{
    ensure_unique();

    if (new_size <= cap())
        return;

    char const* old_ptr = data();
    size_t old_len = len();

    size_t new_capacity = (cap() < 1024) ? std::max(new_size + 1, cap() * 2) : std::max(new_size + 1, cap() + cap() / 2);
    char* new_ptr = get_allocator().allocate_array<char>(new_capacity);

    if (old_ptr != nullptr && old_len > 0)
        ::memcpy(new_ptr, old_ptr, old_len);

    if (m_string_data->is_heap() && m_string_data->m_storage.heap.ptr)
        get_allocator().deallocate_array<char>(m_string_data->m_storage.heap.ptr, m_string_data->m_storage.heap.m_cap);

    m_string_data->m_storage.heap.ptr = new_ptr;
    m_string_data->m_storage.heap.m_cap = new_capacity;
    m_string_data->m_is_heap = true;
    m_string_data->ptr()[m_length] = 0;
    m_offset = 0;
}

void Fa_StringRef::reserve(size_t const new_capacity)
{
    if (new_capacity <= cap())
        return;

    expand(new_capacity);
}

void Fa_StringRef::erase(size_t const at)
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

Fa_StringRef& Fa_StringRef::operator+=(Fa_StringRef const& other)
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

Fa_StringRef& Fa_StringRef::operator+=(char c)
{
    ensure_unique();

    if (UNLIKELY(m_offset + m_length + 1 >= m_string_data->cap()))
        expand(m_length + 1);

    m_string_data->ptr()[m_offset + m_length] = c;
    m_length += 1;
    m_string_data->ptr()[m_offset + m_length] = 0;

    return *this;
}

char Fa_StringRef::operator[](size_t const i) const
{
    if (UNLIKELY(i >= m_length))
        throw std::out_of_range("Fa_StringRef::operator[]: index out of bounds");

    return (*m_string_data)[i + m_offset];
}

char& Fa_StringRef::operator[](size_t const i)
{
    ensure_unique();
    if (UNLIKELY(i >= m_length))
        throw std::out_of_range("Fa_StringRef::operator[]: index out of bounds");

    return (*m_string_data)[i + m_offset];
}

char Fa_StringRef::at(size_t const i) const
{
    if (UNLIKELY(i >= m_length))
        throw std::out_of_range("Fa_StringRef::at: index out of bounds");
    return (*m_string_data)[i + m_offset];
}

char& Fa_StringRef::at(size_t const i)
{
    ensure_unique();
    if (UNLIKELY(i >= m_length))
        throw std::out_of_range("Fa_StringRef::at: index out of bounds");

    return (*m_string_data)[i + m_offset];
}

bool Fa_StringRef::find(char const c) const noexcept
{
    char const* p = data();
    char const* m_end = p + m_length;
    while (p < m_end && *p != c)
        p += 1;
    return p < m_end;
}

bool Fa_StringRef::find(Fa_StringRef const& s) const noexcept
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

std::optional<size_t> Fa_StringRef::find_pos(char const c) const noexcept
{
    char const* p = data();
    char const* m_end = p + m_length;

    while (p < m_end && *p != c)
        p += 1;

    return (p < m_end) ? std::optional<size_t>(p - data()) : std::nullopt;
}

Fa_StringRef& Fa_StringRef::trim_whitespace(bool leading, bool trailing) noexcept
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

Fa_StringRef& Fa_StringRef::truncate(size_t const s) noexcept
{
    if (s < m_length)
        m_length = s;

    return *this;
}

void Fa_StringRef::resize(size_t const s)
{
    ensure_unique();
    if (s > cap())
        expand(s);
}

Fa_StringRef Fa_StringRef::slice(size_t start, size_t m_end) const
{
    if (m_length == 0)
        return { };

    if (start > m_length) {
        diagnostic::emit(diagnostic::errc::m_container::Code::STRING_SLICE_START_OOB);
        return { };
    }

    if (m_end > m_length || m_end == SIZE_MAX)
        m_end = m_length;

    if (m_end < start) {
        diagnostic::emit(diagnostic::errc::m_container::Code::STRING_SLICE_END_BEFORE_START);
        return { };
    }

    return Fa_StringRef(*this, start, m_end - start);
}

Fa_StringRef Fa_StringRef::substr_copy(size_t start, size_t m_end) const
{
    if (m_length == 0)
        return { };
    if (start > m_length)
        throw std::out_of_range("Fa_StringRef::substrCopy: start index out of range");
    if (m_end > m_length || m_end == SIZE_MAX)
        m_end = m_length;
    if (m_end < start)
        throw std::invalid_argument("Fa_StringRef::substrCopy: end must be >= start");

    size_t copy_len = m_end - start;

    if (copy_len == 0)
        return { };

    StringBase* ret = get_allocator().allocate_object<StringBase>(copy_len);

    ::memcpy(ret->ptr(), data() + start, copy_len);
    ret->ptr()[copy_len] = 0;

    return Fa_StringRef(ret);
}

f64 Fa_StringRef::to_double(size_t* pos) const
{
    if (empty())
        throw std::invalid_argument("Fa_StringRef::toDouble: empty string");

    f64 result { };
    auto [end_ptr, ec] = std::from_chars(data(), data() + m_length, result);

    if (ec == std::errc::invalid_argument)
        throw std::invalid_argument("Fa_StringRef::toDouble: invalid number format");
    if (ec == std::errc::result_out_of_range)
        throw std::out_of_range("Fa_StringRef::toDouble: number out of range");
    if (pos)
        *pos = static_cast<size_t>(end_ptr - data());

    return result;
}

Fa_StringRef Fa_StringRef::from_utf16(char16_t const* src)
{
    if (src == nullptr || !src[0])
        return { };

    char16_t const* p = src;
    while (*p)
        p += 1;
    auto src_len = static_cast<size_t>(p - src);

    simdutf::result validation = simdutf::validate_utf16_with_errors(src, src_len);
    if (validation.error != simdutf::error_code::SUCCESS)
        throw std::runtime_error("Invalid UTF-16 at code unit " + std::to_string(validation.count));

    size_t utf8_len = simdutf::utf8_length_from_utf16(src, src_len);
    StringBase* ret_data = get_allocator().allocate_object<StringBase>(utf8_len);
    char* dest = ret_data->ptr();
    size_t written = simdutf::convert_utf16_to_utf8(src, src_len, dest);
    assert(written == utf8_len);
    ret_data->ptr()[utf8_len] = 0;

    return Fa_StringRef(ret_data);
}

void Fa_StringRef::detach()
{
    size_t const len = ::strlen(m_string_data->ptr());
    size_t const copy_len = m_length > 0 ? m_length : (len - m_offset);
    StringBase* s = get_allocator().allocate_object<StringBase>(copy_len);

    if (copy_len > 0)
        ::memcpy(s->ptr(), m_string_data->ptr() + m_offset, copy_len);

    s->ptr()[copy_len] = 0;
    m_string_data->decrement();
    m_string_data = s;
    m_offset = 0;
    m_length = copy_len;
}

} // namespace fairuz
