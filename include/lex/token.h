#pragma once

#include "../utfcpp/source/utf8.h"
#include "lex/util.h"

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

static const std::unordered_map<std::u16string, TokenType, util::U16StringHash, util::U16StringEqual> operators = {
  {u"=", TokenType::EQ},   {u":=", TokenType::ASSIGN}, {u"+", TokenType::PLUS}, {u"-", TokenType::MINUS},
  {u"*", TokenType::STAR}, {u"/", TokenType::SLASH},   {u"<", TokenType::LT},   {u">", TokenType::GT},
  {u"<=", TokenType::LE},  {u">=", TokenType::GE},     {u"!=", TokenType::NEQ}};

static const std::unordered_map<std::u16string, TokenType, util::U16StringHash, util::U16StringEqual> keywords = {
  {u"خطا", TokenType::KW_FALSE},   {u"عدم", TokenType::KW_NONE},    {u"صحيح", TokenType::KW_TRUE},
  {u"و", TokenType::AND},          {u"اخرج", TokenType::KW_RETURN}, {u"اكمل", TokenType::KW_CONTINUE},
  {u"عرف", TokenType::KW_FN},      {u"او", TokenType::OR},          {u"بكل", TokenType::KW_FOR},
  {u"اذا", TokenType::KW_IF},      {u"ليس", TokenType::NOT},        {u"ارجع", TokenType::KW_RETURN},
  {u"طالما", TokenType::KW_WHILE}, {u"ثابت", TokenType::KW_CONST}};

class Token
{
   public:
    using char_type   = wchar_t;
    using string_type = std::u16string;
    using size_type   = std::size_t;

    struct Location
    {
        std::string filepath_;
        size_type   line_{0};
        size_type   column_{0};
        size_type   file_pos_{0};

        Location() = default;

        Location(std::string fp, std::array<size_type, 3> coords) :
            filepath_(fp),
            line_(coords[0]),
            column_(coords[1]),
            file_pos_(coords[2])
        {
        }

        Location(std::string fp, size_type coords[3]) :
            filepath_(fp),
            line_(coords[0]),
            column_(coords[1]),
            file_pos_(coords[2])
        {
        }
    };

    // Main ctor: take value by value so callers can move temporaries in.
    Token(string_type v, TokenType t, std::array<size_type, 3> coords, std::string fp) :
        value_(std::move(v)),
        type_(t),
        location_(fp, coords)
    {
    }

    Token(string_type v, TokenType t, size_type coords[3], std::string fp) :
        value_(std::move(v)),
        type_(t),
        location_(fp, coords)
    {
    }

    Token()                 = default;
    Token(const Token&)     = default;
    Token(Token&&) noexcept = default;

    Token& operator=(const Token&)     = default;
    Token& operator=(Token&&) noexcept = default;

    // Return const references to avoid copies
    const string_type& str() const;

    const TokenType& type() const;

    size_type size() const;

    const size_type& line() const;

    const size_type& column() const;

    const Location& location() const;

    const std::string& filepath() const;

    bool operator==(const Token& other) const;
    bool operator!=(const Token& other) const;

    // friend ostream operator for pretty-printing in tests/logs
    friend std::ostream& operator<<(std::ostream& os, const Token& tok)
    {
        os << "Token(\"" << utf8::utf16to8(tok.value_) << "\", type=" << static_cast<int>(tok.type_)
           << ", line=" << tok.location_.line_ << ", col=" << tok.location_.column_
           << "\", file_pos=" << tok.location_.file_pos_ << "\", file path=" << tok.location_.filepath_ << ")";
        return os;
    }

   private:
    string_type value_;
    TokenType   type_;
    Location    location_;
};
}
}  // lex
}  // mylang