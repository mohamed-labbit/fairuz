#include "../../../include/runtime/allocator/arena.hpp"


namespace mylang {
namespace runtime {
namespace allocator {

ArenaAllocator::ArenaAllocator(std::int32_t growth_strategy, OutOfMemoryHandler oom_handler, bool debug) :
    GrowthFactor_(GrowthStrategy(growth_strategy)),
    OomHandler_(oom_handler),
    DebugFeatures_(debug)
{
  // Enable tracking in debug mode
  if (DebugFeatures_)
  {
    TrackAllocations_ = true;
    EnableStatistics_ = true;
    AllocStats_.reset();
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

  if (TrackAllocations_)
    AllocationMap_.clear();

  AllocatedPtrs_.clear();
  // Reset statistics
  AllocStats_.reset();
  // Reset block size
  NextBlockSize_ = BlockSize_;
}

MYLANG_NODISCARD
Pointer ArenaAllocator::allocateBlock(SizeType requested, SizeType alignment_, bool retry_on_oom)
{
  // Validate and fix alignment if needed
  SizeType min_align = alignof(std::max_align_t);
  if (alignment_ != ((requested + min_align - 1) & ~(min_align - 1)))
    alignment_ = min_align;

  // Determine block size (including growth strategy)
  SizeType block_size = std::max(requested + alignment_, NextBlockSize_);

  // Check against maximum
  if (block_size > MaxBlockSize_)
  {
    if (retry_on_oom)
    {
      std::lock_guard<std::mutex> oom_lock(OomHandlerMutex_);
      if (OomHandler_ && OomHandler_(block_size))
        return allocateBlock(requested, alignment_, false);  // Retry once
    }
    return nullptr;
  }

  try
  {
    // Add new block to vector (requires exclusive lock)
    std::unique_lock<std::shared_mutex> lock(BlocksMutex_);
    Blocks_.emplace_back(block_size, alignment_);
    AllocStats_.ActiveBlocks++;
    return Blocks_.back().begin();
  } catch (const std::bad_alloc&)
  {
    if (retry_on_oom)
    {
      std::lock_guard<std::mutex> oom_lock(OomHandlerMutex_);
      if (OomHandler_ && OomHandler_(block_size))
        return allocateBlock(requested, alignment_, false);  // Retry once
    }
    return nullptr;
  }
}

MYLANG_NODISCARD MYLANG_COMPILER_ABI void* ArenaAllocator::allocate(const SizeType size, const SizeType alignment)
{
  std::chrono::steady_clock::time_point start = std::chrono::high_resolution_clock::now();

  // Validate count
  if (size == 0)
    return nullptr;

  // Check size
  if (size > MAX_BLOCK_SIZE)
  {
    diagnostic::engine.emit("allocation size is too large : " + std::to_string(size),
                            diagnostic::DiagnosticEngine::Severity::ERROR);
    if (OomHandler_ && OomHandler_(size))
      return allocate(size);

    diagnostic::engine.emit("bad alloc!", diagnostic::DiagnosticEngine::Severity::FATAL);
    /// TODO: change after debug
    // diagnostic::engine.panic("bad alloc");
  }

  /// TODO: validate alignment

  SizeType aligned_size = getAligned(size, alignment);

  if (aligned_size > MAX_BLOCK_SIZE)
  {
    std::cerr << "allocation size (after alignment) is too large!" << std::endl;
    return nullptr;
  }

  Pointer mem = allocateFromBlocks(aligned_size);
  if (!mem)
  {
    std::cerr << "allocate_from_blocks() failed!" << std::endl;
    return nullptr;
  }

  // _Tp* region = reinterpret_cast<_Tp*>(mem);


  // Track allocation if enabled
  if (TrackAllocations_)
  {
    AllocationHeader header{};
    header.magic     = AllocationHeader::MAGIC;
    header.size      = static_cast<std::uint32_t>(aligned_size);
    header.alignment = static_cast<std::uint32_t>(alignment);
    header.checksum  = header.compute_checksum();
    header.timestamp = std::chrono::steady_clock::now();
    std::unique_lock<std::shared_mutex> lock(AllocationMapMutex_);
    AllocationMap_[mem] = header;
  }

  // Track Pointer for double-free protection
  std::unique_lock<std::shared_mutex> lock(AllocatedPtrsMutex_);
  AllocatedPtrs_.insert(mem);

  // Construct objects if needed

  std::chrono::steady_clock::time_point end      = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double>         duration = end - start;

  LastPtr_ = static_cast<void*>(mem);

  // Update statistics
  if (EnableStatistics_)
  {
    AllocStats_.TotalAllocations++;
    AllocStats_.TotalAllocated += aligned_size;
    AllocStats_.CurrentlyAllocated += aligned_size;
    AllocStats_.recordAllocationSize(aligned_size);
    AllocStats_.updatePeak(aligned_size);
    AllocStats_.recordAllocTime(duration.count() * 1000000);
  }

  return LastPtr_;
}

MYLANG_COMPILER_ABI
void ArenaAllocator::deallocate(void* ptr, const SizeType size)
{
  auto start = std::chrono::high_resolution_clock::now();
  if (!ptr || size == 0)
    return;
  // Strict LIFO check
  Pointer expected = static_cast<Pointer>(ptr);
  Pointer last     = static_cast<Pointer>(LastPtr_);
  if (expected != last)
    return;
  // We MUST operate on the last block
  std::unique_lock<std::shared_mutex> lock(BlocksMutex_);
  if (Blocks_.empty())
    return;
  ArenaBlock& block = Blocks_.back();
  // CAS-based pop (must be safe)
  bool ok = block.pop(size);
  /// TODO: implement safe decrement
  AllocStats_.CurrentlyAllocated -= size;
  AllocStats_.TotalDeallocated -= size;
  ++AllocStats_.TotalDeallocations;
  if (block.cNext() == block.begin())
    Blocks_.back();
  /// TODO: implement safe decrement
  if (AllocStats_.ActiveBlocks)
    AllocStats_.ActiveBlocks--;
  // Clear LastPtr_ (important!)
  LastPtr_                               = nullptr;
  auto                          end      = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end - start;
  double                        seconds  = duration.count();
  AllocStats_.TotalDeallocTimeNs += seconds * 1000000;
}

MYLANG_NODISCARD
bool ArenaAllocator::verifyAllocation(void* ptr) const
{
  if (!TrackAllocations_)
    return true;

  std::shared_lock<std::shared_mutex> lock(AllocationMapMutex_);
  auto                                it = AllocationMap_.find(ptr);
  if (it == AllocationMap_.end())
    return false;

  return it->second.is_valid();
}

MYLANG_NODISCARD
Pointer ArenaAllocator::allocateFromBlocks(SizeType alloc_size, SizeType align)
{
  {
    std::shared_lock<std::shared_mutex> lock(BlocksMutex_);
    if (!Blocks_.empty())
    {
      // Try free list first
      Pointer mem = Blocks_.back().allocate(alloc_size, align);
      if (mem)
        return mem;
    }
  }

  // Need a new block
  SizeType new_block_size = std::max(alloc_size, NextBlockSize_);
  if (!allocateBlock(new_block_size, align))
  {
    if (DebugFeatures_)
      std::cerr << "-- Failed to allocate block : ArenaAllocator::allocate_block()" << std::endl;
    return nullptr;
  }

  updateNextBlockSize();
  std::shared_lock<std::shared_mutex> lock(BlocksMutex_);
  return Blocks_.back().allocate(alloc_size, align);
}

void ArenaAllocator::updateNextBlockSize() MYLANG_NOEXCEPT
{
  /// TODO: prevent overflow
  if (GrowthFactor_ == GrowthStrategy::EXPONENTIAL)
  {
    SizeType current  = NextBlockSize_;
    SizeType max_size = MaxBlockSize_;
    NextBlockSize_    = std::min(current * 2, max_size);
  }
}

}
}
}