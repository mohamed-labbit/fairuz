#ifdef MYLANG_DEBUG
#    include "../../include/allocator/stats.hpp"

namespace mylang {

void DetailedAllocStats::recordAllocationSize(uint64_t size)
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

void DetailedAllocStats::updatePeak(uint64_t current)
{
    if (current > PeakAllocated)
        PeakAllocated = current;
}

void DetailedAllocStats::recordAllocTime(uint64_t duration_ns)
{
    TotalAllocTimeNs += duration_ns;

    // Update min
    if (duration_ns < MinAllocTimeNs)
        MinAllocTimeNs = duration_ns;
    else if (duration_ns > MaxAllocTimeNs)
        MaxAllocTimeNs = duration_ns;
}

void DetailedAllocStats::reset()
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

std::string StatsFormatter::formatBytes(uint64_t bytes, int precision)
{
    static char const* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
    static int const num_suffixes = 5;

    if (bytes == 0)
        return "0 B";

    auto value = static_cast<double>(bytes);
    int idx = 0;

    while (value >= 1024.0 && idx < num_suffixes - 1) {
        value /= 1024.0;
        ++idx;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value << " " << suffixes[idx];
    return oss.str();
}

std::string StatsFormatter::formatTime(uint64_t nanoseconds)
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

std::string StatsFormatter::formatPercent(uint64_t numerator, uint64_t denominator, int precision)
{
    if (denominator == 0)
        return "N/A";

    double percent = (100.0 * numerator) / denominator;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << percent << "%";
    return oss.str();
}

std::string StatsFormatter::createBar(uint64_t value, uint64_t max, int width)
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

std::string StatsFormatter::formatNumber(uint64_t value)
{
    std::string str = std::to_string(value);
    int insert_pos = str.length() - 3;

    while (insert_pos > 0) {
        str.insert(insert_pos, ",");
        insert_pos -= 3;
    }

    return str;
}

void StatsPrinter::printDetailed(std::ostream& os, bool verbose) const
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

void StatsPrinter::printSummary(std::ostream& os) const
{
    os << "[" << name_ << "] "
       << "Allocs: " << StatsFormatter::formatNumber(stats_.TotalAllocations) << " | "
       << "Deallocs: " << StatsFormatter::formatNumber(stats_.TotalDeallocations) << " | "
       << "Current: " << StatsFormatter::formatBytes(stats_.CurrentlyAllocated) << " | "
       << "Peak: " << StatsFormatter::formatBytes(stats_.PeakAllocated) << "\n";
}

void StatsPrinter::printHeader(std::ostream& os) const
{
    os << "\n";
    os << "+====================================================================="
          "================+\n";
    os << "|                   Arena Allocator Statistics - [" << std::left << std::setw(28) << name_ << "]|\n";
    os << "+====================================================================="
          "================+\n";
}

void StatsPrinter::printFooter(std::ostream& os) const
{
    os << "+====================================================================="
          "================+\n\n";
}

void StatsPrinter::printCoreMetrics(std::ostream& os) const
{
    using std::left;
    using std::right;
    using std::setw;

    os << "\n+-- Core Metrics "
          "--------------------------------------------------------------------+"
          "\n";
    printMetric(os, "Total Allocations", StatsFormatter::formatNumber(stats_.TotalAllocations));
    printMetric(os, "Total Deallocations", StatsFormatter::formatNumber(stats_.TotalDeallocations));
    printMetric(os, "Live Allocations", StatsFormatter::formatNumber(stats_.TotalAllocations - stats_.TotalDeallocations));
    os << "+---------------------------------------------------------------------"
          "---------------+\n";
    printMetric(os, "Total Bytes Allocated", StatsFormatter::formatBytes(stats_.TotalAllocated));
    printMetric(os, "Currently In Use", StatsFormatter::formatBytes(stats_.CurrentlyAllocated));
    printMetric(os, "Peak Memory Usage", StatsFormatter::formatBytes(stats_.PeakAllocated));
    os << "+---------------------------------------------------------------------"
          "---------------+\n";
    printMetric(os, "Active Memory Blocks", std::to_string(stats_.ActiveBlocks));

    if (stats_.TotalAllocations > 0)
        printMetric(os, "Average Allocation Size", StatsFormatter::formatBytes(stats_.TotalAllocated / stats_.TotalAllocations));

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::printSizeDistribution(std::ostream& os) const
{
    if (stats_.TotalAllocations == 0)
        return;

    os << "\n+-- Allocation Size Distribution "
          "----------------------------------------------------+\n";

    struct SizeBucket {
        char const* name;
        uint64_t count;
    };

    SizeBucket buckets[] = { { "<= 8 B", stats_.Small8ByteAllocs }, { "<= 16 B", stats_.Small16ByteAllocs }, { "<= 32 B", stats_.Small32ByteAllocs },
        { "<= 64 B", stats_.Small64ByteAllocs }, { "<= 128 B", stats_.Small128ByteAllocs }, { "<= 256 B", stats_.Small256ByteAllocs },
        { "<= 512 B", stats_.Medium512ByteAllocs }, { "<= 1 KB", stats_.Medium1KBAllocs }, { "<= 2 KB", stats_.Medium2KBAllocs },
        { "<= 4 KB", stats_.Medium4KBAllocs }, { "<= 8 KB", stats_.Large8KBAllocs }, { "<= 16 KB", stats_.Large16KBAllocs },
        { "<= 32 KB", stats_.Large32KBAllocs }, { "> 64 KB", stats_.LargeHugeAllocs } };

    // Find max for scaling bars
    uint64_t max_count = 0;
    for (auto const& bucket : buckets)
        max_count = std::max(max_count, bucket.count);

    for (auto const& bucket : buckets) {
        if (bucket.count == 0)
            continue;

        os << "| " << std::left << std::setw(10) << bucket.name << " " << std::right << std::setw(12) << StatsFormatter::formatNumber(bucket.count) << " "
           << std::setw(8) << StatsFormatter::formatPercent(bucket.count, stats_.TotalAllocations, 1) << " "
           << StatsFormatter::createBar(bucket.count, max_count, 30) << " |\n";
    }

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::printPerformanceMetrics(std::ostream& os) const
{
    // uint64_t fast_hits   =
    // stats_.FastPoolHits.load(std::memory_order_relaxed); uint64_t
    // fast_misses = stats_.FastPoolMisses.load(std::memory_order_relaxed);
    // uint64_t free_hits   =
    // stats_.FreeListHits.load(std::memory_order_relaxed); uint64_t
    // free_misses = stats_.FreeListMisses.load(std::memory_order_relaxed);

    // uint64_t fast_total = fast_hits + fast_misses;
    // uint64_t free_total = free_hits + free_misses;

    os << "\n+-- Performance Metrics "
          "-------------------------------------------------------------+\n";

    // if (fast_total > 0)
    // {
    //   printMetric(os, "Fast Pool Hit Rate",
    //   StatsFormatter::formatPercent(fast_hits, fast_total));
    //   printMetricWithBar(os, "  Hits", fast_hits, fast_total);
    //   printMetricWithBar(os, "  Misses", fast_misses, fast_total);
    // }

    // if (free_total > 0)
    // {
    //   os <<
    //   "+------------------------------------------------------------------------------------+\n";
    //   printMetric(os, "Free List Hit Rate",
    //   StatsFormatter::formatPercent(free_hits, free_total));
    //   printMetricWithBar(os, "  Hits", free_hits, free_total);
    //   printMetricWithBar(os, "  Misses", free_misses, free_total);
    // }

    if (stats_.ReusedAllocations > 0) {
        os << "+-------------------------------------------------------------------"
              "-----------------+\n";
        printMetric(os, "Reused Allocations", StatsFormatter::formatNumber(stats_.ReusedAllocations));
    }

    if (stats_.BlockAllocations > 0 || stats_.BlockExpansions > 0) {
        os << "+-------------------------------------------------------------------"
              "-----------------+\n";
        printMetric(os, "Block Allocations", std::to_string(stats_.BlockAllocations));
        printMetric(os, "Block Expansions", std::to_string(stats_.BlockExpansions));
    }

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::printFragmentationAnalysis(std::ostream& os) const
{
    if (stats_.InternalFragmentation == 0 && stats_.WastedCapacity == 0)
        return;

    os << "\n+-- Fragmentation Analysis "
          "----------------------------------------------------------+\n";

    if (stats_.InternalFragmentation > 0) {
        printMetric(os, "Internal Fragmentation", StatsFormatter::formatBytes(stats_.InternalFragmentation));
        if (stats_.TotalAllocated > 0)
            printMetric(os, "  (% of total)", StatsFormatter::formatPercent(stats_.InternalFragmentation, stats_.TotalAllocated));
    }

    if (stats_.WastedCapacity > 0)
        printMetric(os, "Wasted Capacity", StatsFormatter::formatBytes(stats_.WastedCapacity));

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::printTimingStatistics(std::ostream& os) const
{
    if (stats_.TotalAllocTimeNs == 0 && stats_.TotalDeallocTimeNs == 0)
        return;

    os << "\n+-- Timing Statistics "
          "---------------------------------------------------------------+\n";

    if (stats_.TotalAllocations > 0 && stats_.TotalAllocTimeNs > 0) {
        uint64_t avg_alloc = stats_.TotalAllocTimeNs / stats_.TotalAllocations;
        printMetric(os, "Allocation Time (avg)", StatsFormatter::formatTime(avg_alloc));
        printMetric(os, "  Min", StatsFormatter::formatTime(stats_.MinAllocTimeNs));
        printMetric(os, "  Max", StatsFormatter::formatTime(stats_.MaxAllocTimeNs));
        printMetric(os, "  Total", StatsFormatter::formatTime(stats_.TotalAllocated));
    }

    if (stats_.TotalDeallocations > 0 && stats_.TotalDeallocTimeNs > 0) {
        os << "+-------------------------------------------------------------------"
              "-----------------+\n";
        uint64_t avg_dealloc = stats_.TotalDeallocTimeNs / stats_.TotalDeallocations;
        printMetric(os, "Deallocation Time (avg)", StatsFormatter::formatTime(avg_dealloc));
        printMetric(os, "  Total", StatsFormatter::formatTime(stats_.TotalDeallocTimeNs));
    }

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::printErrorStatistics(std::ostream& os) const
{
    if (stats_.AllocationFailures == 0 && stats_.DoubleFreeAttempts == 0 && stats_.InvalidFreeAttempts == 0 && stats_.ZeroByteRequests == 0)
        return;

    os << "\n+-- Error Statistics "
          "----------------------------------------------------------------+\n";

    if (stats_.AllocationFailures > 0)
        printMetric(os, "Allocation Failures", std::to_string(stats_.AllocationFailures), true);

    if (stats_.DoubleFreeAttempts > 0)
        printMetric(os, "Double-Free Attempts", std::to_string(stats_.DoubleFreeAttempts), true);

    if (stats_.InvalidFreeAttempts > 0)
        printMetric(os, "Invalid Free Attempts", std::to_string(stats_.InvalidFreeAttempts), true);

    if (stats_.ZeroByteRequests > 0)
        printMetric(os, "Zero-unsigned char Requests", std::to_string(stats_.ZeroByteRequests), false);

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::printMetric(std::ostream& os, char const* label, std::string const& value, bool warn) const
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

void StatsPrinter::printMetricWithBar(std::ostream& os, char const* label, uint64_t value, uint64_t max) const
{
    using std::left;
    using std::right;
    using std::setw;

    os << "│ " << left << setw(LABEL_WIDTH) << label << " " << right << setw(12) << StatsFormatter::formatNumber(value) << " "
       << StatsFormatter::createBar(value, max, 20) << " │\n";
}

} // namespace mylang
#endif // MYLANG_DEBUG
