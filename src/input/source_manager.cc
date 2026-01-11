#include "../../include/input/source_manager.hpp"

#include <functional>
#include <iostream>


namespace mylang {
namespace lex {

using offset_pair = std::pair<std::size_t, std::size_t>;

char16_t SourceManager::peek()
{

  return this->InputBuffer_.peek();
  if (Current_ == nullptr) return BUFFER_END;
  char16_t* forward = Current_ + 1;
  if (forward == nullptr) return BUFFER_END;
  return *forward;
}

offset_pair SourceManager::offsetMap_(const std::size_t& offset) const
{

  if (offset == InputBuffer_.bufferOffset()) return std::make_pair(InputBuffer_.position().line, InputBuffer_.position().column);

  std::size_t iter = 0;
  std::size_t diff = 0;

  // Count lines before buffer start
  while (iter < InputBuffer_.bufferOffset())
  {
    if (InputBuffer_.at(iter) == u'\n') diff += 1;
    iter += 1;
  }

  std::size_t base_line   = InputBuffer_.position().line - diff;
  iter                    = 0;
  std::size_t       line  = 1;
  std::size_t       col   = 1;
  const std::size_t limit = std::min(offset, InputBuffer_.size() - 1);

  while (iter < limit)
  {
    char16_t c = InputBuffer_.at(iter);
    if (c == u'\n')
    {
      line += 1;
      col = 1;
    }
    else
    {
      col += 1;
    }
    iter += 1;
  }

  // combine with base line
  line = base_line + (line - 1);
  return std::make_pair(line, col);
}

offset_pair SourceManager::offsetMap(const std::size_t& offset)
{
  /// TODO: : implement this using the new file manager
  std::size_t line = 1;
  std::size_t col  = 1;
  return std::make_pair(line, col);
}

}  // lex
}  // mylang
