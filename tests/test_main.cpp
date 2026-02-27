#include "../include/IR/value.hpp"
#include "../include/ast/AST_allocator.hpp"
#include "../include/runtime/runtime_allocator.hpp"
#include "../include/types/string_allocator.hpp"
#include "test_config.h"

#include <cstring>
#include <gtest/gtest.h>
#include <string>

using namespace mylang;

namespace test_config {

bool print_ast = false;
bool verbose = false;

}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    // Parse custom arguments (after gtest consumes its own)
    for (int i = 1; i < argc; ++i) {
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

    if (test_config::verbose) {
        std::cout << string_allocator.toString(true) << '\n';
        std::cout << '\n';
        std::cout << ast::AST_allocator.toString(true) << '\n';
        std::cout << '\n';
        std::cout << IR::object_allocator.toString(true) << '\n';
        std::cout << '\n';
        std::cout << runtime::runtime_allocator.toString(true) << std::endl;
    }

    return ret;
}
