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
    StringType phase;
    double     milliseconds;
  };

  std::vector<Timing> Timings_;

 public:
  void recordPhase(const StringType& phase, double ms);

  void printReport() const;
};

}
}