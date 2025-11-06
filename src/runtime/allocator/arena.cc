#include "../../../include/runtime/allocator/arena.h"


#include <atomic>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>


namespace mylang {
namespace runtime {
namespace allocator {


ArenaBlock::ArenaBlock(const std::size_t size, const std::size_t alignment) :
    size_(size)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    {
        throw std::invalid_argument("Alignment must be a power of two");
    }

    // ensure size is multiple of alignment
    std::size_t mod = size_ % alignment;
    if (mod)
    {
        size_ += (alignment - mod);
    }

    void* mem = std::aligned_alloc(alignment, size_);
    if (!mem)
    {
        throw std::bad_alloc();
    }

    begin_ = reinterpret_cast<pointer>(mem);
    next_ = begin_;
}

ArenaBlock::ArenaBlock(ArenaBlock&& other) noexcept
{
    std::scoped_lock lock(mutex_, other.mutex_);
    size_ = other.size();
    begin_ = other.begin();
    next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);

    other.begin_ = nullptr;
    other.next_.store(nullptr, std::memory_order_relaxed);
    other.size_ = 0;
}

ArenaBlock& ArenaBlock::operator=(ArenaBlock&& other) noexcept
{
    if (this != &other)
    {
        std::scoped_lock lock(mutex_, other.mutex_);

        if (begin_)
        {
            std::free(begin_);
        }

        size_ = other.size_;
        begin_ = other.begin_;
        next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);

        other.begin_ = nullptr;
        other.next_.store(nullptr, std::memory_order_relaxed);
        other.size_ = 0;
    }
    return *this;
}

pointer ArenaBlock::begin() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return begin_;
}

pointer ArenaBlock::end() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return begin_ + size_;
}

pointer ArenaBlock::cnext() const { return next_.load(std::memory_order_acquire); }

std::size_t ArenaBlock::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

std::size_t ArenaBlock::remaining() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!begin_)
    {
        return 0;
    }

    pointer current_next = next_.load(std::memory_order_acquire);
    return static_cast<std::size_t>(begin_ + size_ - current_next);
}

pointer ArenaBlock::allocate(std::size_t bytes, std::size_t alignment)
{
    if (!begin_)
    {
        return nullptr;
    }

    // Try lock-free allocation first using compare-and-swap
    pointer current_next = next_.load(std::memory_order_acquire);

    while (true)
    {
        std::uintptr_t cur = reinterpret_cast<std::uintptr_t>(current_next);
        std::uintptr_t aligned = (cur + (alignment - 1)) & ~(alignment - 1);
        std::size_t pad = aligned - cur;

        // Check if we have enough space
        std::size_t remaining = begin_ + size_ - current_next;
        if (remaining < bytes + pad)
        {
            return nullptr;
        }

        pointer new_next = reinterpret_cast<pointer>(aligned + bytes);

        // Try to atomically update next pointer
        if (next_.compare_exchange_weak(current_next, new_next, std::memory_order_release, std::memory_order_acquire))
        {
            return reinterpret_cast<pointer>(aligned);
        }

        // CAS failed, retry with updated current_next
        // (current_next was updated by compare_exchange_weak)
    }
}

pointer ArenaBlock::reserve(const std::size_t bytes)
{
    if (!begin_)
    {
        return nullptr;
    }

    pointer current_next = next_.load(std::memory_order_acquire);

    while (true)
    {
        // Check if we have enough space
        std::size_t remaining = begin_ + size_ - current_next;
        if (remaining < bytes)
        {
            return nullptr;
        }

        pointer new_next = current_next + bytes;

        if (next_.compare_exchange_weak(current_next, new_next, std::memory_order_release, std::memory_order_acquire))
        {
            return current_next;
        }
    }
}

template<std::size_t ObjectSize>
LockFreeFastAllocBlock<ObjectSize>::LockFreeFastAllocBlock(std::size_t size)
{
    size_ = size > DEFAULT_BLOCK_SIZE ? size : DEFAULT_BLOCK_SIZE;
    size_ = (size_ + ObjectSize - 1) & ~(ObjectSize - 1);
    begin_ = reinterpret_cast<pointer>(std::aligned_alloc(ObjectSize, size_));
    if (!begin_)
    {
        throw std::bad_alloc();
    }

    next_.store(begin_, std::memory_order_relaxed);
}

template<std::size_t ObjectSize>
LockFreeFastAllocBlock<ObjectSize>::LockFreeFastAllocBlock(LockFreeFastAllocBlock&& other) noexcept :
    size_(other.size_),
    begin_(other.begin_)
{
    next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    other.begin_ = nullptr;
    other.next_.store(nullptr, std::memory_order_relaxed);
    other.size_ = 0;
}

template<std::size_t ObjectSize>
LockFreeFastAllocBlock<ObjectSize>& LockFreeFastAllocBlock<ObjectSize>::operator=(LockFreeFastAllocBlock&& other) noexcept
{
    if (this != &other)
    {
        if (begin_)
        {
            std::free(begin_);
        }

        size_ = other.size_;
        begin_ = other.begin_;
        next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);

        other.begin_ = nullptr;
        other.next_.store(nullptr, std::memory_order_relaxed);
        other.size_ = 0;
    }

    return *this;
}

template<std::size_t ObjectSize>
pointer LockFreeFastAllocBlock<ObjectSize>::begin() const
{
    return begin_;
}

template<std::size_t ObjectSize>
pointer LockFreeFastAllocBlock<ObjectSize>::end() const
{
    return begin_ + size_;
}

template<std::size_t ObjectSize>
pointer LockFreeFastAllocBlock<ObjectSize>::cnext() const
{
    return next_.load(std::memory_order_acquire);
}

template<std::size_t ObjectSize>
std::size_t LockFreeFastAllocBlock<ObjectSize>::size() const
{
    return size_;
}

template<std::size_t ObjectSize>
std::size_t LockFreeFastAllocBlock<ObjectSize>::remaining() const
{
    if (!begin_)
    {
        return 0;
    }

    pointer current = next_.load(std::memory_order_acquire);
    return static_cast<std::size_t>(begin_ + size_ - current);
}

template<std::size_t ObjectSize>
pointer LockFreeFastAllocBlock<ObjectSize>::allocate(std::size_t size)
{
    pointer current_next = next_.load(std::memory_order_acquire);

    while (true)
    {
        std::size_t remaining = begin_ + size_ - current_next;
        if (remaining < size)
        {
            return nullptr;
        }

        pointer new_next = current_next + size;

        if (next_.compare_exchange_weak(current_next, new_next, std::memory_order_release, std::memory_order_acquire))
        {
            return current_next;
        }
    }
}

template<std::size_t ObjectSize>
FastAllocBlockFreeList<ObjectSize>::FastAllocBlockFreeList(FastAllocBlockFreeList&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    regions_ = std::move(other.regions());
}

template<std::size_t ObjectSize>
std::vector<FreeListRegion>& FastAllocBlockFreeList<ObjectSize>::regions() noexcept
{
    return std::ref<std::vector<FreeListRegion>>(regions_);
}

template<std::size_t ObjectSize>
void FastAllocBlockFreeList<ObjectSize>::append(const FreeListRegion& reg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    regions_.push_back(reg);
}

template<std::size_t ObjectSize>
void FastAllocBlockFreeList<ObjectSize>::pop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!regions_.empty())
    {
        regions_.pop_back();
    }
}

template<std::size_t ObjectSize>
void FastAllocBlockFreeList<ObjectSize>::remove(std::size_t at)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (at < regions_.size())
    {
        regions_.erase(regions_.begin() + at);
    }
}

template<std::size_t ObjectSize>
void FastAllocBlockFreeList<ObjectSize>::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    regions_.clear();
}

template<std::size_t ObjectSize>
bool FastAllocBlockFreeList<ObjectSize>::empty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return regions_.empty();
}

template<std::size_t ObjectSize>
std::optional<FreeListRegion> FastAllocBlockFreeList<ObjectSize>::find_and_extract(std::size_t size, std::size_t align)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (size_t i = 0; i < regions_.size(); ++i)
    {
        FreeListRegion& reg = regions_[i];

        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(reg.ptr_);
        std::uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
        std::size_t padding = aligned - addr;

        if (reg.size_ >= size + padding)
        {
            FreeListRegion result = reg;

            // Split if there's leftover
            std::size_t used = size + padding;
            if (reg.size_ > used)
            {
                pointer remainder = reg.ptr_ + used;
                std::size_t remainder_size = reg.size_ - used;
                regions_.push_back(FreeListRegion(remainder, remainder_size));
            }

            regions_.erase(regions_.begin() + i);
            return result;
        }
    }

    return std::nullopt;
}

ArenaAllocator::ArenaAllocator(int growth_strategy, std::size_t min_align, OutOfMemoryHandler oom_handler, bool debug) :
    growth_factor_(GrowthStrategy(growth_strategy)),
    min_alignment_(min_align),
    oom_handler_(oom_handler),
    debug_features_(debug)
{
    if (debug_features_.load(std::memory_order_relaxed))
    {
        track_allocations_.store(true, std::memory_order_relaxed);
        enable_statistics_.store(true, std::memory_order_relaxed);
    }
}

void ArenaAllocator::reset()
{
    std::unique_lock blocks_lock(blocks_mutex_);
    std::unique_lock fast_pool_lock(fast_pool_mutex_);
    std::unique_lock free_list_lock(free_list_mutex_);
    std::unique_lock alloc_map_lock(allocation_map_mutex_);

    blocks_.clear();
    fast_pool8_.clear();
    fast_pool16_.clear();
    fast_pool32_.clear();
    fast_pool64_.clear();
    fast_pool128_.clear();
    fast_pool256_.clear();

    free_list_.clear();
    free_list8_.clear();
    free_list16_.clear();
    free_list32_.clear();
    free_list64_.clear();
    free_list128_.clear();
    free_list256_.clear();

    if (track_allocations_.load(std::memory_order_relaxed))
    {
        allocation_map_.clear();
    }

    alloc_stats_.total_allocations_.store(0, std::memory_order_relaxed);
    alloc_stats_.total_allocated_.store(0, std::memory_order_relaxed);
    alloc_stats_.total_free_.store(0, std::memory_order_relaxed);
    alloc_stats_.total_deallocations_.store(0, std::memory_order_relaxed);
    alloc_stats_.active_blocks_.store(0, std::memory_order_relaxed);

    next_block_size_.store(block_size_, std::memory_order_relaxed);
}

std::size_t ArenaAllocator::total_allocated() const { return alloc_stats_.total_allocated_.load(std::memory_order_relaxed); }

std::size_t ArenaAllocator::total_allocations() const { return alloc_stats_.total_allocations_.load(std::memory_order_relaxed); }

std::size_t ArenaAllocator::active_blocks() const { return alloc_stats_.active_blocks_.load(std::memory_order_relaxed); }

[[nodiscard]]
pointer ArenaAllocator::allocate_block(std::size_t requested, std::size_t alignment_)
{
    if (alignment_ == 0 || (alignment_ & (alignment_ - 1)) != 0)
    {
        return nullptr;
    }

    std::size_t block_size = std::max(requested + alignment_, next_block_size_.load(std::memory_order_relaxed));

    if (block_size > max_block_size_.load(std::memory_order_relaxed))
    {
        std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
        if (oom_handler_ && oom_handler_(block_size))
        {
            block_size = std::max(requested + alignment_, next_block_size_.load(std::memory_order_relaxed));
        }
        else
        {
            return nullptr;
        }
    }

    try
    {
        std::unique_lock<std::shared_mutex> lock(blocks_mutex_);
        blocks_.emplace_back(block_size, alignment_);
        alloc_stats_.active_blocks_.fetch_add(1, std::memory_order_relaxed);
        return blocks_.back().begin();
    } catch (const std::bad_alloc&)
    {
        std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
        if (oom_handler_ && oom_handler_(block_size))
        {
            try
            {
                std::unique_lock<std::shared_mutex> lock(blocks_mutex_);
                blocks_.emplace_back(block_size, alignment_);
                alloc_stats_.active_blocks_.fetch_add(1, std::memory_order_relaxed);
                return blocks_.back().begin();
            } catch (...)
            {
                return nullptr;
            }
        }
        return nullptr;
    }
}

template<std::size_t ObjectSize>
[[nodiscard]]
pointer ArenaAllocator::allocate_fast_block(std::size_t size)
{
    std::size_t alloc_size = std::max(size, static_cast<std::size_t>(DEFAULT_BLOCK_SIZE));

    if (alloc_size > MAX_BLOCK_SIZE)
    {
        std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
        if (oom_handler_ && oom_handler_(alloc_size))
        {
            return allocate_fast_block<ObjectSize>(size);
        }
        return nullptr;
    }

    try
    {
        LockFreeFastAllocBlock<ObjectSize> arena_block(alloc_size);
        std::lock_guard<std::mutex> lock(fast_pool_mutex_);
        auto pool = choose_pool<ObjectSize>();

        if (!pool)
        {
            return nullptr;
        }

        pointer ret = arena_block.begin();
        pool->push_back(std::move(arena_block));
        alloc_stats_.active_blocks_.fetch_add(1, std::memory_order_relaxed);

        return ret;
    } catch (const std::bad_alloc&)
    {
        std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
        if (oom_handler_ && oom_handler_(alloc_size))
        {
            return allocate_fast_block<ObjectSize>(size);
        }

        return nullptr;
    }
}

template<typename _Tp>
[[nodiscard]]
_Tp* ArenaAllocator::allocate(std::size_t count)
{
    if (count == 0)
    {
        return nullptr;
    }

    std::size_t alloc_size = count * sizeof(_Tp);
    std::size_t align = std::max(std::alignment_of<_Tp>::value, min_alignment_);

    if (alloc_size > MAX_BLOCK_SIZE)
    {
        return nullptr;
    }

    pointer mem = nullptr;

    constexpr std::size_t type_size = sizeof(_Tp);
    constexpr std::size_t pool_size = type_size <= 8 ? 8
      : type_size <= 16                              ? 16
      : type_size <= 32                              ? 32
      : type_size <= 64                              ? 64
      : type_size <= 128                             ? 128
      : type_size <= 256                             ? 256
                                                     : 0;

    constexpr bool use_fast_pool = (pool_size > 0) && (pool_size >= type_size);

    if constexpr (use_fast_pool)
    {
        if (align <= pool_size)
        {
            mem = allocate_from_fast_pool<pool_size>(alloc_size);
        }
    }

    if (!mem)
    {
        mem = allocate_from_blocks(alloc_size, align);
    }

    if (!mem)
    {
        return nullptr;
    }

    _Tp* region = reinterpret_cast<_Tp*>(mem);

    if (enable_statistics_.load(std::memory_order_relaxed))
    {
        alloc_stats_.total_allocations_.fetch_add(1, std::memory_order_relaxed);
        alloc_stats_.total_allocated_.fetch_add(alloc_size, std::memory_order_relaxed);
    }

    if (track_allocations_.load(std::memory_order_relaxed))
    {
        AllocationHeader header{};
        header.magic_ = AllocationHeader::MAGIC_;
        header.size_ = static_cast<std::uint32_t>(alloc_size);
        header.alignment_ = static_cast<std::uint32_t>(align);
        header.checksum_ = header.compute_checksum();
        header.timestamp_ = std::chrono::steady_clock::now();
        std::unique_lock<std::shared_mutex> lock(allocation_map_mutex_);
        allocation_map_[region] = header;
    }

    // BUG FIX #3: Construct for all types, not just non-trivial
    if constexpr (std::is_trivially_constructible_v<_Tp>)
    {
        // Zero-initialize or default construct trivial types
        std::memset(region, 0, alloc_size);
    }
    else
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            new (region + i) _Tp();
        }
    }

    return region;
}

template<typename _Tp>
void ArenaAllocator::deallocate(_Tp* ptr, std::size_t count)
{
    if (!ptr)
    {
        return;
    }

    std::size_t byte_size = count * sizeof(_Tp);

    if constexpr (!std::is_trivially_destructible_v<_Tp>)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            ptr[i].~_Tp();
        }
    }

    constexpr std::size_t obj_size = sizeof(_Tp);
    constexpr std::size_t pool_size = obj_size <= 8 ? 8
      : obj_size <= 16                              ? 16
      : obj_size <= 32                              ? 32
      : obj_size <= 64                              ? 64
      : obj_size <= 128                             ? 128
      : obj_size <= 256                             ? 256
                                                    : 0;

    constexpr bool use_fast_pool = (pool_size > 0) && (pool_size >= obj_size);
    FreeListRegion region(reinterpret_cast<pointer>(ptr), byte_size);

    if constexpr (use_fast_pool)
    {
        FastAllocBlockFreeList<obj_size>* free_list = choose_pool_free_list<obj_size>();

        if (free_list)
        {
            free_list->append(region);
        }
        else
        {
            throw std::runtime_error("");
        }
    }
    else
    {
        std::lock_guard<std::mutex> lock(free_list_mutex_);
        free_list_.push_back(region);
    }

    if (enable_statistics_.load(std::memory_order_relaxed))
    {
        alloc_stats_.total_deallocations_.fetch_add(1, std::memory_order_relaxed);
    }

    if (track_allocations_.load(std::memory_order_relaxed) && ptr)
    {
        std::unique_lock<std::shared_mutex> lock(allocation_map_mutex_);
        allocation_map_.erase(ptr);
    }
}

[[nodiscard]]
bool ArenaAllocator::verify_allocation(void* ptr) const
{
    if (!track_allocations_.load(std::memory_order_relaxed))
    {
        return true;
    }

    std::shared_lock<std::shared_mutex> lock(allocation_map_mutex_);
    auto it = allocation_map_.find(ptr);
    if (it == allocation_map_.end())
    {
        return false;
    }

    return it->second.is_valid();
}

template<std::size_t ObjectSize>
[[nodiscard]]
pointer ArenaAllocator::allocate_from_fast_pool(std::size_t alloc_size)
{
    std::lock_guard<std::mutex> lock(fast_pool_mutex_);

    std::vector<LockFreeFastAllocBlock<ObjectSize>>* pool = choose_pool<ObjectSize>();
    if (!pool)
    {
        std::cerr << "choose_pool() Failed!" << std::endl;
        return nullptr;
    }

    if (pool->empty())
    {
        fast_pool_mutex_.unlock();

        if (!allocate_fast_block<ObjectSize>(next_block_size_.load(std::memory_order_relaxed)))
        {
            return nullptr;
        }

        update_next_block_size();

        fast_pool_mutex_.lock();
    }

    pointer mem = allocate_using_fast_free_list<ObjectSize>(alloc_size);
    if (mem)
    {
        return mem;
    }

    LockFreeFastAllocBlock<ObjectSize>& fast_block = pool->back();

    if (fast_block.remaining() < alloc_size)
    {
        fast_pool_mutex_.unlock();

        if (!allocate_fast_block<ObjectSize>(next_block_size_.load(std::memory_order_relaxed)))
        {
            return nullptr;
        }

        update_next_block_size();

        fast_pool_mutex_.lock();
    }

    return pool->back().allocate(alloc_size);
}

template<std::size_t ObjectSize>
[[nodiscard]]
pointer ArenaAllocator::allocate_using_fast_free_list(std::size_t alloc_size)
{
    auto free_list = choose_pool_free_list<ObjectSize>();
    if (!free_list)
    {
        return nullptr;
    }

    if (free_list->empty())
    {
        return nullptr;
    }

    for (size_t i = 0; i < free_list->regions_.size(); ++i)
    {
        FreeListRegion& reg = free_list->regions_[i];

        if (reg.size_ >= alloc_size)
        {
            pointer result = reg.ptr_;

            if (reg.size_ > alloc_size)
            {
                pointer remainder = reg.ptr_ + alloc_size;
                std::size_t remainder_size = reg.size_ - alloc_size;
                free_list->append(FreeListRegion(remainder, remainder_size));
            }

            free_list->remove(i);
            return result;
        }
    }

    return nullptr;
}

// BUG FIX #2: Completely rewritten allocate_using_free_list
[[nodiscard]]
pointer ArenaAllocator::allocate_using_free_list(std::size_t alloc_size, std::size_t align)
{
    std::lock_guard<std::mutex> lock(free_list_mutex_);

    if (free_list_.empty())
    {
        return nullptr;
    }

    for (size_t i = 0; i < free_list_.size(); ++i)
    {
        FreeListRegion& reg = free_list_[i];

        // Check alignment of the pointer
        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(reg.ptr_);
        std::uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
        std::size_t padding = aligned - addr;

        if (reg.size_ >= alloc_size + padding)
        {
            pointer result = reinterpret_cast<pointer>(aligned);

            // If there's leftover space, split the region
            std::size_t used = alloc_size + padding;
            if (reg.size_ > used)
            {
                pointer remainder = reg.ptr_ + used;
                std::size_t remainder_size = reg.size_ - used;
                free_list_.push_back(FreeListRegion(remainder, remainder_size));
            }

            // Remove the used region
            free_list_.erase(free_list_.begin() + i);
            return result;
        }
    }

    return nullptr;
}

[[nodiscard]]
pointer ArenaAllocator::allocate_from_blocks(std::size_t alloc_size, std::size_t align)
{
    if (align == 0 || (align & (align - 1)) != 0)
    {
        std::cerr << "Invalid alignment: " << align << std::endl;
        return nullptr;
    }

    {
        std::shared_lock<std::shared_mutex> lock(blocks_mutex_);

        if (!blocks_.empty())
        {
            // Try free list first
            pointer mem = allocate_using_free_list(alloc_size, align);
            if (mem)
            {
                return mem;
            }

            // Try current block
            mem = blocks_.back().allocate(alloc_size, align);
            if (mem)
            {
                return mem;
            }
        }
    }

    // Need a new block
    std::size_t new_block_size = std::max(alloc_size, next_block_size_.load(std::memory_order_relaxed));
    if (!allocate_block(new_block_size, align))
    {
        std::cerr << "-- Failed to allocate block : ArenaAllocator::allocate_block()" << std::endl;
        return nullptr;
    }

    update_next_block_size();

    std::shared_lock<std::shared_mutex> lock(blocks_mutex_);
    return blocks_.back().allocate(alloc_size, align);
}

void ArenaAllocator::update_next_block_size() noexcept
{
    if (growth_factor_.load(std::memory_order_relaxed) == GrowthStrategy::EXPONENTIAL)
    {
        std::size_t current = next_block_size_.load(std::memory_order_relaxed);
        std::size_t max_size = max_block_size_.load(std::memory_order_relaxed);
        std::size_t new_size = std::min(current * 2, max_size);
        next_block_size_.store(new_size, std::memory_order_relaxed);
    }
}

template<std::size_t ObjectSize>
[[nodiscard]]
std::vector<LockFreeFastAllocBlock<ObjectSize>>* ArenaAllocator::choose_pool() noexcept
{
    if constexpr (ObjectSize == 8)
    {
        return &fast_pool8_;
    }
    else if constexpr (ObjectSize == 16)
    {
        return &fast_pool16_;
    }
    else if constexpr (ObjectSize == 32)
    {
        return &fast_pool32_;
    }
    else if constexpr (ObjectSize == 64)
    {
        return &fast_pool64_;
    }
    else if constexpr (ObjectSize == 128)
    {
        return &fast_pool128_;
    }
    else if constexpr (ObjectSize == 256)
    {
        return &fast_pool256_;
    }
    else
    {
        return nullptr;
    }
}

template<std::size_t ObjectSize>
[[nodiscard]]
FastAllocBlockFreeList<ObjectSize>* ArenaAllocator::choose_pool_free_list() noexcept
{
    if constexpr (ObjectSize == 8)
    {
        return &free_list8_;
    }
    else if constexpr (ObjectSize == 16)
    {
        return &free_list16_;
    }
    else if constexpr (ObjectSize == 32)
    {
        return &free_list32_;
    }
    else if constexpr (ObjectSize == 64)
    {
        return &free_list64_;
    }
    else if constexpr (ObjectSize == 128)
    {
        return &free_list128_;
    }
    else if constexpr (ObjectSize == 256)
    {
        return &free_list256_;
    }
    else
    {
        return nullptr;
    }
}


}
}
}
