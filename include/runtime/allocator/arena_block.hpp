#pragma once

#include "../../diag/diagnostic.hpp"
#include "../../macros.hpp"

#include <atomic>
#include <mutex>
#include <optional>

namespace mylang {
namespace runtime {
namespace allocator {

class ArenaBlock {
private:
    std::size_t Size_ { DEFAULT_BLOCK_SIZE };
    unsigned char* Begin_ { nullptr };
    unsigned char* Next_ { nullptr };
    mutable std::mutex Mutex_;

public:
    explicit ArenaBlock(std::size_t const size = DEFAULT_BLOCK_SIZE, std::size_t const alignment = alignof(std::max_align_t));

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

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        return Size_;
    }

    std::size_t used() const
    {
        if (!Begin_ || Next_ < Begin_)
            return 0;

        return static_cast<std::size_t>(Next_ - Begin_);
    }

    bool pop(std::size_t bytes)
    {
        if (!Begin_ || Next_ < Begin_ + bytes)
            return false;

        Next_ -= bytes;
        return true;
    }

    std::size_t remaining() const
    {
        std::lock_guard<std::mutex> lock(Mutex_);
        if (!Begin_)
            return 0;

        unsigned char* current_next = Next_;
        return static_cast<std::size_t>(Begin_ + Size_ - current_next);
    }

    unsigned char* allocate(std::size_t bytes, std::optional<std::size_t> alignment = std::nullopt);

    unsigned char* reserve(std::size_t const bytes);
}; // ArenaBlock

}
}
}
