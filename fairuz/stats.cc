#include "stats.hpp"

#ifdef FAIRUZ_DEBUG
namespace fairuz {

void DetailedAllocStats::record_allocation_size(u64 size)
{
    if (size <= 8)
        Small8ByteAllocs += 1;
    else if (size <= 16)
        Small16ByteAllocs += 1;
    else if (size <= 32)
        Small32ByteAllocs += 1;
    else if (size <= 64)
        Small64ByteAllocs += 1;
    else if (size <= 128)
        Small128ByteAllocs += 1;
    else if (size <= 256)
        Small256ByteAllocs += 1;
    else if (size <= 512)
        Medium512ByteAllocs += 1;
    else if (size <= 1024)
        Medium1KBAllocs += 1;
    else if (size <= 2048)
        Medium2KBAllocs += 1;
    else if (size <= 4096)
        Medium4KBAllocs += 1;
    else if (size <= 8192)
        Large8KBAllocs += 1;
    else if (size <= 16384)
        Large16KBAllocs += 1;
    else if (size <= 32768)
        Large32KBAllocs += 1;
    else
        LargeHugeAllocs += 1;
}

void DetailedAllocStats::update_peak(u64 current)
{
    if (current > PeakAllocated)
        PeakAllocated = current;
}

void DetailedAllocStats::record_alloc_time(u64 duration_ns)
{
    TotalAllocTimeNs += duration_ns;

    if (duration_ns < MinAllocTimeNs)
        MinAllocTimeNs = duration_ns;

    if (duration_ns > MaxAllocTimeNs)
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

std::string StatsFormatter::format_bytes(u64 bytes, int precision)
{
    static char const* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
    static int const num_suffixes = 5;

    if (bytes == 0)
        return "0 B";

    auto value = static_cast<f64>(bytes);
    int idx = 0;

    while (value >= 1024.0 && idx < num_suffixes - 1) {
        value /= 1024.0;
        idx += 1;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value << " " << suffixes[idx];
    return oss.str();
}

std::string StatsFormatter::format_time(u64 nanoseconds)
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

std::string StatsFormatter::format_percent(u64 numerator, u64 denominator, int precision)
{
    if (denominator == 0)
        return "N/A";

    f64 percent = (100.0 * numerator) / denominator;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << percent << "%";
    return oss.str();
}

std::string StatsFormatter::create_bar(u64 value, u64 max, int width)
{
    if (max == 0)
        return std::string(width, ' ');

    auto filled = static_cast<int>((static_cast<f64>(value) / max) * width);
    filled = std::min(filled, width);

    std::string bar = "|";
    bar += std::string(filled, '#');
    bar += std::string(width - filled, '.');
    bar += "|";
    return bar;
}

std::string StatsFormatter::format_number(u64 value)
{
    std::string str = std::to_string(value);
    int insert_pos = static_cast<int>(str.length()) - 3;

    while (insert_pos > 0) {
        str.insert(insert_pos, ",");
        insert_pos -= 3;
    }

    return str;
}

void StatsPrinter::print_detailed(std::ostream& os, bool verbose) const
{
    print_header(os);
    print_core_metrics(os);

    if (verbose) {
        print_size_distribution(os);
        print_performance_metrics(os);
        print_fragmentation_analysis(os);
        print_timing_statistics(os);
        print_error_statistics(os);
    }

    print_footer(os);
}

void StatsPrinter::print_summary(std::ostream& os) const
{
    os << "[" << m_name << "] "
       << "Allocs: " << StatsFormatter::format_number(m_stats.TotalAllocations) << " | "
       << "Deallocs: " << StatsFormatter::format_number(m_stats.TotalDeallocations) << " | "
       << "Current: " << StatsFormatter::format_bytes(m_stats.CurrentlyAllocated) << " | "
       << "Peak: " << StatsFormatter::format_bytes(m_stats.PeakAllocated) << "\n";
}

void StatsPrinter::print_header(std::ostream& os) const
{
    os << "\n";
    os << "+====================================================================="
          "================+\n";
    os << "|                   Arena Allocator Statistics - [" << std::left << std::setw(28) << m_name << "]|\n";
    os << "+====================================================================="
          "================+\n";
}

void StatsPrinter::print_footer(std::ostream& os) const
{
    os << "+====================================================================="
          "================+\n\n";
}

void StatsPrinter::print_core_metrics(std::ostream& os) const
{
    using std::left;
    using std::right;
    using std::setw;

    os << "\n+Core -- Metrics "
          "--------------------------------------------------------------------+"
          "\n";
    print_metric(os, "Total Allocations", StatsFormatter::format_number(m_stats.TotalAllocations));
    print_metric(os, "Total Deallocations", StatsFormatter::format_number(m_stats.TotalDeallocations));
    print_metric(os, "Live Allocations", StatsFormatter::format_number(m_stats.TotalAllocations >= m_stats.TotalDeallocations ? m_stats.TotalAllocations - m_stats.TotalDeallocations : 0));
    os << "+---------------------------------------------------------------------"
          "---------------+\n";
    print_metric(os, "Total Bytes Allocated", StatsFormatter::format_bytes(m_stats.TotalAllocated));
    print_metric(os, "Total Bytes Deallocated", StatsFormatter::format_bytes(m_stats.TotalDeallocated));
    print_metric(os, "Currently In Use", StatsFormatter::format_bytes(m_stats.CurrentlyAllocated));
    print_metric(os, "Peak Memory Usage", StatsFormatter::format_bytes(m_stats.PeakAllocated));
    os << "+---------------------------------------------------------------------"
          "---------------+\n";
    print_metric(os, "Active Memory Blocks", std::to_string(m_stats.ActiveBlocks));

    if (m_stats.TotalAllocations > 0)
        print_metric(os, "Average Allocation Size", StatsFormatter::format_bytes(m_stats.TotalAllocated / m_stats.TotalAllocations));

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::print_size_distribution(std::ostream& os) const
{
    if (m_stats.TotalAllocations == 0)
        return;

    os << "\n+Allocation -- Size Distribution "
          "----------------------------------------------------+\n";

    struct SizeBucket {
        char const* name;
        u64 count;
    }; // struct SizeBucket

    SizeBucket m_buckets[] = { { "<= 8 B", m_stats.Small8ByteAllocs }, { "<= 16 B", m_stats.Small16ByteAllocs }, { "<= 32 B", m_stats.Small32ByteAllocs },
        { "<= 64 B", m_stats.Small64ByteAllocs }, { "<= 128 B", m_stats.Small128ByteAllocs }, { "<= 256 B", m_stats.Small256ByteAllocs },
        { "<= 512 B", m_stats.Medium512ByteAllocs }, { "<= 1 KB", m_stats.Medium1KBAllocs }, { "<= 2 KB", m_stats.Medium2KBAllocs },
        { "<= 4 KB", m_stats.Medium4KBAllocs }, { "<= 8 KB", m_stats.Large8KBAllocs }, { "<= 16 KB", m_stats.Large16KBAllocs },
        { "<= 32 KB", m_stats.Large32KBAllocs }, { "> 32 KB", m_stats.LargeHugeAllocs } };

    // Find max for scaling bars
    u64 max_count = 0;
    for (auto const& bucket : m_buckets)
        max_count = std::max(max_count, bucket.count);

    for (auto const& bucket : m_buckets) {
        if (bucket.count == 0)
            continue;

        os << "| " << std::left << std::setw(10) << bucket.name << " " << std::right << std::setw(12) << StatsFormatter::format_number(bucket.count) << " "
           << std::setw(8) << StatsFormatter::format_percent(bucket.count, m_stats.TotalAllocations, 1) << " "
           << StatsFormatter::create_bar(bucket.count, max_count, 30) << " |\n";
    }

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::print_performance_metrics(std::ostream& os) const
{
    // u64 fast_hits   =
    // stats_.FastPoolHits.load(std::memory_order_relaxed); u64
    // fast_misses = stats_.FastPoolMisses.load(std::memory_order_relaxed);
    // u64 free_hits   =
    // stats_.FreeListHits.load(std::memory_order_relaxed); u64
    // free_misses = stats_.FreeListMisses.load(std::memory_order_relaxed);

    // u64 fast_total = fast_hits + fast_misses;
    // u64 free_total = free_hits + free_misses;

    os << "\n+Performance -- Metrics "
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

    if (m_stats.ReusedAllocations > 0) {
        os << "+-------------------------------------------------------------------"
              "-----------------+\n";
        print_metric(os, "Reused Allocations", StatsFormatter::format_number(m_stats.ReusedAllocations));
    }

    if (m_stats.BlockAllocations > 0 || m_stats.BlockExpansions > 0) {
        os << "+-------------------------------------------------------------------"
              "-----------------+\n";
        print_metric(os, "Block Allocations", std::to_string(m_stats.BlockAllocations));
        print_metric(os, "Block Expansions", std::to_string(m_stats.BlockExpansions));
    }

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::print_fragmentation_analysis(std::ostream& os) const
{
    if (m_stats.InternalFragmentation == 0 && m_stats.WastedCapacity == 0)
        return;

    os << "\n+Fragmentation -- Analysis "
          "----------------------------------------------------------+\n";

    if (m_stats.InternalFragmentation > 0) {
        print_metric(os, "Internal Fragmentation", StatsFormatter::format_bytes(m_stats.InternalFragmentation));
        if (m_stats.TotalAllocated > 0)
            print_metric(os, "  (% of total)", StatsFormatter::format_percent(m_stats.InternalFragmentation, m_stats.TotalAllocated));
    }

    if (m_stats.WastedCapacity > 0)
        print_metric(os, "Wasted Capacity", StatsFormatter::format_bytes(m_stats.WastedCapacity));

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::print_timing_statistics(std::ostream& os) const
{
    if (m_stats.TotalAllocTimeNs == 0 && m_stats.TotalDeallocTimeNs == 0)
        return;

    os << "\n+Timing -- Statistics "
          "---------------------------------------------------------------+\n";

    if (m_stats.TotalAllocations > 0 && m_stats.TotalAllocTimeNs > 0) {
        u64 avg_alloc = m_stats.TotalAllocTimeNs / m_stats.TotalAllocations;
        print_metric(os, "Allocation Time (avg)", StatsFormatter::format_time(avg_alloc));
        print_metric(os, "  Min", StatsFormatter::format_time(m_stats.MinAllocTimeNs));
        print_metric(os, "  Max", StatsFormatter::format_time(m_stats.MaxAllocTimeNs));
        print_metric(os, "  Total", StatsFormatter::format_time(m_stats.TotalAllocTimeNs));
    }

    if (m_stats.TotalDeallocations > 0 && m_stats.TotalDeallocTimeNs > 0) {
        os << "+-------------------------------------------------------------------"
              "-----------------+\n";
        u64 avg_dealloc = m_stats.TotalDeallocTimeNs / m_stats.TotalDeallocations;
        print_metric(os, "Deallocation Time (avg)", StatsFormatter::format_time(avg_dealloc));
        print_metric(os, "  Total", StatsFormatter::format_time(m_stats.TotalDeallocTimeNs));
    }

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::print_error_statistics(std::ostream& os) const
{
    if (m_stats.AllocationFailures == 0 && m_stats.DoubleFreeAttempts == 0 && m_stats.InvalidFreeAttempts == 0 && m_stats.ZeroByteRequests == 0)
        return;

    os << "\n+Error -- Statistics "
          "----------------------------------------------------------------+\n";

    if (m_stats.AllocationFailures > 0)
        print_metric(os, "Allocation Failures", std::to_string(m_stats.AllocationFailures), true);

    if (m_stats.DoubleFreeAttempts > 0)
        print_metric(os, "Double-Free Attempts", std::to_string(m_stats.DoubleFreeAttempts), true);

    if (m_stats.InvalidFreeAttempts > 0)
        print_metric(os, "Invalid Free Attempts", std::to_string(m_stats.InvalidFreeAttempts), true);

    if (m_stats.ZeroByteRequests > 0)
        print_metric(os, "Zero-Byte Requests", std::to_string(m_stats.ZeroByteRequests), false);

    os << "+---------------------------------------------------------------------"
          "---------------+\n";
}

void StatsPrinter::print_metric(std::ostream& os, char const* label, std::string const& value, bool warn) const
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

void StatsPrinter::print_metric_with_bar(std::ostream& os, char const* label, u64 value, u64 max) const
{
    using std::left;
    using std::right;
    using std::setw;

    os << "│ " << left << setw(LABEL_WIDTH) << label << " " << right << setw(12) << StatsFormatter::format_number(value) << " "
       << StatsFormatter::create_bar(value, max, 20) << " │\n";
}

} // namespace fairuz
#endif // FAIRUZ_DEBUG
