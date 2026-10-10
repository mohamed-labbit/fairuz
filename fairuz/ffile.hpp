#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace fairuz {

// File I/O failures carry data only; callers decide how to report them.
class FileError : public std::runtime_error {
public:
    enum class Kind {
        NotFound,
        NotOpen,
        Read
    };

    FileError(Kind kind, std::string path);
    Kind kind() const noexcept { return m_kind; }
    std::string const& path() const noexcept { return m_path; }

private:
    Kind m_kind;
    std::string m_path;
};

class FileManager {
public:
    FileManager() = default;
    explicit FileManager(std::string const& filepath);

    std::string load(std::string const& filepath, bool replace = false);
    std::string& buffer() noexcept { return m_input_buffer; }
    std::string const& buffer() const noexcept { return m_input_buffer; }
    std::string const& get_path() const noexcept { return m_file_path; }
    std::string_view get_line_at(size_t line_number) const noexcept;

private:
    std::string m_file_path;
    std::string m_input_buffer;
};

} // namespace fairuz
