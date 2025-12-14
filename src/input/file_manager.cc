#include "../../include/input/file_manager.hpp"
#include "../../include/input/error.hpp"
#include "../../utfcpp/source/utf8.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace mylang {
namespace input {

FileManager::FileManager(const std::string& filepath) :
    full_path_(filepath),
    stream_(full_path_, std::ios::binary)
{
    if (!fs::exists(fs::path(filepath)))
        throw error::file_error(to_string(FileManagerError::FILE_NOT_FOUND));
    if (!stream_.is_open())
        throw error::file_error(to_string(FileManagerError::FILE_NOT_OPEN));
}

bool FileManager::is_open() const noexcept { return stream_.is_open() && stream_.good(); }

bool FileManager::is_changed_since_last_time() const noexcept
{
    auto current_write_time = fs::last_write_time(full_path_);
    // TODO : return current_write_time != last_known_write_time;
    return false;
}

typename FileManager::Context FileManager::get_context() const noexcept { return context_; }

void FileManager::reset()
{
    if (!is_open())
        throw error::file_error(to_string(FileManagerError::FILE_NOT_FOUND));

    stream_.clear();
    stream_.seekg(0, std::ios::beg);

    if (stream_.fail())
        throw error::file_error(to_string(FileManagerError::SEEK_OUT_OF_BOUND));
    context_.reset();
}

void FileManager::seek_to_char(const std::size_t char_offset)
{
    if (char_offset == context_.char_offset)
        return;

    // TODO  : revisit this stupid magic number
    if (char_offset < context_.char_offset || char_offset - context_.char_offset > 10000)
        reset();

    while (context_.char_offset < char_offset)
    {
        const std::size_t chars_to_read = char_offset - context_.char_offset;
        std::u16string result = read_window_internal(std::min(chars_to_read, std::size_t{1024}));
        if (result.empty())
            throw error::file_error(to_string(FileManagerError::UNEXPECTED_EOF));
    }
}

void FileManager::seek_to_line(const std::size_t line_number) {}

std::u16string FileManager::read_window(const std::size_t size)
{
    // TODO : add cache validation
    return read_window_internal(size);
}

constexpr std::size_t get_utf8_sequence_length(unsigned char first_byte) 
{
    if ((first_byte & 0x80) == 0)
        return 1;
    if ((first_byte & 0xE0) == 0xC0)
        return 2;
    if ((first_byte & 0xF0) == 0xE0)
        return 3;
    if ((first_byte & 0xF8) == 0xF0)
        return 4;
    return 1;
}

std::size_t FileManager::validate_utf8_bound(std::span<const char> buffer) const {
    if (buffer.empty())
        return 0;

    const std::size_t size = buffer.size();

    for (std::size_t i = 0, n = std::min(MAX_UTF8_CHAR_BYTES, size); i < n; ++i)
    {
        const std::size_t pos = size - 1 - i;
        const unsigned char byte = static_cast<unsigned char>(buffer[pos]);

        if ((byte & 0x80) == 0)
            return 0;

        if ((byte & 0xC0) == 0xC0)
        {
            const std::size_t expected = get_utf8_sequence_length(byte);
            const std::size_t available = size - pos;
            if (available < expected)
                return i + 1;
            return 0;
        }
    }

    return std::min(MAX_UTF8_CHAR_BYTES, size);
}

std::u16string FileManager::read_window_internal(const std::size_t size)
{
    if (size == 0)
        return std::u16string{};

    if (!is_open())
        throw error::file_error(to_string(FileManagerError::FILE_NOT_OPEN));

    const std::size_t byte_chunk_size = size * MAX_UTF8_CHAR_BYTES;
    std::vector<char> byte_buffer(byte_chunk_size);
    stream_.read(byte_buffer.data(), byte_chunk_size);
    std::streamsize bytes_read = stream_.gcount();

    if (bytes_read == 0)
    {
        if (stream_.eof())
        {
            return std::u16string{};
        }
        else if (byte_buffer.empty())
        {
            throw error::file_error(to_string(FileManagerError::READ_ERROR));
        }
    }

    if (bytes_read < 0)
        throw error::file_error(to_string(FileManagerError::READ_ERROR));

    byte_buffer.resize(static_cast<std::size_t>(bytes_read));
    const std::size_t bytes_to_rewind = validate_utf8_bound(byte_buffer);

    if (bytes_to_rewind > 0)
    {
        byte_buffer.resize(bytes_read - bytes_to_rewind);
        stream_.seekg(-static_cast<std::streamoff>(bytes_to_rewind), std::ios_base::cur);

        if (stream_.fail())
            throw error::file_error(to_string(FileManagerError::SEEK_OUT_OF_BOUND));
    }

    const std::size_t valid_bytes = byte_buffer.size();
    context_.byte_offset += valid_bytes;
    // context_.bytes_read_total += valid_bytes
    std::u16string result = utf8::utf8to16(std::string(byte_buffer.begin(), byte_buffer.end()));

    if (result.size() > size)
    {
        const std::size_t chars_to_trim = result.size() - size;
        std::string trimmed_utf8;
        auto start = result.begin() + static_cast<std::ptrdiff_t>(size);
        utf8::utf16to8(start, result.end(), std::back_inserter(trimmed_utf8));
        stream_.seekg(-static_cast<std::streamoff>(trimmed_utf8.size()), std::ios_base::cur);
        context_.byte_offset -= trimmed_utf8.size();
        result.resize(size);
    }

    context_.char_offset += result.size();
    // context_.chars_read_total += result.size();

    return result;
}

std::u16string FileManager::read_line(const std::size_t line_number) {
    std::u16string line = u"";
}

std::u16string FileManager::read_next_line() {}
std::vector<std::u16string> FileManager::read_lines(const std::size_t start_line, const std::size_t count) {}
std::u16string FileManager::read_all() {}
void FileManager::refresh_stats() {}
std::size_t FileManager::get_line_count() {}
std::size_t FileManager::get_char_count() {}
char16_t FileManager::peek_char(const std::size_t char_offset) {}
std::u16string FileManager::peek_range(const std::size_t start_offset, const std::size_t length) {}
std::string FileManager::get_path() const noexcept {}


}
}
