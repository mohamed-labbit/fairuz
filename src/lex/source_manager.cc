#include "../../include/lex/source_manager.h"


namespace mylang {
namespace lex {


using offset_pair = std::pair<std::size_t, std::size_t>;

std::size_t SourceManager::line() const { return this->input_buffer_.position().line(); }

std::size_t SourceManager::column() const { return this->input_buffer_.position().column(); }

std::size_t SourceManager::fpos() const { return this->input_buffer_.position().fpos(); }

const std::string& SourceManager::fpath() const { return std::cref<std::string>(this->filepath_); }

buffer::Position SourceManager::position() const { return this->input_buffer_.position(); }

bool SourceManager::done() const { return this->input_buffer_.empty(); }

char16_t SourceManager::peek() { return this->input_buffer_.peek(); }

char16_t SourceManager::consume_char() { return this->input_buffer_.consume_char(); }

char16_t SourceManager::current() { return input_buffer_.current(); }

offset_pair SourceManager::offset_map_(const std::size_t& offset) const
{
    auto& buf = this->input_buffer_;
    if (offset == buf.buffer_offset())
        return std::make_pair(buf.position().line(), buf.position().column());
    std::size_t iter = 0;
    std::size_t diff = 0;
    // Count lines before buffer start
    while (iter < buf.buffer_offset())
    {
        if (buf.at(iter) == u'\n')
            diff += 1;
        iter += 1;
    }
    std::size_t base_line = buf.position().line() - diff;
    iter = 0;
    std::size_t line = 1;
    std::size_t col = 1;
    const std::size_t limit = std::min(offset, buf.size() - 1);
    while (iter < limit)
    {
        char16_t c = buf.at(iter);
        if (c == u'\n')
        {
            line += 1;
            col = 1;
        }
        else
        {
            col += 1;
        }
        iter += 1;
    }
    // combine with base line
    line = base_line + (line - 1);
    return std::make_pair(line, col);
}

offset_pair SourceManager::offset_map(const std::size_t& offset)
{
    auto& buf = this->input_buffer_;
    auto& file = this->file_;
    if (offset == buf.buffer_offset())
        return std::make_pair(buf.position().line(), buf.position().column());
    if (offset >= file.tellg())
        return std::make_pair(buf.position().line(), buf.position().column());
    file.imbue(std::locale(file.getloc()));
    std::size_t line = 1;
    std::size_t col = 1;
    std::size_t current_offset = 0;
    char c;
    while (file.get(c))
    {
        if (current_offset == offset)
            return std::make_pair(line, col);
        if (c == L'\n')
        {
            line += 1;
            col = 1;
        }
        else
        {
            col += 1;
        }
        current_offset++;
    }
    file.close();
    return std::make_pair(line, col);
}

}  // lex
}  // mylang
