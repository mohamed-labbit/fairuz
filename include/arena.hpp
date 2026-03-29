#ifndef ARENA_HPP
#define ARENA_HPP

#include "diagnostic.hpp"
#include "macros.hpp"

#include <functional>
#include <optional>

namespace fairuz {

class Fa_ArenaBlock {
private:
    size_t Size_ { DEFAULT_BLOCK_SIZE };
    unsigned char* Begin_ { nullptr };
    unsigned char* Next_ { nullptr };
    unsigned char* End_ { nullptr };

public:
    explicit Fa_ArenaBlock(size_t const size = DEFAULT_BLOCK_SIZE, size_t const alignment = alignof(std::max_align_t));

    ~Fa_ArenaBlock();

    // Non-copyable
    Fa_ArenaBlock(Fa_ArenaBlock const&) = delete;
    Fa_ArenaBlock& operator=(Fa_ArenaBlock const&) = delete;

    Fa_ArenaBlock(Fa_ArenaBlock&& other) noexcept;

    Fa_ArenaBlock& operator=(Fa_ArenaBlock&& other) noexcept;

    [[nodiscard]] unsigned char* begin() const;
    [[nodiscard]] unsigned char* end() const;
    [[nodiscard]] unsigned char* cNext() const;

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t used() const;
    [[nodiscard]] size_t remaining() const;

    bool pop(size_t bytes);

    [[nodiscard]] unsigned char* allocate(size_t bytes, std::optional<size_t> alignment = std::nullopt);
    unsigned char* reserve(size_t const bytes);
}; // class Fa_ArenaBlock

class Fa_ArenaAllocator {
public:
    enum class GrowthStrategy : i32 {
        LINEAR,
        EXPONENTIAL
    }; // enum GrowthStrategy

    using OutOfMemoryHandler = std::function<bool(size_t requested)>;

private:
    std::vector<Fa_ArenaBlock> Blocks_ { };
    GrowthStrategy GrowthFactor_ { GrowthStrategy::LINEAR };
    size_t BlockSize_ { DEFAULT_BLOCK_SIZE };
    size_t NextBlockSize_ { DEFAULT_BLOCK_SIZE };
    std::string Name_ { "arena" };
    OutOfMemoryHandler OomHandler_ { nullptr };
    size_t MaxBlockSize_ { MAX_BLOCK_SIZE };
    void* LastPtr_ { nullptr };
    unsigned char* Next_ { nullptr };
    unsigned char* End_ { nullptr };
    void* allocateSlow(size_t size, size_t alignment);
#ifdef fairuz_DEBUG
    void trackAllocation(unsigned char* ptr, size_t size, size_t alignment);
#endif

#ifdef fairuz_DEBUG
    struct VoidPtrHash {
        size_t operator()(void const* ptr) const noexcept { return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(ptr)); }
    }; // struct VoidPtrHash

    struct VoidPtrEqual {
        bool operator()(void const* a, void const* b) const noexcept { return a == b; }
    }; // struct VoidPtrEqual

    DetailedAllocStats AllocStats_;
    std::unordered_mape<void*, AllocationHeader, VoidPtrHash, VoidPtrEqual> AllocationMap_ { };
    std::unordered_set<void*, VoidPtrHash, VoidPtrEqual> AllocatedPtrs_ { };
    bool TrackAllocations_ { false };
    bool DebugFeatures_ { false };
    bool EnableStatistics_ { true };
#endif // fairuz_DEBUG

    static constexpr size_t Alignment_ = alignof(std::max_align_t);

public:
    explicit Fa_ArenaAllocator(i32 growth_strategy = static_cast<i32>(GrowthStrategy::EXPONENTIAL),
        OutOfMemoryHandler oom_handler = nullptr, bool debug = true);

    ~Fa_ArenaAllocator() { reset(); }

    Fa_ArenaAllocator(Fa_ArenaAllocator const&) = delete;
    Fa_ArenaAllocator& operator=(Fa_ArenaAllocator const&) = delete;

    Fa_ArenaAllocator(Fa_ArenaAllocator&&) noexcept = delete;
    Fa_ArenaAllocator& operator=(Fa_ArenaAllocator&&) noexcept = delete;

    void setName(std::string const& name);
    void reset();

    [[nodiscard]] unsigned char* allocateBlock(size_t requested, size_t alignment_ = alignof(std::max_align_t), bool retry_on_oom = true);

    [[nodiscard]] void* allocate(size_t const size, size_t const alignment = alignof(std::max_align_t));

    void deallocate(void* ptr, size_t const size);

    template<typename T>
    [[nodiscard]] T* allocateArray(size_t const count)
    {
        return static_cast<T*>(allocate(count * sizeof(T)));
    }

    template<typename T>
    void deallocateArray(T* ptr, size_t const count) { deallocate(static_cast<void*>(ptr), count * sizeof(T)); }

    template<typename T, typename... Args>
    [[nodiscard]] T* allocateObject(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible with Args...");
        return ::new (allocate(sizeof(T))) T(std::forward<Args>(args)...);
    }

    template<typename T>
    void deallocateObject(T* obj) { deallocate(static_cast<void*>(obj), sizeof(T)); }

#ifdef fairuz_DEBUG
    size_t totalAllocated() const;
    size_t totalAllocations() const;
    size_t activeBlocks() const;
    std::string toString(bool verbose) const;
    void dumpStats(std::ostream& os, bool verbose) const;
    [[nodiscard]] bool verifyAllocation(void* ptr) const;
#endif // fairuz_DEBUG

private:
    [[nodiscard]] unsigned char* allocateFromBlocks(size_t alloc_size, size_t align = alignof(std::max_align_t));

    void updateNextBlockSize() noexcept;

    [[nodiscard]] static constexpr size_t getAligned(size_t n, size_t const alignment = alignof(std::max_align_t)) noexcept
    {
        return (n + alignment - 1) & ~(alignment - 1);
    }
}; // class Fa_ArenaAllocator

struct Fa_AllocatorContext {
    Fa_ArenaAllocator allocator;
}; // struct Fa_AllocatorContext

inline Fa_AllocatorContext* g_context = nullptr;

inline void setContext(Fa_AllocatorContext* ctx) { g_context = ctx; }

inline Fa_AllocatorContext& getContext()
{
    if (UNLIKELY(!g_context)) {
        static Fa_AllocatorContext default_ctx;
        g_context = &default_ctx;
    }
    return *g_context;
}

inline Fa_ArenaAllocator& getAllocator() { return getContext().allocator; }

struct Fa_AllocatorContextScope {
    explicit Fa_AllocatorContextScope(Fa_AllocatorContext& ctx) { g_context = &ctx; }
    ~Fa_AllocatorContextScope() { g_context = nullptr; }

    Fa_AllocatorContextScope(Fa_AllocatorContextScope const&) = delete;
    Fa_AllocatorContextScope& operator=(Fa_AllocatorContextScope const&) = delete;
}; // struct Fa_AllocatorContextScope

} // namespace fairuz

#endif // ARENA_HPP
