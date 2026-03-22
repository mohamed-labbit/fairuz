#ifndef STRING_HPP
#define STRING_HPP

#include "arena.hpp"

#include <ostream>

namespace mylang {

class StringBase {
    friend class StringRef;

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

    mutable std::atomic<uint32_t> RefCount { 1 };

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

    static constexpr size_t kSSOSize = SSO_SIZE;

    bool isHeap() const noexcept { return is_heap; }

    bool isInlined() const noexcept { return !isHeap(); }

    char* ptr() noexcept { return isHeap() ? storage_.heap.ptr : storage_.sso; }

    char const* ptr() const noexcept { return isHeap() ? storage_.heap.ptr : storage_.sso; }

    size_t cap() const noexcept { return isHeap() ? (storage_.heap.cap > 0 ? storage_.heap.cap - 1 : 0) : SSO_SIZE - 1; }

    explicit StringBase(size_t const s);
    explicit StringBase(size_t const s, char const c);
    explicit StringBase(char const* s, size_t n);
    StringBase(char const* s);

    char operator[](size_t const i) const noexcept { return ptr()[i]; }
    char& operator[](size_t const i) noexcept { return ptr()[i]; }

    void increment() const noexcept { RefCount.fetch_add(1, std::memory_order_relaxed); }
    void decrement() const noexcept { RefCount.fetch_sub(1, std::memory_order_acq_rel); }

    uint32_t referenceCount() const noexcept { return RefCount.load(std::memory_order_acquire); }
};

namespace detail {

StringBase* emptyStringSingleton() noexcept;

} // namespace detail

class StringRef {
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
    StringRef() noexcept
        : StringData_(detail::emptyStringSingleton())
        , Offset_(0)
        , Length_(0)
    {
        StringData_->increment();
    }

    explicit StringRef(size_t const s);

    StringRef(StringRef const& other, size_t offset = 0, size_t length = 0);

    StringRef(char const* lit);

    StringRef(char16_t const* u16_str);

    StringRef(size_t const s, char const c);

    explicit StringRef(StringBase* data, size_t offset = 0, size_t length = 0);

    StringRef(StringRef&& other) noexcept;

    StringRef& operator=(StringRef&& other) noexcept;

    ~StringRef();

    StringRef& operator=(StringRef const& other);

    MY_NODISCARD bool operator==(StringRef const& other) const noexcept
    {
        if (StringData_ == other.StringData_ && Offset_ == other.Offset_ && Length_ == other.Length_)
            return true;
        if (Length_ != other.Length_)
            return false;
        if (!StringData_ || !other.StringData_)
            return Length_ == 0;
        return ::memcmp(data(), other.data(), Length_) == 0;
    }

    MY_NODISCARD bool operator!=(StringRef const& other) const noexcept { return !(*this == other); }

    MY_NODISCARD bool operator<(StringRef const& other) const noexcept
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

    MY_NODISCARD bool operator>(StringRef const& other) const noexcept { return other < *this; }
    MY_NODISCARD bool operator<=(StringRef const& other) const noexcept { return !(*this > other); }
    MY_NODISCARD bool operator>=(StringRef const& other) const noexcept { return !(*this < other); }

    void expand(size_t const new_size);
    void reserve(size_t const new_capacity);
    void erase(size_t const at);

    StringRef& operator+=(StringRef const& other);
    StringRef& operator+=(char c);

    char operator[](size_t const i) const;
    char& operator[](size_t const i);

    MY_NODISCARD char at(size_t const i) const;
    char& at(size_t const i);

    StringRef& trimWhitespace(bool leading = true, bool trailing = true) noexcept;

    StringRef operator+(StringRef const& rhs) const
    {
        size_t const r_len = rhs.len();
        if (empty())
            return StringRef(rhs);
        if (r_len == 0)
            return *this;

        size_t const new_len = Length_ + r_len;
        StringBase* result = getAllocator().allocateObject<StringBase>(new_len);

        ::memcpy(result->ptr(), data(), Length_);
        ::memcpy(result->ptr() + Length_, rhs.data(), r_len);

        result->ptr()[new_len] = 0;

        return StringRef(result);
    }

    StringRef operator+(char const* rhs) const
    {
        if (!rhs || !rhs[0])
            return *this;
        StringRef rhs_str = rhs;
        return *this + rhs_str;
    }

    friend StringRef operator+(char const* lhs, StringRef const& rhs)
    {
        if (!lhs || !lhs[0])
            return StringRef(rhs);
        StringRef lhs_str = lhs;
        return lhs_str + rhs;
    }

    friend StringRef operator+(StringRef const& lhs, char rhs)
    {
        StringRef result(lhs);
        result += rhs;
        return result;
    }

    friend StringRef operator+(char lhs, StringRef const& rhs)
    {
        StringRef result;
        result.reserve(rhs.len() + 1);
        result += lhs;
        result += rhs;
        return result;
    }

    MY_NODISCARD size_t len() const noexcept { return Length_; }
    MY_NODISCARD size_t cap() const noexcept { return StringData_ ? StringData_->cap() : 0; }
    MY_NODISCARD StringBase* get() const noexcept { return StringData_; }
    MY_NODISCARD bool empty() const noexcept { return Length_ == 0; }
    MY_NODISCARD char const* data() const noexcept { return StringData_ ? StringData_->ptr() + Offset_ : nullptr; }

    MY_NODISCARD char* data() noexcept
    {
        if (StringData_) {
            ensureUnique();
            return StringData_->ptr() + Offset_;
        }
        return nullptr;
    }

    friend std::ostream& operator<<(std::ostream& os, StringRef const& str)
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

    MY_NODISCARD bool find(char const c) const noexcept;
    MY_NODISCARD bool find(StringRef const& s) const noexcept;

    MY_NODISCARD std::optional<size_t> find_pos(char const c) const noexcept;

    StringRef& truncate(size_t const s) noexcept;
    StringRef slice(size_t start, size_t end = SIZE_MAX) const;
    StringRef substr(size_t start, size_t end = SIZE_MAX) const { return slice(start, end); }
    StringRef substrCopy(size_t start, size_t end = SIZE_MAX) const;

    double toDouble(size_t* pos = nullptr) const;

    MY_NODISCARD static StringRef fromUtf16(char16_t const* utf16_cstr);

    void ensureUnique()
    {
        if (!StringData_)
            return;
        if (__builtin_expect(StringData_->referenceCount() == 1, 1))
            return;
        detach();
    }

    void detach();
}; // StringRef

struct StringRefHash {
    size_t operator()(StringRef const& str) const noexcept
    {
        if (str.empty())
            return 0;

        size_t hash = 14695981039346656037ULL;
        constexpr size_t prime = 1099511628211ULL;

        auto const* bytes = reinterpret_cast<unsigned char const*>(str.data());
        size_t const n = str.len();

        for (size_t i = 0; i < n; ++i) {
            hash ^= static_cast<size_t>(bytes[i]);
            hash *= prime;
        }

        return hash;
    }
};

struct StringRefEqual {
    bool operator()(StringRef const& lhs, StringRef const& rhs) const noexcept { return lhs == rhs; }
};

} // namespace mylang

namespace std {

template<>
struct hash<mylang::StringRef> {
    size_t operator()(mylang::StringRef const& str) const noexcept { return mylang::StringRefHash { }(str); }
};

} // namespace std

#endif // STRING_HPP
