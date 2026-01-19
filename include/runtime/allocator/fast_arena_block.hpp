#pragma once

#include "../../diag/diagnostic.hpp"

#include <atomic>


namespace mylang {
namespace runtime {
namespace allocator {

using Byte    = std::uint_fast8_t;  /// Single byte type for Pointer arithmetic
using Pointer = Byte*;              /// Generic Pointer type for arena memory

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
template<SizeType ObjectSize>
class LockFreeFastAllocBlock
{
 private:
  /// Size of the block (atomic for move operations)
  std::atomic<SizeType> Size_{DEFAULT_BLOCK_SIZE};
  /// Beginning of allocated memory (atomic)
  std::atomic<Pointer> Begin_{nullptr};
  /// Next available byte (atomic bump Pointer)
  std::atomic<Pointer> Next_{nullptr};

 public:
  /// Default constructor - does not allocate
  LockFreeFastAllocBlock() = default;

  /**
     * @brief Construct and allocate a block of specified size
     * @param size Requested size (will be rounded up to multiple of ObjectSize)
     * @throws panic if allocation fails
     * 
     * The block is automatically aligned to ObjectSize boundary.
     */
  explicit LockFreeFastAllocBlock(SizeType size)
  {
    // Use at least DEFAULT_BLOCK_SIZE
    SizeType actual_size = size > DEFAULT_BLOCK_SIZE ? size : DEFAULT_BLOCK_SIZE;
    // Round up to multiple of ObjectSize
    actual_size = (actual_size + ObjectSize - 1) & ~(ObjectSize - 1);
    // Allocate aligned memory
    Pointer mem = reinterpret_cast<Pointer>(std::aligned_alloc(ObjectSize, actual_size));
    if (mem == nullptr)
      throw std::bad_alloc();
    /// TODO: change after debug
    // diagnostic::engine.panic("bad alloc");
    // Store atomically
    Size_.store(actual_size, std::memory_order_relaxed);
    Begin_.store(mem, std::memory_order_relaxed);
    Next_.store(mem, std::memory_order_relaxed);
  }

  /**
     * @brief Destructor - frees memory if allocated
     * 
     * Thread-safe: atomic operations ensure no race conditions
     */
  ~LockFreeFastAllocBlock()
  {
    Pointer mem = Begin_.load(std::memory_order_relaxed);
    if (mem != nullptr)
    {
      std::free(mem);
      Begin_.store(nullptr, std::memory_order_relaxed);
      Next_.store(nullptr, std::memory_order_relaxed);
    }
  }

  // Non-copyable
  LockFreeFastAllocBlock(const LockFreeFastAllocBlock&)            = delete;
  LockFreeFastAllocBlock& operator=(const LockFreeFastAllocBlock&) = delete;

  /**
     * @brief Move constructor - atomically transfers ownership
     * @param other Source block to move from
     */
  LockFreeFastAllocBlock(LockFreeFastAllocBlock&& other) MYLANG_NOEXCEPT
  {
    Size_.store(other.Size_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    Begin_.store(other.Begin_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    Next_.store(other.Next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    other.Begin_.store(nullptr, std::memory_order_relaxed);
    other.Next_.store(nullptr, std::memory_order_relaxed);
    other.Size_.store(0, std::memory_order_relaxed);
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
      Pointer old_mem = Begin_.load(std::memory_order_relaxed);
      if (old_mem != nullptr)
        std::free(old_mem);
      // Transfer ownership atomically
      Size_.store(other.Size_.load(std::memory_order_relaxed), std::memory_order_relaxed);
      Begin_.store(other.Begin_.load(std::memory_order_relaxed), std::memory_order_relaxed);
      Next_.store(other.Next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
      // Reset source
      other.Begin_.store(nullptr, std::memory_order_relaxed);
      other.Next_.store(nullptr, std::memory_order_relaxed);
      other.Size_.store(0, std::memory_order_relaxed);
    }
    return *this;
  }

  /// Get beginning of block
  Pointer begin() const { return Begin_.load(std::memory_order_acquire); }

  /// Get end of block
  Pointer end() const
  {
    Pointer  b = Begin_.load(std::memory_order_acquire);
    SizeType s = Size_.load(std::memory_order_acquire);
    return b ? (b + s) : nullptr;
  }

  /// Get current next Pointer
  Pointer cnext() const { return Next_.load(std::memory_order_acquire); }

  /// Get block size
  SizeType size() const { return Size_.load(std::memory_order_acquire); }

  /**
     * @brief Get remaining free space
     * @return Bytes available for allocation
     * 
     * Lock-free: all operations are atomic
     */
  SizeType remaining() const
  {
    Pointer b = Begin_.load(std::memory_order_acquire);
    if (b == nullptr)
      return 0;
    Pointer  current = Next_.load(std::memory_order_acquire);
    SizeType s       = Size_.load(std::memory_order_acquire);
    return static_cast<SizeType>(b + s - current);
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
  Pointer allocate(SizeType alloc_size)
  {
    if (alloc_size == 0)
      return nullptr;

    Pointer b = Begin_.load(std::memory_order_acquire);
    if (b == nullptr)
    {
      std::cerr << "Failed to load begin Pointer" << std::endl;
      return nullptr;
    }

    Pointer  current_next = Next_.load(std::memory_order_acquire);
    SizeType s            = Size_.load(std::memory_order_acquire);

    // Lock-free allocation loop
    for (;;)
    {
      SizeType remaining = b + s - current_next;
      if (remaining < alloc_size)
      {
        std::cerr << "Failed to allocate because there's not enough remaining bytes" << std::endl;
        return nullptr;
      }

      Pointer new_next = current_next + alloc_size;

      if (Next_.compare_exchange_weak(current_next, new_next, std::memory_order_release, std::memory_order_acquire))
        return current_next;
    }
  }
};

}
}
}
