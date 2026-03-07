#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include "runtime/allocator/arena.hpp"

namespace mylang {

struct Allocator {
private:
    runtime::allocator::ArenaAllocator Allocator_;

public:
    explicit Allocator(std::string const& name = "")
        : Allocator_(static_cast<std::int32_t>(runtime::allocator::ArenaAllocator::GrowthStrategy::EXPONENTIAL))
    {
        if (!name.empty())
            Allocator_.setName(name);
    }

    void* allocateBytes(size_t const size)
    {
        void* mem = Allocator_.allocate(size);
        if (!mem)
            throw std::bad_alloc();

        return mem;
    }

    template<typename T>
    T* allocateArray(size_t const count)
    {
        return static_cast<T*>(allocateBytes(count * sizeof(T)));
    }

    template<typename T>
    void deallocateArray(T* ptr, size_t const count)
    {
        Allocator_.deallocate(static_cast<void*>(ptr), count * sizeof(T));
    }

    template<typename T, typename... Args>
    MY_NODISCARD T* allocateObject(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible with Args...");

        void* mem = allocateBytes(sizeof(T));

        return ::new (mem) T(std::forward<Args>(args)...);
    }

    template<typename T>
    void deallocateObject(T* obj)
    {
        Allocator_.deallocate((void*)obj, sizeof(T));
    }

    std::string toString(bool verbose) const
    {
        return Allocator_.toString(verbose);
    }
} __attribute__((aligned(128)));

} // namespace mylang

#endif // ALLOCATOR_HPP
