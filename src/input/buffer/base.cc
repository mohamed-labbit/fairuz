#include "../../../include/input/buffer/base.hpp"
#include "../../../utfcpp/source/utf8.h"

#include <vector>


namespace mylang {
namespace lex {
namespace buffer {

bool InputBufferBase::refresh_buffer(const std::uint32_t to_refresh)
{
    auto& file = this->file_;
    if (!file->is_open()) return false;
    auto& bufs = this->buffers_;
    std::size_t max_chars = bufs[to_refresh].size() - 1;
    auto buf = read_wchar_window(max_chars);

    if (buf.empty())
    {
        bufs[to_refresh].clear();
        bufs[to_refresh].push_back(BUFFER_END);
        return false;
    }

    bufs[to_refresh].assign(buf.begin(), buf.end());
    bufs[to_refresh].push_back(BUFFER_END);
    return true;
}

typename InputBufferBase::buffer_t InputBufferBase::read_wchar_window(std::size_t max_chars)
{
    auto& file = this->file_;
    auto& byte_pos = this->byte_position_;
    auto& char_count = this->char_count_;

    if (max_chars == 0) return {};
    if (!file->is_open()) return {};

    std::size_t byte_chunk_size = max_chars * 4;
    std::vector<char> byte_buffer(byte_chunk_size);
    file->read(byte_buffer.data(), byte_chunk_size);
    std::streamsize bytes_read = file->gcount();
    if (bytes_read <= 0) return {};

    // validate the buffer bytes
    byte_buffer.resize(bytes_read);
    std::size_t valid_bytes = bytes_read;

    if (bytes_read > 0)
    {
        unsigned char last_byte = static_cast<unsigned char>(byte_buffer[bytes_read - 1]);
        std::int32_t bytes_to_rewind = 0;

        for (std::int32_t i = bytes_read - 1; i >= 0 && i >= bytes_read - 4; --i)
        {
            unsigned char byte = static_cast<unsigned char>(byte_buffer[i]);
            if ((byte & 0x80) == 0)
                break;
            else if ((byte & 0xC0) == 0xC0)
            {
                std::int32_t expected_bytes = 0;
                if ((byte & 0xE0) == 0xC0)
                    expected_bytes = 2;
                else if ((byte & 0xF0) == 0xE0)
                    expected_bytes = 3;
                else if ((byte & 0xF8) == 0xF0)
                    expected_bytes = 4;
                if (i + expected_bytes > bytes_read) bytes_to_rewind = bytes_read - i;
                break;
            }
        }

        if (bytes_to_rewind > 0)
        {
            valid_bytes = bytes_read - bytes_to_rewind;
            byte_buffer.resize(valid_bytes);
            file->seekg(-bytes_to_rewind, std::ios_base::cur);
        }
    }

    byte_pos += valid_bytes;
    std::string u8_str;

    for (auto b : byte_buffer)
        u8_str.push_back(b);

    buffer_t result = utf8::utf8to16(u8_str);
    if (result.size() > max_chars) result.resize(max_chars);

    char_count += result.size();
    return result;
}

}
}  // lex
}  // mylang
