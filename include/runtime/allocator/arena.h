#pragma once

#include "../../macros.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <vector>


namespace mylang {
namespace runtime {
namespace allocator {

using byte = uint_fast8_t;
using pointer = byte*;

// Define constants
#ifndef DEFAULT_BLOCK_SIZE
    #define DEFAULT_BLOCK_SIZE (64 * 1024)  // 64KB
#endif

#ifndef MAX_BLOCK_SIZE
    #define MAX_BLOCK_SIZE (16 * 1024 * 1024)  // 16MB
#endif

struct ArenaAllocStats
{
    std::atomic<std::size_t> total_allocations_{0};
    std::atomic<std::size_t> total_allocated_{0};
    std::atomic<std::size_t> total_free_{0};
    std::atomic<std::size_t> total_deallocations_{0};
    std::atomic<std::size_t> active_blocks_{0};

    ArenaAllocStats() = default;

    // Prevent overflow by saturating at max value
    void safe_increment(std::atomic<std::size_t>& counter, std::size_t amount = 1)
    {
        std::size_t old_val = counter.load(std::memory_order_relaxed);
        std::size_t new_val;
        do
        {
            if (old_val > std::numeric_limits<std::size_t>::max() - amount)
                return;  // Would overflow, saturate
            new_val = old_val + amount;
        } while (!counter.compare_exchange_weak(
          old_val, new_val, std::memory_order_relaxed, std::memory_order_relaxed));
    }
};

class ArenaBlock
{
   private:
    std::size_t size_{DEFAULT_BLOCK_SIZE};
    pointer begin_{nullptr};
    std::atomic<pointer> next_{nullptr};
    mutable std::mutex mutex_;

   public:
    explicit ArenaBlock(const std::size_t size = DEFAULT_BLOCK_SIZE,
      const std::size_t alignment = alignof(std::max_align_t)) :
        size_(size)
    {
        if (alignment == 0 || (alignment & (alignment - 1)) != 0)
            throw std::invalid_argument("Alignment must be a power of two");
        // ensure size is multiple of alignment
        std::size_t mod = size_ % alignment;
        if (mod)
            size_ += (alignment - mod);
        void* mem = std::aligned_alloc(alignment, size_);
        if (!mem)
            throw std::bad_alloc();
        begin_ = reinterpret_cast<pointer>(mem);
        next_ = begin_;
    }

    ~ArenaBlock()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (begin_)
        {
            std::free(begin_);
            begin_ = nullptr;
            next_.store(nullptr, std::memory_order_relaxed);
        }
    }

    ArenaBlock(const ArenaBlock&) = delete;
    ArenaBlock& operator=(const ArenaBlock&) = delete;

    ArenaBlock(ArenaBlock&& other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        size_ = other.size_;
        begin_ = other.begin_;
        next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        other.begin_ = nullptr;
        other.next_.store(nullptr, std::memory_order_relaxed);
        other.size_ = 0;
    }

    ArenaBlock& operator=(ArenaBlock&& other) noexcept
    {
        if (this != &other)
        {
            std::scoped_lock lock(mutex_, other.mutex_);
            if (begin_)
                std::free(begin_);
            size_ = other.size_;
            begin_ = other.begin_;
            next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            other.begin_ = nullptr;
            other.next_.store(nullptr, std::memory_order_relaxed);
            other.size_ = 0;
        }
        return *this;
    }

    pointer begin() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return begin_;
    }

    pointer end() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return begin_ + size_;
    }

    pointer cnext() const { return next_.load(std::memory_order_acquire); }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    std::size_t remaining() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!begin_)
            return 0;
        pointer current_next = next_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(begin_ + size_ - current_next);
    }

    pointer allocate(std::size_t bytes, std::optional<std::size_t> alignment = std::nullopt)
    {
        if (!begin_ || bytes == 0)
            return nullptr;
        std::size_t alignment_value =
          alignment.value_or(std::alignment_of<std::max_align_t>::value);
        // Validate alignment
        if (alignment_value == 0 || (alignment_value & (alignment_value - 1)) != 0)
            throw std::invalid_argument("Invalid alignment!");
        // Try lock-free allocation first using compare-and-swap
        pointer current_next = next_.load(std::memory_order_acquire);
        while (true)
        {
            std::uintptr_t cur = reinterpret_cast<std::uintptr_t>(current_next);
            std::uintptr_t aligned = (cur + (alignment_value - 1)) & ~(alignment_value - 1);
            std::size_t pad = aligned - cur;
            // Check if we have enough space
            std::size_t remaining = begin_ + size_ - current_next;
            if (remaining < bytes + pad)
                return nullptr;
            pointer new_next = reinterpret_cast<pointer>(aligned + bytes);
            // Try to atomically update next pointer
            if (next_.compare_exchange_weak(
                  current_next, new_next, std::memory_order_release, std::memory_order_acquire))
                return reinterpret_cast<pointer>(aligned);
            // CAS failed, retry with updated current_next
        }
    }

    pointer reserve(const std::size_t bytes)
    {
        if (!begin_ || bytes == 0)
            return nullptr;
        pointer current_next = next_.load(std::memory_order_acquire);
        while (true)
        {
            // Check if we have enough space
            std::size_t remaining = begin_ + size_ - current_next;
            if (remaining < bytes)
                return nullptr;
            pointer new_next = current_next + bytes;
            if (next_.compare_exchange_weak(
                  current_next, new_next, std::memory_order_release, std::memory_order_acquire))
                return current_next;
        }
    }
};

template<std::size_t ObjectSize>
class LockFreeFastAllocBlock
{
   private:
    std::atomic<std::size_t> size_{DEFAULT_BLOCK_SIZE};
    std::atomic<pointer> begin_{nullptr};
    std::atomic<pointer> next_{nullptr};

   public:
    LockFreeFastAllocBlock() = default;

    explicit LockFreeFastAllocBlock(std::size_t size)
    {
        std::size_t actual_size = size > DEFAULT_BLOCK_SIZE ? size : DEFAULT_BLOCK_SIZE;
        actual_size = (actual_size + ObjectSize - 1) & ~(ObjectSize - 1);
        pointer mem = reinterpret_cast<pointer>(std::aligned_alloc(ObjectSize, actual_size));
        if (!mem)
            throw std::bad_alloc();
        size_.store(actual_size, std::memory_order_relaxed);
        begin_.store(mem, std::memory_order_relaxed);
        next_.store(mem, std::memory_order_relaxed);
    }

    ~LockFreeFastAllocBlock()
    {
        pointer mem = begin_.load(std::memory_order_relaxed);
        if (mem)
        {
            std::free(mem);
            begin_.store(nullptr, std::memory_order_relaxed);
            next_.store(nullptr, std::memory_order_relaxed);
        }
    }

    LockFreeFastAllocBlock(const LockFreeFastAllocBlock&) = delete;
    LockFreeFastAllocBlock& operator=(const LockFreeFastAllocBlock&) = delete;

    LockFreeFastAllocBlock(LockFreeFastAllocBlock&& other) noexcept
    {
        size_.store(other.size_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        begin_.store(other.begin_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        other.begin_.store(nullptr, std::memory_order_relaxed);
        other.next_.store(nullptr, std::memory_order_relaxed);
        other.size_.store(0, std::memory_order_relaxed);
    }

    LockFreeFastAllocBlock& operator=(LockFreeFastAllocBlock&& other) noexcept
    {
        if (this != &other)
        {
            pointer old_mem = begin_.load(std::memory_order_relaxed);
            if (old_mem)
                std::free(old_mem);
            size_.store(other.size_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            begin_.store(other.begin_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            other.begin_.store(nullptr, std::memory_order_relaxed);
            other.next_.store(nullptr, std::memory_order_relaxed);
            other.size_.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    pointer begin() const { return begin_.load(std::memory_order_acquire); }

    pointer end() const
    {
        pointer b = begin_.load(std::memory_order_acquire);
        std::size_t s = size_.load(std::memory_order_acquire);
        return b ? (b + s) : nullptr;
    }

    pointer cnext() const { return next_.load(std::memory_order_acquire); }
    std::size_t size() const { return size_.load(std::memory_order_acquire); }

    std::size_t remaining() const
    {
        pointer b = begin_.load(std::memory_order_acquire);
        if (!b)
            return 0;
        pointer current = next_.load(std::memory_order_acquire);
        std::size_t s = size_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(b + s - current);
    }

    [[nodiscard]]
    pointer allocate(std::size_t alloc_size)
    {
        if (alloc_size == 0)
            return nullptr;
        pointer b = begin_.load(std::memory_order_acquire);
        if (!b)
        {
            std::cerr << "Failed to load begin pointer" << std::endl;
            return nullptr;
        }
        pointer current_next = next_.load(std::memory_order_acquire);
        std::size_t s = size_.load(std::memory_order_acquire);
        while (true)
        {
            std::size_t remaining = b + s - current_next;
            if (remaining < alloc_size)
            {
                std::cerr << "Failed to allocate because there's not enough remaining bytes"
                          << std::endl;
                return nullptr;
            }
            pointer new_next = current_next + alloc_size;
            if (next_.compare_exchange_weak(
                  current_next, new_next, std::memory_order_release, std::memory_order_acquire))
                return current_next;
        }
    }
};

struct AllocationHeader
{
    uint_fast32_t magic_;
    uint_fast32_t size_;
    uint_fast32_t alignment_;
    uint_fast32_t checksum_;
    std::chrono::steady_clock::time_point timestamp_;
    const char32_t* type_name_;
    uint_fast32_t line_number_;
    const char32_t* file_name_;
    static constexpr uint32_t MAGIC_ = 0xDEADC0DE;

    uint32_t compute_checksum() const { return magic_ ^ size_ ^ alignment_; }
    bool is_valid() const { return magic_ == MAGIC_ && checksum_ == compute_checksum(); }
};

struct AllocationFooter
{
    uint32_t guard;
    static constexpr uint32_t GUARD = 0xFEEDFACE;

    bool is_valid() const { return guard == GUARD; }
};

struct FreeListRegion
{
    pointer ptr_;
    std::size_t size_;  // Size in BYTES

    FreeListRegion() = default;

    FreeListRegion(pointer p, std::size_t s) :
        ptr_(p),
        size_(s)
    {
    }

    FreeListRegion(const FreeListRegion&) = default;
    FreeListRegion(FreeListRegion&& other) noexcept :
        ptr_(other.ptr_),
        size_(other.size_)
    {
        other.ptr_ = nullptr;
        other.size_ = 0;
    }

    FreeListRegion& operator=(const FreeListRegion&) = default;
    FreeListRegion& operator=(FreeListRegion&&) noexcept = default;

    bool has_space_for(std::size_t size) const { return size_ >= size; }

    // For ordered containers
    bool operator<(const FreeListRegion& other) const
    {
        if (size_ != other.size_)
            return size_ < other.size_;
        return ptr_ < other.ptr_;
    }
};

template<std::size_t ObjectSize>
class FastAllocBlockFreeList
{
   private:
    // Use multiset for O(log n) best-fit allocation
    std::multiset<FreeListRegion> regions_;
    mutable std::mutex mutex_;

   public:
    FastAllocBlockFreeList() = default;

    explicit FastAllocBlockFreeList(const std::vector<FreeListRegion>& regions)
    {
        for (const auto& reg : regions)
            regions_.insert(reg);
    }

    FastAllocBlockFreeList(FastAllocBlockFreeList&& other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        regions_ = std::move(other.regions_);
    }

    FastAllocBlockFreeList& operator=(FastAllocBlockFreeList&& other) noexcept
    {
        if (this != &other)
        {
            std::scoped_lock lock(mutex_, other.mutex_);
            regions_ = std::move(other.regions_);
        }
        return *this;
    }

    void append(const FreeListRegion& reg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        regions_.insert(reg);
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return regions_.empty();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        regions_.clear();
    }

    std::optional<FreeListRegion> find_and_extract(std::size_t size, std::size_t align)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Find best fit (smallest region that can hold the allocation)
        FreeListRegion search_key(nullptr, size);
        auto it = regions_.lower_bound(search_key);
        while (it != regions_.end())
        {
            FreeListRegion reg = *it;
            std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(reg.ptr_);
            std::uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
            std::size_t padding = aligned - addr;
            if (reg.size_ >= size + padding)
            {
                regions_.erase(it);
                // Split if there's leftover
                std::size_t used = size + padding;
                if (reg.size_ > used)
                {
                    pointer remainder = reg.ptr_ + used;
                    std::size_t remainder_size = reg.size_ - used;
                    regions_.insert(FreeListRegion(remainder, remainder_size));
                }
                return reg;
            }
            ++it;
        }
        return std::nullopt;
    }
};

class ArenaAllocator
{
   public:
    enum class GrowthStrategy : int { LINEAR, EXPONENTIAL };
    using OutOfMemoryHandler = std::function<bool(std::size_t requested)>;

   private:
    struct VoidPtrHash
    {
        std::size_t operator()(const void* ptr) const noexcept
        {
            return std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(ptr));
        }
    };

    struct VoidPtrEqual
    {
        bool operator()(const void* a, const void* b) const noexcept { return a == b; }
    };

    ArenaAllocStats alloc_stats_;
    std::vector<ArenaBlock> blocks_{};
    mutable std::shared_mutex blocks_mutex_;
    std::vector<LockFreeFastAllocBlock<8>> fast_pool8_{};
    std::vector<LockFreeFastAllocBlock<16>> fast_pool16_{};
    std::vector<LockFreeFastAllocBlock<32>> fast_pool32_{};
    std::vector<LockFreeFastAllocBlock<64>> fast_pool64_{};
    std::vector<LockFreeFastAllocBlock<128>> fast_pool128_{};
    std::vector<LockFreeFastAllocBlock<256>> fast_pool256_{};
    mutable std::mutex fast_pool_mutex_;
    FastAllocBlockFreeList<8> free_list8_;
    FastAllocBlockFreeList<16> free_list16_;
    FastAllocBlockFreeList<32> free_list32_;
    FastAllocBlockFreeList<64> free_list64_;
    FastAllocBlockFreeList<128> free_list128_;
    FastAllocBlockFreeList<256> free_list256_;
    std::atomic<GrowthStrategy> growth_factor_{GrowthStrategy::LINEAR};
    std::size_t block_size_{DEFAULT_BLOCK_SIZE};
    std::atomic<std::size_t> next_block_size_{DEFAULT_BLOCK_SIZE};
    std::string name_{"arena"};
    OutOfMemoryHandler oom_handler_{nullptr};
    mutable std::mutex oom_handler_mutex_;
    // Use multiset for general free list too
    std::multiset<FreeListRegion> free_list_{};
    mutable std::mutex free_list_mutex_;
    std::unordered_map<void*, AllocationHeader, VoidPtrHash, VoidPtrEqual> allocation_map_{};
    mutable std::shared_mutex allocation_map_mutex_;
    // Track allocated pointers to prevent double-free
    std::unordered_set<void*, VoidPtrHash, VoidPtrEqual> allocated_ptrs_{};
    mutable std::shared_mutex allocated_ptrs_mutex_;
    std::atomic<bool> track_allocations_{false};
    std::atomic<bool> debug_features_{false};
    std::atomic<bool> enable_statistics_{true};
    std::size_t min_alignment_{std::alignment_of<std::max_align_t>::value};
    std::atomic<std::size_t> max_block_size_{MAX_BLOCK_SIZE};

   public:
    ArenaAllocator(
      /*std::size_t init_block_size = DEFAULT_BLOCK_SIZE,*/ int growth_strategy = static_cast<int>(
        GrowthStrategy::EXPONENTIAL),
      std::size_t min_align = std::alignment_of<std::max_align_t>::value,
      OutOfMemoryHandler oom_handler = nullptr,
      bool debug = false) :
        /*block_size_(init_block_size),*/
        growth_factor_(GrowthStrategy(growth_strategy)),
        min_alignment_(min_align),
        oom_handler_(oom_handler),
        debug_features_(debug)
    {
        // Validate alignment
        if (min_alignment_ == 0 || (min_alignment_ & (min_alignment_ - 1)) != 0)
            min_alignment_ = alignof(std::max_align_t);
        if (debug_features_.load(std::memory_order_relaxed))
        {
            track_allocations_.store(true, std::memory_order_relaxed);
            enable_statistics_.store(true, std::memory_order_relaxed);
        }
        /*
         * 
        fast_pool8_.push_back(std::move(LockFreeFastAllocBlock<8>(block_size_)));
        fast_pool16_.push_back(std::move(LockFreeFastAllocBlock<16>(block_size_)));
        fast_pool32_.push_back(std::move(LockFreeFastAllocBlock<32>(block_size_)));
        fast_pool64_.push_back(std::move(LockFreeFastAllocBlock<64>(block_size_)));
        fast_pool128_.push_back(std::move(LockFreeFastAllocBlock<128>(block_size_)));
        fast_pool256_.push_back(std::move(LockFreeFastAllocBlock<256>(block_size_)));
        
        blocks_.push_back(std::move(ArenaBlock(block_size_)));
        */
    }

    ~ArenaAllocator() { reset(); }
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&&) noexcept = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) noexcept = delete;

    void reset()
    {
        std::unique_lock blocks_lock(blocks_mutex_);
        std::unique_lock fast_pool_lock(fast_pool_mutex_);
        std::unique_lock free_list_lock(free_list_mutex_);
        std::unique_lock alloc_map_lock(allocation_map_mutex_);
        std::unique_lock alloc_ptrs_lock(allocated_ptrs_mutex_);
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
            allocation_map_.clear();
        allocated_ptrs_.clear();
        alloc_stats_.total_allocations_.store(0, std::memory_order_relaxed);
        alloc_stats_.total_allocated_.store(0, std::memory_order_relaxed);
        alloc_stats_.total_free_.store(0, std::memory_order_relaxed);
        alloc_stats_.total_deallocations_.store(0, std::memory_order_relaxed);
        alloc_stats_.active_blocks_.store(0, std::memory_order_relaxed);
        next_block_size_.store(block_size_, std::memory_order_relaxed);
    }

    std::size_t total_allocated() const
    {
        return alloc_stats_.total_allocated_.load(std::memory_order_relaxed);
    }
    std::size_t total_allocations() const
    {
        return alloc_stats_.total_allocations_.load(std::memory_order_relaxed);
    }
    std::size_t active_blocks() const
    {
        return alloc_stats_.active_blocks_.load(std::memory_order_relaxed);
    }

    [[nodiscard]]
    pointer allocate_block(std::size_t requested,
      std::size_t alignment_ = alignof(std::max_align_t),
      bool retry_on_oom = true)
    {
        // Validate alignment
        if (alignment_ == 0 || (alignment_ & (alignment_ - 1)) != 0)
            alignment_ = min_alignment_;
        std::size_t block_size =
          std::max(requested + alignment_, next_block_size_.load(std::memory_order_relaxed));
        if (block_size > max_block_size_.load(std::memory_order_relaxed))
        {
            if (retry_on_oom)
            {
                std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
                if (oom_handler_ && oom_handler_(block_size))
                    // Retry once without OOM handling to prevent infinite loop
                    return allocate_block(requested, alignment_, false);
            }
            return nullptr;
        }
        try
        {
            std::unique_lock<std::shared_mutex> lock(blocks_mutex_);
            blocks_.emplace_back(block_size, alignment_);
            alloc_stats_.safe_increment(alloc_stats_.active_blocks_);
            return blocks_.back().begin();
        } catch (const std::bad_alloc&)
        {
            if (retry_on_oom)
            {
                std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
                if (oom_handler_ && oom_handler_(block_size))
                    // Retry once without OOM handling to prevent infinite loop
                    return allocate_block(requested, alignment_, false);
            }
            return nullptr;
        }
    }

    template<std::size_t ObjectSize>
    [[nodiscard]]
    pointer allocate_fast_block(std::size_t size, bool retry_on_oom = true)
    {
        std::size_t alloc_size = std::max(size, static_cast<std::size_t>(DEFAULT_BLOCK_SIZE));
        if (alloc_size > MAX_BLOCK_SIZE)
        {
            if (retry_on_oom)
            {
                std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
                if (oom_handler_ && oom_handler_(alloc_size))
                    // Retry once without OOM handling to prevent infinite loop
                    return allocate_fast_block<ObjectSize>(size, false);
            }
            return nullptr;
        }
        try
        {
            LockFreeFastAllocBlock<ObjectSize> arena_block(alloc_size);
            std::lock_guard<std::mutex> lock(fast_pool_mutex_);
            auto pool = choose_pool<ObjectSize>();
            if (!pool)
                return nullptr;
            pointer ret = arena_block.begin();
            pool->push_back(std::move(arena_block));
            alloc_stats_.safe_increment(alloc_stats_.active_blocks_);
            return ret;
        } catch (const std::bad_alloc&)
        {
            if (retry_on_oom)
            {
                std::lock_guard<std::mutex> oom_lock(oom_handler_mutex_);
                if (oom_handler_ && oom_handler_(alloc_size))
                    // Retry once without OOM handling to prevent infinite loop
                    return allocate_fast_block<ObjectSize>(size, false);
            }
            return nullptr;
        }
    }

    template<typename _Tp>
    [[nodiscard]]
    _Tp* allocate(std::size_t count = 1)
    {
        auto start = std::chrono::high_resolution_clock::now();
        if (count == 0)
            return nullptr;
        if (count > MAX_BLOCK_SIZE / sizeof(_Tp))
        {
            std::cerr << "allocation size is too large!" << std::endl;
            if (oom_handler_ && oom_handler_(count))
                return allocate<_Tp>(count);
            throw std::bad_alloc();  // prevent overflow
        }
        std::size_t alloc_size = count * sizeof(_Tp);
        std::size_t align = std::max(std::alignment_of<_Tp>::value, min_alignment_);
        if (alloc_size > MAX_BLOCK_SIZE)
        {
            std::cerr << "allocation size (after alignment) is too larger!" << std::endl;
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
            if (align <= pool_size)
            {
                mem = allocate_from_fast_pool<pool_size>(alloc_size);
                if (!mem)
                    std::cerr << "allocate_from_fast_pool() failed!" << std::endl;
            }
        if (!mem)
            mem = allocate_from_blocks(alloc_size, align);
        if (!mem)
        {
            std::cerr << "allocate_from_blocks() failed!" << std::endl;
            return nullptr;
        }
        _Tp* region = reinterpret_cast<_Tp*>(mem);
        if (enable_statistics_.load(std::memory_order_relaxed))
        {
            alloc_stats_.safe_increment(alloc_stats_.total_allocations_);
            alloc_stats_.safe_increment(alloc_stats_.total_allocated_, alloc_size);
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
        // Track allocated pointer for double-free protection
        std::unique_lock<std::shared_mutex> lock(allocated_ptrs_mutex_);
        allocated_ptrs_.insert(region);
        // Always initialize objects properly
        if constexpr (!std::is_trivially_constructible_v<_Tp>)
            for (std::size_t i = 0; i < count; ++i)
                new (region + i) _Tp();
        auto end = std::chrono::high_resolution_clock::now();
        return region;
    }

    template<typename _Tp>
    void deallocate(_Tp* ptr, std::size_t count = 1)
    {
        if (!ptr)
            return;
        // Check for double-free
        std::unique_lock<std::shared_mutex> lock(allocated_ptrs_mutex_);
        auto it = allocated_ptrs_.find(ptr);
        if (it == allocated_ptrs_.end())
        {
            // Double-free detected!
            if (debug_features_.load(std::memory_order_relaxed))
                std::cerr << "ERROR: Double-free detected for pointer " << ptr << std::endl;
            return;
        }
        allocated_ptrs_.erase(it);
        std::size_t byte_size = count * sizeof(_Tp);
        if constexpr (!std::is_trivially_destructible_v<_Tp>)
            for (std::size_t i = 0; i < count; ++i)
                ptr[i].~_Tp();
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
            FastAllocBlockFreeList<pool_size>* free_list = choose_pool_free_list<pool_size>();
            if (free_list)
                free_list->append(region);
            else
            {
                std::lock_guard<std::mutex> lock(free_list_mutex_);
                free_list_.insert(region);
            }
        }
        else
        {
            std::lock_guard<std::mutex> lock(free_list_mutex_);
            free_list_.insert(region);
        }
        if (enable_statistics_.load(std::memory_order_relaxed))
            alloc_stats_.safe_increment(alloc_stats_.total_deallocations_);
        if (track_allocations_.load(std::memory_order_relaxed) && ptr)
        {
            std::unique_lock<std::shared_mutex> lock(allocation_map_mutex_);
            allocation_map_.erase(ptr);
        }
    }

    [[nodiscard]]
    bool verify_allocation(void* ptr) const
    {
        if (!track_allocations_.load(std::memory_order_relaxed))
            return true;
        std::shared_lock<std::shared_mutex> lock(allocation_map_mutex_);
        auto it = allocation_map_.find(ptr);
        if (it == allocation_map_.end())
            return false;
        return it->second.is_valid();
    }

   private:
    template<std::size_t ObjectSize>
    [[nodiscard]]
    pointer allocate_from_fast_pool(std::size_t alloc_size)
    {
        std::unique_lock<std::mutex> lock(fast_pool_mutex_);
        std::vector<LockFreeFastAllocBlock<ObjectSize>>* pool = choose_pool<ObjectSize>();
        if (!pool)
        {
            std::cerr << "choose_pool() Failed!" << std::endl;
            return nullptr;
        }
        if (pool->empty())
        {
            // Release lock before allocating new block
            lock.unlock();
            std::size_t block_size =
              std::max(alloc_size, next_block_size_.load(std::memory_order_relaxed));
            if (!allocate_fast_block<ObjectSize>(block_size))
            {
                std::cerr << "allocate_fast_block() failed!" << std::endl;
                return nullptr;
            }
            update_next_block_size();
            // Reacquire lock
            lock.lock();
        }
        pointer mem = allocate_using_fast_free_list<ObjectSize>(alloc_size);
        if (mem)
            return mem;
        LockFreeFastAllocBlock<ObjectSize>& fast_block = pool->back();
        if (fast_block.remaining() < alloc_size)
        {
            // Release lock before allocating new block
            lock.unlock();
            std::size_t block_size =
              std::max(alloc_size, next_block_size_.load(std::memory_order_relaxed));
            if (!allocate_fast_block<ObjectSize>(block_size))
            {
                std::cerr << "allocate_fast_block() failed!" << std::endl;
                return nullptr;
            }
            update_next_block_size();
            // Reacquire lock
            lock.lock();
        }
        pointer ret = pool->back().allocate(alloc_size);
        std::string print_value =
          ret != nullptr ? std::to_string(reinterpret_cast<std::uintptr_t>(ret)) : "nullptr";
        std::cout << "-- DEBUG : allocate_from_fast_pool() returned with ret = " << print_value
                  << std::endl;
        return ret;
    }

    template<std::size_t ObjectSize>
    [[nodiscard]]
    pointer allocate_using_fast_free_list(std::size_t alloc_size)
    {
        auto free_list = choose_pool_free_list<ObjectSize>();
        if (!free_list)
            return nullptr;
        auto result = free_list->find_and_extract(alloc_size, ObjectSize);
        if (result.has_value())
        {
            std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(result->ptr_);
            std::uintptr_t aligned = (addr + (ObjectSize - 1)) & ~(ObjectSize - 1);
            return reinterpret_cast<pointer>(aligned);
        }
        return nullptr;
    }

    [[nodiscard]]
    pointer allocate_using_free_list(std::size_t alloc_size, std::size_t align)
    {
        std::lock_guard<std::mutex> lock(free_list_mutex_);
        if (free_list_.empty())
            return nullptr;
        // Find best fit using multiset's ordering
        FreeListRegion search_key(nullptr, alloc_size);
        auto it = free_list_.lower_bound(search_key);

        while (it != free_list_.end())
        {
            FreeListRegion reg = *it;
            // Check alignment of the pointer
            std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(reg.ptr_);
            std::uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
            std::size_t padding = aligned - addr;
            if (reg.size_ >= alloc_size + padding)
            {
                pointer result = reinterpret_cast<pointer>(aligned);
                // Remove the used region
                free_list_.erase(it);
                // If there's leftover space, split the region
                std::size_t used = alloc_size + padding;
                if (reg.size_ > used)
                {
                    pointer remainder = reg.ptr_ + used;
                    std::size_t remainder_size = reg.size_ - used;
                    free_list_.insert(FreeListRegion(remainder, remainder_size));
                }
                return result;
            }
            ++it;
        }
        return nullptr;
    }

    [[nodiscard]]
    pointer allocate_from_blocks(std::size_t alloc_size, std::size_t align)
    {
        // Validate alignment
        align = min_alignment_;
        if (align == 0 || (align & (align - 1)) != 0)
        {
            std::shared_lock<std::shared_mutex> lock(blocks_mutex_);
            if (!blocks_.empty())
            {
                // Try free list first
                pointer mem = allocate_using_free_list(alloc_size, align);
                if (mem)
                    return mem;
                // Try current block
                mem = blocks_.back().allocate(alloc_size, align);
                if (mem)
                    return mem;
            }
        }
        // Need a new block
        std::size_t new_block_size =
          std::max(alloc_size, next_block_size_.load(std::memory_order_relaxed));
        if (!allocate_block(new_block_size, align))
        {
            if (debug_features_.load(std::memory_order_relaxed))
                std::cerr << "-- Failed to allocate block : ArenaAllocator::allocate_block()"
                          << std::endl;
            return nullptr;
        }
        update_next_block_size();
        std::shared_lock<std::shared_mutex> lock(blocks_mutex_);
        return blocks_.back().allocate(alloc_size, align);
    }

    void update_next_block_size() noexcept
    {
        // TODO : prevent overflow
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
    std::vector<LockFreeFastAllocBlock<ObjectSize>>* choose_pool() noexcept
    {
        if constexpr (ObjectSize == 8)
            return &fast_pool8_;
        else if constexpr (ObjectSize == 16)
            return &fast_pool16_;
        else if constexpr (ObjectSize == 32)
            return &fast_pool32_;
        else if constexpr (ObjectSize == 64)
            return &fast_pool64_;
        else if constexpr (ObjectSize == 128)
            return &fast_pool128_;
        else if constexpr (ObjectSize == 256)
            return &fast_pool256_;
        else
            return nullptr;
    }

    template<std::size_t ObjectSize>
    [[nodiscard]]
    FastAllocBlockFreeList<ObjectSize>* choose_pool_free_list() noexcept
    {
        if constexpr (ObjectSize == 8)
            return &free_list8_;
        else if constexpr (ObjectSize == 16)
            return &free_list16_;
        else if constexpr (ObjectSize == 32)
            return &free_list32_;
        else if constexpr (ObjectSize == 64)
            return &free_list64_;
        else if constexpr (ObjectSize == 128)
            return &free_list128_;
        else if constexpr (ObjectSize == 256)
            return &free_list256_;
        else
            return nullptr;
    }
};

}  // namespace allocator
}  // namespace runtime
}  // namespace mylang