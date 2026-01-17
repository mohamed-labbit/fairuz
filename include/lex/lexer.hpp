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
  MYLANG_COMPILER_ABI const std::vector<tok::Token>& tokenStream() const { return TokStream_; }
  /// @brief Tokenizes the entire input source
  MYLANG_COMPILER_ABI std::vector<tok::Token> tokenize();
  /// @brief Returns the current indentation size
  MYLANG_COMPILER_ABI const std::size_t indentSize() const { return IndentSize_; }

  /**
   * @brief Constructs a token with optional metadata.
   *
   * Any omitted fields are inferred from the current source position.
   */
  MYLANG_COMPILER_ABI tok::Token make_token(tok::TokenType             tt,
                                            std::optional<StringType>  lexeme    = std::nullopt,
                                            std::optional<std::size_t> line      = std::nullopt,
                                            std::optional<std::size_t> col       = std::nullopt,
                                            std::optional<std::size_t> file_pos  = std::nullopt,
                                            std::optional<std::string> file_path = std::nullopt) const;

 private:
  SourceManager           SourceManager_;   // Manages source input and positions
  std::size_t             TokIndex_{0};     // Current token index
  unsigned                IndentSize_{0};   // Current indentation size
  unsigned                IndentLevel_{0};  // current level of indentation
  std::vector<tok::Token> TokStream_;       // Accumulated token stream
  std::vector<unsigned>   IndentStack_;     // Legacy indentation stack
  std::vector<unsigned>   AltIndentStack_;  // Alternative indentation stack for consistency
  bool                    AtBOL_{false};    // at beginning of a new line

  /// @brief Lexes a single token and stores it
  MYLANG_COMPILER_ABI tok::Token lexToken();
  /// @brief Updates indentation context based on emitted token
  MYLANG_COMPILER_ABI void updateIndentationContext_(const tok::Token& token);
  /// @brief Consumes and returns the next character from the source
  MYLANG_COMPILER_ABI char16_t consumeChar() { return SourceManager_.consumeChar(); }
  /// @brief Stores a token in the token stream
  MYLANG_COMPILER_ABI void store(tok::Token tok);

  /**
   * @brief Configures the global locale for Unicode handling.
   *
   * Attempts to use Arabic UTF-8 locale, falling back to classic locale
   * if unavailable.
   */
  static void configureLocale()
  {
    try
    {
      std::locale::global(std::locale("ar_SA.UTF-8"));
    } catch (const std::runtime_error&)
    {
      std::locale::global(std::locale::classic());
    }
  }
};


}  // namespace lex
}  // namespace mylang