#ifndef ARRAY_HPP
#define ARRAY_HPP

#include "farena.hpp"
#include "fdiagnostic.hpp"
#include "fmacros.hpp"

#include <assert.h>
#include <bit>
#include <initializer_list>
#include <type_traits>
#include <concepts>

namespace fairuz {

using ArrayErrorCode = diagnostic::errc::container::Code;
using GenericErrorCode = diagnostic::errc::general::Code;

template<typename T, class _Alloc = Fa_ArenaAllocator>
class Fa_Array {
    static constexpr u32 ARRAY_MAX = __UINT32_MAX__;
    static constexpr u32 DEFAULT_CAP = 8;
    static constexpr bool TRIVIAL_COPY = std::is_trivially_copyable_v<T>;
    static constexpr bool TRIVIAL_DTOR = std::is_trivially_destructible_v<T>;
    static constexpr bool IS_ARENA = std::is_same_v<_Alloc, Fa_ArenaAllocator>;

    static void destroy_range(T* first, T* last) noexcept
    {
        if constexpr (!TRIVIAL_DTOR) {
            for (; first != last; first += 1)
                first->~T();
        }
    }

    static void relocate(T* dst, T* src, u32 n) noexcept
    {
        if (n == 0)
            return;

        if constexpr (TRIVIAL_COPY) {
            ::memcpy(static_cast<void*>(dst), src, n * sizeof(T));
        } else {
            for (u32 i = 0; i < n; i += 1) {
                ::new (static_cast<void*>(dst + i)) T(std::move(src[i]));
                src[i].~T();
            }
        }
    }

    static void copy_construct_range(T* dst, T const* src, u32 n)
    {
        if (n == 0)
            return;

        if constexpr (TRIVIAL_COPY) {
            ::memcpy(static_cast<void*>(dst), src, n * sizeof(T));
        } else {
            u32 i = 0;
            try {
                for (; i < n; i += 1)
                    ::new (static_cast<void*>(dst + i)) T(src[i]);
            } catch (...) {
                destroy_range(dst, dst + i);
                diagnostic::panic(GenericErrorCode::INTERNAL_ERROR);
            }
        }
    }

    T* m_arr { nullptr };
    u32 m_size { 0 };
    u32 m_cap { 0 };
    _Alloc* m_allocator { nullptr };

    static u32 next_capacity(u32 const s) noexcept
    {
        if (s <= DEFAULT_CAP)
            return DEFAULT_CAP;
        if (s < 64)
            return 1u << std::bit_width(s - 1); // power-of-2 for small

        return s + (s >> 1); // 1.5x for larger
    }

    void ensure_push_capacity();

    // -- allocator resolution, split at compile time instead of runtime --
    //
    // For _Alloc == Fa_ArenaAllocator: allocator may be null; falls back to
    // get_allocator_ptr(). This overload only participates in overload
    // resolution when IS_ARENA is true.
    void resolve_allocator(_Alloc* allocator)
        requires IS_ARENA
    {
        m_allocator = (allocator != nullptr) ? allocator : get_allocator_ptr();
        assert(m_allocator != nullptr && "get_allocator_ptr() returned null for Fa_ArenaAllocator");
    }

    // For any other _Alloc: allocator must be non-null. This overload only
    // participates when IS_ARENA is false, so a caller who forgets the
    // allocator gets a compile error at the constructor call site (see
    // below), not a runtime assert here.
    void resolve_allocator(_Alloc* allocator)
        requires (!IS_ARENA)
    {
        assert(allocator != nullptr && "null allocator only valid for Fa_ArenaAllocator");
        m_allocator = allocator;
    }

public:
    // Default/allocator constructor: split into two overloads so that the
    // allocator parameter is *optional* only for Fa_ArenaAllocator. For any
    // other _Alloc, omitting it is a hard compile error rather than a
    // runtime assert/fprintf — the contract violation is caught by the
    // compiler at the call site, so "which instance violated it" becomes
    // "which line failed to build."
    Fa_Array()
        requires IS_ARENA
    {
        resolve_allocator(nullptr);
    }

    explicit Fa_Array(_Alloc* allocator)
    {
        // Works for both branches: arena accepts an explicit pointer too,
        // non-arena requires it. Only ever called with a non-null pointer
        // in the non-arena case because there is no default-constructible
        // overload for non-arena _Alloc (see above).
        resolve_allocator(allocator);
    }

    explicit Fa_Array(u32 capacity, T fill_v, _Alloc* allocator)
        requires (!IS_ARENA);
    explicit Fa_Array(u32 capacity, T fill_v = T(), _Alloc* allocator = nullptr)
        requires IS_ARENA;

    Fa_Array(Fa_Array const& other);
    Fa_Array(Fa_Array&& other) noexcept;

    Fa_Array(std::initializer_list<T> list, _Alloc* allocator)
        requires (!IS_ARENA);
    Fa_Array(std::initializer_list<T> list, _Alloc* allocator = nullptr)
        requires IS_ARENA;

    Fa_Array& operator=(Fa_Array const& other);
    Fa_Array& operator=(Fa_Array&& other) noexcept;

    [[nodiscard]] bool operator==(Fa_Array const& other) const
    {
        return m_size == other.m_size && std::equal(m_arr, m_arr + m_size, other.m_arr);
    }

    ~Fa_Array()
    {
        destroy_range(m_arr, m_arr + m_size);
        if constexpr (!IS_ARENA)
            m_allocator->deallocate(m_arr, m_cap);
    }

    static Fa_Array with_capacity(u32 capacity)
        requires IS_ARENA
    {
        Fa_Array a;
        if (capacity == 0)
            return a;

        a.m_arr = a.m_allocator->template allocate_array<T>(capacity);
        assert(a.m_arr != nullptr);
        a.m_cap = capacity;
        return a;
    }

    static Fa_Array with_capacity(u32 capacity, _Alloc* allocator)
        requires (!IS_ARENA)
    {
        Fa_Array a(allocator);
        if (capacity == 0)
            return a;

        a.m_arr = a.m_allocator->template allocate_array<T>(capacity);
        assert(a.m_arr != nullptr);
        a.m_cap = capacity;
        return a;
    }

    template<typename... Args>
    T& emplace(Args&&... args)
    {
        ensure_push_capacity();
        T* slot = m_arr + m_size;

        if constexpr (TRIVIAL_COPY)
            *slot = T(std::forward<Args>(args)...);
        else
            ::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);

        m_size += 1;
        return *slot;
    }

    void clear() { m_size = 0; }

    void push(T const& val);
    void push(T&& val);
    T pop();
    void reserve(u32 const s);
    void resize(u32 const s);
    void erase(u32 const at);
    T* erase(T const* p);
    T& operator[](u32 i) { return m_arr[i]; }
    T const& operator[](u32 i) const { return m_arr[i]; }

    T& back()
    {
        if (m_size == 0)
            diagnostic::fatal_error(ArrayErrorCode::ARRAY_EMPTY_BACK);

        return m_arr[m_size - 1];
    }

    T const& back() const
    {
        if (m_size == 0)
            diagnostic::fatal_error(ArrayErrorCode::ARRAY_EMPTY_BACK);

        return m_arr[m_size - 1];
    }

    T& front()
    {
        if (m_size == 0)
            diagnostic::fatal_error(ArrayErrorCode::ARRAY_EMPTY_FRONT);

        return m_arr[0];
    }

    T const& front() const
    {
        if (m_size == 0)
            diagnostic::fatal_error(ArrayErrorCode::ARRAY_EMPTY_FRONT);

        return m_arr[0];
    }

    u32 size() const noexcept { return m_size; }
    u32 cap() const noexcept { return m_cap; }
    T* data() noexcept { return m_arr; }
    T const* data() const noexcept { return m_arr; }
    bool empty() const noexcept { return m_size == 0; }
    bool full() const noexcept { return m_size == m_cap; }
    T* begin() noexcept { return m_arr; }
    T* end() noexcept { return m_arr + m_size; }
    T const* begin() const noexcept { return m_arr; }
    T const* end() const noexcept { return m_arr + m_size; }
}; // class Fa_Array

template<typename T, class _Alloc>
void Fa_Array<T, _Alloc>::ensure_push_capacity()
{
    if (m_size < m_cap)
        return;

    u32 new_cap = m_cap == 0 ? DEFAULT_CAP : m_cap + (m_cap >> 1);
    T* new_arr = m_allocator->template allocate_array<T>(new_cap);
    if (m_arr && m_size > 0)
        relocate(new_arr, m_arr, m_size);

    m_arr = new_arr;
    m_cap = new_cap;
}

template<typename T, class _Alloc>
Fa_Array<T, _Alloc>::Fa_Array(u32 capacity, T fill_v, _Alloc* allocator)
    requires (!Fa_Array<T, _Alloc>::IS_ARENA)
{
    resolve_allocator(allocator);
    if (capacity > ARRAY_MAX)
        diagnostic::fatal_error(ArrayErrorCode::ARRAY_CAPACITY_EXCEEDED,
            std::to_string(capacity) + " > " + std::to_string(ARRAY_MAX));

    if (capacity == 0)
        return;

    m_arr = m_allocator->template allocate_array<T>(capacity);
    m_cap = capacity;

    u32 i = 0;
    try {
        if constexpr (TRIVIAL_COPY) {
            for (; i < capacity; i += 1)
                m_arr[i] = fill_v;
        } else {
            for (; i < capacity; i += 1)
                ::new (static_cast<void*>(m_arr + i)) T(fill_v);
        }
    } catch (...) {
        destroy_range(m_arr, m_arr + i);
        diagnostic::fatal_error(GenericErrorCode::INTERNAL_ERROR);
    }

    m_size = capacity;
}

template<typename T, class _Alloc>
Fa_Array<T, _Alloc>::Fa_Array(u32 capacity, T fill_v, _Alloc* allocator)
    requires Fa_Array<T, _Alloc>::IS_ARENA
{
    resolve_allocator(allocator);
    if (capacity > ARRAY_MAX)
        diagnostic::fatal_error(ArrayErrorCode::ARRAY_CAPACITY_EXCEEDED,
            std::to_string(capacity) + " > " + std::to_string(ARRAY_MAX));

    if (capacity == 0)
        return;

    m_arr = m_allocator->template allocate_array<T>(capacity);
    m_cap = capacity;

    u32 i = 0;
    try {
        if constexpr (TRIVIAL_COPY) {
            for (; i < capacity; i += 1)
                m_arr[i] = fill_v;
        } else {
            for (; i < capacity; i += 1)
                ::new (static_cast<void*>(m_arr + i)) T(fill_v);
        }
    } catch (...) {
        destroy_range(m_arr, m_arr + i);
        diagnostic::fatal_error(GenericErrorCode::INTERNAL_ERROR);
    }

    m_size = capacity;
}

template<typename T, class _Alloc>
Fa_Array<T, _Alloc>::Fa_Array(Fa_Array const& other)
    : m_cap(other.m_cap)
    , m_allocator(other.m_allocator)
{
    if (m_cap == 0)
        return;

    m_arr = m_allocator->template allocate_array<T>(m_cap);
    assert(m_arr != nullptr);
    copy_construct_range(m_arr, other.m_arr, other.m_size);
    m_size = other.m_size;
}

template<typename T, class _Alloc>
Fa_Array<T, _Alloc>::Fa_Array(Fa_Array&& other) noexcept
    : m_arr(other.m_arr)
    , m_size(other.m_size)
    , m_cap(other.m_cap)
    , m_allocator(other.m_allocator)
{
    other.m_arr = nullptr;
    other.m_size = 0;
    other.m_cap = 0;
}

template<typename T, class _Alloc>
Fa_Array<T, _Alloc>::Fa_Array(std::initializer_list<T> list, _Alloc* allocator)
    requires (!Fa_Array<T, _Alloc>::IS_ARENA)
{
    resolve_allocator(allocator);
    if (list.size() == 0)
        return;

    m_size = static_cast<u32>(list.size());
    m_cap = next_capacity(m_size);
    m_arr = m_allocator->template allocate_array<T>(m_cap);
    assert(m_arr != nullptr);

    u32 i = 0;
    try {
        if constexpr (TRIVIAL_COPY) {
            for (T const& val : list) {
                m_arr[i] = val;
                i += 1;
            }
        } else {
            for (T const& val : list) {
                ::new (static_cast<void*>(m_arr + i)) T(val);
                i += 1;
            }
        }
    } catch (...) {
        destroy_range(m_arr, m_arr + i);
        m_size = 0;
        diagnostic::fatal_error(GenericErrorCode::INTERNAL_ERROR);
    }
}

template<typename T, class _Alloc>
Fa_Array<T, _Alloc>::Fa_Array(std::initializer_list<T> list, _Alloc* allocator)
    requires Fa_Array<T, _Alloc>::IS_ARENA
{
    resolve_allocator(allocator);
    if (list.size() == 0)
        return;

    m_size = static_cast<u32>(list.size());
    m_cap = next_capacity(m_size);
    m_arr = m_allocator->template allocate_array<T>(m_cap);
    assert(m_arr != nullptr);

    u32 i = 0;
    try {
        if constexpr (TRIVIAL_COPY) {
            for (T const& val : list) {
                m_arr[i] = val;
                i += 1;
            }
        } else {
            for (T const& val : list) {
                ::new (static_cast<void*>(m_arr + i)) T(val);
                i += 1;
            }
        }
    } catch (...) {
        destroy_range(m_arr, m_arr + i);
        m_size = 0;
        diagnostic::fatal_error(GenericErrorCode::INTERNAL_ERROR);
    }
}

template<typename T, class _Alloc>
Fa_Array<T, _Alloc>& Fa_Array<T, _Alloc>::operator=(Fa_Array const& other)
{
    if (this == &other)
        return *this;

    destroy_range(m_arr, m_arr + m_size);
    m_size = 0;

    m_allocator = other.m_allocator;

    if (other.m_cap > m_cap) {
        // NOTE: arena – no free on m_arr here.
        m_arr = m_allocator->template allocate_array<T>(other.m_cap);
        assert(m_arr != nullptr);
        m_cap = other.m_cap;
    }

    copy_construct_range(m_arr, other.m_arr, other.m_size);
    m_size = other.m_size;
    return *this;
}

template<typename T, class _Alloc>
Fa_Array<T, _Alloc>& Fa_Array<T, _Alloc>::operator=(Fa_Array&& other) noexcept
{
    if (this == &other)
        return *this;

    destroy_range(m_arr, m_arr + m_size);
    // NOTE: arena – no free on m_arr here.

    m_arr = other.m_arr;
    m_size = other.m_size;
    m_cap = other.m_cap;
    m_allocator = other.m_allocator;

    other.m_allocator = nullptr;
    other.m_arr = nullptr;
    other.m_size = 0;
    other.m_cap = 0;

    return *this;
}

template<typename T, class _Alloc>
void Fa_Array<T, _Alloc>::push(T const& val)
{
    ensure_push_capacity();
    if constexpr (TRIVIAL_COPY)
        m_arr[m_size] = val;
    else
        ::new (static_cast<void*>(m_arr + m_size)) T(val);
    m_size += 1;
}

template<typename T, class _Alloc>
void Fa_Array<T, _Alloc>::push(T&& val)
{
    ensure_push_capacity();
    if constexpr (TRIVIAL_COPY)
        m_arr[m_size] = std::move(val);
    else
        ::new (static_cast<void*>(m_arr + m_size)) T(std::move(val));
    m_size += 1;
}

template<typename T, class _Alloc>
T Fa_Array<T, _Alloc>::pop()
{
    assert(m_size > 0 && "Fa_Array::pop — array is empty");
    m_size -= 1;
    if constexpr (TRIVIAL_DTOR)
        return m_arr[m_size];
    else {
        T val(std::move(m_arr[m_size]));
        m_arr[m_size].~T();
        return val;
    }
}

template<typename T, class _Alloc>
void Fa_Array<T, _Alloc>::reserve(u32 const s)
{
    if (s <= m_cap)
        return;

    u32 const rounded = next_capacity(s);
    T* new_arr = m_allocator->template allocate_array<T>(rounded);
    assert(new_arr != nullptr);

    if (m_arr && m_size > 0)
        relocate(new_arr, m_arr, m_size);

    /// NOTE: arena – no free on m_arr here
    m_arr = new_arr;
    m_cap = rounded;
}

template<typename T, class _Alloc>
void Fa_Array<T, _Alloc>::resize(u32 const s)
{
    if (s < m_size) {
        destroy_range(m_arr + s, m_arr + m_size);
        m_size = s;
    } else if (s > m_size) {
        if (s > m_cap)
            reserve(s);

        u32 i = m_size;
        try {
            if constexpr (TRIVIAL_COPY) {
                for (; i < s; i += 1)
                    m_arr[i] = T { };
            } else {
                for (; i < s; i += 1)
                    ::new (static_cast<void*>(m_arr + i)) T();
            }
        } catch (...) {
            m_size = i;
            diagnostic::fatal_error(GenericErrorCode::INTERNAL_ERROR);
        }

        m_size = s;
    }
}

template<typename T, class _Alloc>
void Fa_Array<T, _Alloc>::erase(u32 const at)
{
    if (at >= m_size)
        diagnostic::fatal_error(ArrayErrorCode::ARRAY_OUT_OF_BOUNDS);

    if constexpr (TRIVIAL_COPY) {
        u32 const remaining = m_size - at - 1;
        if (remaining > 0)
            ::memmove(m_arr + at, m_arr + at + 1, remaining * sizeof(T));
    } else {
        m_arr[at].~T();
        for (u32 i = at; i < m_size - 1; i += 1) {
            ::new (static_cast<void*>(m_arr + i)) T(std::move(m_arr[i + 1]));
            m_arr[i + 1].~T();
        }
    }

    m_size -= 1;
}

template<typename T, class _Alloc>
T* Fa_Array<T, _Alloc>::erase(T const* p)
{
    if (p < m_arr || p >= m_arr + m_size)
        return const_cast<T*>(p);

    T* ptr = const_cast<T*>(p);
    u32 remaining = static_cast<u32>((m_arr + m_size) - (ptr + 1));

    if constexpr (TRIVIAL_COPY) {
        if (remaining > 0)
            ::memmove(ptr, ptr + 1, remaining * sizeof(T));
    } else {
        ptr->~T();
        for (u32 i = 0; i < remaining; i += 1) {
            ::new (static_cast<void*>(ptr + i)) T(std::move(ptr[i + 1]));
            ptr[i + 1].~T();
        }
    }

    m_size -= 1;
    return ptr;
}

template<typename T, typename... Args>
static Fa_Array<T> make_array(Args&&... args)
{
    return get_allocator().allocate_array<T>(std::forward<Args>(args)...);
}

template<typename T, class _Alloc = Fa_ArenaAllocator>
class Fa_Set {
private:
    Fa_Array<T, _Alloc> m_arr;

public:
    Fa_Set() = default;

    bool array_contains(T const& val) const
    {
        for (u32 i = 0, n = m_arr.size(); i < n; ++i) {
            if (m_arr[i] == val)
                return true;
        }
        return false;
    }

    void push(T&& val)
    {
        if (array_contains(val))
            return;
        m_arr.push(val);
    }

    void push(T const& val)
    {
        if (array_contains(val))
            return;
        m_arr.push(val);
    }

    void pop() { m_arr.pop(); }
    void clear() { m_arr.clear(); }

    u32 size() const { return m_arr.size(); }
    bool empty() const { return m_arr.empty(); }

    T& back() { return m_arr.back(); }
    T const& back() const { return m_arr.back(); }

    void erase(T const* p) { m_arr.erase(p); }
    void erase(u32 at) { m_arr.erase(at); }

    T& operator[](u32 const at) { return m_arr[at]; }
    T operator[](u32 const at) const { return m_arr[at]; }
};

} // namespace fairuz

#endif // ARRAY_HPP