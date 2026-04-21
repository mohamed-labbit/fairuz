#ifndef STATS_HPP
#define STATS_HPP

#include "macros.hpp"

#ifdef FAIRUZ_DEBUG

#    include <atomic>
#    include <chrono>
#    include <cstdint>
#    include <iomanip>
#    include <iostream>
#    include <sstream>
#    include <string>
#    include <vector>

namespace fairuz {

struct DetailedAllocStats {
    u64 TotalAllocations { 0 };
    u64 TotalDeallocations { 0 };
    u64 TotalAllocated { 0 };
    u64 TotalDeallocated { 0 };
    u64 CurrentlyAllocated { 0 };
    u64 PeakAllocated { 0 };
    u64 ActiveBlocks { 0 };

    u64 Small8ByteAllocs { 0 };
    u64 Small16ByteAllocs { 0 };
    u64 Small32ByteAllocs { 0 };
    u64 Small64ByteAllocs { 0 };
    u64 Small128ByteAllocs { 0 };
    u64 Small256ByteAllocs { 0 };

    u64 Medium512ByteAllocs { 0 };
    u64 Medium1KBAllocs { 0 };
    u64 Medium2KBAllocs { 0 };
    u64 Medium4KBAllocs { 0 };

    u64 Large8KBAllocs { 0 };
    u64 Large16KBAllocs { 0 };
    u64 Large32KBAllocs { 0 };
    u64 LargeHugeAllocs { 0 };

    u64 BlockAllocations { 0 };
    u64 BlockExpansions { 0 };

    u64 InternalFragmentation { 0 };
    u64 WastedCapacity { 0 };

    u64 AllocationFailures { 0 };
    u64 DoubleFreeAttempts { 0 };
    u64 InvalidFreeAttempts { 0 };
    u64 AlignmentAdjustments { 0 };

    u64 TotalAllocTimeNs { 0 };
    u64 TotalDeallocTimeNs { 0 };
    u64 MinAllocTimeNs { UINT64_MAX };
    u64 MaxAllocTimeNs { 0 };

    u64 ReusedAllocations { 0 };
    u64 ZeroByteRequests { 0 };

    u64 LockContentions { 0 };
    u64 CASRetries { 0 };

    void record_allocation_size(u64 m_size);
    void update_peak(u64 m_current);
    void record_alloc_time(u64 duration_ns);
    void reset();
}; // struct DetailedAllocStats

class StatsFormatter {
public:
    static std::string format_bytes(u64 bytes, int precision = 2);
    static std::string format_time(u64 nanoseconds);
    static std::string format_percent(u64 numerator, u64 denominator, int precision = 2);
    static std::string create_bar(u64 m_value, u64 max, int width = 40);
    static std::string format_number(u64 m_value);
}; // class StatsFormatter

class StatsPrinter {
private:
    DetailedAllocStats const& m_stats;
    std::string const& m_name;

    static constexpr int LABEL_WIDTH = 32;
    static constexpr int VALUE_WIDTH = 18;
    static constexpr int BAR_WIDTH = 35;

public:
    StatsPrinter(DetailedAllocStats const& m_stats, std::string const& m_name)
        : m_stats(m_stats)
        , m_name(m_name)
    {
    }

    void print_detailed(std::ostream& os, bool verbose = true) const;
    void print_summary(std::ostream& os) const;

private:
    void print_header(std::ostream& os) const;
    void print_footer(std::ostream& os) const;
    void print_core_metrics(std::ostream& os) const;
    void print_size_distribution(std::ostream& os) const;
    void print_performance_metrics(std::ostream& os) const;
    void print_fragmentation_analysis(std::ostream& os) const;
    void print_timing_statistics(std::ostream& os) const;
    void print_error_statistics(std::ostream& os) const;
    void print_metric(std::ostream& os, char const* label, std::string const& m_value, bool warn = false) const;
    void print_metric_with_bar(std::ostream& os, char const* label, u64 m_value, u64 max) const;
}; // class StatsPrinter

} // namespace fairuz

#endif // FAIRUZ_DEBUG

#endif // STATS_HPP
