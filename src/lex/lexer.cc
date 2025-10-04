#include "lex/lexer.h"
#include "lex/token.h"
#include "lex/util.h"
#include "macros.h"

#include <algorithm>
#include <memory>
#include <string>


void Lexer::handle_indentation_(size_type line, size_type col) {
    char_type current = this->source_manager_.current();

    // only care if next char is space or tab
    if (current != u' ' && current != u'\t')
    {
        return;
    }

    size_type space_count = 0;
    bool      used_tab    = false;
    bool      used_space  = false;

    // count indentation width
    while (current == u' ' || current == u'\t')
    {
        this->consume_char();

        if (current == u' ')
        {
            space_count += 1;
            used_space = true;
        }
        else if (current == u'\t')
        {
            // tab expands to next multiple of TABWIDTH
            space_count += TABWIDTH - (space_count % TABWIDTH);
            used_tab = true;
        }

        current = this->source_manager_.current();
    }

    // Blank line? -> ignore indentation
    char_type lookahead = this->source_manager_.peek();
    if (lookahead == u'\n' || lookahead == BUF_END)
    {
        return;
    }

    // Mixed spaces and tabs = error
    if (used_space && used_tab)
    {
        throw std::runtime_error("Mixed tabs and spaces in indentation");
    }

    // First indent defines indent size
    if (this->indent_stack_.empty() && this->indent_size_ == 0 && space_count > 0)
    {
        if (space_count > MAX_ALLOWED_INDENT)
        {
            throw std::runtime_error("Indentation too large for first indent");
        }

        this->indent_size_ = space_count;  // base size
    }

    // Ensure consistent multiples of base indent
    if (this->indent_size_ > 0 && space_count % this->indent_size_ != 0)
    {
        throw std::runtime_error("Inconsistent indentation detected");
    }

    size_type indent_count = (this->indent_size_ == 0) ? 0 : space_count / this->indent_size_;
    int       prev         = this->indent_stack_.empty() ? 0 : this->indent_stack_.top();

    if (indent_count > prev)
    {
        // new indent → push level and emit INDENT tokens
        this->indent_stack_.push(indent_count);

        for (size_type i = 0, n = indent_count - prev; i < n; i++)
        {
            string_type s;
            Token       tok(std::move(s), TokenType::INDENT, {line, col});
            this->tok_stream_.push_back(std::move(tok));
            col += this->indent_size_;
        }
    }
    else if (indent_count < prev)
    {
        // dedent → pop until we match a previous level
        while (!this->indent_stack_.empty() && indent_count < this->indent_stack_.top())
        {
            this->indent_stack_.pop();
            string_type s = u"";
            Token       tok(std::move(s), TokenType::DEDENT, {line, col});
            this->tok_stream_.push_back(std::move(tok));
        }

        // if indent_count does not match any previous level → error
        if (this->indent_stack_.empty() || indent_count != this->indent_stack_.top())
        {
            throw std::runtime_error("Indentation does not match any previous level");
        }
    }
}

Token Lexer::handle_identifier_(char_type c, size_type line, size_type col) {
    string_type id(1, c);
    this->consume_char();

    char_type c2 = this->source_manager_.current();
    while (isalpha_arabic(c2) || c2 == u'_' || std::iswdigit(c2))
    {
        id.push_back(c2);
        this->consume_char();
        c2 = this->source_manager_.current();
    }

    TokenType tt = TokenType::IDENTIFIER;
    if (keywords.count(id))
    {
        tt = keywords.at(id);
    }

    Token tok(std::move(id), tt, {line, col});
    this->store(std::move(tok));
    return this->tok_stream_.back();
}

Token Lexer::handle_number_(char_type c, size_type line, size_type col) {
    string_type num(1, c);
    this->consume_char();

    char_type c2 = this->source_manager_.current();
    while (std::iswdigit(c2))
    {
        num.push_back(c2);
        this->consume_char();
        c2 = this->source_manager_.current();
    }

    Token ret(std::move(num), TokenType::NUMBER, {line, col});
    store(std::move(ret));
    return this->tok_stream_.back();
}

Token Lexer::handle_operator_(char_type c, size_type line, size_type col) {
    string_type op(1, c);
    this->consume_char();

    char_type lookahead = this->source_manager_.peek();
    if (lookahead != BUF_END)
    {
        string_type two = op;
        two.push_back(lookahead);

        if (operators.count(two))
        {
            op.push_back(this->consume_char());
        }
    }

    TokenType tt = operators.count(op) ? operators.at(op) : TokenType::UNKNOWN;
    Token     ret(std::move(op), tt, {line, col});
    this->store(std::move(ret));
    return this->tok_stream_.back();
}

Token Lexer::handle_symbol_(char_type c, size_type line, size_type col) {
    TokenType   tt;
    string_type sym(1, c);
    this->consume_char();

    switch (c)
    {
    case '(' :
        tt = TokenType::LPAREN;
        break;
    case ')' :
        tt = TokenType::RPAREN;
        break;
    case '.' :
        tt = TokenType::DOT;
        break;
    case ':' :
        if (this->source_manager_.peek() == u'=')
        {
            this->consume_char();
            sym = u":=";
            tt  = TokenType::ASSIGN;
        }
        else
        {
            tt = TokenType::COLON;
        }
        break;
    default :
        tt = TokenType::UNKNOWN;
        break;
    }

    this->store(Token(std::move(sym), tt, {line, col}));
    return this->tok_stream_.back();
}

Token Lexer::handle_string_literal_(char_type c, size_type line, size_type col) {
    string_type s;
    char_type   quote = c;
    this->consume_char();  // consume opening quote

    char_type c2 = this->source_manager_.current();
    while (c2 != u'\n' && c2 != BUF_END && c2 != quote)
    {
        s.push_back(c2);
        this->consume_char();
        c2 = this->source_manager_.current();
    }

    // now current() is at the closing quote
    if (c2 == quote)
    {
        this->consume_char();  // consume closing quote
    }
    else
    {
        // Unterminated string literal
        return Token(std::move(s), TokenType::UNKNOWN, {line, col});
    }

    Token ret(std::move(s), TokenType::STRING, {line, col});
    this->store(std::move(ret));
    return this->tok_stream_.back();
}

Token Lexer::emit_eof_() {
    this->consume_char();
    if (!tok_stream_.empty() && tok_stream_.back().type() == TokenType::END_OF_FILE)
    {
        return tok_stream_.back();
    }

    Token ret(u"", TokenType::END_OF_FILE, {source_manager_.line(), source_manager_.column() - 1});
    store(std::move(ret));
    return tok_stream_.back();
}

Token Lexer::emit_sof_() {
    Token ret(u"", TokenType::START_OF_FILE, {1, 1});
    this->tok_stream_.push_back(ret);
    this->tok_index_ = 0;
    return this->tok_stream_.back();
}

Token Lexer::handle_newline_(char_type c, size_t line, size_t col) {
    this->consume_char();
    string_type endl = u"\n";
    Token       ret(std::move(endl), TokenType::NEWLINE, {line, col});
    this->store(std::move(ret));

    this->handle_indentation_(line + 1, 1);

    return this->tok_stream_.back();
}

Token Lexer::emit_unknown_(char_type c, size_type line, size_type col) {
    this->consume_char();
    Token ret(string_type(1, c), TokenType::UNKNOWN, {line, col});
    this->store(std::move(ret));
    return tok_stream_.back();
}

Token Lexer::next() {
    // Return cached token if already lexed ahead
    if (this->tok_index_ + 1 < this->tok_stream_.size())
    {
        this->tok_index_++;
        return this->tok_stream_[this->tok_index_];
    }

    // First token: emit SOF
    if (this->tok_stream_.empty())
    {
        return this->emit_sof_();
    }

    while (true)
    {
        char_type ch = this->source_manager_.current();
        if (ch == BUF_END)
        {
            break;
        }

        size_type line = source_manager_.line();
        size_type col  = source_manager_.column();

        switch (ch)
        {
        case u'\n' : {
            return this->handle_newline_(ch, line, col);
        }

        case u' ' :
        case u'\t' :
        case u'\r' :
            this->consume_char();
            continue;  // skip whitespace

        case u'\\' : {
            char_type lookahead = this->source_manager_.peek();
            if (lookahead == ch)
            {
                this->consume_char();
                this->consume_char();  // consume the second '\'

                while (true)
                {
                    char_type c2 = this->source_manager_.peek();
                    if (c2 == u'\n' || c2 == BUF_END)
                    {
                        break;
                    }

                    this->consume_char();
                }

                continue;
            }
        }
        break;

        case u'\'' :
        case u'"' :
            return this->handle_string_literal_(ch, line, col);

        default :
            break;
        }

        // ---------- Identifiers ----------
        if (isalpha_arabic(ch) || ch == u'_')
        {
            return this->handle_identifier_(ch, line, col);
        }

        // ---------- Numbers ----------
        else if (std::iswdigit(ch))
        {
            return this->handle_number_(ch, line, col);
        }

        // ---------- Operators ----------
        else if (is_operator_char(ch))
        {
            return this->handle_operator_(ch, line, col);
        }

        // ---------- Symbols ----------
        else if (is_symbol_char(ch))
        {
            return this->handle_symbol_(ch, line, col);
        }

        // ---------- Unknown fallback ----------
        return this->emit_unknown_(ch, line, col);
    }

    // ---------- EOF ----------
    return this->emit_eof_();
}

std::vector<Token> Lexer::tokenize() {
    // next_token() will push tokens to the stream on it's own
    while (next().type() != TokenType::END_OF_FILE)
    {
        ;
    }

    return tok_stream_;
}

Token Lexer::peek() { return Token(u"", TokenType::END_OF_FILE, {1, 1}); }

Token Lexer::prev() {
    if (tok_index_ > 0)
    {
        --tok_index_;
    }
    else
    {
        return Token(u"", TokenType::END_OF_FILE, {source_manager_.line(), source_manager_.column()});
    }

    return tok_stream_[tok_index_];
}