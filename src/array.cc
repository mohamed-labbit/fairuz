#include "../include/array.hpp"


namespace mylang {

template<typename T>
void Array<T>::grow(uint32_t const new_cap)
{
    if (new_cap <= Cap_)
        return;

    uint32_t const rounded = nextCapacity(new_cap);
    T* new_arr = getRuntimeAllocator().allocateArray<T>(rounded);
    assert(new_arr != nullptr);

    if (arr && Size_ > 0)
        relocate(new_arr, arr, Size_);

    // NOTE: arena – no free on arr here.
    arr = new_arr;
    Cap_ = rounded;
}

template<typename T>
void Array<T>::ensure_push_capacity()
{
    if (Size_ >= Cap_)
        grow(Cap_ == 0 ? DEFAULT_CAP : Cap_ * 2);
}

template<typename T>
Array<T>::Array(uint32_t capacity, T fill_v)
{
    if (capacity > ARRAY_MAX)
        diagnostic::emit(
            "Requested capacity " + std::to_string(capacity) + " exceeds maximum " + std::to_string(ARRAY_MAX),
            diagnostic::Severity::FATAL);

    if (capacity == 0)
        return;

    arr = getRuntimeAllocator().allocateArray<T>(capacity);
    assert(arr != nullptr);
    Cap_ = capacity;

    uint32_t i = 0;
    try {
        if constexpr (trivial_copy) {
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

template <typename T>
Array<T>::Array(Array const& other)
    : Cap_(other.Cap_)
{
    if (Cap_ == 0)
        return;

    arr = getRuntimeAllocator().allocateArray<T>(Cap_);
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
    arr = getRuntimeAllocator().allocateArray<T>(Cap_);
    assert(arr != nullptr);

    uint32_t i = 0;
    try {
        if constexpr (trivial_copy) {
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
        arr = getRuntimeAllocator().allocateArray<T>(other.Cap_);
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



} // namespace mylang
