#pragma once

#include "../../diag/diagnostic.hpp"
#include "../../macros.hpp"

#include <atomic>
#include <mutex>
#include <optional>


namespace mylang {
namespace runtime {
namespace allocator {

using Byte    = std::uint_fast8_t;  /// Single byte type for Pointer arithmetic
using Pointer = Byte*;              /// Generic Pointer type for arena memory

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
  SizeType Size_{DEFAULT_BLOCK_SIZE};
  /// Pointer to the beginning of allocated memory
  Pointer Begin_{nullptr};
  /// Atomic Pointer to the next available byte (bump Pointer)
  std::atomic<Pointer> Next_{nullptr};
  /// Mutex for protecting structural operations (not allocation)
  mutable std::mutex Mutex_;

 public:
  /**
     * @brief Construct an arena block with specified size and alignment
     * @param size Size of the block in bytes
     * @param alignment Required alignment (must be power of 2)
     * @throws emit error if alignment is not a power of 2
     * @throws panic if memory allocation fails
     * 
     * The actual allocated size may be larger than requested to satisfy
     * alignment requirements.
     */
  explicit ArenaBlock(const SizeType size = DEFAULT_BLOCK_SIZE, const SizeType alignment = alignof(std::max_align_t));

  /**
     * @brief Destructor - frees the allocated memory
     * 
     * Thread-safe: acquires mutex before freeing to prevent
     * concurrent access during destruction.
     */
  ~ArenaBlock();

  // Non-copyable
  ArenaBlock(const ArenaBlock&)            = delete;
  ArenaBlock& operator=(const ArenaBlock&) = delete;

  /**
     * @brief Move constructor - transfers ownership of memory
     * @param other The block to move from
     * 
     * Thread-safe: locks the source block during the move.
     * After the move, the source block is left in a valid but empty state.
     */
  ArenaBlock(ArenaBlock&& other) MYLANG_NOEXCEPT;

  /**
     * @brief Move assignment operator
     * @param other The block to move from
     * @return Reference to this block
     * 
     * Thread-safe: locks both blocks during the operation.
     * Frees any existing memory before taking ownership of the source's memory.
     */
  ArenaBlock& operator=(ArenaBlock&& other) MYLANG_NOEXCEPT;

  /**
     * @brief Get Pointer to the beginning of the block
     * @return Pointer to start of memory block
     * 
     * Thread-safe: protected by mutex
     */
  Pointer begin() const
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    return Begin_;
  }

  /**
     * @brief Get Pointer to the end of the block
     * @return Pointer to one past the last byte
     * 
     * Thread-safe: protected by mutex
     */
  Pointer end() const
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    return Begin_ + Size_;
  }

  /**
     * @brief Get the current next Pointer (for inspection)
     * @return Current value of the bump Pointer
     * 
     * Lock-free: uses atomic acquire ordering
     */
  Pointer cNext() const { return Next_.load(std::memory_order_acquire); }

  /**
     * @brief Get the total size of the block
     * @return Size in bytes
     * 
     * Thread-safe: protected by mutex
     */
  SizeType size() const
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    return Size_;
  }

  SizeType used() const
  {
    Pointer current = Next_.load(std::memory_order_acquire);
    if (!Begin_ || current < Begin_)
      return 0;
    return static_cast<SizeType>(current - Begin_);
  }

  bool pop(SizeType bytes)
  {
    Pointer current = Next_.load(std::memory_order_acquire);
    while (true)
    {
      if (!Begin_ || current < Begin_ + bytes)
        return false;
      Pointer desired = current - bytes;
      if (Next_.compare_exchange_weak(current, desired, std::memory_order_acq_rel, std::memory_order_acquire))
        return true;
      // current updated automatically, retry
    }
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
  SizeType remaining() const
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    if (!Begin_)
      return 0;
    Pointer current_next = Next_.load(std::memory_order_acquire);
    return static_cast<SizeType>(Begin_ + Size_ - current_next);
  }

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
  Pointer allocate(SizeType bytes, std::optional<SizeType> alignment = std::nullopt);

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
  Pointer reserve(const SizeType bytes);
};  // ArenaBlock

}
}
}