#include "../../../include/runtime/allocator/arena_block.hpp"

namespace mylang {
namespace runtime {
namespace allocator {

ArenaBlock::ArenaBlock(SizeType const size, SizeType const alignment)
    : Size_(size)
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
    if (!mem)
        throw std::bad_alloc();
    /// TODO: change after debug
    // diagnostic::engine.panic("bad alloc");
    Begin_ = reinterpret_cast<Pointer>(mem);
    Next_ = Begin_;
}

ArenaBlock::ArenaBlock(ArenaBlock&& other) MYLANG_NOEXCEPT
{
    std::lock_guard<std::mutex> lock(other.Mutex_);
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
    std::lock_guard<std::mutex> lock(Mutex_);
    if (Begin_) {
        std::free(Begin_);
        Begin_ = nullptr;
        Next_ = nullptr;
    }
}

ArenaBlock& ArenaBlock::operator=(ArenaBlock&& other) MYLANG_NOEXCEPT
{
    if (this != &other) {
        std::scoped_lock lock(Mutex_, other.Mutex_);
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

Pointer ArenaBlock::allocate(SizeType bytes, std::optional<SizeType> alignment)
{
    if (!Begin_ || bytes == 0)
        return nullptr;

    SizeType alignment_value = alignment.value_or(alignof(std::max_align_t));
    // Validate alignment is a power of 2
    if (alignment_value == 0 || (alignment_value & (alignment_value - 1)) != 0)
        diagnostic::engine.emit("Invalid arguments to ArenaAllocator::allocate()", diagnostic::DiagnosticEngine::Severity::FATAL);

    // Calculate aligned address
    std::uintptr_t cur = reinterpret_cast<std::uintptr_t>(Next_);
    std::uintptr_t aligned = (cur + (alignment_value - 1)) & ~(alignment_value - 1);
    SizeType pad = aligned - cur;

    // Check if we have enough space (including padding)
    std::uintptr_t end_addr = reinterpret_cast<std::uintptr_t>(Begin_) + Size_;
    std::uintptr_t cur_addr = reinterpret_cast<std::uintptr_t>(Next_);

    if (cur_addr > end_addr)
        return nullptr; // Current pointer is beyond block end

    SizeType remaining = end_addr - cur_addr;

    if (remaining < bytes + pad)
        return nullptr; // Not enough space

    Pointer new_next = reinterpret_cast<Pointer>(aligned + bytes);

    // Try to atomically update next Pointer
    Next_ = new_next;
    // Success! Return the aligned address
    return reinterpret_cast<Pointer>(aligned);
    // CAS failed - another thread allocated first
}

Pointer ArenaBlock::reserve(SizeType const bytes)
{
    if (!Begin_ || bytes == 0)
        return nullptr;

    // Check if we have enough space
    SizeType remaining = Begin_ + Size_ - Next_;
    if (remaining < bytes)
        return nullptr;
    Next_ += bytes;
    return Next_;
}

}
}
}
