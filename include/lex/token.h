#pragma once

#include "lex/util.h"
#include <iostream>
#include <locale>
#include <string>
#include <unordered_map>
#include <vector>


enum class TokenType {
    // Special
    START_OF_FILE,
    END_OF_FILE,

    // Identifiers & literals
    IDENTIFIER,
    NUMBER,
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

static const std::unordered_map<std::wstring, TokenType, WStringHash> operators = {
  {L"=", TokenType::EQ},   {L":=", TokenType::ASSIGN}, {L"+", TokenType::PLUS}, {L"-", TokenType::MINUS},
  {L"*", TokenType::STAR}, {L"/", TokenType::SLASH},   {L"<", TokenType::LT},   {L">", TokenType::GT},
  {L"<=", TokenType::LE},  {L">=", TokenType::GE}};

static const std::unordered_map<std::wstring, TokenType, WStringHash> keywords = {
  {L"خطا", TokenType::KW_FALSE},   {L"عدم", TokenType::KW_NONE},    {L"صحيح", TokenType::KW_TRUE},
  {L"و", TokenType::AND},          {L"اخرج", TokenType::KW_RETURN}, {L"اكمل", TokenType::KW_CONTINUE},
  {L"عرف", TokenType::KW_FN},      {L"او", TokenType::OR},          {L"بكل", TokenType::KW_FOR},
  {L"اذا", TokenType::KW_IF},      {L"ليس", TokenType::NOT},        {L"ارجع", TokenType::KW_RETURN},
  {L"طالما", TokenType::KW_WHILE}, {L"ثابت", TokenType::KW_CONST}};

class Token
{
   public:
    using char_type   = wchar_t;
    using string_type = std::wstring;
    using size_type   = std::size_t;

    struct Location
    {
        std::string filepath_{};
        size_type   line_{0};
        size_type   column_{0};

        Location() = default;

        Location(std::string fp, std::array<size_type, 2> coords) :
            filepath_(fp),
            line_(coords[0]),
            column_(coords[1]) {}

        Location(std::string fp, size_type coords[2]) :
            filepath_(fp),
            line_(coords[0]),
            column_(coords[1]) {}
    };

    // Main ctor: take value by value so callers can move temporaries in.
    Token(string_type v, TokenType t, std::array<size_type, 2> coords) :
        value_(std::move(v)),
        type_(t),
        location_("" /*TODO : change to only accept a valid filepath*/, coords) {}

    Token(string_type v, TokenType t, size_type coords[2]) :
        value_(std::move(v)),
        type_(t),
        location_("", coords) {}

    Token()                 = default;
    Token(const Token&)     = default;
    Token(Token&&) noexcept = default;

    Token& operator=(const Token&)     = default;
    Token& operator=(Token&&) noexcept = default;

    // Return const references to avoid copies
    const string_type& str() const;
    TokenType          type() const;
    size_type          size() const;
    size_type          line() const;
    size_type          column() const;
    Location           location() const;

    bool operator==(const Token& other) const;
    bool operator!=(const Token& other) const;

    // friend ostream operator for pretty-printing in tests/logs
    friend std::ostream& operator<<(std::ostream& os, const Token& tok) {
        os << "Token(\"" << to_utf8(tok.value_) << "\", type=" << static_cast<int>(tok.type_)
           << ", line=" << tok.location_.line_ << ", col=" << tok.location_.column_ << ")";
        return os;
    }

   private:
    string_type value_;
    TokenType   type_;
    Location    location_;
};
