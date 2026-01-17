#pragma once

#include "../input/file_manager.hpp"
#include "token.hpp"

#include <stack>

namespace mylang {
namespace lex {

class IndentHandler
{
 private:
  std::stack<unsigned> Indstack_;

 public:
  tok::Token handleIndentation(input::FileManager& fm, const tok::Token tok)
  {
    if (!tok.atbol())
      // if not at beginning of a new line, no changes needed
      return tok;

    std::size_t char_offset = tok.location().FilePos;
    std::size_t col         = 0;
    std::size_t altcol      = 0;
    // indentation size
    /*
        for (;;) {
            c = tok_nextc(tok);
            if (c == ' ') {
                col++, altcol++;
            }
            else if (c == '\t') {
                col = (col / tok->tabsize + 1) * tok->tabsize;
                altcol = (altcol / ALTTABSIZE + 1) * ALTTABSIZE;
            }
            else if (c == '\014')  {/* Control-L (formfeed) 
                col = altcol = 0; /* For Emacs users 
            }
            else if (c == '\\') {
                // Indentation cannot be split over multiple physical lines
                // using backslashes. This means that if we found a backslash
                // preceded by whitespace, **the first one we find** determines
                // the level of indentation of whatever comes next.
                cont_line_col = cont_line_col ? cont_line_col : col;
                if ((c = tok_continuation_line(tok)) == -1) {
                    return MAKE_TOKEN(ERRORTOKEN);
                }
            }
            else if (c == EOF && PyErr_Occurred()) {
                return MAKE_TOKEN(ERRORTOKEN);
            }
            else {
                break;
            }
        }
    */

    for (std::size_t i = 0;;i++)
    {
      char16_t c = fm.peekChar(char_offset + i);
    }
  }
};

}
}