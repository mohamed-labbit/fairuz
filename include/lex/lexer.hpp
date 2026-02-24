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

    tok::Token const* operator()()
    {
        return next();
    }

    tok::Token const* current() const;

    tok::Token const* next();

    tok::Token const* peek(size_t n = 1);

    tok::Token const* prev();

    std::vector<tok::Token const*> const& tokenStream() const
    {
        return TokStream_;
    }

    std::vector<tok::Token const*> tokenize();

    size_t const indentSize() const
    {
        return IndentSize_;
    }

    tok::Token* make_token(
        tok::TokenType tt,
        std::optional<StringRef> lexeme = std::nullopt,
        std::optional<size_t> line = std::nullopt,
        std::optional<size_t> col = std::nullopt,
        std::optional<size_t> file_pos = std::nullopt,
        std::optional<std::string> file_path = std::nullopt) const;

    StringRef getSourceLine(size_t const line)
    {
        /// TODO:
        return "";
    }

private:
    SourceManager SourceManager_;
    size_t TokIndex_ { 0 };
    unsigned int IndentSize_ { 0 };
    unsigned int IndentLevel_ { 0 };
    std::vector<tok::Token const*> TokStream_;
    std::vector<unsigned int> IndentStack_;
    std::vector<unsigned int> AltIndentStack_;
    bool AtBOL_ { true };

    tok::Token const* lexToken();

    void updateIndentationContext_(tok::Token const& token);

    void store(tok::Token const* tok);
}; // class Lexer

struct TokenAllocator {
private:
    runtime::allocator::ArenaAllocator Allocator_;

public:
    TokenAllocator()
        : Allocator_(static_cast<int>(runtime::allocator::ArenaAllocator::GrowthStrategy::LINEAR))
    {
    }

    template<typename... Args>
    tok::Token* make(Args&&... args)
    {
        void* mem = Allocator_.allocate(sizeof(tok::Token));
        if (!mem)
            return nullptr;

        return new (mem) tok::Token(std::forward<Args>(args)...);
    }
};

inline TokenAllocator token_allocator;

inline tok::Token* Lexer::make_token(
    tok::TokenType tt,
    std::optional<StringRef> lexeme,
    std::optional<size_t> line,
    std::optional<size_t> col,
    std::optional<size_t> file_pos,
    std::optional<std::string> file_path) const
{
    tok::Token* ret = token_allocator.make(
        lexeme.value_or(""),
        tt,
        line.value_or(this->SourceManager_.getLineNumber()),
        col.value_or(this->SourceManager_.getColumnNumber()),
        file_pos.value_or(this->SourceManager_.getFileOffset()),
        file_path.value_or(this->SourceManager_.fpath()));

    if (!ret) // protect against bad access
        throw std::bad_alloc();

    return ret;
}

} // namespace lex
} // namespace mylang
