#pragma once

#include "../../diag/diagnostic.hpp"
#include "arena_block.hpp"
#include "meta.hpp"
#include "stats.hpp"

#include <functional>
#include <iomanip>
#include <iostream>
#include <shared_mutex>
#include <unordered_set>

namespace mylang {
namespace runtime {
namespace allocator {

class ArenaAllocator {
public:
    enum class GrowthStrategy : std::int32_t {
        LINEAR,
        EXPONENTIAL
    };

    using OutOfMemoryHandler = std::function<bool(std::size_t requested)>;

private:
    struct VoidPtrHash {
        std::size_t operator()(void const* ptr) const noexcept
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
    std::vector<ArenaBlock> Blocks_ {};
    mutable std::shared_mutex BlocksMutex_;
    GrowthStrategy GrowthFactor_ { GrowthStrategy::LINEAR };
    std::size_t BlockSize_ { DEFAULT_BLOCK_SIZE };
    std::size_t NextBlockSize_ { DEFAULT_BLOCK_SIZE };
    std::string Name_ { "arena" };
    OutOfMemoryHandler OomHandler_ { nullptr };
    mutable std::mutex OomHandlerMutex_;
    std::unordered_map<void*, AllocationHeader, VoidPtrHash, VoidPtrEqual> AllocationMap_ {};
    mutable std::shared_mutex AllocationMapMutex_;
    std::unordered_set<void*, VoidPtrHash, VoidPtrEqual> AllocatedPtrs_ {};
    mutable std::shared_mutex AllocatedPtrsMutex_;
    bool TrackAllocations_ { false };
    bool DebugFeatures_ { false };
    bool EnableStatistics_ { true };
    std::size_t MaxBlockSize_ { MAX_BLOCK_SIZE };
    void* LastPtr_ { nullptr };

    static constexpr std::size_t Alignment_ = alignof(std::max_align_t);

public:
    ArenaAllocator(std::int32_t growth_strategy = static_cast<std::int32_t>(GrowthStrategy::EXPONENTIAL), OutOfMemoryHandler oom_handler = nullptr,
        bool debug = true);

    ~ArenaAllocator() { reset(); }

    ArenaAllocator(ArenaAllocator const&) = delete;
    ArenaAllocator& operator=(ArenaAllocator const&) = delete;

    ArenaAllocator(ArenaAllocator&&) noexcept = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) noexcept = delete;

    void setName(std::string const& name)
    {
        Name_ = name;
    }

    void reset();

    std::size_t totalAllocated() const
    {
        return AllocStats_.TotalAllocated;
    }

    std::size_t totalAllocations() const
    {
        return AllocStats_.TotalAllocations;
    }

    std::size_t activeBlocks() const
    {
        return AllocStats_.ActiveBlocks;
    }

    [[nodiscard]]
    unsigned char* allocateBlock(std::size_t requested, std::size_t alignment_ = alignof(std::max_align_t), bool retry_on_oom = true);

    [[nodiscard]] void* allocate(std::size_t const size, std::size_t const alignment = alignof(std::max_align_t));

    void deallocate(void* ptr, std::size_t const size);

    [[nodiscard]]
    bool verifyAllocation(void* ptr) const;

    std::string toString(bool verbose) const
    {
        std::ostringstream oss;
        dumpStats(oss, verbose);
        return oss.str();
    }

    void dumpStats(std::ostream& os, bool verbose) const
    {
        StatsPrinter printer(AllocStats_, Name_);
        printer.printDetailed(os, verbose);
    }

private:
    [[nodiscard]]
    unsigned char* allocateFromBlocks(std::size_t alloc_size, std::size_t align = alignof(std::max_align_t));

    void updateNextBlockSize() noexcept;

    static constexpr std::size_t getAligned(std::size_t n, std::size_t const alignment = alignof(std::max_align_t)) noexcept
    {
        return (n + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace allocator
} // namespace runtime
} // namespace mylang
