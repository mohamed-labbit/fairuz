#pragma once

#include "../macros.hpp"

#include <vector>


namespace mylang {
namespace parser {

class ParserProfiler
{
 private:
  struct Timing
  {
    StringRef phase;
    double    milliseconds;
  };

  std::vector<Timing> Timings_;

 public:
  void recordPhase(const StringRef& phase, double ms);

  void printReport() const;
};

}
}