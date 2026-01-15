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

IndentationAnalysis Lexer::analyzeIndentation_()
{

  std::size_t         col  = SourceManager_.column();
  std::size_t         line = SourceManager_.line();
  IndentationAnalysis result;
  // Skip indentation handling inside parentheses (implicit line joining)
  if (IndentCtx_.InParentheses > 0)
  {
    result.action = IndentationAnalysis::Action::NONE;
    return result;
  }
  // Measure indentation
  std::size_t indent_count = 0;
  StringType  indent_str;
  char16_t    ch = SourceManager_.current();
  while (ch == u' ' || ch == u'\t')
  {
    indent_str.push_back(ch);
    if (ch == u' ')
      indent_count++;
    else if (ch == u'\t')
      indent_count += 8;  // Tabs are typically 8 spaces
    SourceManager_.consumeChar();
    ch = SourceManager_.current();
  }
  result.IndentString = indent_str;
  result.column       = col + indent_str.length();
  // Check for blank line or comment-only line
  if (ch == u'\n' || ch == BUFFER_END || ch == u'\\')
  {
    result.action = IndentationAnalysis::Action::NONE;
    return result;
  }
  // Detect and validate indentation mode
  if (!indent_str.empty())
  {
    if (IndentCtx_.mode == IndentationContext::IndentMode::UNDETECTED)
      IndentCtx_.detectIndentMode(indent_str);
    if (!IndentCtx_.validateIndent(indent_str))
    {
      result.action       = IndentationAnalysis::Action::ERROR;
      result.ErrorMessage = u"Inconsistent use of tabs and spaces in indentation";
      return result;
    }
  }
  // Compare with current indentation level
  std::size_t current_indent = IndentCtx_.top();
  if (indent_count > current_indent)
  {
    // INDENT
    if (IndentCtx_.ExpectingIndent)
    {
      IndentCtx_.push(indent_count);
      IndentCtx_.ExpectingIndent = false;
      result.action              = IndentationAnalysis::Action::INDENT;
      result.count               = 1;
    }
    else
    {
      result.action       = IndentationAnalysis::Action::ERROR;
      result.ErrorMessage = u"Unexpected indent";
      return result;
    }
  }
  else if (indent_count < current_indent && indent_count != 0)
  {
    // DEDENT - potentially multiple levels
    std::size_t             dedent_count = 0;
    std::stack<std::size_t> temp_stack   = IndentCtx_.IndentStack;
    bool                    found_match  = false;
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
        result.action       = IndentationAnalysis::Action::ERROR;
        result.ErrorMessage = u"Unindent does not match any outer indentation level";
        return result;
      }
      temp_stack.pop();
      dedent_count++;
    }
    if (!found_match && temp_stack.top() != indent_count)
    {
      result.action       = IndentationAnalysis::Action::ERROR;
      result.ErrorMessage = u"Unindent does not match any outer indentation level";
      return result;
    }
    // Apply the dedents
    for (std::size_t i = 0; i < dedent_count; ++i)
      IndentCtx_.pop();
    result.action              = IndentationAnalysis::Action::DEDENT;
    result.count               = dedent_count;
    IndentCtx_.ExpectingIndent = false;
  }
  else
  {
    // Same indentation level
    result.action              = IndentationAnalysis::Action::NONE;
    IndentCtx_.ExpectingIndent = false;
  }
  return result;
}

// BUG FIX #1: Fixed parenthesis tracking - should increment/decrement, not assign bool
void Lexer::updateIndentationContext_(const tok::Token& token)
{
  switch (token.type())
  {
  case tok::TokenType::LPAREN :
  case tok::TokenType::LBRACKET :
    IndentCtx_.InParentheses++;  // FIX: Increment instead of = true
    break;
  case tok::TokenType::RPAREN :
  case tok::TokenType::RBRACKET :
    if (IndentCtx_.InParentheses > 0)
      IndentCtx_.InParentheses--;  // FIX: Decrement instead of = false
    break;
  case tok::TokenType::COLON :
    IndentCtx_.ExpectingIndent = true;
    break;
  case tok::TokenType::NEWLINE :
    IndentCtx_.AtLineStart = true;
    break;
  default :
    if (IndentCtx_.AtLineStart)
      IndentCtx_.AtLineStart = false;
    break;
  }
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
    updateIndentationContext_(TokStream_.back());
    return TokStream_.back();
  };

  if (TokStream_.empty())
  {
    tok::Token ret = make_token(tok::TokenType::BEGINMARKER, std::nullopt, 1, 1);
    store(std::move(ret));
    return TokStream_.back();
  }

  while (true)
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
      StringType endl = u"\n";
      return finish(tok::TokenType::NEWLINE, std::move(endl), line, col);
    }
    case u' ' :
    case u'\t' :
    case u'\r' :
      if (!IndentCtx_.AtLineStart)
      {
        SourceManager_.consumeChar();
        continue;
      }
      break;
    case u'\\' : {
      char16_t lookahead = SourceManager_.peek();
      if (lookahead == ch)
      {
        SourceManager_.consumeChar();
        SourceManager_.consumeChar();
        while (true)
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
    // Handle indentation at line start
    if (IndentCtx_.AtLineStart && ch != u'\n' && ch != BUFFER_END)
    {
      IndentationAnalysis analysis = analyzeIndentation_();
      switch (analysis.action)
      {
      case IndentationAnalysis::Action::INDENT : {
        for (std::size_t i = 0; i < analysis.count; ++i)
        {
          tok::Token indent_tok = make_token(tok::TokenType::INDENT, analysis.IndentString);
          store(std::move(indent_tok));
        }
        return TokStream_.back();
      }
      case IndentationAnalysis::Action::DEDENT : {
        for (std::size_t i = 0; i < analysis.count; ++i)
        {
          tok::Token dedent_tok = make_token(tok::TokenType::DEDENT, u"");
          store(std::move(dedent_tok));
        }
        return TokStream_.back();
      }
      case IndentationAnalysis::Action::ERROR : {
        tok::Token error_tok = make_token(tok::TokenType::INVALID, StringType(analysis.ErrorMessage.begin(), analysis.ErrorMessage.end()));
        store(std::move(error_tok));
        diagnostic::engine.emit("Indentation Error at line" + std::to_string(SourceManager_.line()) + ", col"
                                + std::to_string(SourceManager_.column()) + ": " + utf8::utf16to8(analysis.ErrorMessage));
        return TokStream_.back();
      }
      case IndentationAnalysis::Action::NONE :
      default :
        IndentCtx_.AtLineStart = false;
        break;
      }
      ch = SourceManager_.current();
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
  // Emit all remaining dedents
  while (IndentCtx_.stackSize() > 1)
  {
    IndentCtx_.pop();
    tok::Token dedent_tok = make_token(tok::TokenType::DEDENT, u"", SourceManager_.line(), SourceManager_.column());
    store(std::move(dedent_tok));
  }
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