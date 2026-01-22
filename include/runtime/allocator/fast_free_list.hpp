#pragma once

#include "free_list.hpp"

#include <mutex>
#include <set>
#include <vector>


namespace mylang {
namespace runtime {
namespace allocator {

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
template<SizeType ObjectSize>
class FastAllocBlockFreeList
{
 private:
  /// Ordered set of free regions (sorted by size, then address)
  std::multiset<FreeListRegion> Regions_;
  /// Mutex protecting the regions set
  mutable std::mutex Mutex_;

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
      Regions_.insert(reg);
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
    std::lock_guard<std::mutex> lock(other.Mutex_);
    Regions_ = std::move(other.Regions_);
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
      std::scoped_lock lock(Mutex_, other.Mutex_);
      Regions_ = std::move(other.Regions_);
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
    std::lock_guard<std::mutex> lock(Mutex_);
    Regions_.insert(reg);
  }

  /**
     * @brief Check if the free list is empty
     * @return true if no free regions available
     * 
     * Thread-safe: protected by mutex
     */
  bool empty() const
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    return Regions_.empty();
  }

  /**
     * @brief Remove all regions from the free list
     * 
     * Thread-safe: protected by mutex
     */
  void clear()
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    Regions_.clear();
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
  std::optional<FreeListRegion> findAndExtract(SizeType size, SizeType align)
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    // Find best fit (smallest region that can hold the allocation)
    FreeListRegion search_key(nullptr, size);
    auto           it = Regions_.lower_bound(search_key);

    while (it != Regions_.end())
    {
      FreeListRegion reg = *it;

      // Calculate alignment padding
      std::uintptr_t addr    = reinterpret_cast<std::uintptr_t>(reg.ptr);
      std::uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
      SizeType       padding = aligned - addr;

      if (reg.size >= size + padding)
      {
        // Found a suitable region
        Regions_.erase(it);

        // Split if there's leftover space
        SizeType used = size + padding;
        if (reg.size > used)
        {
          Pointer  remainder      = reg.ptr + used;
          SizeType remainder_size = reg.size - used;
          Regions_.insert(FreeListRegion(remainder, remainder_size));
        }

        return reg;
      }
      ++it;
    }

    return std::nullopt;
  }
};

}
}
}