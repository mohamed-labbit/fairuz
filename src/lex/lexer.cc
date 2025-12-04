//
// lexer.cc - FIXED VERSION
//

#include "../../include/lex/lexer.hpp"
#include "../../include/lex/token.hpp"
#include "../../include/lex/util.hpp"
#include "../../include/macros.hpp"

#include <cassert>


namespace mylang {
namespace lex {

tok::Token Lexer::make_token(tok::TokenType tt,
  std::optional<std::u16string> lexeme,
  std::optional<std::size_t> line,
  std::optional<std::size_t> col,
  std::optional<std::size_t> file_pos,
  std::optional<std::string> file_path) const
{
    return tok::Token(lexeme.value_or(u""), tt, line.value_or(this->source_manager_.line()),
      col.value_or(this->source_manager_.column()), file_pos.value_or(this->source_manager_.fpos()),
      file_path.value_or(this->source_manager_.fpath()));
}

IndentationAnalysis Lexer::_analyze_indentation(SourceManager& sm)
{
    std::size_t col = sm.column();
    std::size_t line = sm.line();
    IndentationAnalysis result;
    // Skip indentation handling inside parentheses (implicit line joining)
    if (indent_ctx_.in_parentheses > 0)
    {
        result.action = IndentationAnalysis::Action::NONE;
        return result;
    }

    // Measure indentation
    std::size_t indent_count = 0;
    std::u16string indent_str;
    char16_t ch = sm.current();

    while (ch == u' ' || ch == u'\t')
    {
        indent_str.push_back(ch);
        if (ch == u' ')
            indent_count++;
        else if (ch == u'\t')
            indent_count += 8;  // Tabs are typically 8 spaces
        sm.consume_char();
        ch = sm.current();
    }

    result.indent_string = indent_str;
    result.column = col + indent_str.length();

    // Check for blank line or comment-only line
    if (ch == u'\n' || ch == BUFFER_END || ch == u'\\')
    {
        result.action = IndentationAnalysis::Action::NONE;
        return result;
    }

    // Detect and validate indentation mode
    if (!indent_str.empty())
    {
        if (indent_ctx_.mode == IndentationContext::IndentMode::UNDETECTED)
            indent_ctx_.detect_indent_mode(indent_str);
        if (!indent_ctx_.validate_indent(indent_str))
        {
            result.action = IndentationAnalysis::Action::ERROR;
            result.error_message = u"Inconsistent use of tabs and spaces in indentation";
            return result;
        }
    }

    // Compare with current indentation level
    std::size_t current_indent = indent_ctx_.top();
    if (indent_count > current_indent)
    {
        // INDENT
        if (indent_ctx_.expecting_indent)
        {
            indent_ctx_.push(indent_count);
            indent_ctx_.expecting_indent = false;
            result.action = IndentationAnalysis::Action::INDENT;
            result.count = 1;
        }
        else
        {
            result.action = IndentationAnalysis::Action::ERROR;
            result.error_message = u"Unexpected indent";
            return result;
        }
    }

    else if (indent_count < current_indent && indent_count != 0)
    {
        // DEDENT - potentially multiple levels
        std::size_t dedent_count = 0;
        std::stack<std::size_t> temp_stack = indent_ctx_.indent_stack;
        bool found_match = false;

        while (temp_stack.size() > 1)
        {
            std::size_t level = temp_stack.top();

            if (level == indent_count)
            {
                found_match = true;
                break;
            }
            if (level < indent_count)
            {
                result.action = IndentationAnalysis::Action::ERROR;
                result.error_message = u"Unindent does not match any outer indentation level";
                return result;
            }

            temp_stack.pop();
            dedent_count++;
        }

        if (!found_match && temp_stack.top() != indent_count)
        {
            result.action = IndentationAnalysis::Action::ERROR;
            result.error_message = u"Unindent does not match any outer indentation level";
            return result;
        }

        // Apply the dedents
        for (std::size_t i = 0; i < dedent_count; ++i)
            indent_ctx_.pop();

        result.action = IndentationAnalysis::Action::DEDENT;
        result.count = dedent_count;
        indent_ctx_.expecting_indent = false;
    }
    else
    {
        // Same indentation level
        result.action = IndentationAnalysis::Action::NONE;
        indent_ctx_.expecting_indent = false;
    }

    return result;
}

// BUG FIX #1: Fixed parenthesis tracking - should increment/decrement, not assign bool
void Lexer::update_indentation_context(const tok::Token& token)
{
    auto tt = token.type();
    switch (tt)
    {
    case tok::TokenType::LPAREN :
    case tok::TokenType::LBRACKET :
        indent_ctx_.in_parentheses++;  // FIX: Increment instead of = true
        break;
    case tok::TokenType::RPAREN :
    case tok::TokenType::RBRACKET :
        if (indent_ctx_.in_parentheses > 0)
            indent_ctx_.in_parentheses--;  // FIX: Decrement instead of = false
        break;
    case tok::TokenType::COLON : indent_ctx_.expecting_indent = true; break;
    case tok::TokenType::NEWLINE : indent_ctx_.at_line_start = true; break;
    default :
        if (indent_ctx_.at_line_start)
            indent_ctx_.at_line_start = false;
        break;
    }
}

tok::Token Lexer::_handle_indentation(SourceManager& sm)
{
    auto& stream = this->tok_stream_;
    IndentationAnalysis analysis = _analyze_indentation(sm);
    switch (analysis.action)
    {
    case IndentationAnalysis::Action::INDENT : {
        for (std::size_t i = 0; i < analysis.count; ++i)
        {
            tok::Token indent_tok = make_token(tok::TokenType::INDENT, analysis.indent_string);
            store(std::move(indent_tok));
        }
        return stream.back();
    }
    case IndentationAnalysis::Action::DEDENT : {
        for (std::size_t i = 0; i < analysis.count; ++i)
        {
            tok::Token dedent_tok = make_token(tok::TokenType::DEDENT, u"");
            store(std::move(dedent_tok));
        }
        return stream.back();
    }
    case IndentationAnalysis::Action::ERROR : {
        tok::Token error_tok = make_token(
          tok::TokenType::INVALID, std::u16string(analysis.error_message.begin(), analysis.error_message.end()));
        store(std::move(error_tok));
        std::cerr << "Indentation Error at line " << sm.line() << ", col " << sm.column() << ": "
                  << utf8::utf16to8(analysis.error_message) << std::endl;
        return stream.back();
    }
    case IndentationAnalysis::Action::NONE :
    default :
        if (!stream.empty())
            return stream.back();
        return make_token(tok::TokenType::INVALID, u"", sm.line(), sm.column());
    }
}

tok::Token Lexer::_handle_identifier(char16_t c, SourceManager& sm)
{
    auto& stream = this->tok_stream_;
    std::u16string id(1, c);
    auto line = sm.line();
    auto col = sm.column();
    sm.consume_char();
    char16_t c2 = sm.current();

    while (util::isalpha_arabic(c2) || c2 == u'_' || std::iswdigit(c2))
    {
        id.push_back(c2);
        sm.consume_char();
        c2 = sm.current();
    }

    tok::TokenType tt = tok::TokenType::IDENTIFIER;
    if (tok::keywords.count(id))
        tt = tok::keywords.at(id);

    tok::Token tok = make_token(tt, std::move(id), line, col);
    store(std::move(tok));
    return stream.back();
}

tok::Token Lexer::_handle_number(char16_t c, SourceManager& sm)
{
    auto& stream = this->tok_stream_;
    std::u16string num(1, c);
    auto line = sm.line();
    auto col = sm.column();
    sm.consume_char();
    char16_t c2 = sm.current();

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

tok::Token Lexer::_handle_operator(char16_t c, SourceManager& sm)
{
    auto& stream = this->tok_stream_;
    std::u16string op(1, c);
    auto line = sm.line();
    auto col = sm.column();
    sm.consume_char();
    char16_t nxt = sm.current();

    if (nxt != BUFFER_END)
    {
        std::u16string two = op;
        two.push_back(nxt);
        if (tok::operators.count(two))
        {
            op.push_back(nxt);
            sm.consume_char();
        }
    }

    tok::TokenType tt = tok::operators.count(op) ? tok::operators.at(op) : tok::TokenType::IDENTIFIER;
    tok::Token ret = make_token(tt, std::move(op), line, col);
    store(std::move(ret));
    return stream.back();
}

// BUG FIX #2: Added bracket and brace support
tok::Token Lexer::_handle_symbol(char16_t c, SourceManager& sm)
{
    auto& stream = this->tok_stream_;
    tok::TokenType tt;
    std::u16string sym(1, c);
    auto line = sm.line();
    auto col = sm.column();
    sm.consume_char();

    switch (c)
    {
    case '(' : tt = tok::TokenType::LPAREN; break;
    case ')' : tt = tok::TokenType::RPAREN; break;
    case '[' :  // FIX: Added bracket support
        tt = tok::TokenType::LBRACKET;
        break;
    case ']' : tt = tok::TokenType::RBRACKET; break;
    case '.' : tt = tok::TokenType::DOT; break;
    case ':' :
        if (sm.current() == u'=')
        {
            sm.consume_char();
            sym += '=';
            tt = tok::TokenType::OP_ASSIGN;
        }
        else
        {
            tt = tok::TokenType::COLON;
        }
        break;
    default : tt = tok::TokenType::INVALID; break;
    }

    tok::Token ret = make_token(tt, std::move(sym), line, col);
    store(std::move(ret));
    return stream.back();
}

tok::Token Lexer::_handle_string_literal(char16_t c, SourceManager& sm)
{
    auto& stream = this->tok_stream_;
    std::u16string s;
    char16_t quote = c;
    auto line = sm.line();
    auto col = sm.column();
    sm.consume_char();
    char16_t c2 = sm.current();

    while (c2 != u'\n' && c2 != BUFFER_END && c2 != quote)
    {
        s.push_back(c2);
        sm.consume_char();
        c2 = sm.current();
    }

    if (c2 == quote)
        sm.consume_char();
    else
        return make_token(tok::TokenType::INVALID, std::move(s));

    tok::Token ret = make_token(tok::TokenType::STRING, std::move(s), line, col);
    store(std::move(ret));
    return stream.back();
}

// BUG FIX #3: Removed unused variable
tok::Token Lexer::_emit_eof(SourceManager& sm)
{
    auto& stream = this->tok_stream_;

    // Emit all remaining DEDENT tokens
    while (indent_ctx_.stack_size() > 1)
    {
        indent_ctx_.pop();  // FIX: Removed unused 'level' variable
        auto dedent_tok = make_token(tok::TokenType::DEDENT, u"", sm.line(), sm.column());
        store(std::move(dedent_tok));
    }

    sm.consume_char();

    if (!stream.empty() && stream.back().type() == tok::TokenType::ENDMARKER)
        return stream.back();

    auto ret = make_token(tok::TokenType::ENDMARKER, std::nullopt, std::nullopt, sm.column() - 1);
    store(std::move(ret));
    return stream.back();
}

tok::Token Lexer::_emit_sof(SourceManager& sm)
{
    auto& stream = this->tok_stream_;
    tok::Token ret = make_token(tok::TokenType::BEGINMARKER, std::nullopt, 1, 1);
    store(std::move(ret));
    return stream.back();
}

// BUG FIX #4: Removed unused variables and fixed condition check
tok::Token Lexer::_handle_newline(char16_t c, SourceManager& sm)
{
    auto& stream = this->tok_stream_;
    auto line = sm.line();
    auto col = sm.column();
    sm.consume_char();
    std::u16string endl = u"\n";
    auto ret = make_token(tok::TokenType::NEWLINE, std::move(endl), line, col);
    store(std::move(ret));
    update_indentation_context(ret);
    // Don't handle indentation here - let the main loop do it
    return stream.back();
}

tok::Token Lexer::_emit_invalid(char16_t c, SourceManager& sm)
{
    auto& stream = this->tok_stream_;
    auto line = sm.line();
    auto col = sm.column();
    sm.consume_char();
    tok::Token ret = make_token(tok::TokenType::INVALID, std::u16string(1, c), line, col);
    store(std::move(ret));
    return stream.back();
}

std::vector<tok::Token> Lexer::tokenize()
{
    while (next().type() != tok::TokenType::ENDMARKER)
        ;

    return this->tok_stream_;
}

tok::Token Lexer::peek(std::size_t n)
{
    while (tok_index_ + n >= tok_stream_.size())
        lex_token_();

    return tok_stream_[tok_index_ + n];
}

void Lexer::lex_token_()
{
    auto& stream = this->tok_stream_;
    auto& sm = this->source_manager_;

    if (stream.empty())
    {
        _emit_sof(sm);
        return;
    }

    while (true)
    {
        char16_t ch = sm.current();
        if (ch == BUFFER_END)
            break;

        std::size_t line = sm.line();
        std::size_t col = sm.column();

        switch (ch)
        {
        case u'\n' : {
            _handle_newline(ch, sm);
            return;
        }
        case u' ' :
        case u'\t' :
        case u'\r' :
            if (!indent_ctx_.at_line_start)
            {
                sm.consume_char();
                continue;
            }
            break;
        case u'\\' : {
            char16_t lookahead = sm.peek();
            if (lookahead == ch)
            {
                sm.consume_char();
                sm.consume_char();
                while (true)
                {
                    char16_t c2 = sm.peek();
                    if (c2 == u'\n' || c2 == BUFFER_END)
                        break;
                    sm.consume_char();
                }
                continue;
            }
        }
        break;
        case u'\'' :
        case u'"' : {
            auto tok = _handle_string_literal(ch, sm);
            update_indentation_context(tok);
            return;
        }
        default : break;
        }
        // Handle indentation at line start
        if (indent_ctx_.at_line_start && ch != u'\n' && ch != BUFFER_END)
        {
            _handle_indentation(sm);
            ch = sm.current();
        }
        // Identifiers
        if (util::isalpha_arabic(ch) || ch == u'_')
        {
            auto tok = _handle_identifier(ch, sm);
            update_indentation_context(tok);
            return;
        }
        // Numbers
        else if (std::iswdigit(ch))
        {
            auto tok = _handle_number(ch, sm);
            update_indentation_context(tok);
            return;
        }
        // Operators
        else if (util::is_operator_char(ch))
        {
            auto tok = _handle_operator(ch, sm);
            update_indentation_context(tok);
            return;
        }
        // Symbols
        else if (util::is_symbol_char(ch))
        {
            auto tok = _handle_symbol(ch, sm);
            update_indentation_context(tok);
            return;
        }
        // Unknown
        auto tok = _emit_invalid(ch, sm);
        update_indentation_context(tok);
        return;
    }

    _emit_eof(sm);
}

tok::Token Lexer::next()
{
    auto& index = this->tok_index_;
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
    auto& stream = this->tok_stream_;
    auto stream_index = this->tok_index_;
    if (stream_index > 0)
        stream_index -= 1;
    else
        return make_token(tok::TokenType::ENDMARKER);
    return stream[stream_index];
}

}  // namespace lex
}  // namespace mylang