#pragma once

#include "../../macros.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
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

class ArenaBlock
{
   private:
    std::size_t size_{DEFAULT_BLOCK_SIZE};
    pointer begin_{nullptr};
    std::atomic<pointer> next_{nullptr};
    mutable std::mutex mutex_;

   public:
    explicit ArenaBlock(const std::size_t size = DEFAULT_BLOCK_SIZE, const std::size_t alignment = alignof(std::max_align_t));

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

    ArenaBlock(ArenaBlock&& other) noexcept;

    ArenaBlock& operator=(ArenaBlock&& other) noexcept;

    pointer begin() const;
    pointer end() const;
    pointer cnext() const;
    pointer& next();

    std::size_t size() const;
    std::size_t remaining() const;

    pointer allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t));

    pointer reserve(const std::size_t bytes);
};

template<std::size_t ObjectSize>
class LockFreeFastAllocBlock
{
   private:
    std::size_t size_{DEFAULT_BLOCK_SIZE};
    pointer begin_{nullptr};
    std::atomic<pointer> next_{nullptr};

   public:
    LockFreeFastAllocBlock() = default;

    explicit LockFreeFastAllocBlock(std::size_t size);

    ~LockFreeFastAllocBlock()
    {
        if (begin_)
        {
            std::free(begin_);
            begin_ = nullptr;
            next_ = nullptr;
        }
    }

    LockFreeFastAllocBlock(const LockFreeFastAllocBlock&) = delete;
    LockFreeFastAllocBlock& operator=(const LockFreeFastAllocBlock&) = delete;

    LockFreeFastAllocBlock(LockFreeFastAllocBlock&& other) noexcept;

    LockFreeFastAllocBlock& operator=(LockFreeFastAllocBlock&& other) noexcept;

    pointer begin() const;
    pointer end() const;
    pointer cnext() const;
    pointer& next();

    std::size_t size() const;
    std::size_t remaining() const;

    [[nodiscard]]
    pointer allocate(std::size_t size);
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
};

template<std::size_t ObjectSize>
class FastAllocBlockFreeList
{
   private:
    std::vector<FreeListRegion> regions_;
    mutable std::mutex mutex_;

   public:
    FastAllocBlockFreeList() = default;

    explicit FastAllocBlockFreeList(const std::vector<FreeListRegion>& regions) :
        regions_(regions)
    {
    }

    FastAllocBlockFreeList(FastAllocBlockFreeList&& other) noexcept;

    std::vector<FreeListRegion>& regions() noexcept;

    void append(const FreeListRegion& reg);

    void pop();

    void remove(std::size_t at);

    bool empty() const;

    void clear();

    std::optional<FreeListRegion> find_and_extract(std::size_t size, std::size_t align);
};

class ArenaAllocator
{
   public:
    enum class GrowthStrategy : int { LINEAR, EXPONENTIAL };
    using OutOfMemoryHandler = std::function<bool(std::size_t requested)>;

   private:
    ArenaAllocStats alloc_stats_;

    std::vector<ArenaBlock> blocks_{};
    mutable std::shared_mutex blocks_mutex_;

    std::vector<LockFreeFastAllocBlock<8>> fast_pool8_{};
    std::vector<LockFreeFastAllocBlock<16>> fast_pool16_{};
    std::vector<LockFreeFastAllocBlock<32>> fast_pool32_{};
    std::vector<LockFreeFastAllocBlock<64>> fast_pool64_{};
    std::vector<LockFreeFastAllocBlock<128>> fast_pool128_{};
    std::vector<LockFreeFastAllocBlock<256>> fast_pool256_{};
    mutable std::mutex fast_pool_mutex_;  // Single mutex for all fast pools


    // BUG FIX #1: Correct template parameters
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
    mutable std::mutex free_list_mutex_;  // Protect OOM handler calls

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

    std::size_t min_alignment_{std::alignment_of<std::max_align_t>::value};
    std::atomic<std::size_t> max_block_size_{MAX_BLOCK_SIZE};

   public:
    ArenaAllocator(int growth_strategy = static_cast<int>(GrowthStrategy::EXPONENTIAL),
      std::size_t min_align = std::alignment_of<std::max_align_t>::value,
      OutOfMemoryHandler oom_handler = nullptr,
      bool debug = false);

    ~ArenaAllocator() { reset(); }

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    ArenaAllocator(ArenaAllocator&&) noexcept = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) noexcept = delete;

    void reset();

    std::size_t total_allocated() const;

    std::size_t total_allocations() const;

    std::size_t active_blocks() const;

    [[nodiscard]]
    pointer allocate_block(std::size_t requested, std::size_t alignment_ = alignof(std::max_align_t));

    template<std::size_t ObjectSize>
    [[nodiscard]]
    pointer allocate_fast_block(std::size_t size);

    template<typename _Tp>
    [[nodiscard]]
    _Tp* allocate(std::size_t count = 1);

    template<typename _Tp>
    void deallocate(_Tp* ptr, std::size_t count = 1);

    [[nodiscard]]
    bool verify_allocation(void* ptr) const;

   private:
    template<std::size_t ObjectSize>
    [[nodiscard]]
    pointer allocate_from_fast_pool(std::size_t alloc_size);

    template<std::size_t ObjectSize>
    [[nodiscard]]
    pointer allocate_using_fast_free_list(std::size_t alloc_size);

    [[nodiscard]]
    pointer allocate_using_free_list(std::size_t alloc_size, std::size_t align);

    [[nodiscard]]
    pointer allocate_from_blocks(std::size_t alloc_size, std::size_t align);

    void update_next_block_size() noexcept;

    template<std::size_t ObjectSize>
    [[nodiscard]]
    std::vector<LockFreeFastAllocBlock<ObjectSize>>* choose_pool() noexcept;

    template<std::size_t ObjectSize>
    [[nodiscard]]
    FastAllocBlockFreeList<ObjectSize>* choose_pool_free_list() noexcept;
};

}  // namespace allocator
}  // namespace runtime
}  // namespace mylang
