#include "lex/lexer.h"
#include "lex/token.h"
#include "lex/util.h"
#include "macros.h"

#include <algorithm>
#include <memory>
#include <string>


Token Lexer::next() {
  // Return cached token if we already lexed ahead
  if (this->tok_index_ + 1 < this->tok_stream_.size())
  {
    ++this->tok_index_;
    return this->tok_stream_[this->tok_index_];
  }

  // First token: emit SOF
  if (this->tok_stream_.empty())
  {
    this->tok_stream_.push_back(Token(L"", TokenType::START_OF_FILE, {1, 1}));
    this->tok_index_ = 0;
  }

  while (true)
  {
    char_type ch = this->source_manager_.peek();
    if (ch == BUF_END)
      break;

    size_type line = source_manager_.line();
    size_type col  = source_manager_.column();

    ch = this->consume_char();


    switch (ch)
    {
    case L'\n' : {
      string_type endl = L"\n";
      Token       ret(std::move(endl), TokenType::NEWLINE, {line, col});
      this->store(std::move(ret));
      size_type ret_index = this->tok_index_;

      // 🔑 New indentation logic
      char_type lookahead = this->source_manager_.peek();
      if (lookahead == L' ' || lookahead == L'\t')
      {
        size_type space_count = 0;
        bool      used_tab    = false;
        bool      used_space  = false;

        while (true)
        {
          lookahead = this->source_manager_.peek();
          if (lookahead != L' ' && lookahead != L'\t')
            break;

          char_type ws = this->consume_char();
          if (ws == L' ')
          {
            ++space_count;
            used_space = true;
          }
          else if (ws == L'\t')
          {
            space_count += TABWIDTH - (space_count % TABWIDTH);
            used_tab = true;
          }
        }

        if (used_tab && used_space)
          throw std::runtime_error("Mixed tabs and spaces in indentation");

        if (this->indent_stack_.empty() && this->indent_size_ == 0 && space_count > 0)
        {
          if (space_count > MAX_ALLOWED_INDENT)
            throw std::runtime_error("Indentation too large for first indent");
          this->indent_size_ = space_count;
        }

        if (this->indent_size_ > 0 && space_count % this->indent_size_ != 0)
          throw std::runtime_error("Inconsistent indentation detected");

        size_type indent_count = space_count / (this->indent_size_ == 0 ? 1 : this->indent_size_);

        int prev = this->indent_stack_.empty() ? 0 : this->indent_stack_.back();

        lookahead = this->source_manager_.peek();
        if (lookahead == L'\n' || lookahead == BUF_END)
          return this->tok_stream_[this->tok_index_];

        if (indent_count > prev)
        {
          this->indent_stack_.push_back(indent_count);
          for (size_type i = 0, n = indent_count - prev; i < n; ++i)
          {
            string_type s = L"";
            Token       tok(std::move(s), TokenType::INDENT, {line, col});
            this->tok_stream_.push_back(std::move(tok));
            col += this->indent_size_;
          }
        }
        else if (indent_count < prev)
        {
          while (!this->indent_stack_.empty() && indent_count < this->indent_stack_.back())
          {
            this->indent_stack_.pop_back();
            string_type s = L"";
            Token       tok(std::move(s), TokenType::DEDENT, {line, col});
            this->tok_stream_.push_back(std::move(tok));
          }
        }
      }

      return this->tok_stream_[ret_index];
    }

    case L' ' :
    case L'\t' :
    case L'\r' :
      continue;

    case L'\\' : {
      char_type lookahead = this->source_manager_.peek();
      if (lookahead == ch)
      {
        this->consume_char();  // consume the second '\'
        while (true)
        {
          char_type c2 = this->source_manager_.peek();
          if (c2 == L'\n' || c2 == BUF_END)
            break;
          this->consume_char();
        }
        continue;
      }
      else
      {
        Token ret(string_type(1, ch), TokenType::UNKNOWN, {line, col});
        this->store(std::move(ret));
        return this->tok_stream_.back();
      }
    }

    case L'_' :
      goto handle_other_symbols;

    case L'\'' :
    case L'"' : {
      string_type s;
      char_type   quote = ch;

      while (true)
      {
        char_type c2 = this->source_manager_.peek();
        if (c2 == L'\n' || c2 == BUF_END || c2 == quote)
          break;
        s.push_back(this->consume_char());
      }

      if (this->source_manager_.peek() == quote)
        this->consume_char();
      else
        return Token(std::move(s), TokenType::UNKNOWN, {line, col});

      Token ret(std::move(s), TokenType::STRING, {line, col});
      this->store(std::move(ret));
      return this->tok_stream_.back();
    }

    default :
      break;
    }

handle_other_symbols:
    if (isalpha_arabic(ch) || ch == L'_')
    {
      string_type id;
      id.push_back(ch);

      while (true)
      {
        char_type c2 = this->source_manager_.peek();
        if (!(isalpha_arabic(c2) || c2 == L'_' || std::iswdigit(c2)))
          break;
        id.push_back(this->consume_char());
      }

      TokenType tt = TokenType::IDENTIFIER;
      if (keywords.find(id) != keywords.end())
        tt = keywords.at(id);

      Token tok(std::move(id), tt, {line, col});
      store(std::move(tok));
      return this->tok_stream_.back();
    }

    else if (std::iswdigit(ch))
    {
      string_type num;
      num.push_back(ch);

      while (true)
      {
        char_type c2 = this->source_manager_.peek();
        if (!std::iswdigit(c2))
          break;
        num.push_back(this->consume_char());
      }

      Token ret(std::move(num), TokenType::NUMBER, {line, col});
      store(std::move(ret));
      return this->tok_stream_.back();
    }

    else if (std::wstring_view(L"=<>!+-|&*/").find(ch) != std::wstring_view::npos)
    {
      string_type op(1, ch);

      char_type lookahead = this->source_manager_.peek();
      if (lookahead != BUF_END)
      {
        string_type two = op;
        two.push_back(lookahead);

        if (operators.find(two) != operators.end())
        {
          op.push_back(this->consume_char());
        }
      }

      TokenType tt = operators.count(op) ? operators.at(op) : TokenType::UNKNOWN;
      Token     ret(std::move(op), tt, {line, col});
      this->store(std::move(ret));
      return this->tok_stream_.back();
    }

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