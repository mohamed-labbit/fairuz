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
        SizeType bytes = 0;
        uint32_t cp = util::decode_utf8_at(FileManager_->buffer(), 0, &bytes);
        Current_ = cp;
        CurrentBytes_ = bytes;
    }
}

MYLANG_NODISCARD uint32_t SourceManager::peekChar()
{
    if (!UngetStack_.empty())
        return UngetStack_.top().ch;

    auto saved = Context_;
    auto saved_current = Current_;
    uint32_t cp = nextChar();

    // Only unget if we actually read a character
    if (cp != BUFFER_END)
        unget(cp);
    else {
        // Restore state if we hit end of buffer
        Context_ = saved;
        Current_ = saved_current;
    }

    return cp;
}

uint32_t SourceManager::currentChar() const
{
    if (Context_.offset >= FileManager_->buffer().len())
        return BUFFER_END;

    SizeType bytes = 0;
    return util::decode_utf8_at(FileManager_->buffer(), Context_.offset, &bytes);
}

void SourceManager::consumeChar()
{
    if (Context_.offset >= FileManager_->buffer().len())
        return;

    SizeType bytes = 0;
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
    // We need to restore to the state BEFORE reading cp
    // The current Context_ is AFTER reading cp

    PushbackEntry e;
    e.ch = cp;
    e.ctx = Context_;        // This is the state AFTER reading cp
    e.bytes = CurrentBytes_; // Bytes used by this character

    rewindPosition_(cp, e.bytes);
    UngetStack_.push(e);
}

void SourceManager::advance(uint32_t const cp, SizeType const bytes)
{
    Context_.offset += bytes;

    if (cp == '\n') {
        ++Context_.line;
        Context_.column = 1;
    } else
        ++Context_.column;
}

void SourceManager::rewindPosition_(uint32_t const cp, SizeType const bytes)
{
    // Sanity check: don't rewind past the beginning
    if (Context_.offset < bytes)
        throw std::runtime_error("SourceManager: attempted to rewind past beginning of file");

    Context_.offset -= bytes;

    if (cp == '\n') {
        // Rewinding past a newline - go back to previous line
        // We need to recalculate the column by scanning back
        Context_.line = (Context_.line > 1) ? (Context_.line - 1) : 1;

        // Find the column position by scanning from start of line
        Context_.column = calculateColumnAtOffset(Context_.offset);
    } else
        // Regular character rewind - just decrement column
        Context_.column = (Context_.column > 1) ? (Context_.column - 1) : 1;
}

SizeType SourceManager::calculateColumnAtOffset(SizeType const target_offset) const
{
    StringRef const& buf = FileManager_->buffer();

    // Scan backwards to find the start of the current line
    SizeType line_start = target_offset;
    while (line_start > 0) {
        if (buf.data()[line_start - 1] == '\n')
            break;
        --line_start;
    }

    // Count UTF-8 characters from line start to target offset
    SizeType column = 1;
    SizeType pos = line_start;

    while (pos < target_offset) {
        SizeType bytes = 0;
        // We just need to count characters, don't need the actual codepoint
        util::decode_utf8_at(buf, pos, &bytes);
        pos += bytes;
        ++column;
    }

    return column;
}

} // lex
} // mylang
