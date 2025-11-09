#pragma once

#include "../../include/lex/sym_table/table.h"
#include "source_manager.h"
#include "token.h"


namespace mylang {
namespace lex {


/*
 * @brief Represents the indentation context at a specific point in the source
 */
struct IndentationContext
{

    std::stack<std::size_t> indent_stack_;  // Stack of indentation levels
    std::size_t current_level_{0};  // Current indentation level
    bool at_line_start_{true};  // Are we at the start of a line?
    std::size_t in_parentheses_{0};  // Nesting depth of (), [], {}
    bool expecting_indent_{false};  // Did previous line end with ':'?
    std::size_t spaces_per_indent_{4};  // Expected spaces per indent level
    bool mixed_indent_error_{false};  // Detected mixed tabs/spaces

    enum class IndentMode {
        UNDETECTED,  // Haven't determined yet
        SPACES,  // Using spaces
        TABS,  // Using tabs
        MIXED  // Mixed (error state)
    };

    IndentMode mode_{IndentMode::UNDETECTED};

    IndentationContext()
    {
        indent_stack_.push(0);  // Base indentation level
    }

    /**
        * @brief Detects the indentation mode from a line
        */
    void detect_indent_mode(const std::u16string& indent_str)
    {
        if (mode_ != IndentMode::UNDETECTED)
            return;
        bool has_spaces = false;
        bool has_tabs = false;
        for (char16_t ch : indent_str)
        {
            if (ch == u' ')
                has_spaces = true;
            if (ch == u'\t')
                has_tabs = true;
        }
        if (has_spaces && has_tabs)
        {
            mode_ = IndentMode::MIXED;
            mixed_indent_error_ = true;
        }
        else if (has_spaces)
        {
            mode_ = IndentMode::SPACES;
            // Try to detect spaces per indent
            if (indent_str.length() > 0)
            {
                // Common indent widths: 2, 4, 8
                for (std::size_t width : {2, 4, 8})
                {
                    if (indent_str.length() % width == 0)
                    {
                        spaces_per_indent_ = width;
                        break;
                    }
                }
            }
        }
        else if (has_tabs)
        {
            mode_ = IndentMode::TABS;
        }
    }

    /**
        * @brief Validates indent consistency
        */
    bool validate_indent(const std::u16string& indent_str) const
    {
        if (mode_ == IndentMode::MIXED)
            return false;
        bool has_spaces = false;
        bool has_tabs = false;
        for (char16_t ch : indent_str)
        {
            if (ch == u' ')
                has_spaces = true;
            if (ch == u'\t')
                has_tabs = true;
        }
        if (mode_ == IndentMode::SPACES && has_tabs)
            return false;
        if (mode_ == IndentMode::TABS && has_spaces)
            return false;
        if (has_spaces && has_tabs)
            return false;
        return true;
    }

    std::size_t top() const { return indent_stack_.top(); }

    void push(std::size_t level) { indent_stack_.push(level); }

    std::size_t pop()
    {
        if (indent_stack_.size() > 1)
        {
            std::size_t val = indent_stack_.top();
            indent_stack_.pop();
            return val;
        }
        return 0;
    }

    std::size_t stack_size() const { return indent_stack_.size(); }
};

/**
 * @brief Result of indentation analysis
 */
struct IndentationAnalysis
{

    enum class Action {
        NONE,  // No indentation tokens needed
        INDENT,  // Emit INDENT token(s)
        DEDENT,  // Emit DEDENT token(s)
        ERROR  // Indentation error
    };

    Action action{Action::NONE};
    std::size_t count{0};  // Number of INDENT/DEDENT tokens
    std::size_t column{0};  // Column where indentation ends
    std::string error_message;  // Error description if any
    std::u16string indent_string;  // The actual indentation characters
};

class Lexer
{
   public:
    explicit Lexer(const std::string& filename) :
        source_manager_(filename),
        tok_index_(0),
        indent_size_(0)
    {
        // setting locale at the constructor, maybe there's a better place I donno ?
        std::locale::global(std::locale("ar_SA.UTF-8"));
    }

    explicit Lexer(const Lexer&) = delete;
    tok::Token operator()() { return next(); }
    tok::Token next();
    tok::Token peek(std::size_t n = 1);
    tok::Token prev();
    const std::vector<tok::Token>& tokenStream() const { return tok_stream_; }
    std::vector<tok::Token> tokenize();
    const std::size_t indent_size() const { return indent_size_; }
    tok::Token make_token(tok::TokenType tt,
      std::optional<std::u16string> lexeme = std::nullopt,
      std::optional<std::size_t> line = std::nullopt,
      std::optional<std::size_t> col = std::nullopt,
      std::optional<std::size_t> file_pos = std::nullopt,
      std::optional<std::string> file_path = std::nullopt) const;

   private:
    SourceManager source_manager_;
    std::size_t tok_index_;
    std::size_t indent_size_;
    std::vector<tok::Token> tok_stream_;
    std::stack<unsigned> indent_stack_;
    SymbolTable symbol_table_;
    IndentationContext indent_ctx_;

    void lex_token_();
    tok::Token _handle_indentation(SourceManager& sm);
    tok::Token _handle_identifier(char16_t c, SourceManager& sm);
    tok::Token _handle_number(char16_t c, SourceManager& sm);
    tok::Token _handle_operator(char16_t c, SourceManager& sm);
    tok::Token _handle_symbol(char16_t c, SourceManager& sm);
    tok::Token _handle_string_literal(char16_t c, SourceManager& sm);
    tok::Token _handle_newline(char16_t c, SourceManager& sm);
    tok::Token _emit_unknown(char16_t c, SourceManager& sm);
    tok::Token _emit_eof(SourceManager& sm);
    tok::Token _emit_sof(SourceManager& sm);
    IndentationAnalysis _analyze_indentation(SourceManager& sm);
    void update_indentation_context(const tok::Token& token);
    char16_t consume_char() { return source_manager_.consume_char(); }

    void store(tok::Token tok)
    {
        // push and update index
        tok_stream_.push_back(std::move(tok));
        tok_index_ = tok_stream_.size() - 1;
    }
};


}  // lex
}  // mylang
