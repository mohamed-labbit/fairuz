#pragma once

// Lexer interface and indentation handling for mylang.
//
// This header defines:
//  - IndentationContext: tracks indentation state for Python-like syntax
//  - IndentationAnalysis: result of analyzing indentation on a new line
//  - Lexer: the main lexical analyzer producing tokens from source input

#include "../../include/lex/sym_table/table.hpp"
#include "../input/source_manager.hpp"
#include "token.hpp"

#include <locale>
#include <optional>


namespace mylang {
namespace lex {


/** 
 * @brief Represents the indentation context at a specific point in the source.
 *
 * This structure maintains all state required to implement indentation-sensitive
 * lexing (similar to Python). It tracks indentation levels, detects indentation
 * style (spaces vs tabs), and ensures consistency across lines.
 */
struct IndentationContext
{
  std::stack<std::size_t> indent_stack;  // Stack of indentation levels (in columns)
  std::size_t current_level{0};          // Current indentation level
  bool at_line_start{true};              // True if lexer is at beginning of a line
  std::size_t in_parentheses{0};         // Nesting depth of (), [], {}
  bool expecting_indent{false};          // True if previous line ended with ':'
  std::size_t spaces_per_indent{4};      // Expected number of spaces per indent
  bool mixed_indent_error{false};        // True if mixed tabs/spaces detected

  /**
   * @brief Indentation style currently in use.
   *
   * UNDETECTED: indentation style not yet inferred
   * SPACES:     indentation uses spaces
   * TABS:       indentation uses tabs
   * MIXED:      both tabs and spaces used (error state)
   */
  enum class IndentMode {
    UNDETECTED,  // Haven't determined yet
    SPACES,      // Using spaces
    TABS,        // Using tabs
    MIXED        // Mixed (error state)
  };

  IndentMode mode{IndentMode::UNDETECTED};

  /**
   * @brief Constructs an indentation context with a base level.
   *
   * The base indentation level (0) is always present on the stack.
   */
  IndentationContext()
  {
    indent_stack.push(0);  // Base indentation level
  }

  /**
   * @brief Detects the indentation mode from a line's leading whitespace.
   *
   * This function is only effective while the mode is UNDETECTED.
   * It examines the indentation string and sets the mode accordingly.
   *
   * If both spaces and tabs are found, the mode becomes MIXED and
   * mixed_indent_error is set.
   */
  void detect_indent_mode(const string_type& indent_str)
  {
    if (mode != IndentMode::UNDETECTED) return;

    bool has_spaces = false;
    bool has_tabs = false;

    for (char16_t ch : indent_str)
    {
      if (ch == u' ') has_spaces = true;
      if (ch == u'\t') has_tabs = true;
    }

    if (has_spaces && has_tabs)
    {
      mode = IndentMode::MIXED;
      mixed_indent_error = true;
    }
    else if (has_spaces)
    {
      mode = IndentMode::SPACES;
      // Attempt to infer spaces-per-indent using common widths
      if (indent_str.length() > 0)
        for (std::size_t width : {2, 4, 8})
          if (indent_str.length() % width == 0)
          {
            spaces_per_indent = width;
            break;
          }
    }
    else if (has_tabs) { mode = IndentMode::TABS; }
  }

  /**
   * @brief Validates that indentation is consistent with the detected mode.
   *
   * @return true if indentation is valid, false otherwise
   */
  bool validate_indent(const string_type& indent_str) const
  {
    if (mode == IndentMode::MIXED) return false;

    bool has_spaces = false;
    bool has_tabs = false;

    for (char16_t ch : indent_str)
    {
      if (ch == u' ') has_spaces = true;
      if (ch == u'\t') has_tabs = true;
    }

    if (mode == IndentMode::SPACES && has_tabs) return false;
    if (mode == IndentMode::TABS && has_spaces) return false;
    if (has_spaces && has_tabs) return false;

    return true;
  }

  /// @brief Returns the current indentation level (top of the stack)
  std::size_t top() const { return indent_stack.top(); }

  /// @brief Pushes a new indentation level onto the stack
  void push(std::size_t level) { indent_stack.push(level); }

  /**
   * @brief Pops the current indentation level.
   *
   * The base level is never removed.
   *
   * @return the popped indentation level, or 0 if base level
   */
  std::size_t pop()
  {
    if (indent_stack.size() > 1)
    {
      std::size_t val = indent_stack.top();
      indent_stack.pop();
      return val;
    }
    return 0;
  }

  /// @brief Returns the number of indentation levels currently tracked
  std::size_t stack_size() const { return indent_stack.size(); }
};

/**
 * @brief Result of indentation analysis for a line.
 *
 * This structure describes what indentation-related action the lexer
 * should take when encountering a new line.
 */
struct IndentationAnalysis
{
  /*
   * @brief Action to perform based on indentation comparison.
   */
  enum class Action {
    NONE,    // No indentation tokens needed
    INDENT,  // Emit one or more INDENT tokens
    DEDENT,  // Emit one or more DEDENT tokens
    ERROR    // Indentation error detected
  };

  Action action{Action::NONE};  // Required action
  std::size_t count{0};         // Number of INDENT/DEDENT tokens
  std::size_t column{0};        // Column where indentation ends
  string_type error_message;    // Error message (if any)
  string_type indent_string;    // Raw indentation characters
};

/**
 * @brief Lexical analyzer for mylang.
 *
 * The Lexer consumes characters from a SourceManager and produces a stream
 * of tokens. It supports indentation-sensitive syntax, Unicode input,
 * and lookahead operations.
 */
class Lexer
{
 public:
  /// @brief Constructs an empty lexer
  explicit Lexer() = default;
  /// @brief Constructs a lexer bound to a file manager
  explicit Lexer(input::FileManager* file_manager);
  /// @brief Copying a lexer is not allowed
  explicit Lexer(const Lexer&) = delete;
  /// @brief Constructs a lexer from an existing token sequence
  explicit Lexer(std::vector<tok::Token>& seq, const std::size_t s);

  /// @brief Returns the next token (call operator convenience)
  MYLANG_COMPILER_ABI tok::Token operator()() { return next(); }
  /// @brief Returns the current token without advancing
  MYLANG_COMPILER_ABI tok::Token current() const;
  /// @brief Advances and returns the next token
  MYLANG_COMPILER_ABI tok::Token next();
  /// @brief Peeks ahead n tokens without advancing
  MYLANG_COMPILER_ABI tok::Token peek(std::size_t n = 1);
  /// @brief Returns the previous token
  MYLANG_COMPILER_ABI tok::Token prev();
  /// @brief Returns the full token stream
  MYLANG_COMPILER_ABI const std::vector<tok::Token>& tokenStream() const { return tok_stream_; }
  /// @brief Tokenizes the entire input source
  MYLANG_COMPILER_ABI std::vector<tok::Token> tokenize();
  /// @brief Returns the current indentation size
  MYLANG_COMPILER_ABI const std::size_t indent_size() const { return indent_size_; }

  /**
   * @brief Constructs a token with optional metadata.
   *
   * Any omitted fields are inferred from the current source position.
   */
  MYLANG_COMPILER_ABI tok::Token make_token(tok::TokenType tt,
                                            std::optional<string_type> lexeme = std::nullopt,
                                            std::optional<std::size_t> line = std::nullopt,
                                            std::optional<std::size_t> col = std::nullopt,
                                            std::optional<std::size_t> file_pos = std::nullopt,
                                            std::optional<std::string> file_path = std::nullopt) const;

 private:
  SourceManager source_manager_;        // Manages source input and positions
  std::size_t tok_index_;               // Current token index
  std::size_t indent_size_;             // Current indentation size
  std::vector<tok::Token> tok_stream_;  // Accumulated token stream
  std::stack<unsigned> indent_stack_;   // Legacy indentation stack
  IndentationContext indent_ctx_;       // Indentation tracking context

  /// @brief Lexes a single token and stores it
  MYLANG_COMPILER_ABI void lex_token_();
  /// @brief Handles indentation at the start of a line
  MYLANG_COMPILER_ABI tok::Token _handle_indentation(SourceManager& sm);
  /// @brief Lexes identifiers and keywords
  MYLANG_COMPILER_ABI tok::Token _handle_identifier(char16_t c, SourceManager& sm);
  /// @brief Lexes numeric literals
  MYLANG_COMPILER_ABI tok::Token _handle_number(char16_t c, SourceManager& sm);
  /// @brief Lexes operators
  MYLANG_COMPILER_ABI tok::Token _handle_operator(char16_t c, SourceManager& sm);
  /// @brief Lexes punctuation and symbols
  MYLANG_COMPILER_ABI tok::Token _handle_symbol(char16_t c, SourceManager& sm);
  /// @brief Lexes string literals
  MYLANG_COMPILER_ABI tok::Token _handle_string_literal(char16_t c, SourceManager& sm);
  /// @brief Handles newline characters
  MYLANG_COMPILER_ABI tok::Token _handle_newline(char16_t c, SourceManager& sm);
  /// @brief Emits an invalid token
  MYLANG_COMPILER_ABI tok::Token _emit_invalid(char16_t c, SourceManager& sm);
  /// @brief Emits end-of-file token
  MYLANG_COMPILER_ABI tok::Token _emit_eof(SourceManager& sm);
  /// @brief Emits start-of-file token
  MYLANG_COMPILER_ABI tok::Token _emit_sof(SourceManager& sm);
  /// @brief Analyzes indentation and determines required action
  MYLANG_COMPILER_ABI IndentationAnalysis _analyze_indentation(SourceManager& sm);
  /// @brief Updates indentation context based on emitted token
  MYLANG_COMPILER_ABI void update_indentation_context(const tok::Token& token);
  /// @brief Consumes and returns the next character from the source
  MYLANG_COMPILER_ABI char16_t consume_char() { return source_manager_.consume_char(); }
  /// @brief Stores a token in the token stream
  MYLANG_COMPILER_ABI void store(tok::Token tok);

  /**
   * @brief Configures the global locale for Unicode handling.
   *
   * Attempts to use Arabic UTF-8 locale, falling back to classic locale
   * if unavailable.
   */
  static void configure_locale()
  {
    try
    {
      std::locale::global(std::locale("ar_SA.UTF-8"));
    } catch (const std::runtime_error&) { std::locale::global(std::locale::classic()); }
  }
};


}  // namespace lex
}  // namespace mylang