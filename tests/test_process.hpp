#ifndef FAIRUZ_TEST_PROCESS_HPP
#define FAIRUZ_TEST_PROCESS_HPP

#include "fplatform.hpp"
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace test_process {

struct Result {
    int exit_code = -1;
    std::string out;
    std::string err;
    bool timed_out = false;
    bool crashed = false;
};
std::filesystem::path temporary_directory();
unsigned long process_id();
inline std::filesystem::path executable() { return fairuz::platform::path(FAIRUZ_TEST_EXECUTABLE); }
Result run(std::filesystem::path const& executable, std::vector<std::string> const& args,
    std::filesystem::path const& working_directory = { },
    std::chrono::seconds timeout = std::chrono::seconds(15));

}
#endif
