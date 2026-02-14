#pragma once

#include "../runtime/allocator/arena.hpp"

namespace mylang {

struct StringAllocator {
private:
    runtime::allocator::ArenaAllocator Allocator_;

public:
    StringAllocator()
        : Allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::EXPONENTIAL))
    {
        Allocator_.setName("String Allocator");
    }

    void* allocateBytes(SizeType const size)
    {
        void* mem = Allocator_.allocate(size);
        if (!mem)
            throw std::bad_alloc();
        return mem;
    }

    template<typename T>
    T* allocateArray(SizeType const count) { return static_cast<T*>(allocateBytes(count * sizeof(T))); }

    template<typename T>
    void deallocateArray(T* p, SizeType const count) { Allocator_.deallocate((void*)p, count * sizeof(T)); }

    template<typename T, typename... Args>
    MYLANG_NODISCARD T* allocateObject(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible with Args...");
        void* mem = allocateBytes(sizeof(T));
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    template<typename T>
    void deallocateObject(T* obj) { Allocator_.deallocate((void*)obj, sizeof(T)); }

    std::string toString(bool verbose) const { return Allocator_.toString(verbose); }
};

inline StringAllocator string_allocator;

}
