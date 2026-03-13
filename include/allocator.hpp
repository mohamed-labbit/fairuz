#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include "diagnostic.hpp"
#include "runtime/allocator/arena.hpp"
#include <cassert>
#include <stdexcept>

namespace mylang {

struct alignas(128) Allocator {
private:
    runtime::allocator::ArenaAllocator arena_;

public:
    explicit Allocator(std::string const& name = "")
        : arena_(static_cast<std::int32_t>(
              runtime::allocator::ArenaAllocator::GrowthStrategy::EXPONENTIAL))
    {
        if (!name.empty())
            arena_.setName(name);
    }

    void* allocateBytes(size_t const size)
    {
        void* mem = arena_.allocate(size);
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
        arena_.deallocate(static_cast<void*>(ptr), count * sizeof(T));
    }

    template<typename T, typename... Args>
    MY_NODISCARD T* allocateObject(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>,
            "T must be constructible with Args...");
        return ::new (allocateBytes(sizeof(T))) T(std::forward<Args>(args)...);
    }

    template<typename T>
    void deallocateObject(T* obj)
    {
        arena_.deallocate(static_cast<void*>(obj), sizeof(T));
    }

    std::string toString(bool verbose) const { return arena_.toString(verbose); }

    ~Allocator() = default;
};

struct AllocatorContext {
    Allocator string_allocator { "strings" };
    Allocator token_allocator { "tokens" };
    Allocator runtime_allocator { "runtime" };
    Allocator AST_allocator { "ast" };
    Allocator arr_allocator { "array" };
};

inline AllocatorContext* g_context = nullptr;

inline void setContext(AllocatorContext* ctx) { g_context = ctx; }

inline AllocatorContext& getContext()
{
    if (!g_context)
        diagnostic::emit("AllocatorContext not initialized — call setContext() in main first", diagnostic::Severity::FATAL);

    return *g_context;
}

inline Allocator& getStringAllocator() { return getContext().string_allocator; }
inline Allocator& getTokenAllocator() { return getContext().token_allocator; }
inline Allocator& getAstAllocator() { return getContext().AST_allocator; }
inline Allocator& getRuntimeAllocator() { return getContext().runtime_allocator; }
inline Allocator& getArrayAllocator() { return getContext().arr_allocator; }

struct AllocatorContextScope {
    explicit AllocatorContextScope(AllocatorContext& ctx) { g_context = &ctx; }
    ~AllocatorContextScope() { g_context = nullptr; }

    AllocatorContextScope(AllocatorContextScope const&) = delete;
    AllocatorContextScope& operator=(AllocatorContextScope const&) = delete;
};

} // namespace mylang

#endif // ALLOCATOR_HPP
