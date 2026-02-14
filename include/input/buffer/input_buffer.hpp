/*
#pragma once

#include "../file_manager.hpp"
#include "base.hpp"
#include <stack>

namespace mylang {
    namespace lex {
        namespace buffer {

        struct Position {
            SizeType line { 0 };
            SizeType column { 0 };
            SizeType FilePos { 0 };

            Position() = default;

            Position(SizeType const line, SizeType const col, SizeType fpos)
            : line(line)
            , column(col)
            , FilePos(fpos)
            {
            }
        };

        class InputBuffer : public InputBufferBase {
            public:
            InputBuffer() = default;

            InputBuffer(input::FileManager* file_manager, SizeType cap = DEFAULT_CAPACITY)
            : Capacity_(cap)
            , InputBufferBase(file_manager, cap)
            {
                reset();
            }

            SizeType size() const
            {
                return Buffers_[CurrentBuffer_].len();
            }

            bool empty() const
            {
                return (!Current_ || Buffers_[CurrentBuffer_].empty());
            }

            char at(SizeType const idx) const
            {
                return Buffers_[CurrentBuffer_][idx];
            }

            uint32_t consumeChar(); // returns a codepoint not a byte char

            MYLANG_NODISCARD
            char const& current();

            MYLANG_NODISCARD
            char const& peek();

            StringRef nPeek(SizeType n);

            void consume(SizeType len)
            {
                while (len-- > 0)
                consumeChar();
            }

            void unget(char ch);

            void reset();

            Position position() const MYLANG_NOEXCEPT
            {
                return CurrentPosition_;
            }

            StringRef getSourceLine(SizeType const line)
            {
                return FileManager_->getSourceLine(line);
            }

            private:
            struct PushbackEntry {
                uint32_t ch;
                Position pos;
            };

            SizeType Capacity_ = DEFAULT_CAPACITY;
            uint8_t CurrentBuffer_ { 0 };
            SizeType FilePos_ { 0 };
            Position CurrentPosition_;
            std::stack<SizeType> Columns_;
            std::stack<PushbackEntry> UngetStack_;

            void swapBuffers_();

            void advancePosition_(char ch);
            void rewindPosition_(char ch);
        };

    }
} // lex
} // mylang

*/
