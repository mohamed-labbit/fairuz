#pragma once

#include "../macros.hpp"
#include "../util.hpp"
#include "file_manager.hpp"

#include <stack>
#include <stdexcept>
#include <string>

namespace mylang {
namespace input {

class SourceManager {
public:
    explicit SourceManager() = default;

    explicit SourceManager(input::FileManager* fm)
        : FileManager_(fm)
    {
        if (!FileManager_)
            throw std::runtime_error("SourceManager: FileManager is null");

        reset();
    }

    void reset();

    SizeType getLineNumber() const
    {
        return Context_.line;
    }

    SizeType getColumnNumber() const
    {
        return Context_.column;
    }

    SizeType getFileOffset() const
    {
        return Context_.offset;
    }

    std::string fpath() const MYLANG_NOEXCEPT
    {
        return FileManager_->getPath();
    }

    bool done() const
    {
        return Context_.offset >= FileManager_->buffer().len();
    }

    // returns codepoint next to offset + 1 without advancing pos
    MYLANG_NODISCARD uint32_t peekChar();

    uint32_t currentChar() const;

    void consumeChar();

    // returns codepoint next to offset + 1
    uint32_t nextChar();

    void unget(uint32_t const cp);

private:
    struct Context {
        SizeType line { 1 };
        SizeType column { 1 };
        SizeType offset { 0 }; // byte offset
    };

    struct PushbackEntry {
        uint32_t ch { BUFFER_END };
        Context ctx;          // Context AFTER reading this character
        SizeType bytes { 0 }; // Number of bytes this character occupies
    };

    input::FileManager* FileManager_ { nullptr };
    Context Context_;
    uint32_t Current_ { BUFFER_END };
    SizeType CurrentBytes_ { 0 }; // Bytes of the current character
    std::stack<PushbackEntry> UngetStack_;

    void advance(uint32_t const cp, SizeType const bytes);

    void rewindPosition_(uint32_t const cp, SizeType const bytes);

    // Calculate what column we're at for a given byte offset
    // This is needed when rewinding past newlines
    SizeType calculateColumnAtOffset(SizeType const target_offset) const;
};

}
} // namespace mylang
