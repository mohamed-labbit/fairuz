#ifndef META_HPP
#define META_HPP

#ifdef fairuz_DEBUG

#    include "../../macros.hpp"

#    include <atomic>

namespace fairuz {

struct AllocationHeader {
    uint_fast32_t magic;
    uint_fast32_t m_size;
    uint_fast32_t m_alignment;
    uint_fast32_t checksum;
    std::chrono::steady_clock::time_point timestamp;
    char32_t const* TypeName;
    uint_fast32_t LineNumber;
    char32_t const* filename;
    static constexpr u32 MAGIC = 0xDEADC0DE;

    u32 compute_checksum() const { return magic ^ m_size ^ m_alignment; }

    bool is_valid() const { return magic == MAGIC && checksum == compute_checksum(); }
}; // struct AllocationHeader

struct AllocationFooter {
    u32 guard;

    static constexpr u32 GUARD = 0xFEEDFACE;

    bool is_valid() const { return guard == GUARD; }
}; // struct AllocationFooter

} // namespace fairuz

#endif // fairuz_DEBUG

#endif // META_HPP
