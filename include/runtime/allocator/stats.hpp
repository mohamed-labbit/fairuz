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
    void recordAllocationSize(std::uint64_t size)
    {
        if (size <= 8)
            Small8ByteAllocs++;
        else if (size <= 16)
            Small16ByteAllocs++;
        else if (size <= 32)
            Small32ByteAllocs++;
        else if (size <= 64)
            Small64ByteAllocs++;
        else if (size <= 128)
            Small128ByteAllocs++;
        else if (size <= 256)
            Small256ByteAllocs++;
        else if (size <= 512)
            Medium512ByteAllocs++;
        else if (size <= 1024)
            Medium1KBAllocs++;
        else if (size <= 2048)
            Medium2KBAllocs++;
        else if (size <= 4096)
            Medium4KBAllocs++;
        else if (size <= 8192)
            Large8KBAllocs++;
        else if (size <= 16384)
            Large16KBAllocs++;
        else if (size <= 32768)
            Large32KBAllocs++;
        else
            LargeHugeAllocs++;
    }

    /**
     * @brief Update peak memory usage
     * @param current Current allocation amount
     */
    void updatePeak(std::uint64_t current)
    {
        if (current > PeakAllocated)
            PeakAllocated = current;
    }

    /**
     * @brief Record allocation timing
     * @param duration_ns Duration in nanoseconds
     */
    void recordAllocTime(std::uint64_t duration_ns)
    {
        TotalAllocTimeNs += duration_ns;

        // Update min
        if (duration_ns < MinAllocTimeNs)
            MinAllocTimeNs = duration_ns;
        else if (duration_ns > MaxAllocTimeNs)
            MaxAllocTimeNs = duration_ns;
    }

    /**
     * @brief Reset all statistics to zero
     */
    void reset()
    {
        TotalAllocations = 0;
        TotalDeallocations = 0;
        TotalAllocated = 0;
        TotalDeallocated = 0;
        CurrentlyAllocated = 0;
        PeakAllocated = 0;
        ActiveBlocks = 0;

        Small8ByteAllocs = 0;
        Small16ByteAllocs = 0;
        Small32ByteAllocs = 0;
        Small64ByteAllocs = 0;
        Small128ByteAllocs = 0;
        Small256ByteAllocs = 0;

        Medium512ByteAllocs = 0;
        Medium1KBAllocs = 0;
        Medium2KBAllocs = 0;
        Medium4KBAllocs = 0;

        Large8KBAllocs = 0;
        Large16KBAllocs = 0;
        Large32KBAllocs = 0;
        LargeHugeAllocs = 0;

        // FastPoolHits=0;
        // FastPoolMisses=0;
        // FreeListHits=0;
        // FreeListMisses=0;
        BlockAllocations = 0;
        BlockExpansions = 0;

        InternalFragmentation = 0;
        WastedCapacity = 0;

        AllocationFailures = 0;
        DoubleFreeAttempts = 0;
        InvalidFreeAttempts = 0;
        AlignmentAdjustments = 0;

        TotalAllocTimeNs = 0;
        TotalDeallocTimeNs = 0;
        MinAllocTimeNs = UINT64_MAX;
        MaxAllocTimeNs = 0;

        ReusedAllocations = 0;
        ZeroByteRequests = 0;

        LockContentions = 0;
        CASRetries = 0;
    }
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
    static std::string formatBytes(std::uint64_t bytes, int precision = 2)
    {
        static char const* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
        static int const num_suffixes = 5;

        if (bytes == 0)
            return "0 B";

        double value = static_cast<double>(bytes);
        int idx = 0;

        while (value >= 1024.0 && idx < num_suffixes - 1) {
            value /= 1024.0;
            ++idx;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value << " " << suffixes[idx];
        return oss.str();
    }

    /**
     * @brief Format time duration
     * @param nanoseconds Duration in nanoseconds
     * @return Formatted string (e.g., "1.23 ms")
     */
    static std::string formatTime(std::uint64_t nanoseconds)
    {
        if (nanoseconds == 0)
            return "0 ns";
        if (nanoseconds == UINT64_MAX)
            return "N/A";

        std::ostringstream oss;

        if (nanoseconds < 1000)
            oss << nanoseconds << " ns";
        else if (nanoseconds < 1000000)
            oss << std::fixed << std::setprecision(2) << (nanoseconds / 1000.0) << " μs";
        else if (nanoseconds < 1000000000)
            oss << std::fixed << std::setprecision(2) << (nanoseconds / 1000000.0) << " ms";
        else
            oss << std::fixed << std::setprecision(2) << (nanoseconds / 1000000000.0) << " s";

        return oss.str();
    }

    /**
     * @brief Format percentage
     * @param numerator Numerator value
     * @param denominator Denominator value
     * @param precision Decimal precision
     * @return Formatted percentage string
     */
    static std::string formatPercent(std::uint64_t numerator, std::uint64_t denominator, int precision = 2)
    {
        if (denominator == 0)
            return "N/A";

        double percent = (100.0 * numerator) / denominator;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << percent << "%";
        return oss.str();
    }

    /**
     * @brief Create horizontal bar chart
     * @param value Current value
     * @param max Maximum value for scaling
     * @param width Bar width in characters
     * @return ASCII bar chart
     */
    static std::string createBar(std::uint64_t value, std::uint64_t max, int width = 40)
    {
        if (max == 0)
            return std::string(width, ' ');

        int filled = static_cast<int>((static_cast<double>(value) / max) * width);
        filled = std::min(filled, width);

        std::string bar = "|";
        bar += std::string(filled, '#');
        bar += std::string(width - filled, '.');
        bar += "|";
        return bar;
    }

    /**
     * @brief Format large numbers with thousands separators
     * @param value Number to format
     * @return Formatted string (e.g., "1,234,567")
     */
    static std::string formatNumber(std::uint64_t value)
    {
        std::string str = std::to_string(value);
        int insert_pos = str.length() - 3;

        while (insert_pos > 0) {
            str.insert(insert_pos, ",");
            insert_pos -= 3;
        }

        return str;
    }
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
    void printDetailed(std::ostream& os, bool verbose = true) const
    {
        using std::left;
        using std::right;
        using std::setw;

        printHeader(os);
        printCoreMetrics(os);

        if (verbose) {
            printSizeDistribution(os);
            printPerformanceMetrics(os);
            printFragmentationAnalysis(os);
            printTimingStatistics(os);
            printErrorStatistics(os);
        }

        printFooter(os);
    }

    /**
     * @brief Print compact summary
     * @param os Output stream
     */
    void printSummary(std::ostream& os) const
    {
        os << "[" << name_ << "] "
           << "Allocs: " << StatsFormatter::formatNumber(stats_.TotalAllocations) << " | "
           << "Deallocs: " << StatsFormatter::formatNumber(stats_.TotalDeallocations) << " | "
           << "Current: " << StatsFormatter::formatBytes(stats_.CurrentlyAllocated) << " | "
           << "Peak: " << StatsFormatter::formatBytes(stats_.PeakAllocated) << "\n";
    }

private:
    void printHeader(std::ostream& os) const
    {
        os << "\n";
        os << "+=====================================================================================+\n";
        os << "|                   Arena Allocator Statistics - [" << std::left << std::setw(28) << name_ << "]|\n";
        os << "+=====================================================================================+\n";
    }

    void printFooter(std::ostream& os) const { os << "+=====================================================================================+\n\n"; }

    void printCoreMetrics(std::ostream& os) const
    {
        using std::left;
        using std::right;
        using std::setw;

        os << "\n+-- Core Metrics --------------------------------------------------------------------+\n";
        printMetric(os, "Total Allocations", StatsFormatter::formatNumber(stats_.TotalAllocations));
        printMetric(os, "Total Deallocations", StatsFormatter::formatNumber(stats_.TotalDeallocations));
        printMetric(os, "Live Allocations", StatsFormatter::formatNumber(stats_.TotalAllocations - stats_.TotalDeallocations));
        os << "+------------------------------------------------------------------------------------+\n";
        printMetric(os, "Total Bytes Allocated", StatsFormatter::formatBytes(stats_.TotalAllocated));
        printMetric(os, "Currently In Use", StatsFormatter::formatBytes(stats_.CurrentlyAllocated));
        printMetric(os, "Peak Memory Usage", StatsFormatter::formatBytes(stats_.PeakAllocated));
        os << "+------------------------------------------------------------------------------------+\n";
        printMetric(os, "Active Memory Blocks", std::to_string(stats_.ActiveBlocks));

        if (stats_.TotalAllocations > 0)
            printMetric(os, "Average Allocation Size", StatsFormatter::formatBytes(stats_.TotalAllocated / stats_.TotalAllocations));

        os << "+------------------------------------------------------------------------------------+\n";
    }

    void printSizeDistribution(std::ostream& os) const
    {
        if (stats_.TotalAllocations == 0)
            return;

        os << "\n+-- Allocation Size Distribution ----------------------------------------------------+\n";

        struct SizeBucket {
            char const* name;
            std::uint64_t count;
        };

        SizeBucket buckets[] = { { "<= 8 B", stats_.Small8ByteAllocs }, { "<= 16 B", stats_.Small16ByteAllocs },
            { "<= 32 B", stats_.Small32ByteAllocs }, { "<= 64 B", stats_.Small64ByteAllocs }, { "<= 128 B", stats_.Small128ByteAllocs },
            { "<= 256 B", stats_.Small256ByteAllocs }, { "<= 512 B", stats_.Medium512ByteAllocs }, { "<= 1 KB", stats_.Medium1KBAllocs },
            { "<= 2 KB", stats_.Medium2KBAllocs }, { "<= 4 KB", stats_.Medium4KBAllocs }, { "<= 8 KB", stats_.Large8KBAllocs },
            { "<= 16 KB", stats_.Large16KBAllocs }, { "<= 32 KB", stats_.Large32KBAllocs }, { "> 64 KB", stats_.LargeHugeAllocs } };

        // Find max for scaling bars
        std::uint64_t max_count = 0;
        for (auto const& bucket : buckets)
            max_count = std::max(max_count, bucket.count);

        for (auto const& bucket : buckets) {
            if (bucket.count == 0)
                continue;

            os << "| " << std::left << std::setw(10) << bucket.name << " " << std::right << std::setw(12)
               << StatsFormatter::formatNumber(bucket.count) << " " << std::setw(8)
               << StatsFormatter::formatPercent(bucket.count, stats_.TotalAllocations, 1) << " "
               << StatsFormatter::createBar(bucket.count, max_count, 30) << " |\n";
        }

        os << "+------------------------------------------------------------------------------------+\n";
    }

    void printPerformanceMetrics(std::ostream& os) const
    {
        // std::uint64_t fast_hits   = stats_.FastPoolHits.load(std::memory_order_relaxed);
        // std::uint64_t fast_misses = stats_.FastPoolMisses.load(std::memory_order_relaxed);
        // std::uint64_t free_hits   = stats_.FreeListHits.load(std::memory_order_relaxed);
        // std::uint64_t free_misses = stats_.FreeListMisses.load(std::memory_order_relaxed);

        // std::uint64_t fast_total = fast_hits + fast_misses;
        // std::uint64_t free_total = free_hits + free_misses;

        os << "\n+-- Performance Metrics -------------------------------------------------------------+\n";

        // if (fast_total > 0)
        // {
        //   printMetric(os, "Fast Pool Hit Rate", StatsFormatter::formatPercent(fast_hits, fast_total));
        //   printMetricWithBar(os, "  Hits", fast_hits, fast_total);
        //   printMetricWithBar(os, "  Misses", fast_misses, fast_total);
        // }

        // if (free_total > 0)
        // {
        //   os << "+------------------------------------------------------------------------------------+\n";
        //   printMetric(os, "Free List Hit Rate", StatsFormatter::formatPercent(free_hits, free_total));
        //   printMetricWithBar(os, "  Hits", free_hits, free_total);
        //   printMetricWithBar(os, "  Misses", free_misses, free_total);
        // }

        if (stats_.ReusedAllocations > 0) {
            os << "+------------------------------------------------------------------------------------+\n";
            printMetric(os, "Reused Allocations", StatsFormatter::formatNumber(stats_.ReusedAllocations));
        }

        if (stats_.BlockAllocations > 0 || stats_.BlockExpansions > 0) {
            os << "+------------------------------------------------------------------------------------+\n";
            printMetric(os, "Block Allocations", std::to_string(stats_.BlockAllocations));
            printMetric(os, "Block Expansions", std::to_string(stats_.BlockExpansions));
        }

        os << "+------------------------------------------------------------------------------------+\n";
    }

    void printFragmentationAnalysis(std::ostream& os) const
    {
        if (stats_.InternalFragmentation == 0 && stats_.WastedCapacity == 0)
            return;

        os << "\n+-- Fragmentation Analysis ----------------------------------------------------------+\n";

        if (stats_.InternalFragmentation > 0) {
            printMetric(os, "Internal Fragmentation", StatsFormatter::formatBytes(stats_.InternalFragmentation));
            if (stats_.TotalAllocated > 0)
                printMetric(os, "  (% of total)", StatsFormatter::formatPercent(stats_.InternalFragmentation, stats_.TotalAllocated));
        }

        if (stats_.WastedCapacity > 0)
            printMetric(os, "Wasted Capacity", StatsFormatter::formatBytes(stats_.WastedCapacity));

        os << "+------------------------------------------------------------------------------------+\n";
    }

    void printTimingStatistics(std::ostream& os) const
    {
        if (stats_.TotalAllocTimeNs == 0 && stats_.TotalDeallocTimeNs == 0)
            return;

        os << "\n+-- Timing Statistics ---------------------------------------------------------------+\n";

        if (stats_.TotalAllocations > 0 && stats_.TotalAllocTimeNs > 0) {
            std::uint64_t avg_alloc = stats_.TotalAllocTimeNs / stats_.TotalAllocations;
            printMetric(os, "Allocation Time (avg)", StatsFormatter::formatTime(avg_alloc));
            printMetric(os, "  Min", StatsFormatter::formatTime(stats_.MinAllocTimeNs));
            printMetric(os, "  Max", StatsFormatter::formatTime(stats_.MaxAllocTimeNs));
            printMetric(os, "  Total", StatsFormatter::formatTime(stats_.TotalAllocated));
        }

        if (stats_.TotalDeallocations > 0 && stats_.TotalDeallocTimeNs > 0) {
            os << "+------------------------------------------------------------------------------------+\n";
            std::uint64_t avg_dealloc = stats_.TotalDeallocTimeNs / stats_.TotalDeallocations;
            printMetric(os, "Deallocation Time (avg)", StatsFormatter::formatTime(avg_dealloc));
            printMetric(os, "  Total", StatsFormatter::formatTime(stats_.TotalDeallocTimeNs));
        }

        os << "+------------------------------------------------------------------------------------+\n";
    }

    void printErrorStatistics(std::ostream& os) const
    {
        if (stats_.AllocationFailures == 0 && stats_.DoubleFreeAttempts == 0 && stats_.InvalidFreeAttempts == 0 && stats_.ZeroByteRequests == 0)
            return;

        os << "\n+-- Error Statistics ----------------------------------------------------------------+\n";

        if (stats_.AllocationFailures > 0)
            printMetric(os, "Allocation Failures", std::to_string(stats_.AllocationFailures), true);
        if (stats_.DoubleFreeAttempts > 0)
            printMetric(os, "Double-Free Attempts", std::to_string(stats_.DoubleFreeAttempts), true);
        if (stats_.InvalidFreeAttempts > 0)
            printMetric(os, "Invalid Free Attempts", std::to_string(stats_.InvalidFreeAttempts), true);
        if (stats_.ZeroByteRequests > 0)
            printMetric(os, "Zero-Byte Requests", std::to_string(stats_.ZeroByteRequests), false);

        os << "+------------------------------------------------------------------------------------+\n";
    }

    void printMetric(std::ostream& os, char const* label, std::string const& value, bool warn = false) const
    {
        using std::left;
        using std::right;
        using std::setw;

        if (warn)
            os << "│ ⚠ ";
        else
            os << "│ ";

        os << left << setw(LABEL_WIDTH) << label << " " << right << setw(VALUE_WIDTH) << value << " │\n";
    }

    void printMetricWithBar(std::ostream& os, char const* label, std::uint64_t value, std::uint64_t max) const
    {
        using std::left;
        using std::right;
        using std::setw;

        os << "│ " << left << setw(LABEL_WIDTH) << label << " " << right << setw(12) << StatsFormatter::formatNumber(value) << " "
           << StatsFormatter::createBar(value, max, 20) << " │\n";
    }
};

} // namespace allocator
} // namespace runtime
} // namespace mylang
