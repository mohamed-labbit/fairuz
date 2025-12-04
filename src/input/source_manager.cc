#include "../../include/input/source_manager.hpp"

#include <functional>


namespace mylang {
namespace lex {

using offset_pair = std::pair<std::size_t, std::size_t>;

std::size_t SourceManager::line() const
{
    if (use_file_buffer_ && input_buffer_.has_value())
        return this->input_buffer_.value().position().line;
    return current_position_.has_value() ? current_position_.value().line : 0;
}

std::size_t SourceManager::column() const
{
    if (use_file_buffer_ && input_buffer_.has_value())
        return this->input_buffer_.value().position().column;
    return current_position_.has_value() ? current_position_.value().column : 0;
}

std::size_t SourceManager::fpos() const
{
    if (use_file_buffer_ && input_buffer_.has_value())
        return this->input_buffer_.value().position().filepos;
    return current_position_.has_value() ? current_position_.value().filepos : 0;
}

const std::string SourceManager::fpath() const
{
    if (use_file_buffer_)
        return this->filepath_;
    return "";
}

buffer::Position SourceManager::position() const
{
    if (use_file_buffer_ && input_buffer_.has_value())
        return this->input_buffer_.value().position();
    return current_position_.value_or(buffer::Position());
}

bool SourceManager::done() const
{
    if (use_file_buffer_ && input_buffer_.has_value())
        return this->input_buffer_.value().empty();
    return source_.has_value() ? source_.value().empty() : true;
}

char16_t SourceManager::peek()
{
    if (use_file_buffer_ && input_buffer_.has_value())
        return this->input_buffer_.value().peek();
    auto& cur = this->current_.value();
    if (cur == nullptr || *cur == BUFFER_END)
        return BUFFER_END;
    char16_t* forward = cur + 1;
    return *forward;
}

char16_t SourceManager::consume_char()
{
    if (use_file_buffer_ && input_buffer_.has_value())
        return this->input_buffer_.value().consume_char();

    char16_t ret = current();
    if (current_.value() != source_.value().data() + source_.value().length() && current_.value() != nullptr)
    {
        auto& cur = current_.value();
        auto& pos = current_position_.value();
        pos.filepos += 1;

        if (*cur == u'\n')
        {
            pos.line += 1;
            pos.column = 1;
            // enable if necessary
            // cols.push(1);
        }
        else
        {
            pos.column += 1;
            /* enable if necessary
            if (!cols.empty())
            {
                cols.top() = cur_pos.column;
            }
            else
            {
                cols.push(cur_pos.column);
            }
            */
        }

        cur++;
    }

    return ret;
}

char16_t SourceManager::current()
{
    if (use_file_buffer_ && input_buffer_.has_value())
        return input_buffer_.value().current();
    return current_.value() ? *current_.value() : BUFFER_END;
}

offset_pair SourceManager::offset_map_(const std::size_t& offset) const
{
    if (!use_file_buffer_)
    {
        // TODO : implement for normal string buffer
    }

    auto& buf = this->input_buffer_.value();
    if (offset == buf.buffer_offset())
        return std::make_pair(buf.position().line, buf.position().column);

    std::size_t iter = 0;
    std::size_t diff = 0;
    // Count lines before buffer start
    while (iter < buf.buffer_offset())
    {
        if (buf.at(iter) == u'\n')
            diff += 1;
        iter += 1;
    }

    std::size_t base_line = buf.position().line - diff;
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
    // TODO : implement this using the new file manager
    std::size_t line = 1;
    std::size_t col = 1;
    return std::make_pair(line, col);
}

}  // lex
}  // mylang
