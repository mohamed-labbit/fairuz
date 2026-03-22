#ifndef ARRAY_HPP
#define ARRAY_HPP

#include "arena.hpp"
#include "diagnostic.hpp"

#include <bit>
#include <cassert>

namespace mylang {

template<typename T>
class Array {
    static constexpr uint32_t ARRAY_MAX = __UINT32_MAX__;
    static constexpr uint32_t DEFAULT_CAP = 8;
    static constexpr bool TRIVIAL_COPY = std::is_trivially_copyable_v<T>;
    static constexpr bool TRIVIAL_DTOR = std::is_trivially_destructible_v<T>;

    static void destroy_range(T* first, T* last) noexcept
    {
        if constexpr (!TRIVIAL_DTOR) {
            for (; first != last; ++first)
                first->~T();
        }
    }

    static void relocate(T* dst, T* src, uint32_t n) noexcept
    {
        if (n == 0)
            return;

        if constexpr (TRIVIAL_COPY)
            ::memcpy(static_cast<void*>(dst), src, n * sizeof(T));
        else {
            for (uint32_t i = 0; i < n; ++i) {
                ::new (static_cast<void*>(dst + i)) T(std::move(src[i]));
                src[i].~T();
            }
        }
    }

    static void copy_construct_range(T* dst, T const* src, uint32_t n)
    {
        if (n == 0)
            return;

        if constexpr (TRIVIAL_COPY)
            ::memcpy(static_cast<void*>(dst), src, n * sizeof(T));
        else {
            uint32_t i = 0;
            try {
                for (; i < n; ++i)
                    ::new (static_cast<void*>(dst + i)) T(src[i]);
            } catch (...) {
                destroy_range(dst, dst + i);
                throw;
            }
        }
    }

    T* arr { nullptr };
    uint32_t Size_ { 0 };
    uint32_t Cap_ { 0 };

    static uint32_t nextCapacity(uint32_t const s) noexcept
    {
        if (s <= DEFAULT_CAP)
            return DEFAULT_CAP;
        if (s < 64)
            return 1u << std::bit_width(s - 1); // power-of-2 for small
        return s + (s >> 1);                    // 1.5x for larger
    }

    void ensure_push_capacity();

public:
    Array() = default;
    explicit Array(uint32_t capacity, T fill_v = T());
    Array(Array const& other);
    Array(Array&& other) noexcept;
    Array(std::initializer_list<T> list);

    Array& operator=(Array const& other);
    Array& operator=(Array&& other) noexcept;

    ~Array() { destroy_range(arr, arr + Size_); }

    static Array withCapacity(uint32_t capacity)
    {
        Array a;
        if (capacity == 0)
            return a;

        a.arr = getAllocator().allocateArray<T>(capacity);
        assert(a.arr != nullptr);
        a.Cap_ = capacity;
        return a;
    }

    template<typename... Args>
    T& emplace(Args&&... args)
    {
        ensure_push_capacity();
        T* slot = arr + Size_;

        if constexpr (TRIVIAL_COPY)
            *slot = T(std::forward<Args>(args)...);
        else
            ::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);

        ++Size_;
        return *slot;
    }

    void push(T const& val);
    void push(T&& val);
    T pop();
    void reserve(uint32_t const s);
    void resize(uint32_t const s);
    void erase(uint32_t const at);
    T* erase(T const* p);
    T& operator[](uint32_t i) { return arr[i]; }
    T const& operator[](uint32_t i) const { return arr[i]; }

    T& back()
    {
        if (Size_ == 0)
            diagnostic::emit("Array::back -- array is empty");

        return arr[Size_ - 1];
    }

    T const& back() const
    {
        if (Size_ == 0)
            diagnostic::emit("Array::back -- array is empty");

        return arr[Size_ - 1];
    }

    T& front()
    {
        if (Size_ == 0)
            diagnostic::emit("Array::front -- array is empty");

        return arr[0];
    }

    T const& front() const
    {
        if (Size_ == 0)
            diagnostic::emit("Array::fron -- array is empty");

        return arr[0];
    }

    uint32_t size() const noexcept { return Size_; }
    uint32_t cap() const noexcept { return Cap_; }
    T* data() noexcept { return arr; }
    T const* data() const noexcept { return arr; }
    bool empty() const noexcept { return Size_ == 0; }
    bool full() const noexcept { return Size_ == Cap_; }
    T* begin() noexcept { return arr; }
    T* end() noexcept { return arr + Size_; }
    T const* begin() const noexcept { return arr; }
    T const* end() const noexcept { return arr + Size_; }
};

template<typename T>
void Array<T>::ensure_push_capacity()
{
    if (Size_ < Cap_)
        return;
    uint32_t new_cap = Cap_ == 0 ? DEFAULT_CAP : Cap_ + (Cap_ >> 1);
    // allocate new_cap directly, skip the nextCapacity rounding
    T* new_arr = getAllocator().allocateArray<T>(new_cap);
    if (arr && Size_ > 0)
        relocate(new_arr, arr, Size_);
    arr = new_arr;
    Cap_ = new_cap;
}

template<typename T>
Array<T>::Array(uint32_t capacity, T fill_v)
{
    if (capacity > ARRAY_MAX)
        diagnostic::emit("Requested capacity " + std::to_string(capacity) + " exceeds maximum " + std::to_string(ARRAY_MAX), diagnostic::Severity::FATAL);

    if (capacity == 0)
        return;

    arr = getAllocator().allocateArray<T>(capacity);
    assert(arr != nullptr);
    Cap_ = capacity;

    uint32_t i = 0;
    try {
        if constexpr (TRIVIAL_COPY) {
            for (; i < capacity; ++i)
                arr[i] = fill_v;
        } else {
            for (; i < capacity; ++i)
                ::new (static_cast<void*>(arr + i)) T(fill_v);
        }
    } catch (...) {
        destroy_range(arr, arr + i);
        throw;
    }
    Size_ = capacity;
}

template<typename T>
Array<T>::Array(Array const& other)
    : Cap_(other.Cap_)
{
    if (Cap_ == 0)
        return;

    arr = getAllocator().allocateArray<T>(Cap_);
    assert(arr != nullptr);
    copy_construct_range(arr, other.arr, other.Size_);
    Size_ = other.Size_;
}

template<typename T>
Array<T>::Array(Array&& other) noexcept
    : arr(other.arr)
    , Size_(other.Size_)
    , Cap_(other.Cap_)
{
    other.arr = nullptr;
    other.Size_ = 0;
    other.Cap_ = 0;
}

template<typename T>
Array<T>::Array(std::initializer_list<T> list)
{
    if (list.size() == 0)
        return;

    Size_ = static_cast<uint32_t>(list.size());
    Cap_ = nextCapacity(Size_);
    arr = getAllocator().allocateArray<T>(Cap_);
    assert(arr != nullptr);

    uint32_t i = 0;
    try {
        if constexpr (TRIVIAL_COPY) {
            for (T const& val : list)
                arr[i++] = val;
        } else {
            for (T const& val : list)
                ::new (static_cast<void*>(arr + i++)) T(val);
        }
    } catch (...) {
        destroy_range(arr, arr + i);
        Size_ = 0;
        throw;
    }
}

template<typename T>
Array<T>& Array<T>::operator=(Array const& other)
{
    if (this == &other)
        return *this;

    destroy_range(arr, arr + Size_);
    Size_ = 0;

    if (other.Cap_ > Cap_) {
        // NOTE: arena – no free on arr here.
        arr = getAllocator().allocateArray<T>(other.Cap_);
        assert(arr != nullptr);
        Cap_ = other.Cap_;
    }

    copy_construct_range(arr, other.arr, other.Size_);
    Size_ = other.Size_;
    return *this;
}

template<typename T>
Array<T>& Array<T>::operator=(Array&& other) noexcept
{
    if (this == &other)
        return *this;

    destroy_range(arr, arr + Size_);
    // NOTE: arena – no free on arr here.

    arr = other.arr;
    Size_ = other.Size_;
    Cap_ = other.Cap_;

    other.arr = nullptr;
    other.Size_ = 0;
    other.Cap_ = 0;

    return *this;
}

template<typename T>
void Array<T>::push(T const& val)
{
    ensure_push_capacity();
    if constexpr (TRIVIAL_COPY)
        arr[Size_] = val;
    else
        ::new (static_cast<void*>(arr + Size_)) T(val);
    ++Size_;
}

template<typename T>
void Array<T>::push(T&& val)
{
    ensure_push_capacity();
    if constexpr (TRIVIAL_COPY)
        arr[Size_] = std::move(val);
    else
        ::new (static_cast<void*>(arr + Size_)) T(std::move(val));
    ++Size_;
}

template<typename T>
T Array<T>::pop()
{
    assert(Size_ > 0 && "Array::pop — array is empty");
    --Size_;
    if constexpr (TRIVIAL_DTOR)
        return arr[Size_];
    else {
        T val(std::move(arr[Size_]));
        arr[Size_].~T();
        return val;
    }
}

template<typename T>
void Array<T>::reserve(uint32_t const s)
{
    if (s <= Cap_)
        return;

    uint32_t const rounded = nextCapacity(s);
    T* new_arr = getAllocator().allocateArray<T>(rounded);
    assert(new_arr != nullptr);

    if (arr && Size_ > 0)
        relocate(new_arr, arr, Size_);

    // NOTE: arena – no free on arr here.
    arr = new_arr;
    Cap_ = rounded;
}

template<typename T>
void Array<T>::resize(uint32_t const s)
{
    if (s < Size_) {
        destroy_range(arr + s, arr + Size_);
        Size_ = s;
    } else if (s > Size_) {
        if (s > Cap_)
            reserve(s);

        uint32_t i = Size_;
        try {
            if constexpr (TRIVIAL_COPY) {
                for (; i < s; ++i)
                    arr[i] = T { };
            } else {
                for (; i < s; ++i)
                    ::new (static_cast<void*>(arr + i)) T();
            }
        } catch (...) {
            Size_ = i;
            throw;
        }
        Size_ = s;
    }
}

template<typename T>
void Array<T>::erase(uint32_t const at)
{
    if (at >= Size_)
        diagnostic::emit("out of bounds access", diagnostic::Severity::FATAL);

    if constexpr (TRIVIAL_COPY) {
        uint32_t const remaining = Size_ - at - 1;
        if (remaining > 0)
            ::memmove(arr + at, arr + at + 1, remaining * sizeof(T));
    } else {
        arr[at].~T();
        for (uint32_t i = at; i < Size_ - 1; ++i) {
            ::new (static_cast<void*>(arr + i)) T(std::move(arr[i + 1]));
            arr[i + 1].~T();
        }
    }

    --Size_;
}

template<typename T>
T* Array<T>::erase(T const* p)
{
    if (p < arr || p >= arr + Size_)
        return const_cast<T*>(p);

    T* ptr = const_cast<T*>(p);
    uint32_t remaining = static_cast<uint32_t>((arr + Size_) - (ptr + 1));

    if constexpr (TRIVIAL_COPY) {
        if (remaining > 0)
            ::memmove(ptr, ptr + 1, remaining * sizeof(T));
    } else {
        ptr->~T();
        for (uint32_t i = 0; i < remaining; ++i) {
            ::new (static_cast<void*>(ptr + i)) T(std::move(ptr[i + 1]));
            ptr[i + 1].~T();
        }
    }

    --Size_;
    return ptr;
}

template<typename T, typename... Args>
static Array<T> makeArray(Args&&... args)
{
    return getAllocator().allocateArray<T>(std::forward<Args>(args)...);
}

} // namespace mylang

#endif // ARRAY_HPP
