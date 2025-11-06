#pragma once

#include "../../macros.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
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
};

// Thread-safe arena block with internal synchronization
class ArenaBlock
{
   private:
    std::size_t size_{DEFAULT_BLOCK_SIZE};
    pointer begin_{nullptr};
    std::atomic<pointer> next_{nullptr};  // Atomic for lock-free bump allocation
    mutable std::mutex mutex_;  // For thread-safe operations

   public:
    explicit ArenaBlock(const std::size_t size = DEFAULT_BLOCK_SIZE, const std::size_t alignment = alignof(std::max_align_t)) :
        size_(size)
    {
        if (alignment == 0 || (alignment & (alignment - 1)) != 0)
        {
            throw std::invalid_argument("Alignment must be a power of two");
        }

        // Ensure size is multiple of alignment
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
        next_.store(begin_, std::memory_order_relaxed);
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
        std::scoped_lock lock(mutex_, other.mutex_);
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

    // Thread-safe lock-free bump allocation using CAS
    [[nodiscard]]
    pointer allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
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

    [[nodiscard]]
    pointer reserve(const std::size_t bytes)
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
};

template<std::size_t ObjectSize>
class LockFreeFastAllocBlock
{
   private:
    std::size_t size_{DEFAULT_BLOCK_SIZE};
    pointer begin_{nullptr};
    std::atomic<pointer> next_{nullptr};  // Atomic for thread safety

   public:
    LockFreeFastAllocBlock() = default;

    explicit LockFreeFastAllocBlock(std::size_t size)
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

    ~LockFreeFastAllocBlock()
    {
        if (begin_)
        {
            std::free(begin_);
            begin_ = nullptr;
            next_.store(nullptr, std::memory_order_relaxed);
        }
    }

    LockFreeFastAllocBlock(const LockFreeFastAllocBlock&) = delete;
    LockFreeFastAllocBlock& operator=(const LockFreeFastAllocBlock&) = delete;

    LockFreeFastAllocBlock(LockFreeFastAllocBlock&& other) noexcept :
        size_(other.size_),
        begin_(other.begin_)
    {
        next_.store(other.next_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        other.begin_ = nullptr;
        other.next_.store(nullptr, std::memory_order_relaxed);
        other.size_ = 0;
    }

    LockFreeFastAllocBlock& operator=(LockFreeFastAllocBlock&& other) noexcept
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

    pointer begin() const { return begin_; }
    pointer end() const { return begin_ + size_; }
    pointer cnext() const { return next_.load(std::memory_order_acquire); }

    std::size_t size() const { return size_; }
    std::size_t remaining() const
    {
        if (!begin_)
            return 0;
        pointer current = next_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(begin_ + size_ - current);
    }

    [[nodiscard]]
    pointer allocate(std::size_t size)
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
};

struct AllocationHeader
{
    uint32_t magic;
    uint32_t size;
    uint32_t alignment;
    uint32_t checksum;
    std::chrono::steady_clock::time_point timestamp;
    const char* type_name;
    uint32_t line_number;
    const char* file_name;

    static constexpr uint32_t MAGIC = 0xDEADC0DE;

    uint32_t compute_checksum() const { return magic ^ size ^ alignment; }

    bool is_valid() const { return magic == MAGIC && checksum == compute_checksum(); }
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
    std::size_t size_;

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
};

template<std::size_t ObjectSize>
class FastAllocBlockFreeList
{
   private:
    std::vector<FreeListRegion> regions_;
    mutable std::mutex mutex_;  // Protect regions vector

   public:
    FastAllocBlockFreeList() = default;

    explicit FastAllocBlockFreeList(const std::vector<FreeListRegion>& regions) :
        regions_(regions)
    {
    }

    FastAllocBlockFreeList(FastAllocBlockFreeList&& other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        regions_ = std::move(other.regions_);
    }

    void append(const FreeListRegion& reg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        regions_.push_back(reg);
    }

    void pop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!regions_.empty())
        {
            regions_.pop_back();
        }
    }

    void remove(std::size_t at)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (at < regions_.size())
        {
            regions_.erase(regions_.begin() + at);
        }
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        regions_.clear();
    }

    // Thread-safe region search and extraction
    std::optional<FreeListRegion> find_and_extract(std::size_t size, std::size_t align)
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
};

// Thread-safe arena allocator
class ArenaAllocator
{
   public:
    enum class GrowthStrategy : int { LINEAR, EXPONENTIAL };
    using OutOfMemoryHandler = std::function<bool(std::size_t requested)>;

   private:
    ArenaAllocStats alloc_stats_;

    // Protect block vectors with mutexes
    std::vector<ArenaBlock> blocks_{};
    mutable std::shared_mutex blocks_mutex_;  // Shared for reads, exclusive for writes

    std::vector<LockFreeFastAllocBlock<8>> fast_pool8_{};
    std::vector<LockFreeFastAllocBlock<16>> fast_pool16_{};
    std::vector<LockFreeFastAllocBlock<32>> fast_pool32_{};
    std::vector<LockFreeFastAllocBlock<64>> fast_pool64_{};
    std::vector<LockFreeFastAllocBlock<128>> fast_pool128_{};
    std::vector<LockFreeFastAllocBlock<256>> fast_pool256_{};
    mutable std::mutex fast_pool_mutex_;  // Single mutex for all fast pools

    FastAllocBlockFreeList<8> free_list8_;
    FastAllocBlockFreeList<16> free_list16_;
    FastAllocBlockFreeList<32> free_list32_;
    FastAllocBlockFreeList<64> free_list64_;
    FastAllocBlockFreeList<128> free_list128_;
    FastAllocBlockFreeList<256> free_list256_;

    std::atomic<GrowthStrategy> growth_factor_{GrowthStrategy::EXPONENTIAL};

    std::size_t block_size_{DEFAULT_BLOCK_SIZE};
    std::atomic<std::size_t> next_block_size_{DEFAULT_BLOCK_SIZE};

    std::string name_{"arena"};

    OutOfMemoryHandler oom_handler_{nullptr};
    mutable std::mutex oom_handler_mutex_;  // Protect OOM handler calls

    std::vector<FreeListRegion> free_list_{};
    mutable std::mutex free_list_mutex_;

    struct VoidPtrHash
    {
        std::size_t operator()(const void* ptr) const noexcept
        {
            return std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(ptr));
        }
    };

    std::unordered_map<void*, AllocationHeader, VoidPtrHash> allocation_map_{};
    mutable std::shared_mutex allocation_map_mutex_;  // Shared for reads, exclusive for writes

    std::atomic<bool> track_allocations_{false};
    std::atomic<bool> debug_features_{false};
    std::atomic<bool> enable_statistics_{true};

    std::size_t min_alignment_{alignof(std::max_align_t)};
    std::atomic<std::size_t> max_block_size_{MAX_BLOCK_SIZE};

   public:
    ArenaAllocator(int growth_strategy = static_cast<int>(GrowthStrategy::EXPONENTIAL),
      std::size_t min_align = alignof(std::max_align_t),
      OutOfMemoryHandler oom_handler = nullptr,
      bool debug = false) :
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

    ~ArenaAllocator() { reset(); }

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    ArenaAllocator(ArenaAllocator&&) noexcept = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) noexcept = delete;

    void reset()
    {
        // Acquire all locks to ensure exclusive access
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

    std::size_t total_allocated() const { return alloc_stats_.total_allocated_.load(std::memory_order_relaxed); }

    std::size_t total_allocations() const { return alloc_stats_.total_allocations_.load(std::memory_order_relaxed); }

    std::size_t active_blocks() const { return alloc_stats_.active_blocks_.load(std::memory_order_relaxed); }

    [[nodiscard]]
    pointer allocate_block(std::size_t requested, std::size_t alignment_ = alignof(std::max_align_t))
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
    pointer allocate_fast_block(std::size_t size)
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
            auto pool = choose_pool_unsafe<ObjectSize>();

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
    _Tp* allocate(std::size_t count = 1)
    {
        if (count == 0)
        {
            return nullptr;
        }

        std::size_t alloc_size = count * sizeof(_Tp);
        std::size_t align = std::max(alignof(_Tp), min_alignment_);

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
            header.magic = AllocationHeader::MAGIC;
            header.size = static_cast<uint32_t>(alloc_size);
            header.alignment = static_cast<uint32_t>(align);
            header.checksum = header.compute_checksum();
            header.timestamp = std::chrono::steady_clock::now();

            std::unique_lock<std::shared_mutex> lock(allocation_map_mutex_);
            allocation_map_[region] = header;
        }

        if constexpr (std::is_trivially_constructible_v<_Tp>)
        {
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
    void deallocate(_Tp* ptr, std::size_t count = 1)
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
            if constexpr (obj_size <= 8)
                free_list8_.append(region);
            else if constexpr (obj_size <= 16)
                free_list16_.append(region);
            else if constexpr (obj_size <= 32)
                free_list32_.append(region);
            else if constexpr (obj_size <= 64)
                free_list64_.append(region);
            else if constexpr (obj_size <= 128)
                free_list128_.append(region);
            else if constexpr (obj_size <= 256)
                free_list256_.append(region);
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
    bool verify_allocation(void* ptr) const
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

   private:
    template<std::size_t ObjectSize>
    [[nodiscard]]
    pointer allocate_from_fast_pool(std::size_t alloc_size)
    {
        std::lock_guard<std::mutex> lock(fast_pool_mutex_);

        std::vector<LockFreeFastAllocBlock<ObjectSize>>* pool = choose_pool_unsafe<ObjectSize>();
        if (!pool)
        {
            std::cerr << "choose_pool() Failed!" << std::endl;
            return nullptr;
        }

        if (pool->empty())
        {
            // Release lock before calling allocate_fast_block to avoid deadlock
            fast_pool_mutex_.unlock();

            if (!allocate_fast_block<ObjectSize>(next_block_size_.load(std::memory_order_relaxed)))
            {
                return nullptr;
            }

            update_next_block_size();

            // Re-acquire lock
            fast_pool_mutex_.lock();
        }

        LockFreeFastAllocBlock<ObjectSize>& fast_block = pool->back();

        // Check if current block has enough space
        if (fast_block.remaining() < alloc_size)
        {
            // Release lock before calling allocate_fast_block to avoid deadlock
            fast_pool_mutex_.unlock();

            if (!allocate_fast_block<ObjectSize>(next_block_size_.load(std::memory_order_relaxed)))
            {
                return nullptr;
            }

            update_next_block_size();

            // Re-acquire lock
            fast_pool_mutex_.lock();
        }

        return pool->back().allocate(alloc_size);
    }

    [[nodiscard]]
    pointer allocate_using_free_list(std::size_t alloc_size, std::size_t align)
    {
        std::lock_guard<std::mutex> lock(free_list_mutex_);

        if (free_list_.empty())
        {
            return nullptr;
        }

        for (size_t i = 0; i < free_list_.size(); ++i)
        {
            FreeListRegion& reg = free_list_[i];

            std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(reg.ptr_);
            std::uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
            std::size_t padding = aligned - addr;

            if (reg.size_ >= alloc_size + padding)
            {
                pointer result = reinterpret_cast<pointer>(aligned);

                // Split region if there's leftover
                std::size_t used = alloc_size + padding;
                if (reg.size_ > used)
                {
                    pointer remainder = reg.ptr_ + used;
                    std::size_t remainder_size = reg.size_ - used;
                    free_list_.push_back(FreeListRegion(remainder, remainder_size));
                }

                free_list_.erase(free_list_.begin() + i);
                return result;
            }
        }

        return nullptr;
    }

    [[nodiscard]]
    pointer allocate_from_blocks(std::size_t alloc_size, std::size_t align)
    {
        if (align == 0 || (align & (align - 1)) != 0)
        {
            std::cerr << "Invalid alignment: " << align << std::endl;
            return nullptr;
        }

        // Try to allocate from existing block (with shared lock for reading)
        {
            std::shared_lock<std::shared_mutex> lock(blocks_mutex_);

            if (!blocks_.empty())
            {
                // Try free list first (will acquire its own lock)
                pointer mem = allocate_using_free_list(alloc_size, align);
                if (mem)
                {
                    return mem;
                }

                // Try current block (blocks are thread-safe internally)
                mem = blocks_.back().allocate(alloc_size, align);
                if (mem)
                {
                    return mem;
                }
            }
        }

        // Need a new block - this will acquire exclusive lock
        std::size_t new_block_size = std::max(alloc_size, next_block_size_.load(std::memory_order_relaxed));
        if (!allocate_block(new_block_size, align))
        {
            std::cerr << "-- Failed to allocate block : ArenaAllocator::allocate_block()" << std::endl;
            return nullptr;
        }

        update_next_block_size();

        // Allocate from the newly created block
        std::shared_lock<std::shared_mutex> lock(blocks_mutex_);
        return blocks_.back().allocate(alloc_size, align);
    }

    void update_next_block_size() noexcept
    {
        if (growth_factor_.load(std::memory_order_relaxed) == GrowthStrategy::EXPONENTIAL)
        {
            std::size_t current = next_block_size_.load(std::memory_order_relaxed);
            std::size_t max_size = max_block_size_.load(std::memory_order_relaxed);
            std::size_t new_size = std::min(current * 2, max_size);
            next_block_size_.store(new_size, std::memory_order_relaxed);
        }
    }

    // Unsafe version - caller must hold fast_pool_mutex_
    template<std::size_t ObjectSize>
    [[nodiscard]]
    std::vector<LockFreeFastAllocBlock<ObjectSize>>* choose_pool_unsafe() noexcept
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
};

}  // namespace allocator
}  // namespace runtime
}  // namespace mylang
