#include "../../include/input/source_manager.hpp"

#include <functional>
#include <iostream>


namespace mylang {
namespace lex {

using offset_pair = std::pair<std::size_t, std::size_t>;

SourceManager::SourceManager(input::FileManager* file_manager) :
    input_buffer_(file_manager, DEFAULT_CAPACITY)
{
  // ...
}

std::size_t SourceManager::line() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::line() called!" << std::endl;
#endif
  return this->input_buffer_.position().line;
}

std::size_t SourceManager::column() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::column() called!" << std::endl;
#endif
  return this->input_buffer_.position().column;
}

std::size_t SourceManager::fpos() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::fpos() called!" << std::endl;
#endif
  return this->input_buffer_.position().filepos;
}

const std::string SourceManager::fpath() const noexcept
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::fpath() called!" << std::endl;
#endif
  return this->filepath_;
}

buffer::Position SourceManager::position() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::position() called!" << std::endl;
#endif
  return this->input_buffer_.position();
}

bool SourceManager::done() const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::done() called!" << std::endl;
#endif
  return this->input_buffer_.empty();
}

char16_t SourceManager::peek()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::peek() called!" << std::endl;
#endif
  return this->input_buffer_.peek();
  if (!current_) return BUFFER_END;
  auto* forward = current_ + 1;
  if (!forward) return BUFFER_END;
  return *forward;
}

char16_t SourceManager::consume_char()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::consume_char() called!" << std::endl;
#endif
  return this->input_buffer_.consume_char();
}

char16_t SourceManager::current()
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::current() called!" << std::endl;
#endif
  return input_buffer_.current();
}

offset_pair SourceManager::offset_map_(const std::size_t& offset) const
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::offset_map_() called!" << std::endl;
#endif
  if (offset == input_buffer_.buffer_offset()) return std::make_pair(input_buffer_.position().line, input_buffer_.position().column);

  std::size_t iter = 0;
  std::size_t diff = 0;

  // Count lines before buffer start
  while (iter < input_buffer_.buffer_offset())
  {
    if (input_buffer_.at(iter) == u'\n') diff += 1;
    iter += 1;
  }

  std::size_t base_line = input_buffer_.position().line - diff;
  iter = 0;
  std::size_t line = 1;
  std::size_t col = 1;
  const std::size_t limit = std::min(offset, input_buffer_.size() - 1);

  while (iter < limit)
  {
    char16_t c = input_buffer_.at(iter);
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

offset_pair SourceManager::offset_map(const std::size_t& offset)
{
#if DEBUG_PRINT
  std::cout << "-- DEBUG : SourceManager::offset_map() called!" << std::endl;
#endif
  /// @todo : implement this using the new file manager
  std::size_t line = 1;
  std::size_t col = 1;
  return std::make_pair(line, col);
}

}  // lex
}  // mylang
