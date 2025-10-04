#include "lex/buffer/base.h"
#include "../utfcpp/source/utf8.h"


namespace mylang {
namespace lex {

bool InputBase::refresh_buffer(const unsigned int to_refresh)
{
    if (!file_.is_open())
    {
        return false;
    }

    size_type max_chars = buffers_[to_refresh].size() - 1;
    auto      buf       = read_wchar_window(max_chars);

    if (buf.empty())
    {
        buffers_[to_refresh].clear();
        buffers_[to_refresh].push_back(BUF_END);
        return false;
    }

    buffers_[to_refresh].assign(buf.begin(), buf.end());
    buffers_[to_refresh].push_back(BUF_END);

    return true;
}

typename InputBase::buffer_t InputBase::read_wchar_window(size_type max_chars)
{
    if (max_chars == 0)
    {
        return {};
    }

    if (!file_.is_open())
    {
        return {};
    }

    size_type         byte_chunk_size = max_chars * 4;  // Conservative estimate
    std::vector<char> byte_buffer(byte_chunk_size);

    file_.read(byte_buffer.data(), byte_chunk_size);
    std::streamsize bytes_read = file_.gcount();

    if (bytes_read <= 0)
    {
        return {};
    }

    byte_buffer.resize(bytes_read);

    size_type valid_bytes = bytes_read;

    if (bytes_read > 0)
    {
        unsigned char last_byte = static_cast<unsigned char>(byte_buffer[bytes_read - 1]);

        int bytes_to_rewind = 0;

        for (int i = bytes_read - 1; i >= 0 && i >= bytes_read - 4; --i)
        {
            unsigned char byte = static_cast<unsigned char>(byte_buffer[i]);

            if ((byte & 0x80) == 0)
            {
                break;
            }
            else if ((byte & 0xC0) == 0xC0)
            {
                int expected_bytes = 0;
                if ((byte & 0xE0) == 0xC0)
                {
                    expected_bytes = 2;
                }
                else if ((byte & 0xF0) == 0xE0)
                {
                    expected_bytes = 3;
                }
                else if ((byte & 0xF8) == 0xF0)
                {
                    expected_bytes = 4;
                }

                if (i + expected_bytes > bytes_read)
                {
                    bytes_to_rewind = bytes_read - i;
                }
                break;
            }
        }

        if (bytes_to_rewind > 0)
        {
            valid_bytes = bytes_read - bytes_to_rewind;
            byte_buffer.resize(valid_bytes);
            file_.seekg(-bytes_to_rewind, std::ios_base::cur);
        }
    }

    byte_position_ += valid_bytes;

    std::string u8_str;
    for (auto b : byte_buffer)
    {
        u8_str.push_back(b);
    }

    buffer_t result = utf8::utf8to16(u8_str);

    if (result.size() > max_chars)
    {
        result.resize(max_chars);
    }

    char_count_ += result.size();

    return result;
}

}  // lex
}  // mylang
