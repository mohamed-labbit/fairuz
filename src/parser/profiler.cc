#include "../../include/parser/profiler.hpp"
#include "../../utfcpp/source/utf8.h"

#include <iomanip>
#include <iostream>


namespace mylang {
namespace parser {

void ParserProfiler::recordPhase(const StringRef& phase, double ms) { Timings_.push_back({phase, ms}); }

void ParserProfiler::printReport() const
{
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════╗\n";
  std::cout << "║      Parser Performance Report        ║\n";
  std::cout << "╚═══════════════════════════════════════╝\n\n";
  double total = 0;
  for (const ParserProfiler::Timing& t : Timings_)
  {
    total += t.milliseconds;
  }
  for (const ParserProfiler::Timing& t : Timings_)
  {
    double percent = (t.milliseconds / total) * 100;
    std::cout << std::left << std::setw(20) << utf8::utf16to8(t.phase) << ": " << std::right << std::setw(8) << std::fixed << std::setprecision(2)
              << t.milliseconds << "ms "
              << "(" << std::setw(5) << percent << "%)\n";
  }
  std::cout << std::left << std::setw(20) << "Total" << ": " << std::right << std::setw(8) << total << "ms\n";
}

}
}