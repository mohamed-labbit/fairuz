#pragma once

#include "lex/source_manager.h"
#include "lex/token.h"


class Lexer
{
   public:
    using char_type   = wchar_t;
    using string_type = std::wstring;
    using size_type   = std::size_t;

    explicit Lexer(const std::string& filename) :
        source_manager_(filename),
        tok_index_(0) {
        // reserve a conservative upper bound: at most source.size() tokens (safe)
        this->tok_stream_.reserve(this->source_manager_.remaining() > 0 ? this->source_manager_.remaining() : 1);
        // setting locale at the constructor, maybe there's a better place I donno ?
        std::locale::global(std::locale("ar_SA.UTF-8"));
    }

    explicit Lexer(const Lexer&) = delete;

    Token operator()() { return next(); }

    // string_type getRaw() const { return source_manager_.getRaw(); }

    Token next();
    Token peek();
    Token prev();

    // get: avoid copies where possible
    const std::vector<Token>& tokenStream() const { return tok_stream_; }
    std::vector<Token>        tokenize();

    // DEBUGGING

    const size_type indent_size() const { return indent_size_; }

   private:
    SourceManager         source_manager_;
    size_type             tok_index_;
    std::vector<Token>    tok_stream_;
    std::vector<unsigned> indent_stack_;
    size_type             indent_size_ = 0;

    char_type consume_char() { return source_manager_.consume_char(); }

    void store(Token tok) {
        // push and update index
        tok_stream_.push_back(std::move(tok));
        tok_index_ = tok_stream_.size() - 1;
    }
};
