#include "../fairuz/../fairuz/farena.hpp"
#include "test_config.h"

#include <gtest/gtest.h>

using namespace fairuz;

namespace test_config {

bool print_ast = false;
bool verbose = false;
bool dump_bytecode = false;

} // namespace test_config

class QuietOutputListener : public ::testing::EmptyTestEventListener {
    void OnTestStart(::testing::TestInfo const&) override
    {
        ::testing::internal::CaptureStdout();
        ::testing::internal::CaptureStderr();
    }

    void OnTestEnd(::testing::TestInfo const& test_info) override
    {
        std::string out = ::testing::internal::GetCapturedStdout();
        std::string err = ::testing::internal::GetCapturedStderr();
        // Replay only on failure -- keeps green runs silent, but you
        // still get the diagnostic text when a test actually breaks.
        if (test_info.result()->Failed()) {
            if (!out.empty()) std::cout << out;
            if (!err.empty()) std::cerr << err;
        }
    }
};

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    
    fairuz::Fa_AllocatorContext g_ctx;
    fairuz::set_context(&g_ctx);

    for (int i = 1; i < argc; i += 1) {
        std::string arg = argv[i];

        if (arg == "--print-ast")
            test_config::print_ast = true;
        else if (arg == "--dump-bytecode")
            test_config::dump_bytecode = true;
        else if (arg == "-v")
            test_config::verbose = true;
        else {
            std::cerr << "main: unknown option " << arg << std::endl;
            return 1;
        }
    }

    if (!test_config::dump_bytecode && !test_config::verbose)
        ::testing::UnitTest::GetInstance()->listeners().Append(new QuietOutputListener());

    int ret = RUN_ALL_TESTS();
    fairuz::g_context = nullptr;

    return ret;
}
