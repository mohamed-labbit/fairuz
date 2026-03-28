#ifndef ARRAY_HPP
#define ARRAY_HPP

#include "arena.hpp"
#include "diagnostic.hpp"

#include <bit>
#include <cassert>

namespace fairuz {

template<typename T>
class Fa_Array {
    static constexpr u32 ARRAY_MAX = __UINT32_MAX__;
    static constexpr u32 DEFAULT_CAP = 8;
    static constexpr bool TRIVIAL_COPY = std::is_trivially_copyable_v<T>;
    static constexpr bool TRIVIAL_DTOR = std::is_trivially_destructible_v<T>;

    static void destroy_range(T* first, T* last) noexcept
    {
        if constexpr (!TRIVIAL_DTOR) {
            for (; first != last; ++first)
                first->~T();
        }
    }

    static void relocate(T* dst, T* src, u32 n) noexcept
    {
        if (n == 0)
            return;

        if constexpr (TRIVIAL_COPY)
            ::memcpy(static_cast<void*>(dst), src, n * sizeof(T));
        else {
            for (u32 i = 0; i < n; ++i) {
                ::new (static_cast<void*>(dst + i)) T(std::move(src[i]));
                src[i].~T();
            }
        }
    }

    static void copy_construct_range(T* dst, T const* src, u32 n)
    {
        if (n == 0)
            return;

        if constexpr (TRIVIAL_COPY)
            ::memcpy(static_cast<void*>(dst), src, n * sizeof(T));
        else {
            u32 i = 0;
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
    u32 Size_ { 0 };
    u32 Cap_ { 0 };

    static u32 nextCapacity(u32 const s) noexcept
    {
        if (s <= DEFAULT_CAP)
            return DEFAULT_CAP;
        if (s < 64)
            return 1u << std::bit_width(s - 1); // power-of-2 for small
        return s + (s >> 1);                    // 1.5x for larger
    }

    void ensure_push_capacity();

public:
    Fa_Array() = default;
    explicit Fa_Array(u32 capacity, T fill_v = T());
    Fa_Array(Fa_Array const& other);
    Fa_Array(Fa_Array&& other) noexcept;
    Fa_Array(std::initializer_list<T> list);

    Fa_Array& operator=(Fa_Array const& other);
    Fa_Array& operator=(Fa_Array&& other) noexcept;

    ~Fa_Array() { destroy_range(arr, arr + Size_); }

    static Fa_Array withCapacity(u32 capacity)
    {
        Fa_Array a;
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

    void clear() { Size_ = 0; }

    void push(T const& val);
    void push(T&& val);
    T pop();
    void reserve(u32 const s);
    void resize(u32 const s);
    void erase(u32 const at);
    T* erase(T const* p);
    T& operator[](u32 i) { return arr[i]; }
    T const& operator[](u32 i) const { return arr[i]; }

    T& back()
    {
        if (Size_ == 0)
            diagnostic::emit(diagnostic::errc::container::Code::ARRAY_EMPTY_BACK, diagnostic::Severity::FATAL);

        return arr[Size_ - 1];
    }

    T const& back() const
    {
        if (Size_ == 0)
            diagnostic::emit(diagnostic::errc::container::Code::ARRAY_EMPTY_BACK, diagnostic::Severity::FATAL);

        return arr[Size_ - 1];
    }

    T& front()
    {
        if (Size_ == 0)
            diagnostic::emit(diagnostic::errc::container::Code::ARRAY_EMPTY_FRONT, diagnostic::Severity::FATAL);

        return arr[0];
    }

    T const& front() const
    {
        if (Size_ == 0)
            diagnostic::emit(diagnostic::errc::container::Code::ARRAY_EMPTY_FRONT, diagnostic::Severity::FATAL);

        return arr[0];
    }

    u32 size() const noexcept { return Size_; }
    u32 cap() const noexcept { return Cap_; }
    T* data() noexcept { return arr; }
    T const* data() const noexcept { return arr; }
    bool empty() const noexcept { return Size_ == 0; }
    bool full() const noexcept { return Size_ == Cap_; }
    T* begin() noexcept { return arr; }
    T* end() noexcept { return arr + Size_; }
    T const* begin() const noexcept { return arr; }
    T const* end() const noexcept { return arr + Size_; }
}; // class Fa_Array

template<typename T>
void Fa_Array<T>::ensure_push_capacity()
{
    if (Size_ < Cap_)
        return;
    u32 new_cap = Cap_ == 0 ? DEFAULT_CAP : Cap_ + (Cap_ >> 1);
    // allocate new_cap directly, skip the nextCapacity rounding
    T* new_arr = getAllocator().allocateArray<T>(new_cap);
    if (arr && Size_ > 0)
        relocate(new_arr, arr, Size_);
    arr = new_arr;
    Cap_ = new_cap;
}

template<typename T>
Fa_Array<T>::Fa_Array(u32 capacity, T fill_v)
{
    if (capacity > ARRAY_MAX)
        diagnostic::emit(diagnostic::errc::container::Code::ARRAY_CAPACITY_EXCEEDED,
            std::to_string(capacity) + " > " + std::to_string(ARRAY_MAX),
            diagnostic::Severity::FATAL);

    if (capacity == 0)
        return;

    arr = getAllocator().allocateArray<T>(capacity);
    Cap_ = capacity;

    u32 i = 0;
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
Fa_Array<T>::Fa_Array(Fa_Array const& other)
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
Fa_Array<T>::Fa_Array(Fa_Array&& other) noexcept
    : arr(other.arr)
    , Size_(other.Size_)
    , Cap_(other.Cap_)
{
    other.arr = nullptr;
    other.Size_ = 0;
    other.Cap_ = 0;
}

template<typename T>
Fa_Array<T>::Fa_Array(std::initializer_list<T> list)
{
    if (list.size() == 0)
        return;

    Size_ = static_cast<u32>(list.size());
    Cap_ = nextCapacity(Size_);
    arr = getAllocator().allocateArray<T>(Cap_);
    assert(arr != nullptr);

    u32 i = 0;
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
Fa_Array<T>& Fa_Array<T>::operator=(Fa_Array const& other)
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
Fa_Array<T>& Fa_Array<T>::operator=(Fa_Array&& other) noexcept
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
void Fa_Array<T>::push(T const& val)
{
    ensure_push_capacity();
    if constexpr (TRIVIAL_COPY)
        arr[Size_] = val;
    else
        ::new (static_cast<void*>(arr + Size_)) T(val);
    ++Size_;
}

template<typename T>
void Fa_Array<T>::push(T&& val)
{
    ensure_push_capacity();
    if constexpr (TRIVIAL_COPY)
        arr[Size_] = std::move(val);
    else
        ::new (static_cast<void*>(arr + Size_)) T(std::move(val));
    ++Size_;
}

template<typename T>
T Fa_Array<T>::pop()
{
    assert(Size_ > 0 && "Fa_Array::pop — array is empty");
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
void Fa_Array<T>::reserve(u32 const s)
{
    if (s <= Cap_)
        return;

    u32 const rounded = nextCapacity(s);
    T* new_arr = getAllocator().allocateArray<T>(rounded);
    assert(new_arr != nullptr);

    if (arr && Size_ > 0)
        relocate(new_arr, arr, Size_);

    // NOTE: arena – no free on arr here.
    arr = new_arr;
    Cap_ = rounded;
}

template<typename T>
void Fa_Array<T>::resize(u32 const s)
{
    if (s < Size_) {
        destroy_range(arr + s, arr + Size_);
        Size_ = s;
    } else if (s > Size_) {
        if (s > Cap_)
            reserve(s);

        u32 i = Size_;
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
void Fa_Array<T>::erase(u32 const at)
{
    if (at >= Size_)
        diagnostic::emit(diagnostic::errc::container::Code::ARRAY_OUT_OF_BOUNDS, diagnostic::Severity::FATAL);

    if constexpr (TRIVIAL_COPY) {
        u32 const remaining = Size_ - at - 1;
        if (remaining > 0)
            ::memmove(arr + at, arr + at + 1, remaining * sizeof(T));
    } else {
        arr[at].~T();
        for (u32 i = at; i < Size_ - 1; ++i) {
            ::new (static_cast<void*>(arr + i)) T(std::move(arr[i + 1]));
            arr[i + 1].~T();
        }
    }

    --Size_;
}

template<typename T>
T* Fa_Array<T>::erase(T const* p)
{
    if (p < arr || p >= arr + Size_)
        return const_cast<T*>(p);

    T* ptr = const_cast<T*>(p);
    u32 remaining = static_cast<u32>((arr + Size_) - (ptr + 1));

    if constexpr (TRIVIAL_COPY) {
        if (remaining > 0)
            ::memmove(ptr, ptr + 1, remaining * sizeof(T));
    } else {
        ptr->~T();
        for (u32 i = 0; i < remaining; ++i) {
            ::new (static_cast<void*>(ptr + i)) T(std::move(ptr[i + 1]));
            ptr[i + 1].~T();
        }
    }

    --Size_;
    return ptr;
}

template<typename T, typename... Args>
static Fa_Array<T> makeArray(Args&&... args)
{
    return getAllocator().allocateArray<T>(std::forward<Args>(args)...);
}

} // namespace fairuz

#endif // ARRAY_HPP
