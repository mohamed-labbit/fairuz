#ifndef STRING_HPP
#define STRING_HPP

#include "allocator.hpp"
#include "macros.hpp"

namespace mylang {

inline Allocator string_allocator("String allocator");

class String {
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
    size_t len_ { 0 };
    mutable size_t RefCount { 1 };

public:
    String()
        : is_heap { false }
    {
        len_ = 0;
        storage_.sso[0] = BUFFER_END;
    }

    ~String()
    {
        if (isHeap())
            string_allocator.deallocateArray<char>(storage_.heap.ptr, storage_.heap.cap);
    }

    size_t length() const noexcept
    {
        return len_;
    }

    bool isHeap() const noexcept
    {
        return is_heap;
    }

    bool isInlined() const noexcept
    {
        return !isHeap();
    }

    char* ptr() noexcept
    {
        return isHeap() ? storage_.heap.ptr : storage_.sso;
    }

    char const* ptr() const noexcept
    {
        return isHeap() ? storage_.heap.ptr : storage_.sso;
    }

    size_t cap() const noexcept
    {
        return isHeap() ? storage_.heap.cap - 1 /*subtract the nul terminator*/ : SSO_SIZE - 1;
    }

    void setLen(size_t const n)
    {
        if (!isHeap() && n > cap())
            throw std::invalid_argument("String::setLen(unsigned long n = " + std::to_string(n) + ") : invalid length");

        len_ = n;
    }

    void terminate() noexcept
    {
        ptr()[length()] = BUFFER_END;
    }

    String(String const& other);

    String(size_t const s);

    String(size_t const s, char const c);

    String(char const* s, size_t n);

    String(char const* s);

    bool operator==(String const& other) const noexcept;

    char operator[](size_t const i) const noexcept
    {
        return ptr()[i];
    }

    char& operator[](size_t const i) noexcept
    {
        return ptr()[i];
    }

    void increment() const noexcept
    {
        ++RefCount;
    }

    void decrement() const noexcept
    {
        --RefCount;
    }

    size_t referenceCount() const noexcept
    {
        return RefCount;
    }
};

class StringRef {
private:
    String* StringData_ { nullptr };
    size_t Offset_ { 0 };
    size_t Length_ { 0 };

    static String* createEmpty()
    {
        String* s = string_allocator.allocateObject<String>();
        return s;
    }

public:
    StringRef();

    explicit StringRef(size_t const s);

    StringRef(StringRef const& other, size_t offset = 0, size_t length = 0);

    StringRef(char const* lit);

    StringRef(char16_t const* u16_str);

    StringRef(size_t const s, char const c);

    explicit StringRef(String* data, size_t offset = 0, size_t length = 0);

    StringRef(StringRef&& other) noexcept;

    StringRef& operator=(StringRef&& other) noexcept;

    ~StringRef();

    StringRef& operator=(StringRef const& other);

    MY_NODISCARD bool operator==(StringRef const& other) const noexcept
    {
        if (StringData_ == other.StringData_)
            return true;

        if (!StringData_ || !other.StringData_)
            return false;

        return Length_ == other.Length_ && ::memcmp(data(), other.data(), Length_) == 0;
    }

    MY_NODISCARD bool operator<(StringRef const& other) const noexcept
    {
        if (StringData_ == other.StringData_)
            return false;

        if (!StringData_)
            return true; // null < non-null

        if (!other.StringData_)
            return false;

        size_t const minLen = std::min(Length_, other.Length_);
        int const cmp = ::memcmp(data(), other.data(), minLen);

        if (cmp != 0)
            return cmp < 0;

        return Length_ < other.Length_;
    }

    MY_NODISCARD bool operator!=(StringRef const& other) const noexcept
    {
        return !(*this == other);
    }

    MY_NODISCARD bool operator<=(StringRef const& other) const noexcept
    {
        return !(*this > other);
    }

    MY_NODISCARD bool operator>(StringRef const& other) const noexcept
    {
        return other < *this;
    }

    MY_NODISCARD bool operator>=(StringRef const& other) const noexcept
    {
        return !(*this < other);
    }

    void expand(size_t const new_size);

    void reserve(size_t const new_capacity);

    void erase(size_t const at);

    StringRef& operator+=(StringRef const& other);
    StringRef& operator+=(char c);

    char operator[](size_t const i) const;
    char& operator[](size_t const i);

    MY_NODISCARD char at(size_t const i) const;

    char& at(size_t const i);

    StringRef& trimWhitespace(std::optional<bool const> leading = std::nullopt, std::optional<bool const> trailing = std::nullopt);

    friend StringRef operator+(StringRef const& lhs, StringRef const& rhs)
    {
        if (lhs.empty() && rhs.empty())
            return "";

        if (lhs.empty())
            return StringRef(rhs);

        if (rhs.empty())
            return StringRef(lhs);

        size_t const actual_len = lhs.len() + rhs.len();
        String* result = string_allocator.allocateObject<String>(actual_len);

        ::memcpy(result->ptr(), lhs.data(), lhs.len() * sizeof(char));
        ::memcpy(result->ptr() + lhs.len(), rhs.data(), rhs.len() * sizeof(char));

        result->setLen(actual_len);
        result->terminate();

        return StringRef(result);
    }

    friend StringRef operator+(StringRef const& lhs, char const* rhs)
    {
        if (!rhs || !rhs[0])
            return StringRef(lhs);

        StringRef rhs_str = rhs;
        return lhs + rhs_str;
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

    MY_NODISCARD size_t len() const noexcept
    {
        return Length_;
    }

    MY_NODISCARD size_t cap() const noexcept
    {
        return StringData_ ? StringData_->cap() : 0;
    }

    MY_NODISCARD String* get() const noexcept
    {
        return StringData_;
    }

    MY_NODISCARD bool empty() const noexcept
    {
        return Length_ == 0;
    }

    MY_NODISCARD char const* data() const noexcept
    {
        return StringData_ ? StringData_->ptr() + Offset_ : nullptr;
    }

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
            os.write(str.data(), str.len());

        return os;
    }

    void clear() noexcept;

    void resize(size_t const s);

    MY_NODISCARD bool find(char const c) const noexcept;
    MY_NODISCARD bool find(StringRef const& s) const noexcept;

    MY_NODISCARD std::optional<size_t> find_pos(char const c) const noexcept;

    StringRef& truncate(size_t const s);

    StringRef slice(size_t start, std::optional<size_t> end) const;

    StringRef substr(std::optional<size_t> start, std::optional<size_t> end) const;

    StringRef substr(size_t start) const
    {
        return substr(std::optional<size_t>(start), std::nullopt);
    }

    double toDouble(size_t* pos = nullptr) const;

    MY_NODISCARD static StringRef fromUtf16(char16_t const* utf8_cstr);

    void ensureUnique()
    {
        if (!StringData_)
            return;

        if (StringData_->referenceCount() > 1)
            detach();
    }

    void detach();
}; // StringRef

struct StringRefHash {
    size_t operator()(StringRef const& str) const noexcept
    {
        if (str.empty() || str.len() == 0)
            return 0;

        size_t hash = 14695981039346656037ULL;
        size_t const prime = 1099511628211ULL;

        char const* data = str.data();
        size_t const len = str.len();

        unsigned char const* bytes = reinterpret_cast<unsigned char const*>(data);
        size_t const byte_count = len * sizeof(char);

        for (size_t i = 0; i < byte_count; ++i) {
            hash ^= static_cast<size_t>(bytes[i]);
            hash *= prime;
        }

        return hash;
    }
};

struct StringRefEqual {
    bool operator()(StringRef const& lhs, StringRef const& rhs) const noexcept
    {
        return lhs == rhs;
    }
};

} // namespace mylang

namespace std {

template<>
struct hash<mylang::StringRef> {
    size_t operator()(mylang::StringRef const& str) const noexcept
    {
        if (str.empty() || str.len() == 0)
            return 0;

        size_t hash_value = 14695981039346656037ULL;
        size_t const prime = 1099511628211ULL;

        char const* data = str.data();
        size_t const len = str.len();

        unsigned char const* bytes = reinterpret_cast<unsigned char const*>(data);
        size_t const byte_count = len * sizeof(char);

        for (size_t i = 0; i < byte_count; ++i) {
            hash_value ^= static_cast<size_t>(bytes[i]);
            hash_value *= prime;
        }

        return hash_value;
    }
};

} // namespace std

#endif // STRING_HPP
