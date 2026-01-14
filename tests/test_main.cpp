#include "test_config.h"
#include <cstring>
#include <gtest/gtest.h>
#include <string>

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

    if (arg == "--print-ast=true" /* && i + 1 < argc*/)
    {
      // FIXED: strcmp returns 0 when strings are equal
      // test_config::print_ast = (strcmp(argv[++i], "true") == 0);
      test_config::print_ast = true;
    }
  }

  std::cout << (test_config::print_ast ? "true" : "false") << std::endl;

  return RUN_ALL_TESTS();
}