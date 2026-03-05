#pragma once

#include "../macros.hpp"
#include "../string.hpp"

#include <unordered_map>
#include <vector>

namespace mylang {
namespace parser {

class LanguageServer {
public:
    struct Position {
        int32_t line;
        int32_t character;
    };

    struct Range {
        Position start;
        Position end;
    };

    struct CompletionItem {
        StringRef label;
        StringRef detail;
        StringRef documentation;
        int32_t kind; // Variable, Function, Class ...
    };

    struct Hover {
        StringRef contents;
        Range range;
    };

    // auto-completion at cursor position
    std::vector<CompletionItem> getCompletions(StringRef const& source, Position pos);

    // hover information
    Hover getHover(StringRef const& source, Position pos);

    // go to definition
    Position getDefinition(StringRef const& source, Position pos);

    // find all references
    std::vector<Range> getReferences(StringRef const& source, Position pos);

    // rename symbol
    std::unordered_map<StringRef, StringRef, StringRefHash, StringRefEqual> rename(StringRef const& source, Position pos, StringRef const& newName);
};

}
}
