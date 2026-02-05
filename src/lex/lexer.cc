//
// lexer.cc
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

Lexer::Lexer(std::vector<const tok::Token*>& seq, const SizeType s) :
    TokStream_(seq),
    TokIndex_(0),
    IndentSize_(0)
{
  configureLocale();
}

std::vector<const tok::Token*> Lexer::tokenize()
{
  while (next()->type() != tok::TokenType::ENDMARKER)
    ;
  return this->TokStream_;
}

const tok::Token* Lexer::peek(SizeType n)
{
  while (TokIndex_ + n >= TokStream_.size())
  {
    if (!TokStream_.empty() && TokStream_.back()->type() == tok::TokenType::ENDMARKER)
      return TokStream_.back();
    lexToken();
  }

  return TokStream_[TokIndex_ + n];
}

const tok::Token* Lexer::lexToken()
{
  auto finish = [this](tok::TokenType tt, StringRef str, SizeType l, SizeType c) {
    tok::Token* ret = make_token(tt, str, l, c);
    store(ret);
    return TokStream_.back();
  };

  if (TokStream_.empty())
  {
    tok::Token* ret = make_token(tok::TokenType::BEGINMARKER, std::nullopt, 1, 1);
    store(ret);
    return TokStream_.back();
  }

  for (;;)
  {
    CharType ch = currentChar();

    if (ch == BUFFER_END)
      break;

    SizeType line = SourceManager_.line();
    SizeType col  = SourceManager_.column();

    switch (ch)
    {
    case u'\n' : {
      consumeChar();
      AtBOL_ = true;
      return finish(tok::TokenType::NEWLINE, u"\n", line, col);
    }

    case u' ' :
    case u'\t' :
    case u'\r' :
      // this is basically the same thing CPython does ...
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
          ch = peekChar();
          if (ch == ' ')
          {
            ++size, ++alt_size;
            consumeChar();
          }
          else if (ch == '\t')
          {
            size     = (size / IndentSize_ + 1) * IndentSize_;
            alt_size = (alt_size / TABWIDTH + 1) * TABWIDTH;
            consumeChar();
          }
          else if (ch == '\f')
          {
            size = alt_size = 0;
            consumeChar();
          }
          else if (ch == '\\')
          {
            cont_line_col = cont_line_col ? cont_line_col : size;
            consumeChar();
            // Handle continuation line if needed
          }
          else if (ch == BUFFER_END)
            diagnostic::engine.panic("Unexpected EOF");
          else
            break;
        }
        // Check for blank lines (comments/whitespace only)
        if (ch == '#' || ch == '\n' || ch == '\r')
        {
          if (size == 0 && ch == '\n')
            blankline = false;  // Empty line
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

            ++IndentLevel_;
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
              --IndentLevel_;
              IndentStack_.pop_back();
              AltIndentStack_.pop_back();
              ++dedent_count;
            }

            if (size != IndentStack_.back())
              diagnostic::engine.panic("Unindent does not match any outer indentation level");

            if (alt_size != AltIndentStack_.back())
              diagnostic::engine.panic("Inconsistent indentation");

            for (int i = 0; i < dedent_count; ++i)
              store(make_token(tok::TokenType::DEDENT, u"", line, col));

            return TokStream_[++TokIndex_];
          }
        }
      }
      else
      {
        // Not at beginning of line - just skip whitespace
        consumeChar();
        continue;
      }
      // Should only reach here if blankline is true
      consumeChar();
      continue;

    case u'\\' : {
      CharType lookahead = peekChar();
      if (lookahead == ch)
      {
        consumeChar();
        consumeChar();

        for (;;)
        {
          CharType c2 = SourceManager_.peek();
          if (c2 == u'\n' || c2 == BUFFER_END)
            break;
          consumeChar();
        }

        continue;
      }
    }
    break;
    case u'\'' :
    case u'"' : {
      StringRef str;
      CharType  quote = ch;
      CharType  c2    = nextChar();

      while (c2 != u'\n' && c2 != BUFFER_END && c2 != quote)
      {
        str += c2;
        c2 = nextChar();
      }

      if (c2 == quote)
        consumeChar();
      else
        return make_token(tok::TokenType::INVALID, str);

      return finish(tok::TokenType::STRING, str, line, col);
    }
    case ',' :
    case u'،' : consumeChar(); return finish(tok::TokenType::COMMA, StringRef(ch), line, col);
    default : break;
    }

    // Identifiers
    if ((ch >= 0x0600 && ch <= 0x06FF) || ch == u'_')
    {
      StringRef id;
      id += ch;
      CharType c2 = nextChar();

      while (util::isalphaArabic(c2) || c2 == u'_' || std::iswdigit(c2))
      {
        id += c2;
        c2 = nextChar();
      }

      tok::TokenType tt = tok::TokenType::IDENTIFIER;
      if (tok::keywords.count(id))
        tt = tok::keywords.at(id);

      return finish(tt, id, line, col);
    }

    // Numbers
    else if (std::iswdigit(ch))
    {
      StringRef num;
      num += ch;
      CharType c2 = nextChar();

      // Check for special number bases (hex, octal, binary)
      if (ch == '0')
      {
        if (c2 == 'x' || c2 == 'X')
        {
          // Hexadecimal literal
          num += c2;
          c2 = nextChar();

          bool hasDigits = false;

          for (;;)
          {
            // Skip underscores
            if (c2 == '_')
            {
              c2 = nextChar();
              continue;
            }
            // Check for valid hex digit
            if (std::iswxdigit(c2))
            {
              num += c2;
              hasDigits = true;
              c2        = nextChar();
            }
            else
              break;
          }

          if (!hasDigits)
            diagnostic::engine.panic("Invalid hexadecimal literal: no digits after 0x");

          return finish(tok::TokenType::NUMBER, num, line, col);
        }
        else if (c2 == 'o' || c2 == 'O')
        {
          // Octal literal
          num += c2;
          c2 = nextChar();

          bool hasDigits = false;

          for (;;)
          {
            // Skip underscores
            if (c2 == '_')
            {
              c2 = nextChar();
              continue;
            }

            // Check for valid octal digit (0-7)
            if (c2 >= '0' && c2 <= '7')
            {
              num += c2;
              hasDigits = true;
              c2        = nextChar();
            }
            else if (std::iswdigit(c2))
              // Invalid octal digit (8 or 9)
              diagnostic::engine.panic("Invalid digit '" + std::string(1, c2) + "' in octal literal");
            else
              break;
          }

          if (!hasDigits)
            diagnostic::engine.panic("Invalid octal literal: no digits after 0o");

          return finish(tok::TokenType::NUMBER, num, line, col);
        }
        else if (c2 == 'b' || c2 == 'B')
        {
          // Binary literal
          num += c2;
          c2 = nextChar();

          bool hasDigits = false;

          for (;;)
          {
            // Skip underscores
            if (c2 == '_')
            {
              c2 = nextChar();
              continue;
            }

            // Check for valid binary digit (0 or 1)
            if (c2 == '0' || c2 == '1')
            {
              num += c2;
              hasDigits = true;
              c2        = nextChar();
            }
            else if (std::iswdigit(c2))
              // Invalid binary digit (2-9)
              diagnostic::engine.panic("Invalid digit '" + std::string(1, c2) + "' in binary literal");
            else
              break;
          }

          if (!hasDigits)
            diagnostic::engine.panic("Invalid binary literal: no digits after 0b");

          return finish(tok::TokenType::NUMBER, num, line, col);
        }
        // If it's just '0' followed by regular digits or '.', fall through to decimal parsing
      }

      // Decimal integer part (with optional underscores)
      for (;;)
      {
        if (c2 == '_')
        {
          // Skip underscores in numeric literals
          c2 = nextChar();
          continue;
        }

        if (std::iswdigit(c2))
        {
          num += c2;
          c2 = nextChar();
        }
        else
          break;
      }

      // Check for decimal point (floating-point number)
      if (c2 == '.')
      {
        num += c2;
        c2 = nextChar();

        // Fractional part (with optional underscores)
        for (;;)
        {
          if (c2 == '_')
          {
            c2 = nextChar();
            continue;
          }

          if (std::iswdigit(c2))
          {
            num += c2;
            c2 = nextChar();
          }
          else
            break;
        }

        return finish(tok::TokenType::FLOAT, num, line, col);
      }

      return finish(tok::TokenType::NUMBER, num, line, col);
    }

    // Operators
    else if (util::isOperator(ch))
    {
      StringRef op;
      op += ch;
      CharType nxt = nextChar();

      if (nxt != BUFFER_END)
      {
        StringRef two = op;
        two += nxt;

        if (tok::operators.count(two))
        {
          op += nxt;
          consumeChar();
        }
      }

      tok::TokenType tt = tok::operators.count(op) ? tok::operators.at(op) : tok::TokenType::IDENTIFIER;
      return finish(tt, op, line, col);
    }

    // Symbols
    else if (util::isSymbol(ch))
    {
      tok::TokenType tt;
      StringRef      sym;
      sym += ch;
      consumeChar();

      switch (ch)
      {
      case '(' : tt = tok::TokenType::LPAREN; break;
      case ')' : tt = tok::TokenType::RPAREN; break;
      case '[' :  // FIX: Added bracket support
        tt = tok::TokenType::LBRACKET;
        break;
      case ']' : tt = tok::TokenType::RBRACKET; break;
      case '.' : tt = tok::TokenType::DOT; break;
      case ':' :
        if (currentChar() == u'=')
        {
          consumeChar();
          sym += '=';
          tt = tok::TokenType::OP_ASSIGN;
        }
        else
          tt = tok::TokenType::COLON;
        break;
      default : tt = tok::TokenType::INVALID; break;
      }

      return finish(tt, sym, line, col);
    }

    // Unknown
    consumeChar();
    return finish(tok::TokenType::INVALID, StringRef(ch), line, col);
  }

  if (!TokStream_.empty() && TokStream_.back()->type() == tok::TokenType::ENDMARKER)
    return TokStream_.back();

  // not calling finish since update context is useless at end
  consumeChar();

  if (!TokStream_.empty() && TokStream_.back()->type() == tok::TokenType::ENDMARKER)
    return TokStream_.back();

  const tok::Token* ret = make_token(tok::TokenType::ENDMARKER, std::nullopt, std::nullopt, SourceManager_.column() - 1);
  store(ret);
  return TokStream_.back();
}

const tok::Token* Lexer::next()
{
  // Advance the index first
  ++TokIndex_;

  // Make sure we have a token at this position
  while (TokIndex_ >= TokStream_.size())
  {
    lexToken();
    // After lexing, check if we hit ENDMARKER
    if (!TokStream_.empty() && TokStream_.back()->type() == tok::TokenType::ENDMARKER)
    {
      // We've reached EOF, stay at ENDMARKER
      TokIndex_ = TokStream_.size() - 1;
      break;
    }
  }

  return TokStream_[TokIndex_];
}

const tok::Token* Lexer::prev()
{
  if (TokIndex_ > 0)
    --TokIndex_;
  else
    return make_token(tok::TokenType::ENDMARKER);

  return TokStream_[TokIndex_];
}

const tok::Token* Lexer::current() const
{
  if (TokStream_.back()->is(tok::TokenType::ENDMARKER))
    return TokStream_.back();

  if (TokIndex_ < TokStream_.size())
    return TokStream_[TokIndex_];

  return make_token(tok::TokenType::ENDMARKER);
}

void Lexer::store(const tok::Token* tok)
{
  // push and update index
  TokStream_.push_back(tok);
}

}  // namespace lex
}  // namespace mylang