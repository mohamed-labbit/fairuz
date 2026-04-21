#ifndef ARENA_HPP
#define ARENA_HPP

#include "diagnostic.hpp"
#include "macros.hpp"

#ifdef FAIRUZ_DEBUG
#    include "stats.hpp"
#    include <unordered_map>
#    include <unordered_set>
#endif // FAIRUZ_DEBUG

#include <functional>
#include <optional>

namespace fairuz {

class Fa_ArenaBlock {
private:
    size_t m_size { DEFAULT_BLOCK_SIZE };
    unsigned char* m_begin { nullptr };
    unsigned char* m_next { nullptr };
    unsigned char* m_end { nullptr };

public:
    explicit Fa_ArenaBlock(size_t const m_size = DEFAULT_BLOCK_SIZE, size_t const m_alignment = alignof(std::max_align_t));

    ~Fa_ArenaBlock();

    // Non-copyable
    Fa_ArenaBlock(Fa_ArenaBlock const&) = delete;
    Fa_ArenaBlock& operator=(Fa_ArenaBlock const&) = delete;

    Fa_ArenaBlock(Fa_ArenaBlock&& other) noexcept;

    Fa_ArenaBlock& operator=(Fa_ArenaBlock&& other) noexcept;

    [[nodiscard]] unsigned char* begin() const;
    [[nodiscard]] unsigned char* end() const;
    [[nodiscard]] unsigned char* next() const;

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t used() const;
    [[nodiscard]] size_t remaining() const;

    bool pop(size_t bytes);

    [[nodiscard]] unsigned char* allocate(size_t bytes, std::optional<size_t> m_alignment = std::nullopt);
    unsigned char* reserve(size_t const bytes);
}; // class Fa_ArenaBlock

class Fa_ArenaAllocator {
public:
    enum class GrowthStrategy : i32 {
        LINEAR
    }; // enum GrowthStrategy

    using OutOfMemoryHandler = std::function<bool(size_t requested)>;

private:
    std::vector<Fa_ArenaBlock> m_blocks { };
    GrowthStrategy m_growth_factor { GrowthStrategy::LINEAR };
    size_t m_block_size { DEFAULT_BLOCK_SIZE };
    size_t m_next_block_size { DEFAULT_BLOCK_SIZE };
    std::string m_name { "arena" };
    OutOfMemoryHandler m_oom_handler { nullptr };
    size_t m_max_block_size { MAX_BLOCK_SIZE };
    void* m_last_ptr { nullptr };
    size_t m_last_size { 0 };
    size_t m_last_consumed { 0 };
    unsigned char* m_next { nullptr };
    unsigned char* m_end { nullptr };
    void* allocate_slow(size_t m_size, size_t m_alignment);
#ifdef FAIRUZ_DEBUG
    struct AllocationHeader {
        void* ptr { nullptr };
        u64 size { 0 };
        u64 consumed { 0 };
        u64 alignment { 0 };
    };

    void track_allocation(unsigned char* ptr, size_t m_size, size_t consumed, size_t m_alignment);
#endif

#ifdef FAIRUZ_DEBUG
    struct VoidPtrHash {
        size_t operator()(void const* ptr) const noexcept { return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(ptr)); }
    }; // struct VoidPtrHash

    struct VoidPtrEqual {
        bool operator()(void const* a, void const* b) const noexcept { return a == b; }
    }; // struct VoidPtrEqual

    DetailedAllocStats m_alloc_stats;
    std::unordered_map<void*, AllocationHeader, VoidPtrHash, VoidPtrEqual> m_allocation_map { };
    std::unordered_set<void*, VoidPtrHash, VoidPtrEqual> m_allocated_ptrs { };
    bool m_track_allocations { false };
    bool m_debug_features { false };
    bool m_enable_statistics { true };
#endif // FAIRUZ_DEBUG

    static constexpr size_t m_alignment = alignof(std::max_align_t);

public:
    explicit Fa_ArenaAllocator(GrowthStrategy growth_strategy = GrowthStrategy::LINEAR,
        OutOfMemoryHandler m_oom_handler = nullptr, bool debug = true);

    ~Fa_ArenaAllocator() { reset(); }

    Fa_ArenaAllocator(Fa_ArenaAllocator const&) = delete;
    Fa_ArenaAllocator& operator=(Fa_ArenaAllocator const&) = delete;

    Fa_ArenaAllocator(Fa_ArenaAllocator&&) noexcept = delete;
    Fa_ArenaAllocator& operator=(Fa_ArenaAllocator&&) noexcept = delete;

    void set_name(std::string const& m_name);
    void reset();

    unsigned char* allocate_block(size_t requested, size_t m_alignment = alignof(std::max_align_t), bool retry_on_oom = true);

    [[nodiscard]] void* allocate(size_t const m_size, size_t const m_alignment = alignof(std::max_align_t));

    void deallocate(void* ptr, size_t const m_size);

    template<typename T>
    [[nodiscard]] T* allocate_array(size_t const count)
    {
        return static_cast<T*>(allocate(count * sizeof(T)));
    }

    template<typename T>
    void deallocate_array(T* ptr, size_t const count) { deallocate(static_cast<void*>(ptr), count * sizeof(T)); }

    template<typename T, typename... Args>
    [[nodiscard]] T* allocate_object(Args&&... m_args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible with Args...");
        return ::new (allocate(sizeof(T))) T(std::forward<Args>(m_args)...);
    }

    template<typename T>
    void deallocate_object(T* obj) { deallocate(static_cast<void*>(obj), sizeof(T)); }

#ifdef FAIRUZ_DEBUG
    size_t total_allocated() const;
    size_t total_allocations() const;
    size_t active_blocks() const;
    DetailedAllocStats const& stats() const;
    std::string to_string(bool verbose) const;
    void dump_stats(std::ostream& os, bool verbose) const;
    [[nodiscard]] bool verify_allocation(void* ptr) const;
#endif // FAIRUZ_DEBUG

private:
    [[nodiscard]] unsigned char* allocate_from_blocks(size_t alloc_size, size_t align = alignof(std::max_align_t));

    [[nodiscard]] static constexpr size_t get_aligned(size_t n, size_t const m_alignment = alignof(std::max_align_t)) noexcept
    {
        return (n + m_alignment - 1) & ~(m_alignment - 1);
    }
}; // class Fa_ArenaAllocator

struct Fa_AllocatorContext {
    Fa_ArenaAllocator allocator { Fa_ArenaAllocator::GrowthStrategy::LINEAR, nullptr, false };
}; // struct Fa_AllocatorContext

inline Fa_AllocatorContext* g_context = nullptr;

inline void set_context(Fa_AllocatorContext* ctx) { g_context = ctx; }

inline Fa_AllocatorContext& get_context()
{
    if (UNLIKELY(!g_context)) {
        static Fa_AllocatorContext default_ctx;
        g_context = &default_ctx;
    }
    return *g_context;
}

inline Fa_ArenaAllocator& get_allocator() { return get_context().allocator; }

struct Fa_AllocatorContextScope {
    explicit Fa_AllocatorContextScope(Fa_AllocatorContext& ctx) { g_context = &ctx; }
    ~Fa_AllocatorContextScope() { g_context = nullptr; }

    Fa_AllocatorContextScope(Fa_AllocatorContextScope const&) = delete;
    Fa_AllocatorContextScope& operator=(Fa_AllocatorContextScope const&) = delete;
}; // struct Fa_AllocatorContextScope

} // namespace fairuz

#endif // ARENA_HPP
