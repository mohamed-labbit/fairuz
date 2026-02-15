#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mylang {
namespace runtime {
namespace allocator {

/**
 * @struct DetailedAllocStats
 * @brief Comprehensive allocation statistics with histograms and performance metrics
 */
struct DetailedAllocStats {
    std::uint64_t TotalAllocations { 0 };   ///< Total allocation requests
    std::uint64_t TotalDeallocations { 0 }; ///< Total deallocation requests
    std::uint64_t TotalAllocated { 0 };     ///< Cumulative bytes allocated
    std::uint64_t TotalDeallocated { 0 };   ///< Cumulative bytes deallocated
    std::uint64_t CurrentlyAllocated { 0 }; ///< Currently in use (live bytes)
    std::uint64_t PeakAllocated { 0 };      ///< Peak memory usage
    std::uint64_t ActiveBlocks { 0 };       ///< Number of active blocks

    // Size Distribution Tracking

    // Small allocations: 8, 16, 32, 64, 128, 256 bytes
    std::uint64_t Small8ByteAllocs { 0 };
    std::uint64_t Small16ByteAllocs { 0 };
    std::uint64_t Small32ByteAllocs { 0 };
    std::uint64_t Small64ByteAllocs { 0 };
    std::uint64_t Small128ByteAllocs { 0 };
    std::uint64_t Small256ByteAllocs { 0 };

    // Medium allocations: 512B, 1KB, 2KB, 4KB
    std::uint64_t Medium512ByteAllocs { 0 };
    std::uint64_t Medium1KBAllocs { 0 };
    std::uint64_t Medium2KBAllocs { 0 };
    std::uint64_t Medium4KBAllocs { 0 };

    // Large allocations: 8KB, 16KB, 32KB, 64KB+
    std::uint64_t Large8KBAllocs { 0 };
    std::uint64_t Large16KBAllocs { 0 };
    std::uint64_t Large32KBAllocs { 0 };
    std::uint64_t LargeHugeAllocs { 0 }; ///< 64KB and above

    // Performance Metrics

    // std::uint64_t FastPoolHits{0};      ///< Allocations served from fast pools
    // std::uint64_t FastPoolMisses{0};    ///< Fast pool misses (went to general allocation)
    // std::uint64_t FreeListHits{0};      ///< Allocations served from free lists
    // std::uint64_t FreeListMisses{0};    ///< Free list misses (needed new block)
    std::uint64_t BlockAllocations { 0 }; ///< Number of new blocks allocated
    std::uint64_t BlockExpansions { 0 };  ///< Number of times blocks grew

    // Fragmentation Tracking

    std::uint64_t InternalFragmentation { 0 }; ///< Bytes lost to alignment/padding
    std::uint64_t WastedCapacity { 0 };        ///< Unused capacity in blocks

    // Error and Edge Case Tracking

    std::uint64_t AllocationFailures { 0 };   ///< Failed allocations (OOM)
    std::uint64_t DoubleFreeAttempts { 0 };   ///< Double-free detection triggers
    std::uint64_t InvalidFreeAttempts { 0 };  ///< Attempts to free unknown pointers
    std::uint64_t AlignmentAdjustments { 0 }; ///< Number of alignment corrections

    // Timing Statistics (nanoseconds)

    std::uint64_t TotalAllocTimeNs { 0 };        ///< Total time spent in allocate()
    std::uint64_t TotalDeallocTimeNs { 0 };      ///< Total time spent in deallocate()
    std::uint64_t MinAllocTimeNs { UINT64_MAX }; ///< Fastest allocation
    std::uint64_t MaxAllocTimeNs { 0 };          ///< Slowest allocation

    // Reuse Statistics

    std::uint64_t ReusedAllocations { 0 }; ///< Memory blocks reused from free lists
    std::uint64_t ZeroByteRequests { 0 };  ///< Requests for 0 bytes (edge case)

    // Thread Contention Metrics

    std::uint64_t LockContentions { 0 }; ///< Number of times a thread had to wait
    std::uint64_t CASRetries { 0 };      ///< CAS operation retries

    /**
     * @brief Update allocation size histogram
     * @param size Allocation size in bytes
     */
    void recordAllocationSize(std::uint64_t size);

    /**
     * @brief Update peak memory usage
     * @param current Current allocation amount
     */
    void updatePeak(std::uint64_t current);

    /**
     * @brief Record allocation timing
     * @param duration_ns Duration in nanoseconds
     */
    void recordAllocTime(std::uint64_t duration_ns);

    /**
     * @brief Reset all statistics to zero
     */
    void reset();
};

/**
 * @class StatsFormatter
 * @brief Advanced formatting utilities for allocation statistics
 */
class StatsFormatter {
public:
    /**
     * @brief Format bytes as human-readable string
     * @param bytes Number of bytes
     * @param precision Decimal precision
     * @return Formatted string (e.g., "1.50 MB")
     */
    static std::string formatBytes(std::uint64_t bytes, int precision = 2);

    /**
     * @brief Format time duration
     * @param nanoseconds Duration in nanoseconds
     * @return Formatted string (e.g., "1.23 ms")
     */
    static std::string formatTime(std::uint64_t nanoseconds);

    /**
     * @brief Format percentage
     * @param numerator Numerator value
     * @param denominator Denominator value
     * @param precision Decimal precision
     * @return Formatted percentage string
     */
    static std::string formatPercent(std::uint64_t numerator, std::uint64_t denominator, int precision = 2);

    /**
     * @brief Create horizontal bar chart
     * @param value Current value
     * @param max Maximum value for scaling
     * @param width Bar width in characters
     * @return ASCII bar chart
     */
    static std::string createBar(std::uint64_t value, std::uint64_t max, int width = 40);

    /**
     * @brief Format large numbers with thousands separators
     * @param value Number to format
     * @return Formatted string (e.g., "1,234,567")
     */
    static std::string formatNumber(std::uint64_t value);
};

/**
 * @class StatsPrinter
 * @brief Comprehensive statistics printer with multiple output formats
 */
class StatsPrinter {
private:
    DetailedAllocStats const& stats_;
    std::string const& name_;

    // Formatting constants
    static constexpr int LABEL_WIDTH = 32;
    static constexpr int VALUE_WIDTH = 18;
    static constexpr int BAR_WIDTH = 35;

public:
    StatsPrinter(DetailedAllocStats const& stats, std::string const& name)
        : stats_(stats)
        , name_(name)
    {
    }

    /**
     * @brief Print comprehensive statistics report
     * @param os Output stream
     * @param verbose Include detailed information
     */
    void printDetailed(std::ostream& os, bool verbose = true) const;

    /**
     * @brief Print compact summary
     * @param os Output stream
     */
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

    void printMetricWithBar(std::ostream& os, char const* label, std::uint64_t value, std::uint64_t max) const;
};

} // namespace allocator
} // namespace runtime
} // namespace mylang
