//
// File.cc
//

#include "File.hpp"
#include "Platform.hpp"

#include <fstream>
#include <iterator>
#include <utility>

namespace fairuz {

FileError::FileError(Kind kind, std::string path)
    : std::runtime_error((kind == Kind::NotFound ? "File not found: " : kind == Kind::NotOpen ? "File is not open: "
                                                                                              : "Failed to read from file: ")
          + path)
    , m_kind(kind)
    , m_path(std::move(path))
{
}

FileManager::FileManager(std::string const& filepath)
{
    load(filepath, true);
}

std::string FileManager::load(std::string const& filepath, bool replace)
{
    auto path = platform::path(filepath);
    std::error_code ec;
    bool exists = std::filesystem::exists(path, ec);
    if (!exists && !ec)
        throw FileError(FileError::Kind::NotFound, filepath);
    if (ec || !std::filesystem::is_regular_file(path, ec) || ec)
        throw FileError(FileError::Kind::NotOpen, filepath);

    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw FileError(FileError::Kind::NotOpen, filepath);
    std::string content;
    try {
        content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    } catch (std::ios_base::failure const&) {
        throw FileError(FileError::Kind::Read, filepath);
    }
    if (in.bad())
        throw FileError(FileError::Kind::Read, filepath);

    // Preserve every byte, including trailing whitespace and malformed UTF-8.
    // Commit only after a successful read so failed reloads preserve the source.
    if (replace || m_file_path.empty()) {
        std::string new_path = filepath;
        m_input_buffer = content;
        m_file_path.swap(new_path);
    }
    return content;
}

std::string_view FileManager::get_line_at(size_t number) const noexcept
{
    if (number == 0)
        return { };
    std::string_view text(m_input_buffer);
    size_t start = 0;
    for (size_t line = 1; line < number; ++line) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos)
            return { };
        start = end + 1;
    }
    auto end = text.find('\n', start);
    if (end == std::string_view::npos)
        end = text.size();
    if (end > start && text[end - 1] == '\r')
        --end;
    return text.substr(start, end - start);
}

} // namespace fairuz
