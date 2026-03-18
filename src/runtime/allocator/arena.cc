#include "../../../include/runtime/allocator/arena.hpp"

namespace mylang {
namespace runtime {
namespace allocator {

using ErrorCode = diagnostic::errc::general::Code;

ArenaBlock::ArenaBlock(size_t const size, size_t const alignment)
    : Size_(size)
{
    // Validate alignment is a power of 2
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        diagnostic::emit("Alignment must be a power of two");
        diagnostic::internalError(ErrorCode::INTERNAL_ERROR);
    }

    // Round up size to multiple of alignment
    size_t mod = Size_ % alignment;
    if (mod)
        Size_ += (alignment - mod);

    // Allocate aligned memory
    void* mem = std::aligned_alloc(alignment, Size_);
    if (!mem)
        diagnostic::internalError(ErrorCode::ALLOC_FAILED);

    /// TODO: change after debug
    // diagnostic::panic("bad alloc");
    Begin_ = reinterpret_cast<unsigned char*>(mem);
    Next_ = Begin_;
}

ArenaBlock::ArenaBlock(ArenaBlock&& other) noexcept
{
    Size_ = other.Size_;
    Begin_ = other.Begin_;
    Next_ = other.Next_;

    // Reset source
    other.Begin_ = nullptr;
    other.Next_ = nullptr;
    other.Size_ = 0;
}

ArenaBlock::~ArenaBlock()
{
    size_t number_of_frees = 0;

    if (Begin_) {
        std::free(Begin_);
        Begin_ = nullptr;
        Next_ = nullptr;
    }
}

ArenaBlock& ArenaBlock::operator=(ArenaBlock&& other) noexcept
{
    if (this != &other) {
        // Free existing memory
        if (Begin_)
            std::free(Begin_);

        // Take ownership
        Size_ = other.Size_;
        Begin_ = other.Begin_;
        Next_ = other.Next_;

        // Reset source
        other.Begin_ = nullptr;
        other.Next_ = nullptr;
        other.Size_ = 0;
    }
    return *this;
}

unsigned char* ArenaBlock::begin() const { return Begin_; }

unsigned char* ArenaBlock::end() const { return Begin_ + Size_; }

unsigned char* ArenaBlock::cNext() const { return Next_; }

size_t ArenaBlock::size() const { return Size_; }

size_t ArenaBlock::used() const
{
    if (!Begin_ || Next_ < Begin_)
        return 0;

    return static_cast<size_t>(Next_ - Begin_);
}

bool ArenaBlock::pop(size_t bytes)
{
    if (!Begin_ || Next_ < Begin_ + bytes)
        return false;

    Next_ -= bytes;
    return true;
}

size_t ArenaBlock::remaining() const
{
    if (!Begin_)
        return 0;

    unsigned char const* current_next = Next_;
    return static_cast<size_t>(Begin_ + Size_ - current_next);
}

unsigned char* ArenaBlock::allocate(size_t bytes, std::optional<size_t> alignment)
{
    if (!Begin_ || bytes == 0)
        return nullptr;

    size_t alignment_value = alignment.value_or(alignof(std::max_align_t));
    // Validate alignment is a power of 2
    if (alignment_value == 0 || (alignment_value & (alignment_value - 1)) != 0) {
        diagnostic::emit("Invalid arguments to ArenaAllocator::allocate()");
        diagnostic::internalError(ErrorCode::INTERNAL_ERROR);
    }

    // Calculate aligned address
    uintptr_t cur = reinterpret_cast<uintptr_t>(Next_);
    uintptr_t aligned = (cur + (alignment_value - 1)) & ~(alignment_value - 1);
    size_t pad = aligned - cur;

    // Check if we have enough space (including padding)
    uintptr_t end_addr = reinterpret_cast<uintptr_t>(Begin_) + Size_;
    uintptr_t cur_addr = reinterpret_cast<uintptr_t>(Next_);

    if (cur_addr > end_addr)
        return nullptr; // Current pointer is beyond block end

    size_t remaining = end_addr - cur_addr;

    if (remaining < bytes + pad)
        return nullptr; // Not enough space

    unsigned char* new_next = reinterpret_cast<unsigned char*>(aligned + bytes);
    // Try to atomically update next unsigned char*
    Next_ = new_next;
    // Success! Return the aligned address
    return reinterpret_cast<unsigned char*>(aligned);
    // CAS failed - another thread allocated first
}

unsigned char* ArenaBlock::reserve(size_t const bytes)
{
    if (!Begin_ || bytes == 0)
        return nullptr;

    // Check if we have enough space
    size_t remaining = Begin_ + Size_ - Next_;
    if (remaining < bytes)
        return nullptr;

    Next_ += bytes;
    return Next_;
}

ArenaAllocator::ArenaAllocator(std::int32_t growth_strategy, OutOfMemoryHandler oom_handler, bool debug)
    : GrowthFactor_(GrowthStrategy(growth_strategy))
    , OomHandler_(oom_handler)
#ifdef MYLANG_DEBUG
    , DebugFeatures_(debug)
#endif // MYLANG_DEBUG
{
#ifdef MYLANG_DEBUG
    // Enable tracking in debug mode
    if (DebugFeatures_) {
        TrackAllocations_ = true;
        EnableStatistics_ = true;
        AllocStats_.reset();
    }
#endif // MYLANG_DEBUG
}

void ArenaAllocator::reset()
{
    // Clear all containers (automatically frees memory via destructors)
    Blocks_.clear();

#ifdef MYLANG_DEBUG
    if (TrackAllocations_)
        AllocationMap_.clear();

    AllocatedPtrs_.clear();
    // Reset statistics
    AllocStats_.reset();
#endif // MYLANG_DEBUG
    // Reset block size
    NextBlockSize_ = BlockSize_;
}

void reset();

void ArenaAllocator::setName(std::string const& name) { Name_ = name; }

#ifdef MYLANG_DEBUG
size_t ArenaAllocator::totalAllocated() const { return AllocStats_.TotalAllocated; }

size_t ArenaAllocator::totalAllocations() const { return AllocStats_.TotalAllocations; }

size_t ArenaAllocator::activeBlocks() const { return AllocStats_.ActiveBlocks; }
#endif // MYLANG_DEBUG

unsigned char* ArenaAllocator::allocateBlock(size_t requested, size_t alignment_, bool retry_on_oom)
{
    // Validate and fix alignment if needed
    size_t min_align = alignof(std::max_align_t);
    if (alignment_ != ((requested + min_align - 1) & ~(min_align - 1)))
        alignment_ = min_align;

    // Determine block size (including growth strategy)
    size_t block_size = std::max(requested + alignment_, NextBlockSize_);

    // Check against maximum
    if (block_size > MaxBlockSize_) {
        if (retry_on_oom) {
            if (OomHandler_ && OomHandler_(block_size))
                return allocateBlock(requested, alignment_, false); // Retry once
        }

        return nullptr;
    }

    try {
        // Add new block to vector (requires exclusive lock)
        Blocks_.emplace_back(block_size, alignment_);
#ifdef MYLANG_DEBUG
        ++AllocStats_.ActiveBlocks;
#endif
        return Blocks_.back().begin();
    } catch (std::bad_alloc const&) {
        if (retry_on_oom) {
            if (OomHandler_ && OomHandler_(block_size))
                return allocateBlock(requested, alignment_, false); // Retry once
        }

        return nullptr;
    }
}

void* ArenaAllocator::allocate(size_t const size, size_t const alignment)
{
#ifdef MYLANG_DEBUG
    std::chrono::steady_clock::time_point start = std::chrono::high_resolution_clock::now();
#endif
    // Validate count
    if (size == 0)
        return nullptr;

    // Check size
    if (size > MAX_BLOCK_SIZE) {
        diagnostic::emit("allocation size is too large : " + std::to_string(size));
        diagnostic::internalError(ErrorCode::ALLOC_FAILED);
        if (OomHandler_ && OomHandler_(size))
            return allocate(size);

        diagnostic::internalError(ErrorCode::ALLOC_FAILED);
        /// TODO: change after debug
        // diagnostic::panic("bad alloc");
    }

    /// TODO: validate alignment

    size_t aligned_size = getAligned(size, alignment);

    if (aligned_size > MAX_BLOCK_SIZE) {
        std::cerr << "allocation size (after alignment) is too large!" << std::endl;
        return nullptr;
    }

    unsigned char* mem = allocateFromBlocks(aligned_size);
    if (!mem) {
        std::cerr << "allocate_from_blocks() failed!" << std::endl;
        return nullptr;
    }
    LastPtr_ = static_cast<void*>(mem);

    // Track allocation if enabled
#ifdef MYLANG_DEBUG
    if (TrackAllocations_) {
        AllocationHeader header { };
        header.magic = AllocationHeader::MAGIC;
        header.size = static_cast<std::uint32_t>(aligned_size);
        header.alignment = static_cast<std::uint32_t>(alignment);
        header.checksum = header.compute_checksum();
        header.timestamp = std::chrono::steady_clock::now();
        AllocationMap_[mem] = header;
    }

    // Track Pointer for double-free protection
    AllocatedPtrs_.insert(mem);

    // Construct objects if needed

    std::chrono::steady_clock::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    // Update statistics
    if (EnableStatistics_) {
        AllocStats_.TotalAllocations++;
        AllocStats_.TotalAllocated += aligned_size;
        AllocStats_.CurrentlyAllocated += aligned_size;
        AllocStats_.recordAllocationSize(aligned_size);
        AllocStats_.updatePeak(aligned_size);
        AllocStats_.recordAllocTime(duration.count() * 1000000);
    }
#endif // MYLANG_DEBUG

    return LastPtr_;
}

void ArenaAllocator::deallocate(void* ptr, size_t const size)
{
#ifdef MYLANG_DEBUG
    auto start = std::chrono::high_resolution_clock::now();
#endif
    if (!ptr || size == 0 || Blocks_.empty())
        return;

    // Strict LIFO check
    auto* expected = static_cast<unsigned char*>(ptr);
    auto* last = static_cast<unsigned char*>(LastPtr_);

    if (expected != last)
        return;

    ArenaBlock& block = Blocks_.back();
    // CAS-based pop (must be safe)
    bool ok = block.pop(size);
    LastPtr_ = nullptr;

#ifdef MYLANG_DEBUG
    AllocStats_.CurrentlyAllocated -= size;
    AllocStats_.TotalDeallocated -= size;
    ++AllocStats_.TotalDeallocations;
    /*
    if (block.cNext() == block.begin())
        Blocks_.back();
    */

    if (AllocStats_.ActiveBlocks)
        --AllocStats_.ActiveBlocks;

    // Clear LastPtr_ (important!)
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    double seconds = duration.count();
    AllocStats_.TotalDeallocTimeNs += seconds * 1000000;
#endif
}

#ifdef MYLANG_DEBUG
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

std::string ArenaAllocator::toString(bool verbose) const
{
    std::ostringstream oss;
    dumpStats(oss, verbose);
    return oss.str();
}

void ArenaAllocator::dumpStats(std::ostream& os, bool verbose) const
{
    StatsPrinter printer(AllocStats_, Name_);
    printer.printDetailed(os, verbose);
}
#endif

unsigned char* ArenaAllocator::allocateFromBlocks(size_t alloc_size, size_t align)
{
    {
        if (!Blocks_.empty()) {
            // Try free list first
            unsigned char* mem = Blocks_.back().allocate(alloc_size, align);
            if (mem)
                return mem;
        }
    } // release lock

    // Need a new block
    size_t new_block_size = std::max(alloc_size, NextBlockSize_);
    if (!allocateBlock(new_block_size, align)) {
#ifdef MYLANG_DEBUG
        if (DebugFeatures_)
            std::cerr << "-- Failed to allocate block : ArenaAllocator::allocate_block()" << std::endl;
#endif
        return nullptr;
    }

    updateNextBlockSize();
    return Blocks_.back().allocate(alloc_size, align);
}

void ArenaAllocator::updateNextBlockSize() noexcept
{
    /// TODO: prevent overflow
    if (GrowthFactor_ == GrowthStrategy::EXPONENTIAL) {
        size_t current = NextBlockSize_;
        size_t max_size = MaxBlockSize_;
        NextBlockSize_ = std::min(current * 2, max_size);
    }
}

}
}
}
