#pragma once

#include <algorithm>
#include <codecvt>
#include <iterator>
#include <locale>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace utf8 {

struct invalid_utf8 : public std::runtime_error
{
    explicit invalid_utf8(const std::string& msg) : std::runtime_error(msg) {}
};

inline std::string utf16to8(const std::u16string& input)
{
    try
    {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
        return conv.to_bytes(input);
    } catch (const std::range_error& e)
    {
        throw invalid_utf8(e.what());
    }
}

inline std::u16string utf8to16(const std::string& input)
{
    try
    {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
        return conv.from_bytes(input);
    } catch (const std::range_error& e)
    {
        throw invalid_utf8(e.what());
    }
}

template<typename InputIt, typename OutputIt>
inline void utf16to8(InputIt first, InputIt last, OutputIt out)
{
    static_assert(std::is_convertible_v<typename std::iterator_traits<InputIt>::value_type, char16_t>,
      "utf16to8 expects UTF-16 code units");
    const std::u16string temp(first, last);
    const std::string converted = utf16to8(temp);
    std::copy(converted.begin(), converted.end(), out);
}

}  // namespace utf8
