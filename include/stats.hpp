#ifndef STATS_HPP
#define STATS_HPP

#ifdef fairuz_DEBUG

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

    void recordAllocationSize(u64 size);
    void updatePeak(u64 current);
    void recordAllocTime(u64 duration_ns);
    void reset();
}; // struct DetailedAllocStats

class StatsFormatter {
public:
    static std::string formatBytes(u64 bytes, int precision = 2);
    static std::string formatTime(u64 nanoseconds);
    static std::string formatPercent(u64 numerator, u64 denominator, int precision = 2);
    static std::string createBar(u64 value, u64 max, int width = 40);
    static std::string formatNumber(u64 value);
}; // class StatsFormatter

class StatsPrinter {
private:
    DetailedAllocStats const& stats_;
    std::string const& name_;

    static constexpr int LABEL_WIDTH = 32;
    static constexpr int VALUE_WIDTH = 18;
    static constexpr int BAR_WIDTH = 35;

public:
    StatsPrinter(DetailedAllocStats const& stats, std::string const& name)
        : stats_(stats)
        , name_(name)
    {
    }

    void printDetailed(std::ostream& os, bool verbose = true) const;
    void printSummary(std::ostream& os) const;

private:
    void printHeader(std::ostream& os) const;
    void printFooter(std::ostream& os) const;
    void printCoreMetrics(std::ostream& os) const;
    void printSizeDistribution(std::ostream& os) const;
    void printPerformanceMetrics(std::ostream& os) const;
    void printFragmentationAnalysis(std::ostream& os) const;
    void printTimingStatistics(std::ostream& os) const;
    void printErrorStatistics(std::ostream& os) const;
    void printMetric(std::ostream& os, char const* label, std::string const& value, bool warn = false) const;
    void printMetricWithBar(std::ostream& os, char const* label, u64 value, u64 max) const;
}; // class StatsPrinter

} // namespace fairuz

#endif // fairuz_DEBUG

#endif // STATS_HPP
