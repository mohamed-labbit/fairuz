#pragma once

#include "../macros.hpp"
#include "string_allocator.hpp"

namespace mylang {

class String {
    friend class StringRef;

    using Pointer = CharType*;
    using ConstPointer = CharType const*;

private:
    // static constexpr SizeType SSO_FLAG = SizeType(1) << (sizeof(SizeType) * 8 - 1);

    union Storage {
        struct
        {
            Pointer ptr;
            SizeType cap;
        } heap;

        CharType sso[SSO_SIZE];

        Storage()
            : heap { nullptr, 0 }
        {
        }
    } storage_;

    bool is_heap;
    SizeType len_ { 0 };
    mutable SizeType RefCount { 1 };

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
            string_allocator.deallocateArray<CharType>(storage_.heap.ptr, storage_.heap.cap);
    }

    // ---- helpers ----

    SizeType length() const noexcept { return len_; }

    bool isHeap() const noexcept { return is_heap; }

    bool isInlined() const noexcept { return !isHeap(); }

    Pointer ptr() noexcept { return isHeap() ? storage_.heap.ptr : storage_.sso; }

    ConstPointer ptr() const noexcept { return isHeap() ? storage_.heap.ptr : storage_.sso; }

    SizeType cap() const noexcept
    {
        return isHeap() ? storage_.heap.cap - 1 /*subtract the nul terminator*/ : SSO_SIZE - 1;
    }

    void setLen(SizeType const n)
    {
        if (!isHeap() && n > cap())
            throw std::invalid_argument("String::setLen(unsigned long n = " + std::to_string(n) + ") : invalid length");
        len_ = n;
    }

    void terminate() noexcept { ptr()[length()] = BUFFER_END; }

    // ---- constructors ----

    String(String const& other)
        : is_heap(other.isHeap())
    {
        setLen(other.length());

        if (other.isInlined())
            ::memcpy(storage_.sso, other.storage_.sso, (length() + 1) * sizeof(CharType));
        else {
            storage_.heap.cap = other.storage_.heap.cap;
            storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);

            ::memcpy(storage_.heap.ptr, other.storage_.heap.ptr, (length() + 1) * sizeof(CharType));
        }
    }

    String(SizeType const s)
    {
        if (s < SSO_SIZE) {
            is_heap = false;
            setLen(0);
        } else {
            is_heap = true;
            setLen(0);
            storage_.heap.cap = s + 1;
            storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);
        }
        terminate();
    }

    String(SizeType const s, CharType const c)
    {
        if (s < SSO_SIZE) {
            is_heap = false;
            ::memset(storage_.sso, c, s);
            setLen(s);
        } else {
            is_heap = true;
            storage_.heap.cap = s + 1;
            storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);
            ::memset(storage_.heap.ptr, c, s);
            setLen(s);
        }

        terminate();
    }

    String(ConstPointer s, SizeType n)
    {
        if (!s || n == 0) {
            is_heap = false;
            setLen(0);
            terminate();
            return;
        }

        if (n < SSO_SIZE) {
            is_heap = false;
            setLen(n);
            ::memcpy(storage_.sso, s, n * sizeof(CharType));
        } else {
            is_heap = true;
            storage_.heap.cap = n + 1;
            storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);
            ::memcpy(storage_.heap.ptr, s, n * sizeof(CharType));
        }

        terminate();
    }

    String(ConstPointer s)
    {
        if (!s) {
            len_ = 0;
            storage_.sso[0] = BUFFER_END;
            return;
        }

        ConstPointer p = s;
        while (*p++)
            ;

        SizeType n = p - s - 1;

        if (n < SSO_SIZE) {
            is_heap = false;
            setLen(n);
            ::memcpy(storage_.sso, s, n * sizeof(CharType));
        } else {
            is_heap = true;
            setLen(n);
            storage_.heap.cap = n + 1;
            storage_.heap.ptr = string_allocator.allocateArray<CharType>(storage_.heap.cap);
            ::memcpy(storage_.heap.ptr, s, (n + 1) * sizeof(CharType));
        }

        terminate();
    }

    // ---- ops ----

    bool operator==(String const& other) const noexcept
    {
        if (length() != other.length())
            return false;

        if (length() == 0)
            return true;

        return ::memcmp(ptr(), other.ptr(), length() * sizeof(CharType)) == 0;
    }

    CharType operator[](SizeType const i) const noexcept { return ptr()[i]; }
    CharType& operator[](SizeType const i) noexcept { return ptr()[i]; }

    void increment() const noexcept { ++RefCount; }
    void decrement() const noexcept { --RefCount; }
    SizeType referenceCount() const noexcept { return RefCount; }
};

}
