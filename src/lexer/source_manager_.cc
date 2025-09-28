#include "lex/source_manager.h"
#include "macros.h"


typename SourceManager::size_type SourceManager::remaining() const {
  return loader_.buffer_size() - offset_;
}

typename SourceManager::size_type SourceManager::line() const { return cur_line_; }
typename SourceManager::size_type SourceManager::column() const { return cur_column_; }
// char_type current() const { return offset_ < source_.length() ? source_[offset_] : EOF; }
bool SourceManager::done() const { return offset_ >= loader_.buffer_size(); }

// next consumes the character while peek doesn't
typename SourceManager::char_type SourceManager::next() {
  if (done())
    return EOF;
  InputBuffer& buf = loader_.get();
  char_type    ret = buf.next();
  move();
  return ret;
}

typename SourceManager::char_type SourceManager::peek() {
  if (done())
    return EOF;
  InputBuffer& buf = loader_.get();
  return buf.next();
}

void SourceManager::move(unsigned len) {
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
}
