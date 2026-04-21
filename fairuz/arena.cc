// arena.cc

#include "arena.hpp"

#include <sys/mman.h>

namespace fairuz {

using ErrorCode = diagnostic::errc::general::Code;

Fa_ArenaBlock::Fa_ArenaBlock(size_t const size, size_t const alignment)
    : m_size(size)
{
    (void)alignment; // silence no-use

    m_begin = reinterpret_cast<unsigned char*>(mmap(reinterpret_cast<void*>(0x200000000ULL), m_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    if (m_begin == MAP_FAILED)
        diagnostic::emit(ErrorCode::MMAP_FAILED, diagnostic::Severity::FATAL);

    if (reinterpret_cast<uintptr_t>(m_begin) > UINT64_C(0x0000FFFFFFFFFFFF)) {
        munmap(m_begin, m_size);
        m_begin = nullptr;
        diagnostic::emit(ErrorCode::NANBOX_ADDRESS_UNSAFE, diagnostic::Severity::FATAL);
    }

    m_next = m_begin;
    m_end = m_begin + m_size;
}

Fa_ArenaBlock::Fa_ArenaBlock(Fa_ArenaBlock&& other) noexcept
    : m_size(other.m_size)
    , m_begin(other.m_begin)
    , m_next(other.m_next)
    , m_end(other.m_end)
{
    other.m_size = 0;
    other.m_begin = nullptr;
    other.m_next = nullptr;
    other.m_end = nullptr;
}

Fa_ArenaBlock::~Fa_ArenaBlock()
{
    if (m_begin != nullptr) {
        munmap(m_begin, m_size);
        m_begin = nullptr;
        m_next = nullptr;
        m_end = nullptr;
    }
}

Fa_ArenaBlock& Fa_ArenaBlock::operator=(Fa_ArenaBlock&& other) noexcept
{
    if (this != &other) {
        if (m_begin != nullptr)
            munmap(m_begin, m_size);

        m_size = other.m_size;
        m_begin = other.m_begin;
        m_next = other.m_next;
        m_end = other.m_end;

        other.m_begin = nullptr;
        other.m_next = nullptr;
        other.m_end = nullptr;
        other.m_size = 0;
    }
    return *this;
}

unsigned char* Fa_ArenaBlock::begin() const { return m_begin; }

unsigned char* Fa_ArenaBlock::end() const { return m_end; }

unsigned char* Fa_ArenaBlock::next() const { return m_next; }

size_t Fa_ArenaBlock::size() const { return m_size; }

size_t Fa_ArenaBlock::used() const
{
    if (m_begin == nullptr || m_next < m_begin)
        return 0;

    return static_cast<size_t>(m_next - m_begin);
}

size_t Fa_ArenaBlock::remaining() const
{
    if (m_begin == nullptr)
        return 0;

    return static_cast<size_t>(m_end - m_next);
}

bool Fa_ArenaBlock::pop(size_t bytes)
{
    if (m_begin == nullptr || m_next < m_begin + bytes)
        return false;

    m_next -= bytes;
    return true;
}

unsigned char* Fa_ArenaBlock::allocate(size_t bytes, std::optional<size_t> alignment)
{
    if (m_begin == nullptr || bytes == 0)
        return nullptr;

    size_t align = alignment.value_or(alignof(std::max_align_t));

    uintptr_t cur = reinterpret_cast<uintptr_t>(m_next);
    uintptr_t aligned = (cur + align - 1) & ~(align - 1);
    uintptr_t next_addr = aligned + bytes;

    if (UNLIKELY(next_addr > reinterpret_cast<uintptr_t>(m_end)))
        return nullptr;

    m_next = reinterpret_cast<unsigned char*>(next_addr);
    return reinterpret_cast<unsigned char*>(aligned);
}

unsigned char* Fa_ArenaBlock::reserve(size_t const bytes)
{
    if (m_begin == nullptr || bytes == 0)
        return nullptr;
    if (static_cast<size_t>(m_end - m_next) < bytes)
        return nullptr;

    m_next += bytes;
    return m_next;
}

Fa_ArenaAllocator::Fa_ArenaAllocator(GrowthStrategy growth_strategy, OutOfMemoryHandler oom_handler, bool /*debug*/)
    : m_growth_factor(growth_strategy)
    , m_oom_handler(oom_handler)
#ifdef FAIRUZ_DEBUG
    , m_debug_features(debug)
#endif
{
#ifdef FAIRUZ_DEBUG
    if (m_debug_features) {
        m_track_allocations = true;
        m_enable_statistics = true;
        m_alloc_stats.reset();
    }
#endif
}

void Fa_ArenaAllocator::reset()
{
    m_blocks.clear();
    m_last_ptr = nullptr;
    m_last_size = 0;
    m_last_consumed = 0;
    m_next = nullptr;
    m_end = nullptr;

#ifdef FAIRUZ_DEBUG
    if (m_track_allocations)
        m_allocation_map.clear();

    m_allocated_ptrs.clear();
    m_alloc_stats.reset();
#endif

    m_next_block_size = m_block_size;

    allocate_block(m_next_block_size, alignof(std::max_align_t));
}

void Fa_ArenaAllocator::set_name(std::string const& name) { m_name = name; }

#ifdef FAIRUZ_DEBUG
size_t Fa_ArenaAllocator::total_allocated() const { return m_alloc_stats.TotalAllocated; }
size_t Fa_ArenaAllocator::total_allocations() const { return m_alloc_stats.TotalAllocations; }
size_t Fa_ArenaAllocator::active_blocks() const { return m_alloc_stats.ActiveBlocks; }
DetailedAllocStats const& Fa_ArenaAllocator::stats() const { return m_alloc_stats; }
#endif

void* Fa_ArenaAllocator::allocate(size_t const size, size_t const alignment)
{
#ifdef FAIRUZ_DEBUG
    auto alloc_started_at = std::chrono::steady_clock::now();
#endif

    if (UNLIKELY(size == 0)) {
#ifdef FAIRUZ_DEBUG
        if (m_enable_statistics)
            m_alloc_stats.ZeroByteRequests += 1;
#endif
        return nullptr;
    }

    if (UNLIKELY(size > MAX_BLOCK_SIZE)) {
#ifdef FAIRUZ_DEBUG
        if (m_enable_statistics)
            m_alloc_stats.AllocationFailures += 1;
#endif
        diagnostic::emit(ErrorCode::ALLOC_FAILED, "allocation m_size is too large: " + std::to_string(size));
        diagnostic::internal_error(ErrorCode::ALLOC_FAILED);
    }

    uintptr_t cur = reinterpret_cast<uintptr_t>(m_next);
    uintptr_t aligned = (cur + alignment - 1) & ~(alignment - 1);
    uintptr_t next_addr = aligned + size;

    if (LIKELY(next_addr <= reinterpret_cast<uintptr_t>(m_end))) {
        m_next = reinterpret_cast<unsigned char*>(next_addr);
        m_last_ptr = reinterpret_cast<void*>(aligned);
        m_last_size = size;
        m_last_consumed = static_cast<size_t>(next_addr - cur);

#ifdef FAIRUZ_DEBUG
        track_allocation(reinterpret_cast<unsigned char*>(aligned), size, m_last_consumed, alignment);
        if (m_enable_statistics) {
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - alloc_started_at).count();
            m_alloc_stats.record_alloc_time(static_cast<u64>(duration));
        }
#endif
        return reinterpret_cast<void*>(aligned);
    }

    void* ptr = allocate_slow(size, alignment);

#ifdef FAIRUZ_DEBUG
    if (ptr == nullptr) {
        if (m_enable_statistics)
            m_alloc_stats.AllocationFailures += 1;
    } else if (m_enable_statistics) {
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - alloc_started_at).count();
        m_alloc_stats.record_alloc_time(static_cast<u64>(duration));
    }
#endif

    return ptr;
}

void* Fa_ArenaAllocator::allocate_slow(size_t size, size_t alignment)
{
    size_t block_size = std::max(size + alignment, m_next_block_size);
    auto record_new_block = [this]() {
#ifdef FAIRUZ_DEBUG
        if (!m_enable_statistics)
            return;

        m_alloc_stats.ActiveBlocks = static_cast<u64>(m_blocks.size());
        m_alloc_stats.BlockAllocations += 1;
        if (m_blocks.size() > 1)
            m_alloc_stats.BlockExpansions += 1;
#endif
    };

    if (block_size > m_max_block_size) {
        if (m_oom_handler && m_oom_handler(block_size)) {
            // OOM handler freed something — retry once
            block_size = std::max(size + alignment, m_next_block_size);
            if (block_size > m_max_block_size)
                return nullptr;
        } else {
            return nullptr;
        }
    }

    try {
        m_blocks.emplace_back(block_size, alignment);
        record_new_block();
    } catch (std::bad_alloc const&) {
        if (m_oom_handler && m_oom_handler(block_size)) {
            {
                try {
                    m_blocks.emplace_back(block_size, alignment);
                    record_new_block();
                } catch (...) {
                    return nullptr;
                }
            }
        } else {
            return nullptr;
        }
    }

    Fa_ArenaBlock& blk = m_blocks.back();
    m_next = blk.begin();
    m_end = blk.end();

    uintptr_t cur = reinterpret_cast<uintptr_t>(m_next);
    uintptr_t aligned = (cur + alignment - 1) & ~(alignment - 1);
    uintptr_t next_addr = aligned + size;

    if (UNLIKELY(next_addr > reinterpret_cast<uintptr_t>(m_end)))
        return nullptr;

    m_next = reinterpret_cast<unsigned char*>(next_addr);
    m_last_ptr = reinterpret_cast<void*>(aligned);
    m_last_size = size;
    m_last_consumed = static_cast<size_t>(next_addr - cur);

#ifdef FAIRUZ_DEBUG
    track_allocation(reinterpret_cast<unsigned char*>(aligned), size, m_last_consumed, alignment);
#endif
    return reinterpret_cast<void*>(aligned);
}

void Fa_ArenaAllocator::deallocate(void* ptr, size_t const size)
{
#ifdef FAIRUZ_DEBUG
    auto dealloc_started_at = std::chrono::steady_clock::now();
#endif

    if (ptr == nullptr || size == 0 || m_blocks.empty())
        return;

#ifdef FAIRUZ_DEBUG
    AllocationHeader header { ptr, size, size, m_alignment };

    if (m_track_allocations) {
        auto it = m_allocation_map.find(ptr);
        if (it == m_allocation_map.end()) {
            if (m_enable_statistics) {
                if (m_allocated_ptrs.contains(ptr))
                    m_alloc_stats.DoubleFreeAttempts += 1;
                else
                    m_alloc_stats.InvalidFreeAttempts += 1;
            }
            return;
        }

        header = it->second;
        if (header.size != size) {
            if (m_enable_statistics)
                m_alloc_stats.InvalidFreeAttempts += 1;
            return;
        }
    }
#endif

    auto expected = static_cast<unsigned char*>(ptr);
    auto last = static_cast<unsigned char*>(m_last_ptr);

    if (expected != last) {
#ifdef FAIRUZ_DEBUG
        if (m_enable_statistics)
            m_alloc_stats.InvalidFreeAttempts += 1;
#endif
        return;
    }

    if (size != m_last_size) {
#ifdef FAIRUZ_DEBUG
        if (m_enable_statistics)
            m_alloc_stats.InvalidFreeAttempts += 1;
#endif
        return;
    }

    Fa_ArenaBlock& block = m_blocks.back();
    size_t bytes_to_pop = m_last_consumed;
#ifdef FAIRUZ_DEBUG
    if (m_track_allocations)
        bytes_to_pop = static_cast<size_t>(header.consumed);
#endif
    if (!block.pop(bytes_to_pop)) {
#ifdef FAIRUZ_DEBUG
        if (m_enable_statistics)
            m_alloc_stats.InvalidFreeAttempts += 1;
#endif
        return;
    }

    m_next = block.next();
    m_last_ptr = nullptr;
    m_last_size = 0;
    m_last_consumed = 0;

#ifdef FAIRUZ_DEBUG
    if (m_track_allocations)
        m_allocation_map.erase(ptr);

    if (m_enable_statistics) {
        if (m_alloc_stats.CurrentlyAllocated >= header.size)
            m_alloc_stats.CurrentlyAllocated -= header.size;
        else
            m_alloc_stats.CurrentlyAllocated = 0;

        m_alloc_stats.TotalDeallocations += 1;
        m_alloc_stats.TotalDeallocated += header.size;

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - dealloc_started_at).count();
        m_alloc_stats.TotalDeallocTimeNs += static_cast<u64>(duration);
    }
#endif
}

unsigned char* Fa_ArenaAllocator::allocate_block(size_t requested, size_t alignment, bool retry_on_oom)
{
    size_t block_size = std::max(requested + alignment, m_next_block_size);

    if (block_size > m_max_block_size) {
        if (retry_on_oom && m_oom_handler && m_oom_handler(block_size))
            return allocate_block(requested, alignment, false);

        return nullptr;
    }

    try {
        m_blocks.emplace_back(block_size, alignment);
#ifdef FAIRUZ_DEBUG
        if (m_enable_statistics) {
            m_alloc_stats.ActiveBlocks = static_cast<u64>(m_blocks.size());
            m_alloc_stats.BlockAllocations += 1;
            if (m_blocks.size() > 1)
                m_alloc_stats.BlockExpansions += 1;
        }
#endif
        Fa_ArenaBlock& blk = m_blocks.back();
        m_next = blk.begin();
        m_end = blk.end();
        return blk.begin();
    } catch (std::bad_alloc const&) {
        if (retry_on_oom && m_oom_handler && m_oom_handler(block_size))
            return allocate_block(requested, alignment, false);

        return nullptr;
    }
}

unsigned char* Fa_ArenaAllocator::allocate_from_blocks(size_t alloc_size, size_t align)
{
    if (!m_blocks.empty()) {
        unsigned char* mem = m_blocks.back().allocate(alloc_size, align);
        if (mem != nullptr) {
            m_next = m_blocks.back().next();
            return mem;
        }
    }

    size_t new_block_size = std::max(alloc_size, m_next_block_size);
    if (allocate_block(new_block_size, align) == nullptr) {
#ifdef FAIRUZ_DEBUG
        if (m_debug_features)
            std::cerr << "-- Failed to allocate block : Fa_ArenaAllocator::allocate_block()" << std::endl;
#endif
        return nullptr;
    }

    unsigned char* mem = m_blocks.back().allocate(alloc_size, align);
    if (mem != nullptr)
        m_next = m_blocks.back().next();

    return mem;
}

#ifdef FAIRUZ_DEBUG
void Fa_ArenaAllocator::track_allocation(unsigned char* ptr, size_t m_size, size_t consumed, size_t m_alignment)
{
    if (m_track_allocations) {
        AllocationHeader header { };
        header.ptr = ptr;
        header.size = static_cast<u64>(m_size);
        header.consumed = static_cast<u64>(consumed);
        header.alignment = static_cast<u64>(m_alignment);
        m_allocation_map[ptr] = header;
        m_allocated_ptrs.insert(ptr);
    }

    if (m_enable_statistics) {
        m_alloc_stats.TotalAllocations += 1;
        m_alloc_stats.TotalAllocated += m_size;
        m_alloc_stats.CurrentlyAllocated += m_size;
        m_alloc_stats.AlignmentAdjustments += static_cast<u64>(consumed - m_size);
        m_alloc_stats.record_allocation_size(m_size);
        m_alloc_stats.update_peak(m_alloc_stats.CurrentlyAllocated);
    }
}

bool Fa_ArenaAllocator::verify_allocation(void* ptr) const
{
    if (!m_track_allocations)
        return true;

    auto it = m_allocation_map.find(ptr);
    if (it == m_allocation_map.end())
        return false;

    return it->second.ptr == ptr && it->second.size > 0 && it->second.consumed >= it->second.size;
}

std::string Fa_ArenaAllocator::to_string(bool verbose) const
{
    std::ostringstream oss;
    dump_stats(oss, verbose);
    return oss.str();
}

void Fa_ArenaAllocator::dump_stats(std::ostream& os, bool verbose) const
{
    StatsPrinter printer(m_alloc_stats, m_name);
    printer.print_detailed(os, verbose);
}
#endif // FAIRUZ_DEBUG

} // namespace fairuz
