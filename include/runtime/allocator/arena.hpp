#pragma once

/**
 * @file arena_allocator.h
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

#include "../../macros.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace mylang {
namespace runtime {
namespace allocator {

//==============================================================================
// Type Aliases
//==============================================================================

/// Single byte type for Pointer arithmetic
using byte = std::uint_fast8_t;

/// Generic Pointer type for arena memory
using Pointer = byte*;

//==============================================================================
// Configuration Constants
//==============================================================================

/// Default block size for new allocations (64KB)
#ifndef DEFAULT_BLOCK_SIZE
    #define DEFAULT_BLOCK_SIZE (64 * 1024)
#endif

/// Maximum allowed block size to prevent excessive memory usage (16MB)
#ifndef MAX_BLOCK_SIZE
    #define MAX_BLOCK_SIZE (16 * 1024 * 1024)
#endif

//==============================================================================
// Statistics Tracking
//==============================================================================

/**
 * @struct ArenaAllocStats
 * @brief Thread-safe statistics for allocation tracking
 * 
 * All counters are atomic to allow lock-free updates from multiple threads.
 * Uses relaxed memory ordering for performance.
 */
struct ArenaAllocStats
{
    /// Total number of allocation requests
    std::atomic<std::size_t> total_allocations{0};
    /// Total bytes allocated (cumulative)
    std::atomic<std::size_t> total_allocated{0};
    /// Total bytes freed (for future use)
    std::atomic<std::size_t> total_free{0};
    /// Total number of deallocation requests
    std::atomic<std::size_t> total_deallocations{0};
    /// Number of currently active memory blocks
    std::atomic<std::size_t> active_blocks{0};

    ArenaAllocStats() = default;

    /**
     * @brief Safely increment an atomic counter with overflow protection
     * @param counter The atomic counter to increment
     * @param amount The amount to add (default: 1)
     * 
     * Implements saturation arithmetic - if overflow would occur, the counter
     * remains at its maximum value instead of wrapping around.
     */
    void safe_increment(std::atomic<std::size_t>& counter, std::size_t amount = 1)
    {
        std::size_t old_val = counter.load(std::memory_order_relaxed);
        std::size_t new_val;
        do
        {
            // Check for overflow before incrementing
            if (old_val > std::numeric_limits<std::size_t>::max() - amount)
            {
                return;  // Would overflow, saturate at current value
            }
            new_val = old_val + amount;
        } while (
          !counter.compare_exchange_weak(old_val, new_val, std::memory_order_relaxed, std::memory_order_relaxed));
    }
};

//==============================================================================
// Arena Block - Basic Memory Block with Lock-Free Allocation
//==============================================================================

/**
 * @class ArenaBlock
 * @brief A contiguous block of memory with thread-safe bump-Pointer allocation
 * 
 * Features:
 * - Lock-free allocation using atomic CAS operations
 * - Automatic alignment handling
 * - Move semantics for efficient container storage
 * - Thread-safe through atomic operations and minimal locking
 * 
 * @note Individual blocks are NOT thread-safe for concurrent allocations
 *       between different blocks. The ArenaAllocator handles this.
 */
class ArenaBlock
{
   private:
    /// Size of this memory block in bytes
    std::size_t size_{DEFAULT_BLOCK_SIZE};

    /// Pointer to the beginning of allocated memory
    Pointer begin_{nullptr};

    /// Atomic Pointer to the next available byte (bump Pointer)
    std::atomic<Pointer> next_{nullptr};

    /// Mutex for protecting structural operations (not allocation)
    mutable std::mutex mutex_;

   public:
    /**
     * @brief Construct an arena block with specified size and alignment
     * @param size Size of the block in bytes
     * @param alignment Required alignment (must be power of 2)
     * @throws std::invalid_argument if alignment is not a power of 2
     * @throws std::bad_alloc if memory allocation fails
     * 
     * The actual allocated size may be larger than requested to satisfy
     * alignment requirements.
     */
    explicit ArenaBlock(
      const std::size_t size = DEFAULT_BLOCK_SIZE, const std::size_t alignment = alignof(std::max_align_t)) :
        size_(size)
    {
        // Validate alignment is a power of 2
        if (alignment == 0 || (alignment & (alignment - 1)) != 0)
        {
            throw std::invalid_argument("Alignment must be a power of two");
        }

        // Round up size to multiple of alignment
        std::size_t mod = size_ % alignment;
        if (mod)
        {
            size_ += (alignment - mod);
        }

        // Allocate aligned memory
        void* mem = std::aligned_alloc(alignment, size_);
        if (!mem)
        {
            throw std::bad_alloc();
        }

        begin_ = reinterpret_cast<Pointer>(mem);
        next_.store(begin_, std::memory_order_relaxed);
    }

    /**
     * @brief Destructor - frees the allocated memory
     * 
     * Thread-safe: acquires mutex before freeing to prevent
     * concurrent access during destruction.
     */
    ~ArenaBlock()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (begin_)
        {
            std::free(begin_);
            begin_ = nullptr;
            next_.store(nullptr, std::memory_order_relaxed);
        }
    }

    // Non-copyable
    ArenaBlock(const ArenaBlock&) = delete;
    ArenaBlock& operator=(const ArenaBlock&) = delete;

    /**
     * @brief Move constructor - transfers ownership of memory
     * @param other The block to move from
     * 
     * Thread-safe: locks the source block during the move.
     * After the move, the source block is left in a valid but empty state.
     */
    ArenaBlock(ArenaBlock&& other) MYLANG_NOEXCEPT
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        size_ = other.size_;
        begin_ = other.begin_;
        next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);

        // Reset source
        other.begin_ = nullptr;
        other.next_.store(nullptr, std::memory_order_relaxed);
        other.size_ = 0;
    }

    /**
     * @brief Move assignment operator
     * @param other The block to move from
     * @return Reference to this block
     * 
     * Thread-safe: locks both blocks during the operation.
     * Frees any existing memory before taking ownership of the source's memory.
     */
    ArenaBlock& operator=(ArenaBlock&& other) MYLANG_NOEXCEPT
    {
        if (this != &other)
        {
            std::scoped_lock lock(mutex_, other.mutex_);

            // Free existing memory
            if (begin_)
            {
                std::free(begin_);
            }

            // Take ownership
            size_ = other.size_;
            begin_ = other.begin_;
            next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);

            // Reset source
            other.begin_ = nullptr;
            other.next_.store(nullptr, std::memory_order_relaxed);
            other.size_ = 0;
        }
        return *this;
    }

    //==========================================================================
    // Accessors
    //==========================================================================

    /**
     * @brief Get Pointer to the beginning of the block
     * @return Pointer to start of memory block
     * 
     * Thread-safe: protected by mutex
     */
    Pointer begin() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return begin_;
    }

    /**
     * @brief Get Pointer to the end of the block
     * @return Pointer to one past the last byte
     * 
     * Thread-safe: protected by mutex
     */
    Pointer end() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return begin_ + size_;
    }

    /**
     * @brief Get the current next Pointer (for inspection)
     * @return Current value of the bump Pointer
     * 
     * Lock-free: uses atomic acquire ordering
     */
    Pointer cnext() const { return next_.load(std::memory_order_acquire); }

    /**
     * @brief Get the total size of the block
     * @return Size in bytes
     * 
     * Thread-safe: protected by mutex
     */
    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    /**
     * @brief Calculate remaining free space in the block
     * @return Number of bytes still available for allocation
     * 
     * Thread-safe: combines locked and atomic operations
     * 
     * @note This is a snapshot value - by the time you use it,
     *       another thread may have consumed the space.
     */
    std::size_t remaining() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!begin_)
        {
            return 0;
        }
        Pointer current_next = next_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(begin_ + size_ - current_next);
    }

    //==========================================================================
    // Allocation Methods
    //==========================================================================

    /**
     * @brief Allocate memory from this block with alignment
     * @param bytes Number of bytes to allocate
     * @param alignment Optional alignment requirement
     * @return Pointer to allocated memory, or nullptr if insufficient space
     * @throws std::invalid_argument if alignment is invalid
     * 
     * Thread-safe: uses lock-free CAS (Compare-And-Swap) loop
     * 
     * Algorithm:
     * 1. Load current next Pointer
     * 2. Calculate aligned address and padding
     * 3. Check if enough space remains
     * 4. Attempt to atomically update next Pointer
     * 5. If CAS fails, retry (another thread allocated first)
     * 
     * @performance O(1) in the uncontended case, O(n) with high contention
     */
    Pointer allocate(std::size_t bytes, std::optional<std::size_t> alignment = std::nullopt)
    {
        if (!begin_ || bytes == 0)
        {
            return nullptr;
        }

        std::size_t alignment_value = alignment.value_or(std::alignment_of<std::max_align_t>::value);

        // Validate alignment is a power of 2
        if (alignment_value == 0 || (alignment_value & (alignment_value - 1)) != 0)
        {
            throw std::invalid_argument("Invalid alignment!");
        }

        // Lock-free allocation loop
        Pointer current_next = next_.load(std::memory_order_acquire);

        while (true)
        {
            // Calculate aligned address
            std::uintptr_t cur = reinterpret_cast<std::uintptr_t>(current_next);
            std::uintptr_t aligned = (cur + (alignment_value - 1)) & ~(alignment_value - 1);
            std::size_t pad = aligned - cur;

            // Check if we have enough space (including padding)
            std::size_t remaining = begin_ + size_ - current_next;
            if (remaining < bytes + pad)
            {
                return nullptr;  // Not enough space
            }

            Pointer new_next = reinterpret_cast<Pointer>(aligned + bytes);

            // Try to atomically update next Pointer
            if (next_.compare_exchange_weak(
                  current_next, new_next, std::memory_order_release, std::memory_order_acquire))
            {
                // Success! Return the aligned address
                return reinterpret_cast<Pointer>(aligned);
            }

            // CAS failed - another thread allocated first
            // current_next was updated by compare_exchange_weak, retry
        }
    }

    /**
     * @brief Reserve memory without alignment (faster)
     * @param bytes Number of bytes to reserve
     * @return Pointer to reserved memory, or nullptr if insufficient space
     * 
     * Lock-free: uses atomic CAS without alignment calculation
     * 
     * @warning Returned Pointer may not be aligned!
     * @note Prefer allocate() for most use cases
     */
    Pointer reserve(const std::size_t bytes)
    {
        if (!begin_ || bytes == 0)
        {
            return nullptr;
        }

        Pointer current_next = next_.load(std::memory_order_acquire);

        while (true)
        {
            // Check if we have enough space
            std::size_t remaining = begin_ + size_ - current_next;
            if (remaining < bytes)
            {
                return nullptr;
            }

            Pointer new_next = current_next + bytes;

            if (next_.compare_exchange_weak(
                  current_next, new_next, std::memory_order_release, std::memory_order_acquire))
            {
                return current_next;
            }
        }
    }
};

//==============================================================================
// Fast Allocation Block - Size-Specific Lock-Free Block
//==============================================================================

/**
 * @class LockFreeFastAllocBlock
 * @brief Optimized memory block for fixed-size allocations
 * @tparam ObjectSize Size category (8, 16, 32, 64, 128, 256 bytes)
 * 
 * Features:
 * - Completely lock-free allocation
 * - All members are atomic for maximum concurrency
 * - Optimized for small, frequent allocations
 * - No alignment calculation overhead
 * 
 * @note This class is fully thread-safe without mutexes
 */
template<std::size_t ObjectSize>
class LockFreeFastAllocBlock
{
   private:
    /// Size of the block (atomic for move operations)
    std::atomic<std::size_t> size_{DEFAULT_BLOCK_SIZE};

    /// Beginning of allocated memory (atomic)
    std::atomic<Pointer> begin_{nullptr};

    /// Next available byte (atomic bump Pointer)
    std::atomic<Pointer> next_{nullptr};

   public:
    /// Default constructor - does not allocate
    LockFreeFastAllocBlock() = default;

    /**
     * @brief Construct and allocate a block of specified size
     * @param size Requested size (will be rounded up to multiple of ObjectSize)
     * @throws std::bad_alloc if allocation fails
     * 
     * The block is automatically aligned to ObjectSize boundary.
     */
    explicit LockFreeFastAllocBlock(std::size_t size)
    {
        // Use at least DEFAULT_BLOCK_SIZE
        std::size_t actual_size = size > DEFAULT_BLOCK_SIZE ? size : DEFAULT_BLOCK_SIZE;

        // Round up to multiple of ObjectSize
        actual_size = (actual_size + ObjectSize - 1) & ~(ObjectSize - 1);

        // Allocate aligned memory
        Pointer mem = reinterpret_cast<Pointer>(std::aligned_alloc(ObjectSize, actual_size));
        if (!mem)
        {
            throw std::bad_alloc();
        }

        // Store atomically
        size_.store(actual_size, std::memory_order_relaxed);
        begin_.store(mem, std::memory_order_relaxed);
        next_.store(mem, std::memory_order_relaxed);
    }

    /**
     * @brief Destructor - frees memory if allocated
     * 
     * Thread-safe: atomic operations ensure no race conditions
     */
    ~LockFreeFastAllocBlock()
    {
        Pointer mem = begin_.load(std::memory_order_relaxed);
        if (mem)
        {
            std::free(mem);
            begin_.store(nullptr, std::memory_order_relaxed);
            next_.store(nullptr, std::memory_order_relaxed);
        }
    }

    // Non-copyable
    LockFreeFastAllocBlock(const LockFreeFastAllocBlock&) = delete;
    LockFreeFastAllocBlock& operator=(const LockFreeFastAllocBlock&) = delete;

    /**
     * @brief Move constructor - atomically transfers ownership
     * @param other Source block to move from
     */
    LockFreeFastAllocBlock(LockFreeFastAllocBlock&& other) MYLANG_NOEXCEPT
    {
        size_.store(other.size_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        begin_.store(other.begin_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);

        other.begin_.store(nullptr, std::memory_order_relaxed);
        other.next_.store(nullptr, std::memory_order_relaxed);
        other.size_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Move assignment operator
     * @param other Source block to move from
     * @return Reference to this block
     */
    LockFreeFastAllocBlock& operator=(LockFreeFastAllocBlock&& other) MYLANG_NOEXCEPT
    {
        if (this != &other)
        {
            // Free existing memory
            Pointer old_mem = begin_.load(std::memory_order_relaxed);
            if (old_mem)
            {
                std::free(old_mem);
            }

            // Transfer ownership atomically
            size_.store(other.size_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            begin_.store(other.begin_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);

            // Reset source
            other.begin_.store(nullptr, std::memory_order_relaxed);
            other.next_.store(nullptr, std::memory_order_relaxed);
            other.size_.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    //==========================================================================
    // Accessors (All Lock-Free)
    //==========================================================================

    /// Get beginning of block
    Pointer begin() const { return begin_.load(std::memory_order_acquire); }

    /// Get end of block
    Pointer end() const
    {
        Pointer b = begin_.load(std::memory_order_acquire);
        std::size_t s = size_.load(std::memory_order_acquire);
        return b ? (b + s) : nullptr;
    }

    /// Get current next Pointer
    Pointer cnext() const { return next_.load(std::memory_order_acquire); }

    /// Get block size
    std::size_t size() const { return size_.load(std::memory_order_acquire); }

    /**
     * @brief Get remaining free space
     * @return Bytes available for allocation
     * 
     * Lock-free: all operations are atomic
     */
    std::size_t remaining() const
    {
        Pointer b = begin_.load(std::memory_order_acquire);
        if (!b)
        {
            return 0;
        }
        Pointer current = next_.load(std::memory_order_acquire);
        std::size_t s = size_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(b + s - current);
    }

    /**
     * @brief Allocate memory from this block
     * @param alloc_size Number of bytes to allocate
     * @return Pointer to allocated memory, or nullptr if insufficient space
     * 
     * Lock-free: uses atomic CAS loop for allocation
     * 
     * @note No alignment calculation - assumes ObjectSize alignment is sufficient
     * @performance Extremely fast in the uncontended case
     */
    MYLANG_NODISCARD
    Pointer allocate(std::size_t alloc_size)
    {
        if (alloc_size == 0)
        {
            return nullptr;
        }

        Pointer b = begin_.load(std::memory_order_acquire);
        if (!b)
        {
            std::cerr << "Failed to load begin Pointer" << std::endl;
            return nullptr;
        }

        Pointer current_next = next_.load(std::memory_order_acquire);
        std::size_t s = size_.load(std::memory_order_acquire);

        // Lock-free allocation loop
        while (true)
        {
            std::size_t remaining = b + s - current_next;
            if (remaining < alloc_size)
            {
                std::cerr << "Failed to allocate because there's not enough remaining bytes" << std::endl;
                return nullptr;
            }

            Pointer new_next = current_next + alloc_size;

            if (next_.compare_exchange_weak(
                  current_next, new_next, std::memory_order_release, std::memory_order_acquire))
            {
                return current_next;
            }
        }
    }
};

//==============================================================================
// Debugging and Tracking Structures
//==============================================================================

/**
 * @struct AllocationHeader
 * @brief Metadata for allocation tracking and debugging
 * 
 * Contains magic numbers, checksums, and timing information
 * to detect memory corruption and track allocation patterns.
 */
struct AllocationHeader
{
    uint_fast32_t magic;  ///< Magic number for validation
    uint_fast32_t size;  ///< Allocation size in bytes
    uint_fast32_t alignment;  ///< Alignment requirement
    uint_fast32_t checksum;  ///< Simple checksum for corruption detection
    std::chrono::steady_clock::time_point timestamp;  ///< When allocated
    const char32_t* type_name;  ///< Type name (for debugging)
    uint_fast32_t line_number;  ///< Source line (for debugging)
    const char32_t* file_name;  ///< Source file (for debugging)
    static constexpr uint32_t MAGIC = 0xDEADC0DE;  ///< Expected magic value

    /// Compute checksum from header fields
    uint32_t compute_checksum() const { return magic ^ size ^ alignment; }
    /// Validate header integrity
    bool is_valid() const { return magic == MAGIC && checksum == compute_checksum(); }
};

/**
 * @struct AllocationFooter
 * @brief Guard value placed after allocations to detect buffer overruns
 */
struct AllocationFooter
{
    uint32_t guard;  ///< Guard value
    static constexpr uint32_t GUARD = 0xFEEDFACE;  ///< Expected guard value

    /// Check if footer is intact
    bool is_valid() const { return guard == GUARD; }
};

//==============================================================================
// Free List Management
//==============================================================================

/**
 * @struct FreeListRegion
 * @brief Represents a freed memory region available for reuse
 * 
 * Used in free lists to track deallocated memory that can be
 * recycled for future allocations. Supports best-fit allocation
 * through ordering by size.
 */
struct FreeListRegion
{
    Pointer ptr;  ///< Pointer to freed memory
    std::size_t size;  ///< Size in bytes

    FreeListRegion() = default;

    /**
     * @brief Construct a free region
     * @param p Pointer to the freed memory
     * @param s Size of the region in bytes
     */
    FreeListRegion(Pointer p, std::size_t s) :
        ptr(p),
        size(s)
    {
    }

    FreeListRegion(const FreeListRegion&) = default;

    /**
     * @brief Move constructor - transfers ownership
     * @param other Source region to move from
     */
    FreeListRegion(FreeListRegion&& other) MYLANG_NOEXCEPT: ptr(other.ptr), size(other.size)
    {
        other.ptr = nullptr;
        other.size = 0;
    }

    FreeListRegion& operator=(const FreeListRegion&) = default;
    FreeListRegion& operator=(FreeListRegion&&) MYLANG_NOEXCEPT = default;

    /**
     * @brief Check if this region can hold an allocation
     * @param size Required size in bytes
     * @return true if this region is large enough
     */
    bool has_space_for(std::size_t size) const { return size >= size; }

    /**
     * @brief Comparison operator for ordered containers
     * @param other Region to compare with
     * @return true if this region should be ordered before other
     * 
     * Orders by size first (for best-fit), then by address (for stability)
     */
    bool operator<(const FreeListRegion& other) const
    {
        if (size != other.size)
        {
            return size < other.size;
        }
        return ptr < other.ptr;
    }
};

/**
 * @class FastAllocBlockFreeList
 * @brief Thread-safe free list for a specific size category
 * @tparam ObjectSize Size category (8, 16, 32, 64, 128, 256 bytes)
 * 
 * Uses a multiset for O(log n) best-fit allocation.
 * Best-fit minimizes fragmentation compared to first-fit.
 * 
 * Thread-safety: Protected by internal mutex
 */
template<std::size_t ObjectSize>
class FastAllocBlockFreeList
{
   private:
    /// Ordered set of free regions (sorted by size, then address)
    std::multiset<FreeListRegion> regions_;

    /// Mutex protecting the regions set
    mutable std::mutex mutex_;

   public:
    FastAllocBlockFreeList() = default;

    /**
     * @brief Construct from a vector of regions
     * @param regions Initial regions to add to the free list
     */
    explicit FastAllocBlockFreeList(const std::vector<FreeListRegion>& regions)
    {
        for (const FreeListRegion& reg : regions)
        {
            regions_.insert(reg);
        }
    }

    /**
     * @brief Move constructor
     * @param other Source free list to move from
     * 
     * Thread-safe: locks the source during move
     */
    FastAllocBlockFreeList(FastAllocBlockFreeList&& other) MYLANG_NOEXCEPT
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        regions_ = std::move(other.regions_);
    }

    /**
     * @brief Move assignment operator
     * @param other Source free list to move from
     * @return Reference to this free list
     * 
     * Thread-safe: locks both lists during operation
     */
    FastAllocBlockFreeList& operator=(FastAllocBlockFreeList&& other) MYLANG_NOEXCEPT
    {
        if (this != &other)
        {
            std::scoped_lock lock(mutex_, other.mutex_);
            regions_ = std::move(other.regions_);
        }
        return *this;
    }

    /**
     * @brief Add a region to the free list
     * @param reg Region to add
     * 
     * Thread-safe: protected by mutex
     * Complexity: O(log n) for insertion into multiset
     */
    void append(const FreeListRegion& reg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        regions_.insert(reg);
    }

    /**
     * @brief Check if the free list is empty
     * @return true if no free regions available
     * 
     * Thread-safe: protected by mutex
     */
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return regions_.empty();
    }

    /**
     * @brief Remove all regions from the free list
     * 
     * Thread-safe: protected by mutex
     */
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        regions_.clear();
    }

    /**
     * @brief Find and remove a suitable region for allocation
     * @param size Required size in bytes
     * @param align Required alignment
     * @return Optional containing the region if found, nullopt otherwise
     * 
     * Thread-safe: protected by mutex
     * 
     * Algorithm:
     * 1. Use lower_bound to find smallest region that might fit
     * 2. Iterate forward checking each region
     * 3. Account for alignment padding when checking fit
     * 4. Remove the selected region from the set
     * 5. If region is larger than needed, split it and return remainder
     * 
     * Complexity: O(log n) for lower_bound + O(k) for iteration where
     *             k is the number of regions with similar sizes
     */
    std::optional<FreeListRegion> find_and_extract(std::size_t size, std::size_t align)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find best fit (smallest region that can hold the allocation)
        FreeListRegion search_key(nullptr, size);
        auto it = regions_.lower_bound(search_key);

        while (it != regions_.end())
        {
            FreeListRegion reg = *it;

            // Calculate alignment padding
            std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(reg.ptr_);
            std::uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
            std::size_t padding = aligned - addr;

            if (reg.size_ >= size + padding)
            {
                // Found a suitable region
                regions_.erase(it);

                // Split if there's leftover space
                std::size_t used = size + padding;
                if (reg.size_ > used)
                {
                    Pointer remainder = reg.ptr_ + used;
                    std::size_t remainder_size = reg.size_ - used;
                    regions_.insert(FreeListRegion(remainder, remainder_size));
                }

                return reg;
            }
            ++it;
        }

        return std::nullopt;
    }
};

//==============================================================================
// Main Arena Allocator
//==============================================================================

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
class MYLANG_COMPILER_API ArenaAllocator
{
   public:
    //==========================================================================
    // Public Types
    //==========================================================================

    /**
     * @enum GrowthStrategy
     * @brief Strategy for expanding block size as allocations grow
     */
    enum class GrowthStrategy : std::int32_t {
        LINEAR,  ///< Keep block size constant
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
    using OutOfMemoryHandler = std::function<bool(std::size_t requested)>;

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
        std::size_t operator()(const void* ptr) const MYLANG_NOEXCEPT
        {
            return std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(ptr));
        }
    };

    /**
     * @struct VoidPtrEqual
     * @brief Equality comparison for void pointers
     */
    struct VoidPtrEqual
    {
        bool operator()(const void* a, const void* b) const MYLANG_NOEXCEPT { return a == b; }
    };

    //==========================================================================
    // Member Variables
    //==========================================================================

    // Statistics
    ArenaAllocStats alloc_stats_;  ///< Allocation statistics
    // General purpose blocks
    std::vector<ArenaBlock> blocks_{};  ///< Main memory blocks
    mutable std::shared_mutex blocks_mutex_;  ///< Protects blocks vector
    // Fast pools for small allocations (8, 16, 32, 64, 128, 256 bytes)
    std::vector<LockFreeFastAllocBlock<8>> fast_pool8_{};  ///< Pool for 8-byte objects
    std::vector<LockFreeFastAllocBlock<16>> fast_pool16_{};  ///< Pool for 16-byte objects
    std::vector<LockFreeFastAllocBlock<32>> fast_pool32_{};  ///< Pool for 32-byte objects
    std::vector<LockFreeFastAllocBlock<64>> fast_pool64_{};  ///< Pool for 64-byte objects
    std::vector<LockFreeFastAllocBlock<128>> fast_pool128_{};  ///< Pool for 128-byte objects
    std::vector<LockFreeFastAllocBlock<256>> fast_pool256_{};  ///< Pool for 256-byte objects
    mutable std::mutex fast_pool_mutex_;  ///< Protects all fast pools
    // Free lists for memory reuse
    FastAllocBlockFreeList<8> free_list8_;  ///< Free list for 8-byte objects
    FastAllocBlockFreeList<16> free_list16_;  ///< Free list for 16-byte objects
    FastAllocBlockFreeList<32> free_list32_;  ///< Free list for 32-byte objects
    FastAllocBlockFreeList<64> free_list64_;  ///< Free list for 64-byte objects
    FastAllocBlockFreeList<128> free_list128_;  ///< Free list for 128-byte objects
    FastAllocBlockFreeList<256> free_list256_;  ///< Free list for 256-byte objects
    // Configuration
    std::atomic<GrowthStrategy> growth_factor_{GrowthStrategy::LINEAR};  ///< Block growth strategy
    std::size_t block_size_{DEFAULT_BLOCK_SIZE};  ///< Initial block size
    std::atomic<std::size_t> next_block_size_{DEFAULT_BLOCK_SIZE};  ///< Next block size to allocate
    std::string name_{"arena"};  ///< Allocator name (for debugging)
    // Out-of-memory handling
    OutOfMemoryHandler oom_handler_{nullptr};  ///< OOM callback
    mutable std::mutex oom_handler_mutex_;  ///< Protects OOM handler calls
    // General free list for large allocations
    std::multiset<FreeListRegion> free_list_{};  ///< General purpose free list
    mutable std::mutex free_list_mutex_;  ///< Protects general free list
    // Debugging and tracking
    std::unordered_map<void*, AllocationHeader, VoidPtrHash, VoidPtrEqual> allocation_map_{};  ///< Allocation metadata
    mutable std::shared_mutex allocation_map_mutex_;  ///< Protects allocation map
    std::unordered_set<void*, VoidPtrHash, VoidPtrEqual>
      allocated_ptrs_{};  ///< Active allocations (double-free protection)
    mutable std::shared_mutex allocated_ptrs_mutex_;  ///< Protects allocated pointers set
    // Feature flags
    std::atomic<bool> track_allocations_{false};  ///< Enable allocation tracking
    std::atomic<bool> debug_features_{false};  ///< Enable debug features
    std::atomic<bool> enable_statistics_{true};  ///< Enable statistics collection
    // Alignment settings
    std::size_t min_alignment_{std::alignment_of<std::max_align_t>::value};  ///< Minimum alignment
    std::atomic<std::size_t> max_block_size_{MAX_BLOCK_SIZE};  ///< Maximum block size

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
    ArenaAllocator(std::int32_t growth_strategy = static_cast<std::int32_t>(GrowthStrategy::EXPONENTIAL),
      std::size_t min_align = std::alignment_of<std::max_align_t>::value,
      OutOfMemoryHandler oom_handler = nullptr,
      bool debug = false) :
        growth_factor_(GrowthStrategy(growth_strategy)),
        min_alignment_(min_align),
        oom_handler_(oom_handler),
        debug_features_(debug)
    {
        // Validate alignment
        if (min_alignment_ == 0 || (min_alignment_ & (min_alignment_ - 1)) != 0)
        {
            min_alignment_ = alignof(std::max_align_t);
        }

        // Enable tracking in debug mode
        if (debug_features_.load(std::memory_order_relaxed))
        {
            track_allocations_.store(true, std::memory_order_relaxed);
            enable_statistics_.store(true, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Destructor - frees all memory
     * 
     * Automatically calls reset() to release all resources.
     */
    ~ArenaAllocator() { reset(); }

    // Non-copyable and non-movable (contains mutexes)
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&&) MYLANG_NOEXCEPT = delete;
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
    void reset()
    {
        // Acquire all locks in consistent order
        std::unique_lock blocks_lock(blocks_mutex_);
        std::unique_lock fast_pool_lock(fast_pool_mutex_);
        std::unique_lock free_list_lock(free_list_mutex_);
        std::unique_lock alloc_map_lock(allocation_map_mutex_);
        std::unique_lock alloc_ptrs_lock(allocated_ptrs_mutex_);

        // Clear all containers (automatically frees memory via destructors)
        blocks_.clear();
        fast_pool8_.clear();
        fast_pool16_.clear();
        fast_pool32_.clear();
        fast_pool64_.clear();
        fast_pool128_.clear();
        fast_pool256_.clear();

        free_list_.clear();
        free_list8_.clear();
        free_list16_.clear();
        free_list32_.clear();
        free_list64_.clear();
        free_list128_.clear();
        free_list256_.clear();

        if (track_allocations_.load(std::memory_order_relaxed))
        {
            allocation_map_.clear();
        }

        allocated_ptrs_.clear();

        // Reset statistics
        alloc_stats_.total_allocations.store(0, std::memory_order_relaxed);
        alloc_stats_.total_allocated.store(0, std::memory_order_relaxed);
        alloc_stats_.total_free.store(0, std::memory_order_relaxed);
        alloc_stats_.total_deallocations.store(0, std::memory_order_relaxed);
        alloc_stats_.active_blocks.store(0, std::memory_order_relaxed);

        // Reset block size
        next_block_size_.store(block_size_, std::memory_order_relaxed);
    }

    /**
     * @brief Get total bytes allocated (cumulative)
     * @return Total bytes allocated since construction or last reset
     */
    std::size_t total_allocated() const { return alloc_stats_.total_allocated.load(std::memory_order_relaxed); }

    /**
     * @brief Get total number of allocations
     * @return Number of allocation requests processed
     */
    std::size_t total_allocations() const { return alloc_stats_.total_allocations.load(std::memory_order_relaxed); }

    /**
     * @brief Get number of active memory blocks
     * @return Count of currently allocated blocks
     */
    std::size_t active_blocks() const { return alloc_stats_.active_blocks.load(std::memory_order_relaxed); }

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
    Pointer allocate_block(
      std::size_t requested, std::size_t alignment_ = alignof(std::max_align_t), bool retry_on_oom = true)
    {
        // Validate and fix alignment if needed
        if (alignment_ == 0 || (alignment_ & (alignment_ - 1)) != 0)
        {
            alignment_ = min_alignment_;
        }

        // Determine block size (including growth strategy)
        std::size_t block_size = std::max(requested + alignment_, next_block_size_.load(std::memory_order_relaxed));

        // Check against maximum
        if (block_size > max_block_size_.load(std::memory_order_relaxed))
        {
            if (retry_on_oom)
            {
                std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
                if (oom_handler_ && oom_handler_(block_size))
                {
                    return allocate_block(requested, alignment_, false);  // Retry once
                }
            }
            return nullptr;
        }

        try
        {
            // Add new block to vector (requires exclusive lock)
            std::unique_lock<std::shared_mutex> lock(blocks_mutex_);
            blocks_.emplace_back(block_size, alignment_);
            alloc_stats_.safe_increment(alloc_stats_.active_blocks);
            return blocks_.back().begin();
        } catch (const std::bad_alloc&)
        {
            if (retry_on_oom)
            {
                std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
                if (oom_handler_ && oom_handler_(block_size))
                {
                    return allocate_block(requested, alignment_, false);  // Retry once
                }
            }
            return nullptr;
        }
    }

    /**
     * @brief Allocate a fast block for small objects
     * @tparam ObjectSize Size category (8, 16, 32, 64, 128, 256)
     * @param size Minimum size needed
     * @param retry_on_oom Whether to call OOM handler on failure
     * @return Pointer to block start, or nullptr on failure
     * 
     * Internal method for allocating size-specific blocks.
     * 
     * Thread-safe: Protected by fast_pool_mutex.
     */
    template<std::size_t ObjectSize>
    MYLANG_NODISCARD Pointer allocate_fast_block(std::size_t size, bool retry_on_oom = true)
    {
        std::size_t alloc_size = std::max(size, static_cast<std::size_t>(DEFAULT_BLOCK_SIZE));

        if (alloc_size > MAX_BLOCK_SIZE)
        {
            if (retry_on_oom)
            {
                std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
                if (oom_handler_ && oom_handler_(alloc_size))
                {
                    return allocate_fast_block<ObjectSize>(size, false);
                }
            }
            return nullptr;
        }

        try
        {
            LockFreeFastAllocBlock<ObjectSize> arena_block(alloc_size);

            std::lock_guard<std::mutex> lock(fast_pool_mutex_);
            std::vector<LockFreeFastAllocBlock<ObjectSize>>* pool = choose_pool<ObjectSize>();
            if (!pool)
            {
                return nullptr;
            }

            Pointer ret = arena_block.begin();
            pool->push_back(std::move(arena_block));
            alloc_stats_.safe_increment(alloc_stats_.active_blocks_);

            return ret;
        } catch (const std::bad_alloc&)
        {
            if (retry_on_oom)
            {
                std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
                if (oom_handler_ && oom_handler_(alloc_size))
                {
                    return allocate_fast_block<ObjectSize>(size, false);
                }
            }
            return nullptr;
        }
    }

    //==========================================================================
    // Public Allocation API
    //==========================================================================

    /**
     * @brief Allocate memory for objects of type T
     * @tparam _Tp Type to allocate
     * @param count Number of objects (default: 1)
     * @return Pointer to allocated memory, or nullptr on failure
     * @throws std::bad_alloc if count would cause overflow
     * 
     * Features:
     * - Automatic type alignment
     * - Overflow detection
     * - Fast path for small objects (≤256 bytes)
     * - Automatic object construction for non-trivial types
     * - Double-free protection (when debug enabled)
     * - Optional allocation tracking
     * 
     * Algorithm:
     * 1. Validate count (overflow check)
     * 2. Calculate size and alignment
     * 3. Try fast pool if applicable (8-256 bytes)
     * 4. Fall back to general blocks
     * 5. Construct objects if needed
     * 6. Update statistics and tracking
     * 
     * Thread-safe: Yes
     * 
     * @example
     * @code
     * // Single object
     * MyClass* obj = arena.allocate<MyClass>();
     * 
     * // Array of objects
     * int* array = arena.allocate<int>(1000);
     * @endcode
     */
    template<typename _Tp>
    MYLANG_NODISCARD MYLANG_COMPILER_API _Tp* allocate(std::size_t count = 1)
    {
        std::chrono::steady_clock::time_point start = std::chrono::high_resolution_clock::now();

        // Validate count
        if (count == 0)
        {
            return nullptr;
        }

        // Check for overflow
        if (count > MAX_BLOCK_SIZE / sizeof(_Tp))
        {
            std::cerr << "allocation size is too large!" << std::endl;
            if (oom_handler_ && oom_handler_(count))
            {
                return allocate<_Tp>(count);
            }
            throw std::bad_alloc();
        }

        std::size_t alloc_size = count * sizeof(_Tp);
        std::size_t align = std::max(std::alignment_of<_Tp>::value, min_alignment_);

        if (alloc_size > MAX_BLOCK_SIZE)
        {
            std::cerr << "allocation size (after alignment) is too large!" << std::endl;
            return nullptr;
        }

        Pointer mem = nullptr;

        // Try fast pool for small objects
        constexpr std::size_t type_size = sizeof(_Tp);
        constexpr std::size_t pool_size = type_size <= 8 ? 8
          : type_size <= 16                              ? 16
          : type_size <= 32                              ? 32
          : type_size <= 64                              ? 64
          : type_size <= 128                             ? 128
          : type_size <= 256                             ? 256
                                                         : 0;

        constexpr bool use_fast_pool = (pool_size > 0) && (pool_size >= type_size);

        if constexpr (use_fast_pool)
        {
            if (align <= pool_size)
            {
                mem = allocate_from_fast_pool<pool_size>(alloc_size);
                if (!mem)
                {
                    std::cerr << "allocate_from_fast_pool() failed!" << std::endl;
                }
            }
        }

        // Fall back to general blocks
        if (!mem)
        {
            mem = allocate_from_blocks(alloc_size, align);
        }

        if (!mem)
        {
            std::cerr << "allocate_from_blocks() failed!" << std::endl;
            return nullptr;
        }

        _Tp* region = reinterpret_cast<_Tp*>(mem);

        // Update statistics
        if (enable_statistics_.load(std::memory_order_relaxed))
        {
            alloc_stats_.safe_increment(alloc_stats_.total_allocations_);
            alloc_stats_.safe_increment(alloc_stats_.total_allocated_, alloc_size);
        }

        // Track allocation if enabled
        if (track_allocations_.load(std::memory_order_relaxed))
        {
            AllocationHeader header{};
            header.magic_ = AllocationHeader::MAGIC_;
            header.size_ = static_cast<std::uint32_t>(alloc_size);
            header.alignment_ = static_cast<std::uint32_t>(align);
            header.checksum_ = header.compute_checksum();
            header.timestamp_ = std::chrono::steady_clock::now();

            std::unique_lock<std::shared_mutex> lock(allocation_map_mutex_);
            allocation_map_[region] = header;
        }

        // Track Pointer for double-free protection
        {
            std::unique_lock<std::shared_mutex> lock(allocated_ptrs_mutex_);
            allocated_ptrs_.insert(region);
        }

        // Construct objects if needed
        if constexpr (!std::is_trivially_constructible_v<_Tp>)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                new (region + i) _Tp();
            }
        }

        std::chrono::steady_clock::time_point end = std::chrono::high_resolution_clock::now();

        return region;
    }

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
    template<typename _Tp>
    MYLANG_COMPILER_API void deallocate(_Tp* ptr, std::size_t count = 1)
    {
        if (!ptr)
        {
            return;
        }

        // Check for double-free
        {
            std::unique_lock<std::shared_mutex> lock(allocated_ptrs_mutex_);
            auto it = allocated_ptrs_.find(ptr);
            if (it == allocated_ptrs_.end())
            {
                // Double-free detected!
                if (debug_features_.load(std::memory_order_relaxed))
                {
                    std::cerr << "ERROR: Double-free detected for Pointer " << ptr << std::endl;
                }
                return;
            }
            allocated_ptrs_.erase(it);
        }

        std::size_t byte_size = count * sizeof(_Tp);

        // Call destructors for non-trivial types
        if constexpr (!std::is_trivially_destructible_v<_Tp>)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                ptr[i].~_Tp();
            }
        }

        // Determine which free list to use
        constexpr std::size_t obj_size = sizeof(_Tp);
        constexpr std::size_t pool_size = obj_size <= 8 ? 8
          : obj_size <= 16                              ? 16
          : obj_size <= 32                              ? 32
          : obj_size <= 64                              ? 64
          : obj_size <= 128                             ? 128
          : obj_size <= 256                             ? 256
                                                        : 0;

        constexpr bool use_fast_pool = (pool_size > 0) && (pool_size >= obj_size);

        FreeListRegion region(reinterpret_cast<Pointer>(ptr), byte_size);

        // Add to appropriate free list
        if constexpr (use_fast_pool)
        {
            FastAllocBlockFreeList<pool_size>* free_list = choose_pool_free_list<pool_size>();
            if (free_list)
            {
                free_list->append(region);
            }
            else
            {
                std::lock_guard<std::mutex> lock(free_list_mutex_);
                free_list_.insert(region);
            }
        }
        else
        {
            std::lock_guard<std::mutex> lock(free_list_mutex_);
            free_list_.insert(region);
        }

        // Update statistics
        if (enable_statistics_.load(std::memory_order_relaxed))
        {
            alloc_stats_.safe_increment(alloc_stats_.total_deallocations_);
        }

        // Remove from tracking
        if (track_allocations_.load(std::memory_order_relaxed) && ptr)
        {
            std::unique_lock<std::shared_mutex> lock(allocation_map_mutex_);
            allocation_map_.erase(ptr);
        }
    }

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
    bool verify_allocation(void* ptr) const
    {
        if (!track_allocations_.load(std::memory_order_relaxed))
        {
            return true;
        }

        std::shared_lock<std::shared_mutex> lock(allocation_map_mutex_);
        auto it = allocation_map_.find(ptr);
        if (it == allocation_map_.end())
        {
            return false;
        }

        return it->second.is_valid();
    }

   private:
    //==========================================================================
    // Private Allocation Helpers
    //==========================================================================

    /**
     * @brief Allocate from a fast pool
     * @tparam ObjectSize Size category (8, 16, 32, 64, 128, 256)
     * @param alloc_size Size to allocate
     * @return Pointer to allocated memory, or nullptr
     * 
     * Algorithm:
     * 1. Check free list first for recycled memory
     * 2. If free list empty, try existing block
     * 3. If block full, allocate new block
     * 4. Update block size for growth strategy
     * 
     * Thread-safe: Uses fast_pool_mutex, releases during recursive calls
     */
    template<std::size_t ObjectSize>
    MYLANG_NODISCARD Pointer allocate_from_fast_pool(std::size_t alloc_size)
    {
        std::unique_lock<std::mutex> lock(fast_pool_mutex_);

        std::vector<LockFreeFastAllocBlock<ObjectSize>>* pool = choose_pool<ObjectSize>();
        if (!pool)
        {
            std::cerr << "choose_pool() Failed!" << std::endl;
            return nullptr;
        }

        // If pool is empty, allocate a new block
        if (pool->empty())
        {
            lock.unlock();  // Release lock during recursive call

            std::size_t block_size = std::max(alloc_size, next_block_size_.load(std::memory_order_relaxed));
            if (!allocate_fast_block<ObjectSize>(block_size))
            {
                std::cerr << "allocate_fast_block() failed!" << std::endl;
                return nullptr;
            }

            update_next_block_size();
            lock.lock();  // Reacquire
        }

        // Try free list first
        Pointer mem = allocate_using_fast_free_list<ObjectSize>(alloc_size);
        if (mem)
        {
            return mem;
        }

        // Try current block
        LockFreeFastAllocBlock<ObjectSize>& fast_block = pool->back();

        if (fast_block.remaining() < alloc_size)
        {
            lock.unlock();  // Release lock during recursive call

            std::size_t block_size = std::max(alloc_size, next_block_size_.load(std::memory_order_relaxed));
            if (!allocate_fast_block<ObjectSize>(block_size))
            {
                std::cerr << "allocate_fast_block() failed!" << std::endl;
                return nullptr;
            }

            update_next_block_size();
            lock.lock();  // Reacquire
        }

        Pointer ret = pool->back().allocate(alloc_size);
        /*
         * 
        std::string print_value =
          ret != nullptr ? std::to_string(reinterpret_cast<std::uintptr_t>(ret)) : "nullptr";
        std::cout << "-- DEBUG : allocate_from_fast_pool() returned with ret = " << print_value
                  << std::endl;
         */
        return ret;
    }

    /**
     * @brief Try to allocate from fast pool's free list
     * @tparam ObjectSize Size category
     * @param alloc_size Size to allocate
     * @return Pointer to recycled memory, or nullptr
     * 
     * Implements best-fit allocation from freed memory.
     * Thread-safe: Free list has internal locking
     */
    template<std::size_t ObjectSize>
    MYLANG_NODISCARD Pointer allocate_using_fast_free_list(std::size_t alloc_size)
    {
        FastAllocBlockFreeList<ObjectSize>* free_list = choose_pool_free_list<ObjectSize>();
        if (!free_list)
        {
            return nullptr;
        }

        std::optional<FreeListRegion> result = free_list->find_and_extract(alloc_size, ObjectSize);
        if (result.has_value())
        {
            // Align the result
            std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(result->ptr_);
            std::uintptr_t aligned = (addr + (ObjectSize - 1)) & ~(ObjectSize - 1);
            return reinterpret_cast<Pointer>(aligned);
        }

        return nullptr;
    }

    MYLANG_NODISCARD
    Pointer allocate_using_free_list(std::size_t alloc_size, std::size_t align)
    {
        std::lock_guard<std::mutex> lock(free_list_mutex_);
        if (free_list_.empty())
        {
            return nullptr;
        }

        // Find best fit using multiset's ordering
        FreeListRegion search_key(nullptr, alloc_size);
        auto it = free_list_.lower_bound(search_key);

        while (it != free_list_.end())
        {
            FreeListRegion reg = *it;
            // Check alignment of the Pointer
            std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(reg.ptr);
            std::uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
            std::size_t padding = aligned - addr;

            if (reg.size >= alloc_size + padding)
            {
                Pointer result = reinterpret_cast<Pointer>(aligned);
                // Remove the used region
                free_list_.erase(it);
                // If there's leftover space, split the region
                std::size_t used = alloc_size + padding;

                if (reg.size > used)
                {
                    Pointer remainder = reg.ptr + used;
                    std::size_t remainder_size = reg.size - used;
                    free_list_.insert(FreeListRegion(remainder, remainder_size));
                }
                return result;
            }
            ++it;
        }
        return nullptr;
    }

    MYLANG_NODISCARD
    Pointer allocate_from_blocks(std::size_t alloc_size, std::size_t align)
    {
        // Validate alignment
        align = min_alignment_;

        if (align == 0 || (align & (align - 1)) != 0)
        {
            std::shared_lock<std::shared_mutex> lock(blocks_mutex_);
            if (!blocks_.empty())
            {
                // Try free list first
                Pointer mem = allocate_using_free_list(alloc_size, align);
                if (mem)
                {
                    return mem;
                }

                // Try current block
                mem = blocks_.back().allocate(alloc_size, align);
                if (mem)
                {
                    return mem;
                }
            }
        }

        // Need a new block
        std::size_t new_block_size = std::max(alloc_size, next_block_size_.load(std::memory_order_relaxed));
        if (!allocate_block(new_block_size, align))
        {
            if (debug_features_.load(std::memory_order_relaxed))
            {
                std::cerr << "-- Failed to allocate block : ArenaAllocator::allocate_block()" << std::endl;
            }
            return nullptr;
        }

        update_next_block_size();
        std::shared_lock<std::shared_mutex> lock(blocks_mutex_);
        return blocks_.back().allocate(alloc_size, align);
    }

    void update_next_block_size() MYLANG_NOEXCEPT
    {
        // TODO : prevent overflow
        if (growth_factor_.load(std::memory_order_relaxed) == GrowthStrategy::EXPONENTIAL)
        {
            std::size_t current = next_block_size_.load(std::memory_order_relaxed);
            std::size_t max_size = max_block_size_.load(std::memory_order_relaxed);
            std::size_t new_size = std::min(current * 2, max_size);
            next_block_size_.store(new_size, std::memory_order_relaxed);
        }
    }

    template<std::size_t ObjectSize>
    MYLANG_NODISCARD std::vector<LockFreeFastAllocBlock<ObjectSize>>* choose_pool() MYLANG_NOEXCEPT
    {
        if constexpr (ObjectSize == 8)
        {
            return &fast_pool8_;
        }
        else if constexpr (ObjectSize == 16)
        {
            return &fast_pool16_;
        }
        else if constexpr (ObjectSize == 32)
        {
            return &fast_pool32_;
        }
        else if constexpr (ObjectSize == 64)
        {
            return &fast_pool64_;
        }
        else if constexpr (ObjectSize == 128)
        {
            return &fast_pool128_;
        }
        else if constexpr (ObjectSize == 256)
        {
            return &fast_pool256_;
        }
        else
        {
            return nullptr;
        }
    }

    template<std::size_t ObjectSize>
    MYLANG_NODISCARD FastAllocBlockFreeList<ObjectSize>* choose_pool_free_list() MYLANG_NOEXCEPT
    {
        if constexpr (ObjectSize == 8)
        {
            return &free_list8_;
        }
        else if constexpr (ObjectSize == 16)
        {
            return &free_list16_;
        }
        else if constexpr (ObjectSize == 32)
        {
            return &free_list32_;
        }
        else if constexpr (ObjectSize == 64)
        {
            return &free_list64_;
        }
        else if constexpr (ObjectSize == 128)
        {
            return &free_list128_;
        }
        else if constexpr (ObjectSize == 256)
        {
            return &free_list256_;
        }
        else
        {
            return nullptr;
        }
    }
};

}  // namespace allocator
}  // namespace runtime
}  // namespace mylang