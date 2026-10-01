#ifndef FAIRUZ_TEST_INTEGER_ORACLE_HPP
#define FAIRUZ_TEST_INTEGER_ORACLE_HPP

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// An independent decimal oracle: no compiler-specific 128-bit type and no
// calls into the integer implementation being tested.
namespace integer_oracle {

inline std::string magnitude(int64_t value)
{
    uint64_t bits = static_cast<uint64_t>(value);
    return std::to_string(value < 0 ? uint64_t { 0 } - bits : bits);
}
inline std::string add(int64_t a, int64_t b, bool subtract = false)
{
    std::string lhs = magnitude(a), rhs = magnitude(b);
    bool negative = a < 0, rhs_negative = (b < 0) != subtract;
    bool difference = negative != rhs_negative;
    if (difference && (lhs.size() < rhs.size() || (lhs.size() == rhs.size() && lhs < rhs))) {
        std::swap(lhs, rhs);
        negative = rhs_negative;
    }
    std::reverse(lhs.begin(), lhs.end());
    std::reverse(rhs.begin(), rhs.end());
    size_t size = std::max(lhs.size(), rhs.size());
    lhs.resize(size, '0');
    rhs.resize(size, '0');
    std::string result;
    int carry = 0;
    for (size_t i = 0; i < size; ++i) {
        int digit = lhs[i] - '0' + (difference ? -(rhs[i] - '0') : rhs[i] - '0') + carry;
        carry = difference ? (digit < 0 ? -1 : 0) : digit / 10;
        if (digit < 0)
            digit += 10;
        result.push_back(static_cast<char>('0' + digit % 10));
    }
    if (!difference && carry)
        result.push_back(static_cast<char>('0' + carry));
    while (result.size() > 1 && result.back() == '0')
        result.pop_back();
    if (negative && result != "0")
        result.push_back('-');
    std::reverse(result.begin(), result.end());
    return result;
}
inline std::string multiply(int64_t a, int64_t b)
{
    auto lhs = magnitude(a), rhs = magnitude(b);
    std::reverse(lhs.begin(), lhs.end());
    std::reverse(rhs.begin(), rhs.end());
    std::vector<unsigned> digits(lhs.size() + rhs.size());
    for (size_t i = 0; i < lhs.size(); ++i)
        for (size_t j = 0; j < rhs.size(); ++j)
            digits[i + j] += unsigned(lhs[i] - '0') * unsigned(rhs[j] - '0');
    for (size_t i = 0; i + 1 < digits.size(); ++i) {
        digits[i + 1] += digits[i] / 10;
        digits[i] %= 10;
    }
    while (digits.size() > 1 && !digits.back())
        digits.pop_back();
    std::string result;
    if ((a < 0) != (b < 0) && a && b)
        result.push_back('-');
    for (auto it = digits.rbegin(); it != digits.rend(); ++it)
        result.push_back(static_cast<char>('0' + *it));
    return result;
}

}
#endif
