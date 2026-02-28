#pragma once

#include "../macros.hpp"
#include "string_allocator.hpp"

namespace mylang {

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

}
