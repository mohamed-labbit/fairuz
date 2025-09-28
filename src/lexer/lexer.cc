#include "lex/lexer.h"
#include "lex/token.h"
#include "lex/util.h"
#include "macros.h"

#include <algorithm>
#include <memory>
#include <string>


const unsigned int TABWIDTH           = 8;
const unsigned int MAX_ALLOWED_INDENT = TABWIDTH;


Token Lexer::next() {
  // If we already lexed ahead and are walking forward again, return cached.
  if (tok_index_ + 1 < tok_stream_.size())
  {
    ++tok_index_;
    return tok_stream_[tok_index_];
  }

  // First-call sentinel
  if (tok_stream_.empty())
  {
    // make STARTFILE consistent with line/col convention (1-based)
    tok_stream_.push_back(Token(L"", TokenType::START_OF_FILE, {1, 1}));
    tok_index_ = 0;
  }

  char_type ch;
  while ((ch = source_manager_.peek()) != BUF_END)
  {
    size_type line = source_manager_.line();
    size_type col  = source_manager_.column();
    consume_char();

    // newline
    switch (ch)
    {
    case L'\n' : {
      string_type endl = L"\n";
      Token       ret(std::move(endl), TokenType::NEWLINE, {line, col});
      store(std::move(ret));
      size_type ret_index = tok_index_;

      // lexing ahead for indentation
      ch = source_manager_.peek();
      if (ch == L' ' || ch == L'\t')
      {
        consume_char();

        line = source_manager_.line();
        col  = source_manager_.column();

        // Count indentation
        size_type space_count = 1;
        char_type cur         = source_manager_.peek();

        bool used_tabs   = false;
        bool used_spaces = false;

        while (cur == L' ' || cur == L'\t')
        {
          if (cur == L' ')
          {
            space_count++;
            used_spaces = true;
          }
          else if (cur == L'\t')
          {
            space_count += TABWIDTH - (space_count % TABWIDTH);
            used_tabs = true;
          }

          consume_char();
          cur = source_manager_.peek();
        }

        if (used_tabs && used_spaces)
          throw std::runtime_error("Mixed tabs and spaces in indentation");

        if (indent_stack_.empty() && indent_size_ == 0 && space_count > 0)
        {
          if (space_count > MAX_ALLOWED_INDENT)
            throw std::runtime_error("Indentation too large for first indent");
          indent_size_ = space_count;
        }

        if (indent_size_ > 0 && space_count % indent_size_ != 0)
          throw std::runtime_error("Inconsistent indentation detected");

        size_type indent_count =
          space_count
          / (indent_size_ == 0 ? 1 : indent_size_);  // not needed to check for 0 but careful for me

        int prev = indent_stack_.empty() ? 0 : indent_stack_.back();

        if (source_manager_.peek() == L'\n' || source_manager_.peek() == EOF)
          return tok_stream_[tok_index_];

        if (indent_count > prev)
        {

          indent_stack_.push_back(indent_count);
          // dont use store because it moves the buffer index
          for (size_type i = 0, n = indent_count - prev; i < n; ++i)
          {
            string_type s = L"";  // just to satistfy the token constructor
            Token       tok(std::move(s), TokenType::INDENT, {line, col});
            tok_stream_.push_back(std::move(tok));
            col += indent_size_;
          }
        }

        else if (indent_count < prev)
        {
          while (!indent_stack_.empty() && indent_count < indent_stack_.back())
          {
            indent_stack_.pop_back();
            string_type s = L"";  // just to satisfy the token constructor
            Token       tok(std::move(s), TokenType::DEDENT, {line, col});
            tok_stream_.push_back(std::move(tok));
          }
        }
      }

      return tok_stream_[ret_index];
    }
    break;
    case L' ' :
    case L'\t' :
    case L'\r' :
      continue;
      break;
    case L'\\' : {
      if (source_manager_.peek() == L'\\')
      {
        consume_char();  // second '\'
        // consume until newline or EOF (but do not consume the newline here)
        while (source_manager_.peek() != L'\n' && source_manager_.peek() != '\0')
          consume_char();

        continue;
      }
      else
      {
        Token ret(string_type(1, ch), TokenType::UNKNOWN, {line, col});
        store(std::move(ret));
        return tok_stream_.back();
      }
    }
    break;
    case L'_' :
      goto handle_other_symbols;
      break;
    case L'\'' :
    case L'"' : {
      char_type   quote = ch;
      string_type s;

      char_type cur = source_manager_.peek();
      while (cur != L'\n' && cur != quote)
      {
        s.push_back(cur);
        consume_char();
        cur = source_manager_.peek();
      }

      // consume closing quote if present
      if (cur == quote)
        consume_char();
      else
        // should emmit error due to the lack of closing
        // quote, currently just returns UNKNOWN
        return Token(std::move(s), TokenType::UNKNOWN, {line, col});

      Token ret(std::move(s), TokenType::STRING, {line, col});
      store(std::move(ret));
      return tok_stream_.back();
    }
    break;
    default :
      // goto handler;
      break;
    }

handle_other_symbols:
    // Identifiers
    if (isalpha_arabic(ch) || ch == L'_')
    {
      string_type id;
      id.push_back(ch);

      char_type cur;
      while ((cur = source_manager_.peek()) != EOF)
      {
        if (isalpha_arabic(cur) || cur == L'_')
        {
          id.push_back(cur);
          consume_char();
        }
        else
        {
          break;
        }
      }

      TokenType tt = TokenType::IDENTIFIER;

      if (keywords.find(id) != keywords.end())
        tt = keywords.at(id);

      Token tok(std::move(id), tt, {line, col});
      store(std::move(tok));
      return tok_stream_.back();
    }

    // numbers (use iswdigit for char_type)
    else if (std::iswdigit(ch))
    {
      string_type num;
      num.push_back(ch);

      char_type cur;
      while (std::iswdigit((cur = source_manager_.peek())))
      {
        num.push_back(cur);
        consume_char();
      }

      Token ret(std::move(num), TokenType::NUMBER, {line, col});
      store(std::move(ret));
      return tok_stream_.back();
    }

    // operators (allow two-char ops by consulting operators map)
    else if (std::wstring_view(L"=<>!+-|&*/").find(ch) != std::wstring_view::npos)
    {
      string_type op(1, ch);

      // optional second character if it forms a valid operator
      char_type cur;
      if ((cur = source_manager_.peek()) != EOF)
      {
        char_type   nxt = cur;
        string_type two = op;
        two.push_back(nxt);

        if (operators.find(two) != operators.end())
        {
          op.push_back(nxt);
          consume_char();
        }
      }

      TokenType tt;
      if (operators.find(op) != operators.end())
        tt = operators.at(op);
      else
        tt = TokenType::UNKNOWN;

      Token ret(std::move(op), tt, {line, col});
      store(std::move(ret));
      return tok_stream_.back();
    }

    // symbols
    else if (std::wstring_view(L",[]().:").find(ch) != std::wstring_view::npos)
    {
      TokenType   tt;
      string_type sym(1, ch);

      switch (ch)
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
      case ':' : {
        if (source_manager_.peek() == L'=')
        {
          sym = L":=";
          consume_char();
          tt = TokenType::ASSIGN;
        }
        else
        {
          tt = TokenType::COLON;
        }
      }
      break;
      default :
        tt = TokenType::UNKNOWN;
        break;
        // ',', '[', ']' remain as SYMBOL
      }

      store(Token(std::move(sym), tt, {line, col}));
      return tok_stream_.back();
    }

    // unknown char -> consume and return UNKNOWN token (avoid infinite loop)
    {
      Token ret(string_type(1, ch), TokenType::UNKNOWN, {line, col});
      store(std::move(ret));
      return tok_stream_.back();
    }
  }

  // EOF
  {
    Token ret(L"", TokenType::END_OF_FILE, {source_manager_.line(), source_manager_.column()});
    store(std::move(ret));
    return tok_stream_.back();
  }
}

std::vector<Token> Lexer::tokenize() {
  // next_token() will push tokens to the stream on it's own
  while (next().type() != TokenType::END_OF_FILE)
    ;

  return tok_stream_;
}

Token Lexer::peek() { return Token(L"", TokenType::END_OF_FILE, {1, 1}); }

Token Lexer::prev() {
  if (tok_index_ > 0)
    --tok_index_;
  else
    return Token(L"", TokenType::END_OF_FILE, {source_manager_.line(), source_manager_.column()});

  return tok_stream_[tok_index_];
}