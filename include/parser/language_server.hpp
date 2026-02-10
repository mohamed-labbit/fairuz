#pragma once

#include "../macros.hpp"
#include "../types/string.hpp"

#include <unordered_map>
#include <vector>

namespace mylang {
namespace parser {

class LanguageServer {
public:
    struct Position {
        std::int32_t line, character;
    };

    struct Range {
        Position start, end;
    };

    struct CompletionItem {
        StringRef label;
        StringRef detail;
        StringRef documentation;
        std::int32_t kind; // Variable, Function, Class, etc.
    };

    struct Hover {
        StringRef contents;
        Range range;
    };

    // Auto-completion at cursor position
    std::vector<CompletionItem> getCompletions(StringRef const& source, Position pos);

    // Hover information
    Hover getHover(StringRef const& source, Position pos);

    // Go to definition
    Position getDefinition(StringRef const& source, Position pos);

    // Find all references
    std::vector<Range> getReferences(StringRef const& source, Position pos);

    // Rename symbol
    std::unordered_map<StringRef, StringRef, StringRefHash, StringRefEqual>
    rename(StringRef const& source, Position pos, StringRef const& newName);
};

}
}
