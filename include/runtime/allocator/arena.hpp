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

#include <functional>
#include <iomanip>
#include <iostream>
#include <shared_mutex>
#include <unordered_set>


namespace mylang {
namespace runtime {
namespace allocator {

using byte    = std::uint_fast8_t;  /// Single byte type for Pointer arithmetic
using Pointer = byte*;              /// Generic Pointer type for arena memory

/// Default block size for new allocations (64KB)
#ifndef DEFAULT_BLOCK_SIZE
  #define DEFAULT_BLOCK_SIZE (64 * 1024)
#endif

/// Maximum allowed block size to prevent excessive memory usage (16MB)
#ifndef MAX_BLOCK_SIZE
  #define MAX_BLOCK_SIZE (16 * 1024 * 1024)
#endif

/**
 * @class ArenaAllocator
 * @brief High-performance thread-safe memory allocator with multiple optimization strategies
 * 
 * Architecture:
 * - General purpose blocks for large/irregular allocations
 * - Fast pools for small fixed-size allocations (8-256 bytes)
 * - Free lists for memory reuse
 * - Double-free detection
 * - Optional allocation tracking
 * 
 * Thread-Safety
 * * Thread-Safety:
 * - Lock-free allocation within individual blocks (CAS operations)
 * - Fine-grained locking for structural operations (adding blocks, free lists)
 * - Shared/exclusive locking for read/write operations on maps
 * - Double-free protection through tracked Pointer set
 * 
 * Performance Characteristics:
 * - Small allocations (≤256 bytes): O(1) amortized via fast pools
 * - Large allocations: O(1) for bump allocation, O(log n) for free list
 * - Deallocation: O(log n) for free list insertion
 * - Thread contention: Minimal due to lock-free fast paths
 * 
 * Memory Overhead:
 * - Per-block: ~48 bytes (size, pointers, mutex)
 * - Per-allocation (debug mode): ~64 bytes (header)
 * - Free list entries: ~16 bytes each
 * 
 * @example
 * @code
 * ArenaAllocator arena;
 * 
 * // Allocate an integer
 * int* num = arena.allocate<int>();
 * *num = 42;
 * 
 * // Allocate an array
 * double* arr = arena.allocate<double>(100);
 * 
 * // Deallocate (returns to free list)
 * arena.deallocate(num);
 * arena.deallocate(arr, 100);
 * 
 * // Reset all at once (fastest)
 * arena.reset();
 * @endcode
 */
class MYLANG_COMPILER_ABI ArenaAllocator
{
 public:
  /**
     * @enum GrowthStrategy
     * @brief Strategy for expanding block size as allocations grow
     */
  enum class GrowthStrategy : std::int32_t {
    LINEAR,      ///< Keep block size constant
    EXPONENTIAL  ///< Double block size each time (default)
  };

  /**
     * @typedef OutOfMemoryHandler
     * @brief Callback for out-of-memory situations
     * @param requested Size that couldn't be allocated
     * @return true to retry allocation, false to fail
     * 
     * Allows custom recovery strategies like:
     * - Forcing garbage collection
     * - Freeing caches
     * - Growing max block size
     * - Logging and failing gracefully
     */
  using OutOfMemoryHandler = std::function<bool(SizeType requested)>;

 private:
  //==========================================================================
  // Private Types
  //==========================================================================

  /**
     * @struct VoidPtrHash
     * @brief Hash function for void pointers
     */
  struct VoidPtrHash
  {
    SizeType operator()(const void* ptr) const MYLANG_NOEXCEPT { return std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(ptr)); }
  };

  /**
     * @struct VoidPtrEqual
     * @brief Equality comparison for void pointers
     */
  struct VoidPtrEqual
  {
    bool operator()(const void* a, const void* b) const MYLANG_NOEXCEPT { return a == b; }
  };

  // Statistics
  ArenaAllocStats AllocStats_;  ///< Allocation statistics
  // General purpose blocks
  std::vector<ArenaBlock>   Blocks_{};     ///< Main memory blocks
  mutable std::shared_mutex BlocksMutex_;  ///< Protects blocks vector
  // Fast pools for small allocations (8, 16, 32, 64, 128, 256 bytes)
  mutable std::mutex FastPoolMutex_;  ///< Protects all fast pools
  // Configuration
  std::atomic<GrowthStrategy> GrowthFactor_{GrowthStrategy::LINEAR};  ///< Block growth strategy
  SizeType                    BlockSize_{DEFAULT_BLOCK_SIZE};         ///< Initial block size
  std::atomic<SizeType>       NextBlockSize_{DEFAULT_BLOCK_SIZE};     ///< Next block size to allocate
  std::string                 Name_{"arena"};                         ///< Allocator name (for debugging)
  // Out-of-memory handling
  OutOfMemoryHandler OomHandler_{nullptr};  ///< OOM callback
  mutable std::mutex OomHandlerMutex_;      ///< Protects OOM handler calls
  mutable std::mutex FreeListMutex_;        ///< Protects general free list
  // Debugging and tracking
  std::unordered_map<void*, AllocationHeader, VoidPtrHash, VoidPtrEqual> AllocationMap_{};     ///< Allocation metadata
  mutable std::shared_mutex                                              AllocationMapMutex_;  ///< Protects allocation map
  std::unordered_set<void*, VoidPtrHash, VoidPtrEqual>                   AllocatedPtrs_{};     ///< Active allocations (double-free protection)
  mutable std::shared_mutex                                              AllocatedPtrsMutex_;  ///< Protects allocated pointers set
  // Feature flags
  std::atomic<bool> TrackAllocations_{false};  ///< Enable allocation tracking
  std::atomic<bool> DebugFeatures_{false};     ///< Enable debug features
  std::atomic<bool> EnableStatistics_{true};   ///< Enable statistics collection
  // Alignment settings
  static constexpr SizeType Alignment_ = alignof(std::max_align_t);  ///< Minimum alignment
  std::atomic<SizeType>     MaxBlockSize_{MAX_BLOCK_SIZE};           ///< Maximum block size
  // for deallocation
  std::atomic<void*> LastPtr_{nullptr};

 public:
  //==========================================================================
  // Construction and Destruction
  //==========================================================================

  /**
     * @brief Construct an arena allocator
     * @param growth_strategy Block growth strategy (LINEAR or EXPONENTIAL)
     * @param min_align Minimum alignment for all allocations (must be power of 2)
     * @param oom_handler Optional callback for out-of-memory situations
     * @param debug Enable debug features (tracking, validation)
     * 
     * If min_align is invalid (not a power of 2), it defaults to alignof(max_align_t).
     * Debug mode enables allocation tracking and double-free detection.
     */
  ArenaAllocator(std::int32_t       growth_strategy = static_cast<std::int32_t>(GrowthStrategy::EXPONENTIAL),
                 OutOfMemoryHandler oom_handler     = nullptr,
                 bool               debug           = true);
  /**
     * @brief Destructor - frees all memory
     * 
     * Automatically calls reset() to release all resources.
     */
  ~ArenaAllocator() { reset(); }

  // Non-copyable and non-movable (contains mutexes)
  ArenaAllocator(const ArenaAllocator&)            = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;

  ArenaAllocator(ArenaAllocator&&) MYLANG_NOEXCEPT            = delete;
  ArenaAllocator& operator=(ArenaAllocator&&) MYLANG_NOEXCEPT = delete;

  //==========================================================================
  // Reset and Statistics
  //==========================================================================

  /**
     * @brief Reset the allocator, freeing all memory
     * 
     * Thread-safe: Acquires all locks in a consistent order to prevent deadlocks.
     * 
     * After reset:
     * - All memory is freed
     * - All pointers become invalid
     * - Statistics are zeroed
     * - Ready for new allocations
     * 
     * @warning All previously allocated pointers become invalid!
     */
  void reset();

  /**
     * @brief Get total bytes allocated (cumulative)
     * @return Total bytes allocated since construction or last reset
     */
  SizeType totalAllocated() const { return AllocStats_.TotalAllocated.load(std::memory_order_relaxed); }

  /**
     * @brief Get total number of allocations
     * @return Number of allocation requests processed
     */
  SizeType totalAllocations() const { return AllocStats_.TotalAllocations.load(std::memory_order_relaxed); }

  /**
     * @brief Get number of active memory blocks
     * @return Count of currently allocated blocks
     */
  SizeType activeBlocks() const { return AllocStats_.ActiveBlocks.load(std::memory_order_relaxed); }

  //==========================================================================
  // Block Allocation (Internal)
  //==========================================================================

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
  MYLANG_NODISCARD MYLANG_COMPILER_ABI void* allocate(const SizeType size, const SizeType alignment = alignof(std::max_align_t));

  /**
     * @brief Deallocate memory (returns to free list)
     * @tparam _Tp Type that was allocated
     * @param ptr Pointer to deallocate
     * @param count Number of objects (must match allocation count)
     * 
     * Features:
     * - Double-free detection
     * - Automatic destructor calls for non-trivial types
     * - Returns memory to appropriate free list for reuse
     * - Updates statistics
     * 
     * Thread-safe: Yes
     * 
     * @warning Arena allocators typically don't support individual deallocation.
     *          This implementation supports it via free lists, but reset() is faster
     *          for bulk deallocation.
     * 
     * @note Does NOT actually free memory to the OS - memory is retained
     *       in free lists for reuse.
     */
  MYLANG_COMPILER_ABI void deallocate(void* ptr, const SizeType size);

  /**
     * @brief Verify allocation integrity (debug feature)
     * @param ptr Pointer to verify
     * @return true if allocation is valid and uncorrupted
     * 
     * Only works if tracking is enabled (debug mode).
     * Checks magic numbers and checksums in allocation headers.
     * 
     * Thread-safe: Yes (read-only operation)
     */
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
    using std::setw;
    using std::left;
    using std::right;

    constexpr int LABEL_W = 28;
    constexpr int VALUE_W = 14;

    auto bytes = [](SizeType b) -> std::string {
      constexpr const char* suffix[] = {"B", "KB", "MB", "GB"};
      double                value    = static_cast<double>(b);
      int                   idx      = 0;
      while (value >= 1024.0 && idx < 3)
      {
        value /= 1024.0;
        ++idx;
      }
      std::ostringstream ss;
      ss << std::fixed << std::setprecision(2) << value << suffix[idx];
      return ss.str();
    };

    os << "\n";
    os << "═══════════════════════════════════════════════════════\n";
    os << " Arena Allocator Statistics [" << Name_ << "]\n";
    os << "═══════════════════════════════════════════════════════\n";

    // Core statistics
    os << Color::BOLD << Color::CYAN << left << setw(LABEL_W) << "Total allocations" << Color::RESET << right << setw(VALUE_W)
       << AllocStats_.TotalAllocations.load() << "\n";
    os << Color::BOLD << Color::CYAN << left << setw(LABEL_W) << "Total bytes allocated" << Color::RESET << right << setw(VALUE_W)
       << bytes(AllocStats_.TotalAllocated.load()) << "\n";
    os << Color::BOLD << Color::CYAN << left << setw(LABEL_W) << "Active blocks" << Color::RESET << right << setw(VALUE_W)
       << AllocStats_.ActiveBlocks.load() << "\n";
    os << Color::BOLD << Color::CYAN << left << setw(LABEL_W) << "Current block size" << Color::RESET << right << setw(VALUE_W) << bytes(BlockSize_)
       << "\n";
    os << Color::BOLD << Color::CYAN << left << setw(LABEL_W) << "Next block size" << Color::RESET << right << setw(VALUE_W)
       << bytes(NextBlockSize_.load()) << "\n";
    os << Color::BOLD << Color::CYAN << left << setw(LABEL_W) << "Max block size" << Color::RESET << right << setw(VALUE_W)
       << bytes(MaxBlockSize_.load()) << "\n";
    os << Color::BOLD << Color::CYAN << left << setw(LABEL_W) << "Growth strategy" << Color::RESET << right << setw(VALUE_W)
       << (GrowthFactor_.load() == GrowthStrategy::EXPONENTIAL ? "Exponential" : "Linear") << "\n";

    // Feature flags
    os << "\n";
    os << " Features\n";
    os << " ───────────────────────────────────────────────────\n";
    os << Color::BOLD << Color::GREEN << left << setw(LABEL_W) << "Statistics enabled" << Color::RESET << right << setw(VALUE_W)
       << (EnableStatistics_.load() ? "yes" : "no") << "\n";
    os << Color::BOLD << Color::GREEN << left << setw(LABEL_W) << "Allocation tracking" << Color::RESET << right << setw(VALUE_W)
       << (TrackAllocations_.load() ? "yes" : "no") << "\n";
    os << Color::BOLD << Color::GREEN << left << setw(LABEL_W) << "Debug features" << Color::RESET << right << setw(VALUE_W)
       << (DebugFeatures_.load() ? "yes" : "no") << "\n";

    // Allocation tracking
    if (verbose && TrackAllocations_.load())
    {
      std::shared_lock mapLock(AllocationMapMutex_);
      std::shared_lock ptrLock(AllocatedPtrsMutex_);
      os << "\n";
      os << " Allocation Tracking\n";
      os << " ───────────────────────────────────────────────────\n";
      os << Color::BOLD << Color::MAGENTA << left << setw(LABEL_W) << "Live allocations" << Color::RESET << right << setw(VALUE_W)
         << AllocationMap_.size() << "\n";
      os << Color::BOLD << Color::MAGENTA << left << setw(LABEL_W) << "Tracked pointers" << Color::RESET << right << setw(VALUE_W)
         << AllocatedPtrs_.size() << "\n";
    }

    // Block details
    if (verbose)
    {
      std::shared_lock blocksLock(BlocksMutex_);
      SizeType         totalCapacity = 0;
      SizeType         totalUsed     = 0;
      for (const auto& block : Blocks_)
      {
        totalCapacity += block.size();
        totalUsed += block.used();
      }
      os << "\n";
      os << " Memory Blocks\n";
      os << " ───────────────────────────────────────────────────\n";
      os << Color::BOLD << Color::YELLOW << left << setw(LABEL_W) << "Block count" << Color::RESET << right << setw(VALUE_W) << Blocks_.size()
         << "\n";
      os << Color::BOLD << Color::YELLOW << left << setw(LABEL_W) << "Total capacity" << Color::RESET << right << setw(VALUE_W)
         << bytes(totalCapacity) << "\n";
      os << Color::BOLD << Color::YELLOW << left << setw(LABEL_W) << "Total used" << Color::RESET << right << setw(VALUE_W) << bytes(totalUsed)
         << "\n";
      if (totalCapacity > 0)
      {
        double usage = (100.0 * totalUsed) / totalCapacity;
        os << Color::BOLD << Color::YELLOW << left << setw(LABEL_W) << "Utilization" << Color::RESET << right << setw(VALUE_W) << std::fixed
           << std::setprecision(2) << usage << "%" << "\n";
      }
    }
    os << "═══════════════════════════════════════════════════════\n";
  }

 private:
  MYLANG_NODISCARD
  Pointer allocateFromBlocks(SizeType alloc_size, SizeType align = alignof(std::max_align_t));

  void updateNextBlockSize() MYLANG_NOEXCEPT;

  static constexpr SizeType getAligned(SizeType n, const SizeType alignment = alignof(std::max_align_t)) noexcept
  {
    return (n + alignment - 1) & ~(alignment - 1);
  }
};

}  // namespace allocator
}  // namespace runtime
}  // namespace mylang