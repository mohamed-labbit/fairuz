#ifndef ARRAY_HPP
#define ARRAY_HPP

#include "allocator.hpp"
#include "diagnostic.hpp"
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace mylang {

// Array<T>
//
// A dynamic array backed by the runtime allocator.  Correctly handles both
// trivially-copyable types (uses memcpy/memmove fast paths) and
// non-trivially-copyable types (uses placement-new, explicit destructors,
// and move semantics).
//
// Memory lifetime follows the arena: the backing buffer is never freed by
// this class.  If individual-free support is added to the allocator, insert
// the corresponding calls at each "NOTE: arena – no free" comment.

template<typename T>
class Array {
    static constexpr uint32_t ARRAY_MAX = __UINT32_MAX__;
    static constexpr uint32_t DEFAULT_CAP = 8;

    // Convenience predicates — resolved at compile time, no runtime cost.
    static constexpr bool trivial_copy = std::is_trivially_copyable_v<T>;
    static constexpr bool trivial_dtor = std::is_trivially_destructible_v<T>;

    // -----------------------------------------------------------------------
    // Raw-storage helpers
    //
    // The allocator hands us a T* but for non-trivial T we treat the backing
    // memory as uninitialized bytes and drive construction/destruction ourselves
    // via placement new.  This avoids default-constructing every element on
    // allocation — matching the behaviour you'd expect from std::vector.
    // -----------------------------------------------------------------------

    // Destroy elements [first, last).  Compiled away for trivially-destructible T.
    static void destroy_range(T* first, T* last) noexcept
    {
        if constexpr (!trivial_dtor) {
            for (; first != last; ++first)
                first->~T();
        }
    }

    // Relocate n elements from src → dst (uninitialized destination).
    // Trivial T: single memcpy.
    // Non-trivial T: move-construct into dst, then destroy src.
    static void relocate(T* dst, T* src, uint32_t n) noexcept
    {
        if (n == 0)
            return;
        if constexpr (trivial_copy) {
            ::memcpy(static_cast<void*>(dst), src, n * sizeof(T));
        } else {
            for (uint32_t i = 0; i < n; ++i) {
                ::new (static_cast<void*>(dst + i)) T(std::move(src[i]));
                src[i].~T();
            }
        }
    }

    // Copy-construct n elements from src into uninitialized dst.
    // Provides strong exception guarantee: destroys already-constructed
    // elements if a copy throws.
    static void copy_construct_range(T* dst, T const* src, uint32_t n)
    {
        if (n == 0)
            return;
        if constexpr (trivial_copy) {
            ::memcpy(static_cast<void*>(dst), src, n * sizeof(T));
        } else {
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
        return 1u << std::bit_width(s - 1); // smallest power-of-two >= s
    }

    // Ensure the backing store can hold at least new_cap elements.
    // Live elements are relocated (moved for non-trivial T) into the new buffer.
    void grow(uint32_t const new_cap);

    void ensure_push_capacity();
public:
    Array() = default;
    // Fill constructor: size == capacity, every element copy-constructed from fill_v.
    explicit Array(uint32_t capacity, T fill_v = T());
    // Copy constructor
    Array(Array const& other);
    // Move constructor — O(1), no allocation, no element construction
    Array(Array&& other) noexcept;
    // Initializer-list constructor
    Array(std::initializer_list<T> list);
    
    Array& operator=(Array const& other);
    Array& operator=(Array&& other) noexcept;

    // Destructor: destroy live elements; buffer lifetime owned by arena.
    ~Array()
    {
        destroy_range(arr, arr + Size_);
        // NOTE: arena – no free on arr here.
    }

    static Array withCapacity(uint32_t capacity)
    {
        Array a;
        if (capacity == 0)
            return a;
        a.arr = getRuntimeAllocator().allocateArray<T>(capacity);
        assert(a.arr != nullptr);
        a.Cap_ = capacity;
        return a;
    }

    template<typename... Args>
    T& emplace(Args&&... args)
    {
        ensure_push_capacity();
        T* slot = arr + Size_;
        if constexpr (trivial_copy) {
            *slot = T(std::forward<Args>(args)...);
        } else {
            ::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);
        }
        ++Size_;
        return *slot;
    }

    void push(T const& val)
    {
        ensure_push_capacity();
        if constexpr (trivial_copy) {
            arr[Size_] = val;
        } else {
            ::new (static_cast<void*>(arr + Size_)) T(val);
        }
        ++Size_;
    }

    void push(T&& val)
    {
        ensure_push_capacity();
        if constexpr (trivial_copy) {
            arr[Size_] = std::move(val);
        } else {
            ::new (static_cast<void*>(arr + Size_)) T(std::move(val));
        }
        ++Size_;
    }

    T pop()
    {
        assert(Size_ > 0 && "Array::pop — array is empty");
        --Size_;
        if constexpr (trivial_dtor) {
            return arr[Size_];
        } else {
            T val(std::move(arr[Size_]));
            arr[Size_].~T();
            return val;
        }
    }

    // reserve: ensure capacity for at least s elements without changing Size_.
    void reserve(uint32_t const s) { grow(s); }

    // resize: set logical size to s.
    //   Shrinking: destroys excess elements.
    //   Growing:   default-constructs new elements.
    void resize(uint32_t const s)
    {
        if (s < Size_) {
            destroy_range(arr + s, arr + Size_);
            Size_ = s;
        } else if (s > Size_) {
            if (s > Cap_)
                grow(s);
            uint32_t i = Size_;
            try {
                if constexpr (trivial_copy) {
                    for (; i < s; ++i)
                        arr[i] = T { };
                } else {
                    for (; i < s; ++i)
                        ::new (static_cast<void*>(arr + i)) T();
                }
            } catch (...) {
                Size_ = i; // keep Size_ accurate so ~Array() cleans up correctly
                throw;
            }
            Size_ = s;
        }
    }

    void erase(uint32_t const at)
    {
        if (at >= Size_)
            diagnostic::emit("out of bounds access", diagnostic::Severity::FATAL);

        if constexpr (trivial_copy) {
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

    T* erase(T const* p)
    {
        if (p < arr || p >= arr + Size_)
            return const_cast<T*>(p);

        T* ptr = const_cast<T*>(p);
        uint32_t remaining = static_cast<uint32_t>((arr + Size_) - (ptr + 1));

        if constexpr (trivial_copy) {
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

    T& operator[](uint32_t i)
    {
        if (i >= Size_)
            diagnostic::emit("Array::operator[] - index out of bounds");
        return arr[i];
    }

    T const& operator[](uint32_t i) const
    {
        if (i >= Size_)
            diagnostic::emit("Array::operator[] - index out of bounds");
        return arr[i];
    }

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

template<typename T, typename... Args>
static Array<T> makeArray(Args&&... args) { return getRuntimeAllocator().allocateArray<T>(std::forward<Args>(args)...); }

} // namespace mylang

#endif // ARRAY_HPP
