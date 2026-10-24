// Runs a temporary source file through the CLI and checks process health.
#ifndef FAIRUZ_TESTS_SUPPORT_CLI_PROGRAM_HPP
#define FAIRUZ_TESTS_SUPPORT_CLI_PROGRAM_HPP

#include "test_source_program.hpp"

namespace test_cli {

inline test_process::Result run(std::filesystem::path const& input)
{
    auto result = test_process::run(test_process::executable(), { fairuz::platform::utf8(input) });
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.crashed) << result.err;
    return result;
}

} // namespace test_cli

#endif // FAIRUZ_TESTS_SUPPORT_CLI_PROGRAM_HPP
