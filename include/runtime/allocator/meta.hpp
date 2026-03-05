#ifndef META_HPP
#define META_HPP

#include "../../macros.hpp"

#include <atomic>

namespace mylang {
namespace runtime {
namespace allocator {

struct AllocationHeader {
    uint_fast32_t magic;
    uint_fast32_t size;
    uint_fast32_t alignment;
    uint_fast32_t checksum;
    std::chrono::steady_clock::time_point timestamp;
    char32_t const* TypeName;
    uint_fast32_t LineNumber;
    char32_t const* filename;
    static constexpr uint32_t MAGIC = 0xDEADC0DE;

    uint32_t compute_checksum() const
    {
        return magic ^ size ^ alignment;
    }

    bool is_valid() const
    {
        return magic == MAGIC && checksum == compute_checksum();
    }
};

struct AllocationFooter {
    uint32_t guard;

    static constexpr uint32_t GUARD = 0xFEEDFACE;

    bool is_valid() const
    {
        return guard == GUARD;
    }
};

}
}
}

#endif // META_HPP