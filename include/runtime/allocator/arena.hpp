#ifndef ARENA_HPP
#define ARENA_HPP

#include "../../diagnostic.hpp"
#include "meta.hpp"
#include "stats.hpp"

#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>

namespace mylang::runtime::allocator {

class ArenaBlock {
private:
    size_t Size_ { DEFAULT_BLOCK_SIZE };
    unsigned char* Begin_ { nullptr };
    unsigned char* Next_ { nullptr };

public:
    explicit ArenaBlock(size_t const size = DEFAULT_BLOCK_SIZE, size_t const alignment = alignof(std::max_align_t));

    ~ArenaBlock();

    // Non-copyable
    ArenaBlock(ArenaBlock const&) = delete;
    ArenaBlock& operator=(ArenaBlock const&) = delete;

    ArenaBlock(ArenaBlock&& other) noexcept;

    ArenaBlock& operator=(ArenaBlock&& other) noexcept;

    unsigned char* begin() const;
    unsigned char* end() const;
    unsigned char* cNext() const;

    size_t size() const;
    size_t used() const;

    bool pop(size_t bytes);

    size_t remaining() const;

    unsigned char* allocate(size_t bytes, std::optional<size_t> alignment = std::nullopt);
    unsigned char* reserve(size_t const bytes);
}; // ArenaBlock

class ArenaAllocator {
public:
    enum class GrowthStrategy : std::int32_t {
        LINEAR,
        EXPONENTIAL
    };

    using OutOfMemoryHandler = std::function<bool(size_t requested)>;

private:
    std::vector<ArenaBlock> Blocks_ { };
    GrowthStrategy GrowthFactor_ { GrowthStrategy::LINEAR };
    size_t BlockSize_ { DEFAULT_BLOCK_SIZE };
    size_t NextBlockSize_ { DEFAULT_BLOCK_SIZE };
    std::string Name_ { "arena" };
    OutOfMemoryHandler OomHandler_ { nullptr };
    size_t MaxBlockSize_ { MAX_BLOCK_SIZE };
    void* LastPtr_ { nullptr };

#ifdef MYLANG_DEBUG
    struct VoidPtrHash {
        size_t operator()(void const* ptr) const noexcept
        {
            return std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(ptr));
        }
    };

    struct VoidPtrEqual {
        bool operator()(void const* a, void const* b) const noexcept
        {
            return a == b;
        }
    };

    DetailedAllocStats AllocStats_;
    std::unordered_map<void*, AllocationHeader, VoidPtrHash, VoidPtrEqual> AllocationMap_ { };
    std::unordered_set<void*, VoidPtrHash, VoidPtrEqual> AllocatedPtrs_ { };
    bool TrackAllocations_ { false };
    bool DebugFeatures_ { false };
    bool EnableStatistics_ { true };
#endif // MYLANG_DEBUG

    static constexpr size_t Alignment_ = alignof(std::max_align_t);

public:
    explicit ArenaAllocator(std::int32_t growth_strategy = static_cast<std::int32_t>(GrowthStrategy::EXPONENTIAL), OutOfMemoryHandler oom_handler = nullptr,
        bool debug = true);

    ~ArenaAllocator()
    {
        reset();
    }

    ArenaAllocator(ArenaAllocator const&) = delete;
    ArenaAllocator& operator=(ArenaAllocator const&) = delete;

    ArenaAllocator(ArenaAllocator&&) noexcept = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) noexcept = delete;

    void setName(std::string const& name);
    void reset();

    MY_NODISCARD unsigned char* allocateBlock(size_t requested, size_t alignment_ = alignof(std::max_align_t), bool retry_on_oom = true);

    MY_NODISCARD void* allocate(size_t const size, size_t const alignment = alignof(std::max_align_t));

    void deallocate(void* ptr, size_t const size);

#ifdef MYLANG_DEBUG
    size_t totalAllocated() const;
    size_t totalAllocations() const;
    size_t activeBlocks() const;
    std::string toString(bool verbose) const;
    void dumpStats(std::ostream& os, bool verbose) const;
    MY_NODISCARD bool verifyAllocation(void* ptr) const;
#endif // MYLANG_DEBUG

private:
    MY_NODISCARD unsigned char* allocateFromBlocks(size_t alloc_size, size_t align = alignof(std::max_align_t));

    void updateNextBlockSize() noexcept;

    static constexpr size_t getAligned(size_t n, size_t const alignment = alignof(std::max_align_t)) noexcept
    {
        return (n + alignment - 1) & ~(alignment - 1);
    }
}; // class ArenaAllocator

} // namespace mylang::runtime::allocator

#endif // ARENA_HPP
