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
class Lexer {
public:
    /// @brief Constructs an empty lexer
    explicit Lexer() = default;

    /// @brief Constructs a lexer bound to a file manager
    explicit Lexer(input::FileManager* file_manager);

    /// @brief Copying a lexer is not allowed
    explicit Lexer(Lexer const&) = delete;

    /// @brief Constructs a lexer from an existing token sequence
    explicit Lexer(std::vector<tok::Token const*>& seq, SizeType const s);

    /// @brief Returns the next token (call operator convenience)
    MYLANG_COMPILER_ABI const tok::Token* operator()() { return next(); }

    /// @brief Returns the current token without advancing
    MYLANG_COMPILER_ABI const tok::Token* current() const;

    /// @brief Advances and returns the next token
    MYLANG_COMPILER_ABI const tok::Token* next();

    /// @brief Peeks ahead n tokens without advancing
    MYLANG_COMPILER_ABI const tok::Token* peek(SizeType n = 1);

    /// @brief Returns the previous token
    MYLANG_COMPILER_ABI const tok::Token* prev();

    /// @brief Returns the full token stream
    MYLANG_COMPILER_ABI const std::vector<tok::Token const*>& tokenStream() const { return TokStream_; }

    /// @brief Tokenizes the entire input source
    MYLANG_COMPILER_ABI std::vector<tok::Token const*> tokenize();

    /// @brief Returns the current indentation size
    MYLANG_COMPILER_ABI const SizeType indentSize() const { return IndentSize_; }

    /**
     * @brief Constructs a token with optional metadata.
     *
     * Any omitted fields are inferred from the current source position.
     */
    MYLANG_COMPILER_ABI tok::Token* make_token(tok::TokenType tt,
        std::optional<StringRef> lexeme = std::nullopt,
        std::optional<SizeType> line = std::nullopt,
        std::optional<SizeType> col = std::nullopt,
        std::optional<SizeType> file_pos = std::nullopt,
        std::optional<std::string> file_path = std::nullopt) const;

    MYLANG_COMPILER_ABI StringRef getSourceLine(SizeType const line) { return SourceManager_.getSourceLine(line); }

private:
    SourceManager SourceManager_;              // Manages source input and positions
    SizeType TokIndex_ { 0 };                  // Current token index
    unsigned IndentSize_ { 0 };                // Current indentation size
    unsigned IndentLevel_ { 0 };               // current level of indentation
    std::vector<tok::Token const*> TokStream_; // Accumulated token stream
    std::vector<unsigned> IndentStack_;        // Legacy indentation stack
    std::vector<unsigned> AltIndentStack_;     // Alternative indentation stack for consistency
    bool AtBOL_ { false };                     // at beginning of a new line

    /// @brief Lexes a single token and stores it
    MYLANG_COMPILER_ABI const tok::Token* lexToken();

    /// @brief Updates indentation context based on emitted token
    MYLANG_COMPILER_ABI void updateIndentationContext_(tok::Token const& token);

    /// @brief Consumes and returns the next character from the source
    MYLANG_COMPILER_ABI CharType consumeChar() { return SourceManager_.consumeChar(); }

    MYLANG_COMPILER_ABI CharType currentChar() { return SourceManager_.current(); }

    MYLANG_COMPILER_ABI CharType nextChar()
    {
        SourceManager_.consumeChar();
        return SourceManager_.current();
    }

    MYLANG_COMPILER_ABI CharType peekChar() { return SourceManager_.peek(); }

    /// @brief Stores a token in the token stream
    MYLANG_COMPILER_ABI void store(tok::Token const* tok);

    /**
     * @brief Configures the global locale for Unicode handling.
     *
     * Attempts to use Arabic UTF-8 locale, falling back to classic locale
     * if unavailable.
     */
    static void configureLocale()
    {
        try {
            std::locale::global(std::locale("ar_SA.UTF-8"));
        } catch (std::runtime_error const&) {
            std::locale::global(std::locale::classic());
        }
    }
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

inline tok::Token* Lexer::make_token(tok::TokenType tt,
    std::optional<StringRef> lexeme,
    std::optional<SizeType> line,
    std::optional<SizeType> col,
    std::optional<SizeType> file_pos,
    std::optional<std::string> file_path) const
{
    tok::Token* ret = token_allocator.make(lexeme.value_or(u""), tt, line.value_or(this->SourceManager_.line()), col.value_or(this->SourceManager_.column()),
        file_pos.value_or(this->SourceManager_.fpos()), file_path.value_or(this->SourceManager_.fpath()));
    if (!ret) // protect against bad access
        throw std::bad_alloc();
    return ret;
}

} // namespace lex
} // namespace mylang
