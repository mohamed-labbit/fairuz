#include "../../../include/input/buffer/input_buffer.hpp"
#include <algorithm>
#include <cassert>

namespace mylang {
namespace lex {
namespace buffer {

CharType InputBuffer::consumeChar()
{
    CharType ch;

    if (!UngetStack_.empty()) {
        // Restore the complete state from unget stack
        auto entry = UngetStack_.top();
        UngetStack_.pop();

        CurrentPosition_ = entry.pos;
        ch = entry.ch;

        // Note: We don't need to restore Current_ pointer because
        // the next consumeChar() or current() call will handle buffer state correctly.
        // We just return the ungotten character.
        return ch;
    }

    // Ensure buffer is valid before reading
    if (*Current_ == BUFFER_END) {
        if (!refreshBuffer(CurrentBuffer_ ^ 1))
            return BUFFER_END;
        swapBuffers_();
    }

    ch = *Current_;
    ++Current_;
    advancePosition_(ch);

    return ch;
}

MYLANG_NODISCARD const CharType& InputBuffer::current()
{
    static CharType const end = BUFFER_END;

    if (!Current_)
        return end;

    if (*Current_ == BUFFER_END) {
        if (!refreshBuffer(CurrentBuffer_ ^ 1))
            return end;
        swapBuffers_();
    }

    return *Current_;
}

MYLANG_NODISCARD const CharType& InputBuffer::peek()
{
    static CharType const end = BUFFER_END;

    if (!Current_)
        return end;

    // Ensure current buffer is valid
    if (*Current_ == BUFFER_END) {
        if (!refreshBuffer(CurrentBuffer_ ^ 1))
            return end;
        swapBuffers_();
    }

    // Now Current_ points to valid data, check the next character
    Pointer forward = Current_ + 1;

    if (*forward == BUFFER_END) {
        // The next character would be in the other buffer
        // Refresh it and return the first character from that buffer
        if (!refreshBuffer(CurrentBuffer_ ^ 1))
            return end;

        // Return first character of the other buffer without swapping
        std::int32_t otherBuffer = CurrentBuffer_ ^ 1;
        if (Buffers_[otherBuffer].len() > 0 && Buffers_[otherBuffer][0] != BUFFER_END)
            return Buffers_[otherBuffer][0];

        return end;
    }

    return *forward;
}

StringRef InputBuffer::nPeek(SizeType n)
{
    StringRef out;

    if (n == 0)
        return out;

    // Ensure current buffer is valid
    if (Current_ && *Current_ == BUFFER_END) {
        if (!refreshBuffer(CurrentBuffer_ ^ 1))
            return out;
        swapBuffers_();
    }

    if (!Current_)
        return out;

    SizeType rem = n;
    std::int32_t buf_idx = CurrentBuffer_;
    SizeType offset = static_cast<SizeType>(Current_ - Buffers_[buf_idx].data() + 1);

    while (rem > 0) {
        // Check if we need to move to the next buffer
        if (offset >= Buffers_[buf_idx].len() || Buffers_[buf_idx][offset] == BUFFER_END) {
            // Try to refresh the other buffer
            if (!refreshBuffer(buf_idx ^ 1))
                break;

            buf_idx ^= 1;
            offset = 0;
        }

        // Append character and continue
        out += Buffers_[buf_idx][offset];
        ++offset;
        --rem;
    }

    return out;
}

void InputBuffer::unget(CharType ch)
{
    Position prev_pos = CurrentPosition_;
    rewindPosition_(ch);
    UngetStack_.push({ ch, prev_pos });
}

void InputBuffer::reset()
{
    CurrentBuffer_ = 0;

    // Clear both buffers properly
    Buffers_[0].clear();
    Buffers_[1].clear();

    Current_ = Buffers_[0].data();
    CurrentPosition_ = { 1, 1, 0 };

    // Clear column stack and initialize
    while (!Columns_.empty())
        Columns_.pop();
    Columns_.push(1);

    // Clear unget stack
    while (!UngetStack_.empty())
        UngetStack_.pop();
}

void InputBuffer::swapBuffers_()
{
    CurrentBuffer_ ^= 1;
    Current_ = Buffers_[CurrentBuffer_].data();

    // Ensure column stack is never empty
    if (Columns_.empty())
        Columns_.push(1);
}

void InputBuffer::advancePosition_(CharType ch)
{
    ++CurrentPosition_.FilePos;

    if (ch == u'\n') {
        ++CurrentPosition_.line;
        CurrentPosition_.column = 1;
        Columns_.push(1);
    } else {
        ++CurrentPosition_.column;

        if (!Columns_.empty())
            Columns_.top() = CurrentPosition_.column;
        else
            // This should never happen, but handle it gracefully
            Columns_.push(CurrentPosition_.column);
    }
}

void InputBuffer::rewindPosition_(CharType ch)
{
    // Don't rewind past the beginning of the file
    if (CurrentPosition_.FilePos == 0)
        return;

    CurrentPosition_.FilePos = std::max<SizeType>(0, CurrentPosition_.FilePos - 1);

    if (ch == u'\n') {
        // We're rewinding past a newline, so go back to the previous line
        if (!Columns_.empty())
            Columns_.pop();

        CurrentPosition_.line = std::max<SizeType>(1, CurrentPosition_.line - 1);
        CurrentPosition_.column = Columns_.empty() ? 1 : Columns_.top();

        // Ensure column stack is never empty
        if (Columns_.empty())
            Columns_.push(CurrentPosition_.column);
    } else {
        // Regular character rewind
        CurrentPosition_.column = std::max<SizeType>(0, CurrentPosition_.column - 1);

        if (!Columns_.empty())
            Columns_.top() = CurrentPosition_.column;
        else
            // This should never happen, but handle it gracefully
            Columns_.push(CurrentPosition_.column);
    }
}

} // namespace buffer
} // namespace lex
} // namespace mylang
