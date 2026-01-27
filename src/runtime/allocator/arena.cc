#include "../../../include/runtime/allocator/arena.hpp"


namespace mylang {
namespace runtime {
namespace allocator {

ArenaAllocator::ArenaAllocator(std::int32_t growth_strategy, SizeType min_align, OutOfMemoryHandler oom_handler, bool debug) :
    GrowthFactor_(GrowthStrategy(growth_strategy)),
    MinAlignment_(min_align),
    OomHandler_(oom_handler),
    DebugFeatures_(debug)
{
  // Validate alignment
  if (MinAlignment_ == 0 || (MinAlignment_ & (MinAlignment_ - 1)) != 0)
  {
    MinAlignment_ = alignof(std::max_align_t);
  }
  // Enable tracking in debug mode
  if (DebugFeatures_.load(std::memory_order_relaxed))
  {
    TrackAllocations_.store(true, std::memory_order_relaxed);
    EnableStatistics_.store(true, std::memory_order_relaxed);
  }
}

void ArenaAllocator::reset()
{
  // Acquire all locks in consistent order
  std::unique_lock blocks_lock(BlocksMutex_);
  std::unique_lock fast_pool_lock(FastPoolMutex_);
  std::unique_lock free_list_lock(FreeListMutex_);
  std::unique_lock alloc_map_lock(AllocationMapMutex_);
  std::unique_lock alloc_ptrs_lock(AllocatedPtrsMutex_);

  // Clear all containers (automatically frees memory via destructors)
  Blocks_.clear();
  FastPool8_.clear();
  FastPool16_.clear();
  FastPool32_.clear();
  FastPool64_.clear();
  FastPool128_.clear();
  FastPool256_.clear();
  FreeList_.clear();
  FreeList8_.clear();
  FreeList16_.clear();
  FreeList32_.clear();
  FreeList64_.clear();
  FreeList128_.clear();
  FreeList256_.clear();

  if (TrackAllocations_.load(std::memory_order_relaxed))
  {
    AllocationMap_.clear();
  }

  AllocatedPtrs_.clear();
  // Reset statistics
  AllocStats_.TotalAllocations.store(0, std::memory_order_relaxed);
  AllocStats_.TotalAllocated.store(0, std::memory_order_relaxed);
  AllocStats_.TotalFree.store(0, std::memory_order_relaxed);
  AllocStats_.TotalDeallocations.store(0, std::memory_order_relaxed);
  AllocStats_.ActiveBlocks.store(0, std::memory_order_relaxed);
  // Reset block size
  NextBlockSize_.store(BlockSize_, std::memory_order_relaxed);
}

MYLANG_NODISCARD
Pointer ArenaAllocator::allocateBlock(SizeType requested, SizeType alignment_, bool retry_on_oom)
{
  // Validate and fix alignment if needed
  if (alignment_ == 0 || (alignment_ & (alignment_ - 1)) != 0)
  {
    alignment_ = MinAlignment_;
  }

  // Determine block size (including growth strategy)
  SizeType block_size = std::max(requested + alignment_, NextBlockSize_.load(std::memory_order_relaxed));

  // Check against maximum
  if (block_size > MaxBlockSize_.load(std::memory_order_relaxed))
  {
    if (retry_on_oom)
    {
      std::lock_guard<std::mutex> oom_lock(OomHandlerMutex_);
      if (OomHandler_ && OomHandler_(block_size))
      {
        return allocateBlock(requested, alignment_, false);  // Retry once
      }
    }
    return nullptr;
  }

  try
  {
    // Add new block to vector (requires exclusive lock)
    std::unique_lock<std::shared_mutex> lock(BlocksMutex_);
    Blocks_.emplace_back(block_size, alignment_);
    AllocStats_.safe_increment(AllocStats_.ActiveBlocks);
    return Blocks_.back().begin();
  } catch (const std::bad_alloc&)
  {
    if (retry_on_oom)
    {
      std::lock_guard<std::mutex> oom_lock(OomHandlerMutex_);
      if (OomHandler_ && OomHandler_(block_size))
      {
        return allocateBlock(requested, alignment_, false);  // Retry once
      }
    }
    return nullptr;
  }
}

MYLANG_NODISCARD
bool ArenaAllocator::verifyAllocation(void* ptr) const
{
  if (!TrackAllocations_.load(std::memory_order_relaxed))
  {
    return true;
  }
  std::shared_lock<std::shared_mutex> lock(AllocationMapMutex_);
  auto                                it = AllocationMap_.find(ptr);
  if (it == AllocationMap_.end())
  {
    return false;
  }
  return it->second.is_valid();
}

MYLANG_NODISCARD
Pointer ArenaAllocator::allocateUsingFreeList(SizeType alloc_size, SizeType align)
{
  std::lock_guard<std::mutex> lock(FreeListMutex_);
  if (FreeList_.empty())
  {
    return nullptr;
  }

  // Find best fit using multiset's ordering
  FreeListRegion search_key(nullptr, alloc_size);
  auto           it = FreeList_.lower_bound(search_key);

  while (it != FreeList_.end())
  {
    FreeListRegion reg = *it;
    // Check alignment of the Pointer
    std::uintptr_t addr    = reinterpret_cast<std::uintptr_t>(reg.ptr);
    std::uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
    SizeType       padding = aligned - addr;

    if (reg.size >= alloc_size + padding)
    {
      Pointer result = reinterpret_cast<Pointer>(aligned);
      // Remove the used region
      FreeList_.erase(it);
      // If there's leftover space, split the region
      SizeType used = alloc_size + padding;

      if (reg.size > used)
      {
        Pointer  remainder      = reg.ptr + used;
        SizeType remainder_size = reg.size - used;
        FreeList_.insert(FreeListRegion(remainder, remainder_size));
      }
      return result;
    }
    ++it;
  }
  return nullptr;
}

MYLANG_NODISCARD
Pointer ArenaAllocator::allocateFromBlocks(SizeType alloc_size, SizeType align)
{
  // Validate alignment
  align = MinAlignment_;

  if (align == 0 || (align & (align - 1)) != 0)
  {
    std::shared_lock<std::shared_mutex> lock(BlocksMutex_);
    if (!Blocks_.empty())
    {
      // Try free list first
      Pointer mem = nullptr;
#if USE_FREE_LIST
      mem = allocateUsingFreeList(alloc_size, align);
      if (mem != nullptr)
      {
        return mem;
      }
#endif
      // Try current block
      mem = Blocks_.back().allocate(alloc_size, align);
      if (mem != nullptr)
      {
        return mem;
      }
    }
  }

  // Need a new block
  SizeType new_block_size = std::max(alloc_size, NextBlockSize_.load(std::memory_order_relaxed));
  if (allocateBlock(new_block_size, align) == nullptr)
  {
    if (DebugFeatures_.load(std::memory_order_relaxed))
    {
      std::cerr << "-- Failed to allocate block : ArenaAllocator::allocate_block()" << std::endl;
    }
    return nullptr;
  }

  updateNextBlockSize();
  std::shared_lock<std::shared_mutex> lock(BlocksMutex_);
  return Blocks_.back().allocate(alloc_size, align);
}

void ArenaAllocator::updateNextBlockSize() MYLANG_NOEXCEPT
{
  /// TODO: prevent overflow
  if (GrowthFactor_.load(std::memory_order_relaxed) == GrowthStrategy::EXPONENTIAL)
  {
    SizeType current  = NextBlockSize_.load(std::memory_order_relaxed);
    SizeType max_size = MaxBlockSize_.load(std::memory_order_relaxed);
    SizeType new_size = std::min(current * 2, max_size);
    NextBlockSize_.store(new_size, std::memory_order_relaxed);
  }
}

}
}
}