#pragma once

#include "source_manager.hpp"
#include "token.hpp"

#include <locale>
#include <optional>

namespace mylang {
namespace lex {

class Lexer {
public:
    explicit Lexer() = default;

    explicit Lexer(FileManager* file_manager);

    explicit Lexer(Lexer const&) = delete;

    explicit Lexer(std::vector<tok::Token const*>& seq, size_t const s);

    tok::Token const* operator()() { return next(); }
    tok::Token const* current() const;
    tok::Token const* next();
    tok::Token const* peek(size_t n = 1);

    std::vector<tok::Token const*> tokenize();

    tok::Token* makeToken(
        tok::TokenType tt,
        std::optional<StringRef> lexeme = std::nullopt,
        std::optional<size_t> line = std::nullopt,
        std::optional<size_t> col = std::nullopt,
        std::optional<size_t> file_pos = std::nullopt,
        std::optional<std::string> file_path = std::nullopt) const;

private:
    SourceManager SourceManager_;
    size_t TokIndex_ { 0 };
    unsigned int IndentSize_ { 0 };
    unsigned int IndentLevel_ { 0 };
    std::vector<tok::Token const*> TokStream_;
    std::vector<unsigned int> IndentStack_;
    std::vector<unsigned int> AltIndentStack_;
    bool AtBOL_ { true };

    // main lexer loop
    tok::Token const* lexToken();

    void store(tok::Token const* tok);
}; // class Lexer

inline Allocator token_allocator("Token allocator");

inline tok::Token* Lexer::makeToken(
    tok::TokenType tt,
    std::optional<StringRef> lexeme,
    std::optional<size_t> line,
    std::optional<size_t> col,
    std::optional<size_t> file_pos,
    std::optional<std::string> file_path) const
{
    return token_allocator.allocateObject<tok::Token>(
        lexeme.value_or(""),
        tt,
        line.value_or(this->SourceManager_.getLineNumber()),
        col.value_or(this->SourceManager_.getColumnNumber()),
        file_pos.value_or(this->SourceManager_.getFileOffset()),
        file_path.value_or(this->SourceManager_.fpath()));
}

} // namespace lex
} // namespace mylang
