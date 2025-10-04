#include "lex/source_manager.h"


typename SourceManager::size_type SourceManager::line() const { return this->input_buffer_.position().line_; }

typename SourceManager::size_type SourceManager::column() const { return this->input_buffer_.position().column_; }

Position SourceManager::position() const { return this->input_buffer_.position(); }

bool SourceManager::done() const { return this->input_buffer_.empty(); }

typename SourceManager::char_type SourceManager::peek() { return this->input_buffer_.peek(); }

typename SourceManager::char_type SourceManager::consume_char() { return this->input_buffer_.consume_char(); }

typename SourceManager::char_type SourceManager::current() { return input_buffer_.current(); }

Position SourceManager::offset_map_(const size_type& offset) const {
    if (offset == this->input_buffer_.buffer_offset())
    {
        return this->input_buffer_.position();
    }

    size_type iter = 0;
    size_type diff = 0;

    // Count lines before buffer start
    while (iter < this->input_buffer_.buffer_offset())
    {
        if (this->input_buffer_.at(iter) == u'\n')
        {
            diff += 1;
        }

        iter += 1;
    }

    size_type base_line = this->input_buffer_.position().line_ - diff;

    iter           = 0;
    size_type line = 1;
    size_type col  = 1;

    const size_type limit = std::min(offset, this->input_buffer_.size() - 1);

    while (iter < limit)
    {
        char_type c = this->input_buffer_.at(iter);

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

    return Position(line, col);
}

Position SourceManager::offset_map(const size_type& offset) {
    if (offset == this->input_buffer_.buffer_offset())
    {
        return this->input_buffer_.position();
    }

    if (offset >= this->file_.tellg())
    {
        return Position(this->input_buffer_.position().line_, this->input_buffer_.position().column_);
    }

    this->file_.imbue(std::locale(this->file_.getloc()));

    size_type line           = 1;
    size_type col            = 1;
    size_type current_offset = 0;

    char c;
    while (this->file_.get(c))
    {
        if (current_offset == offset)
        {
            return Position(line, col);
        }

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

    this->file_.close();
    return Position(line, col);
}