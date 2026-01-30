#include "../include/types.hpp"
#include "test_config.h"
#include <cstring>
#include <gtest/gtest.h>
#include <string>

using namespace mylang;

namespace test_config {

bool print_ast = false;

}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  // Parse custom arguments (after gtest consumes its own)
  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];

    if (arg == "--print-ast")
    {
      test_config::print_ast = true;
    }
    else
    {
      std::cerr << "main: unknown option " << arg << std::endl;
      return 1;
    }
  }

  if (RUN_ALL_TESTS() == 0)
    std::cout << string_allocator.toString(true) << std::endl;

  return 0;
}
