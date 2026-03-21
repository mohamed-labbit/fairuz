#ifndef STATS_HPP
#define STATS_HPP

#ifdef MYLANG_DEBUG

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mylang {

struct DetailedAllocStats {
  std::uint64_t TotalAllocations{0};
  std::uint64_t TotalDeallocations{0};
  std::uint64_t TotalAllocated{0};
  std::uint64_t TotalDeallocated{0};
  std::uint64_t CurrentlyAllocated{0};
  std::uint64_t PeakAllocated{0};
  std::uint64_t ActiveBlocks{0};

  std::uint64_t Small8ByteAllocs{0};
  std::uint64_t Small16ByteAllocs{0};
  std::uint64_t Small32ByteAllocs{0};
  std::uint64_t Small64ByteAllocs{0};
  std::uint64_t Small128ByteAllocs{0};
  std::uint64_t Small256ByteAllocs{0};

  std::uint64_t Medium512ByteAllocs{0};
  std::uint64_t Medium1KBAllocs{0};
  std::uint64_t Medium2KBAllocs{0};
  std::uint64_t Medium4KBAllocs{0};

  std::uint64_t Large8KBAllocs{0};
  std::uint64_t Large16KBAllocs{0};
  std::uint64_t Large32KBAllocs{0};
  std::uint64_t LargeHugeAllocs{0};

  std::uint64_t BlockAllocations{0};
  std::uint64_t BlockExpansions{0};

  std::uint64_t InternalFragmentation{0};
  std::uint64_t WastedCapacity{0};

  std::uint64_t AllocationFailures{0};
  std::uint64_t DoubleFreeAttempts{0};
  std::uint64_t InvalidFreeAttempts{0};
  std::uint64_t AlignmentAdjustments{0};

  std::uint64_t TotalAllocTimeNs{0};
  std::uint64_t TotalDeallocTimeNs{0};
  std::uint64_t MinAllocTimeNs{UINT64_MAX};
  std::uint64_t MaxAllocTimeNs{0};

  std::uint64_t ReusedAllocations{0};
  std::uint64_t ZeroByteRequests{0};

  std::uint64_t LockContentions{0};
  std::uint64_t CASRetries{0};

  void recordAllocationSize(std::uint64_t size);
  void updatePeak(std::uint64_t current);
  void recordAllocTime(std::uint64_t duration_ns);
  void reset();
};

class StatsFormatter {
public:
  static std::string formatBytes(std::uint64_t bytes, int precision = 2);
  static std::string formatTime(std::uint64_t nanoseconds);
  static std::string formatPercent(std::uint64_t numerator, std::uint64_t denominator, int precision = 2);
  static std::string createBar(std::uint64_t value, std::uint64_t max, int width = 40);
  static std::string formatNumber(std::uint64_t value);
};

class StatsPrinter {
private:
  DetailedAllocStats const &stats_;
  std::string const &name_;

  static constexpr int LABEL_WIDTH = 32;
  static constexpr int VALUE_WIDTH = 18;
  static constexpr int BAR_WIDTH = 35;

public:
  StatsPrinter(DetailedAllocStats const &stats, std::string const &name) : stats_(stats), name_(name) {}

  void printDetailed(std::ostream &os, bool verbose = true) const;
  void printSummary(std::ostream &os) const;

private:
  void printHeader(std::ostream &os) const;
  void printFooter(std::ostream &os) const;
  void printCoreMetrics(std::ostream &os) const;
  void printSizeDistribution(std::ostream &os) const;
  void printPerformanceMetrics(std::ostream &os) const;
  void printFragmentationAnalysis(std::ostream &os) const;
  void printTimingStatistics(std::ostream &os) const;
  void printErrorStatistics(std::ostream &os) const;
  void printMetric(std::ostream &os, char const *label, std::string const &value, bool warn = false) const;
  void printMetricWithBar(std::ostream &os, char const *label, std::uint64_t value, std::uint64_t max) const;
};

} // namespace mylang

#endif // MYLANG_DEBUG

#endif // STATS_HPP
