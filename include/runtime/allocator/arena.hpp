#ifndef ARENA_HPP
#define ARENA_HPP

#include "../../diagnostic.hpp"
#include "meta.hpp"
#include "stats.hpp"

#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>

namespace mylang::runtime::allocator {

class ArenaBlock {
private:
    size_t Size_ { DEFAULT_BLOCK_SIZE };
    unsigned char* Begin_ { nullptr };
    unsigned char* Next_ { nullptr };
    mutable std::mutex Mutex_;

public:
    explicit ArenaBlock(size_t const size = DEFAULT_BLOCK_SIZE, size_t const alignment = alignof(std::max_align_t));

    ~ArenaBlock();

    // Non-copyable
    ArenaBlock(ArenaBlock const&) = delete;
    ArenaBlock& operator=(ArenaBlock const&) = delete;

    ArenaBlock(ArenaBlock&& other) noexcept;

    ArenaBlock& operator=(ArenaBlock&& other) noexcept;

    unsigned char* begin() const
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        return Begin_;
    }

    unsigned char* end() const
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        return Begin_ + Size_;
    }

    unsigned char* cNext() const
    {
        return Next_;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        return Size_;
    }

    size_t used() const
    {
        if (!Begin_ || Next_ < Begin_)
            return 0;

        return static_cast<size_t>(Next_ - Begin_);
    }

    bool pop(size_t bytes)
    {
        if (!Begin_ || Next_ < Begin_ + bytes)
            return false;

        Next_ -= bytes;
        return true;
    }

    size_t remaining() const
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        if (!Begin_)
            return 0;

        unsigned char const* current_next = Next_;
        return static_cast<size_t>(Begin_ + Size_ - current_next);
    }

    unsigned char* allocate(size_t bytes, std::optional<size_t> alignment = std::nullopt);

    unsigned char* reserve(size_t const bytes);
}; // ArenaBlock

class ArenaAllocator {
public:
    enum class GrowthStrategy : std::int32_t {
        LINEAR,
        EXPONENTIAL
    };

    using OutOfMemoryHandler = std::function<bool(size_t requested)>;

private:
    struct VoidPtrHash {
        size_t operator()(void const* ptr) const noexcept
        {
            return std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(ptr));
        }
    };

    struct VoidPtrEqual {
        bool operator()(void const* a, void const* b) const noexcept
        {
            return a == b;
        }
    };

    DetailedAllocStats AllocStats_;
    std::vector<ArenaBlock> Blocks_ { };
    mutable std::shared_mutex BlocksMutex_;
    GrowthStrategy GrowthFactor_ { GrowthStrategy::LINEAR };
    size_t BlockSize_ { DEFAULT_BLOCK_SIZE };
    size_t NextBlockSize_ { DEFAULT_BLOCK_SIZE };
    std::string Name_ { "arena" };
    OutOfMemoryHandler OomHandler_ { nullptr };
    mutable std::mutex OomHandlerMutex_;
    std::unordered_map<void*, AllocationHeader, VoidPtrHash, VoidPtrEqual> AllocationMap_ { };
    mutable std::shared_mutex AllocationMapMutex_;
    std::unordered_set<void*, VoidPtrHash, VoidPtrEqual> AllocatedPtrs_ { };
    mutable std::shared_mutex AllocatedPtrsMutex_;
    bool TrackAllocations_ { false };
    bool DebugFeatures_ { false };
    bool EnableStatistics_ { true };
    size_t MaxBlockSize_ { MAX_BLOCK_SIZE };
    void* LastPtr_ { nullptr };

    static constexpr size_t Alignment_ = alignof(std::max_align_t);

public:
    explicit ArenaAllocator(std::int32_t growth_strategy = static_cast<std::int32_t>(GrowthStrategy::EXPONENTIAL), OutOfMemoryHandler oom_handler = nullptr,
        bool debug = true);

    ~ArenaAllocator()
    {
        reset();
    }

    ArenaAllocator(ArenaAllocator const&) = delete;
    ArenaAllocator& operator=(ArenaAllocator const&) = delete;

    ArenaAllocator(ArenaAllocator&&) noexcept = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) noexcept = delete;

    void setName(std::string const& name)
    {
        Name_ = name;
    }

    void reset();

    size_t totalAllocated() const
    {
        return AllocStats_.TotalAllocated;
    }

    size_t totalAllocations() const
    {
        return AllocStats_.TotalAllocations;
    }

    size_t activeBlocks() const
    {
        return AllocStats_.ActiveBlocks;
    }

    MY_NODISCARD unsigned char* allocateBlock(size_t requested, size_t alignment_ = alignof(std::max_align_t), bool retry_on_oom = true);

    MY_NODISCARD void* allocate(size_t const size, size_t const alignment = alignof(std::max_align_t));

    void deallocate(void* ptr, size_t const size);

    MY_NODISCARD bool verifyAllocation(void* ptr) const;

    std::string toString(bool verbose) const
    {
        std::ostringstream oss;
        dumpStats(oss, verbose);
        return oss.str();
    }

    void dumpStats(std::ostream& os, bool verbose) const
    {
        StatsPrinter printer(AllocStats_, Name_);
        printer.printDetailed(os, verbose);
    }

private:
    MY_NODISCARD unsigned char* allocateFromBlocks(size_t alloc_size, size_t align = alignof(std::max_align_t));

    void updateNextBlockSize() noexcept;

    static constexpr size_t getAligned(size_t n, size_t const alignment = alignof(std::max_align_t)) noexcept
    {
        return (n + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace mylang::runtime::allocator

#endif // ARENA_HPP
