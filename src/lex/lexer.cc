#include "../../include/lex/lexer.hpp"
#include "../../include/util.hpp"

namespace mylang {
namespace lex {

Lexer::Lexer(FileManager* fm)
    : SourceManager_(fm)
    , TokIndex_(0)
    , IndentSize_(4)
    , IndentLevel_(0)
    , AtBOL_(true)
{
    TokStream_.reserve(1024);
    IndentStack_.reserve(32);
    AltIndentStack_.reserve(32);
    IndentStack_.push_back(0);
    AltIndentStack_.push_back(0);

    util::configureLocale();
}

Lexer::Lexer(std::vector<tok::Token const*>& seq, SizeType const s)
    : TokStream_(seq)
    , TokIndex_(0)
    , IndentSize_(4)
    , IndentLevel_(0)
    , AtBOL_(true)
{
    IndentStack_.push_back(0);
    AltIndentStack_.push_back(0);
    util::configureLocale();
}

tok::Token const* Lexer::lexToken()
{
    auto finish = [this](tok::TokenType tt, StringRef str, SizeType line, SizeType col) {
        tok::Token const* ret = make_token(tt, str, line, col);
        store(ret);
        return TokStream_.back();
    };

    if (TokStream_.empty()) {
        tok::Token const* ret = make_token(tok::TokenType::BEGINMARKER, "", 1, 1);
        store(ret);
        return TokStream_.back();
    }

    auto nextLine = [this](SizeType const& line, SizeType const& col) {
        uint32_t current = SourceManager_.currentChar();

        if (AtBOL_) {
            unsigned int size = 0;
            unsigned int alt_size = 0;
            unsigned int cont_line_col = 0;
            bool blank_line = false;

            AtBOL_ = false;

            // calculate indentation size
            for (;;) {
                if (current == ' ') {
                    SourceManager_.consumeChar();
                    size++, alt_size++;
                } else if (current == '\t') {
                    SourceManager_.consumeChar();
                    size = (size / IndentSize_ + 1) * IndentSize_;
                    alt_size = (alt_size / TABWIDTH + 1) * TABWIDTH;
                } else if (current == '\f') {
                    SourceManager_.consumeChar();
                    size = alt_size = 0;
                } else if (current == '\\') {
                    SourceManager_.consumeChar();
                    cont_line_col = cont_line_col ? cont_line_col : size;
                } else if (current == BUFFER_END) {
                    break; // Don't panic at EOF during indentation check
                } else {
                    break;
                }
                current = SourceManager_.currentChar();
            }

            // check if a blank line
            if (current == '#' || current == '\n' || current == '\r') {
                if (size == 0 && current == '\n')
                    blank_line = false; // empty
                else
                    blank_line = true; // comment or whitespace-only
            }

            // process indentation
            if (!blank_line && current != BUFFER_END) {
                size = cont_line_col ? cont_line_col : size;
                alt_size = cont_line_col ? cont_line_col : alt_size;

                if (size == IndentStack_.back()) {
                    // no change
                    // consistency check
                    if (alt_size != AltIndentStack_.back())
                        diagnostic::engine.panic("Inconsistent indentation");
                }
                // Indent
                else if (size > IndentStack_.back()) {
                    if (IndentLevel_ + 1 > MAX_ALLOWED_INDENT)
                        diagnostic::engine.panic("Too many indentation levels");

                    if (alt_size <= AltIndentStack_.back())
                        diagnostic::engine.panic("Inconsistent indentation (tabs/spaces)");

                    ++IndentLevel_;
                    IndentStack_.push_back(size);
                    AltIndentStack_.push_back(alt_size);

                    store(make_token(tok::TokenType::INDENT, "", line, col));
                }
                // Dedent
                else /*size < IndentStack_.back()*/ {
                    unsigned int dedent_count = 0;

                    while (IndentLevel_ > 0 && size < IndentStack_.back()) {
                        --IndentLevel_;
                        IndentStack_.pop_back();
                        AltIndentStack_.pop_back();
                        ++dedent_count;
                    }

                    if (size != IndentStack_.back())
                        diagnostic::engine.panic("Unindent does not match any outer level of indentation");

                    if (alt_size != AltIndentStack_.back())
                        diagnostic::engine.panic("Inconsistent indentation");

                    for (unsigned int i = 0; i < dedent_count; ++i)
                        store(make_token(tok::TokenType::DEDENT, "", line, col));
                }
            }
        }
    };

    for (;;) {
        SizeType line = SourceManager_.getLineNumber();
        SizeType col = SourceManager_.getColumnNumber();

        uint32_t current = SourceManager_.currentChar();
        if (current == BUFFER_END)
            break;

        switch (current) {
        case '\n': {
            SourceManager_.consumeChar();
            AtBOL_ = true;
            tok::Token const* ret = make_token(tok::TokenType::NEWLINE, util::encode_utf8_str('\n'), line, col);
            store(ret);
            nextLine(line, col);
            return ret;
        }

        // ignore whitespace if not at beginning of a new line
        case ' ':
        case '\t':
        case '\r':
            SourceManager_.consumeChar();
            continue;

        // comments
        case '#': {
            // consume entire comment line
            while (current != '\n' && current != BUFFER_END)
                current = SourceManager_.nextChar();

            continue;
        }

        // string literal
        case '\'':
        case '"': {
            StringRef string_literal;
            uint32_t quote = current; // account for ' and "
            current = SourceManager_.nextChar();

            // capture quoted string with escape sequence support
            while (current != '\n' && current != BUFFER_END && current != quote) {
                if (current == '\\') {
                    current = SourceManager_.nextChar();

                    // handle escape sequences
                    switch (current) {
                    case 'n':
                        string_literal += '\n';
                        break;
                    case 't':
                        string_literal += '\t';
                        break;
                    case 'r':
                        string_literal += '\r';
                        break;
                    case '\\':
                        string_literal += '\\';
                        break;
                    case '\'':
                        string_literal += '\'';
                        break;
                    case '"':
                        string_literal += '"';
                        break;
                    case '0':
                        string_literal += '\0';
                        break;
                    default:
                        // if not a recognized escape, keep the backslash
                        string_literal += '\\';
                        string_literal += util::encode_utf8_str(current);
                        break;
                    }

                    SourceManager_.consumeChar();
                } else {
                    string_literal += util::encode_utf8_str(current);
                    current = SourceManager_.nextChar();
                }
            }

            // ensure terminated with quotes
            if (current != quote)
                return finish(tok::TokenType::INVALID, string_literal, line, col);

            SourceManager_.consumeChar(); // closing quote

            return finish(tok::TokenType::STRING, string_literal, line, col);
        }

        // separators
        case ',':
        case '.': {
            SourceManager_.consumeChar();
            tok::Token const* ret = finish(current == ',' ? tok::TokenType::COMMA : tok::TokenType::DOT,
                util::encode_utf8_str(current), line, col);

            return ret;
        }

        // identifiers
        case 0x0621:
        case 0x0622:
        case 0x0623:
        case 0x0624:
        case 0x0625:
        case 0x0626:
        case 0x0627:
        case 0x0628:
        case 0x0629:
        case 0x062A:
        case 0x062B:
        case 0x062C:
        case 0x062D:
        case 0x062E:
        case 0x062F:
        case 0x0630:
        case 0x0631:
        case 0x0632:
        case 0x0633:
        case 0x0634:
        case 0x0635:
        case 0x0636:
        case 0x0637:
        case 0x0638:
        case 0x0639:
        case 0x063A:
        case 0x0641:
        case 0x0642:
        case 0x0643:
        case 0x0644:
        case 0x0645:
        case 0x0646:
        case 0x0647:
        case 0x0648:
        case 0x0649:
        case 0x064A: {
            StringRef identifier;

            while (util::isalphaArabic(current) || current == '_' || ::isdigit(current)) {
                identifier += util::encode_utf8_str(current);
                current = SourceManager_.nextChar();
            }

            // check if this identifier is a reserved keyword
            tok::TokenType tt = tok::TokenType::IDENTIFIER;
            if (tok::keywords.count(identifier))
                tt = tok::keywords.at(identifier);

            return finish(tt, identifier, line, col);
        }

        // operators
        case '=':
        case '<':
        case '>':
        case '!':
        case '+':
        case '-':
        case '|':
        case '&':
        case '*':
        case '/': {
            StringRef operator_str;
            operator_str += util::encode_utf8_str(current);
            SourceManager_.consumeChar();

            uint32_t next = SourceManager_.currentChar();
            if (next != BUFFER_END) {
                StringRef two = operator_str + util::encode_utf8_str(next);
                if (tok::operators.count(two)) {
                    operator_str = two;
                    SourceManager_.consumeChar();
                }
            }

            // check if the operator exists in the map
            if (!tok::operators.count(operator_str))
                return finish(tok::TokenType::INVALID, operator_str, line, col);

            return finish(tok::operators.at(operator_str), operator_str, line, col);
        }

        case '[':
        case ']':
        case '(':
        case ')':
        case ':': {
            StringRef symbol;
            symbol += util::encode_utf8_str(current);
            SourceManager_.consumeChar();

            tok::TokenType tt;
            switch (current) {
            case '[':
                tt = tok::TokenType::LBRACKET;
                break;
            case ']':
                tt = tok::TokenType::RBRACKET;
                break;
            case '(':
                tt = tok::TokenType::LPAREN;
                break;
            case ')':
                tt = tok::TokenType::RPAREN;
                break;
            case ':': {
                uint32_t next_c = SourceManager_.currentChar();
                if (next_c == '=') {
                    symbol += util::encode_utf8_str('=');
                    SourceManager_.consumeChar();
                    tt = tok::TokenType::OP_ASSIGN;
                } else {
                    tt = tok::TokenType::COLON;
                }
            } break;
            default:
                tt = tok::TokenType::INVALID;
                break;
            }

            return finish(tt, symbol, line, col);
        }

        default:
            break;
        }

        // number literal
        if (::isdigit(current)) {
            StringRef number;
            number += util::encode_utf8_str(current);
            uint32_t next = SourceManager_.nextChar();

            if (current == '0') {
                // hex
                if (next == 'x' || next == 'X') {
                    number += util::encode_utf8_str(next);
                    current = SourceManager_.nextChar();

                    // should have digits to be valid hex
                    bool has_digits = false;

                    for (;;) {
                        if (current == '_') {
                            current = SourceManager_.nextChar();
                            continue;
                        }

                        // check for valid hex digits (0-9, A-F, a-f)
                        if (::isxdigit(current)) {
                            number += util::encode_utf8_str(current);
                            current = SourceManager_.nextChar();
                            has_digits = true;
                        } else
                            break;
                    }

                    if (!has_digits)
                        diagnostic::engine.panic("Invalid hex literal, it has no digits after 0x");

                    return finish(tok::TokenType::NUMBER, number, line, col);
                }
                // octal
                else if (next == 'o' || next == 'O') {
                    number += util::encode_utf8_str(next);
                    current = SourceManager_.nextChar();

                    bool has_digits = false;

                    for (;;) {
                        if (current == '_') {
                            current = SourceManager_.nextChar();
                            continue;
                        }

                        // check for valid octal digits (0-7)
                        if (current >= '0' && current <= '7') {
                            number += util::encode_utf8_str(current);
                            current = SourceManager_.nextChar();
                            has_digits = true;
                        } else if (::isdigit(current))
                            diagnostic::engine.panic("Invalid digit '" + std::string(1, current) + "' in octal literal");
                        else
                            break;
                    }

                    if (!has_digits)
                        diagnostic::engine.panic("Invalid octal literal: no digits after 0o");

                    return finish(tok::TokenType::NUMBER, number, line, col);
                }
                // binary
                else if (next == 'b' || next == 'B') {
                    number += util::encode_utf8_str(next); // consume peeked
                    current = SourceManager_.nextChar();

                    bool has_digits = false;

                    for (;;) {
                        if (current == '_') {
                            current = SourceManager_.nextChar();
                            continue;
                        }

                        // valid digits in binary (0 and 1)
                        if (current == '0' || current == '1') {
                            number += util::encode_utf8_str(current);
                            current = SourceManager_.nextChar();
                            has_digits = true;
                        } else if (::isdigit(current))
                            diagnostic::engine.panic("Invalid digit '" + std::string(1, current) + "' in binary literal");
                        else
                            break;
                    }

                    if (!has_digits)
                        diagnostic::engine.panic("Invalid binary literal: no digits after 0b");

                    return finish(tok::TokenType::NUMBER, number, line, col);
                }
                // If it's just '0' followed by regular digits or '.', fall through to decimal parsing
            }

            current = SourceManager_.currentChar(); // sanity check

            // integer part, might be followed by a decimal part
            for (;;) {
                if (current == '_') {
                    current = SourceManager_.nextChar();
                    continue;
                }

                if (::isdigit(current)) {
                    number += util::encode_utf8_str(current);
                    current = SourceManager_.nextChar();
                    continue;
                } else
                    break;
            }

            // check if decimal
            if (current == '.') {
                number += util::encode_utf8_str(current);
                current = SourceManager_.nextChar();

                for (;;) {
                    if (current == '_') {
                        current = SourceManager_.nextChar();
                        continue;
                    }

                    if (::isdigit(current)) {
                        number += util::encode_utf8_str(current);
                        current = SourceManager_.nextChar();
                        continue;
                    } else
                        break;
                }

                return finish(tok::TokenType::FLOAT, number, line, col);
            }

            return finish(tok::TokenType::NUMBER, number, line, col);
        }

        return finish(tok::TokenType::INVALID, util::encode_utf8_str(current), line, col);
    }

    if (!TokStream_.empty() && TokStream_.back()->type() == tok::TokenType::ENDMARKER)
        return TokStream_.back();

    // Emit any remaining dedents at EOF
    SizeType last_line = SourceManager_.getLineNumber();
    SizeType last_col = SourceManager_.getColumnNumber();

    while (IndentLevel_ > 0) {
        --IndentLevel_;
        IndentStack_.pop_back();
        AltIndentStack_.pop_back();
        store(make_token(tok::TokenType::DEDENT, "", last_line, last_col));
    }

    tok::Token const* ret = make_token(tok::TokenType::ENDMARKER, "", last_line, last_col);
    store(ret);

    return TokStream_.back();
}

tok::Token const* Lexer::next()
{
    if (TokIndex_ + 1 < TokStream_.size())
        return TokStream_[++TokIndex_];

    while (true) {
        lexToken();

        if (!TokStream_.empty() && TokStream_.back()->type() == tok::TokenType::ENDMARKER)
            break;

        if (TokIndex_ + 1 < TokStream_.size())
            break;
    }

    if (TokIndex_ + 1 < TokStream_.size())
        ++TokIndex_;

    return TokStream_[TokIndex_];
}

tok::Token const* Lexer::prev()
{
    if (TokIndex_ > 0)
        --TokIndex_;
    else
        return TokStream_.empty()
            ? nullptr
            : TokStream_.front();

    return TokStream_[TokIndex_];
}

tok::Token const* Lexer::current() const
{
    if (TokIndex_ < TokStream_.size())
        return TokStream_[TokIndex_];

    if (!TokStream_.empty())
        return TokStream_.back();

    return nullptr;
}

tok::Token const* Lexer::peek(SizeType n)
{
    while (TokIndex_ + n >= TokStream_.size()) {
        if (!TokStream_.empty() && TokStream_.back()->type() == tok::TokenType::ENDMARKER)
            return TokStream_.back();

        lexToken();
    }

    return TokStream_[TokIndex_ + n];
}

void Lexer::store(tok::Token const* tok)
{
    TokStream_.push_back(tok);
}

std::vector<tok::Token const*> Lexer::tokenize()
{
    while (next()->type() != tok::TokenType::ENDMARKER)
        ;

    return this->TokStream_;
}

void Lexer::updateIndentationContext_(tok::Token const& token)
{
    // Placeholder for future use
}

}
}
