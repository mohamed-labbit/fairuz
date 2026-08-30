//
// farena.cc
//

#include "farena.hpp"
#include "fdiagnostic.hpp"

#include <sys/mman.h>

namespace fairuz {

using GeneralErrorCode = diagnostic::errc::general::Code;

Fa_ArenaBlock::Fa_ArenaBlock(size_t const size, size_t const alignment)
    : m_size(size)
{
    (void)alignment; // silence no-use

    m_begin = reinterpret_cast<unsigned char*>(mmap(reinterpret_cast<void*>(0x200000000ULL), m_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    if (m_begin == MAP_FAILED)
        diagnostic::panic(GeneralErrorCode::MMAP_FAILED);

    if (reinterpret_cast<uintptr_t>(m_begin) > UINT64_C(0x0000FFFFFFFFFFFF)) {
        munmap(m_begin, m_size);
        m_begin = nullptr;
        diagnostic::panic(GeneralErrorCode::NANBOX_ADDRESS_UNSAFE);
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

Fa_ArenaAllocator::Fa_ArenaAllocator(OutOfMemoryHandler oom_handler)
    : m_oom_handler(oom_handler)
{
}

void Fa_ArenaAllocator::reset()
{
    m_blocks.clear();
    m_last_ptr = nullptr;
    m_last_size = 0;
    m_last_consumed = 0;
    m_next = nullptr;
    m_end = nullptr;
    m_next_block_size = m_block_size;

    allocate_block(m_next_block_size, alignof(std::max_align_t));
}

void Fa_ArenaAllocator::set_name(std::string const& name) { m_name = name; }

void* Fa_ArenaAllocator::allocate(size_t const size, size_t const alignment)
{
    if (UNLIKELY(size == 0))
        return nullptr;

    if (UNLIKELY(size > MAX_BLOCK_SIZE))
        diagnostic::panic(GeneralErrorCode::ALLOC_FAILED, "allocation size is too large: " + std::to_string(size));

    uintptr_t cur = reinterpret_cast<uintptr_t>(m_next);
    uintptr_t aligned = (cur + alignment - 1) & ~(alignment - 1);
    uintptr_t next_addr = aligned + size;

    if (LIKELY(next_addr <= reinterpret_cast<uintptr_t>(m_end))) {
        m_next = reinterpret_cast<unsigned char*>(next_addr);
        m_last_ptr = reinterpret_cast<void*>(aligned);
        m_last_size = size;
        m_last_consumed = static_cast<size_t>(next_addr - cur);

        return reinterpret_cast<void*>(aligned);
    }

    void* ptr = allocate_slow(size, alignment);
    if (ptr == nullptr)
        diagnostic::panic(diagnostic::errc::general::Code::ALLOC_FAILED);

    return ptr;
}

void* Fa_ArenaAllocator::allocate_slow(size_t size, size_t alignment)
{
    size_t block_size = std::max(size + alignment, m_next_block_size);

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
    } catch (std::bad_alloc const&) {
        if (m_oom_handler && m_oom_handler(block_size)) {
            {
                try {
                    m_blocks.emplace_back(block_size, alignment);
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

    return reinterpret_cast<void*>(aligned);
}

void Fa_ArenaAllocator::deallocate(void* ptr, size_t const size)
{
    if (ptr == nullptr || size == 0 || m_blocks.empty())
        return;

    auto expected = static_cast<unsigned char*>(ptr);
    auto last = static_cast<unsigned char*>(m_last_ptr);

    if (expected != last)
        return;

    if (size != m_last_size)
        return;

    Fa_ArenaBlock& block = m_blocks.back();
    size_t bytes_to_pop = m_last_consumed;
    if (!block.pop(bytes_to_pop))
        return;

    m_next = block.next();
    m_last_ptr = nullptr;
    m_last_size = 0;
    m_last_consumed = 0;
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
    if (allocate_block(new_block_size, align) == nullptr)
        return nullptr;

    unsigned char* mem = m_blocks.back().allocate(alloc_size, align);
    if (mem != nullptr)
        m_next = m_blocks.back().next();

    return mem;
}

} // namespace fairuz
