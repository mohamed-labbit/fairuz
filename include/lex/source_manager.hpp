#pragma once

#include "../macros.hpp"
#include "../util.hpp"
#include "file_manager.hpp"

#include <stack>
#include <stdexcept>
#include <string>

namespace mylang {
namespace lex {

class SourceManager {
public:
    explicit SourceManager() = default;

    explicit SourceManager(FileManager* fm)
        : FileManager_(fm)
    {
        if (!FileManager_)
            throw std::runtime_error("SourceManager: FileManager is null");

        reset();
    }

    void reset();

    size_t getLineNumber() const
    {
        return Context_.line;
    }

    size_t getColumnNumber() const
    {
        return Context_.column;
    }

    size_t getFileOffset() const
    {
        return Context_.offset;
    }

    std::string fpath() const noexcept
    {
        return FileManager_->getPath();
    }

    bool done() const
    {
        return Context_.offset >= FileManager_->buffer().len();
    }

    // returns codepoint next to offset + 1 without advancing pos
    [[nodiscard]] uint32_t peekChar();

    uint32_t currentChar() const;

    void consumeChar();

    // returns codepoint next to offset + 1
    uint32_t nextChar();

    void unget(uint32_t const cp);

private:
    struct Context {
        size_t line { 1 };
        size_t column { 1 };
        size_t offset { 0 }; // byte offset
    };

    struct PushbackEntry {
        uint32_t ch { BUFFER_END };
        Context ctx;
        size_t bytes { 0 };
    };

    FileManager* FileManager_ { nullptr };
    Context Context_;
    uint32_t Current_ { BUFFER_END };
    size_t CurrentBytes_ { 0 };
    std::stack<PushbackEntry> UngetStack_;

    void advance(uint32_t const cp, size_t const bytes);

    void rewindPosition_(uint32_t const cp, size_t const bytes);

    size_t calculateColumnAtOffset(size_t const target_offset) const;
};

}
} // namespace mylang
