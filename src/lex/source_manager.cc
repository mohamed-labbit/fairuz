/*
#include "lex/source_manager.h"
#include "macros.h"

typename SourceManager::size_type SourceManager::line() const {
    Position pos = input_buffer_.position();
    return pos.line_;
}

typename SourceManager::size_type SourceManager::column() const {
    Position pos = input_buffer_.position();
    return pos.column_;
}

// char_type current() const { return offset_ < source_.length() ? source_[offset_] : EOF; }
bool SourceManager::done() const { return this->input_buffer_.data() == nullptr; }

typename SourceManager::char_type SourceManager::peek() { return input_buffer_.peek(); }

typename SourceManager::char_type SourceManager::consume_char() { return input_buffer_.consume_char(); }

*/