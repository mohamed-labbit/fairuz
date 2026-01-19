#include "../../../include/runtime/allocator/arena_block.hpp"


namespace mylang {
namespace runtime {
namespace allocator {

ArenaBlock::ArenaBlock(const SizeType size, const SizeType alignment) :
    Size_(size)
{
  // Validate alignment is a power of 2
  if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    diagnostic::engine.emit("Alignment must be a power of two", diagnostic::DiagnosticEngine::Severity::FATAL);
  // Round up size to multiple of alignment
  SizeType mod = Size_ % alignment;
  if (mod)
    Size_ += (alignment - mod);
  // Allocate aligned memory
  void* mem = std::aligned_alloc(alignment, Size_);
  if (mem == nullptr)
    throw std::bad_alloc();
  /// TODO: change after debug
  // diagnostic::engine.panic("bad alloc");
  Begin_ = reinterpret_cast<Pointer>(mem);
  Next_.store(Begin_, std::memory_order_relaxed);
}

ArenaBlock::ArenaBlock(ArenaBlock&& other) MYLANG_NOEXCEPT
{
  std::lock_guard<std::mutex> lock(other.Mutex_);
  Size_  = other.Size_;
  Begin_ = other.Begin_;
  Next_.store(other.Next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
  // Reset source
  other.Begin_ = nullptr;
  other.Next_.store(nullptr, std::memory_order_relaxed);
  other.Size_ = 0;
}

ArenaBlock::~ArenaBlock()
{
  size_t                      number_of_frees = 0;
  std::lock_guard<std::mutex> lock(Mutex_);
  if (Begin_ != nullptr)
  {
    std::free(Begin_);
    Begin_ = nullptr;
    Next_.store(nullptr, std::memory_order_relaxed);
  }
}

ArenaBlock& ArenaBlock::operator=(ArenaBlock&& other) MYLANG_NOEXCEPT
{
  if (this != &other)
  {
    std::scoped_lock lock(Mutex_, other.Mutex_);
    // Free existing memory
    if (Begin_ != nullptr)
      std::free(Begin_);
    // Take ownership
    Size_  = other.Size_;
    Begin_ = other.Begin_;
    Next_.store(other.Next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    // Reset source
    other.Begin_ = nullptr;
    other.Next_.store(nullptr, std::memory_order_relaxed);
    other.Size_ = 0;
  }
  return *this;
}

Pointer ArenaBlock::allocate(SizeType bytes, std::optional<SizeType> alignment)
{
  if (Begin_ == nullptr || bytes == 0)
    return nullptr;

  SizeType alignment_value = alignment.value_or(alignof(std::max_align_t));
  // Validate alignment is a power of 2
  if (alignment_value == 0 || (alignment_value & (alignment_value - 1)) != 0)
    diagnostic::engine.emit("Invalid arguments to ArenaAllocator::allocate()", diagnostic::DiagnosticEngine::Severity::FATAL);
  // Lock-free allocation loop
  Pointer current_next = Next_.load(std::memory_order_acquire);

  for (;;)
  {
    // Calculate aligned address
    std::uintptr_t cur     = reinterpret_cast<std::uintptr_t>(current_next);
    std::uintptr_t aligned = (cur + (alignment_value - 1)) & ~(alignment_value - 1);
    SizeType       pad     = aligned - cur;
    // Check if we have enough space (including padding)
    SizeType remaining = Begin_ + Size_ - current_next;
    if (remaining < bytes + pad)
      return nullptr;  // Not enough space
    Pointer new_next = reinterpret_cast<Pointer>(aligned + bytes);
    // Try to atomically update next Pointer
    if (Next_.compare_exchange_weak(current_next, new_next, std::memory_order_release, std::memory_order_acquire))
      // Success! Return the aligned address
      return reinterpret_cast<Pointer>(aligned);
    // CAS failed - another thread allocated first
    // current_next was updated by compare_exchange_weak, retry
  }
}

Pointer ArenaBlock::reserve(const SizeType bytes)
{
  if (Begin_ == nullptr || bytes == 0)
    return nullptr;

  Pointer current_next = Next_.load(std::memory_order_acquire);

  for (;;)
  {
    // Check if we have enough space
    SizeType remaining = Begin_ + Size_ - current_next;
    if (remaining < bytes)
      return nullptr;
    Pointer new_next = current_next + bytes;
    if (Next_.compare_exchange_weak(current_next, new_next, std::memory_order_release, std::memory_order_acquire))
      return current_next;
  }
}

}
}
}