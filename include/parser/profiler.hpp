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
    string_type phase;
    double milliseconds;
  };

  std::vector<Timing> Timings_;

 public:
  void recordPhase(const string_type& phase, double ms);

  void printReport() const;
};

}
}