#include "lex/source_manager.h"
#include "macros.h"


typename SourceManager::size_type SourceManager::remaining() const {
  return this->input_buffer_.remaining();
}

typename SourceManager::size_type SourceManager::line() const {
  Position pos = input_buffer_.position();
  return pos.line;
}

typename SourceManager::size_type SourceManager::column() const {
  Position pos = input_buffer_.position();
  return pos.column;
}

// char_type current() const { return offset_ < source_.length() ? source_[offset_] : EOF; }
bool SourceManager::done() const { return this->input_buffer_.data() == nullptr; }

typename SourceManager::char_type SourceManager::peek() {
  /*
  if (done())
  return EOF;
  InputBuffer& buf = loader_.get();
  return buf.next();
  */

  return input_buffer_.peek();
}

typename SourceManager::char_type SourceManager::consume_char() {
  /*
  InputBuffer& buf = loader_.get();
  for (; offset_ < loader_.buffer_size() && len > 0; ++offset_, --len)
  {
    if (buf.current() == '\n')
    {
      ++cur_line_;
      cur_column_ = 1;
      buf.move();
      continue;
    }
    
    ++cur_column_;
  }
  */

  return input_buffer_.consume_char();
}
