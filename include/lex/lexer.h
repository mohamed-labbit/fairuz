#pragma once

#include "lex/source_manager.h"
#include "lex/token.h"


class Lexer
{
   public:
    using char_type   = char16_t;
    using string_type = std::u16string;
    using size_type   = std::size_t;

    explicit Lexer(const std::string& filename) :
        source_manager_(filename),
        tok_index_(0),
        indent_size_(0) {
        // setting locale at the constructor, maybe there's a better place I donno ?
        std::locale::global(std::locale("ar_SA.UTF-8"));
    }

    explicit Lexer(const Lexer&) = delete;

    Token operator()() { return next(); }

    Token next();
    Token peek();
    Token prev();

    const std::vector<Token>& tokenStream() const { return tok_stream_; }
    std::vector<Token>        tokenize();

    const size_type indent_size() const { return indent_size_; }

   private:
    SourceManager source_manager_;

    size_type tok_index_;
    size_type indent_size_;

    std::vector<Token>   tok_stream_;
    std::stack<unsigned> indent_stack_;

    void  handle_indentation_(size_type line, size_type col);
    Token handle_identifier_(char_type c, size_type line, size_type col);
    Token handle_number_(char_type c, size_type line, size_type col);
    Token handle_operator_(char_type c, size_type line, size_type col);
    Token handle_symbol_(char_type c, size_type line, size_type col);
    Token handle_string_literal_(char_type c, size_type line, size_type col);
    Token handle_newline_(char_type c, size_type line, size_type col);
    Token emit_unknown_(char_type c, size_type line, size_type col);
    Token emit_eof_();
    Token emit_sof_();

    char_type consume_char() { return source_manager_.consume_char(); }

    void store(Token tok) {
        // push and update index
        tok_stream_.push_back(std::move(tok));
        tok_index_ = tok_stream_.size() - 1;
    }
};
