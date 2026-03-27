/// lexer.cc

#include "../include/lexer.hpp"
#include "../include/ctype.hpp"
#include "../include/util.hpp"

#include <fstream>

#define CONSUME_BASE_DIGITS(valid_expr, token_type, err_code, detail) \
    do {                                                              \
        number += util::encode_utf8_str(next);                        \
        current = SourceManager_.nextChar();                          \
        bool any = false;                                             \
        for (;;) {                                                    \
            if (current == '_') {                                     \
                current = SourceManager_.nextChar();                  \
                continue;                                             \
            }                                                         \
            if (!(valid_expr))                                        \
                break;                                                \
            number += util::encode_utf8_str(current);                 \
            current = SourceManager_.nextChar();                      \
            any = true;                                               \
        }                                                             \
        if (!any)                                                     \
            diagnostic::panic(err_code, detail);                      \
        return finish(token_type, number, line, col);                 \
    } while (0)

#define OCTAL_DIGIT(c)                                                                                 \
    ((c) >= '0' && (c) <= '7'                                                                          \
            ? true                                                                                     \
            : (IS_DIGIT(c)                                                                             \
                      ? (diagnostic::panic(diagnostic::errc::lexer::Code::INVALID_OCTAL_DIGIT), false) \
                      : false))

#define BINARY_DIGIT(c)                                                                                 \
    ((c) == '0' || (c) == '1'                                                                           \
            ? true                                                                                      \
            : (IS_DIGIT(c)                                                                              \
                      ? (diagnostic::panic(diagnostic::errc::lexer::Code::INVALID_BINARY_DIGIT), false) \
                      : false))

#define FINISH(tt, str, line, col)                                 \
    ({                                                             \
        const tok::Token* _token = MAKE_TOKEN(tt, str, line, col); \
        store(_token);                                             \
        TokStream_.back();                                         \
    })

namespace mylang::lex {

namespace fs = std::filesystem;

FileManager::FileManager(std::string const& filepath)
    : FilePath_(filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in) {
        diagnostic::panic(diagnostic::errc::lexer::Code::FILE_NOT_OPEN, filepath);
    }

    std::string content { std::istreambuf_iterator<char> { in },
        std::istreambuf_iterator<char> { } };
    InputBuffer_ = StringRef(content.data());
    InputBuffer_.trimWhitespace(false, true);
    LastKnownWriteTime_ = fs::last_write_time(filepath);
}

StringRef FileManager::load(std::string const& filepath, bool replace)
{
    if (filepath.empty())
        return "";

    std::ifstream in(filepath, std::ios::binary);
    StringRef ret = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()).data();
    if (!in)
        diagnostic::panic(diagnostic::errc::lexer::Code::FILE_NOT_OPEN, filepath);

    if (replace || FilePath_.empty()) {
        InputBuffer_ = ret;
        FilePath_ = filepath;
        LastKnownWriteTime_ = fs::last_write_time(filepath);
    }

    return ret;
}

void SourceManager::reset()
{
    Context_.line = 1;
    Context_.column = 1;
    Context_.offset = 0;
    Current_ = 0;

    while (!UngetStack_.empty())
        UngetStack_.pop();

    if (FileManager_ && FileManager_->buffer().len() > 0) {
        uint64_t bytes = 0;
        uint32_t cp = util::decode_utf8_at(FileManager_->buffer(), 0, &bytes);
        Current_ = cp;
        CurrentBytes_ = bytes;
    }
}

uint32_t SourceManager::peekChar()
{
    if (!UngetStack_.empty())
        return UngetStack_.top().ch;

    Context saved = Context_;
    uint32_t saved_current = Current_;
    uint32_t cp = nextChar();

    if (cp != 0)
        unget(cp);
    else {
        Context_ = saved;
        Current_ = saved_current;
    }

    return cp;
}

uint32_t SourceManager::currentChar() const
{
    if (Context_.offset >= FileManager_->buffer().len())
        return 0;

    uint64_t bytes = 0;
    return util::decode_utf8_at(FileManager_->buffer(), Context_.offset, &bytes);
}

void SourceManager::consumeChar()
{
    if (Context_.offset >= FileManager_->buffer().len())
        return;

    uint64_t bytes = 0;
    uint32_t cp = util::decode_utf8_at(FileManager_->buffer(), Context_.offset, &bytes);
    advance(cp, bytes);
}

uint32_t SourceManager::nextChar()
{
    consumeChar();
    return currentChar();
}

void SourceManager::unget(uint32_t const cp)
{
    PushbackEntry e;
    e.ch = cp;
    e.ctx = Context_;
    e.bytes = CurrentBytes_;

    rewindPosition_(cp, e.bytes);
    UngetStack_.push(e);
}

void SourceManager::advance(uint32_t const cp, uint64_t const bytes)
{
    Context_.offset += bytes;

    if (cp == '\n') {
        ++Context_.line;
        Context_.column = 1;
    } else
        ++Context_.column;
}

void SourceManager::rewindPosition_(uint32_t const cp, uint64_t const bytes)
{
    if (Context_.offset < bytes)
        throw std::runtime_error("SourceManager: attempted to rewind past beginning of file");

    Context_.offset -= bytes;

    if (cp == '\n') {
        Context_.line = (Context_.line > 1) ? (Context_.line - 1) : 1;
        Context_.column = calculateColumnAtOffset(Context_.offset);
    } else
        Context_.column = (Context_.column > 1) ? (Context_.column - 1) : 1;
}

uint32_t SourceManager::calculateColumnAtOffset(uint64_t const target_offset) const
{
    StringRef const& buf = FileManager_->buffer();

    uint64_t line_start = target_offset;
    while (line_start > 0) {
        if (buf.data()[line_start - 1] == '\n')
            break;

        --line_start;
    }

    uint32_t column = 1;
    uint64_t pos = line_start;

    while (pos < target_offset) {
        uint64_t bytes = 0;
        util::decode_utf8_at(buf, pos, &bytes);
        pos += bytes;
        ++column;
    }

    return column;
}

Lexer::Lexer(FileManager* fm)
    : SourceManager_(fm)
    , TokIndex_(0)
    , IndentSize_(4)
    , IndentLevel_(0)
    , AtBOL_(true)
{
    TokStream_ = Array<mylang::tok::Token const*>::withCapacity(1024);
    IndentStack_ = Array<unsigned int>::withCapacity(8);
    AltIndentStack_ = Array<unsigned int>::withCapacity(8);
    IndentStack_.push(0);
    AltIndentStack_.push(0);

    util::configureLocale();
}

Lexer::Lexer(Array<tok::Token const*>& seq /*, size_t const s*/)
    : TokStream_(seq)
    , TokIndex_(0)
    , IndentSize_(4)
    , IndentLevel_(0)
    , AtBOL_(true)
{
    IndentStack_.push(0);
    AltIndentStack_.push(0);
    util::configureLocale();
}

tok::Token const* Lexer::lexToken()
{
    auto finish = [this](tok::TokenType tt, StringRef str, size_t line, size_t col) {
        tok::Token const* ret = MAKE_TOKEN(tt, str, line, col);
        store(ret);
        return TokStream_.back();
    };

    if (TokStream_.empty())
        return FINISH(tok::TokenType::BEGINMARKER, "", 1, 1);

    auto nextLine = [this](uint32_t line, uint32_t col) {
        if (!AtBOL_)
            return;
        AtBOL_ = false;

        unsigned int size = 0;
        unsigned int alt_size = 0;
        unsigned int cont_line_col = 0;

        uint32_t current = SourceManager_.currentChar();

        // count leading whitespace
        for (;;) {
            if (current == ' ') {
                SourceManager_.consumeChar();
                ++size;
                ++alt_size;
            } else if (current == '\t') {
                SourceManager_.consumeChar();
                size = (size / IndentSize_ + 1) * IndentSize_;
                alt_size = (alt_size / TABWIDTH + 1) * TABWIDTH;
            } else if (current == '\f') {
                // form-feed resets the column counter — handled explicitly,
                // NOT via ml_is_space, because it has special indent semantics.
                SourceManager_.consumeChar();
                size = alt_size = 0;
            } else if (current == '\\') {
                SourceManager_.consumeChar();
                if (!cont_line_col)
                    cont_line_col = size;
            } else {
                break;
            }
            current = SourceManager_.currentChar();
        }

        // blank
        if (current == '#' || current == '\n' || current == '\r')
            return;
        if (current == 0)
            return;

        if (cont_line_col) {
            size = alt_size = cont_line_col;
        }

        if (size == IndentStack_.back()) {
            if (alt_size != AltIndentStack_.back())
                diagnostic::panic(diagnostic::errc::lexer::Code::INCONSISTENT_INDENTATION);

        } else if (size > IndentStack_.back()) {
            if (IndentLevel_ + 1 > MAX_ALLOWED_INDENT)
                diagnostic::panic(diagnostic::errc::lexer::Code::TOO_MANY_INDENT_LEVELS);
            if (alt_size <= AltIndentStack_.back())
                diagnostic::panic(diagnostic::errc::lexer::Code::MIXED_INDENTATION);

            ++IndentLevel_;
            IndentStack_.push(size);
            AltIndentStack_.push(alt_size);
            store(MAKE_TOKEN(tok::TokenType::INDENT, "", line, col));

        } else {
            unsigned int dedent_count = 0;
            while (IndentLevel_ > 0 && size < IndentStack_.back()) {
                --IndentLevel_;
                IndentStack_.pop();
                AltIndentStack_.pop();
                ++dedent_count;
            }
            if (size != IndentStack_.back())
                diagnostic::panic(diagnostic::errc::lexer::Code::INVALID_UNINDENT);
            if (alt_size != AltIndentStack_.back())
                diagnostic::panic(diagnostic::errc::lexer::Code::INCONSISTENT_INDENTATION);

            for (unsigned int i = 0; i < dedent_count; ++i)
                store(MAKE_TOKEN(tok::TokenType::DEDENT, "", line, col));
        }
    };

    for (;;) {
        uint32_t line = SourceManager_.getLineNumber();
        uint32_t col = SourceManager_.getColumnNumber();
        uint32_t current = SourceManager_.currentChar();

        if (current == 0)
            break;

        if (current == '\n') {
            SourceManager_.consumeChar();
            AtBOL_ = true;
            tok::Token const* ret = MAKE_TOKEN(tok::TokenType::NEWLINE, util::encode_utf8_str('\n'), line, col);
            store(ret);
            nextLine(line, col);
            return ret;
        }

        if (current == ' ' || current == '\t' || current == '\r') {
            SourceManager_.consumeChar();
            continue;
        }

        if (current == '#') {
            while (current != '\n' && current != 0)
                current = SourceManager_.nextChar();
            continue;
        }

        if (current == '\'' || current == '"') {
            StringRef string_literal;
            uint32_t quote = current;
            current = SourceManager_.nextChar();

            while (current != '\n' && current != 0 && current != quote) {
                if (current == '\\') {
                    current = SourceManager_.nextChar();
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

            if (current != quote)
                return finish(tok::TokenType::INVALID, string_literal, line, col);

            SourceManager_.consumeChar(); // closing quote
            return FINISH(tok::TokenType::STRING, string_literal, line, col);
        }

        if (current == ',' || current == '.' || current == u'،' || current == u'٬') {
            SourceManager_.consumeChar();
            return finish((current == '.') ? tok::TokenType::DOT : tok::TokenType::COMMA, util::encode_utf8_str(current), line, col);
        }

        if (current == '[' || current == ']' || current == '(' || current == ')' || current == ':') {
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
                if (SourceManager_.currentChar() == '=') {
                    symbol += util::encode_utf8_str('=');
                    SourceManager_.consumeChar();
                    tt = tok::TokenType::OP_ASSIGN;
                } else {
                    tt = tok::TokenType::COLON;
                }
                break;
            }
            default:
                tt = tok::TokenType::INVALID;
                break;
            }
            return FINISH(tt, symbol, line, col);
        }

        if (current == '=' || current == '<' || current == '>'
            || current == '!' || current == '+' || current == '-'
            || current == '|' || current == '&' || current == '*'
            || current == '/' || current == '%' || current == u'٪'
            || current == '^' || current == '~') {
            StringRef operator_str;
            operator_str += util::encode_utf8_str(current);
            SourceManager_.consumeChar();

            uint32_t next = SourceManager_.currentChar();
            if (next != 0) {
                StringRef two = operator_str + util::encode_utf8_str(next);
                if (tok::lookupOperator(two)) {
                    operator_str = two;
                    SourceManager_.consumeChar();
                }
            }

            if (auto type = tok::lookupOperator(operator_str))
                return FINISH(*type, operator_str, line, col);

            return FINISH(tok::TokenType::INVALID, operator_str, line, col);
        }

        if (IS_DIGIT(current)) {
            StringRef number;
            number += util::encode_utf8_str(current);
            uint32_t next = SourceManager_.nextChar();

            // numeric base prefix: 0x / 0o / 0b
            if (current == '0') {
                if (next == 'x' || next == 'X')
                    CONSUME_BASE_DIGITS(IS_XDIGIT(current), tok::TokenType::HEX, diagnostic::errc::lexer::Code::INVALID_BASE_LITERAL, "no digits after 0x");

                if (next == 'o' || next == 'O')
                    CONSUME_BASE_DIGITS(OCTAL_DIGIT(current), tok::TokenType::OCTAL, diagnostic::errc::lexer::Code::INVALID_BASE_LITERAL, "no digits after 0o");

                if (next == 'b' || next == 'B')
                    CONSUME_BASE_DIGITS(BINARY_DIGIT(current), tok::TokenType::BINARY, diagnostic::errc::lexer::Code::INVALID_BASE_LITERAL, "no digits after 0b");
            }

            current = SourceManager_.currentChar();
            for (;;) {
                if (current == '_') {
                    current = SourceManager_.nextChar();
                    continue;
                }
                if (!IS_DIGIT(current))
                    break;
                number += util::encode_utf8_str(current);
                current = SourceManager_.nextChar();
            }

            if (current == '.') {
                number += util::encode_utf8_str(current);
                current = SourceManager_.nextChar();
                for (;;) {
                    if (current == '_') {
                        current = SourceManager_.nextChar();
                        continue;
                    }
                    if (!IS_DIGIT(current))
                        break;
                    number += util::encode_utf8_str(current);
                    current = SourceManager_.nextChar();
                }
                return FINISH(tok::TokenType::DECIMAL, number, line, col);
            }

            return FINISH(tok::TokenType::INTEGER, number, line, col);
        }

        if (IS_ARDIGIT(current)) {
            StringRef digits;
            while (IS_ARDIGIT(current)) {
                digits += util::encode_utf8_str(current);
                current = SourceManager_.nextChar();
            }
            return FINISH(tok::TokenType::INTEGER, digits, line, col);
        }

        if (IS_IDENT_S(current)) {
            StringRef identifier;
            while (IS_IDENT_C(current)) {
                identifier += util::encode_utf8_str(current);
                current = SourceManager_.nextChar();
            }
            tok::TokenType tt = tok::TokenType::IDENTIFIER;
            if (auto type = tok::lookupKeyword(identifier))
                tt = *type;
            return FINISH(tt, identifier, line, col);
        }

        StringRef source_line = getLineAt(line);
        std::string snippet = source_line.empty() ? std::string() : std::string(source_line.data(), source_line.len());
        diagnostic::report(diagnostic::Severity::ERROR, line, col, diagnostic::errc::lexer::Code::INVALID_CHARACTER, snippet);
        diagnostic::panic(diagnostic::errc::lexer::Code::INVALID_CHARACTER, "U+" + [](uint32_t cp) {
                char buf[8]; std::snprintf(buf, sizeof(buf), "%04X", cp); return std::string(buf); }(current));
    }

    if (!TokStream_.empty() && TokStream_.back()->type() == tok::TokenType::ENDMARKER)
        return TokStream_.back();

    uint32_t last_line = SourceManager_.getLineNumber();
    uint32_t last_col = SourceManager_.getColumnNumber();

    while (IndentLevel_ > 0) {
        --IndentLevel_;
        IndentStack_.pop();
        AltIndentStack_.pop();
        store(MAKE_TOKEN(tok::TokenType::DEDENT, "", last_line, last_col));
    }

    return FINISH(tok::TokenType::ENDMARKER, "", last_line, last_col);
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

tok::Token const* Lexer::current() const
{
    if (TokIndex_ < TokStream_.size())
        return TokStream_[TokIndex_];

    if (!TokStream_.empty())
        return TokStream_.back();

    return nullptr;
}

tok::Token const* Lexer::peek(size_t n)
{
    while (TokIndex_ + n >= TokStream_.size()) {
        if (!TokStream_.empty() && TokStream_.back()->type() == tok::TokenType::ENDMARKER)
            return TokStream_.back();

        lexToken();
    }

    return TokStream_[TokIndex_ + n];
}

void Lexer::store(tok::Token const* tok) { TokStream_.push(tok); }

Array<tok::Token const*> Lexer::tokenize()
{
    while (next()->type() != tok::TokenType::ENDMARKER)
        ;

    return this->TokStream_;
}

} // namespace mylang::lex
