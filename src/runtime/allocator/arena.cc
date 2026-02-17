#include "../../../include/runtime/allocator/arena.hpp"

namespace mylang {
namespace runtime {
namespace allocator {

ArenaAllocator::ArenaAllocator(std::int32_t growth_strategy, OutOfMemoryHandler oom_handler, bool debug)
    : GrowthFactor_(GrowthStrategy(growth_strategy))
    , OomHandler_(oom_handler)
    , DebugFeatures_(debug)
{
    // Enable tracking in debug mode
    if (DebugFeatures_) {
        TrackAllocations_ = true;
        EnableStatistics_ = true;
        AllocStats_.reset();
    }
}

void ArenaAllocator::reset()
{
    // Acquire all locks in consistent order
    std::unique_lock blocks_lock(BlocksMutex_);
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

unsigned char* ArenaAllocator::allocateBlock(std::size_t requested, std::size_t alignment_, bool retry_on_oom)
{
    // Validate and fix alignment if needed
    std::size_t min_align = alignof(std::max_align_t);
    if (alignment_ != ((requested + min_align - 1) & ~(min_align - 1)))
        alignment_ = min_align;

    // Determine block size (including growth strategy)
    std::size_t block_size = std::max(requested + alignment_, NextBlockSize_);

    // Check against maximum
    if (block_size > MaxBlockSize_) {
        if (retry_on_oom) {
            std::lock_guard<std::mutex> oom_lock(OomHandlerMutex_);
            if (OomHandler_ && OomHandler_(block_size))
                return allocateBlock(requested, alignment_, false); // Retry once
        }
        return nullptr;
    }

    try {
        // Add new block to vector (requires exclusive lock)
        std::unique_lock<std::shared_mutex> lock(BlocksMutex_);
        Blocks_.emplace_back(block_size, alignment_);
        ++AllocStats_.ActiveBlocks;
        return Blocks_.back().begin();
    } catch (std::bad_alloc const&) {
        if (retry_on_oom) {
            std::lock_guard<std::mutex> oom_lock(OomHandlerMutex_);
            if (OomHandler_ && OomHandler_(block_size))
                return allocateBlock(requested, alignment_, false); // Retry once
        }
        return nullptr;
    }
}

void* ArenaAllocator::allocate(std::size_t const size, std::size_t const alignment)
{
    std::chrono::steady_clock::time_point start = std::chrono::high_resolution_clock::now();

    // Validate count
    if (size == 0)
        return nullptr;

    // Check size
    if (size > MAX_BLOCK_SIZE) {
        diagnostic::engine.emit("allocation size is too large : " + std::to_string(size), diagnostic::DiagnosticEngine::Severity::ERROR);
        if (OomHandler_ && OomHandler_(size))
            return allocate(size);

        diagnostic::engine.emit("bad alloc!", diagnostic::DiagnosticEngine::Severity::FATAL);
        /// TODO: change after debug
        // diagnostic::engine.panic("bad alloc");
    }

    /// TODO: validate alignment

    std::size_t aligned_size = getAligned(size, alignment);

    if (aligned_size > MAX_BLOCK_SIZE) {
        std::cerr << "allocation size (after alignment) is too large!" << std::endl;
        return nullptr;
    }

    unsigned char* mem = allocateFromBlocks(aligned_size);
    if (!mem) {
        std::cerr << "allocate_from_blocks() failed!" << std::endl;
        return nullptr;
    }

    // Track allocation if enabled
    if (TrackAllocations_) {
        AllocationHeader header {};
        header.magic = AllocationHeader::MAGIC;
        header.size = static_cast<std::uint32_t>(aligned_size);
        header.alignment = static_cast<std::uint32_t>(alignment);
        header.checksum = header.compute_checksum();
        header.timestamp = std::chrono::steady_clock::now();
        std::unique_lock<std::shared_mutex> lock(AllocationMapMutex_);
        AllocationMap_[mem] = header;
    }

    // Track Pointer for double-free protection
    std::unique_lock<std::shared_mutex> lock(AllocatedPtrsMutex_);
    AllocatedPtrs_.insert(mem);

    // Construct objects if needed

    std::chrono::steady_clock::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    LastPtr_ = static_cast<void*>(mem);

    // Update statistics
    if (EnableStatistics_) {
        AllocStats_.TotalAllocations++;
        AllocStats_.TotalAllocated += aligned_size;
        AllocStats_.CurrentlyAllocated += aligned_size;
        AllocStats_.recordAllocationSize(aligned_size);
        AllocStats_.updatePeak(aligned_size);
        AllocStats_.recordAllocTime(duration.count() * 1000000);
    }

    return LastPtr_;
}

void ArenaAllocator::deallocate(void* ptr, std::size_t const size)
{
    auto start = std::chrono::high_resolution_clock::now();
    if (!ptr || size == 0 || Blocks_.empty())
        return;

    // Strict LIFO check
    unsigned char* expected = static_cast<unsigned char*>(ptr);
    unsigned char* last = static_cast<unsigned char*>(LastPtr_);

    if (expected != last)
        return;

    // We MUST operate on the last block
    std::unique_lock<std::shared_mutex> lock(BlocksMutex_);

    ArenaBlock& block = Blocks_.back();
    // CAS-based pop (must be safe)
    bool ok = block.pop(size);
    /// TODO: implement safe decrement

    AllocStats_.CurrentlyAllocated -= size;
    AllocStats_.TotalDeallocated -= size;
    ++AllocStats_.TotalDeallocations;

    if (block.cNext() == block.begin())
        Blocks_.back();

    if (AllocStats_.ActiveBlocks)
        --AllocStats_.ActiveBlocks;

    // Clear LastPtr_ (important!)
    LastPtr_ = nullptr;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    double seconds = duration.count();
    AllocStats_.TotalDeallocTimeNs += seconds * 1000000;
}

bool ArenaAllocator::verifyAllocation(void* ptr) const
{
    if (!TrackAllocations_)
        return true;

    std::shared_lock<std::shared_mutex> lock(AllocationMapMutex_);
    auto it = AllocationMap_.find(ptr);
    if (it == AllocationMap_.end())
        return false;

    return it->second.is_valid();
}

unsigned char* ArenaAllocator::allocateFromBlocks(std::size_t alloc_size, std::size_t align)
{
    {
        std::shared_lock<std::shared_mutex> lock(BlocksMutex_);
        if (!Blocks_.empty()) {
            // Try free list first
            unsigned char* mem = Blocks_.back().allocate(alloc_size, align);
            if (mem)
                return mem;
        }
    } // release lock

    // Need a new block
    std::size_t new_block_size = std::max(alloc_size, NextBlockSize_);
    if (!allocateBlock(new_block_size, align)) {
        if (DebugFeatures_)
            std::cerr << "-- Failed to allocate block : ArenaAllocator::allocate_block()" << std::endl;

        return nullptr;
    }

    updateNextBlockSize();
    std::shared_lock<std::shared_mutex> lock(BlocksMutex_);
    return Blocks_.back().allocate(alloc_size, align);
}

void ArenaAllocator::updateNextBlockSize() noexcept
{
    /// TODO: prevent overflow
    if (GrowthFactor_ == GrowthStrategy::EXPONENTIAL) {
        std::size_t current = NextBlockSize_;
        std::size_t max_size = MaxBlockSize_;
        NextBlockSize_ = std::min(current * 2, max_size);
    }
}

}
}
}
