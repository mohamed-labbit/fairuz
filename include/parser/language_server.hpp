#pragma once

#include "../macros.hpp"
#include "../types/string.hpp"

#include <unordered_map>
#include <vector>


namespace mylang {
namespace parser {

class LanguageServer
{
 public:
  struct Position
  {
    std::int32_t line, character;
  };

  struct Range
  {
    Position start, end;
  };

  struct CompletionItem
  {
    StringRef    label;
    StringRef    detail;
    StringRef    documentation;
    std::int32_t kind;  // Variable, Function, Class, etc.
  };

  struct Hover
  {
    StringRef contents;
    Range     range;
  };

  // Auto-completion at cursor position
  std::vector<CompletionItem> getCompletions(const StringRef& source, Position pos);

  // Hover information
  Hover getHover(const StringRef& source, Position pos);

  // Go to definition
  Position getDefinition(const StringRef& source, Position pos);

  // Find all references
  std::vector<Range> getReferences(const StringRef& source, Position pos);

  // Rename symbol
  std::unordered_map<StringRef, StringRef, StringRefHash, StringRefEqual> rename(const StringRef& source, Position pos, const StringRef& newName);
};

}
}