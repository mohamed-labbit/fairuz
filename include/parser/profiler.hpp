#pragma once

#include "../macros.hpp"
#include "../types/string.hpp"

#include <vector>

namespace mylang {
namespace parser {

class ParserProfiler {
private:
    struct Timing {
        StringRef phase;
        double milliseconds;
    };

    std::vector<Timing> Timings_;

public:
    void recordPhase(StringRef const& phase, double ms);

    void printReport() const;
};

}
}
