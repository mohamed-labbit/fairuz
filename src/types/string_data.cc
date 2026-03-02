#include "../../include/types/string_data.hpp"

namespace mylang {

String::String(String const& other)
    : is_heap(other.isHeap())
{
    setLen(other.length());

    if (other.isInlined())
        ::memcpy(storage_.sso, other.storage_.sso, (length() + 1) * sizeof(char));
    else {
        storage_.heap.cap = other.storage_.heap.cap;
        storage_.heap.ptr = string_allocator.allocateArray<char>(storage_.heap.cap);

        ::memcpy(storage_.heap.ptr, other.storage_.heap.ptr, (length() + 1) * sizeof(char));
    }
}

String::String(size_t const s)
{
    if (s < SSO_SIZE) {
        is_heap = false;
        setLen(0);
    } else {
        is_heap = true;
        setLen(0);
        storage_.heap.cap = s + 1;
        storage_.heap.ptr = string_allocator.allocateArray<char>(storage_.heap.cap);
    }
    terminate();
}

String::String(size_t const s, char const c)
{
    if (s < SSO_SIZE) {
        is_heap = false;
        ::memset(storage_.sso, c, s);
        setLen(s);
    } else {
        is_heap = true;
        storage_.heap.cap = s + 1;
        storage_.heap.ptr = string_allocator.allocateArray<char>(storage_.heap.cap);
        ::memset(storage_.heap.ptr, c, s);
        setLen(s);
    }
    terminate();
}

String::String(char const* s, size_t n)
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
        ::memcpy(storage_.sso, s, n * sizeof(char));
    } else {
        is_heap = true;
        storage_.heap.cap = n + 1;
        storage_.heap.ptr = string_allocator.allocateArray<char>(storage_.heap.cap);
        ::memcpy(storage_.heap.ptr, s, n * sizeof(char));
    }
    terminate();
}

String::String(char const* s)
{
    if (!s) {
        len_ = 0;
        storage_.sso[0] = BUFFER_END;
        return;
    }

    size_t n = ::strlen(s);

    if (n < SSO_SIZE) {
        is_heap = false;
        setLen(n);
        ::memcpy(storage_.sso, s, n * sizeof(char));
    } else {
        is_heap = true;
        setLen(n);
        storage_.heap.cap = n + 1;
        storage_.heap.ptr = string_allocator.allocateArray<char>(storage_.heap.cap);
        ::memcpy(storage_.heap.ptr, s, (n + 1) * sizeof(char));
    }
    terminate();
}

bool String::operator==(String const& other) const noexcept
{
    if (length() != other.length())
        return false;
    if (length() == 0)
        return true;

    return ::memcmp(ptr(), other.ptr(), length() * sizeof(char)) == 0;
}

}
