#pragma once

#include "../../macros.hpp"

#include <atomic>


namespace mylang {
namespace runtime {
namespace allocator {

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
  std::atomic<SizeType> TotalAllocations{0};
  /// Total bytes allocated (cumulative)
  std::atomic<SizeType> TotalAllocated{0};
  /// Total bytes freed (for future use)
  std::atomic<SizeType> TotalFree{0};
  /// Total number of deallocation requests
  std::atomic<SizeType> TotalDeallocations{0};
  /// Number of currently active memory blocks
  std::atomic<SizeType> ActiveBlocks{0};

  ArenaAllocStats() = default;

  /**
     * @brief Safely increment an atomic counter with overflow protection
     * @param counter The atomic counter to increment
     * @param amount The amount to add (default: 1)
     * 
     * Implements saturation arithmetic - if overflow would occur, the counter
     * remains at its maximum value instead of wrapping around.
     */
  void safe_increment(std::atomic<SizeType>& counter, SizeType amount = 1)
  {
    SizeType old_val = counter.load(std::memory_order_relaxed);
    SizeType new_val;
    do
    {
      // Check for overflow before incrementing
      if (old_val > std::numeric_limits<SizeType>::max() - amount)
        return;  // Would overflow, saturate at current value
      new_val = old_val + amount;
    } while (!counter.compare_exchange_weak(old_val, new_val, std::memory_order_relaxed, std::memory_order_relaxed));
  }
};

/**
 * @struct AllocationHeader
 * @brief Metadata for allocation tracking and debugging
 * 
 * Contains magic numbers, checksums, and timing information
 * to detect memory corruption and track allocation patterns.
 */
struct AllocationHeader
{
  uint_fast32_t                         magic;               ///< Magic number for validation
  uint_fast32_t                         size;                ///< Allocation size in bytes
  uint_fast32_t                         alignment;           ///< Alignment requirement
  uint_fast32_t                         checksum;            ///< Simple checksum for corruption detection
  std::chrono::steady_clock::time_point timestamp;           ///< When allocated
  const char32_t*                       TypeName;            ///< Type name (for debugging)
  uint_fast32_t                         LineNumber;          ///< Source line (for debugging)
  const char32_t*                       filename;            ///< Source file (for debugging)
  static MYLANG_CONSTEXPR uint32_t      MAGIC = 0xDEADC0DE;  ///< Expected magic value

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
  uint32_t                         guard;               ///< Guard value
  static MYLANG_CONSTEXPR uint32_t GUARD = 0xFEEDFACE;  ///< Expected guard value
  /// Check if footer is intact
  bool is_valid() const { return guard == GUARD; }
};
}
}
}