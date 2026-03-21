#ifndef LEXER_HPP
#define LEXER_HPP

#include "array.hpp"
#include "token.hpp"

#include <filesystem>
#include <stack>

namespace mylang {
namespace lex {

enum class FileManagerError {
  FILE_NOT_FOUND,
  FILE_NOT_OPEN,
  SEEK_OUT_OF_BOUND,
  READ_ERROR,
  INVALID_UTF8,
  INVALID_CHAR_OFFSET,
  PERMISSION_DENIED,
  UNEXPECTED_EOF,
  SYSTEM_ERROR,
  ENCODING_ERROR,
  CACHE_ERROR,
  INVALID_LINE_NUMBER,
  BUFFER_TOO_SMALL
};

static constexpr char const *toString(FileManagerError error) noexcept {
  switch (error) {
  case FileManagerError::FILE_NOT_FOUND:
    return "File not found";
  case FileManagerError::FILE_NOT_OPEN:
    return "File is not open";
  case FileManagerError::SEEK_OUT_OF_BOUND:
    return "Seek position out of bounds";
  case FileManagerError::READ_ERROR:
    return "Failed to read from file";
  case FileManagerError::INVALID_UTF8:
    return "Invalid UTF-8 sequence encountered";
  case FileManagerError::INVALID_CHAR_OFFSET:
    return "Invalid character offset";
  case FileManagerError::PERMISSION_DENIED:
    return "Permission denied";
  case FileManagerError::UNEXPECTED_EOF:
    return "Unexpected end of file";
  case FileManagerError::SYSTEM_ERROR:
    return "System error occurred";
  case FileManagerError::ENCODING_ERROR:
    return "Encoding conversion error";
  case FileManagerError::CACHE_ERROR:
    return "Cache operation failed";
  case FileManagerError::INVALID_LINE_NUMBER:
    return "Invalid line number";
  case FileManagerError::BUFFER_TOO_SMALL:
    return "Buffer too small for operation";
  default:
    return "Unknown error";
  }
}

class FileManager {
public:
  explicit FileManager(std::string const &filepath);

  FileManager(FileManager &&) noexcept = delete;
  FileManager &operator=(FileManager &&) noexcept = delete;

  FileManager(FileManager const &) noexcept = delete;
  FileManager &operator=(FileManager &) noexcept = delete;

  ~FileManager() = default;

  StringRef load(std::string const &filepath, bool const replace = false);
  StringRef &buffer() { return InputBuffer_; }
  StringRef const &buffer() const { return InputBuffer_; }
  std::string getPath() const { return FilePath_; }

  StringRef getLineAt(uint32_t const line_idx) const {
    if (line_idx == 0 || InputBuffer_.empty())
      return {};

    char const *data = InputBuffer_.data();
    size_t const size = InputBuffer_.len();

    uint32_t current_line = 1;
    size_t line_start = 0;

    for (size_t i = 0; i <= size; ++i) {
      if (i == size || data[i] == '\n') {
        if (current_line == line_idx) {
          size_t len = i - line_start;
          if (len > 0 && data[line_start + len - 1] == '\r')
            --len;

          return StringRef(InputBuffer_.get(), line_start, len);
        }
        ++current_line;
        line_start = i + 1;
      }
    }

    return {};
  }

private:
  std::string FilePath_;
  StringRef InputBuffer_;
  std::filesystem::file_time_type LastKnownWriteTime_;
};

class SourceManager {
public:
  explicit SourceManager() = default;

  explicit SourceManager(FileManager *fm) : FileManager_(fm) {
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
  MY_NODISCARD uint32_t peekChar();
  uint32_t currentChar() const;
  void consumeChar();
  uint32_t nextChar();
  void unget(uint32_t const cp);
  StringRef getLineAt(uint32_t const line_idx) const { return FileManager_->getLineAt(line_idx); }

private:
  struct Context {
    uint32_t line{1};
    uint32_t column{1};
    uint64_t offset{0}; // byte offset
  };

  struct PushbackEntry {
    uint32_t ch{BUFFER_END};
    Context ctx;
    uint64_t bytes{0};
  };

  FileManager *FileManager_{nullptr};
  Context Context_;
  uint32_t Current_{BUFFER_END};
  uint64_t CurrentBytes_{0};
  std::stack<PushbackEntry> UngetStack_;

  void advance(uint32_t const cp, uint64_t const bytes);
  void rewindPosition_(uint32_t const cp, uint64_t const bytes);
  uint32_t calculateColumnAtOffset(uint64_t const target_offset) const;
};

class Lexer {
public:
  explicit Lexer() = default;
  explicit Lexer(FileManager *file_manager);
  explicit Lexer(Lexer const &) = delete;
  explicit Lexer(Array<tok::Token const *> &seq /*, size_t const s*/);

  tok::Token const *operator()() { return next(); }
  tok::Token const *current() const;
  tok::Token const *next();
  tok::Token const *peek(size_t n = 1);
  Array<tok::Token const *> tokenize();
  static tok::Token *makeToken(tok::TokenType tt, StringRef lexeme = "", size_t line = 0, size_t col = 0);
  StringRef getLineAt(uint32_t const line_idx) const { return SourceManager_.getLineAt(line_idx); }

private:
  SourceManager SourceManager_;
  size_t TokIndex_{0};
  unsigned int IndentSize_{0};
  unsigned int IndentLevel_{0};
  Array<tok::Token const *> TokStream_;
  Array<unsigned int> IndentStack_;
  Array<unsigned int> AltIndentStack_;
  bool AtBOL_{true};

  // main lexer loop
  tok::Token const *lexToken();

  void store(tok::Token const *tok);
}; // class Lexer

inline tok::Token *Lexer::makeToken(tok::TokenType tt, StringRef lexeme, size_t line, size_t col) {
  return getAllocator().allocateObject<tok::Token>(lexeme, tt, line, col);
}

} // namespace lex
} // namespace mylang

#endif // LEXER_HPP
