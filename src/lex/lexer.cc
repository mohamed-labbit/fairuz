//
// lexer.cc
//

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
            tok::Token tok = make_token(tok::TokenType::DEDENT);
            stream.push_back(tok);
        }

        assert(indent == indents.top());
    };

    while (current != BUF_END)
    {
        if (current == u'\n')
        {
            sm.consume_char();
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
                    sm.consume_char();
                    current = sm.current();
                }

                if (indent > indents.top())
                {
                    indents.push(indent);
                    stream.push_back(make_token(tok::TokenType::INDENT));
                }
                else if (indent == indents.top())
                {
                    stream.push_back(make_token(tok::TokenType::NEWLINE));
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
    auto& sm     = this->source_manager_;
    auto& stream = this->tok_stream_;

    string_type id(1, c);
    sm.consume_char();

    char_type c2 = sm.current();
    while (util::isalpha_arabic(c2) || c2 == u'_' || std::iswdigit(c2))
    {
        id.push_back(c2);
        sm.consume_char();
        c2 = sm.current();
    }

    tok::TokenType tt = tok::TokenType::IDENTIFIER;
    if (tok::keywords.count(id))
    {
        tt = tok::keywords.at(id);
    }

    // NOTE : leave symbol table stuff to the parser

    tok::Token tok = make_token(tt, std::move(id), line, col);
    store(std::move(tok));
    return stream.back();
}

tok::Token Lexer::handle_number_(char_type c, size_type line, size_type col)
{
    auto& sm     = this->source_manager_;
    auto& stream = this->tok_stream_;

    string_type num(1, c);
    sm.consume_char();

    char_type c2 = sm.current();
    while (std::iswdigit(c2))
    {
        num.push_back(c2);
        sm.consume_char();
        c2 = sm.current();
    }

    tok::Token ret = make_token(tok::TokenType::NUMBER, std::move(num), line, col);
    store(std::move(ret));
    return stream.back();
}

tok::Token Lexer::handle_operator_(char_type c, size_type line, size_type col)
{
    auto& sm     = this->source_manager_;
    auto& stream = this->tok_stream_;

    string_type op(1, c);
    sm.consume_char();

    char_type nxt = sm.current();
    if (nxt != BUF_END)
    {
        string_type two = op;
        two.push_back(nxt);

        if (tok::operators.count(two))
        {
            op.push_back(nxt);
            sm.consume_char();
        }
    }

    tok::TokenType tt  = tok::operators.count(op) ? tok::operators.at(op) : tok::TokenType::UNKNOWN;
    tok::Token     ret = make_token(tt, std::move(op), line, col);
    store(std::move(ret));
    return stream.back();
}

tok::Token Lexer::handle_symbol_(char_type c, size_type line, size_type col)
{
    auto& sm     = this->source_manager_;
    auto& stream = this->tok_stream_;

    tok::TokenType tt;
    string_type    sym(1, c);
    sm.consume_char();

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
        if (sm.peek() == u'=')
        {
            sm.consume_char();
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

    tok::Token ret = make_token(tt, std::move(sym), line, col);
    store(std::move(ret));
    return stream.back();
}

tok::Token Lexer::handle_string_literal_(char_type c, size_type line, size_type col)
{
    auto& sm     = this->source_manager_;
    auto& stream = this->tok_stream_;

    string_type s;
    char_type   quote = c;
    sm.consume_char();  // consume opening quote

    char_type c2 = sm.current();
    while (c2 != u'\n' && c2 != BUF_END && c2 != quote)
    {
        s.push_back(c2);
        sm.consume_char();
        c2 = sm.current();
    }

    // now current() is at the closing quote
    if (c2 == quote)
    {
        sm.consume_char();  // consume closing quote
    }
    else
    {
        // Unterminated string literal
        return make_token(tok::TokenType::UNKNOWN, std::move(s), line, col);
    }

    tok::Token ret = make_token(tok::TokenType::STRING, std::move(s), line, col);
    store(std::move(ret));
    return stream.back();
}

tok::Token Lexer::emit_eof_()
{
    auto& sm     = this->source_manager_;
    auto& stream = this->tok_stream_;

    sm.consume_char();
    if (!stream.empty() && stream.back().type() == tok::TokenType::END_OF_FILE)
    {
        return stream.back();
    }

    tok::Token ret = make_token(tok::TokenType::END_OF_FILE, std::nullopt, std::nullopt, sm.column() - 1);
    store(std::move(ret));
    return stream.back();
}

tok::Token Lexer::emit_sof_()
{
    auto& stream = this->tok_stream_;

    tok::Token ret = make_token(tok::TokenType::START_OF_FILE);
    store(std::move(ret));
    return stream.back();
}

tok::Token Lexer::handle_newline_(char_type c, size_t line, size_t col)
{
    auto& sm     = this->source_manager_;
    auto& stream = this->tok_stream_;

    sm.consume_char();
    string_type endl = u"\n";
    tok::Token  ret  = make_token(tok::TokenType::NEWLINE, std::move(endl), line, col);
    store(std::move(ret));
    handle_indentation_(line + 1, 1);
    return stream.back();
}

tok::Token Lexer::emit_unknown_(char_type c, size_type line, size_type col)
{
    auto& sm     = this->source_manager_;
    auto& stream = this->tok_stream_;

    sm.consume_char();
    tok::Token ret = make_token(tok::TokenType::UNKNOWN, string_type(1, c), line, col);
    store(std::move(ret));
    return stream.back();
}

std::vector<tok::Token> Lexer::tokenize()
{
    // next_token() will push tokens to the stream on it's own
    while (next().type() != tok::TokenType::END_OF_FILE)
    {
        ;
    }

    return this->tok_stream_;
}

tok::Token Lexer::peek(size_type n)
{
    while (tok_index_ + n >= tok_stream_.size())
    {
        lex_token_();
    }

    return tok_stream_[tok_index_ + n];
}

void Lexer::lex_token_()
{
    auto& stream       = this->tok_stream_;
    auto& stream_index = this->tok_index_;
    auto& sm           = this->source_manager_;

    // First token: emit SOF
    if (stream.empty())
    {
        emit_sof_();
        return;
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
            handle_newline_(ch, line, col);
            return;
        }

        case u' ' :
        case u'\t' :
        case u'\r' :
            sm.consume_char();
            continue;  // skip whitespace

        case u'\\' : {
            char_type lookahead = sm.peek();
            if (lookahead == ch)
            {
                sm.consume_char();
                sm.consume_char();  // consume the second '\'

                while (true)
                {
                    char_type c2 = sm.peek();
                    if (c2 == u'\n' || c2 == BUF_END)
                    {
                        break;
                    }

                    sm.consume_char();
                }

                continue;
            }
        }
        break;

        case u'\'' :
        case u'"' :
            handle_string_literal_(ch, line, col);
            return;

        default :
            break;
        }

        // ---------- Identifiers ----------
        if (util::isalpha_arabic(ch) || ch == u'_')
        {
            handle_identifier_(ch, line, col);
            return;
        }

        // ---------- Numbers ----------
        else if (std::iswdigit(ch))
        {
            handle_number_(ch, line, col);
            return;
        }

        // ---------- Operators ----------
        else if (util::is_operator_char(ch))
        {
            handle_operator_(ch, line, col);
            return;
        }

        // ---------- Symbols ----------
        else if (util::is_symbol_char(ch))
        {
            handle_symbol_(ch, line, col);
            return;
        }

        // ---------- Unknown fallback ----------
        emit_unknown_(ch, line, col);
        return;
    }

    // ---------- EOF ----------
    emit_eof_();
}

tok::Token Lexer::next()
{
    auto& index  = this->tok_index_;
    auto& stream = this->tok_stream_;

    if (index < stream.size())
    {
        auto copy = index;
        index += 1;
        return stream[copy];
    }

    lex_token_();
    return stream[index];
}

tok::Token Lexer::prev()
{
    auto& stream       = this->tok_stream_;
    auto  stream_index = this->tok_index_;

    if (stream_index > 0)
    {
        stream_index -= 1;
    }
    else
    {
        return make_token(tok::TokenType::END_OF_FILE);
    }

    return stream[stream_index];
}


}  // lex
}  // mylang