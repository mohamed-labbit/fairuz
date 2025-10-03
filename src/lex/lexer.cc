#include "lex/lexer.h"
#include "lex/token.h"
#include "lex/util.h"
#include "macros.h"

#include <algorithm>
#include <memory>
#include <string>


void Lexer::handle_indentation(size_type line, size_type col) {
    char_type lookahead = this->source_manager_.peek();

    // only care if next char is space or tab
    if (lookahead != L' ' && lookahead != L'\t')
    {
        return;
    }

    size_type space_count = 0;
    bool      used_tab    = false;
    bool      used_space  = false;

    // count indentation width
    while (true)
    {
        lookahead = this->source_manager_.peek();
        if (lookahead != L' ' && lookahead != L'\t')
        {
            break;
        }

        char_type ws = this->consume_char();

        if (ws == L' ')
        {
            space_count += 1;
            used_space = true;
        }
        else if (ws == L'\t')
        {
            // tab expands to next multiple of TABWIDTH
            space_count += TABWIDTH - (space_count % TABWIDTH);
            used_tab = true;
        }
    }

    // Blank line? -> ignore indentation
    lookahead = this->source_manager_.peek();
    if (lookahead == L'\n' || lookahead == BUF_END)
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
    int       prev         = this->indent_stack_.empty() ? 0 : this->indent_stack_.back();

    if (indent_count > prev)
    {
        // new indent → push level and emit INDENT tokens
        this->indent_stack_.push_back(indent_count);

        for (size_type i = 0, n = indent_count - prev; i < n; i++)
        {
            string_type s = L"";
            Token       tok(std::move(s), TokenType::INDENT, {line, col});
            this->tok_stream_.push_back(std::move(tok));
            col += this->indent_size_;
        }
    }
    else if (indent_count < prev)
    {
        // dedent → pop until we match a previous level
        while (!this->indent_stack_.empty() && indent_count < this->indent_stack_.back())
        {
            this->indent_stack_.pop_back();
            string_type s = L"";
            Token       tok(std::move(s), TokenType::DEDENT, {line, col});
            this->tok_stream_.push_back(std::move(tok));
        }

        // if indent_count does not match any previous level → error
        if (this->indent_stack_.empty() || indent_count != this->indent_stack_.back())
        {
            throw std::runtime_error("Indentation does not match any previous level");
        }
    }
}

Token Lexer::handle_identifier(char_type c, size_type line, size_type col) {
    string_type id(1, c);
    this->consume_char();

    while (true)
    {
        char_type c2 = this->source_manager_.peek();
        if (!(isalpha_arabic(c2) || c2 == L'_' || std::iswdigit(c2)))
        {
            break;
        }

        id.push_back(this->consume_char());
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

Token Lexer::handle_number(char_type c, size_type line, size_type col) {
    string_type num(1, c);
    this->consume_char();

    while (true)
    {
        char_type c2 = this->source_manager_.peek();
        if (!std::iswdigit(c2))
        {
            break;
        }
        num.push_back(this->consume_char());
    }

    Token ret(std::move(num), TokenType::NUMBER, {line, col});
    store(std::move(ret));
    return this->tok_stream_.back();
}

Token Lexer::handle_operator(char_type c, size_type line, size_type col) {
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

Token Lexer::handle_symbol(char_type c, size_type line, size_type col) {
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
        if (this->source_manager_.peek() == L'=')
        {
            this->consume_char();
            sym = L":=";
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

Token Lexer::handle_string_literal(char_type c, size_type line, size_type col) {
    string_type s;
    char_type   quote = c;
    this->consume_char();

    while (true)
    {
        char_type c2 = this->source_manager_.peek();
        if (c2 == L'\n' || c2 == BUF_END || c2 == quote)
        {
            break;
        }

        s.push_back(this->consume_char());
    }

    if (this->source_manager_.peek() == quote)
    {
        this->consume_char();  // consume closing quote
    }
    else
    {
        return Token(std::move(s), TokenType::UNKNOWN, {line, col});
    }

    Token ret(std::move(s), TokenType::STRING, {line, col});
    this->store(std::move(ret));
    return this->tok_stream_.back();
}

Token Lexer::emit_eof() {
    this->consume_char();
    if (!tok_stream_.empty() && tok_stream_.back().type() == TokenType::END_OF_FILE)
    {
        return tok_stream_.back();
    }

    Token ret(L"", TokenType::END_OF_FILE, {source_manager_.line(), source_manager_.column()});
    store(std::move(ret));
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
        this->tok_stream_.push_back(Token(L"", TokenType::START_OF_FILE, {1, 1}));
        this->tok_index_ = 0;
        return this->tok_stream_.back();
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
        case L'\n' : {
            Token ret(L"\n", TokenType::NEWLINE, {line, col});
            this->store(std::move(ret));
            this->consume_char();

            // indentation is processed immediately after newline
            this->handle_indentation(line + 1, 1);

            return this->tok_stream_.back();
        }

        case L' ' :
        case L'\t' :
        case L'\r' :
            continue;  // skip whitespace

        case L'\\' : {
            char_type lookahead = this->source_manager_.peek();
            if (lookahead == ch)
            {
                this->consume_char();
                this->consume_char();  // consume the second '\'

                while (true)
                {
                    char_type c2 = this->source_manager_.peek();
                    if (c2 == L'\n' || c2 == BUF_END)
                    {
                        break;
                    }

                    this->consume_char();
                }

                continue;
            }
        }
        break;

        case L'\'' :
        case L'"' : {
            return this->handle_string_literal(ch, line, col);
        }

        default :
            break;
        }

        // ---------- Identifiers ----------
        if (isalpha_arabic(ch) || ch == L'_')
        {
            return this->handle_identifier(ch, line, col);
        }

        // ---------- Numbers ----------
        else if (std::iswdigit(ch))
        {
            return this->handle_number(ch, line, col);
        }

        // ---------- Operators ----------
        else if (is_operator_char(ch))
        {
            return this->handle_operator(ch, line, col);
        }

        // ---------- Symbols ----------
        else if (is_symbol_char(ch))
        {
            return this->handle_symbol(ch, line, col);
        }

        // ---------- Unknown fallback ----------
        this->consume_char();
        Token ret(string_type(1, ch), TokenType::UNKNOWN, {line, col});
        this->store(std::move(ret));
        return tok_stream_.back();
    }

    // ---------- EOF ----------
    return this->emit_eof();
}

std::vector<Token> Lexer::tokenize() {
    // next_token() will push tokens to the stream on it's own
    while (next().type() != TokenType::END_OF_FILE)
    {
        ;
    }

    return tok_stream_;
}

Token Lexer::peek() { return Token(L"", TokenType::END_OF_FILE, {1, 1}); }

Token Lexer::prev() {
    if (tok_index_ > 0)
    {
        --tok_index_;
    }
    else
    {
        return Token(L"", TokenType::END_OF_FILE, {source_manager_.line(), source_manager_.column()});
    }

    return tok_stream_[tok_index_];
}