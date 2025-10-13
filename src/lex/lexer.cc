#include "lex/lexer.h"
#include "lex/token.h"
#include "lex/util.h"
#include "macros.h"

#include <algorithm>
#include <cassert>
#include <list>
#include <memory>
#include <string>


namespace mylang {
namespace lex {


tok::Token Lexer::make_token(tok::TokenType             tt,
                             std::optional<string_type> lexeme,
                             std::optional<size_type>   line,
                             std::optional<size_type>   col,
                             std::optional<size_type>   file_pos,
                             std::optional<std::string> file_path) const
{
    return tok::Token(lexeme.value_or(u""), tt,
                      {line.value_or(this->source_manager_.line()), col.value_or(this->source_manager_.column()),
                       file_pos.value_or(this->source_manager_.fpos())},
                      file_path.value_or(this->source_manager_.fpath()));
}

void Lexer::handle_indentation_(size_type line, size_type col)
{
    auto& indents = this->indent_stack_;
    auto& stream  = this->tok_stream_;
    auto& sm      = this->source_manager_;
    auto  current = sm.current();

    auto dedent = [this](unsigned indent, std::stack<unsigned>& indents, std::vector<tok::Token>& stream) {
        while (indent < indents.top())
        {
            indents.pop();
            tok::Token tok = this->make_token(tok::TokenType::DEDENT);
            stream.push_back(tok);
        }
        // optional consistency check:
        assert(indent == indents.top());
    };

    while (current != BUF_END)
    {
        if (current == u'\n')
        {
            this->consume_char();
            current = sm.current();

            if (current == BUF_END)
            {
                dedent(0, indents, stream);
                break;
            }
            else if (current == ' ')  // tabs not supported yet
            {
                unsigned indent = 0;
                while (current == ' ')
                {
                    indent += 1;
                    this->consume_char();
                    current = sm.current();
                }

                if (indent > indents.top())
                {
                    indents.push(indent);
                    stream.push_back(this->make_token(tok::TokenType::INDENT));
                }
                else if (indent == indents.top())
                {
                    stream.push_back(this->make_token(tok::TokenType::NEWLINE));
                }
                else
                {
                    dedent(indent, indents, stream);
                }
            }
        }
        else
        {
            break;
        }
    }

    // optional safety check
    assert(indents.size() >= 1);
}

tok::Token Lexer::handle_identifier_(char_type c, size_type line, size_type col)
{
    string_type id(1, c);
    this->consume_char();

    char_type c2 = this->source_manager_.current();
    while (util::isalpha_arabic(c2) || c2 == u'_' || std::iswdigit(c2))
    {
        id.push_back(c2);
        this->consume_char();
        c2 = this->source_manager_.current();
    }

    tok::TokenType tt = tok::TokenType::IDENTIFIER;
    if (tok::keywords.count(id))
    {
        tt = tok::keywords.at(id);
    }

    if (tt == tok::TokenType::IDENTIFIER) 
    {
        this->symbol_table_.insert(
          SymbolTableEntry(id,
                           tok::Token::Location(this->source_manager_.fpath(),
                                                {this->source_manager_.line(), this->source_manager_.column(),
                                                 this->source_manager_.fpos()}),
                           SymbolType::VARIABLE, ScopeType::GLOBAL));
    }

    tok::Token tok = this->make_token(tt, std::move(id), line, col);
    this->store(std::move(tok));
    return this->tok_stream_.back();
}

tok::Token Lexer::handle_number_(char_type c, size_type line, size_type col)
{
    string_type num(1, c);
    this->consume_char();

    char_type c2 = this->source_manager_.current();
    while (std::iswdigit(c2))
    {
        num.push_back(c2);
        this->consume_char();
        c2 = this->source_manager_.current();
    }

    tok::Token ret = this->make_token(tok::TokenType::NUMBER, std::move(num), line, col);
    store(std::move(ret));
    return this->tok_stream_.back();
}

tok::Token Lexer::handle_operator_(char_type c, size_type line, size_type col)
{
    string_type op(1, c);
    this->consume_char();

    char_type lookahead = this->source_manager_.peek();
    if (lookahead != BUF_END)
    {
        string_type two = op;
        two.push_back(lookahead);

        if (tok::operators.count(two))
        {
            op.push_back(this->consume_char());
        }
    }

    tok::TokenType tt  = tok::operators.count(op) ? tok::operators.at(op) : tok::TokenType::UNKNOWN;
    tok::Token     ret = this->make_token(tt, std::move(op), line, col);
    this->store(std::move(ret));
    return this->tok_stream_.back();
}

tok::Token Lexer::handle_symbol_(char_type c, size_type line, size_type col)
{
    tok::TokenType tt;
    string_type    sym(1, c);
    this->consume_char();

    switch (c)
    {
    case '(' :
        tt = tok::TokenType::LPAREN;
        break;
    case ')' :
        tt = tok::TokenType::RPAREN;
        break;
    case '.' :
        tt = tok::TokenType::DOT;
        break;
    case ':' :
        if (this->source_manager_.peek() == u'=')
        {
            this->consume_char();
            sym = u":=";
            tt  = tok::TokenType::ASSIGN;
        }
        else
        {
            tt = tok::TokenType::COLON;
        }
        break;
    default :
        tt = tok::TokenType::UNKNOWN;
        break;
    }

    tok::Token ret = this->make_token(tt, std::move(sym), line, col);
    return this->tok_stream_.back();
}

tok::Token Lexer::handle_string_literal_(char_type c, size_type line, size_type col)
{
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
        return this->make_token(tok::TokenType::UNKNOWN, std::move(s), line, col);
    }

    tok::Token ret = this->make_token(tok::TokenType::STRING, std::move(s), line, col);
    this->store(std::move(ret));
    return this->tok_stream_.back();
}

tok::Token Lexer::emit_eof_()
{
    this->consume_char();
    if (!tok_stream_.empty() && tok_stream_.back().type() == tok::TokenType::END_OF_FILE)
    {
        return tok_stream_.back();
    }

    tok::Token ret =
      this->make_token(tok::TokenType::END_OF_FILE, std::nullopt, std::nullopt, this->source_manager_.column() - 1);
    store(std::move(ret));
    return tok_stream_.back();
}

tok::Token Lexer::emit_sof_()
{
    tok::Token ret = this->make_token(tok::TokenType::START_OF_FILE);
    this->store(std::move(ret));
    return this->tok_stream_.back();
}

tok::Token Lexer::handle_newline_(char_type c, size_t line, size_t col)
{
    this->consume_char();
    string_type endl = u"\n";
    tok::Token  ret  = this->make_token(tok::TokenType::NEWLINE, std::move(endl), line, col);
    this->store(std::move(ret));
    this->handle_indentation_(line + 1, 1);
    return this->tok_stream_.back();
}

tok::Token Lexer::emit_unknown_(char_type c, size_type line, size_type col)
{
    this->consume_char();
    tok::Token ret = this->make_token(tok::TokenType::UNKNOWN, string_type(1, c), line, col);
    this->store(std::move(ret));
    return tok_stream_.back();
}

tok::Token Lexer::next()
{
    auto& stream       = this->tok_stream_;
    auto& stream_index = this->tok_index_;
    auto& sm           = this->source_manager_;

    // Return cached token if already lexed ahead
    if (stream_index + 1 < stream.size())
    {
        stream_index += 1;
        return stream[stream_index];
    }

    // First token: emit SOF
    if (stream.empty())
    {
        return this->emit_sof_();
    }

    while (true)
    {
        char_type ch = sm.current();
        if (ch == BUF_END)
        {
            break;
        }

        size_type line = sm.line();
        size_type col  = sm.column();

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
            char_type lookahead = sm.peek();
            if (lookahead == ch)
            {
                this->consume_char();
                this->consume_char();  // consume the second '\'

                while (true)
                {
                    char_type c2 = sm.peek();
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
        if (util::isalpha_arabic(ch) || ch == u'_')
        {
            return this->handle_identifier_(ch, line, col);
        }

        // ---------- Numbers ----------
        else if (std::iswdigit(ch))
        {
            return this->handle_number_(ch, line, col);
        }

        // ---------- Operators ----------
        else if (util::is_operator_char(ch))
        {
            return this->handle_operator_(ch, line, col);
        }

        // ---------- Symbols ----------
        else if (util::is_symbol_char(ch))
        {
            return this->handle_symbol_(ch, line, col);
        }

        // ---------- Unknown fallback ----------
        return this->emit_unknown_(ch, line, col);
    }

    // ---------- EOF ----------
    return this->emit_eof_();
}

std::vector<tok::Token> Lexer::tokenize()
{
    // next_token() will push tokens to the stream on it's own
    while (next().type() != tok::TokenType::END_OF_FILE)
    {
        ;
    }

    return tok_stream_;
}

tok::Token Lexer::peek() { return this->make_token(tok::TokenType::END_OF_FILE); }

tok::Token Lexer::prev()
{
    if (tok_index_ > 0)
    {
        --tok_index_;
    }
    else
    {
        return this->make_token(tok::TokenType::END_OF_FILE);
    }

    return tok_stream_[tok_index_];
}


}  // lex
}  // mylang
