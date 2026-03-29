#ifndef STRING_HPP
#define STRING_HPP

#include "arena.hpp"

#include <ostream>

namespace fairuz {

class StringBase {
    friend class Fa_StringRef;

private:
    union Storage {
        struct {
            char* ptr;
            size_t cap;
        } heap;

        char sso[SSO_SIZE];

        Storage()
            : heap { nullptr, 0 }
        {
        }
    } storage_;

    bool is_heap;

    mutable u32 RefCount { 1 };

public:
    StringBase()
        : is_heap { false }
    {
        storage_.sso[0] = 0;
    }

    ~StringBase()
    {
        if (isHeap())
            getAllocator().deallocateArray<char>(storage_.heap.ptr, storage_.heap.cap);
    }

    StringBase(StringBase const&) = delete;
    StringBase& operator=(StringBase const&) = delete;

    explicit StringBase(size_t const s);
    explicit StringBase(size_t const s, char const c);
    explicit StringBase(char const* s, size_t n);
    StringBase(char const* s);

    static constexpr size_t kSSOSize = SSO_SIZE;

    bool isHeap() const noexcept { return is_heap; }

    char* ptr() noexcept { return UNLIKELY(isHeap()) ? storage_.heap.ptr : storage_.sso; }

    char const* ptr() const noexcept { return UNLIKELY(isHeap()) ? storage_.heap.ptr : storage_.sso; }

    size_t cap() const noexcept { return UNLIKELY(isHeap()) ? (storage_.heap.cap > 0 ? storage_.heap.cap - 1 : 0) : SSO_SIZE - 1; }

    char operator[](size_t const i) const noexcept { return ptr()[i]; }
    char& operator[](size_t const i) noexcept { return ptr()[i]; }

    void increment() const noexcept { RefCount++; }
    void decrement() const noexcept { RefCount--; }

    u32 referenceCount() const noexcept { return RefCount; }
}; // class StringBase

namespace detail {

StringBase* emptyStringSingleton() noexcept;

} // namespace detail

class Fa_StringRef {
private:
    StringBase* StringData_ { nullptr };
    size_t Offset_ { 0 };
    size_t Length_ { 0 };

    static StringBase* createEmpty()
    {
        StringBase* s = getAllocator().allocateObject<StringBase>();
        return s;
    }

public:
    // Default: points at the global empty singleton — zero heap allocation.
    Fa_StringRef() noexcept
        : StringData_(detail::emptyStringSingleton())
        , Offset_(0)
        , Length_(0)
    {
        StringData_->increment();
    }

    explicit Fa_StringRef(size_t const s);

    Fa_StringRef(Fa_StringRef const& other, size_t offset = 0, size_t length = 0);

    Fa_StringRef(char const* lit);

    Fa_StringRef(char16_t const* u16_str);

    Fa_StringRef(size_t const s, char const c);

    explicit Fa_StringRef(StringBase* data, size_t offset = 0, size_t length = 0);

    Fa_StringRef(Fa_StringRef&& other) noexcept;

    Fa_StringRef& operator=(Fa_StringRef&& other) noexcept;

    ~Fa_StringRef();

    Fa_StringRef& operator=(Fa_StringRef const& other);

    [[nodiscard]] bool operator==(Fa_StringRef const& other) const noexcept
    {
        if (StringData_ == other.StringData_ && Offset_ == other.Offset_ && Length_ == other.Length_)
            return true;
        if (Length_ != other.Length_)
            return false;
        if (!StringData_ || !other.StringData_)
            return Length_ == 0;
        return ::memcmp(data(), other.data(), Length_) == 0;
    }

    [[nodiscard]] bool operator!=(Fa_StringRef const& other) const noexcept { return !(*this == other); }

    [[nodiscard]] bool operator<(Fa_StringRef const& other) const noexcept
    {
        if (StringData_ == other.StringData_ && Offset_ == other.Offset_ && Length_ == other.Length_)
            return false;
        if (!StringData_)
            return other.Length_ > 0;
        if (!other.StringData_)
            return false;

        size_t const minLen = std::min(Length_, other.Length_);
        int const cmp = ::memcmp(data(), other.data(), minLen);
        return cmp != 0 ? cmp < 0 : Length_ < other.Length_;
    }

    [[nodiscard]] bool operator>(Fa_StringRef const& other) const noexcept { return other < *this; }
    [[nodiscard]] bool operator<=(Fa_StringRef const& other) const noexcept { return !(*this > other); }
    [[nodiscard]] bool operator>=(Fa_StringRef const& other) const noexcept { return !(*this < other); }

    void expand(size_t const new_size);
    void reserve(size_t const new_capacity);
    void erase(size_t const at);

    Fa_StringRef& operator+=(Fa_StringRef const& other);
    Fa_StringRef& operator+=(char c);

    char operator[](size_t const i) const;
    char& operator[](size_t const i);

    [[nodiscard]] char at(size_t const i) const;
    char& at(size_t const i);

    Fa_StringRef& trimWhitespace(bool leading = true, bool trailing = true) noexcept;

    Fa_StringRef operator+(Fa_StringRef const& rhs) const
    {
        size_t const r_len = rhs.len();
        if (empty())
            return Fa_StringRef(rhs);
        if (r_len == 0)
            return *this;

        size_t const new_len = Length_ + r_len;
        StringBase* result = getAllocator().allocateObject<StringBase>(new_len);

        ::memcpy(result->ptr(), data(), Length_);
        ::memcpy(result->ptr() + Length_, rhs.data(), r_len);

        result->ptr()[new_len] = 0;

        return Fa_StringRef(result);
    }

    Fa_StringRef operator+(char const* rhs) const
    {
        if (!rhs || !rhs[0])
            return *this;
        Fa_StringRef rhs_str = rhs;
        return *this + rhs_str;
    }

    friend Fa_StringRef operator+(char const* lhs, Fa_StringRef const& rhs)
    {
        if (!lhs || !lhs[0])
            return Fa_StringRef(rhs);
        Fa_StringRef lhs_str = lhs;
        return lhs_str + rhs;
    }

    friend Fa_StringRef operator+(Fa_StringRef const& lhs, char rhs)
    {
        Fa_StringRef result(lhs);
        result += rhs;
        return result;
    }

    friend Fa_StringRef operator+(char lhs, Fa_StringRef const& rhs)
    {
        Fa_StringRef result;
        result.reserve(rhs.len() + 1);
        result += lhs;
        result += rhs;
        return result;
    }

    [[nodiscard]] size_t len() const noexcept { return Length_; }
    [[nodiscard]] size_t cap() const noexcept { return StringData_ ? StringData_->cap() : 0; }
    [[nodiscard]] StringBase* get() const noexcept { return StringData_; }
    [[nodiscard]] bool empty() const noexcept { return Length_ == 0; }
    [[nodiscard]] char const* data() const noexcept { return StringData_ ? StringData_->ptr() + Offset_ : nullptr; }

    [[nodiscard]] char* data() noexcept
    {
        if (StringData_) {
            ensureUnique();
            return StringData_->ptr() + Offset_;
        }
        return nullptr;
    }

    friend std::ostream& operator<<(std::ostream& os, Fa_StringRef const& str)
    {
        if (!str.empty())
            os.write(str.data(), static_cast<std::streamsize>(str.len()));

        return os;
    }

    void clear() noexcept
    {
        Length_ = 0;
        Offset_ = 0;
    }

    void resize(size_t const s);

    [[nodiscard]] bool find(char const c) const noexcept;
    [[nodiscard]] bool find(Fa_StringRef const& s) const noexcept;

    [[nodiscard]] std::optional<size_t> find_pos(char const c) const noexcept;

    Fa_StringRef& truncate(size_t const s) noexcept;
    Fa_StringRef slice(size_t start, size_t end = SIZE_MAX) const;
    Fa_StringRef substr(size_t start, size_t end = SIZE_MAX) const
    {
        if (Length_ == 0)
            return { };
        if (start > Length_)
            return { };
        if (end > Length_ || end == SIZE_MAX)
            end = Length_;
        if (end < start)
            return { };
        return substrCopy(start, end);
    }
    Fa_StringRef substrCopy(size_t start, size_t end = SIZE_MAX) const;

    f64 toDouble(size_t* pos = nullptr) const;

    [[nodiscard]] static Fa_StringRef fromUtf16(char16_t const* utf16_cstr);

    void ensureUnique()
    {
        if (!StringData_)
            return;
        if (LIKELY(StringData_->referenceCount() == 1))
            return;
        detach();
    }

    void detach();
}; // class Fa_StringRef

namespace detail {

// wy_read helpers — always read unaligned, the compiler emits ldr on ARM64.
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

// 128-bit multiply — GCC/Clang both lower this to a single MUL on ARM64/x86-64.
inline void wymix(u64& a, u64& b) noexcept
{
#if defined(__SIZEOF_INT128__)
    auto r = static_cast<__uint128_t>(a) * b;
    a = static_cast<u64>(r);
    b = static_cast<u64>(r >> 64);
#else
    // 32-bit fallback via four 32×32→64 multiplies.
    u64 ha = a >> 32, hb = b >> 32;
    u64 la = static_cast<u32>(a), lb = static_cast<u32>(b);
    u64 hh = ha * hb, hl = ha * lb, lh = la * hb, ll = la * lb;
    u64 m = (ll >> 32) + static_cast<u32>(hl) + static_cast<u32>(lh);
    a = (hl >> 32) + (lh >> 32) + (hh) + (m >> 32);
    b = ll + (m << 32);
#endif
    a ^= b;
}

// wyhash secret constants (from the reference implementation).
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
            // Read 4 bytes from each end; they may overlap for len in [4,7].
            a = (static_cast<u64>(wy_read4(p)) << 32)
                | wy_read4(p + ((len >> 3) << 2));
            b = (static_cast<u64>(wy_read4(p + len - 4)) << 32)
                | wy_read4(p + len - 4 - ((len >> 3) << 2));
        } else if (len > 0) {
            // 1–3 bytes: spread across three positions without branching.
            a = (static_cast<u64>(p[0]) << 16)
                | (static_cast<u64>(p[len >> 1]) << 8)
                | p[len - 1];
            b = 0;
        }
        // len == 0: a = b = 0, falls through to final mix.
    } else {
        size_t i = len;
        // If not 8-byte aligned, read one full 8-byte pair to align the loop.
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
        // Final 9–16 bytes (always two overlapping 8-byte reads).
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

// ─────────────────────────────────────────────────────────────────────────────
// Drop-in replacement — same interface as the FNV-1a version.
// ─────────────────────────────────────────────────────────────────────────────

struct Fa_StringRefHash {
    // Seed is exposed so callers can build seeded hash tables if needed,
    // but the default zero-seed is fine for HashTable / StringCache_.
    u64 seed { 0 };

    size_t operator()(Fa_StringRef const& str) const noexcept
    {
        if (str.empty())
            return 0;

        size_t n = str.len();
        auto const* p = reinterpret_cast<u8 const*>(str.data());

        if (n <= 16) {
            // Read up to 16 bytes using two overlapping reads,
            // same trick as wyhash's short path — no memcpy, no loop.
            u64 a = 0, b = 0;
            if (n >= 8) {
                // Two 8-byte reads, potentially overlapping.
                __builtin_memcpy(&a, p, 8);
                __builtin_memcpy(&b, p + n - 8, 8);
            } else if (n >= 4) {
                __builtin_memcpy(&a, p, 4);
                __builtin_memcpy(&b, p + n - 4, 4);
            } else {
                // 1–3 bytes: spread across three positions.
                a = (u64(p[0]) << 16)
                    | (u64(p[n >> 1]) << 8)
                    | p[n - 1];
            }
            // Murmur-style finalizer — two multiplies, fast on ARM64.
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

struct Fa_StringRefEqual {
    bool operator()(Fa_StringRef const& lhs, Fa_StringRef const& rhs) const noexcept { return lhs == rhs; }
}; // struct Fa_StringRefEqual

} // namespace fairuz

namespace std {

template<>
struct hash<fairuz::Fa_StringRef> {
    size_t operator()(fairuz::Fa_StringRef const& str) const noexcept { return fairuz::Fa_StringRefHash { }(str); }
}; // struct hash

} // namespace std

#endif // STRING_HPP
