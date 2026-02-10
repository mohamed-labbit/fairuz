#include "../../include/input/source_manager.hpp"

#include <functional>
#include <iostream>

namespace mylang {
namespace lex {

CharType SourceManager::peek() { return this->InputBuffer_.peek(); }

std::pair<SizeType, SizeType> SourceManager::offsetMap_(SizeType const& offset) const
{

    if (offset == InputBuffer_.bufferOffset())
        return std::make_pair(InputBuffer_.position().line, InputBuffer_.position().column);

    SizeType iter = 0;
    SizeType diff = 0;

    // Count lines before buffer start
    while (iter < InputBuffer_.bufferOffset()) {
        if (InputBuffer_.at(iter) == u'\n')
            ++diff;
        ++iter;
    }

    SizeType base_line = InputBuffer_.position().line - diff;
    SizeType line = 1;
    SizeType col = 1;
    SizeType const limit = std::min(offset, InputBuffer_.size() - 1);

    iter = 0;
    while (iter < limit) {
        CharType c = InputBuffer_.at(iter);
        if (c == u'\n')
            ++line, col = 1;
        else
            ++col;
        ++iter;
    }

    // combine with base line
    line = base_line + (line - 1);
    return std::make_pair(line, col);
}

std::pair<SizeType, SizeType> SourceManager::offsetMap(SizeType const& offset)
{
    /// TODO: : implement this using the new file manager
    SizeType line = 1, col = 1;
    return std::make_pair(line, col);
}

} // lex
} // mylang
