/**
 * @file arena.h
 * @brief Thread-safe arena/region-based memory allocator with fast path optimization
 *
 * This allocator implements a sophisticated memory management system with:
 * - Lock-free allocation using CAS (Compare-And-Swap) operations
 * - Size-specific fast pools for small objects (8, 16, 32, 64, 128, 256 bytes)
 * - Free list management for memory reuse
 * - Double-free protection
 * - Optional allocation tracking and debugging
 * - Configurable growth strategies (linear/exponential)
 * - Thread-safe operations with fine-grained locking
 *
 * @author mylang runtime team
 * @version 2.0
 */

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

using byte = std::uint_fast8_t; /// Single byte type for Pointer arithmetic
using Pointer = byte*;          /// Generic Pointer type for arena memory

class MYLANG_COMPILER_ABI ArenaAllocator {
public:
    enum class GrowthStrategy : std::int32_t {
        LINEAR,     ///< Keep block size constant
        EXPONENTIAL ///< Double block size each time (default)
    };

    using OutOfMemoryHandler = std::function<bool(SizeType requested)>;

private:
    struct VoidPtrHash {
        SizeType operator()(void const* ptr) const MYLANG_NOEXCEPT { return std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(ptr)); }
    };

    struct VoidPtrEqual {
        bool operator()(void const* a, void const* b) const MYLANG_NOEXCEPT { return a == b; }
    };

    DetailedAllocStats AllocStats_;

    // General purpose blocks
    std::vector<ArenaBlock> Blocks_ {};     ///< Main memory blocks
    mutable std::shared_mutex BlocksMutex_; ///< Protects blocks vector
    // Fast pools for small allocations (8, 16, 32, 64, 128, 256 bytes)
    mutable std::mutex FastPoolMutex_; ///< Protects all fast pools
    // Configuration
    GrowthStrategy GrowthFactor_ { GrowthStrategy::LINEAR }; ///< Block growth strategy
    SizeType BlockSize_ { DEFAULT_BLOCK_SIZE };              ///< Initial block size
    SizeType NextBlockSize_ { DEFAULT_BLOCK_SIZE };          ///< Next block size to allocate
    std::string Name_ { "arena" };                           ///< Allocator name (for debugging)
    // Out-of-memory handling
    OutOfMemoryHandler OomHandler_ { nullptr }; ///< OOM callback
    mutable std::mutex OomHandlerMutex_;        ///< Protects OOM handler calls
    mutable std::mutex FreeListMutex_;          ///< Protects general free list
    // Debugging and tracking
    std::unordered_map<void*, AllocationHeader, VoidPtrHash, VoidPtrEqual> AllocationMap_ {}; ///< Allocation metadata
    mutable std::shared_mutex AllocationMapMutex_;                                            ///< Protects allocation map
    std::unordered_set<void*, VoidPtrHash, VoidPtrEqual> AllocatedPtrs_ {};                   ///< Active allocations (double-free protection)
    mutable std::shared_mutex AllocatedPtrsMutex_;                                            ///< Protects allocated pointers set
    // Feature flags
    bool TrackAllocations_ { false }; ///< Enable allocation tracking
    bool DebugFeatures_ { false };    ///< Enable debug features
    bool EnableStatistics_ { true };  ///< Enable statistics collection
    // Alignment settings
    static constexpr SizeType Alignment_ = alignof(std::max_align_t); ///< Minimum alignment
    SizeType MaxBlockSize_ { MAX_BLOCK_SIZE };                        ///< Maximum block size
    // for deallocation
    void* LastPtr_ { nullptr };

public:
    ArenaAllocator(std::int32_t growth_strategy = static_cast<std::int32_t>(GrowthStrategy::EXPONENTIAL), OutOfMemoryHandler oom_handler = nullptr,
        bool debug = true);

    ~ArenaAllocator() { reset(); }

    ArenaAllocator(ArenaAllocator const&) = delete;
    ArenaAllocator& operator=(ArenaAllocator const&) = delete;

    ArenaAllocator(ArenaAllocator&&) MYLANG_NOEXCEPT = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) MYLANG_NOEXCEPT = delete;

    void setName(std::string const& name) { Name_ = name; }

    //==========================================================================
    // Reset and Statistics
    //==========================================================================

    void reset();

    SizeType totalAllocated() const
    {
        return AllocStats_.TotalAllocated;
    }

    SizeType totalAllocations() const
    {
        return AllocStats_.TotalAllocations;
    }

    SizeType activeBlocks() const
    {
        return AllocStats_.ActiveBlocks;
    }

    /**
     * @brief Allocate a new memory block
     * @param requested Minimum size needed
     * @param alignment_ Required alignment
     * @param retry_on_oom Whether to call OOM handler on failure
     * @return Pointer to block start, or nullptr on failure
     *
     * Internal method used by public allocation functions.
     *
     * Thread-safe: Uses exclusive lock when adding to blocks vector.
     *
     * The actual block size may be larger than requested to:
     * - Satisfy alignment requirements
     * - Implement growth strategy
     * - Meet minimum block size
     */
    MYLANG_NODISCARD
    Pointer allocateBlock(SizeType requested, SizeType alignment_ = alignof(std::max_align_t), bool retry_on_oom = true);

    /**
     * @brief Allocate memory for objects of type T
     * @param size Number of bytes
     * @param alignment custom alignment, validated and overwritten if wrong
     * @return Pointer to allocated memory, or nullptr on failure
     * @throws panic if size larger than MAX
     *
     * Features:
     * - Automatic alignment
     * - Double-free protection (when debug enabled)
     * - Optional allocation tracking
     *
     * Thread-safe: Yes
     */
    MYLANG_NODISCARD MYLANG_COMPILER_ABI void* allocate(SizeType const size, SizeType const alignment = alignof(std::max_align_t));

    MYLANG_COMPILER_ABI void deallocate(void* ptr, SizeType const size);

    MYLANG_NODISCARD
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
    MYLANG_NODISCARD
    Pointer allocateFromBlocks(SizeType alloc_size, SizeType align = alignof(std::max_align_t));

    void updateNextBlockSize() MYLANG_NOEXCEPT;

    static constexpr SizeType getAligned(SizeType n, SizeType const alignment = alignof(std::max_align_t)) noexcept
    {
        return (n + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace allocator
} // namespace runtime
} // namespace mylang
