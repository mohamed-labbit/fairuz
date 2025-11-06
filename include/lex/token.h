#pragma once

#include "../../utfcpp/source/utf8.h"
#include "util.h"

#include <iostream>
#include <locale>
#include <string>
#include <unordered_map>
#include <vector>


namespace mylang {
namespace lex {
namespace tok {

enum class TokenType {
    // Special
    START_OF_FILE,
    END_OF_FILE,

    // Identifiers & literals
    IDENTIFIER,
    NUMBER,
    FLOAT,
    STRING,

    // Keywords
    KW_IF,
    KW_WHILE,
    KW_FN,
    KW_CONST,
    KW_FOR,
    KW_RETURN,
    KW_CONTINUE,
    KW_TRUE,
    KW_FALSE,
    KW_NONE,

    // Operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    AND,
    OR,
    NOT,
    EQ,
    NEQ,
    LT,
    LE,
    GT,
    GE,
    ASSIGN,

    // Symbols / punctuation
    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    COMMA,
    DOT,
    COLON,

    // Layout
    NEWLINE,  // only if significant
    INDENT,
    DEDENT,

    // Error handling
    UNKNOWN
};

static const std::unordered_map<std::u16string, TokenType, util::U16StringHash, util::U16StringEqual> operators = {{u"=", TokenType::EQ},
  {u":=", TokenType::ASSIGN}, {u"+", TokenType::PLUS}, {u"-", TokenType::MINUS}, {u"*", TokenType::STAR}, {u"/", TokenType::SLASH},
  {u"<", TokenType::LT}, {u">", TokenType::GT}, {u"<=", TokenType::LE}, {u">=", TokenType::GE}, {u"!=", TokenType::NEQ}};

static const std::unordered_map<std::u16string, TokenType, util::U16StringHash, util::U16StringEqual> keywords = {
  {u"خطا", TokenType::KW_FALSE}, {u"عدم", TokenType::KW_NONE}, {u"صحيح", TokenType::KW_TRUE}, {u"و", TokenType::AND},
  {u"اخرج", TokenType::KW_RETURN}, {u"اكمل", TokenType::KW_CONTINUE}, {u"عرف", TokenType::KW_FN}, {u"او", TokenType::OR},
  {u"بكل", TokenType::KW_FOR}, {u"اذا", TokenType::KW_IF}, {u"ليس", TokenType::NOT}, {u"ارجع", TokenType::KW_RETURN},
  {u"طالما", TokenType::KW_WHILE}, {u"ثابت", TokenType::KW_CONST}};

class Token
{
   public:

    struct Location
    {
        std::string filepath_;
        std::size_t line_{0};
        std::size_t column_{0};
        std::size_t file_pos_{0};

        Location() = default;

        Location(std::string fp, std::array<std::size_t, 3> coords) :
            filepath_(fp),
            line_(coords[0]),
            column_(coords[1]),
            file_pos_(coords[2])
        {
        }

        Location(std::string fp, std::size_t coords[3]) :
            filepath_(fp),
            line_(coords[0]),
            column_(coords[1]),
            file_pos_(coords[2])
        {
        }
    };

    // Main ctor: take value by value so callers can move temporaries in.
    Token(std::u16string v, TokenType t, std::array<std::size_t, 3> coords, std::string fp) :
        value_(std::move(v)),
        type_(t),
        location_(fp, coords)
    {
    }

    Token(std::u16string v, TokenType t, std::size_t coords[3], std::string fp) :
        value_(std::move(v)),
        type_(t),
        location_(fp, coords)
    {
    }

    Token() = default;
    Token(const Token&) = default;
    Token(Token&&) noexcept = default;

    Token& operator=(const Token&) = default;
    Token& operator=(Token&&) noexcept = default;

    // Return const references to avoid copies
    const std::u16string& lexeme() const;

    const TokenType& type() const;

    std::size_t size() const;

    const std::size_t& line() const;

    const std::size_t& column() const;

    const Location& location() const;

    const std::string& filepath() const;

    bool operator==(const Token& other) const;
    bool operator!=(const Token& other) const;

    // friend ostream operator for pretty-printing in tests/logs
    friend std::ostream& operator<<(std::ostream& os, const Token& tok)
    {
        os << "Token(\"" << utf8::utf16to8(tok.value_) << "\", type=" << static_cast<int>(tok.type_) << ", line=" << tok.location_.line_
           << ", col=" << tok.location_.column_ << "\", file_pos=" << tok.location_.file_pos_ << "\", file path=" << tok.location_.filepath_
           << ")";
        return os;
    }

   private:
    std::u16string value_;
    TokenType type_;
    Location location_;
};
}
}  // lex
}  // mylang