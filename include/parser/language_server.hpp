#pragma once

#include "../macros.hpp"

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
    StringType   label;
    StringType   detail;
    StringType   documentation;
    std::int32_t kind;  // Variable, Function, Class, etc.
  };

  struct Hover
  {
    StringType contents;
    Range      range;
  };

  // Auto-completion at cursor position
  std::vector<CompletionItem> getCompletions(const StringType& source, Position pos);

  // Hover information
  Hover getHover(const StringType& source, Position pos);

  // Go to definition
  Position getDefinition(const StringType& source, Position pos);

  // Find all references
  std::vector<Range> getReferences(const StringType& source, Position pos);

  // Rename symbol
  std::unordered_map<StringType, StringType> rename(const StringType& source, Position pos, const StringType& newName);
};

}
}