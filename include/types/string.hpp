#pragma once

#include "../macros.hpp"
#include "string_allocator.hpp"
#include "string_data.hpp"

namespace mylang {

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

    [[nodiscard]] bool operator==(StringRef const& other) const noexcept
    {
        if (StringData_ == other.StringData_)
            return true;

        if (!StringData_ || !other.StringData_)
            return false;

        return Length_ == other.Length_ && ::memcmp(data(), other.data(), Length_) == 0;
    }

    [[nodiscard]] bool operator<(StringRef const& other) const noexcept
    {
        if (StringData_ == other.StringData_)
            return false;

        if (!StringData_)
            return true; // null < non-null

        if (!other.StringData_)
            return false;

        size_t minLen = std::min(Length_, other.Length_);
        int cmp = ::memcmp(data(), other.data(), minLen);

        if (cmp != 0)
            return cmp < 0;

        return Length_ < other.Length_;
    }

    [[nodiscard]] bool operator!=(StringRef const& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] bool operator<=(StringRef const& other) const noexcept
    {
        return !(*this > other);
    }

    [[nodiscard]] bool operator>(StringRef const& other) const noexcept
    {
        return other < *this;
    }

    [[nodiscard]] bool operator>=(StringRef const& other) const noexcept
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

    [[nodiscard]] char at(size_t const i) const;

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

        size_t actual_len = lhs.len() + rhs.len();
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

    [[nodiscard]] size_t len() const noexcept
    {
        return Length_;
    }

    [[nodiscard]] size_t cap() const noexcept
    {
        return StringData_ ? StringData_->cap() : 0;
    }

    [[nodiscard]] String* get() const noexcept
    {
        return StringData_;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return Length_ == 0;
    }

    char const* data() const noexcept
    {
        return StringData_ ? StringData_->ptr() + Offset_ : nullptr;
    }

    char* data() noexcept
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

    [[nodiscard]] bool find(char const c) const noexcept;
    [[nodiscard]] bool find(StringRef const& s) const noexcept;

    [[nodiscard]] std::optional<size_t> find_pos(char const c) const noexcept;

    StringRef& truncate(size_t const s);

    StringRef slice(size_t start, std::optional<size_t> end) const;

    StringRef substr(std::optional<size_t> start, std::optional<size_t> end) const;

    StringRef substr(size_t start) const
    {
        return substr(std::optional<size_t>(start), std::nullopt);
    }

    double toDouble(size_t* pos = nullptr) const;

    [[nodiscard]] static StringRef fromUtf16(char16_t const* utf8_cstr);

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
