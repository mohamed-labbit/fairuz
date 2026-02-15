#pragma once

#include "../macros.hpp"
#include "string_allocator.hpp"
#include "string_data.hpp"

namespace mylang {

class StringRef {
private:
    String* StringData_ { nullptr };
    SizeType Offset_ { 0 };
    SizeType Length_ { 0 }; // visible length

    // Helper to create empty string
    static String* createEmpty()
    {
        String* s = string_allocator.allocateObject<String>();
        return s;
    }

public:
    // FIXED: Initialize StringData_ properly
    StringRef()
        : StringData_(createEmpty())
    {
    }

    explicit StringRef(SizeType const s)
        : StringData_(string_allocator.allocateObject<String>(s))
    {
    }

    StringRef(StringRef const& other, SizeType offset = 0, SizeType length = 0)
        : StringData_(other.get())
        , Offset_(other.Offset_ + offset)
        , Length_(length)
    {
        if (Length_ == 0)
            Length_ = other.Length_ - offset;

        if (StringData_)
            StringData_->increment();
    }

    StringRef(char const* lit)
        : StringData_(string_allocator.allocateObject<String>(lit))
    {
        Length_ = StringData_->length();
    }

    StringRef(char16_t const* u16_str)
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

    StringRef(SizeType const s, char const c)
        : StringData_(string_allocator.allocateObject<String>(s, c))
    {
    }

    explicit StringRef(String* data, SizeType offset = 0, SizeType length = 0)
        : StringData_(data ? data : createEmpty())
        , Offset_(offset)
        , Length_(length)
    {
        if (StringData_)
            StringData_->increment();

        if (!Length_)
            Length_ = StringData_->length() - offset;
    }

    StringRef(StringRef&& other) noexcept
        : StringData_(other.StringData_)
        , Offset_(other.Offset_)
        , Length_(other.Length_)
    {
        other.StringData_ = nullptr;
        other.Offset_ = 0;
        other.Length_ = 0;
    }

    StringRef& operator=(StringRef&& other) noexcept
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

    ~StringRef()
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

    StringRef& operator=(StringRef const& other)
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

    // Equality operator
    MYLANG_NODISCARD bool operator==(StringRef const& other) const noexcept
    {
        if (!StringData_ || !other.StringData_)
            return StringData_ == other.StringData_;
        return Length_ == other.Length_ && ::memcmp(data(), other.data(), Length_ * sizeof(char)) == 0;
    }

    MYLANG_NODISCARD bool operator!=(StringRef const& other) const noexcept
    {
        return !(*this == other);
    }

    // Expand capacity
    void expand(SizeType const new_size);

    // Reserve capacity (doesn't change length, only capacity)
    void reserve(SizeType const new_capacity);

    // Erase character at position
    void erase(SizeType const at);

    // Append another StringRef
    StringRef& operator+=(StringRef const& other);
    StringRef& operator+=(char c);

    // FIXED: Add null checks
    char operator[](SizeType const i) const;
    char& operator[](SizeType const i);

    // Safe access with bounds checking (always checked)
    MYLANG_NODISCARD char at(SizeType const i) const;

    char& at(SizeType const i);

    StringRef& trimWhitespace(std::optional<bool const> leading = std::nullopt, std::optional<bool const> trailing = std::nullopt);

    // StringRef + StringRef
    friend StringRef operator+(StringRef const& lhs, StringRef const& rhs)
    {
        if (lhs.empty() && rhs.empty())
            return "";

        if (lhs.empty())
            return StringRef(rhs);

        if (rhs.empty())
            return StringRef(lhs);

        // FIXED: Correct length calculation
        SizeType actual_len = lhs.len() + rhs.len();
        String* result = string_allocator.allocateObject<String>(actual_len);

        ::memcpy(result->ptr(), lhs.data(), lhs.len() * sizeof(char));
        ::memcpy(result->ptr() + lhs.len(), rhs.data(), rhs.len() * sizeof(char));

        result->setLen(actual_len);
        result->terminate();

        return StringRef(result);
    }

    // StringRef + const char* (UTF-8 string)
    friend StringRef operator+(StringRef const& lhs, char const* rhs)
    {
        if (!rhs || !rhs[0])
            return StringRef(lhs);

        StringRef rhs_str = rhs;
        return lhs + rhs_str;
    }

    // const char* + StringRef
    friend StringRef operator+(char const* lhs, StringRef const& rhs)
    {
        if (!lhs || !lhs[0])
            return StringRef(rhs);

        StringRef lhs_str = lhs;
        return lhs_str + rhs;
    }

    // StringRef + char (single character)
    friend StringRef operator+(StringRef const& lhs, char rhs)
    {
        StringRef result(lhs);
        result += rhs;
        return result;
    }

    // char + StringRef
    friend StringRef operator+(char lhs, StringRef const& rhs)
    {
        StringRef result;
        result.reserve(rhs.len() + 1);
        result += lhs;
        result += rhs;
        return result;
    }

    // Accessors
    MYLANG_NODISCARD SizeType len() const noexcept
    {
        return Length_;
    }

    MYLANG_NODISCARD SizeType cap() const noexcept
    {
        return StringData_ ? StringData_->cap() : 0;
    }

    MYLANG_NODISCARD String* get() const noexcept
    {
        return StringData_;
    }

    MYLANG_NODISCARD bool empty() const noexcept
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

    // Clear the string (doesn't deallocate memory)
    void clear() noexcept;

    // Resize capacity (preserves content)
    void resize(SizeType const s);

    // Find a character
    MYLANG_NODISCARD bool find(char const c) const noexcept;
    MYLANG_NODISCARD bool find(StringRef const& s) const noexcept;

    // Find position of a character (returns optional index)
    MYLANG_NODISCARD std::optional<SizeType> find_pos(char const c) const noexcept;

    // Truncate string to specified length
    StringRef& truncate(SizeType const s);

    StringRef slice(SizeType start, std::optional<SizeType> end) const; // no copy

    StringRef substr(std::optional<SizeType> start, std::optional<SizeType> end) const;

    StringRef substr(SizeType start) const
    {
        return substr(std::optional<SizeType>(start), std::nullopt);
    }

    // Convert to double - improved error handling
    double toDouble(SizeType* pos = nullptr) const;

    // Convenience overload for C strings
    MYLANG_NODISCARD static StringRef fromUtf16(char16_t const* utf8_cstr);

    void ensureUnique()
    {
        if (!StringData_)
            return;
        if (StringData_->referenceCount() > 1)
            detach();
    }

    void detach()
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
}; // StringRef

// Hash functor for StringRef
struct StringRefHash {
    std::size_t operator()(StringRef const& str) const noexcept
    {
        if (str.empty() || str.len() == 0)
            return 0;

        // FNV-1a hash algorithm (good distribution, fast)
        std::size_t hash = 14695981039346656037ULL; // FNV offset basis
        std::size_t const prime = 1099511628211ULL; // FNV prime

        char const* data = str.data();
        SizeType const len = str.len();

        // Hash the raw bytes of char16_t data
        Byte const* bytes = reinterpret_cast<Byte const*>(data);
        std::size_t const byte_count = len * sizeof(char);

        for (std::size_t i = 0; i < byte_count; ++i) {
            hash ^= static_cast<std::size_t>(bytes[i]);
            hash *= prime;
        }

        return hash;
    }
};

// Equal comparison functor for consistency
struct StringRefEqual {
    bool operator()(StringRef const& lhs, StringRef const& rhs) const noexcept
    {
        return lhs == rhs;
    }
};

} // namespace mylang

// Specialize std::hash for StringRef
namespace std {

template<>
struct hash<mylang::StringRef> {
    std::size_t operator()(mylang::StringRef const& str) const noexcept
    {
        if (str.empty() || str.len() == 0)
            return 0;

        // FNV-1a hash
        std::size_t hash_value = 14695981039346656037ULL;
        std::size_t const prime = 1099511628211ULL;

        char const* data = str.data();
        mylang::SizeType const len = str.len();

        unsigned char const* bytes = reinterpret_cast<unsigned char const*>(data);
        std::size_t const byte_count = len * sizeof(char);

        for (std::size_t i = 0; i < byte_count; ++i) {
            hash_value ^= static_cast<std::size_t>(bytes[i]);
            hash_value *= prime;
        }

        return hash_value;
    }
};

} // namespace std
