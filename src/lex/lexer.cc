//
// lexer.cc - FIXED VERSION
//

#include "../../include/lex/lexer.hpp"
#include "../../include/diag/diagnostic.hpp"
#include "../../include/lex/token.hpp"
#include "../../include/lex/util.hpp"
#include "../../include/macros.hpp"

#include <cassert>


namespace mylang {
namespace lex {

Lexer::Lexer(input::FileManager* file_manager) :
    SourceManager_(file_manager),
    TokIndex_(0),
    IndentSize_(0)
{
  IndentStack_.push_back(0);
  AltIndentStack_.push_back(0);
  configureLocale();
}

Lexer::Lexer(std::vector<tok::Token>& seq, const std::size_t s) :
    TokStream_(seq),
    TokIndex_(0),
    IndentSize_(0)
{
  configureLocale();
}

tok::Token Lexer::make_token(tok::TokenType             tt,
                             std::optional<StringType>  lexeme,
                             std::optional<std::size_t> line,
                             std::optional<std::size_t> col,
                             std::optional<std::size_t> file_pos,
                             std::optional<std::string> file_path) const
{
  return tok::Token(lexeme.value_or(u""), tt, line.value_or(this->SourceManager_.line()), col.value_or(this->SourceManager_.column()),
                    file_pos.value_or(this->SourceManager_.fpos()), file_path.value_or(this->SourceManager_.fpath()));
}

std::vector<tok::Token> Lexer::tokenize()
{

  while (next().type() != tok::TokenType::ENDMARKER)
    ;
  return this->TokStream_;
}

tok::Token Lexer::peek(std::size_t n)
{
  while (TokIndex_ + n >= TokStream_.size())
  {
    if (!TokStream_.empty() && TokStream_.back().type() == tok::TokenType::ENDMARKER)
      return TokStream_.back();
    lexToken();
  }
  return TokStream_[TokIndex_ + n];
}

tok::Token Lexer::lexToken()
{
  auto finish = [this](tok::TokenType tt, StringType str, std::size_t l, std::size_t c) {
    tok::Token ret = make_token(tt, std::move(str), l, c);
    store(std::move(ret));
    return TokStream_.back();
  };

  /*
  if (TokIndex_ < TokStream_.size() - 2)
  {
    TokIndex_++;
    return TokStream_.at(TokIndex_);
  }
  */

  if (TokStream_.empty())
  {
    tok::Token ret = make_token(tok::TokenType::BEGINMARKER, std::nullopt, 1, 1);
    store(std::move(ret));
    return TokStream_.back();
  }

  for (;;)
  {
    char16_t ch = SourceManager_.current();

    if (ch == BUFFER_END)
      break;

    std::size_t line = SourceManager_.line();
    std::size_t col  = SourceManager_.column();

    switch (ch)
    {
    case u'\n' : {
      SourceManager_.consumeChar();
      AtBOL_ = true;
      return finish(tok::TokenType::NEWLINE, u"\n", line, col);
    }

    case u' ' :
    case u'\t' :
    case u'\r' :
      if (AtBOL_)
      {
        unsigned size          = 0;
        unsigned alt_size      = 0;
        unsigned cont_line_col = 0;
        bool     blankline     = false;

        AtBOL_ = false;

        // Calculate indentation
        for (;;)
        {
          ch = SourceManager_.peek();
          if (ch == ' ')
          {
            size++, alt_size++;
            SourceManager_.consumeChar();
          }
          else if (ch == '\t')
          {
            size     = (size / IndentSize_ + 1) * IndentSize_;
            alt_size = (alt_size / TABWIDTH + 1) * TABWIDTH;
            SourceManager_.consumeChar();
          }
          else if (ch == '\f')
          {
            size = alt_size = 0;
            SourceManager_.consumeChar();
          }
          else if (ch == '\\')
          {
            cont_line_col = cont_line_col ? cont_line_col : size;
            SourceManager_.consumeChar();
            // Handle continuation line if needed
          }
          else if (ch == BUFFER_END)
          {
            diagnostic::engine.panic("Unexpected EOF");
          }
          else
          {
            break;
          }
        }
        // Check for blank lines (comments/whitespace only)
        if (ch == '#' || ch == '\n' || ch == '\r')
        {
          if (size == 0 && ch == '\n')
            blankline = false;          // Empty line
          else
            blankline = true;  // Comment or whitespace-only
        }
        // Process indentation changes
        if (!blankline)
        {
          size     = cont_line_col ? cont_line_col : size;
          alt_size = cont_line_col ? cont_line_col : alt_size;
          if (size == IndentStack_.back())
          {
            // No change - check consistency
            if (alt_size != AltIndentStack_.back())
              diagnostic::engine.panic("Inconsistent indentation");
          }
          else if (size > IndentStack_.back())
          {
            // Indent
            if (IndentLevel_ + 1 >= MAX_ALLOWED_INDENT)
              diagnostic::engine.panic("Too many indentation levels");
            if (alt_size <= AltIndentStack_.back())
              diagnostic::engine.panic("Inconsistent indentation (tabs/spaces)");
            IndentLevel_++;
            IndentStack_.push_back(size);
            AltIndentStack_.push_back(alt_size);
            return finish(tok::TokenType::INDENT, u"", line, col);
          }
          else
          {
            // Dedent - can be multiple levels
            int dedent_count = 0;
            while (IndentLevel_ > 0 && size < IndentStack_.back())
            {
              IndentLevel_--;
              IndentStack_.pop_back();
              AltIndentStack_.pop_back();
              dedent_count++;
            }
            if (size != IndentStack_.back())
              diagnostic::engine.panic("Unindent does not match any outer indentation level");
            if (alt_size != AltIndentStack_.back())
              diagnostic::engine.panic("Inconsistent indentation");
            // Store all DEDENT tokens
            for (int i = 0; i < dedent_count; i++)
              store(make_token(tok::TokenType::DEDENT, u"", line, col));
            return TokStream_[TokIndex_++];
          }
        }
      }
      else
      {
        // Not at beginning of line - just skip whitespace
        SourceManager_.consumeChar();
        continue;
      }
      // Should only reach here if blankline is true
      SourceManager_.consumeChar();
      continue;

    case u'\\' : {
      char16_t lookahead = SourceManager_.peek();
      if (lookahead == ch)
      {
        SourceManager_.consumeChar();
        SourceManager_.consumeChar();

        for (;;)
        {
          char16_t c2 = SourceManager_.peek();
          if (c2 == u'\n' || c2 == BUFFER_END)
            break;

          SourceManager_.consumeChar();
        }

        continue;
      }
    }
    break;
    case u'\'' :
    case u'"' : {
      StringType str;
      char16_t   quote = ch;
      SourceManager_.consumeChar();
      char16_t c2 = SourceManager_.current();

      while (c2 != u'\n' && c2 != BUFFER_END && c2 != quote)
      {
        str += c2;
        SourceManager_.consumeChar();
        c2 = SourceManager_.current();
      }

      if (c2 == quote)
        SourceManager_.consumeChar();
      else
        return make_token(tok::TokenType::INVALID, std::move(str));

      return finish(tok::TokenType::STRING, std::move(str), line, col);
    }
    case ',' :
    case u'،' :
      SourceManager_.consumeChar();
      return finish(tok::TokenType::COMMA, StringType(1, ch), line, col);
    default :
      break;
    }

    // Identifiers
    if (util::isalphaArabic(ch) || ch == u'_')
    {
      StringType id(1, ch);
      SourceManager_.consumeChar();
      char16_t c2 = SourceManager_.current();

      while (util::isalphaArabic(c2) || c2 == u'_' || std::iswdigit(c2))
      {
        id += c2;
        SourceManager_.consumeChar();
        c2 = SourceManager_.current();
      }

      tok::TokenType tt = tok::TokenType::IDENTIFIER;
      if (tok::keywords.count(id))
        tt = tok::keywords.at(id);

      return finish(tt, std::move(id), line, col);
    }

    // Numbers
    else if (std::iswdigit(ch))
    {
      StringType num(1, ch);
      SourceManager_.consumeChar();
      char16_t c2 = SourceManager_.current();

      while (std::iswdigit(c2))
      {
        num += c2;
        SourceManager_.consumeChar();
        c2 = SourceManager_.current();
      }

      if (c2 == '.')
      {
        num += c2;
        SourceManager_.consumeChar();
        char16_t c2 = SourceManager_.current();

        while (std::iswdigit(c2))
        {
          num += c2;
          SourceManager_.consumeChar();
          c2 = SourceManager_.current();
        }

        return finish(tok::TokenType::FLOAT, std::move(num), line, col);
      }

      return finish(tok::TokenType::NUMBER, std::move(num), line, col);
    }

    // Operators
    else if (util::isOperator(ch))
    {
      StringType op(1, ch);
      SourceManager_.consumeChar();
      char16_t nxt = SourceManager_.current();

      if (nxt != BUFFER_END)
      {
        StringType two = op;
        two += nxt;

        if (tok::operators.count(two))
        {
          op += nxt;
          SourceManager_.consumeChar();
        }
      }

      tok::TokenType tt = tok::operators.count(op) ? tok::operators.at(op) : tok::TokenType::IDENTIFIER;
      return finish(tt, std::move(op), line, col);
    }
    // Symbols
    else if (util::isSymbol(ch))
    {
      tok::TokenType tt;
      StringType     sym(1, ch);
      SourceManager_.consumeChar();

      switch (ch)
      {
      case '(' :
        tt = tok::TokenType::LPAREN;
        break;
      case ')' :
        tt = tok::TokenType::RPAREN;
        break;
      case '[' :  // FIX: Added bracket support
        tt = tok::TokenType::LBRACKET;
        break;
      case ']' :
        tt = tok::TokenType::RBRACKET;
        break;
      case '.' :
        tt = tok::TokenType::DOT;
        break;
      case ':' :
        if (SourceManager_.current() == u'=')
        {
          SourceManager_.consumeChar();
          sym += '=';
          tt = tok::TokenType::OP_ASSIGN;
        }
        else
        {
          tt = tok::TokenType::COLON;
        }
        break;
      default :
        tt = tok::TokenType::INVALID;
        break;
      }

      return finish(tt, std::move(sym), line, col);
    }

    // Unknown
    SourceManager_.consumeChar();
    return finish(tok::TokenType::INVALID, StringType(1, ch), line, col);
  }

  if (!TokStream_.empty() && TokStream_.back().type() == tok::TokenType::ENDMARKER)
    return TokStream_.back();

  // not calling finish since update context is useless at end
  SourceManager_.consumeChar();

  if (!TokStream_.empty() && TokStream_.back().type() == tok::TokenType::ENDMARKER)
    return TokStream_.back();

  tok::Token ret = make_token(tok::TokenType::ENDMARKER, std::nullopt, std::nullopt, SourceManager_.column() - 1);
  store(std::move(ret));
  return TokStream_.back();
}

tok::Token Lexer::next()
{
  // Advance the index first
  TokIndex_++;

  // Make sure we have a token at this position
  while (TokIndex_ >= TokStream_.size())
  {
    lexToken();

    // After lexing, check if we hit ENDMARKER
    if (!TokStream_.empty() && TokStream_.back().type() == tok::TokenType::ENDMARKER)
    {
      // We've reached EOF, stay at ENDMARKER
      TokIndex_ = TokStream_.size() - 1;
      break;
    }
  }

  return TokStream_[TokIndex_];
}

tok::Token Lexer::prev()
{
  if (TokIndex_ > 0)
    TokIndex_ -= 1;
  else
    return make_token(tok::TokenType::ENDMARKER);
  return TokStream_[TokIndex_];
}

tok::Token Lexer::current() const
{
  if (TokStream_.back().is(tok::TokenType::ENDMARKER))
    return TokStream_.back();
  if (TokIndex_ < TokStream_.size())
    return TokStream_[TokIndex_];
  return make_token(tok::TokenType::ENDMARKER);
}

void Lexer::store(tok::Token tok)
{
  // push and update index
  TokStream_.push_back(std::move(tok));
}

}  // namespace lex
}  // namespace mylang