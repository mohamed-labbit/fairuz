//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Provides cross-platform filesystem and process helpers.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_PLATFORM_HPP
#define FAIRUZ_PLATFORM_HPP

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace fairuz::platform {

inline void configure_locale()
{
    try {
        std::locale::global(std::locale("ar_SA.UTF-8"));
    } catch (std::runtime_error const&) {
        std::locale::global(std::locale::classic());
    }
}

inline std::filesystem::path path(std::string_view utf8)
{
    if (utf8.empty()) return { };
    return std::filesystem::path(std::u8string(reinterpret_cast<char8_t const*>(utf8.data()), utf8.size()));
}

inline std::string utf8(std::filesystem::path const& value)
{
    auto text = value.generic_u8string();
    return { reinterpret_cast<char const*>(text.data()), text.size() };
}

FILE* open_file(std::string_view filename, char const* mode);
std::string environment(char const* name);
bool write_file_atomic(std::string const& filename, char const* data, size_t size, std::string& error);

} // namespace fairuz::platform

#endif // FAIRUZ_PLATFORM_HPP
