#pragma once

#include "../../macros.hpp"


namespace mylang {
namespace runtime {
namespace allocator {

using Byte    = std::uint_fast8_t;  /// Single byte type for Pointer arithmetic
using Pointer = Byte*;              /// Generic Pointer type for arena memory

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
  Pointer  ptr;   ///< Pointer to freed memory
  SizeType size;  ///< Size in bytes

  FreeListRegion() = default;

  /**
     * @brief Construct a free region
     * @param p Pointer to the freed memory
     * @param s Size of the region in bytes
     */
  FreeListRegion(Pointer p, SizeType s) :
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
    other.ptr  = nullptr;
    other.size = 0;
  }

  FreeListRegion& operator=(const FreeListRegion&)            = default;
  FreeListRegion& operator=(FreeListRegion&&) MYLANG_NOEXCEPT = default;

  /**
     * @brief Check if this region can hold an allocation
     * @param size Required size in bytes
     * @return true if this region is large enough
     */
  bool hasSpaceFor(SizeType size) const { return size >= this->size; }

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
      return size < other.size;
    return ptr < other.ptr;
  }
};

}
}
}