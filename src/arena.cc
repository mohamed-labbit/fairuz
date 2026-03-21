// arena.cc

#include "../include/arena.hpp"

namespace mylang {

using ErrorCode = diagnostic::errc::general::Code;

ArenaBlock::ArenaBlock(size_t const size, size_t const alignment) : Size_(size) {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    diagnostic::emit("Alignment must be a power of two");
    diagnostic::internalError(ErrorCode::INTERNAL_ERROR);
  }

  size_t mod = Size_ % alignment;
  if (mod)
    Size_ += (alignment - mod);

  void *mem = std::aligned_alloc(alignment, Size_);
  if (!mem)
    diagnostic::internalError(ErrorCode::ALLOC_FAILED);

  Begin_ = reinterpret_cast<unsigned char *>(mem);
  Next_ = Begin_;
  End_ = Begin_ + Size_;
}

ArenaBlock::ArenaBlock(ArenaBlock &&other) noexcept : Size_(other.Size_), Begin_(other.Begin_), Next_(other.Next_), End_(other.End_) {
  other.Begin_ = nullptr;
  other.Next_ = nullptr;
  other.End_ = nullptr;
  other.Size_ = 0;
}

ArenaBlock::~ArenaBlock() {
  if (Begin_) {
    std::free(Begin_);
    Begin_ = nullptr;
    Next_ = nullptr;
    End_ = nullptr;
  }
}

ArenaBlock &ArenaBlock::operator=(ArenaBlock &&other) noexcept {
  if (this != &other) {
    if (Begin_)
      std::free(Begin_);

    Size_ = other.Size_;
    Begin_ = other.Begin_;
    Next_ = other.Next_;
    End_ = other.End_;

    other.Begin_ = nullptr;
    other.Next_ = nullptr;
    other.End_ = nullptr;
    other.Size_ = 0;
  }
  return *this;
}

unsigned char *ArenaBlock::begin() const { return Begin_; }
unsigned char *ArenaBlock::end() const { return End_; }
unsigned char *ArenaBlock::cNext() const { return Next_; }
size_t ArenaBlock::size() const { return Size_; }

size_t ArenaBlock::used() const {
  if (!Begin_ || Next_ < Begin_)
    return 0;
  return static_cast<size_t>(Next_ - Begin_);
}

size_t ArenaBlock::remaining() const {
  if (!Begin_)
    return 0;
  return static_cast<size_t>(End_ - Next_);
}

bool ArenaBlock::pop(size_t bytes) {
  if (!Begin_ || Next_ < Begin_ + bytes)
    return false;
  Next_ -= bytes;
  return true;
}

unsigned char *ArenaBlock::allocate(size_t bytes, std::optional<size_t> alignment) {
  if (!Begin_ || bytes == 0)
    return nullptr;

  size_t align = alignment.value_or(alignof(std::max_align_t));

  uintptr_t cur = reinterpret_cast<uintptr_t>(Next_);
  uintptr_t aligned = (cur + align - 1) & ~(align - 1);
  uintptr_t next = aligned + bytes;

  if (__builtin_expect(next > reinterpret_cast<uintptr_t>(End_), 0))
    return nullptr;

  Next_ = reinterpret_cast<unsigned char *>(next);
  return reinterpret_cast<unsigned char *>(aligned);
}

unsigned char *ArenaBlock::reserve(size_t const bytes) {
  if (!Begin_ || bytes == 0)
    return nullptr;
  if (static_cast<size_t>(End_ - Next_) < bytes)
    return nullptr;
  Next_ += bytes;
  return Next_;
}

ArenaAllocator::ArenaAllocator(std::int32_t growth_strategy, OutOfMemoryHandler oom_handler, bool debug)
    : GrowthFactor_(GrowthStrategy(growth_strategy)), OomHandler_(oom_handler)
#ifdef MYLANG_DEBUG
      ,
      DebugFeatures_(debug)
#endif
{
#ifdef MYLANG_DEBUG
  if (DebugFeatures_) {
    TrackAllocations_ = true;
    EnableStatistics_ = true;
    AllocStats_.reset();
  }
#endif
}

void ArenaAllocator::reset() {
  Blocks_.clear();
  Next_ = nullptr;
  End_ = nullptr;

#ifdef MYLANG_DEBUG
  if (TrackAllocations_)
    AllocationMap_.clear();
  AllocatedPtrs_.clear();
  AllocStats_.reset();
#endif

  NextBlockSize_ = BlockSize_;
}

void ArenaAllocator::setName(std::string const &name) { Name_ = name; }

#ifdef MYLANG_DEBUG
size_t ArenaAllocator::totalAllocated() const { return AllocStats_.TotalAllocated; }
size_t ArenaAllocator::totalAllocations() const { return AllocStats_.TotalAllocations; }
size_t ArenaAllocator::activeBlocks() const { return AllocStats_.ActiveBlocks; }
#endif

void *ArenaAllocator::allocate(size_t const size, size_t const alignment) {
  if (__builtin_expect(size == 0, 0))
    return nullptr;

  if (__builtin_expect(size > MAX_BLOCK_SIZE, 0)) {
    diagnostic::emit("allocation size is too large : " + std::to_string(size));
    diagnostic::internalError(ErrorCode::ALLOC_FAILED);
  }

  uintptr_t cur = reinterpret_cast<uintptr_t>(Next_);
  uintptr_t aligned = (cur + alignment - 1) & ~(alignment - 1);
  uintptr_t next = aligned + size;

  if (__builtin_expect(next <= reinterpret_cast<uintptr_t>(End_), 1)) {
    Next_ = reinterpret_cast<unsigned char *>(next);

#ifdef MYLANG_DEBUG
    trackAllocation(reinterpret_cast<unsigned char *>(aligned), size, alignment);
#endif
    return reinterpret_cast<void *>(aligned);
  }

  return allocateSlow(size, alignment);
}

void *ArenaAllocator::allocateSlow(size_t size, size_t alignment) {
  size_t block_size = std::max(size + alignment, NextBlockSize_);

  if (block_size > MaxBlockSize_) {
    if (OomHandler_ && OomHandler_(block_size)) {
      // OOM handler freed something — retry once
      block_size = std::max(size + alignment, NextBlockSize_);
      if (block_size > MaxBlockSize_)
        return nullptr;
    } else {
      return nullptr;
    }
  }

  try {
    Blocks_.emplace_back(block_size, alignment);
#ifdef MYLANG_DEBUG
    ++AllocStats_.ActiveBlocks;
#endif
  } catch (std::bad_alloc const &) {
    if (OomHandler_ && OomHandler_(block_size)) {
      try {
        Blocks_.emplace_back(block_size, alignment);
      } catch (...) {
        return nullptr;
      }
    } else {
      return nullptr;
    }
  }

  updateNextBlockSize();

  ArenaBlock &blk = Blocks_.back();
  Next_ = blk.begin();
  End_ = blk.end();

  uintptr_t cur = reinterpret_cast<uintptr_t>(Next_);
  uintptr_t aligned = (cur + alignment - 1) & ~(alignment - 1);
  uintptr_t next = aligned + size;

  if (__builtin_expect(next > reinterpret_cast<uintptr_t>(End_), 0))
    return nullptr;

  Next_ = reinterpret_cast<unsigned char *>(next);

#ifdef MYLANG_DEBUG
  trackAllocation(reinterpret_cast<unsigned char *>(aligned), size, alignment);
#endif
  return reinterpret_cast<void *>(aligned);
}

void ArenaAllocator::deallocate(void *ptr, size_t const size) {
  if (!ptr || size == 0 || Blocks_.empty())
    return;

  auto *expected = static_cast<unsigned char *>(ptr);
  auto *last = static_cast<unsigned char *>(LastPtr_);

  if (expected != last)
    return;

  ArenaBlock &block = Blocks_.back();
  if (block.pop(size)) {
    Next_ = block.cNext();
    LastPtr_ = nullptr;
  }

#ifdef MYLANG_DEBUG
  if (AllocStats_.CurrentlyAllocated >= size)
    AllocStats_.CurrentlyAllocated -= size;
  ++AllocStats_.TotalDeallocations;
  if (AllocStats_.ActiveBlocks)
    --AllocStats_.ActiveBlocks;
#endif
}

unsigned char *ArenaAllocator::allocateBlock(size_t requested, size_t alignment_, bool retry_on_oom) {
  size_t block_size = std::max(requested + alignment_, NextBlockSize_);

  if (block_size > MaxBlockSize_) {
    if (retry_on_oom && OomHandler_ && OomHandler_(block_size))
      return allocateBlock(requested, alignment_, false);
    return nullptr;
  }

  try {
    Blocks_.emplace_back(block_size, alignment_);
#ifdef MYLANG_DEBUG
    ++AllocStats_.ActiveBlocks;
#endif
    ArenaBlock &blk = Blocks_.back();
    Next_ = blk.begin();
    End_ = blk.end();
    return blk.begin();
  } catch (std::bad_alloc const &) {
    if (retry_on_oom && OomHandler_ && OomHandler_(block_size))
      return allocateBlock(requested, alignment_, false);
    return nullptr;
  }
}

unsigned char *ArenaAllocator::allocateFromBlocks(size_t alloc_size, size_t align) {
  if (!Blocks_.empty()) {
    unsigned char *mem = Blocks_.back().allocate(alloc_size, align);
    if (mem) {
      Next_ = Blocks_.back().cNext();
      return mem;
    }
  }

  size_t new_block_size = std::max(alloc_size, NextBlockSize_);
  if (!allocateBlock(new_block_size, align)) {
#ifdef MYLANG_DEBUG
    if (DebugFeatures_)
      std::cerr << "-- Failed to allocate block : ArenaAllocator::allocate_block()" << std::endl;
#endif
    return nullptr;
  }

  updateNextBlockSize();
  unsigned char *mem = Blocks_.back().allocate(alloc_size, align);
  if (mem)
    Next_ = Blocks_.back().cNext();
  return mem;
}

void ArenaAllocator::updateNextBlockSize() noexcept {
  if (GrowthFactor_ == GrowthStrategy::EXPONENTIAL)
    NextBlockSize_ = std::min(NextBlockSize_ * 2, MaxBlockSize_);
}

#ifdef MYLANG_DEBUG
void ArenaAllocator::trackAllocation(unsigned char *ptr, size_t size, size_t alignment) {
  LastPtr_ = ptr;

  if (TrackAllocations_) {
    AllocationHeader header{};
    header.magic = AllocationHeader::MAGIC;
    header.size = static_cast<std::uint32_t>(size);
    header.alignment = static_cast<std::uint32_t>(alignment);
    header.checksum = header.compute_checksum();
    header.timestamp = std::chrono::steady_clock::now();
    AllocationMap_[ptr] = header;
    AllocatedPtrs_.insert(ptr);
  }

  if (EnableStatistics_) {
    AllocStats_.TotalAllocations++;
    AllocStats_.TotalAllocated += size;
    AllocStats_.CurrentlyAllocated += size;
    AllocStats_.recordAllocationSize(size);
    AllocStats_.updatePeak(size);
  }
}

bool ArenaAllocator::verifyAllocation(void *ptr) const {
  if (!TrackAllocations_)
    return true;
  std::shared_lock<std::shared_mutex> lock(AllocationMapMutex_);
  auto it = AllocationMap_.find(ptr);
  if (it == AllocationMap_.end())
    return false;
  return it->second.is_valid();
}

std::string ArenaAllocator::toString(bool verbose) const {
  std::ostringstream oss;
  dumpStats(oss, verbose);
  return oss.str();
}

void ArenaAllocator::dumpStats(std::ostream &os, bool verbose) const {
  StatsPrinter printer(AllocStats_, Name_);
  printer.printDetailed(os, verbose);
}
#endif // MYLANG_DEBUG

} // namespace mylang
