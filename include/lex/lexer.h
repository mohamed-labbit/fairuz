#pragma once

#include "lex/source_manager.h"
#include "lex/sym_table/table.h"
#include "lex/token.h"


namespace mylang {
namespace lex {

class Lexer
{
   public:
    using char_type   = char16_t;
    using string_type = std::u16string;
    using size_type   = std::size_t;

    explicit Lexer(const std::string& filename) :
        source_manager_(filename),
        tok_index_(0),
        indent_size_(0)
    {
        // setting locale at the constructor, maybe there's a better place I donno ?
        std::locale::global(std::locale("ar_SA.UTF-8"));
    }

    explicit Lexer(const Lexer&) = delete;
    
    mylang::lex::tok::Token operator()() { return next(); }

    mylang::lex::tok::Token next();
    mylang::lex::tok::Token peek();
    mylang::lex::tok::Token prev();

    const std::vector<mylang::lex::tok::Token>& tokenStream() const { return tok_stream_; }
    std::vector<mylang::lex::tok::Token>        tokenize();

    const size_type indent_size() const { return indent_size_; }

    mylang::lex::tok::Token make_token(mylang::lex::tok::TokenType tt,
                                       std::optional<string_type>  lexeme    = std::nullopt,
                                       std::optional<size_type>    line      = std::nullopt,
                                       std::optional<size_type>    col       = std::nullopt,
                                       std::optional<size_type>    file_pos  = std::nullopt,
                                       std::optional<std::string>  file_path = std::nullopt) const;

   private:
    SourceManager source_manager_;

    size_type tok_index_;
    size_type indent_size_;

    std::vector<mylang::lex::tok::Token> tok_stream_;
    std::stack<unsigned>                 indent_stack_;

    mylang::lex::SymbolTable symbol_table_;

    void                    handle_indentation_(size_type line, size_type col);
    mylang::lex::tok::Token handle_identifier_(char_type c, size_type line, size_type col);
    mylang::lex::tok::Token handle_number_(char_type c, size_type line, size_type col);
    mylang::lex::tok::Token handle_operator_(char_type c, size_type line, size_type col);
    mylang::lex::tok::Token handle_symbol_(char_type c, size_type line, size_type col);
    mylang::lex::tok::Token handle_string_literal_(char_type c, size_type line, size_type col);
    mylang::lex::tok::Token handle_newline_(char_type c, size_t line, size_t col);
    mylang::lex::tok::Token emit_unknown_(char_type c, size_type line, size_type col);
    mylang::lex::tok::Token emit_eof_();
    mylang::lex::tok::Token emit_sof_();

    char_type consume_char() { return source_manager_.consume_char(); }

    void store(mylang::lex::tok::Token tok)
    {
        // push and update index
        tok_stream_.push_back(std::move(tok));
        tok_index_ = tok_stream_.size() - 1;
    }
};


}  // lexer
}  // mylang
