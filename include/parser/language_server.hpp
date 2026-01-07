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
    string_type label;
    string_type detail;
    string_type documentation;
    std::int32_t kind;  // Variable, Function, Class, etc.
  };

  struct Hover
  {
    string_type contents;
    Range range;
  };

  // Auto-completion at cursor position
  std::vector<CompletionItem> getCompletions(const string_type& source, Position pos);
  // Hover information
  Hover getHover(const string_type& source, Position pos);
  // Go to definition
  Position getDefinition(const string_type& source, Position pos);
  // Find all references
  std::vector<Range> getReferences(const string_type& source, Position pos);
  // Rename symbol
  std::unordered_map<string_type, string_type> rename(const string_type& source, Position pos, const string_type& newName);
};

}
}