find ./src -type f \( -name "*.hpp" -o -name "*.cc" -o -name "*.cpp" \) -print -exec clang-format -i {} \;
find ./include -type f \( -name "*.hpp" -o -name "*.cc" -o -name "*.cpp" \) -print -exec clang-format -i {} \;
find ./tests -type f \( -name "*.hpp" -o -name "*.cc" -o -name "*.cpp" \) -print -exec clang-format -i {} \;