#include "../fairuz/../fairuz/arena.hpp"
#include "test_config.h"

#include <gtest/gtest.h>

using namespace fairuz;

namespace test_config {

bool print_ast = false;
bool verbose = false;

} // namespace test_config

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    fairuz::Fa_AllocatorContext g_ctx;
    fairuz::set_context(&g_ctx);

    for (int i = 1; i < argc; i += 1) {
        std::string arg = argv[i];

        if (arg == "--print-ast")
            test_config::print_ast = true;
        else if (arg == "-v")
            test_config::verbose = true;
        else {
            std::cerr << "main: unknown option " << arg << std::endl;
            return 1;
        }
    }

    int ret = RUN_ALL_TESTS();

#ifdef FAIRUZ_DEBUG

    if (test_config::verbose) {
        std::cout << get_allocator().to_string(true) << '\n';
        std::cout << '\n';
        std::cout << get_allocator().to_string(true) << '\n';
        std::cout << '\n';
        std::cout << get_allocator().to_string(true) << '\n';
        std::cout << '\n';
        std::cout << get_allocator().to_string(true) << std::endl;
    }
#endif // FAIRUZ_DEBUG
    fairuz::g_context = nullptr;

    return ret;
}
