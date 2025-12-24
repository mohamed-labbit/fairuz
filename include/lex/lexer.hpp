#pragma once

#include "../../include/lex/sym_table/table.hpp"
#include "../input/source_manager.hpp"
#include "token.hpp"


#include <locale>
#include <optional>


namespace mylang {
namespace lex {


/*
 * @brief Represents the indentation context at a specific point in the source
 */
struct IndentationContext
{

  std::stack<std::size_t> indent_stack;               // Stack of indentation levels
  std::size_t             current_level{0};           // Current indentation level
  bool                    at_line_start{true};        // Are we at the start of a line?
  std::size_t             in_parentheses{0};          // Nesting depth of (), [], {}
  bool                    expecting_indent{false};    // Did previous line end with ':'?
  std::size_t             spaces_per_indent{4};       // Expected spaces per indent level
  bool                    mixed_indent_error{false};  // Detected mixed tabs/spaces

  enum class IndentMode {
    UNDETECTED,  // Haven't determined yet
    SPACES,      // Using spaces
    TABS,        // Using tabs
    MIXED        // Mixed (error state)
  };

  IndentMode mode{IndentMode::UNDETECTED};

  IndentationContext()
  {
    indent_stack.push(0);  // Base indentation level
  }

  /**
        * @brief Detects the indentation mode from a line
        */
  void detect_indent_mode(const string_type& indent_str)
  {
    if (mode != IndentMode::UNDETECTED) return;

    bool has_spaces = false;
    bool has_tabs   = false;

    for (char16_t ch : indent_str)
    {
      if (ch == u' ') has_spaces = true;
      if (ch == u'\t') has_tabs = true;
    }

    if (has_spaces && has_tabs)
    {
      mode               = IndentMode::MIXED;
      mixed_indent_error = true;
    }
    else if (has_spaces)
    {
      mode = IndentMode::SPACES;
      // Try to detect spaces per indent
      if (indent_str.length() > 0)
        // Common indent widths: 2, 4, 8
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
        * @brief Validates indent consistency
        */
  bool validate_indent(const string_type& indent_str) const
  {
    if (mode == IndentMode::MIXED) return false;
    bool has_spaces = false;
    bool has_tabs   = false;
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

  std::size_t top() const { return indent_stack.top(); }

  void push(std::size_t level) { indent_stack.push(level); }

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

  std::size_t stack_size() const { return indent_stack.size(); }
};

/**
 * @brief Result of indentation analysis
 */
struct IndentationAnalysis
{

  enum class Action {
    NONE,    // No indentation tokens needed
    INDENT,  // Emit INDENT token(s)
    DEDENT,  // Emit DEDENT token(s)
    ERROR    // Indentation error
  };

  Action      action{Action::NONE};
  std::size_t count{0};       // Number of INDENT/DEDENT tokens
  std::size_t column{0};      // Column where indentation ends
  string_type error_message;  // Error description if any
  string_type indent_string;  // The actual indentation characters
};

class Lexer
{
 public:
  explicit Lexer() = default;

  explicit Lexer(input::FileManager* file_manager);
  explicit Lexer(const Lexer&) = delete;

  MYLANG_COMPILER_ABI tok::Token operator()() { return next(); }
  MYLANG_COMPILER_ABI tok::Token current() const;
  MYLANG_COMPILER_ABI tok::Token next();
  MYLANG_COMPILER_ABI tok::Token peek(std::size_t n = 1);
  MYLANG_COMPILER_ABI tok::Token prev();
  MYLANG_COMPILER_ABI const std::vector<tok::Token>& tokenStream() const { return tok_stream_; }
  MYLANG_COMPILER_ABI std::vector<tok::Token> tokenize();
  MYLANG_COMPILER_ABI const std::size_t indent_size() const { return indent_size_; }
  MYLANG_COMPILER_ABI tok::Token make_token(tok::TokenType             tt,
                                            std::optional<string_type> lexeme    = std::nullopt,
                                            std::optional<std::size_t> line      = std::nullopt,
                                            std::optional<std::size_t> col       = std::nullopt,
                                            std::optional<std::size_t> file_pos  = std::nullopt,
                                            std::optional<std::string> file_path = std::nullopt) const;

 private:
  SourceManager           source_manager_;
  std::size_t             tok_index_;
  std::size_t             indent_size_;
  std::vector<tok::Token> tok_stream_;
  std::stack<unsigned>    indent_stack_;
  IndentationContext      indent_ctx_;

  MYLANG_COMPILER_ABI void lex_token_();
  MYLANG_COMPILER_ABI tok::Token _handle_indentation(SourceManager& sm);
  MYLANG_COMPILER_ABI tok::Token _handle_identifier(char16_t c, SourceManager& sm);
  MYLANG_COMPILER_ABI tok::Token _handle_number(char16_t c, SourceManager& sm);
  MYLANG_COMPILER_ABI tok::Token _handle_operator(char16_t c, SourceManager& sm);
  MYLANG_COMPILER_ABI tok::Token _handle_symbol(char16_t c, SourceManager& sm);
  MYLANG_COMPILER_ABI tok::Token _handle_string_literal(char16_t c, SourceManager& sm);
  MYLANG_COMPILER_ABI tok::Token _handle_newline(char16_t c, SourceManager& sm);
  MYLANG_COMPILER_ABI tok::Token _emit_invalid(char16_t c, SourceManager& sm);
  MYLANG_COMPILER_ABI tok::Token _emit_eof(SourceManager& sm);
  MYLANG_COMPILER_ABI tok::Token          _emit_sof(SourceManager& sm);
  MYLANG_COMPILER_ABI IndentationAnalysis _analyze_indentation(SourceManager& sm);
  MYLANG_COMPILER_ABI void                update_indentation_context(const tok::Token& token);
  MYLANG_COMPILER_ABI char16_t            consume_char() { return source_manager_.consume_char(); }
  MYLANG_COMPILER_ABI void                store(tok::Token tok);

  static void configure_locale()
  {
    try
    {
      std::locale::global(std::locale("ar_SA.UTF-8"));
    } catch (const std::runtime_error&) { std::locale::global(std::locale::classic()); }
  }
};


}  // lex
}  // mylang
