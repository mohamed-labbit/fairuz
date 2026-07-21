#ifndef STRING_HPP
#define STRING_HPP

#include "arena.hpp"
#include "diagnostic.hpp"

#include <ostream>

namespace fairuz {

template<class Allocator>
class Fa_StringRefImpl;

template<class Allocator = Fa_ArenaAllocator>
class StringBase {
    template<class>
    friend class Fa_StringRefImpl;

private:
    union Storage {
        struct {
            char* ptr { nullptr };
            size_t cap { 0 };
        } heap;

        char sso[SSO_SIZE];

        Storage()
            : heap { nullptr, 0 }
        {
        }
    } m_storage;

    bool m_is_heap;
    Allocator* m_allocator { nullptr };

    mutable u32 ref_count { 1 };

    // Fa_ArenaAllocator uses the global arena; other allocator types fall
    // back to one process-local singleton per allocator instantiation.
    static Allocator* resolve_allocator(Allocator* allocator)
    {
        if (allocator != nullptr)
            return allocator;

        if constexpr (std::is_same_v<Allocator, Fa_ArenaAllocator>) {
            return &get_allocator();
        } else {
            static Allocator default_allocator;
            return &default_allocator;
        }
    }

public:
    StringBase()
        : m_is_heap { false }
        , m_allocator(resolve_allocator(nullptr))
    {
        m_storage.sso[0] = 0;
    }

    ~StringBase()
    {
        if (is_heap())
            m_allocator->template deallocate_array<char>(m_storage.heap.ptr, m_storage.heap.cap);
    }

    StringBase(StringBase const&) = delete;
    StringBase& operator=(StringBase const&) = delete;

    explicit StringBase(size_t const s, Allocator* allocator = nullptr);

    explicit StringBase(size_t const s, char const c, Allocator* allocator = nullptr);

    explicit StringBase(char const* s, size_t n, Allocator* allocator = nullptr);

    StringBase(char const* s, Allocator* allocator = nullptr);

    static constexpr size_t k_sso_size = SSO_SIZE;

    bool is_heap() const noexcept { return m_is_heap; }

    Allocator* allocator() const noexcept { return m_allocator; }

    char* ptr() noexcept { return UNLIKELY(is_heap()) ? m_storage.heap.ptr : m_storage.sso; }

    char const* ptr() const noexcept { return UNLIKELY(is_heap()) ? m_storage.heap.ptr : m_storage.sso; }

    size_t cap() const noexcept { return UNLIKELY(is_heap()) ? (m_storage.heap.cap > 0 ? m_storage.heap.cap - 1 : 0) : SSO_SIZE - 1; }

    char operator[](size_t const i) const noexcept { return ptr()[i]; }
    char& operator[](size_t const i) noexcept { return ptr()[i]; }

    void increment() const noexcept { ref_count += 1; }
    void decrement() const noexcept { ref_count -= 1; }

    u32 reference_count() const noexcept { return ref_count; }
}; // class StringBase

namespace detail {

// One singleton per Allocator instantiation.
template<class Allocator>
StringBase<Allocator>* empty_string_singleton() noexcept;

} // namespace detail

template<class Allocator = Fa_ArenaAllocator>
class Fa_StringRefImpl {
private:
    StringBase<Allocator>* m_string_data { nullptr };
    size_t m_offset { 0 };
    size_t m_length { 0 };
    Allocator* m_allocator { nullptr };

    // Same rule as StringBase::resolve_allocator.
    static Allocator* resolve_allocator(Allocator* allocator)
    {
        if (allocator != nullptr)
            return allocator;

        if constexpr (std::is_same_v<Allocator, Fa_ArenaAllocator>) {
            return &get_allocator();
        } else {
            static Allocator default_allocator;
            return &default_allocator;
        }
    }

public:
    // Default: points at the global empty singleton — zero heap allocation.
    // Uses the allocator fallback selected by resolve_allocator().
    Fa_StringRefImpl() noexcept
        : m_string_data(detail::empty_string_singleton<Allocator>())
        , m_offset(0)
        , m_length(0)
        , m_allocator(resolve_allocator(nullptr))
    {
        m_string_data->increment();
    }

    explicit Fa_StringRefImpl(size_t const s, Allocator* allocator = nullptr);

    Fa_StringRefImpl(Fa_StringRefImpl const& other, size_t offset = 0, size_t length = 0);

    Fa_StringRefImpl(char const* lit, Allocator* allocator = nullptr);

    Fa_StringRefImpl(char16_t const* u16_str, Allocator* allocator = nullptr);

    Fa_StringRefImpl(size_t const s, char const c, Allocator* allocator = nullptr);

    explicit Fa_StringRefImpl(StringBase<Allocator>* data, size_t offset = 0, size_t length = 0);

    Fa_StringRefImpl(Fa_StringRefImpl&& other) noexcept;

    Fa_StringRefImpl& operator=(Fa_StringRefImpl&& other) noexcept;

    ~Fa_StringRefImpl();

    Fa_StringRefImpl& operator=(Fa_StringRefImpl const& other);

    Allocator* allocator() const noexcept { return m_allocator; }

    [[nodiscard]] bool operator==(Fa_StringRefImpl const& other) const noexcept
    {
        if (m_string_data == other.m_string_data && m_offset == other.m_offset && m_length == other.m_length)
            return true;
        if (m_length != other.m_length)
            return false;
        if (m_string_data == nullptr || !other.m_string_data)
            return m_length == 0;
        return ::memcmp(data(), other.data(), m_length) == 0;
    }

    [[nodiscard]] bool operator!=(Fa_StringRefImpl const& other) const noexcept { return !(*this == other); }

    [[nodiscard]] bool operator<(Fa_StringRefImpl const& other) const noexcept
    {
        if (m_string_data == other.m_string_data && m_offset == other.m_offset && m_length == other.m_length)
            return false;
        if (m_string_data == nullptr)
            return other.m_length > 0;
        if (other.m_string_data == nullptr)
            return false;

        size_t const min_len = std::min(m_length, other.m_length);
        int const cmp = ::memcmp(data(), other.data(), min_len);
        return cmp != 0 ? cmp < 0 : m_length < other.m_length;
    }

    [[nodiscard]] bool operator>(Fa_StringRefImpl const& other) const noexcept { return other < *this; }
    [[nodiscard]] bool operator<=(Fa_StringRefImpl const& other) const noexcept { return !(*this > other); }
    [[nodiscard]] bool operator>=(Fa_StringRefImpl const& other) const noexcept { return !(*this < other); }

    void expand(size_t const new_size);
    void reserve(size_t const new_capacity);
    void erase(size_t const at);

    Fa_StringRefImpl& operator+=(Fa_StringRefImpl const& other);
    Fa_StringRefImpl& operator+=(char c);

    char operator[](size_t const i) const;
    char& operator[](size_t const i);

    [[nodiscard]] char at(size_t const i) const;
    char& at(size_t const i);

    Fa_StringRefImpl& trim_whitespace(bool leading = true, bool trailing = true) noexcept;

    Fa_StringRefImpl operator+(Fa_StringRefImpl const& rhs) const
    {
        size_t const r_len = rhs.len();
        if (empty())
            return Fa_StringRefImpl(rhs);
        if (r_len == 0)
            return *this;

        size_t const new_len = m_length + r_len;
        StringBase<Allocator>* result = m_allocator->template allocate_object<StringBase<Allocator>>(new_len, m_allocator);

        ::memcpy(result->ptr(), data(), m_length);
        ::memcpy(result->ptr() + m_length, rhs.data(), r_len);

        result->ptr()[new_len] = 0;

        return Fa_StringRefImpl(result);
    }

    Fa_StringRefImpl operator+(char const* rhs) const
    {
        if (rhs == nullptr || !rhs[0])
            return *this;
        Fa_StringRefImpl rhs_str(rhs, m_allocator);
        return *this + rhs_str;
    }

    friend Fa_StringRefImpl operator+(char const* lhs, Fa_StringRefImpl const& rhs)
    {
        if (lhs == nullptr || !lhs[0])
            return Fa_StringRefImpl(rhs);
        Fa_StringRefImpl lhs_str(lhs, rhs.m_allocator);
        return lhs_str + rhs;
    }

    friend Fa_StringRefImpl operator+(Fa_StringRefImpl const& lhs, char rhs)
    {
        Fa_StringRefImpl result(lhs);
        result += rhs;
        return result;
    }

    friend Fa_StringRefImpl operator+(char lhs, Fa_StringRefImpl const& rhs)
    {
        Fa_StringRefImpl result(static_cast<size_t>(0), rhs.m_allocator);
        result.reserve(rhs.len() + 1);
        result += lhs;
        result += rhs;
        return result;
    }

    [[nodiscard]] size_t len() const noexcept { return m_length; }
    [[nodiscard]] size_t cap() const noexcept { return m_string_data ? m_string_data->cap() : 0; }
    [[nodiscard]] StringBase<Allocator>* get() const noexcept { return m_string_data; }
    [[nodiscard]] bool empty() const noexcept { return m_length == 0; }
    [[nodiscard]] char const* data() const noexcept { return m_string_data ? m_string_data->ptr() + m_offset : nullptr; }

    [[nodiscard]] char* data() noexcept
    {
        if (m_string_data != nullptr) {
            ensure_unique();
            return m_string_data->ptr() + m_offset;
        }
        return nullptr;
    }

    friend std::ostream& operator<<(std::ostream& os, Fa_StringRefImpl const& str)
    {
        if (!str.empty())
            os.write(str.data(), static_cast<std::streamsize>(str.len()));

        return os;
    }

    void clear() noexcept
    {
        m_length = 0;
        m_offset = 0;
    }

    void resize(size_t const s);

    [[nodiscard]] bool find(char const c) const noexcept;
    [[nodiscard]] bool find(Fa_StringRefImpl const& s) const noexcept;

    [[nodiscard]] std::optional<size_t> find_pos(char const c) const noexcept;

    Fa_StringRefImpl& truncate(size_t const s) noexcept;
    Fa_StringRefImpl slice(size_t start, size_t m_end = SIZE_MAX) const;
    Fa_StringRefImpl substr(size_t start, size_t end = SIZE_MAX) const
    {
        if (m_length == 0)
            return Fa_StringRefImpl(static_cast<size_t>(0), m_allocator);
        if (start > m_length)
            return Fa_StringRefImpl(static_cast<size_t>(0), m_allocator);
        if (end > m_length || end == SIZE_MAX)
            end = m_length;
        if (end < start)
            return Fa_StringRefImpl(static_cast<size_t>(0), m_allocator);
        return substr_copy(start, end);
    }
    Fa_StringRefImpl substr_copy(size_t start, size_t m_end = SIZE_MAX) const;

    f64 to_double(size_t* pos = nullptr) const;

    [[nodiscard]] static Fa_StringRefImpl from_utf16(char16_t const* utf16_cstr, Allocator* allocator = nullptr);

    void ensure_unique()
    {
        if (m_string_data == nullptr)
            return;
        if (LIKELY(m_string_data->reference_count() == 1))
            return;
        detach();
    }

    void detach();
}; // class Fa_StringRefImpl

namespace detail {

inline u64 wy_read8(void const* p) noexcept
{
    u64 v;
    __builtin_memcpy(&v, p, 8);
    return v;
}
inline u32 wy_read4(void const* p) noexcept
{
    u32 v;
    __builtin_memcpy(&v, p, 4);
    return v;
}

inline void wymix(u64& a, u64& b) noexcept
{
#if defined(__SIZEOF_INT128__)
    auto r = static_cast<__uint128_t>(a) * b;
    a = static_cast<u64>(r);
    b = static_cast<u64>(r >> 64);
#else
    u64 ha = a >> 32, hb = b >> 32;
    u64 la = static_cast<u32>(a), lb = static_cast<u32>(b);
    u64 hh = ha * hb, hl = ha * lb, lh = la * hb, ll = la * lb;
    u64 m = (ll >> 32) + static_cast<u32>(hl) + static_cast<u32>(lh);
    a = (hl >> 32) + (lh >> 32) + (hh) + (m >> 32);
    b = ll + (m << 32);
#endif
    a ^= b;
}

static constexpr u64 WY_P0 = UINT64_C(0xa0761d6478bd642f);
static constexpr u64 WY_P1 = UINT64_C(0xe7037ed1a0b428db);
static constexpr u64 WY_P2 = UINT64_C(0x8ebc6af09c88c6e3);
static constexpr u64 WY_P3 = UINT64_C(0x589965cc75374cc3);

inline u64 wyhash(void const* key, size_t len, u64 seed) noexcept
{
    auto p = static_cast<u8 const*>(key);
    seed ^= WY_P0;

    u64 a = 0, b = 0;

    if (len <= 16) {
        if (len >= 4) {
            a = (static_cast<u64>(wy_read4(p)) << 32)
                | wy_read4(p + ((len >> 3) << 2));
            b = (static_cast<u64>(wy_read4(p + len - 4)) << 32)
                | wy_read4(p + len - 4 - ((len >> 3) << 2));
        } else if (len > 0) {
            a = (static_cast<u64>(p[0]) << 16)
                | (static_cast<u64>(p[len >> 1]) << 8)
                | p[len - 1];
            b = 0;
        }
    } else {
        size_t i = len;
        if (i > 48) {
            u64 see1 = seed, see2 = seed;
            do {
                seed ^= WY_P1;
                a = wy_read8(p);
                b = wy_read8(p + 8);
                wymix(a, b);
                seed ^= b;
                see1 ^= WY_P2;
                a = wy_read8(p + 16);
                b = wy_read8(p + 24);
                wymix(a, b);
                see1 ^= b;
                see2 ^= WY_P3;
                a = wy_read8(p + 32);
                b = wy_read8(p + 40);
                wymix(a, b);
                see2 ^= b;
                p += 48;
                i -= 48;
            } while (i > 48);
            seed ^= see1 ^ see2;
        }
        while (i > 16) {
            seed ^= WY_P1;
            a = wy_read8(p);
            b = wy_read8(p + 8);
            wymix(a, b);
            seed ^= b;
            p += 16;
            i -= 16;
        }
        a = wy_read8(p + i - 16);
        b = wy_read8(p + i - 8);
    }

    a ^= WY_P1;
    b ^= seed;
    wymix(a, b);
    a ^= WY_P0 ^ len;
    b ^= WY_P1;
    wymix(a, b);

    return a ^ b;
}

} // namespace detail

template<class Allocator = Fa_ArenaAllocator>
struct Fa_StringRefHashImpl {
    u64 seed { 0 };

    size_t operator()(Fa_StringRefImpl<Allocator> const& str) const noexcept
    {
        if (str.empty())
            return 0;

        size_t n = str.len();
        auto const* p = reinterpret_cast<u8 const*>(str.data());

        if (n <= 16) {
            u64 a = 0, b = 0;
            if (n >= 8) {
                __builtin_memcpy(&a, p, 8);
                __builtin_memcpy(&b, p + n - 8, 8);
            } else if (n >= 4) {
                __builtin_memcpy(&a, p, 4);
                __builtin_memcpy(&b, p + n - 4, 4);
            } else {
                a = (u64(p[0]) << 16) | (u64(p[n >> 1]) << 8) | p[n - 1];
            }
            a ^= b ^ n;
            a ^= a >> 33;
            a *= UINT64_C(0xff51afd7ed558ccd);
            a ^= a >> 33;
            a *= UINT64_C(0xc4ceb9fe1a85ec53);
            a ^= a >> 33;
            return static_cast<size_t>(a);
        }

        return static_cast<size_t>(detail::wyhash(p, n, seed));
    }
}; // struct Fa_StringRefHash

template<class Allocator = Fa_ArenaAllocator>
struct Fa_StringRefEqualImpl {
    bool operator()(Fa_StringRefImpl<Allocator> const& lhs, Fa_StringRefImpl<Allocator> const& rhs) const noexcept
    {
        return lhs == rhs;
    }
}; // struct Fa_StringRefEqual

using Fa_StringRef = Fa_StringRefImpl<>;
using Fa_StringRefHash = Fa_StringRefHashImpl<>;
using Fa_StringRefEqual = Fa_StringRefEqualImpl<>;

} // namespace fairuz

namespace std {

template<class Allocator>
struct hash<fairuz::Fa_StringRefImpl<Allocator>> {
    size_t operator()(fairuz::Fa_StringRefImpl<Allocator> const& str) const noexcept
    {
        return fairuz::Fa_StringRefHashImpl<Allocator> { }(str);
    }
}; // struct hash

} // namespace std

#include "string.tpp"

#endif // STRING_HPP
