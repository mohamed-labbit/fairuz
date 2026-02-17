#include "../../include/lex/source_manager.hpp"

#include <functional>
#include <iostream>

namespace mylang {
namespace lex {

void SourceManager::reset()
{
    Context_.line = 1;
    Context_.column = 1;
    Context_.offset = 0;
    Current_ = BUFFER_END;

    while (!UngetStack_.empty())
        UngetStack_.pop();

    if (FileManager_ && FileManager_->buffer().len() > 0) {
        std::size_t bytes = 0;
        uint32_t cp = util::decode_utf8_at(FileManager_->buffer(), 0, &bytes);
        Current_ = cp;
        CurrentBytes_ = bytes;
    }
}

uint32_t SourceManager::peekChar()
{
    if (!UngetStack_.empty())
        return UngetStack_.top().ch;

    auto saved = Context_;
    auto saved_current = Current_;
    uint32_t cp = nextChar();

    if (cp != BUFFER_END)
        unget(cp);
    else {
        Context_ = saved;
        Current_ = saved_current;
    }

    return cp;
}

uint32_t SourceManager::currentChar() const
{
    if (Context_.offset >= FileManager_->buffer().len())
        return BUFFER_END;

    std::size_t bytes = 0;
    return util::decode_utf8_at(FileManager_->buffer(), Context_.offset, &bytes);
}

void SourceManager::consumeChar()
{
    if (Context_.offset >= FileManager_->buffer().len())
        return;

    std::size_t bytes = 0;
    uint32_t cp = util::decode_utf8_at(FileManager_->buffer(), Context_.offset, &bytes);
    advance(cp, bytes);
}

// returns codepoint next to offset + 1
uint32_t SourceManager::nextChar()
{
    consumeChar();
    return currentChar();
}

void SourceManager::unget(uint32_t const cp)
{
    PushbackEntry e;
    e.ch = cp;
    e.ctx = Context_;
    e.bytes = CurrentBytes_;

    rewindPosition_(cp, e.bytes);
    UngetStack_.push(e);
}

void SourceManager::advance(uint32_t const cp, std::size_t const bytes)
{
    Context_.offset += bytes;

    if (cp == '\n') {
        ++Context_.line;
        Context_.column = 1;
    } else
        ++Context_.column;
}

void SourceManager::rewindPosition_(uint32_t const cp, std::size_t const bytes)
{
    if (Context_.offset < bytes)
        throw std::runtime_error("SourceManager: attempted to rewind past beginning of file");

    Context_.offset -= bytes;

    if (cp == '\n') {
        Context_.line = (Context_.line > 1) ? (Context_.line - 1) : 1;
        Context_.column = calculateColumnAtOffset(Context_.offset);
    } else
        Context_.column = (Context_.column > 1) ? (Context_.column - 1) : 1;
}

std::size_t SourceManager::calculateColumnAtOffset(std::size_t const target_offset) const
{
    StringRef const& buf = FileManager_->buffer();

    std::size_t line_start = target_offset;
    while (line_start > 0) {
        if (buf.data()[line_start - 1] == '\n')
            break;

        --line_start;
    }

    std::size_t column = 1;
    std::size_t pos = line_start;

    while (pos < target_offset) {
        std::size_t bytes = 0;
        util::decode_utf8_at(buf, pos, &bytes);
        pos += bytes;
        ++column;
    }

    return column;
}

} // lex
} // mylang
