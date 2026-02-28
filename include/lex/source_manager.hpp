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

    uint32_t getLineNumber() const { return Context_.line; }
    uint32_t getColumnNumber() const { return Context_.column; }
    uint64_t getFileOffset() const { return Context_.offset; }

    std::string fpath() const noexcept { return FileManager_->getPath(); }

    bool done() const { return Context_.offset >= FileManager_->buffer().len(); }

    // returns codepoint next to offset + 1 without advancing pos
    [[nodiscard]] uint32_t peekChar();

    uint32_t currentChar() const;

    void consumeChar();

    // returns codepoint next to offset + 1
    uint32_t nextChar();

    void unget(uint32_t const cp);

private:
    struct Context {
        uint32_t line { 1 };
        uint32_t column { 1 };
        uint64_t offset { 0 }; // byte offset
    };

    struct PushbackEntry {
        uint32_t ch { BUFFER_END };
        Context ctx;
        uint64_t bytes { 0 };
    };

    FileManager* FileManager_ { nullptr };
    Context Context_;
    uint32_t Current_ { BUFFER_END };
    uint64_t CurrentBytes_ { 0 };
    std::stack<PushbackEntry> UngetStack_;

    void advance(uint32_t const cp, uint64_t const bytes);

    void rewindPosition_(uint32_t const cp, uint64_t const bytes);

    uint32_t calculateColumnAtOffset(uint64_t const target_offset) const;
};

}
} // namespace mylang
